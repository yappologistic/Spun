#include "cider.h"
#include "library.h"
#include <QClipboard>
#include <QGuiApplication>
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
#include <algorithm>

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
    m_coverRetry.setSingleShot(true);
    connect(&m_coverRetry,&QTimer::timeout,this,&Cider::requestCurrentArtwork);
    QFile connection(m_connectionPath);
    if (connection.open(QIODevice::ReadOnly))
        m_apiToken = QJsonDocument::fromJson(connection.readAll()).object().value("token").toString();
    m_launchTimer.setSingleShot(true); m_launchTimer.setInterval(20000);
    connect(&m_launchTimer,&QTimer::timeout,this,[this] { emit launchChanged(); emit apiFeedback("Cider is taking longer to connect. Check its window and local API.",true); });
    m_recoveryTimer.setSingleShot(true);
    connect(&m_recoveryTimer,&QTimer::timeout,this,&Cider::reconnect);
    m_queuePoll.setInterval(10000);
    connect(&m_queuePoll, &QTimer::timeout, this, &Cider::refreshQueue);
    m_eventCoalesce.setSingleShot(true);m_eventCoalesce.setInterval(180);
    connect(&m_events,&CiderEvents::connectedChanged,this,[this] {
        m_queuePoll.setInterval(m_events.connected()?30000:10000);
        if(m_events.connected() && (m_queueVisible || m_libraryVisible))refreshQueue();
        emit liveChanged();
    });
    connect(&m_events,&CiderEvents::event,this,[this](const QString &type) {
        if(type=="playbackStatus.nowPlayingItemDidChange")m_eventTrack=true;
        if(type=="queueStatus.queueChanged" || type=="playbackStatus.nowPlayingItemDidChange")m_eventQueue=true;
        else if(type.startsWith("audioStatus.") || type=="playbackStatus.nowPlayingStatusDidChange" || type=="queueStatus.smartQueueChanged")m_eventSettings=true;
        else return;
        if(!m_eventCoalesce.isActive())m_eventCoalesce.start();
    });
    connect(&m_eventCoalesce,&QTimer::timeout,this,[this] {
        const bool queue=std::exchange(m_eventQueue,false),settings=std::exchange(m_eventSettings,false);
        if(std::exchange(m_eventTrack,false)) { refresh(); refreshArtwork(); }
        if(queue && (m_queueVisible || m_libraryVisible))refreshQueue();
        if(settings)emit remoteSettingsChanged();
    });
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
            m_position = m_duration = 0; m_track.clear(); m_desktopArtUrl=QUrl(); refreshArtwork();
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
Cider::~Cider() { m_events.configure(false,m_rpcBase,{}); if (m_artDecodeActive) m_artLoader.waitForFinished(); }
qint64 Cider::position() const {
    return qBound<qint64>(0, m_position + (m_playing && m_clock.isValid() ? m_clock.elapsed() : 0), m_duration);
}
void Cider::refresh() {
    if (!m_enabled || m_refreshing) return;
    m_refreshing = true;
    auto message = QDBusMessage::createMethodCall(service, path, props, "GetAll");
    message << iface;
    auto *watch = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 1500), this);
    const auto metadataRevision=m_metadataRevision;
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, watch, metadataRevision] {
        QDBusPendingReply<QVariantMap> reply = *watch;
        watch->deleteLater(); m_refreshing = false;
        if (reply.isError()) {
            if (m_available) { m_tick.stop(); m_available = m_playing = false; emit trackChanged(); emit playingChanged(); }
            return;
        }
        const bool wasAvailable = m_available; m_available = true;
        auto values=reply.value();
        // A PropertiesChanged signal can overtake an older GetAll reply.
        if(metadataRevision!=m_metadataRevision) { values.remove("Metadata"); values.remove("Position"); }
        apply(values);
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
        const bool identityChanged = title != m_title || artist != m_artist || album != m_album || track != m_track;
        metadataChanged = identityChanged || duration != m_duration;
        if (track != m_track) { m_position = 0; m_clock.restart(); }
        m_title = title; m_artist = artist; m_album = album; m_track = track; m_duration = duration;
        const auto url = QUrl(unwrap(meta.value("mpris:artUrl")).toString());
        const bool artworkChanged=identityChanged || url!=m_desktopArtUrl;
        m_desktopArtUrl=url;
        if(artworkChanged)refreshArtwork();
    }
    if (v.contains("PlaybackStatus")) {
        const auto status = unwrap(v.value("PlaybackStatus")).toString();
        const bool isPlaying = status == "Playing";
        const bool statusChanged = m_playbackStatus != status;
        m_playbackStatus = status;
        if (isPlaying != m_playing || statusChanged) {
            m_position = metadataChanged ? 0 : previousPosition;
            m_playing = isPlaying; m_clock.restart();
            if (m_playing) m_tick.start(); else m_tick.stop();
            emit playingChanged();
        }
    }
    if (v.contains("Position")) {
        m_desktopPosition=unwrap(v.value("Position")).toLongLong()/1000;
        if(metadataChanged || !m_positionFromApi || qAbs(m_desktopPosition-position())<1200) {
            m_positionFromApi=false;m_position=m_desktopPosition;m_clock.restart();
            if(position()!=previousPosition)emit positionChanged();
        } else if(m_desktopPosition!=m_staleDesktopPosition) {
            m_staleDesktopPosition=m_desktopPosition;confirmSeek(m_track,m_seekGeneration,-1,0);
        }
    }
    if (v.contains("Volume")) {
        const auto volume=unwrap(v.value("Volume")).toDouble();
        if(volume!=m_desktopVolume) {
            m_desktopVolume=volume;
            if(m_apiToken.isEmpty()) { m_volume=volume; emit volumeChanged(); }
            else refreshVolume();
        }
    }
    if (v.contains("Shuffle")) { bool shuffle = unwrap(v.value("Shuffle")).toBool(); if (shuffle != m_shuffle) { m_shuffle=shuffle; emit settingsChanged(); } }
    if (v.contains("LoopStatus")) { int repeat = qMax(0, QStringList{"None","Playlist","Track"}.indexOf(unwrap(v.value("LoopStatus")).toString())); if (repeat != m_repeat) { m_repeat=repeat; emit settingsChanged(); } }
    const bool oldSeek=m_canSeek, oldNext=m_canNext, oldPrevious=m_canPrevious;
    if (v.contains("CanSeek")) m_canSeek = unwrap(v.value("CanSeek")).toBool();
    if (v.contains("CanGoNext")) m_canNext = unwrap(v.value("CanGoNext")).toBool();
    if (v.contains("CanGoPrevious")) m_canPrevious = unwrap(v.value("CanGoPrevious")).toBool();
    if (!metadataChanged && (oldSeek!=m_canSeek || oldNext!=m_canNext || oldPrevious!=m_canPrevious)) emit trackChanged();
    if (metadataChanged) { emit trackChanged(); if (m_queueVisible || m_libraryVisible) refreshQueue(); if (m_discVisible) refreshDisc(); }
}
void Cider::propertiesChanged(const QString &interface, const QVariantMap &values, const QStringList &invalidated) {
    if (interface == iface) { if(values.contains("Metadata") || invalidated.contains("Metadata"))++m_metadataRevision; apply(values); if (!invalidated.isEmpty()) refresh(); }
}
void Cider::seeked(qlonglong value) {
    m_desktopPosition=value/1000;
    if(m_positionFromApi && qAbs(m_desktopPosition-position())>=1200) {
        if(m_desktopPosition!=m_staleDesktopPosition) {m_staleDesktopPosition=m_desktopPosition;confirmSeek(m_track,m_seekGeneration,-1,0);}
        return;
    }
    m_positionFromApi=false;m_position=m_desktopPosition;m_clock.restart();emit positionChanged();emit positionDiscontinuity(m_position);
}
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
    const int generation=++m_seekGeneration;
    schedule("seek", [this, track, position, generation] {
        if(track!=m_track || generation!=m_seekGeneration)return;
        if(m_apiToken.isEmpty()) {call("SetPosition",{QVariant::fromValue(QDBusObjectPath(track)),QVariant::fromValue(position*1000)});return;}
        apiRequest("POST","/playback/seek",{{"position",position/1000.0}},[this,track,position,generation](bool ok,QJsonObject) {
            if(ok)QTimer::singleShot(160,this,[this,track,position,generation]{confirmSeek(track,generation,position);});
        });
    });
}
void Cider::confirmSeek(const QString &track,int generation,qint64 target,int attempts) {
    if(track!=m_track || generation!=m_seekGeneration)return;
    apiRequest("GET","/playback",{},[this,track,generation,target,attempts](bool ok,QJsonObject json) {
        if(track!=m_track || generation!=m_seekGeneration)return;
        const auto seconds=json.value("data").toObject().value("time").toObject().value("currentTime");
        const qint64 actual=qRound64(seconds.toDouble()*1000);
        if(ok && seconds.isDouble() && actual>=0 && (target<0 || qAbs(actual-target)<1200)) {
            // Some Cider versions leave MPRIS Position unchanged after a seek.
            // Keep the confirmed time until that desktop value changes again.
            m_staleDesktopPosition=m_desktopPosition;m_positionFromApi=true;
            m_position=actual;m_clock.restart();emit positionChanged();emit positionDiscontinuity(actual);return;
        }
        if(ok && attempts>0) {QTimer::singleShot(250,this,[this,track,generation,target,attempts]{confirmSeek(track,generation,target,attempts-1);});return;}
        emit apiFeedback("Cider hasn’t confirmed the seek position. Check playback before trying again.",true);
    },false);
}
void Cider::refreshVolume() {
    if(m_volumeReading) { m_volumeReadAgain=true; return; }
    m_volumeReading=true;
    apiRequest("GET","/audio/volume",{},[this](bool ok,QJsonObject json) {
        m_volumeReading=false;
        const auto value=json.value("data").toObject().value("volume");
        if(ok && value.isDouble() && value.toDouble()>=0 && value.toDouble()<=1) {
            m_volumeReady=true;
            if(m_volume!=value.toDouble()) { m_volume=value.toDouble(); emit volumeChanged(); }
        }
        if(std::exchange(m_volumeReadAgain,false))refreshVolume();
    },false);
}
void Cider::setVolume(double value) {
    value=qBound(0.0,value,1.0);
    if(m_apiToken.isEmpty()) { set("Volume",value); return; }
    // Cider's MPRIS read and write paths use different volume curves.
    // Keep paired controls on its normalized API scale instead.
    schedule("set:Volume",[this,value] {
        apiRequest("PATCH","/audio/volume",{{"volume",value}},[this](bool ok,QJsonObject) {
            if(ok)QTimer::singleShot(160,this,&Cider::refreshVolume);
        });
    });
}
void Cider::setShuffle(bool value) { changeMode("shuffle",value); }
void Cider::setAutoplay(bool value) { changeMode("autoplay",value); }
void Cider::setRepeatMode(int value) { set("LoopStatus", QStringList{"None","Playlist","Track"}[qBound(0,value,2)]); }
void Cider::raise() {
    if (m_available) {
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(service,path,"org.mpris.MediaPlayer2","Raise"));
    } else ensureRunning();
}
void Cider::refreshArtwork() {
    ++m_coverGeneration; m_coverAttempts=0; m_coverRetry.stop();
    if(m_coverReply) { auto *reply=m_coverReply.data();m_coverReply=nullptr;reply->abort();reply->deleteLater(); }
    // Paired Cider supplies artwork and track identity in one API response.
    // Desktop metadata may momentarily combine a new track with an old URL.
    if(m_apiToken.isEmpty()) { loadArt(m_desktopArtUrl); return; }
    loadArt({});
    if(!m_title.isEmpty())requestCurrentArtwork();
}
void Cider::requestCurrentArtwork() {
    if(m_coverReply || m_apiToken.isEmpty() || m_title.isEmpty())return;
    ++m_coverAttempts;
    const auto generation=m_coverGeneration;
    const auto tokenGeneration=m_tokenGeneration;
    auto url=m_rpcBase;url.setPath("/api/v2/playback/now-playing");url.setQuery(QString());
    QNetworkRequest request(url);request.setTransferTimeout(4000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("apptoken",m_apiToken.toUtf8());
    auto *reply=m_rpcNetwork.get(request);m_coverReply=reply;
    connect(reply,&QNetworkReply::readyRead,reply,[reply]{if(reply->bytesAvailable()>512*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,tokenGeneration]{
        reply->deleteLater();
        if(generation!=m_coverGeneration || tokenGeneration!=m_tokenGeneration)return;
        m_coverReply=nullptr;
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto json=QJsonDocument::fromJson(reply->readAll()).object();
        const auto current=json.value("data").toObject();
        const auto params=current.value("playParams").toObject();
        const auto desktopId=m_track.section('/',-1);
        QStringList ids;
        for(const auto &key:{"id","catalogId","reportingId","libraryId"}) {
            const auto id=params.value(key).toString();if(!id.isEmpty())ids.append(id);
        }
        // IDs take precedence over display names, including library/catalog aliases.
        const bool sameTrack=!desktopId.isEmpty() && !ids.isEmpty() ? ids.contains(desktopId) :
            current.value("name").toString()==m_title && !m_title.isEmpty() &&
            current.value("albumName").toString()==m_album && current.value("artistName").toString()==m_artist;
        if(reply->error()==QNetworkReply::NoError && status==200 && sameTrack) {
            QString cover=current.value("artwork").toObject().value("url").toString();
            cover.replace("{w}","1200").replace("{h}","1200").replace("{f}","jpg");
            const QUrl art(cover);
            if(!art.isEmpty() && (art.isLocalFile() || art.scheme()=="https" || art.scheme()=="http") && art.userInfo().isEmpty()) {
                loadArt(art);return;
            }
        }
        // Older Cider versions or restricted tokens retain desktop-only support.
        if(status==401 || status==403 || status==404) { loadArt(m_desktopArtUrl); return; }
        // Album playback and crossfade can expose the previous API item briefly.
        // Never fall back to its artwork; retry only a bounded number of times.
        if(m_coverAttempts<4) {
            static constexpr int delays[]{250,750,2000};
            m_coverRetry.start(delays[m_coverAttempts-1]);
        }
    });
}
void Cider::loadArt(const QUrl &url) {
    ++m_artGeneration; m_pendingArtBytes.clear(); m_pendingArtFile.clear();
    m_artUrl = url;
    if (m_artReply) { auto *previous=m_artReply.data(); m_artReply=nullptr; previous->abort(); previous->deleteLater(); }
    m_art = {}; emit artworkChanged();
    if (url.isLocalFile()) { m_pendingArtFile=url.toLocalFile(); decodeArt(); return; }
    if (url.scheme() != "https" && url.scheme() != "http") return;
    QNetworkRequest request(url);
    request.setTransferTimeout(12000);
    m_artReply = m_network.get(request);
    auto *reply = m_artReply.data();
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64 total) { if (received > 12*1024*1024 || total > 12*1024*1024) reply->abort(); });
    const auto generation=m_artGeneration;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        if (m_artReply == reply) m_artReply=nullptr;
        if (generation == m_artGeneration && reply->error() == QNetworkReply::NoError) {
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
    else if(!m_libraryVisible)m_queuePoll.stop();
    scheduleRecovery();
    emit queueStatusChanged();
}
bool Cider::saveToken() {
    if (m_connectionPath.isEmpty()) return true;
    const auto bytes=QJsonDocument(QJsonObject{{"token",m_apiToken}}).toJson(QJsonDocument::Compact);
    QFile previous(m_connectionPath);
    if (previous.open(QIODevice::ReadOnly) && previous.readAll()==bytes) return true;
    QDir().mkpath(QFileInfo(m_connectionPath).absolutePath());
    QSaveFile file(m_connectionPath);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner) ||
        file.write(bytes)!=bytes.size() || !file.commit()) {
        emit apiFeedback("Connected for this session. Couldn’t save Cider access.",true); return false;
    }
    return true;
}
void Cider::connectQueue(const QString &token) {
    const auto value=token.trimmed();
    if (value.isEmpty() || value.size()>8192 || value.contains('\n') || value.contains('\r')) return;
    ++m_tokenGeneration; m_queueBusy=false;
    if(!m_batchOps.isEmpty()) { m_afterQueue={};finishBatch(false); }
    if(!m_cleanupIndices.isEmpty()) { m_afterQueue={};finishCleanup(false); }
    m_apiToken=value; refreshArtwork(); m_volumeReady=false;refreshVolume(); m_needsToken=false; m_queueError.clear();updateEvents();
    m_recoveryTimer.stop(); m_connectionState.clear(); m_connectionMessage.clear(); emit connectionChanged();
    if(m_queueVisible || m_libraryVisible)m_queuePoll.start();
    refreshQueue();
}
void Cider::authorize() {
    if (m_authReply) return;
    auto url=m_rpcBase; url.setPath("/api/v2/auth/request"); url.setQuery(QString());
    QNetworkRequest req(url); req.setTransferTimeout(120000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    // Bootstrap is unauthenticated; an expired token must not block approval.
    const QJsonObject body{{"app_name","Spun"},{"scopes",QJsonArray{"playback","queue","library","audio"}}};
    auto *reply=m_rpcNetwork.post(req,QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_authReply=reply; m_connectionMessage="Approve Spun in Cider."; emit connectionChanged();
    connect(reply,&QNetworkReply::readyRead,reply,[reply] { if(reply->bytesAvailable()>65536)reply->abort(); });
    connect(reply,&QNetworkReply::finished,this,[this,reply] {
        reply->deleteLater(); if (m_authReply!=reply) return; m_authReply=nullptr;
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto object=QJsonDocument::fromJson(reply->readAll()).object();
        const auto data=object.value("data").isObject()?object.value("data").toObject():object;
        const auto token=data.value("token").toString().trimmed();
        if (reply->error()==QNetworkReply::NoError && status>=200 && status<300 && !token.isEmpty() && token.size()<=8192 && !token.contains('\n') && !token.contains('\r')) {
            connectQueue(token); saveToken(); emit connectionRestored();
            m_connectionMessage="Connected to Cider.";
        } else {
            if (status==404 || status==405) m_connectionMessage="This Cider version needs a manual app token.";
            else if (status==401 || status==403) m_connectionMessage="Access wasn’t approved. Try again when you’re ready.";
            else if (status==408) m_connectionMessage="Approval timed out. Try again when Cider is ready.";
            else if (status==409) m_connectionMessage="Cider already has an approval prompt open. Finish or dismiss it first.";
            else if (status==429) m_connectionMessage="Too many requests. Wait a moment, then try again.";
            else if (!status) m_connectionMessage="Couldn’t connect. Open Cider and enable its local API, then try again.";
            else m_connectionMessage="Cider didn’t return an access token. Try again or use a manual token.";
        }
        emit connectionChanged();
    });
}
void Cider::cancelAuthorization() {
    if (!m_authReply) return;
    auto reply=m_authReply; m_authReply=nullptr; reply->abort();
    m_connectionMessage="Request cancelled. Dismiss any open approval prompt in Cider."; emit connectionChanged();
}
void Cider::setLibraryVisible(bool visible) {
    if(m_libraryVisible==visible)return;
    m_libraryVisible=visible;
    if(visible && !m_apiToken.isEmpty()) { refreshQueue();m_queuePoll.start(); }
    else if(!m_queueVisible)m_queuePoll.stop();
    scheduleRecovery(); emit connectionChanged();
}
void Cider::scheduleRecovery() {
    if (!recovering() || (!m_libraryVisible && !m_queueVisible)) { m_recoveryTimer.stop(); return; }
    if (!m_recoveryTimer.isActive() && !m_probeReply) m_recoveryTimer.start(m_recoveryDelay);
}
void Cider::observeConnection(int status, QNetworkReply::NetworkError error) {
    if (status==401 || status==403) {
        m_connectionState="access"; m_connectionMessage=m_apiToken.isEmpty()?"Connect Spun to Cider.":"Cider access expired or is missing permissions. Reconnect to approve Spun.";
        m_needsToken=true; m_queueReady=false; m_recoveryTimer.stop();
    } else if (!status && error!=QNetworkReply::NoError) {
        m_connectionState=(error==QNetworkReply::TimeoutError || error==QNetworkReply::OperationCanceledError)?"timeout":"offline";
        m_connectionMessage=launching()?"Cider is starting…":m_available?"Cider’s local API is unavailable. Check Connectivity in Cider.":"Cider isn’t reachable. Open it to reconnect.";
        scheduleRecovery();
    } else return;
    emit connectionChanged(); emit queueStatusChanged();
}
void Cider::reconnect() {
    if (m_probeReply || m_authReply) return;
    m_recoveryTimer.stop();
    auto url=m_rpcBase; url.setPath("/api/v2/client/info"); url.setQuery(QString());
    QNetworkRequest req(url); req.setTransferTimeout(4000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("apptoken",m_apiToken.toUtf8());
    auto *reply=m_rpcNetwork.get(req); m_probeReply=reply;
    const int generation=m_tokenGeneration;
    connect(reply,&QNetworkReply::readyRead,reply,[reply] { if(reply->bytesAvailable()>65536)reply->abort(); });
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation] {
        reply->deleteLater(); m_probeReply=nullptr;
        if(generation!=m_tokenGeneration)return;
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto object=QJsonDocument::fromJson(reply->readAll()).object();
        const auto data=object.value("data").isObject()?object.value("data").toObject():object;
        if (status==200 && reply->error()==QNetworkReply::NoError && data.value("version").isString()) {
            m_connectionState="connected"; m_connectionMessage.clear(); m_needsToken=false; m_queueError.clear(); m_recoveryDelay=3000;
            emit connectionChanged(); emit queueStatusChanged(); emit connectionRestored();
            if(m_queueVisible || m_libraryVisible)refreshQueue();
        } else {
            m_recoveryDelay=qMin(30000,m_recoveryDelay*2);
            observeConnection(status,reply->error()); scheduleRecovery();
        }
    });
}
void Cider::queueFailed(const QString &message, bool needsToken) {
    m_queueBusy=false; m_needsToken=needsToken;
    if (m_afterQueue) { m_afterQueue={}; if(!m_batchOps.isEmpty())finishBatch(false);else if(!m_cleanupIndices.isEmpty())finishCleanup(false);else if(!m_insertItems.isEmpty())finishInsert(false);else { m_controlBusy=false; emit controlChanged(); emit apiFeedback("Couldn’t verify the queue. Refresh it and try again.",true); } }
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
    if (m_queueBusy || m_needsToken || recovering()) return;
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
    const int generation=m_tokenGeneration;
    connect(reply,&QNetworkReply::finished,this,[this,reply,offset,generation] {
        if (generation!=m_tokenGeneration) { reply->deleteLater(); return; }
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->readAll();
        const auto error=reply->error(); reply->deleteLater();
        const auto doc=QJsonDocument::fromJson(bytes);
        const auto object=doc.object();
        observeConnection(status,error);
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
                {"id",track.value("id").toString()}, {"type",track.value("type").toString()},
                {"catalogId",attr.value("playParams").toObject().value("catalogId").toString()}, {"url",attr.value("url").toString()},
                {"playable",!attr.value("playParams").toObject().isEmpty()}, {"rpcIndex",index++}, {"artwork",art}});
        }
        if (index<total) { requestQueuePage(index); return; }
        const bool changed=m_queue!=m_pendingQueue;
        const int nextIndex=position>=0 && position<m_pendingQueue.size() ? position : -1;
        const bool indexChanged=m_queueIndex!=nextIndex;
        if (changed || indexChanged) ++m_queueRevision;
        m_queue=m_pendingQueue; m_pendingQueue.clear();
        m_queueIndex=nextIndex;
        m_queueBusy=false; m_queueReady=true; m_needsToken=false; m_queueError.clear();
        if (!m_apiToken.isEmpty()) saveToken();
        if (changed) emit queueChanged();
        if (indexChanged) emit currentIndexChanged();
        emit queueStatusChanged();
        if (m_afterQueue) { auto done=std::move(m_afterQueue); m_afterQueue={}; done(); }
        if (m_queueRefreshPending && !m_controlBusy) { m_queueRefreshPending=false; QTimer::singleShot(0,this,&Cider::refreshQueue); }
    });
}
void Cider::select(int index) {
    if (m_controlBusy || m_queueBusy || !m_queueReady || !m_queueError.isEmpty() || index < 0 || index >= m_queue.size()) return;
    m_controlBusy=true; emit controlChanged();
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
        const bool failed=reply->error()!=QNetworkReply::NoError;
        m_controlBusy=false; emit controlChanged();
        if (failed) emit apiFeedback("Could not play that queued track. Try again.",true);
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
        m_pendingDisc["currentId"]=currentId; m_pendingDisc["albumId"]=match.captured(2);
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
            tracks.append(QVariantMap{{"id",song.value("id").toString()},{"type","songs"},{"playable",!attr.value("playParams").toObject().isEmpty()},{"title",attr.value("name").toString()},
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
void Cider::apiRequest(const QByteArray &method, const QString &endpoint, const QJsonObject &body, std::function<void(bool,QJsonObject)> done, bool reportError) {
    if (m_apiToken.isEmpty()) { if(reportError)emit apiFeedback("Connect Spun to Cider in Queue first.",true); done(false,{}); return; }
    const QUrl relative("/api/v2"+endpoint);
    auto url=m_rpcBase; url.setPath(relative.path()); url.setQuery(relative.query());
    QNetworkRequest req(url); req.setTransferTimeout(6000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("apptoken",m_apiToken.toUtf8());
    const bool sendsBody=method!="GET" && method!="HEAD";
    if(sendsBody)req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    auto *reply=m_rpcNetwork.sendCustomRequest(req,method,sendsBody?QJsonDocument(body).toJson(QJsonDocument::Compact):QByteArray());
    connect(reply,&QNetworkReply::readyRead,reply,[reply] { if (reply->bytesAvailable()>512*1024) reply->abort(); });
    connect(reply,&QNetworkReply::finished,this,[this,reply,done,reportError] {
        reply->deleteLater(); const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->readAll(); const auto doc=QJsonDocument::fromJson(bytes); const auto json=doc.object();
        const bool ok=reply->error()==QNetworkReply::NoError && status>=200 && status<300 &&
            (bytes.trimmed().isEmpty() || doc.isObject()) && !json.contains("error") && !json.contains("errors");
        if (!ok && reportError) emit apiFeedback(status==401 || status==403 ? "Check Spun’s API permissions in Cider." :
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
// Queue identity excludes mutable artwork and rpcIndex, and preserves duplicates.
static QString queueKey(const QVariantMap &row) {
    const auto catalog=row.value("catalogId").toString();
    return !catalog.isEmpty()?"songs:"+catalog:row.value("type").toString()+":"+row.value("id").toString();
}
static QStringList queueKeys(const QVariantList &rows) {
    QStringList result; result.reserve(rows.size());
    for(const auto &row:rows)result.append(queueKey(row.toMap()));
    return result;
}
QVariantMap Cider::previewCleanup(const QString &mode) const {
    if((mode!="duplicates" && mode!="upcoming") || !m_queueReady || !m_queueError.isEmpty() || m_queueBusy || controlBusy() || recovering())return {};
    QSet<QString> seen; QVariantList rows,indices;
    // Unknown identities must never be treated as duplicates by their title.
    auto identity=[](const QVariantMap &row) {
        if(!QStringList{"songs","library-songs"}.contains(row.value("type").toString()) || row.value("id").toString().isEmpty())return QString();
        return queueKey(row);
    };
    if(m_queueIndex>=0 && m_queueIndex<m_queue.size())seen.insert(identity(m_queue[m_queueIndex].toMap()));
    for(int i=qMax(0,m_queueIndex+1);i<m_queue.size();++i) {
        const auto row=m_queue[i].toMap();const auto key=identity(row);
        if(mode=="upcoming" || (!key.isEmpty() && seen.contains(key))) { rows.append(row);indices.append(i); }
        if(!key.isEmpty())seen.insert(key);
    }
    return {{"rows",rows},{"indices",indices},{"revision",m_queueRevision},{"mode",mode}};
}
void Cider::cleanQueue(const QString &mode, int revision) {
    const auto preview=previewCleanup(mode);
    if(preview.isEmpty())return;
    if(revision!=m_queueRevision) { emit apiFeedback("The queue changed. Preview cleanup again.",true);return; }
    const auto indices=preview.value("indices").toList();if(indices.isEmpty())return;
    m_cleanupIndices.clear();for(const auto &index:indices)m_cleanupIndices.append(index.toInt());
    m_cleanupExpected=queueKeys(m_queue);m_cleanupDone=0;m_cleanupPosition=m_queueIndex;m_cleanupGeneration=m_tokenGeneration;
    m_undoTrack.clear();m_controlBusy=true;emit controlChanged();emit queueStatusChanged();
    emit apiFeedback("Cleaning upcoming queue…",false);
    m_afterQueue=[this,revision] { if(revision!=m_queueRevision)finishCleanup(false);else cleanupNext(); };fetchQueue();
}
void Cider::finishCleanup(bool success) {
    const int done=m_cleanupDone,total=m_cleanupIndices.size();
    m_cleanupIndices.clear();m_cleanupExpected.clear();m_controlBusy=false;
    emit controlChanged();emit queueStatusChanged();
    emit apiFeedback(success ? QString("Removed %1 upcoming %2").arg(done).arg(done==1?"song":"songs") :
        QString("%1 of %2 removals confirmed. Cleanup stopped; check the queue before retrying.").arg(done).arg(total),!success);
}
void Cider::cleanupNext() {
    if(m_cleanupGeneration!=m_tokenGeneration || queueKeys(m_queue)!=m_cleanupExpected || m_queueIndex!=m_cleanupPosition) { finishCleanup(false);return; }
    if(m_cleanupDone==m_cleanupIndices.size()) { finishCleanup(true);return; }
    const int index=m_cleanupIndices[m_cleanupIndices.size()-1-m_cleanupDone];
    if(index<=m_queueIndex || index<0 || index>=m_queue.size()) { finishCleanup(false);return; }
    apiRequest("DELETE","/queue/items/"+QString::number(index),{},[this,index,generation=m_cleanupGeneration](bool ok,QJsonObject) {
        if(generation!=m_tokenGeneration)return;
        if(!ok) { finishCleanup(false);refreshQueue();return; }
        m_cleanupExpected.removeAt(index);
        m_afterQueue=[this] {
            if(queueKeys(m_queue)!=m_cleanupExpected || m_queueIndex!=m_cleanupPosition) { finishCleanup(false);return; }
            ++m_cleanupDone;cleanupNext();
        };fetchQueue();
    });
}
void Cider::editQueue(int from, int to, int revision, bool remove) {
    if (controlBusy() || m_queueBusy || !m_queueReady || !m_queueError.isEmpty()) return;
    if (revision!=m_queueRevision) { emit apiFeedback("The queue changed. Try again.",true); return; }
    if (from<0 || to<0 || from>=m_queue.size() || to>=m_queue.size() || (!remove && from==to)) return;
    m_undoTrack.clear(); m_controlBusy=true; emit queueStatusChanged(); emit controlChanged();
    m_afterQueue=[this,from,to,revision,remove] {
        if (revision!=m_queueRevision) { m_controlBusy=false; emit controlChanged(); emit apiFeedback("The queue changed. Try again.",true); return; }
        const auto removed=m_queue[from].toMap(); auto expected=queueKeys(m_queue);
        const int current=m_queueIndex;
        if(remove)expected.removeAt(from);else expected.move(from,to);
        apiRequest(remove?"DELETE":"POST",remove?"/queue/items/"+QString::number(from):"/queue/move",
            remove?QJsonObject{}:QJsonObject{{"from",from},{"to",to}},[this,remove,removed,expected,from,current](bool ok,QJsonObject) {
                if (!ok) { m_controlBusy=false; emit controlChanged(); refreshQueue(); return; }
                m_afterQueue=[this,remove,removed,expected,from,current] {
                    m_controlBusy=false; emit controlChanged();
                    if(queueKeys(m_queue)!=expected) { emit apiFeedback("The queue changed. Check it before retrying.",true);return; }
                    // Restoring a removed current song would also need to rewind playback.
                    if(remove && from!=current && m_queueIndex==(from<current?current-1:current) && removed.value("playable").toBool()) {
                        m_undoTrack=removed;m_undoIndex=from;m_undoRevision=m_queueRevision;m_undoClock.start();
                        const int revision=m_undoRevision;
                        QTimer::singleShot(8000,this,[this,revision] { if(revision==m_undoRevision) { m_undoTrack.clear();emit queueStatusChanged(); } });
                    }
                    emit queueStatusChanged();emit apiFeedback(remove?"Removed from queue":"Queue reordered",false);
                };
                fetchQueue();
            });
    }; fetchQueue();
}
void Cider::undoQueueRemoval() {
    if(!canUndoQueue() || controlBusy() || m_queueBusy)return;
    const auto track=m_undoTrack;const int index=m_undoIndex,revision=m_undoRevision;
    m_restoringQueue=true;insertQueue({track},index,revision);
    if(!m_controlBusy)m_restoringQueue=false;
}
void Cider::insertQueue(const QVariantList &items, int index, int revision) {
    if(controlBusy() || m_queueBusy || !m_queueReady || !m_queueError.isEmpty() || items.isEmpty())return;
    if(revision!=m_queueRevision || index<0 || index>m_queue.size() || (!m_restoringQueue && index<=m_queueIndex)) {
        emit apiFeedback("Drop into the upcoming queue and try again.",true);return;
    }
    if(items.size()>5000 || m_queue.size()+items.size()>10000) { emit apiFeedback("This queue is too large to insert more songs.",true);return; }
    static const QRegularExpression identifier("^[A-Za-z0-9._-]{1,200}$");
    for(const auto &value:items) { const auto row=value.toMap();
        if(!row.value("playable").toBool() || !QStringList{"songs","library-songs"}.contains(row.value("type").toString()) || !identifier.match(row.value("id").toString()).hasMatch()) {
            emit apiFeedback("Only available songs can be inserted.",true);return;
        }
    }
    m_undoTrack.clear();m_insertItems=items;m_insertAt=index;m_insertDone=0;
    m_insertExpected=queueKeys(m_queue);m_insertPosition=m_queueIndex;m_controlBusy=true;
    emit controlChanged();emit queueStatusChanged();
    m_afterQueue=[this,revision] { if(revision!=m_queueRevision)finishInsert(false);else insertNext(); };fetchQueue();
}
void Cider::finishInsert(bool success) {
    const int done=m_insertDone,total=m_insertItems.size();const bool undo=m_restoringQueue;
    m_insertItems.clear();m_insertExpected.clear();m_controlBusy=false;m_restoringQueue=false;
    emit controlChanged();emit queueStatusChanged();
    emit apiFeedback(success?(undo?"Restored to its original position":(total==1?QString("Song inserted"):QString("%1 tracks inserted").arg(total))):
        QString("%1 of %2 tracks placed. Check the queue before retrying; a song may already have been added.").arg(done).arg(total),!success);
}
void Cider::verifyInsert(const QStringList &before,int beforePosition,std::function<void()> done,int attempts) {
    m_afterQueue=[this,before,beforePosition,done,attempts] {
        const auto actual=queueKeys(m_queue);
        if(actual==m_insertExpected && m_queueIndex==m_insertPosition) {done();return;}
        // Cider can acknowledge a command before its catalog lookup finishes.
        // Re-read the unchanged pre-action state; never repeat the mutation.
        if(attempts>0 && actual==before && m_queueIndex==beforePosition) {
            QTimer::singleShot(250,this,[this,before,beforePosition,done,attempts] {
                if(!m_insertItems.isEmpty())verifyInsert(before,beforePosition,done,attempts-1);
            });
        } else finishInsert(false);
    };
    fetchQueue();
}
void Cider::insertNext() {
    if(queueKeys(m_queue)!=m_insertExpected || m_queueIndex!=m_insertPosition) { finishInsert(false);return; }
    if(m_insertDone>=m_insertItems.size()) { finishInsert(true);return; }
    const auto row=m_insertItems[m_insertDone].toMap();
    apiRequest("POST","/queue/add-later",{{"type",row.value("type").toString()},{"id",row.value("id").toString()}},[this,row](bool ok,QJsonObject) {
        if(!ok) { finishInsert(false);refreshQueue();return; }
        const auto before=m_insertExpected;const int position=m_insertPosition;
        verifyAppend(before,position,queueKey(row));
    });
}
void Cider::verifyAppend(const QStringList &before,int position,const QString &key,int attempts) {
    m_afterQueue=[this,before,position,key,attempts] {
        const auto actual=queueKeys(m_queue);
        if(actual==before && m_queueIndex==position && attempts>0) {
            QTimer::singleShot(250,this,[this,before,position,key,attempts] {if(!m_insertItems.isEmpty())verifyAppend(before,position,key,attempts-1);});return;
        }
        if(actual.size()!=before.size()+1 || m_queueIndex!=position) {finishInsert(false);return;}
        // Cider places user additions before an automatically generated radio tail.
        // Locate the one added occurrence in linear time, preserving every old row.
        int prefix=0,suffix=0;
        while(prefix<before.size() && actual[prefix]==before[prefix])++prefix;
        while(suffix<before.size() && actual[actual.size()-1-suffix]==before[before.size()-1-suffix])++suffix;
        const int from=prefix;
        if(from<=position || from<before.size()-suffix || actual[from]!=key) {finishInsert(false);return;}
        m_insertExpected=actual;
        const int to=m_insertAt+m_insertDone;
        if(from==to) { ++m_insertDone;insertNext();return; }
        apiRequest("POST","/queue/move",{{"from",from},{"to",to}},[this,from,to](bool ok,QJsonObject) {
            if(!ok) { finishInsert(false);refreshQueue();return; }
            const auto beforeMove=m_insertExpected;const int beforePosition=m_insertPosition;
            m_insertExpected.move(from,to);
            if(to<=m_insertPosition)++m_insertPosition;
            verifyInsert(beforeMove,beforePosition,[this] {++m_insertDone;insertNext();});
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

void Cider::refreshAudioOptions() {
    if(m_audioBusy)return;
    m_audioBusy=true;m_audioError.clear();emit audioOptionsChanged();
    apiRequest("GET","/audio/automix",{},[this](bool ok,QJsonObject json) {
        const auto data=json.value("data").toObject();
        if(ok && data.value("enabled").isBool())m_audioOptions["automix"]=data.value("enabled").toBool();else m_audioOptions.remove("automix");
        apiRequest("GET","/audio/listening-mode",{},[this](bool ok,QJsonObject json) {
            const auto data=json.value("data").toObject();const auto mode=data.value("mode").toString();
            if(ok && data.value("available").toBool(true) && QStringList{"off","game","antifatigue"}.contains(mode))m_audioOptions["listeningMode"]=mode=="game"?"gaming":mode=="antifatigue"?"unwind":"off";else m_audioOptions.remove("listeningMode");
            if(m_audioOptions.isEmpty())m_audioError="Extra audio controls aren’t available with this Cider connection.";
            m_audioBusy=false;emit audioOptionsChanged();
        });
    });
}
void Cider::setAudioOption(const QString &key,const QVariant &value) {
    if(m_audioBusy || !m_audioOptions.contains(key) || m_audioOptions.value(key)==value)return;
    if((key=="automix" && value.metaType().id()!=QMetaType::Bool) || (key=="listeningMode" && !QStringList{"off","gaming","unwind"}.contains(value.toString())))return;
    if(key!="automix" && key!="listeningMode")return;
    const auto endpoint=key=="automix"?"/audio/automix":"/audio/listening-mode";
    const auto field=key=="automix"?"enabled":"mode";
    m_audioBusy=true;m_audioError.clear();emit audioOptionsChanged();
    const QVariant wireValue=key=="listeningMode"?(value=="gaming"?QVariant("game"):value=="unwind"?QVariant("antifatigue"):value):value;
    apiRequest("PATCH",endpoint,{{field,QJsonValue::fromVariant(wireValue)}},[this,key,value,wireValue,endpoint,field](bool ok,QJsonObject) {
        if(!ok) { m_audioOptions.remove(key);m_audioBusy=false;m_audioError="Refresh audio settings before trying again.";emit audioOptionsChanged();return; }
        apiRequest("GET",endpoint,{},[this,key,value,wireValue,field](bool read,QJsonObject json) {
            const auto actual=json.value("data").toObject().value(field).toVariant();
            if(read && actual==wireValue)m_audioOptions[key]=value;
            else { m_audioOptions.remove(key);m_audioError="Cider hasn’t confirmed this setting. Refresh to check."; }
            m_audioBusy=false;emit audioOptionsChanged();
        });
    });
}
void Cider::copySongLink() {
    if(m_linkBusy)return;
    const auto track=m_track;m_linkBusy=true;
    apiRequest("GET","/playback/now-playing",{},[this,track](bool ok,QJsonObject json) {
        m_linkBusy=false;
        if(track!=m_track) { emit apiFeedback("The song changed. Copy its link again.",true);return; }
        const auto link=Library::songLink(json.value("data").toObject().value("url").toString());
        if(!ok || link.isEmpty()) { emit apiFeedback("No shareable Apple Music link for this song.",true);return; }
        QGuiApplication::clipboard()->setText(link);emit apiFeedback("Song link copied",false);
    });
}

void Cider::refreshAudioQuality() {
    if(m_qualityBusy)return;
    const auto track=m_track; m_qualityBusy=true;m_audioQuality.clear();emit audioQualityChanged();
    apiRequest("GET","/playback/audio-quality",{},[this,track](bool ok,QJsonObject json) {
        m_qualityBusy=false;
        if(track!=m_track) { m_audioQuality="Song changed. Refresh to see its quality.";emit audioQualityChanged();return; }
        const auto data=json.value("data").toObject();
        QStringList lines;const auto label=data.value("flavorLabel").toString().trimmed().left(120);
        if(ok && !label.isEmpty())lines.append(label);
        const auto output=data.value("deviceAudioConfig").toObject();
        const auto rate=output.value("sampleRate").toDouble();const auto channels=output.value("channelCount").toInt();
        // Device output is not the source file's sample rate or quality.
        if(ok && rate>=8000 && rate<=768000) lines.append(QString("Output: %1 kHz").arg(rate/1000.,0,'g',5)+(channels>0 && channels<=32?QString(" · %1 channels").arg(channels):QString()));
        m_audioQuality=lines.isEmpty()?"Quality unavailable from Cider.":lines.join("\n");emit audioQualityChanged();
    });
}

void Cider::setLiveVisible(bool visible) {
    if(m_liveVisible==visible)return;
    m_liveVisible=visible;updateEvents();emit liveChanged();
}
void Cider::updateEvents() {
    m_events.configure(m_liveVisible,m_rpcBase,m_apiToken.toUtf8());
    if(!m_liveVisible) { m_eventCoalesce.stop();m_eventQueue=m_eventSettings=m_eventTrack=false; }
}

void Cider::editQueueSelection(const QVariantList &indices, const QString &operation, int revision) {
    if(controlBusy() || m_queueBusy || !m_queueReady || !m_queueError.isEmpty() || recovering())return;
    if(revision!=m_queueRevision) { emit apiFeedback("The queue changed. Select the songs again.",true);return; }
    if(indices.size()>5000) { emit apiFeedback("Select up to 5,000 songs at a time.",true);return; }
    if(indices.isEmpty() || !QStringList{"up","down","next","end","remove"}.contains(operation))return;
    QList<int> positions;QSet<int> selected;
    for(const auto &value:indices) {
        bool ok=false;const int index=value.toInt(&ok);
        if(!ok || value.toDouble()!=index || index<=m_queueIndex || index<0 || index>=m_queue.size() || selected.contains(index))return;
        positions.append(index);selected.insert(index);
    }
    std::sort(positions.begin(),positions.end());m_batchOps.clear();
    const int first=qMax(0,m_queueIndex+1),last=m_queue.size()-1;
    if(operation=="up" || operation=="next") {
        int destination=first;
        for(int index:positions) {
            const int to=operation=="next"?destination++:index-1;
            if(to>=first && to!=index && (operation=="next" || !selected.contains(to))) { m_batchOps.append({index,to});selected.remove(index);selected.insert(to); }
        }
    } else {
        int destination=last;
        for(auto it=positions.crbegin();it!=positions.crend();++it) {
            const int index=*it,to=operation=="remove"?-1:operation=="end"?destination--:index+1;
            if(operation=="remove" || (to<=last && to!=index && (operation=="end" || !selected.contains(to)))) { m_batchOps.append({index,to});selected.remove(index);selected.insert(to); }
        }
    }
    if(m_batchOps.isEmpty()) { emit apiFeedback("Selected songs are already there",false);return; }
    m_batchExpected=queueKeys(m_queue);m_batchPosition=m_queueIndex;m_batchGeneration=m_tokenGeneration;m_batchDone=0;
    m_undoTrack.clear();m_controlBusy=true;emit controlChanged();emit queueStatusChanged();
    emit apiFeedback(operation=="remove"?"Removing selected songs…":"Moving selected songs…",false);
    m_afterQueue=[this,revision] { if(revision!=m_queueRevision)finishBatch(false);else batchNext(); };fetchQueue();
}
void Cider::finishBatch(bool success) {
    const int done=m_batchDone,total=m_batchOps.size();
    m_batchOps.clear();m_batchExpected.clear();m_controlBusy=false;
    emit controlChanged();emit queueStatusChanged();
    emit apiFeedback(success?QString("Updated %1 selected %2").arg(done).arg(done==1?"song":"songs"):
        QString("%1 of %2 changes confirmed. Stopped; check the queue before retrying.").arg(done).arg(total),!success);
}
void Cider::batchNext() {
    if(m_batchGeneration!=m_tokenGeneration || queueKeys(m_queue)!=m_batchExpected || m_queueIndex!=m_batchPosition) { finishBatch(false);return; }
    if(m_batchDone==m_batchOps.size()) { finishBatch(true);return; }
    const auto [from,to]=m_batchOps[m_batchDone];
    if(from<=m_queueIndex || from>=m_queue.size() || (to>=0 && (to<=m_queueIndex || to>=m_queue.size()))) { finishBatch(false);return; }
    const bool remove=to<0;
    apiRequest(remove?"DELETE":"POST",remove?"/queue/items/"+QString::number(from):"/queue/move",remove?QJsonObject{}:QJsonObject{{"from",from},{"to",to}},[this,from,to,generation=m_batchGeneration](bool ok,QJsonObject) {
        if(generation!=m_tokenGeneration)return;
        if(!ok) { finishBatch(false);refreshQueue();return; }
        if(to<0)m_batchExpected.removeAt(from);else m_batchExpected.move(from,to);
        m_afterQueue=[this] {
            if(queueKeys(m_queue)!=m_batchExpected || m_queueIndex!=m_batchPosition) { finishBatch(false);return; }
            ++m_batchDone;batchNext();
        };fetchQueue();
    });
}
