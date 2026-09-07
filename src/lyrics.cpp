#include "lyrics.h"
#include "player.h"
#include "cider.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkProxy>
#include <QUrlQuery>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <taglib/fileref.h>
#include <taglib/tpropertymap.h>
#include <algorithm>
#include <cmath>
#include <set>

void LyricTimeline::reset(const QVariantList &lines) {
    struct Event { qint64 time; int index; bool begins; };
    QList<Event> events;
    events.reserve(lines.size()*2);
    for (int i=0;i<lines.size();++i) {
        const auto row=lines[i].toMap();
        const auto start=row["start"].toLongLong(),end=row["end"].toLongLong();
        if (start<0 || (end>=0 && end<=start)) continue;
        events.append({start,i,true});
        if (end>=0) events.append({end,i,false});
    }
    std::sort(events.begin(),events.end(),[](const Event &a,const Event &b){return a.time<b.time;});
    m_changes.clear();
    std::set<int> active;
    int previous=-1;
    for (qsizetype i=0;i<events.size();) {
        const auto time=events[i].time;
        // Apply the entire boundary before choosing a row: ends are exclusive,
        // and the last matching row wins even for overlapping or unsorted lyrics.
        do {
            const auto &event=events[i++];
            if (event.begins) active.insert(event.index);
            else active.erase(event.index);
        } while (i<events.size() && events[i].time==time);
        const int current=active.empty() ? -1 : *active.rbegin();
        if (current!=previous) {m_changes.append({time,current});previous=current;}
    }
}
int LyricTimeline::indexAt(qint64 position) const {
    const auto next=std::upper_bound(m_changes.cbegin(),m_changes.cend(),position,
        [](qint64 time,const Change &change){return time<change.time;});
    return next==m_changes.cbegin() ? -1 : std::prev(next)->index;
}

static qint64 stamp(QString text) {
    text=text.trimmed();
    bool ok=false;
    if (text.endsWith("ms")) { const double n=text.chopped(2).toDouble(&ok); return ok && std::isfinite(n) && n>=0 && n<=86400000 ? qRound64(n) : -1; }
    if (text.endsWith('s')) { const double n=text.chopped(1).toDouble(&ok); return ok && std::isfinite(n) && n>=0 && n<=86400 ? qRound64(n*1000) : -1; }
    const auto parts=text.split(':');
    if (parts.size()<2 || parts.size()>3) return -1;
    double seconds=0;
    for (const auto &part:parts) { const double n=part.toDouble(&ok); if (!ok || !std::isfinite(n) || n<0) return -1; seconds=seconds*60+n; }
    return seconds<=86400 ? qRound64(seconds*1000) : -1;
}
QVariantList Lyrics::parse(const QString &text) {
    if (text.size()>256*1024) return {};
    QVariantList result;
    auto append=[&](QString line,qint64 start,qint64 end=-1) {
        line=line.trimmed();
        if (!line.isEmpty() && result.size()<2000) result.append(QVariantMap{{"text",line},{"start",start},{"end",end}});
    };
    if (text.trimmed().startsWith('<')) {
        QXmlStreamReader xml(text);
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isDTD()) return {};
            if (xml.isStartElement() && xml.name()==QStringLiteral("p")) {
                const auto start=stamp(xml.attributes().value("begin").toString());
                const auto end=stamp(xml.attributes().value("end").toString());
                append(xml.readElementText(QXmlStreamReader::IncludeChildElements).simplified(),start,end);
            }
        }
        if (xml.hasError()) return {};
    } else {
        const QRegularExpression timing("\\[([0-9]{1,3}:[0-9]{2}(?:[.:][0-9]{1,3})?)\\]");
        const auto offsetMatch=QRegularExpression("\\[offset:([+-]?[0-9]+)\\]",QRegularExpression::CaseInsensitiveOption).match(text);
        const qint64 offset=offsetMatch.hasMatch() ? qBound(-86400000LL,offsetMatch.captured(1).toLongLong(),86400000LL) : 0;
        const bool hasTiming=timing.match(text).hasMatch();
        for (const auto &line:text.split('\n')) {
            if (hasTiming) {
                auto matches=timing.globalMatch(line);
                QString words=line; words.remove(timing);
                while (matches.hasNext()) {
                    auto time=matches.next().captured(1);
                    const int lastColon=time.lastIndexOf(':');
                    if (time.count(':')==2) time[lastColon]='.';
                    const auto start=stamp(time);
                    if(start>=0)append(words,qMax<qint64>(0,start+offset));
                }
            } else if (!line.trimmed().startsWith('[')) append(line,-1);
        }
    }
    const bool timed=std::any_of(result.begin(),result.end(),[](const QVariant &v){ return v.toMap()["start"].toLongLong()>=0; });
    if (timed) {
        result.erase(std::remove_if(result.begin(),result.end(),[](const QVariant &v){return v.toMap()["start"].toLongLong()<0;}),result.end());
        std::stable_sort(result.begin(),result.end(),[](const QVariant &a,const QVariant &b){return a.toMap()["start"].toLongLong()<b.toMap()["start"].toLongLong();});
        for (int i=0;i+1<result.size();++i) {
            auto row=result[i].toMap();
            if (row["end"].toLongLong()<0) row["end"]=result[i+1].toMap()["start"];
            result[i]=row;
        }
    }
    return result;
}
int Lyrics::indexAt(const QVariantList &lines,qint64 position) {
    int current=-1;
    for (int i=0;i<lines.size();++i) {
        const auto row=lines[i].toMap(); const auto start=row["start"].toLongLong(),end=row["end"].toLongLong();
        if (start>=0 && start<=position && (end<0 || position<end)) current=i;
    }
    return current;
}
Lyrics::Lyrics(Player *player,Cider *cider,QObject *parent):QObject(parent),m_player(player),m_cider(cider) {
    m_network.setProxy(QNetworkProxy::NoProxy);
    connect(player,&Player::trackChanged,this,[this]{if(!m_remote)trackChanged();});
    connect(cider,&Cider::trackChanged,this,[this]{if(m_remote)trackChanged();});
    connect(player,&Player::positionChanged,this,[this]{if(!m_remote)updateIndex();});
    connect(cider,&Cider::positionChanged,this,[this]{if(m_remote)updateIndex();});
}
QString Lyrics::key() const { return m_remote ? "cider:"+m_cider->m_track+":"+m_cider->title() : "local:"+m_player->currentUrl().toString(); }
void Lyrics::cancel() { ++m_generation; if(m_reply)m_reply->abort(); m_loading=false; }
void Lyrics::setActive(bool value) { if(m_active==value)return; m_active=value; if(value)refresh();else cancel();emit changed(); }
void Lyrics::setRemote(bool value) { if(m_remote==value)return; m_remote=value;trackChanged(); }
void Lyrics::trackChanged() { if(m_key==key())return;cancel();m_lines.clear();m_timeline.reset({});m_timed=false;m_message.clear();m_key=key();updateIndex();if(m_active)refresh();else emit changed(); }
void Lyrics::updateIndex() {
    const int next=m_active && m_timed ? m_timeline.indexAt(m_remote?m_cider->position():m_player->position()) : -1;
    if(next!=m_index){m_index=next;emit currentIndexChanged();}
}
void Lyrics::finish(const QVariantList &lines,const QString &message) {
    m_lines=lines;m_loading=false;m_message=message.isEmpty() && lines.isEmpty() ? "No lyrics available" : message;
    m_timed=!lines.isEmpty() && lines.first().toMap()["start"].toLongLong()>=0;
    m_timeline.reset(m_timed ? lines : QVariantList{});
    if(!lines.isEmpty()){m_cache=lines;m_cacheKey=m_key;}
    emit changed();updateIndex();
}
void Lyrics::request(const QString &path,const QJsonObject &body,int generation,std::function<void(QJsonObject)> done) {
    auto url=m_cider->m_rpcBase;url.setPath(path);url.setQuery(QString());
    QNetworkRequest req(url);req.setTransferTimeout(8000);req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    if(!m_cider->m_apiToken.isEmpty())req.setRawHeader("apptoken",m_cider->m_apiToken.toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    auto *reply=body.isEmpty()?m_network.get(req):m_network.post(req,QJsonDocument(body).toJson(QJsonDocument::Compact));m_reply=reply;
    connect(reply,&QNetworkReply::readyRead,reply,[reply]{if(reply->bytesAvailable()>1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,done]{
        reply->deleteLater();if(generation!=m_generation)return;
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(status==404){finish({});return;}
        if(reply->error()!=QNetworkReply::NoError || status!=200){finish({},status==401||status==403?"Connect Cider in Queue to load lyrics.":"Could not load lyrics. Try again.");return;}
        const auto doc=QJsonDocument::fromJson(reply->readAll());
        if(!doc.isObject()){finish({},"Could not read lyrics.");return;}
        done(doc.object());
    });
}
void Lyrics::refresh() {
    cancel();if(!m_active)return;m_key=key();m_lines.clear();m_timeline.reset({});m_message.clear();m_timed=false;updateIndex();
    if(m_cacheKey==m_key && !m_cache.isEmpty()){finish(m_cache);return;}
    m_loading=true;emit changed();const int generation=m_generation;
    if(!m_remote){
        const QString path=m_player->currentUrl().toLocalFile();
        auto *watcher=new QFutureWatcher<QVariantList>(this);
        connect(watcher,&QFutureWatcher<QVariantList>::finished,this,[this,watcher,generation]{const auto result=watcher->result();watcher->deleteLater();if(generation==m_generation)finish(result);});
        watcher->setFuture(QtConcurrent::run([path]{
            if(path.isEmpty())return QVariantList{};
            const QFileInfo info(path);
            for(const auto &suffix:{".lrc",".txt"}){
                QFile file(info.absolutePath()+"/"+info.completeBaseName()+suffix);
                if(file.size()<=256*1024 && file.open(QIODevice::ReadOnly)){auto rows=parse(QString::fromUtf8(file.readAll()));if(!rows.isEmpty())return rows;}
            }
            TagLib::FileRef file(QFile::encodeName(path).constData(),false);
            if(!file.isNull())for(const auto &name:{"LYRICS","UNSYNCEDLYRICS"}){
                const auto props=file.file()->properties();
                if(props.contains(name)){auto rows=parse(QString::fromStdString(props[name].toString().to8Bit(true)));if(!rows.isEmpty())return rows;}
            }
            return QVariantList{};
        }));return;
    }
    request("/api/v2/playback/now-playing",{},generation,[this,generation](QJsonObject response){
        const auto attr=response["data"].toObject();const QUrl url(attr["url"].toString());
        QString id=attr["playParams"].toObject()["catalogId"].toString();
        if(id.isEmpty())id=attr["playParams"].toObject()["id"].toString();
        if(!QRegularExpression("^[0-9]+$").match(id).hasMatch())id=QUrlQuery(url).queryItemValue("i");
        const auto region=QRegularExpression("^/([a-zA-Z]{2})/").match(url.path());
        if(!QRegularExpression("^[0-9]+$").match(id).hasMatch() || url.host()!="music.apple.com" || !region.hasMatch()){finish({});return;}
        const auto path="/v1/catalog/"+region.captured(1)+"/songs/"+id+"/lyrics";
        request("/api/v1/amapi/run-v3",{{"path",path}},generation,[this](QJsonObject json){
            const auto data=json["data"].toObject();const auto records=data["data"].toArray();
            if(records.isEmpty()){
                const auto errors=data["errors"].toArray();
                const auto code=errors.isEmpty()?QString():errors.first().toObject()["code"].toVariant().toString();
                finish({},code=="TOKEN_INVALID"||code=="FETCH_ERROR"?"Cider could not load lyrics. Try again.":QString());return;
            }
            finish(parse(records.first().toObject()["attributes"].toObject()["ttml"].toString()));
        });
    });
}

bool Lyrics::seekToLine(int index) {
    if (!m_active || m_loading || !m_timed || key()!=m_key || index<0 || index>=m_lines.size()) return false;
    const auto start=m_lines[index].toMap().value("start",-1).toLongLong();
    const auto duration=m_remote ? m_cider->duration() : m_player->duration();
    if (start<0 || duration<=0 || start>=duration || (m_remote && !m_cider->canSeek())) return false;
    if (m_remote) m_cider->seek(start); else m_player->seek(start);
    return true;
}
