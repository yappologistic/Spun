#include "mpris.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QWindow>

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
PlayerAdaptor::PlayerAdaptor(Player *p) : QDBusAbstractAdaptor(p), m_player(p) {
    connect(p, &Player::trackChanged, this, &PlayerAdaptor::changed);
    connect(p, &Player::playingChanged, this, &PlayerAdaptor::changed);
    connect(p, &Player::durationChanged, this, &PlayerAdaptor::changed);
    connect(p, &Player::settingsChanged, this, &PlayerAdaptor::changed);
    connect(p, &Player::volumeChanged, this, &PlayerAdaptor::changed);
    connect(p, &Player::queueChanged, this, &PlayerAdaptor::changed);
    connect(p, &Player::artworkChanged, this, &PlayerAdaptor::changed);
}
QString PlayerAdaptor::status() const {
    switch (m_player->playbackState()) {
    case QMediaPlayer::PlayingState: return "Playing";
    case QMediaPlayer::PausedState: return "Paused";
    default: return "Stopped";
    }
}
QString PlayerAdaptor::loop() const { return QStringList{"None", "Playlist", "Track"}[m_player->repeatMode()]; }
void PlayerAdaptor::setLoop(const QString &value) {
    int index = QStringList{"None", "Playlist", "Track"}.indexOf(value);
    if (index >= 0) m_player->setRepeatMode(index);
}
QVariantMap PlayerAdaptor::metadata() const {
    if (!canControl()) return {};
    QVariantMap result{
        {"mpris:trackid", QVariant::fromValue(QDBusObjectPath(m_player->trackId()))},
        {"mpris:length", m_player->duration() * 1000}, {"xesam:title", m_player->title()},
        {"xesam:artist", QStringList{m_player->artist()}}, {"xesam:album", m_player->album()},
        {"xesam:url", m_player->currentUrl().toString()}};
    if (!m_player->artworkFile().isEmpty()) result["mpris:artUrl"] = QUrl::fromLocalFile(m_player->artworkFile()).toString();
    return result;
}
void PlayerAdaptor::Seek(qlonglong offset) { m_player->seek(m_player->position() + offset / 1000); emit Seeked(position()); }
void PlayerAdaptor::SetPosition(const QDBusObjectPath &id, qlonglong pos) {
    if (id.path() == m_player->trackId()) { m_player->seek(pos / 1000); emit Seeked(position()); }
}
void PlayerAdaptor::changed() {
    const QVariantMap values{{"PlaybackStatus", status()}, {"LoopStatus", loop()}, {"Shuffle", shuffle()},
        {"Metadata", metadata()}, {"Volume", volume()}, {"CanPlay", canControl()}, {"CanPause", canControl()},
        {"CanSeek", canControl()}, {"CanGoNext", canControl()}, {"CanGoPrevious", canControl()}};
    auto message = QDBusMessage::createSignal("/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged");
    message << QString("org.mpris.MediaPlayer2.Player") << values << QStringList();
    QDBusConnection::sessionBus().send(message);
}
bool registerMpris(Player *player) {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService("org.mpris.MediaPlayer2.spun")) return false;
    new RootAdaptor(player);
    new PlayerAdaptor(player);
    return bus.registerObject("/org/mpris/MediaPlayer2", player, QDBusConnection::ExportAdaptors);
}
