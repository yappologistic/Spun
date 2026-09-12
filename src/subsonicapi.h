#pragma once
#include "remotemusicapi.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QSettings>
#include <QUrlQuery>
#include <functional>

// One configured account. Track identity includes server and user, never
// credentials.
class SubsonicApi : public RemoteMusicApi {
  Q_OBJECT
public:
  using Reply = std::function<void(const QVariantMap &, const QString &)>;
  using Params = QList<QPair<QString, QString>>;
  explicit SubsonicApi(bool restore = true, const QString &settingsPath = {});
  QString serviceName() const override { return m_serviceName; }
  QString scheme() const override { return "subsonic"; }
  QUrl shareUrl(const QVariantMap &) const override;
  QNetworkRequest artworkRequest(const QUrl &u) const override;
  void reportPlayback(const QVariantMap &, qint64, bool, bool) override;
  void movePlaylistItem(const QString &, const QString &, int, Reply) override;
  ~SubsonicApi() override;
  bool connected() const { return m_connected; }
  bool connecting() const { return m_connecting; }
  QString address() const { return m_address; }
  QString username() const { return m_username; }
  QString identity() const { return m_identity; }
  QString error() const { return m_error; }
  QVariantList playlists() const { return m_playlists; }
  QVariantList folders() const { return m_folders; }
  QString folder() const {
    return m_settings.value("subsonic/folder").toString();
  }
  void setFolder(const QString &id);
  bool scrobbling() const {
    return m_settings.value("subsonic/scrobble", true).toBool();
  }
  void setScrobbling(bool value);
  int bitrate() const {
    return m_settings.value("subsonic/bitrate", 0).toInt();
  }
  void setBitrate(int value);
  bool keyringAvailable() const;
  Q_INVOKABLE void connectServer(const QString &address, const QString &user,
                                 const QString &password, bool remember = true);
  Q_INVOKABLE void disconnectServer();
  Q_INVOKABLE void reloadPlaylists();
  void createPlaylist(const QString &name, Reply callback = {});
  void call(const QString &method, const Params &params, Reply callback,
            const QString &channel = {});
  void cancel(const QString &channel);
  void browse(const QVariantMap &request, Reply callback,
              const QString &channel = "catalog");
  void cover(const QVariantMap &track, const QString &path, Reply callback,
             const QString &channel = "cover");
  void lyrics(const QVariantMap &track, Reply callback);
  void download(const QVariantMap &track, const QString &path, Reply callback,
                const QString &channel = "audio");
  void star(const QVariantMap &track, bool starred, Reply callback);
  void rate(const QVariantMap &track, int rating, Reply callback);
  void editPlaylist(const QString &id, const Params &params, Reply callback);
  void removePlaylist(const QString &id, Reply callback);
  bool owns(const QVariantMap &track) const;
  bool isStarred(const QString &id) const { return m_stars.contains(id); }
  QVariantMap item(const QVariantMap &raw, const QString &kind) const;
  QUrl artworkUrl(const QUrl &reference) const;
  void scrobble(const QVariantMap &track, bool submission, qint64 timestamp,
                Reply callback);

private:
  QUrl url(const QString &method, const Params &params) const;

protected:
  void secret(const QStringList &arguments, const QByteArray &input,
              std::function<void(bool, QByteArray)> callback);

private:
  void discover();
  void playlistAccess(const QString &, Reply);
  QString m_serviceName = "Subsonic", m_playing;
  qint64 m_playStarted = 0, m_listened = 0, m_lastReport = 0;
  bool m_submitted = false, m_wasPlaying = false;
  QVariantList items(const QVariant &rows, const QString &kind) const;
  void stopRequests();
  QNetworkAccessManager m_network;
  QSettings m_settings;
  QString m_address, m_username, m_password, m_identity, m_error;
  bool m_connected = false, m_connecting = false, m_lyricsExtension = false,
       m_formPost = false, m_restoring = false;
  quint64 m_generation = 0;
  QHash<QString, QPointer<QNetworkReply>> m_channels;
  QSet<QString> m_stars;
  QVariantList m_playlists, m_folders;
};
