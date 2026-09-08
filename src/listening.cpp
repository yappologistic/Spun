#include "listening.h"
#include "cider.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <QUuid>
#include <QDateTime>
#include <QPointer>
#include <cmath>

static qint64 snapshotPosition(const QJsonObject &data) {
    const auto value=data.value("time").toObject().value("currentTime");const double seconds=value.toDouble(-1);
    return value.isDouble() && std::isfinite(seconds) && seconds>=0 && seconds<=604800?qint64(seconds*1000):-1;
}

QString Listening::identity(const QVariantMap &row) {
    const auto id=row.value("catalogId").toString();
    return id.isEmpty()?row.value("type").toString()+":"+row.value("id").toString():"songs:"+id;
}
QVariantMap Listening::cleanTrack(const QVariantMap &row) {
    static const QRegularExpression id("^[A-Za-z0-9._-]{1,200}$");
    if(!QStringList{"songs","library-songs"}.contains(row.value("type").toString()) || !id.match(row.value("id").toString()).hasMatch() || !row.value("playable").toBool())return {};
    QVariantMap result{{"id",row.value("id")},{"type",row.value("type")},{"playable",true}};
    for(const auto &key:QStringList{"title","artist"})result[key]=row.value(key).toString().left(500);
    if(id.match(row.value("catalogId").toString()).hasMatch())result["catalogId"]=row.value("catalogId");
    const double duration=row.value("duration").toDouble();
    if(std::isfinite(duration) && duration>0 && duration<=604800000)result["duration"]=duration;
    // Keep only small, remote artwork references; never serialize local desktop paths.
    const QUrl artwork(row.value("artwork").toString());
    if(artwork.scheme()=="https" && artwork.userInfo().isEmpty() && artwork.toString().size()<2048)result["artwork"]=artwork.toString();
    return result;
}
QVariantMap Listening::snapshotTrack(const QJsonObject &snapshot) {
    const auto attrs=snapshot.value("nowPlaying").toObject(),params=attrs.value("playParams").toObject();
    const auto kind=params.value("kind").toString("song");
    if(kind!="song" && kind!="songs" && kind!="library-song" && kind!="library-songs")return {};
    const bool library=params.value("isLibrary").toBool() || kind.startsWith("library-") || params.value("id").toString().startsWith("i.");
    auto artwork=attrs.value("artwork").toObject().value("url").toString();
    artwork.replace("{w}","96").replace("{h}","96").replace("{f}","jpg");
    return cleanTrack({{"id",params.value("id").toString()},{"type",library?"library-songs":"songs"},
        {"catalogId",params.value("catalogId").toString()},{"playable",!params.isEmpty()},
        {"title",attrs.value("name").toString()},{"artist",attrs.value("artistName").toString()},
        {"duration",attrs.value("durationInMillis").toDouble()},{"artwork",artwork}});
}
Listening::Listening(Cider *cider,QObject *parent):QObject(parent),m_cider(cider) {
    if(!cider->m_connectionPath.isEmpty())m_path=QFileInfo(cider->m_connectionPath).dir().filePath("listening.json");
    QFile file(m_path);
    if(file.exists())m_storageValid=false;
    if(file.open(QIODevice::ReadOnly) && file.size()<=4*1024*1024) {
        const auto document=QJsonDocument::fromJson(file.readAll());
        auto data=document.object();
        m_storageValid=document.isObject() && data.value("version").toInt()==1;
        if(!m_storageValid)data={};
        m_remember=data.value("rememberSession").toBool();
        QSet<QString> keys;
        for(const auto &value:data.value("bookmarks").toArray()) {
            const auto raw=value.toObject().toVariantMap();auto row=cleanTrack(raw);
            const auto key=raw.value("key").toString();const auto position=raw.value("position").toLongLong();
            if(row.isEmpty() || QUuid(key).isNull() || keys.contains(key) || position<0 || position>=row.value("duration").toLongLong() || m_bookmarks.size()>=100)continue;
            keys.insert(key);row["key"]=key;row["position"]=position;m_bookmarks.append(row);
        }
        const auto saved=data.value("session").toObject();QVariantList tracks;bool valid=true;
        for(const auto &value:saved.value("tracks").toArray()) {
            const auto row=cleanTrack(value.toObject().toVariantMap());
            if(row.isEmpty() || tracks.size()>=5000) {valid=false;break;}tracks.append(row);
        }
        const auto position=saved.value("position").toVariant().toLongLong();
        if(m_remember && valid && !tracks.isEmpty() && position>=0 && position<tracks.first().toMap().value("duration").toLongLong())
            m_session={{"tracks",tracks},{"position",position},{"savedAt",saved.value("savedAt").toString().left(40)}};
    }
    m_checkpoint.setInterval(15000);m_saveDelay.setInterval(700);m_saveDelay.setSingleShot(true);
    connect(&m_checkpoint,&QTimer::timeout,this,&Listening::checkpoint);
    connect(&m_saveDelay,&QTimer::timeout,this,&Listening::checkpoint);
    connect(cider,&Cider::queueStatusChanged,this,&Listening::queueUpdated);
    connect(cider,&Cider::controlChanged,this,&Listening::queueUpdated);
    connect(cider,&Cider::playingChanged,this,[this] { if(m_remember && m_cider->playing())m_checkpoint.start();else m_checkpoint.stop();scheduleCheckpoint(); });
    connect(cider,&Cider::trackChanged,this,[this] { if(m_remember && !m_busy) QTimer::singleShot(0,m_cider,&Cider::refreshQueue); });
    connect(cider,&Cider::connectionRestored,this,[this] { if(m_remember)m_cider->refreshQueue(); });
    if(m_remember) { if(cider->playing())m_checkpoint.start();QTimer::singleShot(0,cider,&Cider::refreshQueue); }
}
Listening::~Listening() { if(m_busy) {m_cider->m_listeningBusy=false;emit m_cider->controlChanged();} }
QVariantMap Listening::session() const {
    if(m_session.isEmpty())return {};
    const auto tracks=m_session.value("tracks").toList();
    return {{"title",tracks.first().toMap().value("title")},{"trackCount",tracks.size()},
        {"position",m_session.value("position")},{"savedAt",m_session.value("savedAt")}};
}
bool Listening::persist() {
    if(m_path.isEmpty() || !m_storageValid)return false;
    QDir().mkpath(QFileInfo(m_path).absolutePath());QSaveFile file(m_path);
    if(!file.open(QIODevice::WriteOnly))return false;
    file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"rememberSession",m_remember},
        {"bookmarks",QJsonArray::fromVariantList(m_bookmarks)},{"session",QJsonObject::fromVariantMap(m_session)}}).toJson(QJsonDocument::Compact);
    return bytes.size()<=4*1024*1024 && file.write(bytes)==bytes.size() && file.commit();
}
void Listening::setRememberSession(bool enabled) {
    if(m_remember==enabled || m_busy)return;
    const auto old=m_session;m_remember=enabled;if(!enabled)m_session.clear();
    if(!persist()) {m_remember=!enabled;m_session=old;emit feedback("Couldn’t save the session preference.",true);return;}
    if(enabled) {if(m_cider->playing())m_checkpoint.start();m_cider->refreshQueue();scheduleCheckpoint();}
    else {m_checkpoint.stop();m_saveDelay.stop();}
    emit changed();
}
void Listening::request(const QByteArray &method,const QString &path,const QJsonObject &body,std::function<void(bool,QJsonObject)> done,bool quiet) {
    const auto guard=QPointer<Listening>(this);const int generation=m_cider->m_tokenGeneration;
    m_cider->apiRequest(method,path,body,[guard,generation,done](bool ok,QJsonObject data) {
        if(!guard)return;
        if(generation!=guard->m_cider->m_tokenGeneration || (guard->m_busy && guard->m_tokenGeneration!=generation))ok=false;
        done(ok,data.value("data").toObject());
    },!quiet);
}
bool Listening::begin() {
    if(m_busy || m_cider->controlBusy() || m_cider->queueBusy() || m_cider->m_apiToken.isEmpty() || m_cider->recovering()) {
        emit feedback("Wait for Cider to connect and finish its current action.",true);return false;
    }
    m_busy=true;m_error.clear();m_cider->m_listeningBusy=true;m_tokenGeneration=m_cider->m_tokenGeneration;
    emit m_cider->controlChanged();emit changed();return true;
}
void Listening::finish(const QString &message,bool error) {
    m_inserting=false;m_restoring=false;if(!error)m_preparing=false;m_busy=false;m_target.clear();m_error=error?message:QString();
    m_cider->m_listeningBusy=false;emit m_cider->controlChanged();emit changed();emit feedback(message,error);
}
void Listening::addBookmark() {
    if(m_bookmarks.size()>=100) {emit feedback("You have 100 bookmarks. Remove one before adding another.",true);return;}
    if(!begin())return;
    request("GET","/playback",{},[this](bool ok,QJsonObject data) {
        auto row=snapshotTrack(data);const auto time=data.value("time").toObject().value("currentTime");
        const auto position=snapshotPosition(data);
        if(!ok || row.isEmpty() || !time.isDouble() || position<0 || position>=row.value("duration").toLongLong()) {finish("This song’s position is unavailable.",true);return;}
        for(const auto &value:m_bookmarks)if(identity(value.toMap())==identity(row) && qAbs(value.toMap().value("position").toLongLong()-position)<1000) {finish("This moment is already bookmarked");return;}
        row["key"]=QUuid::createUuid().toString(QUuid::WithoutBraces);row["position"]=position;m_bookmarks.prepend(row);
        if(!persist()) {m_bookmarks.removeFirst();finish("Couldn’t save the bookmark.",true);return;}
        finish("Bookmarked · Find it in Quick jump");
    });
}
void Listening::removeBookmark(const QString &key) {
    if(m_busy)return;
    for(int i=0;i<m_bookmarks.size();++i)if(m_bookmarks[i].toMap().value("key")==key) {
        const auto row=m_bookmarks.takeAt(i);
        if(!persist()) {m_bookmarks.insert(i,row);emit feedback("Couldn’t remove the bookmark.",true);}else emit feedback("Bookmark removed",false);
        emit changed();return;
    }
}
void Listening::playBookmark(const QString &key) {
    for(const auto &value:m_bookmarks)if(value.toMap().value("key")==key) {
        const auto row=value.toMap();if(begin())playAt(row,row.value("position").toLongLong(),false);return;
    }
    emit feedback("This bookmark is no longer available.",true);
}
void Listening::playAt(const QVariantMap &track,qint64 position,bool fromQueue) {
    m_target=track;m_targetPosition=position;
    request("POST",fromQueue?"/queue/jump":"/playback/play-item",fromQueue?QJsonObject{{"index",0}}:QJsonObject{{"type",track.value("type").toString()},{"id",track.value("id").toString()}},[this](bool ok,QJsonObject) {
        if(!ok) {finish("Playback wasn’t confirmed. Check Cider before retrying.",true);return;}
        QTimer::singleShot(250,this,[this]{verifyPlaying(12);});
    });
}
void Listening::verifyPlaying(int attempts) {
    request("GET","/playback",{},[this,attempts](bool ok,QJsonObject data) {
        if(!ok) {finish("Couldn’t verify the playing song; its position was left unchanged.",true);return;}
        if(identity(snapshotTrack(data))!=identity(m_target)) {
            if(attempts>0)QTimer::singleShot(250,this,[this,attempts]{verifyPlaying(attempts-1);});
            else finish("Cider didn’t start the expected song; its position was left unchanged.",true);
            return;
        }
        const double duration=data.value("time").toObject().value("duration").toDouble();
        if(duration<=0 || m_targetPosition>=duration*1000) {finish("The saved position is outside this version of the song.",true);return;}
        request("POST","/playback/seek",{{"position",m_targetPosition/1000.0}},[this](bool success,QJsonObject) {
            if(!success) {finish("Song opened, but Cider couldn’t confirm the saved position.",true);return;}
            QTimer::singleShot(350,this,&Listening::verifySeek);
        });
    });
}
void Listening::verifySeek() {
    request("GET","/playback",{},[this](bool ok,QJsonObject data) {
        const double position=data.value("time").toObject().value("currentTime").toDouble(-1)*1000;
        const bool verified=ok && identity(snapshotTrack(data))==identity(m_target) && qAbs(position-m_targetPosition)<2000;
        finish(verified?"Resumed from saved position":"Cider hasn’t confirmed the saved position. Check playback before retrying.",!verified);
        m_cider->refresh();m_cider->refreshQueue();
    });
}
void Listening::scheduleCheckpoint() { if(m_remember && !m_busy && !m_preparing && !m_saveDelay.isActive())m_saveDelay.start(); }
void Listening::checkpoint() {
    if(!m_remember || m_busy || m_reading || m_preparing || !m_cider->queueReady() || m_cider->queueBusy() || !m_cider->queueError().isEmpty() || m_cider->controlBusy() || m_cider->recovering())return;
    const auto queue=m_cider->queue();const int index=m_cider->currentIndex(),revision=m_cider->queueRevision();
    if(index<0 || index>=queue.size() || queue.size()-index>5000)return;
    m_reading=true;
    request("GET","/playback",{},[this,queue,index,revision](bool ok,QJsonObject data) {
        m_reading=false;
        if(!ok || !m_remember || m_busy || m_preparing || revision!=m_cider->queueRevision())return;
        const auto current=snapshotTrack(data);const auto time=data.value("time").toObject().value("currentTime");const auto position=snapshotPosition(data);
        if(current.isEmpty() || identity(current)!=identity(queue[index].toMap()) || !time.isDouble() || position<0 || position>=current.value("duration").toLongLong())return;
        QVariantList tracks;for(const auto &value:queue.mid(index)) {auto row=cleanTrack(value.toMap());if(row.isEmpty())return;tracks.append(row);}
        // Keep the full-duration current reference even if the queue omitted duration metadata.
        tracks[0]=current;
        if(m_session.value("tracks").toList()==tracks && m_session.value("position").toLongLong()==position)return;
        const auto previous=m_session;
        m_session={{"tracks",tracks},{"position",position},{"savedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
        if(!persist()) {m_session=previous;m_checkpoint.stop();m_error="Couldn’t save the listening session.";}else m_error.clear();
        emit changed();
    },true);
}
void Listening::prepareRecovery() {
    if(m_busy)return;
    m_preparing=true;m_error.clear();emit changed();m_cider->refreshQueue();
}
void Listening::cancelRecovery() { if(!m_busy) {m_preparing=false;scheduleCheckpoint();} }
void Listening::forgetSession() {
    if(m_busy)return;
    const auto previous=m_session;m_session.clear();
    if(!persist()) {m_session=previous;m_error="Couldn’t forget this session.";}else m_error.clear();
    m_preparing=false;emit changed();
}
void Listening::restoreSession() {
    if(m_session.isEmpty() || !begin())return;
    m_restoring=true;m_preparing=true;
    // Re-read after confirmation. Never clear or replace a nonempty remote queue.
    m_cider->refreshQueue();
}
void Listening::queueUpdated() {
    if(m_restoring && !m_cider->m_controlBusy && !m_cider->queueBusy()) {
        if(m_tokenGeneration!=m_cider->m_tokenGeneration || !m_cider->queueReady() || !m_cider->queueError().isEmpty()) {finish("Couldn’t verify Cider’s queue. Nothing else was changed.",true);return;}
        const auto tracks=m_session.value("tracks").toList();const auto queue=m_cider->queue();
        if(m_inserting) {
            bool same=tracks.size()==queue.size();for(int i=0;same && i<tracks.size();++i)same=identity(tracks[i].toMap())==identity(queue[i].toMap());
            if(!same || m_cider->currentIndex()!=-1) {finish("Recovery stopped because the queue changed. Check Cider before retrying.",true);return;}
            m_restoring=false;m_inserting=false;playAt(tracks.first().toMap(),m_session.value("position").toLongLong(),true);return;
        }
        if(!queue.isEmpty()) {finish("Cider already has a queue. Recovery won’t replace it.",true);return;}
        m_inserting=true;m_cider->m_listeningBusy=false;
        m_cider->insertQueue(tracks,0,m_cider->queueRevision());
        m_cider->m_listeningBusy=m_busy;
        if(!m_cider->m_controlBusy && m_restoring)finish("Couldn’t start session recovery.",true);
        return;
    }
    if(!m_cider->queueBusy() && !m_cider->controlBusy()) {
        if(m_observedRevision!=m_cider->queueRevision()) {m_observedRevision=m_cider->queueRevision();scheduleCheckpoint();}
        emit changed();
    }
}
