#pragma once
#include "jellyfinapi.h"
#include "lyrics.h"
#include "player.h"
#include <QTemporaryDir>
#include <QTimer>

class Jellyfin : public QObject {
  Q_OBJECT
  Q_PROPERTY(JellyfinApi *server READ server CONSTANT)
  Q_PROPERTY(Player *transport READ transport CONSTANT)
  Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(bool more READ more NOTIFY changed)
  Q_PROPERTY(bool actionBusy READ actionBusy NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(QString page READ page NOTIFY changed)
  Q_PROPERTY(QString heading READ heading NOTIFY changed)
  Q_PROPERTY(QVariantList items READ items NOTIFY changed)
  Q_PROPERTY(QVariantMap collection READ collection NOTIFY changed)
  Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
  Q_PROPERTY(bool canBack READ canBack NOTIFY changed)
  Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
  Q_PROPERTY(QVariantList lines READ lines NOTIFY changed)
  Q_PROPERTY(bool loading READ loading NOTIFY changed)
  Q_PROPERTY(QString message READ message NOTIFY changed)
  Q_PROPERTY(bool timed READ timed NOTIFY changed)
  Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
public:
  explicit Jellyfin(const QString &directory, bool restore = true,
                    QObject *parent = nullptr);
  ~Jellyfin() override;
  JellyfinApi *server() { return &m_api; }
  Player *transport() { return &m_player; }
  bool enabled() const { return m_enabled; }
  void setEnabled(bool);
  bool busy() const { return m_busy; }
  bool more() const { return m_more; }
  bool actionBusy() const { return m_actionBusy; }
  QString error() const { return m_error; }
  QString page() const { return m_request.value("mode", "albums").toString(); }
  QString heading() const { return m_heading; }
  QVariantList items() const { return m_items; }
  QVariantMap collection() const { return m_collection; }
  QVariantMap current() const;
  bool canBack() const { return !m_back.isEmpty(); }
  Q_INVOKABLE QString artwork(const QString &id);
  Q_INVOKABLE void show(const QString &page);
  Q_INVOKABLE void search(const QString &query,
                          const QString &filter = "songs");
  Q_INVOKABLE void open(const QVariantMap &item);
  Q_INVOKABLE void back();
  Q_INVOKABLE void reload();
  Q_INVOKABLE void loadMore();
  Q_INVOKABLE void playItems(const QVariantList &, int index = 0);
  Q_INVOKABLE void playItem(const QVariantMap &);
  Q_INVOKABLE void enqueue(const QVariantMap &);
  Q_INVOKABLE bool favorite(const QString &id) const {
    return m_api.isStarred(id);
  }
  Q_INVOKABLE void toggleFavorite(const QVariantMap &);
  Q_INVOKABLE void createPlaylist(const QString &name);
  Q_INVOKABLE void renamePlaylist(const QString &id, const QString &name);
  Q_INVOKABLE void addToPlaylist(const QString &id, const QVariantMap &);
  Q_INVOKABLE void removeFromPlaylist(const QVariantMap &);
  Q_INVOKABLE void movePlaylistItem(int from, int to);
  Q_INVOKABLE void deletePlaylist(const QString &id);
  Q_INVOKABLE void copyLink(const QVariantMap &);
  bool active() const { return m_active; }
  void setActive(bool);
  QVariantList lines() const { return m_lines; }
  bool loading() const { return m_loading; }
  QString message() const { return m_message; }
  bool timed() const { return m_timed; }
  int currentIndex() const { return m_lyricIndex; }
  Q_INVOKABLE void refresh();
  Q_INVOKABLE bool seekToLine(int);
signals:
  void changed();
  void currentIndexChanged();
  void feedback(const QString &message, bool error);
  void artworkChanged();

private:
  void browse(const QVariantMap &, bool append = false);
  void loadCurrent();
  void loadArt();
  void loadThumbs();
  void persist();
  void restoreQueue();
  void report(bool stopped = false);
  void updateLyrics();
  void finishAction(const QString &, bool refreshPage = false);
  QList<Track> tracks(const QVariantList &);
  QVariantMap original(const QVariantMap &) const;
  JellyfinApi m_api;
  Player m_player;
  QString m_directory, m_identity, m_error, m_heading = "Albums",
                                            m_message = "No lyrics available";
  bool m_enabled = false, m_busy = false, m_more = false, m_actionBusy = false,
       m_active = false, m_loading = false, m_timed = false,
       m_thumbBusy = false, m_storageValid = true;
  int m_lyricIndex = -1;
  quint64 m_generation = 0;
  QVariantMap m_request{{"mode", "albums"}}, m_collection, m_reported;
  qint64 m_reportedPosition = 0;
  QVariantList m_items, m_back, m_lines;
  QHash<QString, QVariantMap> m_catalog;
  QHash<QString, QString> m_thumbs;
  QStringList m_thumbOrder, m_thumbPending;
  QString m_thumbActive;
  std::shared_ptr<QTemporaryDir> m_audio, m_art;
  QTemporaryDir m_thumbnailDirectory;
  QTimer m_save, m_reportTimer;
  LyricTimeline m_timeline;
};
