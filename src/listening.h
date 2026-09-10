#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <QJsonObject>
#include <functional>
class Cider;

// Small local references only: no audio, credentials or artwork bytes are persisted.
class Listening : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool rememberSession READ rememberSession WRITE setRememberSession NOTIFY changed)
    Q_PROPERTY(QVariantMap session READ session NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit Listening(Cider *cider, QObject *parent=nullptr);
    ~Listening() override;
    QVariantList bookmarks() const { return m_bookmarks; }
    bool busy() const { return m_busy; }
    bool rememberSession() const { return m_remember; }
    QVariantMap session() const;
    QString error() const { return m_error; }
    void setRememberSession(bool enabled);
    Q_INVOKABLE void cancelAlbumPosition();
    Q_INVOKABLE bool playAlbumPosition(const QString &albumId, const QVariantList &tracks, int index, qint64 position);
    Q_INVOKABLE void addBookmark();
    Q_INVOKABLE void removeBookmark(const QString &key);
    Q_INVOKABLE void playBookmark(const QString &key);
    Q_INVOKABLE void prepareRecovery();
    Q_INVOKABLE void cancelRecovery();
    Q_INVOKABLE void restoreSession();
    Q_INVOKABLE void forgetSession();
    // Debounced, and only while the optional recovery setting is enabled.
    void checkpoint();
    static QVariantMap cleanTrack(const QVariantMap &row);
    static QString identity(const QVariantMap &row);
signals:
    void changed();
    void feedback(const QString &message, bool error);
private:
    void request(const QByteArray &method,const QString &path,const QJsonObject &body,std::function<void(bool,QJsonObject)> done,bool quiet=false);
    bool begin();
    void finish(const QString &message,bool error=false);
    bool persist();
    void scheduleCheckpoint();
    void queueUpdated();
    void playAt(const QVariantMap &track,qint64 position,bool fromQueue);
    void verifyPlaying(int attempts);
    void verifySeek();
    static QVariantMap snapshotTrack(const QJsonObject &snapshot);
    bool m_recordOperation=false, m_recordLoading=false;
    int m_recordIndex=0, m_recordAttempts=0;
    QVariantList m_recordTracks;
    Cider *m_cider;
    QString m_path, m_error;
    QVariantList m_bookmarks;
    QVariantMap m_session, m_target;
    qint64 m_targetPosition=0;
    int m_tokenGeneration=0,m_observedRevision=-1;
    bool m_storageValid=true;
    bool m_remember=false,m_busy=false,m_reading=false,m_inserting=false,m_restoring=false,m_preparing=false;
    quint64 m_operationGeneration=0;
    QTimer m_recordTimeout;
    QTimer m_checkpoint,m_saveDelay;
};
