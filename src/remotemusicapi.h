#pragma once
#include <QNetworkRequest>
#include <QObject>
#include <QVariantMap>
#include <functional>

// Shared music-library contract. Protocol and authentication stay in each
// adapter.
class RemoteMusicApi : public QObject {
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
  Q_PROPERTY(QString serviceName READ serviceName NOTIFY changed)
  Q_PROPERTY(QString scheme READ scheme CONSTANT)
public:
  using QObject::QObject;
  using Reply = std::function<void(const QVariantMap &, const QString &)>;
  using Params = QList<QPair<QString, QString>>;
  virtual QString serviceName() const = 0;
  virtual QString scheme() const = 0;
  virtual bool connected() const = 0;
  virtual bool connecting() const = 0;
  virtual QString address() const = 0;
  virtual QString username() const = 0;
  virtual QString identity() const = 0;
  virtual QString error() const = 0;
  virtual QVariantList playlists() const = 0;
  virtual QVariantList folders() const = 0;
  virtual QString folder() const = 0;
  virtual bool scrobbling() const = 0;
  virtual int bitrate() const = 0;
  virtual bool keyringAvailable() const = 0;
  virtual void setFolder(const QString &) = 0;
  virtual void setScrobbling(bool) = 0;
  virtual void setBitrate(int) = 0;
  Q_INVOKABLE virtual void connectServer(const QString &, const QString &,
                                         const QString &,
                                         bool remember = true) = 0;
  Q_INVOKABLE virtual void disconnectServer() = 0;
  Q_INVOKABLE virtual void reloadPlaylists() = 0;
  virtual void createPlaylist(const QString &, Reply = {}) = 0;
  virtual void cancel(const QString &) = 0;
  virtual void browse(const QVariantMap &, Reply,
                      const QString &channel = "catalog") = 0;
  virtual void cover(const QVariantMap &, const QString &, Reply,
                     const QString &channel = "cover") = 0;
  virtual void lyrics(const QVariantMap &, Reply) = 0;
  virtual void download(const QVariantMap &, const QString &, Reply,
                        const QString &channel = "audio") = 0;
  virtual void star(const QVariantMap &, bool, Reply) = 0;
  virtual void editPlaylist(const QString &, const Params &, Reply) = 0;
  virtual void movePlaylistItem(const QString &, const QString &, int,
                                Reply) = 0;
  virtual void removePlaylist(const QString &, Reply) = 0;
  virtual bool owns(const QVariantMap &) const = 0;
  virtual bool isStarred(const QString &) const = 0;
  virtual QVariantMap item(const QVariantMap &, const QString &) const = 0;
  virtual QUrl artworkUrl(const QUrl &) const = 0;
  virtual QNetworkRequest artworkRequest(const QUrl &) const = 0;
  virtual QUrl shareUrl(const QVariantMap &) const = 0;
  virtual void reportPlayback(const QVariantMap &, qint64, bool, bool) = 0;
signals:
  void changed();
  void accountChanged();
  void message(const QString &text);
};
