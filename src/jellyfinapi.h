#pragma once
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QSet>
#include <QSettings>
#include <QUrlQuery>
#include <functional>
// Jellyfin credentials stay in request headers; persisted rows contain only
// opaque identities.
class JellyfinApi : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool connected READ connected NOTIFY changed)
  Q_PROPERTY(bool connecting READ connecting NOTIFY changed)
  Q_PROPERTY(QString address READ address NOTIFY changed)
  Q_PROPERTY(QString username READ username NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(QVariantList playlists READ playlists NOTIFY changed)
  Q_PROPERTY(QVariantList folders READ folders NOTIFY changed)
  Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY changed)
  Q_PROPERTY(bool scrobbling READ scrobbling WRITE setScrobbling NOTIFY changed)
  Q_PROPERTY(int bitrate READ bitrate WRITE setBitrate NOTIFY changed)
  Q_PROPERTY(bool keyringAvailable READ keyringAvailable CONSTANT)
public:
  using Reply = std::function<void(const QVariantMap &, const QString &)>;
  using Params = QList<QPair<QString, QString>>;
  explicit JellyfinApi(bool restore = true, const QString &settingsPath = {});
  bool keyringAvailable() const;
  ~JellyfinApi() override;
  bool connected() const { return m_connected; }
  bool connecting() const { return m_connecting; }
  QString address() const { return m_address; }
  QString username() const { return m_username; }
  QString identity() const { return m_identity; }
  QString error() const { return m_error; }
  QVariantList playlists() const { return m_playlists; }
  QVariantList folders() const { return m_folders; }
  QString folder() const {
    return m_settings.value("jellyfin/folder").toString();
  }
  bool scrobbling() const {
    return m_settings.value("jellyfin/scrobble", true).toBool();
  }
  int bitrate() const {
    return m_settings.value("jellyfin/bitrate", 0).toInt();
  }
  void setFolder(const QString &id);
  void setScrobbling(bool value);
  void setBitrate(int value);
  Q_INVOKABLE void connectServer(const QString &, const QString &,
                                 const QString &, bool remember = true);
  Q_INVOKABLE void disconnectServer();
  Q_INVOKABLE void reloadPlaylists();
  void createPlaylist(const QString &name, Reply cb = {});
  void cancel(const QString &channel);
  void browse(const QVariantMap &, Reply, const QString &channel = "catalog");
  void cover(const QVariantMap &, const QString &, Reply,
             const QString &channel = "cover");
  void lyrics(const QVariantMap &, Reply);
  void download(const QVariantMap &, const QString &, Reply,
                const QString &channel = "audio");
  void star(const QVariantMap &, bool, Reply);
  void editPlaylist(const QString &, const Params &, Reply);
  void movePlaylistItem(const QString &, const QString &, int, Reply);
  void removePlaylist(const QString &, Reply);
  bool owns(const QVariantMap &) const;
  bool isStarred(const QString &id) const { return m_stars.contains(id); }
  QVariantMap item(const QVariantMap &, const QString &) const;
  QUrl artworkUrl(const QUrl &) const;
  QNetworkRequest artworkRequest(const QUrl &) const;
  void reportPlayback(const QVariantMap &, qint64, bool, bool);

signals:
  void changed();
  void accountChanged();
  void message(const QString &text);

private:
  void secret(const QStringList &, const QByteArray &,
              std::function<void(bool, QByteArray)>);
  QUrl url(const QString &, const Params & = {}) const;
  QNetworkRequest request(const QUrl &) const;
  void json(const QString &, const Params &, const QByteArray &,
            const QJsonObject &, Reply, const QString &channel = {});
  void fetchRows(const QString &, Params, bool, Reply, const QString &);
  void file(const QUrl &, const QString &, qint64, Reply, const QString &);
  void stopRequests();
  void resolvePlaylistAccess(QVariantList, Reply, const QString &);
  void sendPlaybackReport();
  QList<QPair<QString, QJsonObject>> m_reports;
  void discover();
  void acceptLogin(const QVariantMap &, bool);
  void playlistAccess(const QString &, Reply, const QString &channel = {});
  QNetworkAccessManager m_network;
  QSettings m_settings;
  QString m_address, m_username, m_identity, m_error, m_token, m_user, m_device;
  bool m_connected = false, m_connecting = false;
  quint64 m_generation = 0;
  QHash<QString, QPointer<QNetworkReply>> m_channels;
  QVariantList m_playlists, m_folders;
  QSet<QString> m_stars;
  QString m_playing;
};
