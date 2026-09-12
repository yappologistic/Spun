#pragma once
#include "lyrics.h"
#include "player.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <functional>

// Anonymous catalog plus a local-only library. No browser profile or account
// state.
class Youtube : public QObject {
  Q_OBJECT
  Q_PROPERTY(Player *transport READ transport CONSTANT)
  Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
  Q_PROPERTY(bool ready READ ready NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(QString heading READ heading NOTIFY changed)
  Q_PROPERTY(QString page READ page NOTIFY changed)
  Q_PROPERTY(QVariantList items READ items NOTIFY changed)
  Q_PROPERTY(QVariantList playlists READ playlists NOTIFY changed)
  Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
  Q_PROPERTY(bool canBack READ canBack NOTIFY changed)
  Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
  Q_PROPERTY(QVariantList lines READ lines NOTIFY changed)
  Q_PROPERTY(bool loading READ loading NOTIFY changed)
  Q_PROPERTY(QString message READ message NOTIFY changed)
  Q_PROPERTY(bool timed READ timed NOTIFY changed)
  Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
public:
  explicit Youtube(const QString &directory, QObject *parent = nullptr);
  ~Youtube() override;
  Player *transport() { return &m_player; }
  bool enabled() const { return m_enabled; }
  void setEnabled(bool enabled);
  bool ready() const { return m_ready; }
  bool busy() const { return m_busy; }
  QString error() const { return m_error; }
  QString heading() const { return m_heading; }
  QString page() const { return m_page; }
  QVariantList items() const { return m_items; }
  QVariantList playlists() const;
  QVariantMap current() const;
  bool canBack() const { return !m_back.isEmpty(); }
  Q_INVOKABLE void check();
  Q_INVOKABLE void search(const QString &query,
                          const QString &filter = "songs");
  Q_INVOKABLE void show(const QString &page);
  Q_INVOKABLE void open(const QVariantMap &item);
  Q_INVOKABLE void back();
  Q_INVOKABLE void playItems(const QVariantList &items, int index = 0);
  Q_INVOKABLE void playItem(const QVariantMap &item);
  Q_INVOKABLE void enqueue(const QVariantMap &item);
  Q_INVOKABLE void radio(const QVariantMap &item);
  Q_INVOKABLE bool favorite(const QString &id) const;
  Q_INVOKABLE void toggleFavorite(const QVariantMap &item);
  Q_INVOKABLE QString createPlaylist(const QString &name);
  Q_INVOKABLE void renamePlaylist(const QString &id, const QString &name);
  Q_INVOKABLE void deletePlaylist(const QString &id);
  Q_INVOKABLE void addToPlaylist(const QString &id, const QVariantMap &item);
  Q_INVOKABLE void removeFromPlaylist(const QString &id, int index);
  Q_INVOKABLE void clearHistory();
  Q_INVOKABLE void copyLink(const QVariantMap &item);
  Q_INVOKABLE void openClipboardLink();
  bool active() const { return m_lyricsActive; }
  void setActive(bool active);
  QVariantList lines() const { return m_lines; }
  bool loading() const { return m_lyricsLoading; }
  QString message() const { return m_lyricsMessage; }
  bool timed() const { return m_timed; }
  int currentIndex() const { return m_lyricIndex; }
  Q_INVOKABLE void refresh();
  Q_INVOKABLE bool seekToLine(int index);
  static QVariantMap cleanItem(const QVariantMap &item);
signals:
  void changed();
  void currentIndexChanged();
  void feedback(const QString &message, bool error);

private:
  using Callback = std::function<void(const QVariantMap &)>;
  void request(const QString &channel, const QVariantMap &request,
               Callback callback,
               std::shared_ptr<QTemporaryDir> directory = {});
  void cancel(const QString &channel);
  void browse(const QVariantMap &request, const QString &title,
              bool remember = true);
  void loadCurrent();
  void loadArt();
  void prepareNext();
  void updateLyricIndex();
  void persist();
  void localPage();
  QList<Track> tracks(const QVariantList &items);
  QVariantList queueItems() const;
  void fail(const QString &message);
  Player m_player;
  QString m_directory, m_path, m_heading = "YouTube Music", m_page = "search",
                               m_error;
  bool m_enabled = false, m_ready = false, m_busy = false, m_checked = false,
       m_storageValid = true;
  QVariantList m_items, m_favorites, m_history, m_playlists, m_back;
  QHash<QString, QVariantMap> m_catalog;
  QHash<QString, QPointer<QProcess>> m_jobs;
  QNetworkAccessManager m_network;
  QPointer<QNetworkReply> m_artReply;
  std::shared_ptr<QTemporaryDir> m_audio, m_prepared;
  QString m_preparedKey, m_recordedKey;
  QTimer m_save;
  bool m_lyricsActive = false, m_lyricsLoading = false, m_timed = false;
  QVariantList m_lines;
  QString m_lyricsMessage = "No lyrics available";
  int m_lyricIndex = -1;
  LyricTimeline m_timeline;
};
