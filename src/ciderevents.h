#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QNetworkReply>
#include <QTimer>

// Socket.IO's HTTP transport uses the existing Qt network stack. One pending
// long-poll sleeps on Cider; no WebSocket or JavaScript runtime is bundled.
class CiderEvents : public QObject {
    Q_OBJECT
public:
    explicit CiderEvents(QObject *parent=nullptr);
    ~CiderEvents() override;
    void configure(bool enabled, const QUrl &base, const QByteArray &token);
    bool connected() const { return m_connected; }
signals:
    void connectedChanged();
    void event(const QString &type);
private:
    void open();
    void stop();
    void failed();
    void poll();
    void receive(const QByteArray &bytes);
    void send(const QByteArray &packet);
    void flush();
    QNetworkRequest request() const;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_poll, m_post;
    QTimer m_retry;
    QUrl m_base;
    QByteArray m_token, m_sid, m_pending;
    bool m_enabled=false, m_connected=false;
    int m_generation=0, m_retryDelay=1000, m_timeout=35000;
};
