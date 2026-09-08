#include "ciderevents.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <utility>

CiderEvents::CiderEvents(QObject *parent):QObject(parent) {
    m_network.setProxy(QNetworkProxy::NoProxy);
    m_retry.setSingleShot(true);
    connect(&m_retry,&QTimer::timeout,this,&CiderEvents::open);
}
CiderEvents::~CiderEvents() { m_enabled=false;stop(); }
void CiderEvents::configure(bool enabled,const QUrl &base,const QByteArray &token) {
    enabled=enabled && !token.isEmpty();
    if(enabled==m_enabled && base==m_base && token==m_token)return;
    stop();m_enabled=enabled;m_base=base;m_token=token;m_retryDelay=1000;
    if(m_enabled)open();
}
void CiderEvents::stop() {
    ++m_generation;m_retry.stop();
    for(auto reply:{m_poll,m_post})if(reply) { reply->disconnect(this);reply->abort();reply->deleteLater(); }
    m_poll=nullptr;m_post=nullptr;m_sid.clear();m_pending.clear();
    if(m_connected) { m_connected=false;emit connectedChanged(); }
}
void CiderEvents::failed() {
    stop();if(!m_enabled)return;
    m_retry.start(m_retryDelay);m_retryDelay=qMin(60000,m_retryDelay*2);
}
QNetworkRequest CiderEvents::request() const {
    auto url=m_base;url.setPath("/socket.io/");
    QUrlQuery query;query.addQueryItem("EIO","4");query.addQueryItem("transport","polling");
    if(!m_sid.isEmpty())query.addQueryItem("sid",QString::fromLatin1(m_sid));
    url.setQuery(query);
    QNetworkRequest req(url);req.setRawHeader("apptoken",m_token);req.setHeader(QNetworkRequest::ContentTypeHeader,"text/plain;charset=UTF-8");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setTransferTimeout(m_sid.isEmpty()?8000:m_timeout);return req;
}
void CiderEvents::open() { if(m_enabled)poll(); }
void CiderEvents::poll() {
    if(!m_enabled || m_poll)return;
    auto *reply=m_network.get(request());m_poll=reply;const int generation=m_generation;
    connect(reply,&QNetworkReply::readyRead,reply,[reply] { if(reply->bytesAvailable()>1024*1024)reply->abort(); });
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation] {
        reply->deleteLater();if(generation!=m_generation)return;m_poll=nullptr;
        if(reply->error()!=QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200) { failed();return; }
        const auto bytes=reply->readAll();if(bytes.isEmpty() || bytes.size()>1024*1024) { failed();return; }
        receive(bytes);
        if(generation==m_generation && m_enabled)QTimer::singleShot(0,this,&CiderEvents::poll);
    });
}
void CiderEvents::receive(const QByteArray &bytes) {
    const int generation=m_generation;
    for(const auto &packet:bytes.split('\x1e')) {
        if(generation!=m_generation)return;
        if(packet.startsWith('0') && m_sid.isEmpty()) {
            const auto data=QJsonDocument::fromJson(packet.mid(1)).object();const auto sid=data.value("sid").toString();
            if(!QRegularExpression("^[A-Za-z0-9_-]{1,200}$").match(sid).hasMatch()) { failed();return; }
            m_sid=sid.toLatin1();m_timeout=qBound(10000,data.value("pingInterval").toInt(25000)+data.value("pingTimeout").toInt(20000)+5000,120000);send("40");
        } else if(packet=="2")send("3");
        else if(packet.startsWith("40") && !m_sid.isEmpty()) {
            if(!m_connected) { m_connected=true;m_retryDelay=1000;emit connectedChanged(); }
        } else if(packet.startsWith("42") && m_connected) {
            const auto args=QJsonDocument::fromJson(packet.mid(2)).array();
            if(args.size()==2 && args[0].toString()=="API:Playback") {
                const auto type=args[1].toObject().value("type").toString();
                // Events are invalidation hints, never authoritative queue data.
                if(type.size()<128)emit event(type);
            }
        } else if(packet=="1" || packet.startsWith("41") || packet.startsWith("44")) { failed();return; }
    }
}
void CiderEvents::send(const QByteArray &packet) {
    if(!m_pending.isEmpty())m_pending+='\x1e';
    m_pending+=packet;
    if(m_pending.size()>4096) { failed();return; }flush();
}
void CiderEvents::flush() {
    if(m_post || m_pending.isEmpty() || !m_enabled)return;
    auto req=request();req.setTransferTimeout(8000);
    auto *reply=m_network.post(req,std::exchange(m_pending,{}));m_post=reply;const int generation=m_generation;
    connect(reply,&QNetworkReply::readyRead,reply,[reply] { if(reply->bytesAvailable()>4096)reply->abort(); });
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation] {
        reply->deleteLater();if(generation!=m_generation)return;m_post=nullptr;
        if(reply->error()!=QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200) { failed();return; }flush();
    });
}
