#include "mpris.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QWindow>
#include <QQmlProperty>
#include <QTimer>
#include <QCryptographicHash>
#include <cmath>

void RootAdaptor::Quit() { QCoreApplication::quit(); }
void RootAdaptor::OpenFiles(const QStringList &paths) {
    QList<QUrl> urls;
    for (const auto &path : paths) urls.append(QUrl(path));
    static_cast<Player *>(parent())->addUrls(urls);
}
void RootAdaptor::Raise() {
    for (auto *window : QGuiApplication::topLevelWindows())
        if (window->title() == "Spun") { window->showNormal(); window->raise(); window->requestActivate(); }
}
PlayerAdaptor::PlayerAdaptor(Player *p, Cider *cider, Player *youtube, Player *jellyfin) : QDBusAbstractAdaptor(p), m_player(p), m_local(p), m_youtube(youtube), m_jellyfin(jellyfin), m_cider(cider) {
    for (auto *p : {m_local,m_youtube,m_jellyfin}) { if (!p) continue;
    connect(p, &Player::trackChanged, this, &PlayerAdaptor::changed);
    connect(p, &Player::playingChanged, this, &PlayerAdaptor::changed);
    connect(p, &Player::durationChanged, this, &PlayerAdaptor::changed);
    connect(p, &Player::settingsChanged, this, &PlayerAdaptor::changed);
    connect(p, &Player::volumeChanged, this, &PlayerAdaptor::changed);
    connect(p, &Player::queueChanged, this, &PlayerAdaptor::changed);
    connect(p, &Player::artworkChanged, this, &PlayerAdaptor::changed);
    connect(p, &Player::seeked, this, [this,p](qint64 value) { if (!m_remote && m_player==p) emit Seeked(value * 1000); });
    }
    if (cider) {
        connect(cider, &Cider::trackChanged, this, &PlayerAdaptor::changed);
        connect(cider, &Cider::playingChanged, this, &PlayerAdaptor::changed);
        connect(cider, &Cider::settingsChanged, this, &PlayerAdaptor::changed);
        connect(cider, &Cider::volumeChanged, this, &PlayerAdaptor::changed);
        connect(cider, &Cider::artworkChanged, this, &PlayerAdaptor::changed);
        connect(cider, &Cider::positionDiscontinuity, this, [this](qint64 value) { if (m_remote) emit Seeked(value * 1000); });
    }
}
QString PlayerAdaptor::status() const {
    if (m_remote) return m_cider->playbackStatus();
    switch (m_player->playbackState()) {
    case QMediaPlayer::PlayingState: return "Playing";
    case QMediaPlayer::PausedState: return "Paused";
    default: return "Stopped";
    }
}
QString PlayerAdaptor::loop() const { return QStringList{"None", "Playlist", "Track"}[m_remote ? m_cider->repeatMode() : m_player->repeatMode()]; }
void PlayerAdaptor::setLoop(const QString &value) {
    int index = QStringList{"None", "Playlist", "Track"}.indexOf(value);
    if (index >= 0) { if (m_remote) m_cider->setRepeatMode(index); else m_player->setRepeatMode(index); }
}
void PlayerAdaptor::setSourceWindow(QObject *window) {
    m_sourceWindow = window;
    QQmlProperty(window, "useCider").connectNotifySignal(this, SLOT(syncSource()));
    QQmlProperty(window, "useYoutube").connectNotifySignal(this, SLOT(syncSource()));
    if(window->metaObject()->indexOfProperty("useJellyfin")>=0)QQmlProperty(window, "useJellyfin").connectNotifySignal(this, SLOT(syncSource()));
    syncSource();
}
void PlayerAdaptor::syncSource() {
    m_player = m_jellyfin && m_sourceWindow && m_sourceWindow->property("useJellyfin").toBool() ? m_jellyfin : m_youtube && m_sourceWindow && m_sourceWindow->property("useYoutube").toBool() ? m_youtube : m_local;
    setRemote(m_sourceWindow && m_sourceWindow->property("useCider").toBool());
    changed(); emit Seeked(position());
}
void PlayerAdaptor::setRemote(bool remote) {
    remote = remote && m_cider;
    if (remote == m_remote) return;
    m_remote = remote;
    changed();
    emit Seeked(position());
}
QString PlayerAdaptor::trackId() const {
    if (!m_remote) return m_player->trackId();
    if (!m_cider->count()) return "/org/mpris/MediaPlayer2/TrackList/NoTrack";
    return "/org/spun/cider/t" + QString::fromLatin1(QCryptographicHash::hash(m_cider->trackKey().toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
}
qint64 PlayerAdaptor::duration() const { return m_remote ? m_cider->duration() : m_player->duration(); }
void PlayerAdaptor::setVolume(double value) {
    if (!std::isfinite(value)) return;
    if (m_remote) m_cider->setVolume(qBound(0., value, 1.)); else m_player->setVolume(qBound(0., value, 1.));
}
void PlayerAdaptor::Next() {
    if (!canNext()) return;
    if (m_remote) m_cider->next();
    else m_player->next(false, m_player->playing());
}
void PlayerAdaptor::Previous() {
    if (!canPrevious()) return;
    if (m_remote) m_cider->previous();
    else m_player->previous(m_player->playing());
}
QVariantMap PlayerAdaptor::metadata() const {
    if (!canControl()) return {};
    QVariantMap result{
        {"mpris:trackid", QVariant::fromValue(QDBusObjectPath(trackId()))},
        {"mpris:length", duration() * 1000}, {"xesam:title", (m_remote ? m_cider->title() : m_player->title())},
        {"xesam:artist", QStringList{m_remote ? m_cider->artist() : m_player->artist()}}, {"xesam:album", (m_remote ? m_cider->album() : m_player->album())},
        {"xesam:url", m_player->currentUrl().toString()}};
    if (m_remote) {
        result.remove("xesam:url");
        if (!m_cider->artworkUrl().isEmpty()) result["mpris:artUrl"] = m_cider->artworkUrl().toString();
    } else if (!m_player->artworkFile().isEmpty()) result["mpris:artUrl"] = QUrl::fromLocalFile(m_player->artworkFile()).toString();
    return result;
}
void PlayerAdaptor::seekTo(qint64 value) { if (m_remote) m_cider->seek(value); else m_player->seek(value); }
void PlayerAdaptor::Seek(qlonglong offset) {
    if (!canSeek()) return;
    const auto delta = offset / 1000;
    const auto current = position() / 1000;
    if (delta > duration() - current) { Next(); return; }
    seekTo(delta < -current ? 0 : current + delta);
}
void PlayerAdaptor::SetPosition(const QDBusObjectPath &id, qlonglong pos) {
    if (canSeek() && id.path() == trackId() && pos >= 0 && pos / 1000 <= duration()) seekTo(pos / 1000);
}
void PlayerAdaptor::changed() {
    if (m_changePending) return;
    m_changePending = true;
    QTimer::singleShot(0, this, [this] {
        m_changePending = false;
        const QVariantMap values{{"PlaybackStatus", status()}, {"LoopStatus", loop()}, {"Shuffle", shuffle()},
            {"Metadata", metadata()}, {"Volume", volume()}, {"CanPlay", canControl()}, {"CanPause", canControl()},
            {"CanSeek", canSeek()}, {"CanGoNext", canNext()}, {"CanGoPrevious", canPrevious()}};
        QVariantMap changes;
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            if (!m_lastProperties.contains(it.key()) || m_lastProperties.value(it.key()) != it.value()) changes.insert(it.key(), it.value());
        m_lastProperties = values;
        if (changes.isEmpty()) return;
        auto message = QDBusMessage::createSignal("/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged");
        message << QString("org.mpris.MediaPlayer2.Player") << changes << QStringList();
        QDBusConnection::sessionBus().send(message);
    });
}
PlayerAdaptor *registerMpris(Player *player, Cider *cider, Player *youtube, Player *jellyfin) {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService("org.mpris.MediaPlayer2.spun")) return nullptr;
    new RootAdaptor(player);
    auto *adaptor = new PlayerAdaptor(player, cider, youtube, jellyfin);
    if (!bus.registerObject("/org/mpris/MediaPlayer2", player, QDBusConnection::ExportAdaptors)) {
        bus.unregisterService("org.mpris.MediaPlayer2.spun"); return nullptr;
    }
    return adaptor;
}
