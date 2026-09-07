#include "cider.h"
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QProcess>
#include <QStandardPaths>
#include <QNetworkProxy>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QBuffer>
#include <QImageReader>
#include <QtConcurrent>
#include <utility>

static const QString service = QStringLiteral("org.mpris.MediaPlayer2.cider");
static const QString path = QStringLiteral("/org/mpris/MediaPlayer2");
static const QString iface = QStringLiteral("org.mpris.MediaPlayer2.Player");
static const QString props = QStringLiteral("org.freedesktop.DBus.Properties");
static QVariant unwrap(QVariant v) { return v.metaType() == QMetaType::fromType<QDBusVariant>() ? v.value<QDBusVariant>().variant() : v; }
static QVariantMap asMap(QVariant v) {
    v = unwrap(v);
    return v.metaType() == QMetaType::fromType<QDBusArgument>() ? qdbus_cast<QVariantMap>(v.value<QDBusArgument>()) : v.toMap();
}

Cider::Cider(bool enabled, const QString &connectionPath, const QUrl &rpcBase, QObject *parent)
    : QObject(parent), m_enabled(enabled), m_connectionPath(connectionPath), m_rpcBase(rpcBase) {
    m_rpcNetwork.setProxy(QNetworkProxy::NoProxy);
    QFile connection(m_connectionPath);
    if (connection.open(QIODevice::ReadOnly))
        m_apiToken = QJsonDocument::fromJson(connection.readAll()).object().value("token").toString();
    m_launchTimer.setSingleShot(true); m_launchTimer.setInterval(20000);
    connect(&m_launchTimer,&QTimer::timeout,this,[this] { emit launchChanged(); emit apiFeedback("Cider is taking longer to connect. Check its window and local API.",true); });
    m_queuePoll.setInterval(10000);
    connect(&m_queuePoll, &QTimer::timeout, this, &Cider::refreshQueue);
    m_commandClock.start();
    m_commands.setInterval(50);
    connect(&m_commands, &QTimer::timeout, this, &Cider::flushCommands);
    if (!enabled) return;
    auto bus = QDBusConnection::sessionBus();
    auto *watch = new QDBusServiceWatcher(service, bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watch, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString &, const QString &, const QString &owner) {
        if (owner.isEmpty()) {
            m_tick.stop();
            m_available = m_playing = false; m_title.clear(); m_artist.clear(); m_album.clear();
            m_position = m_duration = 0; loadArt({});
            m_queue.clear(); m_queueIndex = -1; ++m_queueRevision; m_queueReady = false; m_modesReady=false; emit settingsChanged(); emit currentIndexChanged(); emit queueChanged(); emit queueStatusChanged();
            emit trackChanged(); emit playingChanged(); emit positionChanged();
        } else refresh();
    });
    bus.connect(service, path, props, "PropertiesChanged", this, SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
    bus.connect(service, path, iface, "Seeked", this, SLOT(seeked(qlonglong)));
    m_tick.setInterval(200);
    connect(&m_tick, &QTimer::timeout, this, [this] { if (m_playing) emit positionChanged(); });
    m_poll.setInterval(2000);
    connect(&m_poll, &QTimer::timeout, this, &Cider::refresh);
    m_poll.start();
    refresh();
}
Cider::~Cider() { if (m_artDecodeActive) m_artLoader.waitForFinished(); }
qint64 Cider::position() const {
    return qBound<qint64>(0, m_position + (m_playing && m_clock.isValid() ? m_clock.elapsed() : 0), m_duration);
}
void Cider::refresh() {
    if (!m_enabled || m_refreshing) return;
    m_refreshing = true;
    auto message = QDBusMessage::createMethodCall(service, path, props, "GetAll");
    message << iface;
    auto *watch = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 1500), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, watch] {
        QDBusPendingReply<QVariantMap> reply = *watch;
        watch->deleteLater(); m_refreshing = false;
        if (reply.isError()) {
            if (m_available) { m_tick.stop(); m_available = m_playing = false; emit trackChanged(); emit playingChanged(); }
            return;
        }
        const bool wasAvailable = m_available; m_available = true;
        apply(reply.value());
        if (!wasAvailable) { emit trackChanged(); refreshModes(); }
        if (m_launchTimer.isActive()) { m_launchTimer.stop(); emit launchChanged(); }
    });
}
void Cider::apply(const QVariantMap &v) {
    const auto previousPosition = position();
    bool metadataChanged = false;
    if (v.contains("Metadata")) {
        const auto meta = asMap(v.value("Metadata"));
        const auto title = unwrap(meta.value("xesam:title")).toString();
        const auto artist = unwrap(meta.value("xesam:artist")).toStringList().join(", ");
        const auto album = unwrap(meta.value("xesam:album")).toString();
        auto id = unwrap(meta.value("mpris:trackid"));
        const auto track = id.canConvert<QDBusObjectPath>() ? id.value<QDBusObjectPath>().path() : id.toString();
        const auto duration = unwrap(meta.value("mpris:length")).toLongLong()/1000;
        metadataChanged = title != m_title || artist != m_artist || album != m_album || track != m_track || duration != m_duration;
        if (track != m_track) { m_position = 0; m_clock.restart(); }
        m_title = title; m_artist = artist; m_album = album; m_track = track; m_duration = duration;
        const auto url = QUrl(unwrap(meta.value("mpris:artUrl")).toString());
        if (url != m_artUrl) loadArt(url);
    }
    if (v.contains("PlaybackStatus")) {
        const bool isPlaying = unwrap(v.value("PlaybackStatus")).toString() == "Playing";
        if (isPlaying != m_playing) {
            m_position = metadataChanged ? 0 : previousPosition;
            m_playing = isPlaying; m_clock.restart();
            if (m_playing) m_tick.start(); else m_tick.stop();
            emit playingChanged();
        }
    }
    if (v.contains("Position")) { m_position = unwrap(v.value("Position")).toLongLong()/1000; m_clock.restart(); if (position() != previousPosition) emit positionChanged(); }
    if (v.contains("Volume")) { const auto volume = unwrap(v.value("Volume")).toDouble(); if (volume != m_volume) { m_volume=volume; emit volumeChanged(); } }
    if (v.contains("Shuffle")) { bool shuffle = unwrap(v.value("Shuffle")).toBool(); if (shuffle != m_shuffle) { m_shuffle=shuffle; emit settingsChanged(); } }
    if (v.contains("LoopStatus")) { int repeat = qMax(0, QStringList{"None","Playlist","Track"}.indexOf(unwrap(v.value("LoopStatus")).toString())); if (repeat != m_repeat) { m_repeat=repeat; emit settingsChanged(); } }
    if (v.contains("CanSeek")) m_canSeek = unwrap(v.value("CanSeek")).toBool();
    if (v.contains("CanGoNext")) m_canNext = unwrap(v.value("CanGoNext")).toBool();
    if (v.contains("CanGoPrevious")) m_canPrevious = unwrap(v.value("CanGoPrevious")).toBool();
    if (metadataChanged) { emit trackChanged(); if (m_queueVisible) refreshQueue(); if (m_discVisible) refreshDisc(); }
}
void Cider::propertiesChanged(const QString &interface, const QVariantMap &values, const QStringList &invalidated) {
    if (interface == iface) { apply(values); if (!invalidated.isEmpty()) refresh(); }
}
void Cider::seeked(qlonglong value) { m_position=value/1000; m_clock.restart(); emit positionChanged(); }
void Cider::call(const QString &method, const QVariantList &args) {
    if (!m_available) { raise(); return; }
    auto message = QDBusMessage::createMethodCall(service, path, iface, method); message.setArguments(args);
    auto *watch = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 2000), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, watch] {
        QDBusPendingReply<> reply = *watch;
        if (reply.isError()) fail("Cider could not complete that action.");
        watch->deleteLater(); QTimer::singleShot(160, this, &Cider::refresh);
    });
}
void Cider::set(const QString &key, const QVariant &value) {
    if (!m_available) return;
    schedule("set:" + key, [this, key, value] {
    auto message = QDBusMessage::createMethodCall(service, path, props, "Set");
    message << iface << key << QVariant::fromValue(QDBusVariant(value));
    auto *watch = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 2000), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, watch] {
        QDBusPendingReply<> reply=*watch;
        if (reply.isError()) fail("Cider could not change that setting.");
        watch->deleteLater(); refresh();
    });
    });
}
void Cider::schedule(const QString &key, std::function<void()> action) {
    m_pendingCommands[key] = std::move(action);
    flushCommands();
    if (!m_pendingCommands.isEmpty()) m_commands.start();
}
void Cider::flushCommands() {
    const auto keys = m_pendingCommands.keys();
    for (const auto &key : keys) {
        if (m_commandClock.elapsed() - m_lastCommand.value(key, -1000) < 750) continue;
        auto action = m_pendingCommands.take(key);
        m_lastCommand[key] = m_commandClock.elapsed();
        action();
    }
    if (m_pendingCommands.isEmpty()) m_commands.stop();
}
void Cider::toggle() { call("PlayPause"); }
void Cider::play() { call("Play"); }
void Cider::pause() { if (m_available) call("Pause"); }
void Cider::next() { if (m_canNext) call("Next"); }
void Cider::previous() { if (m_canPrevious) call("Previous"); }
void Cider::seek(qint64 ms) {
    if (!m_canSeek || !m_track.startsWith('/')) return;
    const auto position=qBound<qint64>(0, ms, m_duration);
    const auto track = m_track;
    schedule("seek", [this, track, position] {
        if (track == m_track) call("SetPosition", {QVariant::fromValue(QDBusObjectPath(track)), QVariant::fromValue(position*1000)});
    });
}
void Cider::setVolume(double value) { set("Volume", qBound(0.0,value,1.0)); }
void Cider::setShuffle(bool value) { changeMode("shuffle",value); }
void Cider::setAutoplay(bool value) { changeMode("autoplay",value); }
void Cider::setRepeatMode(int value) { set("LoopStatus", QStringList{"None","Playlist","Track"}[qBound(0,value,2)]); }
void Cider::raise() {
    if (m_available) {
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(service,path,"org.mpris.MediaPlayer2","Raise"));
    } else ensureRunning();
}
void Cider::loadArt(const QUrl &url) {
    ++m_artGeneration; m_pendingArtBytes.clear(); m_pendingArtFile.clear();
    m_artUrl = url;
    if (m_artReply) { m_artReply->abort(); m_artReply->deleteLater(); }
    m_art = {}; emit artworkChanged();
    if (url.isLocalFile()) { m_pendingArtFile=url.toLocalFile(); decodeArt(); return; }
    if (url.scheme() != "https" && url.scheme() != "http") return;
    QNetworkRequest request(url);
    request.setTransferTimeout(12000);
    m_artReply = m_network.get(request);
    auto *reply = m_artReply.data();
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64 total) { if (received > 12*1024*1024 || total > 12*1024*1024) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, url] {
        if (url == m_artUrl && reply->error() == QNetworkReply::NoError) {
            m_pendingArtBytes = reply->readAll();
            decodeArt();
        }
        reply->deleteLater();
    });
}
void Cider::decodeArt() {
    if (m_artDecodeActive || (m_pendingArtBytes.isEmpty() && m_pendingArtFile.isEmpty())) return;
    m_artDecodeActive = true;
    const auto generation = m_artGeneration;
    auto bytes = std::exchange(m_pendingArtBytes, {});
    auto file = std::exchange(m_pendingArtFile, {});
    m_artLoader.disconnect(this);
    connect(&m_artLoader, &QFutureWatcherBase::finished, this, [this, generation] {
        m_artDecodeActive = false;
        if (generation == m_artGeneration) {
            m_art = m_artLoader.result();
            emit artworkChanged();
        }
        decodeArt();
    });
    m_artLoader.setFuture(QtConcurrent::run([bytes=std::move(bytes), file=std::move(file)] {
        QBuffer buffer;
        buffer.setData(bytes);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader;
        if (!file.isEmpty()) reader.setFileName(file);
        else reader.setDevice(&buffer);
        // Match the local player and remote cover resolution. JPEG can decode
        // directly near this size, avoiding a full-resolution temporary image.
        // Preserve small local images and the existing remote scaling behavior.
        const auto original = reader.size();
        const QSize limit(1200,1200);
        if (original.isValid() && (file.isEmpty() || original.width()>1200 || original.height()>1200))
            reader.setScaledSize(original.scaled(limit,Qt::KeepAspectRatio));
        QImage art = reader.read();
        if (file.isEmpty() && !art.isNull())
            art = art.scaled(limit,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        return art;
    }));
}
void Cider::fail(const QString &message) { m_error=message; emit errorChanged(); }
void Cider::dismissError() { m_error.clear(); emit errorChanged(); }

void Cider::setQueueVisible(bool visible) {
    if (m_queueVisible == visible) return;
    m_queueVisible = visible;
    if (visible) { refreshQueue(); m_queuePoll.start(); }
    else m_queuePoll.stop();
    emit queueStatusChanged();
}
void Cider::connectQueue(const QString &token) {
    if (m_queueBusy) return;
    m_apiToken = token.trimmed();
    m_needsToken = false; m_queueError.clear();
    refreshQueue();
}
void Cider::queueFailed(const QString &message, bool needsToken) {
    m_queueBusy=false; m_needsToken=needsToken;
    if (m_afterQueue) { m_afterQueue={}; m_controlBusy=false; emit controlChanged(); emit apiFeedback("Couldn’t verify the queue. Refresh it and try again.",true); }
    if (needsToken) m_queueReady=false;
    m_pendingQueue.clear();
    m_queueError=message;
    emit queueStatusChanged();
}
void Cider::refreshQueue() {
    if (m_controlBusy) return;
    if (m_queueBusy) { m_queueRefreshPending=true; return; }
    fetchQueue();
}
void Cider::fetchQueue() {
    if (m_queueBusy || m_needsToken) return;
    m_queueBusy=true;
    m_pendingQueue.clear(); m_pendingQueueTotal=-1; m_pendingQueuePosition=-1;
    requestQueuePage(0);
    emit queueStatusChanged();
}
void Cider::requestQueuePage(int offset) {
    constexpr int pageSize=50;
    auto url=m_rpcBase; url.setPath("/api/v2/queue");
    QUrlQuery query; query.addQueryItem("offset",QString::number(offset)); query.addQueryItem("limit",QString::number(pageSize));
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setTransferTimeout(6000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    if (!m_apiToken.isEmpty()) request.setRawHeader("apptoken",m_apiToken.toUtf8());
    auto *reply=m_rpcNetwork.get(request);
    connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 received,qint64 total) {
        if (received>16*1024*1024 || total>16*1024*1024) reply->abort();
    });
    connect(reply,&QNetworkReply::finished,this,[this,reply,offset] {
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->readAll();
        const auto error=reply->error(); reply->deleteLater();
        const auto doc=QJsonDocument::fromJson(bytes);
        const auto object=doc.object();
        if (status==401 || status==403) {
            queueFailed(m_apiToken.isEmpty() ? "Connect Cider to see your queue." : "Check the Spun token and its queue access in Cider.",true); return;
        }
        if (error!=QNetworkReply::NoError || status!=200) {
            if (error==QNetworkReply::TimeoutError || error==QNetworkReply::OperationCanceledError)
                queueFailed("Cider's queue request timed out. Retrying…");
            else if (error==QNetworkReply::ConnectionRefusedError || error==QNetworkReply::HostNotFoundError)
                queueFailed("Cannot reach Cider's local API. Retrying…");
            else if (status==404) queueFailed("This Cider version does not provide the queue API.");
            else queueFailed("Cider could not return the queue. Retrying…");
            return;
        }
        const auto data=object.value("data").toObject(), meta=object.value("meta").toObject();
        if (!data.value("items").isArray() || !meta.value("total").isDouble() || !data.value("position").isDouble()) {
            queueFailed("Cider returned an unreadable queue."); return;
        }
        const auto items=data.value("items").toArray();
        const int total=meta.value("total").toInt(-1), position=data.value("position").toInt(-1);
        if (offset==0) { m_pendingQueueTotal=total; m_pendingQueuePosition=position; }
        if (total<0 || total!=m_pendingQueueTotal || position!=m_pendingQueuePosition || (items.isEmpty() && offset<total)) {
            queueFailed("Cider's queue changed while loading. Retrying…"); return;
        }
        int index=offset;
        for (const auto &value:items) {
            const auto item=value.toObject();
            const auto track=item.value("track").isObject() ? item.value("track").toObject() : item;
            const auto attr=track.value("attributes").toObject();
            QString art=attr.value("artwork").toObject().value("url").toString();
            art.replace("{w}","96").replace("{h}","96").replace("{f}","jpg");
            const QUrl artUrl(art);
            if (artUrl.scheme()!="https" && artUrl.scheme()!="http") art.clear();
            m_pendingQueue.append(QVariantMap{{"title",attr.value("name").toString("Unknown track")},
                {"artist",attr.value("artistName").toString("Unknown artist")}, {"duration",attr.value("durationInMillis").toDouble()},
                {"id",track.value("id").toString()}, {"rpcIndex",index++}, {"artwork",art}});
        }
        if (index<total) { requestQueuePage(index); return; }
        const bool changed=m_queue!=m_pendingQueue;
        const int nextIndex=position>=0 && position<m_pendingQueue.size() ? position : -1;
        const bool indexChanged=m_queueIndex!=nextIndex;
        if (changed || indexChanged) ++m_queueRevision;
        m_queue=m_pendingQueue; m_pendingQueue.clear();
        m_queueIndex=nextIndex;
        m_queueBusy=false; m_queueReady=true; m_needsToken=false; m_queueError.clear();
        if (!m_apiToken.isEmpty() && !m_connectionPath.isEmpty()) {
            QFile previous(m_connectionPath); QByteArray old;
            if (previous.open(QIODevice::ReadOnly)) old=previous.readAll();
            const auto next=QJsonDocument(QJsonObject{{"token",m_apiToken}}).toJson(QJsonDocument::Compact);
            if (old!=next) {
                QDir().mkpath(QFileInfo(m_connectionPath).absolutePath());
                QSaveFile file(m_connectionPath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
                    file.write(next); file.commit();
                }
            }
        }
        if (changed) emit queueChanged();
        if (indexChanged) emit currentIndexChanged();
        emit queueStatusChanged();
        if (m_afterQueue) { auto done=std::move(m_afterQueue); m_afterQueue={}; done(); }
        if (m_queueRefreshPending && !m_controlBusy) { m_queueRefreshPending=false; QTimer::singleShot(0,this,&Cider::refreshQueue); }
    });
}
void Cider::select(int index) {
    if (m_controlBusy || !m_queueReady || !m_queueError.isEmpty() || index < 0 || index >= m_queue.size()) return;
    auto url=m_rpcBase; url.setPath("/api/v2/queue/jump");
    QNetworkRequest request(url);
    request.setTransferTimeout(3000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    if (!m_apiToken.isEmpty()) request.setRawHeader("apptoken",m_apiToken.toUtf8());
    // Cider 4 passes the zero-based provider queue index through this endpoint.
    const auto data=QJsonDocument(QJsonObject{{"index",m_queue[index].toMap().value("rpcIndex").toInt()}}).toJson();
    auto *reply=m_rpcNetwork.post(request,data);
    connect(reply,&QNetworkReply::finished,this,[this,reply] {
        if (reply->error()!=QNetworkReply::NoError) { m_queueError="Could not play that queued track."; emit queueStatusChanged(); }
        reply->deleteLater(); refresh(); refreshQueue();
    });
}

void Cider::setDiscVisible(bool value) {
    if (m_discVisible==value) return;
    m_discVisible=value;
    if (value) refreshDisc();
    else { ++m_discGeneration; if (m_discReply) m_discReply->abort(); m_discLoading=false; emit discDetailsChanged(); }
}
void Cider::failDisc(const QString &message) {
    m_discLoading=false; m_discError=message; emit discDetailsChanged();
}
void Cider::requestDiscJson(const QString &path, const QJsonObject &body, int generation, std::function<void(QJsonObject)> done) {
    auto url=m_rpcBase; url.setPath(path); url.setQuery(QString());
    QNetworkRequest request(url);
    request.setTransferTimeout(8000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    if (!m_apiToken.isEmpty()) request.setRawHeader("apptoken",m_apiToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    auto *reply=body.isEmpty() ? m_rpcNetwork.get(request) : m_rpcNetwork.post(request,QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_discReply=reply;
    connect(reply,&QNetworkReply::readyRead,reply,[reply] { if (reply->bytesAvailable()>2*1024*1024) reply->abort(); });
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,done] {
        reply->deleteLater();
        if (generation!=m_discGeneration) return;
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error()!=QNetworkReply::NoError || status!=200) {
            failDisc(status==401 || status==403 ? "Connect Cider in Queue to load album details." : "Album details unavailable. Try again."); return;
        }
        const auto json=QJsonDocument::fromJson(reply->readAll());
        if (!json.isObject()) { failDisc("Album details unavailable. Try again."); return; }
        done(json.object());
    });
}
void Cider::refreshDisc() {
    const int generation=++m_discGeneration;
    if (m_discReply) m_discReply->abort();
    m_discDetails.clear(); m_pendingDisc.clear(); m_discPages.clear(); m_discError.clear(); m_discLoading=true;
    emit discDetailsChanged();
    requestDiscJson("/api/v2/playback/now-playing",{},generation,[this,generation](QJsonObject json) {
        const auto current=json.value("data").toObject();
        const QUrl link(current.value("url").toString());
        const auto match=QRegularExpression("^/([a-zA-Z]{2})/album/[^/]+/([0-9]+)$").match(link.path());
        if (link.host()!="music.apple.com" || !match.hasMatch()) { failDisc("No album details for this track."); return; }
        m_discAlbumPath="/v1/catalog/"+match.captured(1)+"/albums/"+match.captured(2);
        const QString currentId=current.value("playParams").toObject().value("id").toString();
        if (m_discAlbumPath==m_discCachePath && !m_discCache.isEmpty()) {
            m_discDetails=m_discCache; m_discDetails["currentId"]=currentId; m_discLoading=false; emit discDetailsChanged(); return;
        }
        m_pendingDisc["currentId"]=currentId;
        requestDiscJson("/api/v1/amapi/run-v3",{{"path",m_discAlbumPath+"?include=tracks"}},generation,[this,generation](QJsonObject response) {
            const auto records=response.value("data").toObject().value("data").toArray();
            if (records.isEmpty()) { failDisc("Album details unavailable. Try again."); return; }
            const auto album=records.first().toObject();
            const auto attr=album.value("attributes").toObject();
            m_pendingDisc["title"]=attr.value("name").toString(); m_pendingDisc["artist"]=attr.value("artistName").toString();
            const QString date=attr.value("releaseDate").toString();
            m_pendingDisc["year"]=QRegularExpression("^[0-9]{4}($|-)").match(date).hasMatch() ? date.left(4) : QString();
            m_pendingDisc["scope"]="album";
            const auto tracks=album.value("relationships").toObject().value("tracks").toObject();
            m_pendingDisc["expected"]=attr.value("trackCount").toInt();
            // Feed the initial relationship through the same parser used for later pages.
            m_pendingDisc["page"]=tracks.toVariantMap();
            requestDiscTracks({},generation);
        });
    });
}
void Cider::requestDiscTracks(const QString &path, int generation) {
    auto accept=[this,generation](QJsonObject page) {
        auto tracks=m_pendingDisc.value("tracks").toList();
        for (const auto &value:page.value("data").toArray()) {
            const auto song=value.toObject(), attr=song.value("attributes").toObject();
            tracks.append(QVariantMap{{"id",song.value("id").toString()},{"title",attr.value("name").toString()},
                {"artist",attr.value("artistName").toString()},{"number",attr.value("trackNumber").toInt()},
                {"disc",attr.value("discNumber").toInt(1)},{"duration",attr.value("durationInMillis").toDouble()}});
        }
        m_pendingDisc["tracks"]=tracks;
        const auto next=page.value("next").toString();
        if (!next.isEmpty()) { requestDiscTracks(next,generation); return; }
        if (tracks.isEmpty() || (m_pendingDisc.value("expected").toInt()>0 && tracks.size()!=m_pendingDisc.value("expected").toInt())) {
            failDisc("Album track list is incomplete. Try again."); return;
        }
        m_pendingDisc.remove("page"); m_pendingDisc.remove("expected");
        m_discDetails=m_pendingDisc; m_discCache=m_discDetails; m_discCachePath=m_discAlbumPath;
        m_discLoading=false; emit discDetailsChanged();
    };
    if (path.isEmpty()) { accept(QJsonObject::fromVariantMap(m_pendingDisc.value("page").toMap())); return; }
    const QUrl next(path);
    if (!next.isRelative() || next.path()!=m_discAlbumPath+"/tracks" || m_discPages.contains(path) || m_discPages.size()>=100) {
        failDisc("Album track list unavailable."); return;
    }
    m_discPages.insert(path);
    requestDiscJson("/api/v1/amapi/run-v3",{{"path",path}},generation,[accept](QJsonObject response) { accept(response.value("data").toObject()); });
}

// Cider v2 contracts are shared with ciderapp/CiderDeck's official client.
void Cider::apiRequest(const QByteArray &method, const QString &endpoint, const QJsonObject &body, std::function<void(bool,QJsonObject)> done) {
    if (m_apiToken.isEmpty()) { emit apiFeedback("Connect Spun to Cider in Queue first.",true); done(false,{}); return; }
    auto url=m_rpcBase; url.setPath("/api/v2"+endpoint); url.setQuery(QString());
    QNetworkRequest req(url); req.setTransferTimeout(6000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("apptoken",m_apiToken.toUtf8()); req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    auto *reply=m_rpcNetwork.sendCustomRequest(req,method,body.isEmpty()?QByteArray():QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply,&QNetworkReply::readyRead,reply,[reply] { if (reply->bytesAvailable()>512*1024) reply->abort(); });
    connect(reply,&QNetworkReply::finished,this,[this,reply,done] {
        reply->deleteLater(); const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->readAll(); const auto doc=QJsonDocument::fromJson(bytes); const auto json=doc.object();
        const bool ok=reply->error()==QNetworkReply::NoError && status>=200 && status<300 &&
            (bytes.trimmed().isEmpty() || doc.isObject()) && !json.contains("error") && !json.contains("errors");
        if (!ok) emit apiFeedback(status==401 || status==403 ? "Check Spun’s API permissions in Cider." :
            status==404 || status==405 ? "This control isn’t supported by your Cider version." : "Cider couldn’t confirm the change. Check its state before retrying.",true);
        done(ok,json);
    });
}
bool Cider::applyCrossfade(const QJsonObject &json) {
    const auto data=json.value("data").toObject();
    const auto seconds=data.value("durationSec");
    if (!data.value("enabled").isBool() || !seconds.isDouble() || seconds.toDouble()<0 || seconds.toDouble()>60) {
        m_crossfadeReady=false; m_crossfadeError="Cider returned an unreadable crossfade setting."; return false;
    }
    m_crossfade=data.value("enabled").toBool(); m_crossfadeSeconds=seconds.toDouble();
    m_crossfadeReady=true; m_crossfadeError.clear(); return true;
}
void Cider::refreshCrossfade() {
    if (m_crossfadeBusy) return;
    m_crossfadeBusy=true; m_crossfadeReady=false; m_crossfadeError.clear(); emit crossfadeChanged();
    apiRequest("GET","/audio/crossfade",{},[this](bool ok,QJsonObject json) {
        if (ok) applyCrossfade(json);
        else { m_crossfadeReady=false; m_crossfadeError="Crossfade unavailable. Check Cider’s audio permissions."; }
        m_crossfadeBusy=false; emit crossfadeChanged();
    });
}
void Cider::setCrossfade(bool value) { if (value!=m_crossfade) changeCrossfade({{"enabled",value}}); }
void Cider::setCrossfadeSeconds(double value) {
    if (value>=1 && value<=12 && value!=m_crossfadeSeconds) changeCrossfade({{"durationSec",value}});
}
void Cider::changeCrossfade(const QJsonObject &patch) {
    if (m_crossfadeBusy || !m_crossfadeReady) return;
    m_crossfadeBusy=true; emit crossfadeChanged();
    // Patch just the changed field, preserving Cider's exclusions and mix options.
    apiRequest("PATCH","/audio/crossfade",patch,[this,patch](bool ok,QJsonObject) {
        if (!ok) {
            m_crossfadeBusy=false; m_crossfadeReady=false; m_crossfadeError="Couldn’t change crossfade. Refresh to check Cider.";
            emit crossfadeChanged(); return;
        }
        apiRequest("GET","/audio/crossfade",{},[this,patch](bool read,QJsonObject json) {
            const bool confirmed=read && applyCrossfade(json) &&
                (!patch.contains("enabled") || m_crossfade==patch.value("enabled").toBool()) &&
                (!patch.contains("durationSec") || qAbs(m_crossfadeSeconds-patch.value("durationSec").toDouble())<.01);
            if (!confirmed) { m_crossfadeReady=false; m_crossfadeError="Cider hasn’t confirmed crossfade. Try again."; }
            m_crossfadeBusy=false; emit crossfadeChanged();
        });
    });
}
bool Cider::applyModes(const QJsonObject &json) {
    const auto data=json.value("data").toObject();
    if (!data.value("shuffleMode").isBool() || !data.value("autoplay").isBool()) {
        m_modesReady=false; emit settingsChanged(); return false;
    }
    m_shuffle=data.value("shuffleMode").toBool(); m_autoplay=data.value("autoplay").toBool(); m_modesReady=true;
    emit settingsChanged(); return true;
}
void Cider::refreshModes() {
    if (m_modesReading || m_controlBusy || m_apiToken.isEmpty()) return;
    m_modesReading=true; emit controlChanged();
    apiRequest("GET","/playback",{},[this](bool ok,QJsonObject json) {
        m_modesReading=false; emit controlChanged();
        if (!ok) { m_modesReady=false; emit settingsChanged(); }
        else applyModes(json);
    });
}
void Cider::changeMode(const QString &mode, bool value) {
    if (m_controlBusy || m_modesReading) return;
    m_controlBusy=true; emit controlChanged();
    apiRequest("GET","/playback",{},[this,mode,value](bool ok,QJsonObject json) {
        if (!ok || !applyModes(json)) { m_controlBusy=false; emit controlChanged(); return; }
        if ((mode=="shuffle"?m_shuffle:m_autoplay)==value) { m_controlBusy=false; emit controlChanged(); return; }
        apiRequest("POST","/playback/"+mode+"/toggle",{},[this,mode,value](bool success,QJsonObject) {
            if (!success) { m_controlBusy=false; m_modesReady=false; emit settingsChanged(); emit controlChanged(); return; }
            apiRequest("GET","/playback",{},[this,mode,value](bool read,QJsonObject result) {
                const bool confirmed=read && applyModes(result) && (mode=="shuffle"?m_shuffle:m_autoplay)==value;
                m_controlBusy=false; emit controlChanged();
                if (!confirmed) { m_modesReady=false; emit settingsChanged(); emit apiFeedback("Cider hasn’t confirmed the new playback mode. Try again.",true); }
                else if (mode=="autoplay") emit apiFeedback(value?"Autoplay on":"Autoplay off",false);
                refreshQueue();
            });
        });
    });
}
void Cider::moveQueue(int from, int to, int revision) { editQueue(from,to,revision,false); }
void Cider::removeQueue(int index, int revision) { editQueue(index,index,revision,true); }
void Cider::editQueue(int from, int to, int revision, bool remove) {
    if (controlBusy() || m_queueBusy || !m_queueReady || !m_queueError.isEmpty()) return;
    if (revision!=m_queueRevision) { emit apiFeedback("The queue changed. Try again.",true); return; }
    if (from<0 || to<0 || from>=m_queue.size() || to>=m_queue.size() || (!remove && from==to)) return;
    m_controlBusy=true; emit controlChanged();
    // Index-based endpoints require a fresh queue before writing. Do not guess after a change.
    m_afterQueue=[this,from,to,revision,remove] {
        if (revision!=m_queueRevision) { m_controlBusy=false; emit controlChanged(); emit apiFeedback("The queue changed. Try again.",true); return; }
        apiRequest(remove?"DELETE":"POST",remove?"/queue/items/"+QString::number(from):"/queue/move",
            remove?QJsonObject{}:QJsonObject{{"from",from},{"to",to}},[this,remove](bool ok,QJsonObject) {
                if (!ok) { m_controlBusy=false; emit controlChanged(); refreshQueue(); return; }
                m_afterQueue=[this,remove] { m_controlBusy=false; emit controlChanged(); emit apiFeedback(remove?"Removed from queue":"Queue reordered",false); };
                fetchQueue();
            });
    };
    fetchQueue();
}
QStringList Cider::launchCommand() {
    const auto native=QStandardPaths::findExecutable("cider");
    if (!native.isEmpty()) return {native};
    const auto flatpak=QStandardPaths::findExecutable("flatpak");
    const QString desktop="applications/sh.cider.Cider.desktop";
    if (!flatpak.isEmpty() && (!QStandardPaths::locate(QStandardPaths::GenericDataLocation,desktop).isEmpty() ||
        QFile::exists(QDir::homePath()+"/.local/share/flatpak/exports/share/"+desktop) || QFile::exists("/var/lib/flatpak/exports/share/"+desktop)))
        return {flatpak,"run","sh.cider.Cider"};
    return {};
}
void Cider::ensureRunning() {
    if (!m_enabled || m_available || launching()) return;
    m_launchTimer.start(); emit launchChanged();
    auto message=QDBusMessage::createMethodCall("org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","NameHasOwner");
    message << service;
    auto *watch=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message,1500),this);
    connect(watch,&QDBusPendingCallWatcher::finished,this,[this,watch] {
        QDBusPendingReply<bool> reply=*watch; watch->deleteLater();
        if (m_available || (!reply.isError() && reply.value())) { refresh(); return; }
        if (reply.isError()) { m_launchTimer.stop(); emit launchChanged(); emit apiFeedback("Couldn’t check whether Cider is running.",true); return; }
        auto command=launchCommand();
        const auto program=command.isEmpty()?QString():command.takeFirst();
        if (program.isEmpty() || !QProcess::startDetached(program,command)) {
            m_launchTimer.stop(); emit launchChanged(); emit apiFeedback("Couldn’t start Cider. Open it from your application launcher.",true);
        }
    });
}
