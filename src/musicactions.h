#pragma once
#include <QObject>
#include <QVariantMap>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <functional>
class Cider;

// Explicit user actions only; status is fetched while the current-song menu is open.
class MusicActions : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool observing READ observing WRITE setObserving NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool favorite READ favorite NOTIFY changed)
    Q_PROPERTY(bool saved READ saved NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit MusicActions(Cider *cider, QObject *parent=nullptr);
    ~MusicActions() override;
    bool observing() const { return m_observing; }
    bool busy() const { return m_busy; }
    bool ready() const { return m_ready; }
    bool favorite() const { return m_favorite; }
    bool saved() const { return m_saved; }
    QString error() const { return m_error; }
    void setObserving(bool value);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void enqueue(const QVariantMap &item, bool next);
    Q_INVOKABLE void toggleFavorite();
    Q_INVOKABLE void save();
signals:
    void changed();
    void currentChanged();
    void feedback(const QString &message, bool error);
private:
    void request(const QByteArray &method, const QString &path, const QJsonObject &body,
                 bool mutation, std::function<void(QJsonObject)> done);
    void changeCurrent(const QByteArray &method, const QString &path, const QJsonObject &body, const QString &message);
    void invalidateCurrent();
    Cider *m_cider;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_read, m_write;
    QTimer m_poll;
    int m_generation=0;
    bool m_observing=false, m_busy=false, m_ready=false, m_favorite=false, m_saved=false;
    QString m_error;
};
