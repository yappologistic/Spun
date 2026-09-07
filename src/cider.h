#pragma once
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
    friend class Lyrics;
    friend class Library;
    friend class MusicActions;
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY trackChanged)
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
    bool available() const { return m_available; }
    ~Cider() override;
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
    qint64 position() const;
    qint64 duration() const { return m_duration; }
    double volume() const { return m_volume; }
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
    bool controlBusy() const { return m_controlBusy || m_modesReading; }
    int queueRevision() const { return m_queueRevision; }
    bool launching() const { return m_launchTimer.isActive(); }
    Q_INVOKABLE void refreshModes();
    Q_INVOKABLE void setAutoplay(bool value);
    Q_INVOKABLE void moveQueue(int from, int to, int revision);
    Q_INVOKABLE void removeQueue(int index, int revision);
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
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(qint64 milliseconds);
    Q_INVOKABLE void raise();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void dismissError();
signals:
    void crossfadeChanged();
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
    void volumeChanged();
    void settingsChanged();
    void errorChanged();
private slots:
    void propertiesChanged(const QString &interface, const QVariantMap &values, const QStringList &invalidated);
    void seeked(qlonglong microseconds);
private:
    void requestDiscJson(const QString &path, const QJsonObject &body, int generation, std::function<void(QJsonObject)> done);
    void requestDiscTracks(const QString &path, int generation);
    void failDisc(const QString &message);
    void apiRequest(const QByteArray &method, const QString &endpoint, const QJsonObject &body, std::function<void(bool,QJsonObject)> done);
    bool applyCrossfade(const QJsonObject &json);
    void changeCrossfade(const QJsonObject &patch);
    bool m_crossfade=false, m_crossfadeReady=false, m_crossfadeBusy=false;
    double m_crossfadeSeconds=5;
    QString m_crossfadeError;
    void changeMode(const QString &mode, bool value);
    bool applyModes(const QJsonObject &json);
    void fetchQueue();
    void editQueue(int from, int to, int revision, bool remove);
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
    QString m_title, m_artist, m_album, m_track, m_error;
    QUrl m_artUrl;
    QImage m_art;
    QFutureWatcher<QImage> m_artLoader;
    QByteArray m_pendingArtBytes;
    QString m_pendingArtFile;
    quint64 m_artGeneration = 0;
    bool m_artDecodeActive = false;
    qint64 m_position = 0, m_duration = 0;
    double m_volume = 1;
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
