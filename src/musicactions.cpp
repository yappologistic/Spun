#include "musicactions.h"
#include "cider.h"
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QRegularExpression>

// Endpoint contracts: ciderapp/CiderDeck src/services/cider-client.ts (Cider v2).
MusicActions::MusicActions(Cider *cider, QObject *parent) : QObject(parent), m_cider(cider) {
    m_network.setProxy(QNetworkProxy::NoProxy);
    m_poll.setInterval(2000);
    connect(&m_poll,&QTimer::timeout,this,&MusicActions::refresh);
    connect(cider,&Cider::trackChanged,this,&MusicActions::invalidateCurrent);
}
MusicActions::~MusicActions() {
    for (auto reply : {m_read,m_write}) if (reply) { reply->disconnect(this); reply->abort(); }
}
void MusicActions::invalidateCurrent() {
    ++m_generation; m_ready=false; m_disliked=false; m_favorite=false; m_saved=false; m_error.clear();
    if (m_read) { m_read->disconnect(this); m_read->abort(); m_read->deleteLater(); m_read=nullptr; }
    emit changed(); emit currentChanged();
    if (m_observing) refresh();
}
void MusicActions::setObserving(bool value) {
    if (value==m_observing) return;
    m_observing=value;
    if (value) { m_ready=false; m_error.clear(); refresh(); m_poll.start(); }
    else {
        m_poll.stop();
        if (m_read) { m_read->disconnect(this); m_read->abort(); m_read->deleteLater(); m_read=nullptr; }
    }
    emit changed();
}
void MusicActions::request(const QByteArray &method, const QString &path, const QJsonObject &body,
                           bool mutation, std::function<void(QJsonObject)> done) {
    if (m_cider->m_apiToken.isEmpty()) {
        m_batch.clear(); m_error="Connect Spun to Cider in Queue."; m_busy=false; m_ready=false;
        emit changed(); if (mutation) emit feedback(m_error,true); return;
    }
    auto url=m_cider->m_rpcBase; url.setPath("/api/v2"+path); url.setQuery(QString());
    QNetworkRequest req(url); req.setTransferTimeout(8000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("apptoken",m_cider->m_apiToken.toUtf8());
    const bool sendsBody=method!="GET" && method!="HEAD";
    if(sendsBody)req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    auto *reply=m_network.sendCustomRequest(req,method,sendsBody?QJsonDocument(body).toJson(QJsonDocument::Compact):QByteArray());
    if (mutation) m_write=reply; else m_read=reply;
    const int generation=m_generation;
    connect(reply,&QNetworkReply::readyRead,reply,[reply] { if (reply->bytesAvailable()>256*1024) reply->abort(); });
    connect(reply,&QNetworkReply::finished,this,[this,reply,mutation,generation,done] {
        reply->deleteLater();
        if (mutation) { m_write=nullptr; m_busy=false; } else m_read=nullptr;
        if (!mutation && generation!=m_generation) return;
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->readAll(); const auto json=QJsonDocument::fromJson(bytes);
        const auto object=json.object();
        if (reply->error()!=QNetworkReply::NoError || status<200 || status>=300 ||
            (!bytes.trimmed().isEmpty() && !json.isObject()) || object.contains("error") || object.contains("errors") || object.value("data").toObject().contains("errors")) {
            QString message;
            if (status==401 || status==403) message="Allow this action in Cider’s Spun access token.";
            else if (status==404 || status==405) message="This action isn’t available in your Cider version.";
            else if (reply->error()==QNetworkReply::ConnectionRefusedError) message="Open Cider and try again.";
            else message=mutation ? "Cider couldn’t confirm the action. Check Cider before retrying." : "Couldn’t read song status. Try again.";
            if (mutation && !m_batch.isEmpty()) {
                message=QString("%1 of %2 tracks confirmed. Check the queue before retrying; the last request may have reached Cider.").arg(m_batchDone).arg(m_batch.size());
                m_batch.clear();
                QTimer::singleShot(350,m_cider,[cider=m_cider] { if(cider->queueVisible())cider->refreshQueue(); });
            }
            if (generation==m_generation) { m_error=message; if (!mutation) m_ready=false; }
            emit changed(); if (mutation) emit feedback(message,true); return;
        }
        if (generation==m_generation) m_error.clear();
        done(object); emit changed();
    });
}
void MusicActions::refresh() {
    if (!m_observing || m_read || busy()) return;
    request("GET","/library/now-playing/status",{},false,[this](QJsonObject json) {
        const auto data=json.value("data").toObject();
        if (!data.value("inLibrary").isBool() || !data.value("rating").isDouble() ||
            data.value("rating").toInt() < -1 || data.value("rating").toInt() > 1) {
            m_ready=false; m_error="Song status unavailable."; return;
        }
        m_saved=data.value("inLibrary").toBool(); m_favorite=data.value("rating").toInt()==1; m_disliked=data.value("rating").toInt()==-1; m_ready=true;
    });
}
void MusicActions::enqueue(const QVariantMap &item, bool next) {
    if (busy() || m_cider->controlBusy() || !item.value("playable").toBool()) return;
    const auto type=item.value("type").toString(), id=item.value("id").toString();
    if (!QStringList{"songs","albums","playlists","library-songs","library-albums","library-playlists"}.contains(type) ||
        !QRegularExpression("^[A-Za-z0-9._-]+$").match(id).hasMatch()) return;
    m_busy=true; m_error.clear(); emit changed();
    request("POST",next ? "/queue/add-next" : "/queue/add-later",{{"type",type},{"id",id}},true,[this,next](QJsonObject) {
        emit feedback(next ? "Playing next" : "Added to queue",false);
        QTimer::singleShot(350,m_cider,[cider=m_cider] { if (cider->queueVisible()) cider->refreshQueue(); });
    });
}
void MusicActions::changeCurrent(const QByteArray &method, const QString &path, const QJsonObject &body, const QString &message) {
    if (!m_observing || !m_ready || busy()) return;
    // A status read already in flight must not restore the pre-action state.
    if (m_read) { m_read->disconnect(this); m_read->abort(); m_read->deleteLater(); m_read=nullptr; }
    const int generation=m_generation;
    m_busy=true; m_error.clear(); m_ready=false; emit changed();
    request(method,path,body,true,[this,generation,message](QJsonObject) {
        emit feedback(generation==m_generation ? message : "Previous song updated",false);
        // Read back the server state, including any add-to-library side effects of Favorite.
        refresh();
    });
}
void MusicActions::toggleFavorite() {
    changeCurrent(m_favorite ? "PUT" : "POST",m_favorite ? "/library/now-playing/rating" : "/library/now-playing/love",
                  m_favorite ? QJsonObject{{"rating",0}} : QJsonObject{},m_favorite ? "Removed from favorites" : "Added to favorites");
}
void MusicActions::save() {
    if (!m_saved) changeCurrent("POST","/library/now-playing/add",{},"Saved to library");
}

void MusicActions::enqueueMany(const QVariantList &items, bool next) {
    if(busy() || m_cider->controlBusy() || items.isEmpty())return;
    static const QRegularExpression idPattern("^[A-Za-z0-9._-]+$");
    for(const auto &value:items) {
        const auto row=value.toMap();
        if(!row.value("playable").toBool() || !QStringList{"songs","library-songs"}.contains(row.value("type").toString()) ||
            !idPattern.match(row.value("id").toString()).hasMatch()) {
            emit feedback("Select available songs to add to the queue.",true); return;
        }
    }
    m_batch=items; m_batchNext=next; m_batchDone=0; m_error.clear(); emit changed(); enqueueBatchItem();
}
void MusicActions::enqueueBatchItem() {
    if(m_batchDone>=m_batch.size()) {
        const int total=m_batch.size();m_batch.clear();m_busy=false;emit changed();
        emit feedback(QString(m_batchNext?"%1 tracks playing next":"%1 tracks added to queue").arg(total),false);
        QTimer::singleShot(350,m_cider,[cider=m_cider] { if(cider->queueVisible())cider->refreshQueue(); });return;
    }
    // Each add-next inserts at the front; send in reverse so the final queue
    // matches the visual selection order. Never replay an uncertain mutation.
    const auto row=m_batch[m_batchNext?m_batch.size()-1-m_batchDone:m_batchDone].toMap();
    m_busy=true;
    request("POST",m_batchNext?"/queue/add-next":"/queue/add-later",{{"type",row.value("type").toString()},{"id",row.value("id").toString()}},true,
        [this](QJsonObject) { ++m_batchDone; enqueueBatchItem(); });
}

void MusicActions::toggleDislike() {
    changeCurrent(m_disliked ? "DELETE" : "POST",m_disliked ? "/library/now-playing/rating" : "/library/now-playing/dislike",
                  {},m_disliked ? "Rating cleared" : "Suggesting less like this");
}
