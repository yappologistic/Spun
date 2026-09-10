#pragma once
#include "ciderevents.h"
#include <QObject>
#include <QVariantMap>
#include <QImage>
#include <QTimer>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QMap>
#include <QJsonObject>
#include <QSet>
#include <functional>
#include <QFutureWatcher>

class Cider : public QObject {
    friend class Listening;
    friend class Lyrics;
    friend class Library;
    friend class MusicActions;
    Q_OBJECT
    Q_PROPERTY(QString audioQuality READ audioQuality NOTIFY audioQualityChanged)
    Q_PROPERTY(bool qualityBusy READ qualityBusy NOTIFY audioQualityChanged)
    Q_PROPERTY(bool canUndoQueue READ canUndoQueue NOTIFY queueStatusChanged)
    Q_PROPERTY(bool authorizing READ authorizing NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionMessage READ connectionMessage NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionChanged)
    Q_PROPERTY(bool recovering READ recovering NOTIFY connectionChanged)
    Q_PROPERTY(bool liveVisible READ liveVisible WRITE setLiveVisible NOTIFY liveChanged)
    Q_PROPERTY(bool liveConnected READ liveConnected NOTIFY liveChanged)
    Q_PROPERTY(bool libraryVisible READ libraryVisible WRITE setLibraryVisible NOTIFY connectionChanged)
    Q_PROPERTY(bool available READ available NOTIFY trackChanged)
    Q_PROPERTY(QString trackKey READ trackKey NOTIFY trackChanged)
    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString album READ album NOTIFY trackChanged)
    Q_PROPERTY(QString albumKey READ albumKey NOTIFY trackChanged)
    Q_PROPERTY(QVariantMap discDetails READ discDetails NOTIFY discDetailsChanged)
    Q_PROPERTY(bool discVisible READ discVisible WRITE setDiscVisible NOTIFY discDetailsChanged)
    Q_PROPERTY(bool discLoading READ discLoading NOTIFY discDetailsChanged)
    Q_PROPERTY(QString discError READ discError NOTIFY discDetailsChanged)
    Q_PROPERTY(QString format READ format CONSTANT)
    Q_PROPERTY(QImage artwork READ artwork NOTIFY artworkChanged)
    Q_PROPERTY(int count READ count NOTIFY trackChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(bool queueVisible READ queueVisible WRITE setQueueVisible NOTIFY queueStatusChanged)
    Q_PROPERTY(bool queueReady READ queueReady NOTIFY queueStatusChanged)
    Q_PROPERTY(bool queueBusy READ queueBusy NOTIFY queueStatusChanged)
    Q_PROPERTY(bool needsToken READ needsToken NOTIFY queueStatusChanged)
    Q_PROPERTY(QString queueError READ queueError NOTIFY queueStatusChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY trackChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QVariantMap audioOptions READ audioOptions NOTIFY audioOptionsChanged)
    Q_PROPERTY(bool audioBusy READ audioBusy NOTIFY audioOptionsChanged)
    Q_PROPERTY(QString audioError READ audioError NOTIFY audioOptionsChanged)
    Q_PROPERTY(bool crossfade READ crossfade NOTIFY crossfadeChanged)
    Q_PROPERTY(double crossfadeSeconds READ crossfadeSeconds NOTIFY crossfadeChanged)
    Q_PROPERTY(bool crossfadeReady READ crossfadeReady NOTIFY crossfadeChanged)
    Q_PROPERTY(bool crossfadeBusy READ crossfadeBusy NOTIFY crossfadeChanged)
    Q_PROPERTY(QString crossfadeError READ crossfadeError NOTIFY crossfadeChanged)
    Q_PROPERTY(bool autoplay READ autoplay NOTIFY settingsChanged)
    Q_PROPERTY(bool modesReady READ modesReady NOTIFY settingsChanged)
    Q_PROPERTY(bool controlBusy READ controlBusy NOTIFY controlChanged)
    Q_PROPERTY(int queueRevision READ queueRevision NOTIFY queueStatusChanged)
    Q_PROPERTY(bool launching READ launching NOTIFY launchChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY settingsChanged)
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY settingsChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool canSeek READ canSeek NOTIFY trackChanged)
    Q_PROPERTY(bool canNext READ canNext NOTIFY trackChanged)
    Q_PROPERTY(bool canPrevious READ canPrevious NOTIFY trackChanged)
public:
    explicit Cider(bool enabled = true, const QString &connectionPath = {}, const QUrl &rpcBase = QUrl("http://localhost:10767"), QObject *parent = nullptr);
    QString audioQuality() const { return m_audioQuality; }
    bool qualityBusy() const { return m_qualityBusy; }
    Q_INVOKABLE void refreshAudioQuality();
    bool canUndoQueue() const { return !m_undoTrack.isEmpty() && m_undoRevision==m_queueRevision && m_undoClock.isValid() && m_undoClock.elapsed()<8000; }
    Q_INVOKABLE void undoQueueRemoval();
    Q_INVOKABLE void insertQueue(const QVariantList &items, int index, int revision);
    bool authorizing() const { return !m_authReply.isNull(); }
    QString connectionMessage() const { return m_connectionMessage; }
    QString connectionState() const { return m_connectionState; }
    bool recovering() const { return m_connectionState=="offline" || m_connectionState=="timeout"; }
    bool libraryVisible() const { return m_libraryVisible; }
    bool liveVisible() const { return m_liveVisible; }
    bool liveConnected() const { return m_events.connected(); }
    void setLiveVisible(bool visible);
    void setLibraryVisible(bool visible);
    Q_INVOKABLE void authorize();
    Q_INVOKABLE void cancelAuthorization();
    Q_INVOKABLE void reconnect();
    bool available() const { return m_available; }
    ~Cider() override;
    QUrl artworkUrl() const { return m_artUrl; }
    QString trackKey() const { return count() ? m_track : QString{}; }
    QString title() const { return m_title.isEmpty() ? "Cider" : m_title; }
    QString artist() const { return m_artist; }
    QString album() const { return m_album; }
    QString albumKey() const { return m_album.isEmpty() ? m_track : m_album+"|"+m_artUrl.path(); }
    QVariantMap discDetails() const { return m_discDetails; }
    bool discVisible() const { return m_discVisible; }
    bool discLoading() const { return m_discLoading; }
    QString discError() const { return m_discError; }
    void setDiscVisible(bool value);
    Q_INVOKABLE void refreshDisc();
    QString format() const { return "CIDER"; }
    QImage artwork() const { return m_art; }
    int count() const { return m_available && !m_title.isEmpty() ? 1 : 0; }
    int currentIndex() const { return m_queueIndex; }
    QVariantList queue() const { return m_queue; }
    bool queueVisible() const { return m_queueVisible; }
    bool queueReady() const { return m_queueReady; }
    bool queueBusy() const { return m_queueBusy; }
    bool needsToken() const { return m_needsToken; }
    QString queueError() const { return m_queueError; }
    void setQueueVisible(bool visible);
    Q_INVOKABLE void connectQueue(const QString &token);
    Q_INVOKABLE void refreshQueue();
    Q_INVOKABLE void select(int index);
    bool playing() const { return m_playing; }
    QString playbackStatus() const { return m_available ? m_playbackStatus : QStringLiteral("Stopped"); }
    qint64 position() const;
    qint64 duration() const { return m_duration; }
    double volume() const { return m_volume; }
    bool volumeReady() const { return m_apiToken.isEmpty() || m_volumeReady; }
    QVariantMap audioOptions() const { return m_audioOptions; }
    bool audioBusy() const { return m_audioBusy; }
    QString audioError() const { return m_audioError; }
    Q_INVOKABLE void refreshAudioOptions();
    Q_INVOKABLE void setAudioOption(const QString &key, const QVariant &value);
    Q_INVOKABLE void copySongLink();
    bool crossfade() const { return m_crossfade; }
    double crossfadeSeconds() const { return m_crossfadeSeconds; }
    bool crossfadeReady() const { return m_crossfadeReady; }
    bool crossfadeBusy() const { return m_crossfadeBusy; }
    QString crossfadeError() const { return m_crossfadeError; }
    Q_INVOKABLE void refreshCrossfade();
    Q_INVOKABLE void setCrossfade(bool value);
    Q_INVOKABLE void setCrossfadeSeconds(double value);
    bool autoplay() const { return m_autoplay; }
    bool modesReady() const { return m_modesReady; }
    bool controlBusy() const { return m_controlBusy || m_modesReading || m_listeningBusy; }
    int queueRevision() const { return m_queueRevision; }
    bool launching() const { return m_launchTimer.isActive(); }
    Q_INVOKABLE void refreshModes();
    Q_INVOKABLE void setAutoplay(bool value);
    Q_INVOKABLE void moveQueue(int from, int to, int revision);
    Q_INVOKABLE void removeQueue(int index, int revision);
    Q_INVOKABLE QVariantMap previewCleanup(const QString &mode) const;
    Q_INVOKABLE void editQueueSelection(const QVariantList &indices, const QString &operation, int revision);
    Q_INVOKABLE void cleanQueue(const QString &mode, int revision);
    Q_INVOKABLE void ensureRunning();
    static QStringList launchCommand();
    bool shuffle() const { return m_shuffle; }
    int repeatMode() const { return m_repeat; }
    QString error() const { return m_error; }
    bool canSeek() const { return m_canSeek; }
    bool canNext() const { return m_canNext; }
    bool canPrevious() const { return m_canPrevious; }
    void setVolume(double value);
    void setShuffle(bool value);
    void setRepeatMode(int value);
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    void stop() { if (m_available) call("Stop"); }
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(qint64 milliseconds);
    Q_INVOKABLE void raise();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void dismissError();
signals:
    void liveChanged();
    void remoteSettingsChanged();
    void audioQualityChanged();
    void connectionChanged();
    void connectionRestored();
    void crossfadeChanged();
    void audioOptionsChanged();
    void apiFeedback(const QString &message, bool error);
    void controlChanged();
    void launchChanged();
    void discDetailsChanged();
    void queueChanged();
    void currentIndexChanged();
    void queueStatusChanged();
    void trackChanged();
    void artworkChanged();
    void playingChanged();
    void positionChanged();
    void positionDiscontinuity(qint64 position);
    void volumeChanged();
    void settingsChanged();
    void errorChanged();
private slots:
    void propertiesChanged(const QString &interface, const QVariantMap &values, const QStringList &invalidated);
    void seeked(qlonglong microseconds);
private:
    void updateEvents();
    bool m_listeningBusy=false;
    bool m_liveVisible=false, m_eventQueue=false, m_eventSettings=false;
    CiderEvents m_events;
    QTimer m_eventCoalesce;
    QString m_audioQuality;
    bool m_qualityBusy=false;
    QVariantMap m_undoTrack;
    int m_undoIndex=-1, m_undoRevision=-1;
    QElapsedTimer m_undoClock;
    QVariantList m_insertItems;
    QStringList m_insertExpected;
    int m_insertAt=0, m_insertDone=0, m_insertPosition=-1;
    void verifyAppend(const QStringList &before,int position,const QString &key,int attempts=8);
    bool m_restoringQueue=false;
    void insertNext();
    void verifyInsert(const QStringList &before,int beforePosition,std::function<void()> done,int attempts=24);
    void finishInsert(bool success);
    void observeConnection(int status, QNetworkReply::NetworkError error);
    void scheduleRecovery();
    bool saveToken();
    bool m_libraryVisible=false;
    int m_recoveryDelay=3000, m_tokenGeneration=0;
    QString m_connectionState, m_connectionMessage;
    QTimer m_recoveryTimer;
    QPointer<QNetworkReply> m_authReply, m_probeReply;
    void requestDiscJson(const QString &path, const QJsonObject &body, int generation, std::function<void(QJsonObject)> done);
    void requestDiscTracks(const QString &path, int generation);
    void failDisc(const QString &message);
    void apiRequest(const QByteArray &method, const QString &endpoint, const QJsonObject &body, std::function<void(bool,QJsonObject)> done, bool reportError=true);
    bool applyCrossfade(const QJsonObject &json);
    void changeCrossfade(const QJsonObject &patch);
    QVariantMap m_audioOptions;
    QString m_audioError;
    bool m_audioBusy=false, m_linkBusy=false;
    bool m_crossfade=false, m_crossfadeReady=false, m_crossfadeBusy=false;
    double m_crossfadeSeconds=5;
    QString m_crossfadeError;
    void changeMode(const QString &mode, bool value);
    bool applyModes(const QJsonObject &json);
    void fetchQueue();
    void editQueue(int from, int to, int revision, bool remove);
    void batchNext();
    void finishBatch(bool success);
    QList<QPair<int,int>> m_batchOps;
    QStringList m_batchExpected;
    int m_batchDone=0, m_batchPosition=-1, m_batchGeneration=0;
    void cleanupNext();
    void finishCleanup(bool success);
    QList<int> m_cleanupIndices;
    QStringList m_cleanupExpected;
    int m_cleanupDone=0, m_cleanupPosition=-1, m_cleanupGeneration=0;
    void requestQueuePage(int offset);
    void queueFailed(const QString &message, bool needsToken = false);
    void apply(const QVariantMap &values);
    void call(const QString &method, const QVariantList &args = {});
    void set(const QString &key, const QVariant &value);
    void decodeArt();
    void loadArt(const QUrl &url);
    void fail(const QString &message);
    void schedule(const QString &key, std::function<void()> action);
    void flushCommands();
    bool m_enabled = false, m_autoplay = false, m_modesReady = false, m_modesReading = false, m_controlBusy = false;
    int m_queueRevision = 0;
    QTimer m_launchTimer;
    std::function<void()> m_afterQueue;
    bool m_available = false, m_playing = false, m_shuffle = false, m_refreshing = false;
    bool m_canSeek = false, m_canNext = false, m_canPrevious = false;
    QString m_playbackStatus="Stopped";
    QString m_title, m_artist, m_album, m_track, m_error;
    QUrl m_artUrl;
    QImage m_art;
    QFutureWatcher<QImage> m_artLoader;
    QByteArray m_pendingArtBytes;
    QString m_pendingArtFile;
    quint64 m_artGeneration = 0;
    bool m_artDecodeActive = false;
    void confirmSeek(const QString &track, int generation, qint64 target, int attempts=5);
    qint64 m_position = 0, m_duration = 0, m_desktopPosition = -1, m_staleDesktopPosition = -1;
    int m_seekGeneration=0;
    bool m_positionFromApi=false;
    void refreshVolume();
    double m_volume = 1, m_desktopVolume = -1;
    bool m_volumeReading=false, m_volumeReadAgain=false, m_volumeReady=false;
    int m_repeat = 0;
    QElapsedTimer m_clock;
    QTimer m_tick, m_poll;
    QTimer m_commands;
    QElapsedTimer m_commandClock;
    QMap<QString, qint64> m_lastCommand;
    QMap<QString, std::function<void()>> m_pendingCommands;
    bool m_discVisible=false, m_discLoading=false;
    int m_discGeneration=0;
    QVariantMap m_discDetails, m_pendingDisc, m_discCache;
    QString m_discError, m_discAlbumPath, m_discCachePath;
    QSet<QString> m_discPages;
    QPointer<QNetworkReply> m_discReply;
    QVariantList m_queue, m_pendingQueue;
    int m_pendingQueuePosition = -1, m_pendingQueueTotal = -1;
    QString m_connectionPath, m_apiToken, m_queueError;
    QUrl m_rpcBase;
    int m_queueIndex = -1;
    bool m_queueRefreshPending = false;
    bool m_queueVisible = false, m_queueReady = false, m_queueBusy = false, m_needsToken = false;
    QTimer m_queuePoll;
    QNetworkAccessManager m_rpcNetwork;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_artReply;
};
