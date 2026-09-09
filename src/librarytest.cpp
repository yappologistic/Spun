#include "librarytest.h"
#include "testinput.h"
#include "library.h"
#include "listening.h"
#include "musicactions.h"
#include "cider.h"
#include "player.h"
#include <QQmlContext>
#include <QQmlProperty>
#include <QQmlEngine>
#include <QDir>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QFile>
#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QTest>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <iostream>

int exerciseCiderEvents(const QString &temp);

int exerciseLibrary(QQuickWindow *window, const QString &temp, const QString &captures) {
    if(!captures.isEmpty())QDir().mkpath(captures);
    int recommendationReads=0,discoveryDelay=0,discoveryStatus=200,discographyDelay=0;bool emptyRecommendations=false,malformedRecommendations=false,hostileRecommendations=false,failRecommendationPage=false,rejectDiscography=false,emptyDiscography=false;
    bool tailFixture=false,listeningFixture=false,snapshotFailure=false,ignoreSeek=false,wrongSong=false;
    QString snapshotId="b",snapshotType="songs";double snapshotPosition=12.345;int snapshotReads=0,seekWrites=0,delayedAppendMs=0;
    int failures=0, plays=0, reads=0; bool failPage=false, hostilePage=false, rejectPlay=false, rejectRead=false, manyRows=false;
    bool pagedTracks=false, failTracks=false, slowTracks=false, hostileTracks=false, renamedTracks=false; int historyReads=0;
    bool failReleases=false, slowReleases=false; int releaseReads=0, radioReads=0;
    bool slowTop=false, rejectTop=false, emptyTop=false, hostileTop=false;
    bool playedShuffle=false, transactionQueue=false, changeQueueOnRead=false, changeAfterAppend=false; int queueWrites=0,qualityReads=0;
    QJsonObject quality{{"flavorLabel","AAC 256kbps"},{"deviceAudioConfig",QJsonObject{{"sampleRate",48000},{"channelCount",2}}}};
    QString playedType, playedId, playedPath, lastTerm, lastBrowsePath;
    QString stationContext;bool rejectStation=false,unconfirmedStation=false,stationPlaying=true;int stationChecks=0,stationDelayChecks=0;
    QJsonArray fixtureQueue; int fixturePosition=-1;
    bool radioTailInsertion=false,noRelatedTracks=false,otherTrackError=false;
    int jumps=0,jumpedIndex=-1,jumpDelay=0; bool rejectJump=false;
    int queueReads=0,queueDelay=0;int deleteWrites=0,rejectDeleteAt=-1; QList<int> deletedIndices;
    bool changeAfterDelete=false,advanceAfterDelete=false,failAfterDelete=false,failQueueRead=false;
    double normalizedVolume=.55;int volumeWrites=0,volumeReads=0;bool rejectVolume=false,seekFixture=false;
    bool automix=false, audioUnavailable=false, rejectAudio=false; QString listeningMode="off"; int audioReads=0,audioWrites=0; QJsonObject audioPatch;
    QString shareUrl="https://music.apple.com/ca/album/example/100?i=123&ls=1"; int shareDelay=0;
    const auto originalClipboard=QGuiApplication::clipboard()->text();
    int edits=0, statusReads=0, rating=0, editDelay=0;
    bool crossfade=false, rejectCrossfade=false, malformedCrossfade=false; double fadeSeconds=5; int fadeReads=0, fadeWrites=0; QJsonObject fadePatch;
    bool saved=false, rejectEdit=false, malformedStatus=false;
    int authCode=200, authDelay=0, authRequests=0, probes=0; bool malformedAuth=false, rejectProbe=false; QJsonObject authBody;
    QString editPath, editMethod; QJsonObject editBody; QStringList queueIds; int rejectQueueAt=-1;
    auto check=[&](bool ok,const char *name) { std::cout<<(ok?"PASS ":"FAIL ")<<name<<std::endl; if(!ok)++failures; };
    auto wait=[&](const std::function<bool()> &condition) { QElapsedTimer t;t.start();while(!condition()&&t.elapsed()<10000)QTest::qWait(25);return condition(); };
    auto resource=[](QString id,QString type,QString title,bool playable=true) {
        QJsonObject attrs{{"name",title},{"artistName","Spun Sound Lab"},{"trackCount",2},{"durationInMillis",32000}};
        if(playable)attrs["playParams"]=QJsonObject{{"id",id}};
        return QJsonObject{{"id",id},{"type",type},{"attributes",attrs}};
    };
    QTcpServer server;check(server.listen(QHostAddress::LocalHost),"library fixture starts");
    QObject::connect(&server,&QTcpServer::newConnection,&server,[&] {
        auto *s=server.nextPendingConnection();
        QObject::connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);
        QObject::connect(s,&QTcpSocket::readyRead,s,[&,s] {
            auto request=s->property("buffer").toByteArray()+s->readAll();s->setProperty("buffer",request);
            int split=request.indexOf("\r\n\r\n");if(split<0)return;
            int length=0;for(const auto &line:request.left(split).toLower().split('\n'))if(line.startsWith("content-length:"))length=line.mid(15).trimmed().toInt();
            if(request.size()<split+4+length)return;
            auto body=QJsonDocument::fromJson(request.mid(split+4,length)).object();
            const QString endpoint=QString::fromUtf8(request.split(' ').value(1));
            QJsonObject response{{"data",QJsonObject{}}}; int code=200, delay=0;
            const bool mutation=request.startsWith("POST ")||request.startsWith("PUT ")||request.startsWith("PATCH ")||request.startsWith("DELETE ");
            if(mutation && request.left(split).toLower().contains("content-type: application/json") && length==0)code=400;
            else if(endpoint=="/api/v2/auth/request") {
                ++authRequests; authBody=body; code=authCode; delay=authDelay;
                response["data"]=malformedAuth?QJsonObject{}:QJsonObject{{"token","test-library-token"},{"scopes",QJsonArray{"playback","queue","library","audio"}}};
            } else if(!request.left(split).contains("test-library-token"))code=403;
            else if(endpoint=="/api/v2/client/info") { ++probes; if(rejectProbe)code=401; response["data"]=QJsonObject{{"version","4.0.9"}}; }
            else if(endpoint=="/api/v2/playback/play-href") {
                ++plays;playedPath=endpoint;playedId=body["href"].toString().section('/',-1);playedType="stations";stationChecks=0;
                if(rejectStation)code=403;else stationContext=playedId;
            }
            else if(endpoint=="/api/v2/queue/position") {response["data"]=QJsonObject{{"position",0},{"total",1}};}
            else if(endpoint=="/api/v2/queue?offset=0&limit=1") {
                ++stationChecks;response["data"]=QJsonObject{{"items",QJsonArray{QJsonObject{{"containerContext",QJsonObject{{"id",unconfirmedStation||stationChecks<=stationDelayChecks?"different":stationContext}}}}}},{"position",0}};
            }
            else if(endpoint.startsWith("/api/v2/queue?")) { ++queueReads;delay=queueDelay;if(failQueueRead)code=500; if(changeQueueOnRead) { changeQueueOnRead=false;fixtureQueue.append(QJsonObject{{"track",resource("external","songs","External")}}); } response["data"]=QJsonObject{{"items",fixtureQueue},{"position",fixturePosition}}; response["meta"]=QJsonObject{{"total",fixtureQueue.size()}}; }
            else if(endpoint=="/api/v2/queue/jump") {
                ++jumps; jumpedIndex=body["index"].toInt(-1); delay=jumpDelay;
                if(rejectJump)code=503;
                else if(jumpedIndex<0 || jumpedIndex>=fixtureQueue.size())code=400;
                else {fixturePosition=jumpedIndex;if(listeningFixture) {const auto track=fixtureQueue[jumpedIndex].toObject()["track"].toObject();snapshotId=track["id"].toString();snapshotType=track["type"].toString();snapshotPosition=0;}}
            }
            else if(listeningFixture && endpoint=="/api/v2/playback") {
                ++snapshotReads;if(snapshotFailure)code=503;
                auto attrs=resource(wrongSong?"wrong":snapshotId,snapshotType,"Bookmarked <song>")["attributes"].toObject();
                attrs["playParams"]=QJsonObject{{"id",wrongSong?"wrong":snapshotId},{"kind","song"},{"isLibrary",snapshotType=="library-songs"}};
                response["data"]=QJsonObject{{"state","playing"},{"nowPlaying",attrs},{"time",QJsonObject{{"currentTime",snapshotPosition},{"duration",32.0}}}};
            }
            else if((listeningFixture || seekFixture) && endpoint=="/api/v2/playback/seek") {++seekWrites;if(!ignoreSeek)snapshotPosition=body["position"].toDouble();}
            else if(endpoint=="/api/v2/playback") {response["data"]=QJsonObject{{"state",stationPlaying?"playing":"paused"},{"time",QJsonObject{{"currentTime",snapshotPosition}}}};}
            else if(endpoint=="/api/v2/playback/audio-quality") { ++qualityReads;response["data"]=quality; }
            else if(transactionQueue && (endpoint.startsWith("/api/v2/queue/items/") || endpoint=="/api/v2/queue/move")) {
                ++queueWrites;
                if(rejectEdit)code=403;
                else if(endpoint.startsWith("/api/v2/queue/items/")) {
                    const int index=endpoint.section('/',-1).toInt();
                    ++deleteWrites;deletedIndices.append(index);
                    if(deleteWrites==rejectDeleteAt)code=500;
                    else if(index<0 || index>=fixtureQueue.size())code=400;
                    else { fixtureQueue.removeAt(index);if(index<fixturePosition)--fixturePosition;else if(index==fixturePosition)fixturePosition=qMin(fixturePosition,int(fixtureQueue.size())-1);
                        if(changeAfterDelete) { changeAfterDelete=false;changeQueueOnRead=true; }
                        if(advanceAfterDelete) { advanceAfterDelete=false;++fixturePosition; }
                        if(failAfterDelete) { failAfterDelete=false;failQueueRead=true; }
                    }
                } else {
                    const int from=body["from"].toInt(),to=body["to"].toInt();
                    if(from<0 || to<0 || from>=fixtureQueue.size() || to>=fixtureQueue.size())code=400;
                    else { const auto row=fixtureQueue.takeAt(from);fixtureQueue.insert(to,row);
                        if(fixturePosition==from)fixturePosition=to;else if(from<fixturePosition && to>=fixturePosition)--fixturePosition;else if(from>fixturePosition && to<=fixturePosition)++fixturePosition;
                    }
                }
            }
            else if(endpoint=="/api/v2/playback/now-playing") { response["data"]=QJsonObject{{"url",shareUrl}};delay=shareDelay; }
            else if(endpoint=="/api/v2/audio/volume") {
                if(request.startsWith("PATCH")) { ++volumeWrites;if(rejectVolume)code=403;else normalizedVolume=body["volume"].toDouble(); }
                else ++volumeReads;
                response["data"]=QJsonObject{{"volume",normalizedVolume}};
            }
            else if(endpoint=="/api/v2/audio/automix" || endpoint=="/api/v2/audio/listening-mode") {
                if(request.startsWith("PATCH")) { ++audioWrites;audioPatch=body;
                    if(rejectAudio)code=403;else if(endpoint.endsWith("automix"))automix=body["enabled"].toBool();else if(!QStringList{"off","game","antifatigue"}.contains(body["mode"].toString()))code=400;else listeningMode=body["mode"].toString();
                } else ++audioReads;
                if(audioUnavailable)code=404;
                response["data"]=endpoint.endsWith("automix")?QJsonObject{{"enabled",automix},{"vocalGuard",true}}:QJsonObject{{"mode",listeningMode},{"available",true}};
            }
            else if(endpoint=="/api/v2/library/now-playing/status") {
                ++statusReads;
                response["data"]=malformedStatus ? QJsonObject{} : QJsonObject{{"inLibrary",saved},{"rating",rating}};
                delay=editDelay;
            } else if(endpoint.startsWith("/api/v2/library/now-playing/") || endpoint.startsWith("/api/v2/queue/add-")) {
                ++edits; editPath=endpoint; editBody=body; editMethod=QString::fromUtf8(request.split(' ').first()); delay=editDelay;
                if(endpoint.startsWith("/api/v2/queue/add-")) queueIds.append(body["id"].toString());
                if(rejectEdit || (rejectQueueAt>=0 && queueIds.size()==rejectQueueAt)) code=403;
                else if(transactionQueue && endpoint.endsWith("/add-later")) { if(changeAfterAppend) { changeQueueOnRead=true;changeAfterAppend=false; } ++queueWrites;const auto row=QJsonObject{{"track",resource(body["id"].toString(),body["type"].toString(),body["id"].toString())}};
                    if(radioTailInsertion)fixtureQueue.insert(fixturePosition+1,row);
                    else if(delayedAppendMs)QTimer::singleShot(delayedAppendMs,&server,[&,row]{fixtureQueue.append(row);});else fixtureQueue.append(row); }
                else if(endpoint.endsWith("/dislike")) rating=-1;
                else if(endpoint.endsWith("/love")) rating=1;
                else if(endpoint.endsWith("/rating")) rating=body["rating"].toInt();
                else if(endpoint.endsWith("/add")) saved=true;
            }
            else if(endpoint=="/api/v2/audio/crossfade") {
                if(request.startsWith("PATCH")) {
                    ++fadeWrites; fadePatch=body;
                    if(!rejectCrossfade) {
                        if(body.contains("enabled"))crossfade=body["enabled"].toBool();
                        if(body.contains("durationSec"))fadeSeconds=body["durationSec"].toDouble();
                    }
                } else ++fadeReads;
                if(rejectCrossfade)code=403;
                response["data"]=malformedCrossfade?QJsonObject{}:QJsonObject{{"enabled",crossfade},{"durationSec",fadeSeconds},{"exclusions",QJsonObject{{"albums",true}}}};
            }
            else if(endpoint=="/api/v1/amapi/run-v3") {
                ++reads;lastBrowsePath=body["path"].toString(); const QUrl path(lastBrowsePath); const QUrlQuery query(path); const int offset=query.queryItemValue("offset").toInt();
                QJsonObject data; QJsonArray rows;
                if(path.path().startsWith("/v1/me/recommendations")) {
                    ++recommendationReads;delay=discoveryDelay;code=discoveryStatus;
                    const auto catalog=[&](const QString &id,const QString &type,const QString &name) {auto row=resource(id,type,name);row["href"]="/v1/catalog/ca/"+type+"/"+id;return row;};
                    if(!emptyRecommendations) {
                        if(offset) {
                            if(failRecommendationPage)code=503;
                            rows={catalog(path.path().endsWith("contents")?"late":"pl.later",path.path().endsWith("contents")?"albums":"playlists","Later recommendation")};
                        } else {
                            const auto album=catalog("recommended","albums","Recommended <album>");
                            QJsonObject nested{{"type","personal-recommendation"},{"relationships",QJsonObject{{"contents",QJsonObject{{"data",QJsonArray{catalog("ra.mix","stations","Your station")}}}}}}};
                            auto bad=catalog("bad","albums","Bad route");bad["href"]="https://example.invalid/credentials";
                            QJsonObject group{{"id","group1"},{"type","personal-recommendation"},{"attributes",QJsonObject{{"title",QJsonObject{{"stringForDisplay","Made for you"}}}}},
                                {"relationships",QJsonObject{{"contents",QJsonObject{{"data",QJsonArray{album,catalog("pl.mix","playlists","Weekly mix"),album,nested,bad}},{"next",hostileRecommendations?"https://example.invalid/steal":"/v1/me/recommendations/group1/contents?offset=1"}}}}}};
                            rows={group};data["next"]="/v1/me/recommendations?offset=1";
                        }
                    }
                    if(malformedRecommendations)data["data"]="invalid";
                }
                else if(path.path()=="/v1/me/storefront") rows.append(QJsonObject{{"id","ca"}});
                else if(path.path().endsWith("/search")) {
                    const QString type=query.queryItemValue("types");lastTerm=query.queryItemValue("term",QUrl::FullyDecoded);
                    if(lastTerm=="slow")delay=600;
                    if(lastTerm!="empty")rows.append(resource(type=="stations"?"ra.station":"123",type,lastTerm));
                    if(type=="artists" && lastTerm=="Shared name")rows.append(resource("456",type,lastTerm));
                    data={{"results",QJsonObject{{type,QJsonObject{{"data",rows}}}}}};
                } else if(path.path()=="/v1/me/recent/played/tracks") {
                    ++historyReads;
                    rows={resource(offset?"i.older":"recent-new","songs",offset?"Older memory":"Latest song")};
                    if(offset)rows[0]=resource("i.older","library-songs","Older memory");
                    if(!offset)data["next"]="/v1/me/recent/played/tracks?types=songs,library-songs&offset=1";
                } else if(path.path().endsWith("/songs/999/station")) {
                    ++radioReads; data["errors"]=QJsonArray{QJsonObject{{"code","40403"}}};
                } else if(path.path().endsWith("/station")) {
                    ++radioReads;if(path.path().contains("/777/"))delay=500;
                    rows={resource("ra.seed","stations","Song radio")};
                } else if(path.path().endsWith("/artists") && query.hasQueryItem("views")) {
                    ++releaseReads;if(failReleases)code=503;if(slowReleases)delay=500;
                    for(const auto &id:query.queryItemValue("ids").split(',')) {
                        auto release=resource("album"+id,"albums","Release "+id);auto attrs=release["attributes"].toObject();attrs["releaseDate"]=id=="123"?"2026-01-01":"2026-08-01";release["attributes"]=attrs;
                        QJsonObject views;views["latest-release"]=QJsonObject{{"data",QJsonArray{release}}};
                        rows.append(QJsonObject{{"id",id},{"type","artists"},{"views",views}});
                    }
                } else if(path.path().endsWith("/view/similar-artists")) {
                    rows={resource("456","artists","Related artist",false)};
                } else if(path.path().contains("/artists/") && path.path().endsWith("/view/top-songs")) {
                    if(slowTop)delay=450;
                    if(rejectTop)code=404;
                    if(!emptyTop)rows={resource(offset?"top2":"top1","songs",offset?"Second hit":"First hit")};
                    if(!offset&&!emptyTop)data["next"]=hostileTop?"https://example.invalid/steal":path.path()+"?offset=1";
                } else if(path.path().contains("/artists/") && path.path().endsWith("/albums")) {
                    rows={resource(offset?"456":"234","albums",offset?"Evening sketches":"First Light")};
                    if(!offset)data["next"]=path.path()+"?offset=1";
                } else if(path.path().endsWith("/view/full-albums") || path.path().endsWith("/view/singles") || path.path().endsWith("/view/live-albums")) {
                    delay=discographyDelay;if(rejectDiscography)code=404;
                    if(!emptyDiscography) {rows={resource(offset?"filter2":"filter1","albums",path.path().section('/',-1)+(offset?" second":" first"))};if(!offset)data["next"]=path.path()+"?offset=1";}
                } else if(path.path().endsWith("/tracks")) {
                    const auto type=path.path().contains("/me/")?"library-songs":"songs";
                    rows={resource("i.first",type,renamedTracks?"Renamed café":"First Light"),resource("i.second",type,"Second Light"),resource("i.unavailable",type,"Unavailable",false)};
                    if(tailFixture)rows={resource("repeat",type,"Repeat"),resource("repeat",type,"Repeat"),resource("middle",type,"Middle")};
                    if(pagedTracks) {
                        if(!offset)data["next"]=hostileTracks?"https://example.invalid/tracks":path.path()+"?offset=1";
                        else { rows={resource("i.hidden",type,"Café Afterglow")}; if(failTracks)code=503; if(slowTracks)delay=400; }
                    }
                } else if(path.path().startsWith("/v1/catalog/")) {
                    const auto parts=path.path().split('/');
                    rows.append(resource(parts.last(),parts.value(4),"Linked music"));
                    if(parts.last()=="999")delay=600;
                } else if(path.path()=="/v1/me/library/songs") {
                    rows.append(resource(offset?"i.saved2":"i.saved1","library-songs",offset?"Another saved song":"Saved song"));
                    if(!offset)data["next"]="/v1/me/library/songs?offset=1";
                } else {
                    const bool albums=path.path().endsWith("/albums");const QString type=albums?"library-albums":"library-playlists";
                    rows.append(resource(offset?"l.second":albums?"l.first":"p.first",type,offset?"Second album":albums?"The first mixtape":"Quiet evenings"));
                    if(manyRows && albums && !offset) for(int i=1;i<12;++i) rows.append(resource("l.album"+QString::number(i),type,"Mixtape "+QString::number(i)));
                    if(albums&&!offset)data["next"]=hostilePage?"https://example.invalid/steal":"/v1/me/library/albums?offset=1";
                    if(offset&&failPage)code=503;
                }
                if(!data.contains("results"))data["data"]=path.path().startsWith("/v1/me/recommendations")&&malformedRecommendations?QJsonValue("invalid"):QJsonValue(rows);
                if(path.path().endsWith("/tracks") && noRelatedTracks)data=QJsonObject{{"errors",QJsonArray{QJsonObject{{"code",otherTrackError?"50000":"40403"},{"status","404"}}}}};
                response["data"]=data;if(rejectRead)code=403;
            } else if(endpoint.startsWith("/api/v2/playback/")) {
                ++plays;playedShuffle=body["shuffle"].toBool();playedId=body["id"].toString();playedType=body["type"].toString();playedPath=endpoint;if(rejectPlay)code=422;
                if(listeningFixture && !rejectPlay && endpoint.endsWith("play-item")) {snapshotId=playedId;snapshotType=playedType;snapshotPosition=0;}
            }
            QByteArray bytes=QJsonDocument(response).toJson(QJsonDocument::Compact);
            auto send=[s,bytes,code] { s->write("HTTP/1.1 "+QByteArray::number(code)+" Result\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "+QByteArray::number(bytes.size())+"\r\n\r\n"+bytes);s->disconnectFromHost(); };
            QTimer::singleShot(delay,s,send);
        });
    });
    const QString config=temp+"/library-connection.json";
    QFile token(config);check(token.open(QIODevice::WriteOnly),"test token file opens");token.write("{\"token\":\"test-library-token\"}");token.close();
    Cider cider(false,config,QUrl(QString("http://127.0.0.1:%1").arg(server.serverPort())));
    Library browser(&cider); browser.setActive(true);browser.setSection("albums");
    check(wait([&]{return !browser.busy();})&&browser.items().size()==1&&browser.hasMore(),"library albums load through the existing Cider token");
    failPage=true;browser.more();check(wait([&]{return !browser.busy();})&&!browser.error().isEmpty()&&browser.items().size()==1,"failed pagination preserves existing library rows");
    failPage=false;browser.more();check(wait([&]{return !browser.busy();})&&browser.items().size()==2&&!browser.hasMore(),"library pagination appends the next page once");
    browser.open(0);check(wait([&]{return !browser.busy();})&&browser.items().size()==3&&plays==0,"opening an album loads tracks without changing playback");
    noRelatedTracks=true;browser.reload();check(wait([&]{return !browser.busy();})&&browser.items().isEmpty()&&browser.error().isEmpty(),"no-related-resources library album is a normal empty list");
    otherTrackError=true;browser.reload();check(wait([&]{return !browser.busy();})&&!browser.error().isEmpty(),"other upstream track errors remain visible instead of becoming empty lists");
    noRelatedTracks=false;otherTrackError=false;browser.reload();wait([&]{return !browser.busy();});
    browser.play(1);check(wait([&]{return !browser.starting();})&&playedType=="library-songs"&&playedId=="i.second"&&playedPath.endsWith("play-item"),"library track playback sends its actual resource ID and type");
    browser.play(2);QTest::qWait(50);check(plays==1,"unavailable tracks cannot start playback");
    browser.playCollection();check(wait([&]{return !browser.starting();})&&playedType=="library-albums"&&playedId=="l.first"&&playedPath.endsWith("play-collection"),"album Play uses Cider's collection playback endpoint");
    browser.back();check(browser.items().size()==2&&browser.collection().isEmpty(),"Back restores all previously loaded albums");
    browser.setSection("playlists");wait([&]{return !browser.busy();});browser.open(0);wait([&]{return !browser.busy();});
    browser.playCollection();check(wait([&]{return !browser.starting();})&&playedType=="library-playlists"&&playedId=="p.first","playlist playback preserves its library identity");
    rejectPlay=true;browser.playCollection();check(wait([&]{return !browser.starting();})&&!browser.playError().isEmpty(),"Cider playback failures remain visible and retryable");rejectPlay=false;
    browser.back();browser.setQuery("A & B");check(wait([&]{return !browser.busy();})&&lastTerm=="A & B","library search safely encodes typed text");
    browser.setSection("search");browser.setQuery("slow");QTest::qWait(450);browser.setQuery("new");
    check(wait([&]{return !browser.busy();})&&browser.items().value(0).toMap()["title"]=="new","late search responses cannot replace newer results");
    browser.play(0);check(wait([&]{return !browser.starting();})&&playedType=="songs"&&playedId=="123","catalog song playback sends catalog IDs");
    browser.setKind("albums");wait([&]{return !browser.busy();});browser.open(0);wait([&]{return !browser.busy();});browser.playCollection();
    check(wait([&]{return !browser.starting();})&&playedType=="albums","catalog album playback uses catalog collection type");browser.back();
    browser.setKind("playlists");wait([&]{return !browser.busy();});browser.open(0);wait([&]{return !browser.busy();});browser.playCollection();
    check(wait([&]{return !browser.starting();})&&playedType=="playlists","catalog playlist playback uses catalog collection type");browser.back();
    browser.setKind("stations");browser.setQuery("Chill");
    check(wait([&]{return !browser.busy();})&&browser.items().first().toMap()["type"]=="stations","station search uses the catalog through the existing token");
    const auto station=browser.items().first().toMap();browser.open(0);
    check(wait([&]{return !browser.starting();})&&playedType=="stations"&&playedId=="ra.station"&&browser.collection().isEmpty(),"station row starts station playback without requesting a track list");
    browser.togglePin(station);Library stationReload(&cider);
    check(stationReload.pins().size()==1&&stationReload.pins().first().toMap()["type"]=="stations","station pins survive relaunch");
    browser.playPin(0);check(wait([&]{return !browser.starting();})&&playedType=="stations","station pin starts its original station identity");browser.togglePin(station);
    check(Library::linkPath("https://music.apple.com/ca/station/chill/ra.123")=="/v1/catalog/ca/stations/ra.123","Apple Music station links resolve safely");
    browser.setKind("playlists");wait([&]{return !browser.busy();});
    const int artistPlays=plays;
    browser.showArtist("Spun Sound Lab");
    check(wait([&]{return !browser.busy();})&&browser.collection()["type"]=="artists"&&browser.items().first().toMap()["type"]=="albums","unique artist lookup opens its catalog albums");
    const auto artistPin=browser.collection();browser.togglePin(artistPin);
    Library restoredArtistPins(&cider);
    check(browser.isPinned(artistPin)&&restoredArtistPins.pins().size()==1&&restoredArtistPins.pins().first().toMap()["type"]=="artists","artist pins persist without changing playback");
    auto invalidArtist=artistPin;invalidArtist["path"]="https://example.com/artists/123";browser.togglePin(invalidArtist);
    check(browser.pins().size()==1,"artist pins reject external resource paths");
    browser.playCollection();browser.shuffleCollection(artistPin);QTest::qWait(30);
    check(plays==artistPlays,"artist resources never dispatch collection playback");
    browser.togglePin(artistPin);check(browser.pins().isEmpty(),"artist pins can be removed independently");
    browser.setArtistView("songs");
    check(wait([&]{return !browser.busy();})&&browser.items().size()==1&&browser.items().first().toMap()["id"]=="top1"&&lastBrowsePath.endsWith("/view/top-songs?limit=25"),"artist Top songs uses the existing Cider proxy");
    browser.more();check(wait([&]{return !browser.busy();})&&browser.items().size()==2,"artist top songs paginate in server order");
    browser.setCollectionQuery("Second");check(browser.items().size()==1&&browser.items().first().toMap()["id"]=="top2","top songs support inline filtering");
    browser.setArtistView("albums");check(wait([&]{return !browser.busy();})&&browser.collectionQuery().isEmpty()&&browser.items().first().toMap()["type"]=="albums","switching artist view clears the filter and restores albums");
    slowTop=true;browser.setArtistView("songs");QTest::qWait(40);browser.setArtistView("albums");wait([&]{return !browser.busy();});QTest::qWait(500);
    check(browser.artistView()=="albums"&&browser.items().first().toMap()["type"]=="albums","late top-song replies cannot overwrite artist albums");slowTop=false;
    rejectTop=true;browser.setArtistView("songs");check(wait([&]{return !browser.busy();})&&!browser.error().isEmpty()&&browser.collection()["type"]=="artists","unavailable top songs preserve the artist and Albums option");rejectTop=false;
    emptyTop=true;browser.reload();check(wait([&]{return !browser.busy();})&&browser.items().isEmpty()&&browser.error().isEmpty(),"artists with no top songs show a normal empty list");emptyTop=false;
    hostileTop=true;browser.reload();wait([&]{return !browser.busy();});const int beforeTopMore=reads;browser.more();
    check(reads==beforeTopMore&&!browser.error().isEmpty(),"top-song pagination never forwards credentials outside Cider");hostileTop=false;
    browser.setArtistView("albums");wait([&]{return !browser.busy();});
    browser.more();check(wait([&]{return !browser.busy();})&&browser.items().size()==2,"artist discography paginates");
    browser.setCollectionQuery("evening");check(browser.items().size()==1,"artist albums support inline filtering");
    browser.open(0);check(wait([&]{return !browser.busy();})&&browser.collection()["id"]=="456"&&browser.items().first().toMap()["type"]=="songs","artist album opens its catalog tracks");
    browser.back();check(browser.collection()["type"]=="artists"&&browser.collectionQuery()=="evening"&&browser.items().size()==1,"Back restores the artist and album filter");
    browser.back();check(browser.collection().isEmpty()&&browser.kind()=="artists"&&plays==artistPlays,"artist browsing never changes playback");
    browser.showArtist("Shared name");check(wait([&]{return !browser.busy();})&&browser.collection().isEmpty()&&browser.items().size()==2,"ambiguous artist names stay in the chooser");
    browser.showArtist("slow");QTest::qWait(50);browser.setQuery("fresh artist");
    check(wait([&]{return !browser.busy();})&&browser.collection().isEmpty()&&browser.items().first().toMap()["title"]=="fresh artist","editing an artist lookup cancels automatic navigation");
    browser.setKind("playlists");wait([&]{return !browser.busy();});
    browser.setQuery("empty");check(wait([&]{return !browser.busy();})&&browser.items().isEmpty()&&browser.error().isEmpty(),"empty search is a normal state");
    browser.setQuery("slow");QTest::qWait(400);browser.setActive(false);QTest::qWait(650);
    check(browser.items().isEmpty()&&!browser.busy(),"closing browser discards pending searches");
    browser.setSection("albums");hostilePage=true;browser.setActive(true);wait([&]{return !browser.busy();});int before=reads;browser.more();
    check(!browser.error().isEmpty()&&reads==before,"pagination cannot send the connection token to another host");hostilePage=false;
    rejectRead=true;browser.reload();check(wait([&]{return !browser.busy();})&&browser.needsConnection(),"expired Cider access offers connection settings");rejectRead=false;
    browser.reload();wait([&]{return !browser.busy();});

    browser.setSection("songs");
    check(wait([&]{return !browser.busy();})&&browser.items().value(0).toMap()["type"]=="library-songs"&&browser.hasMore(),"saved songs load with library identity");
    browser.more();check(wait([&]{return !browser.busy();})&&browser.items().size()==2,"saved songs paginate without a background full-library fetch");
    browser.play(1);check(wait([&]{return !browser.starting();})&&playedId=="i.saved2"&&playedType=="library-songs","saved song plays its library resource");
    browser.setQuery("saved & rare");check(wait([&]{return !browser.busy();})&&lastTerm=="saved & rare"&&browser.items().value(0).toMap()["type"]=="library-songs","saved song search covers the library through Cider");
    check(Library::linkPath("https://music.apple.com/ca/album/name/123?i=456&ls=1")=="/v1/catalog/ca/songs/456","album song query resolves the selected song");
    for(const auto &bad:QStringList{"https://music.apple.com.evil.test/ca/song/123","https://user@music.apple.com/ca/song/123","http://music.apple.com/ca/song/123","https://music.apple.com:10767/ca/song/123","https://music.apple.com/ca/song/../123","https://music.apple.com/ca/album/title/123?i=bad","https://example.com/ca/song/123"})
        check(Library::linkPath(bad).isEmpty(),"untrusted or malformed link is rejected");
    int linkPlays=plays;
    check(browser.openLink("https://music.apple.com/ca/album/title/123?i=456"),"song link accepted");
    check(wait([&]{return !browser.busy();})&&browser.items().value(0).toMap()["id"]=="456"&&plays==linkPlays,"link previews a song without changing playback");
    browser.play(0);check(wait([&]{return !browser.starting();})&&playedId=="456"&&playedType=="songs","explicit link playback sends the resolved song");
    browser.openLink("https://music.apple.com/us/album/title/123");wait([&]{return !browser.busy();});browser.open(0);
    check(wait([&]{return !browser.busy();})&&browser.collection()["path"]=="/v1/catalog/us/albums/123"&&browser.items().value(0).toMap()["path"].toString().startsWith("/v1/catalog/us/songs/"),"linked album and its tracks preserve their storefront");
    browser.back();browser.openLink("https://music.apple.com/ca/playlist/title/pl.abc-123");
    check(wait([&]{return !browser.busy();})&&browser.items().value(0).toMap()["type"]=="playlists","playlist link resolves its collection");
    browser.openLink("https://music.apple.com/ca/song/999");QTest::qWait(50);browser.setQuery("fresh");
    check(wait([&]{return !browser.busy();})&&browser.items().value(0).toMap()["title"]=="fresh","late link response cannot overwrite a newer search");
    cider.refreshCrossfade();check(wait([&]{return !cider.crossfadeBusy();})&&cider.crossfadeReady()&&!cider.crossfade()&&cider.crossfadeSeconds()==5,"crossfade loads confirmed Cider settings");
    cider.setCrossfade(true);cider.setCrossfade(true);
    check(wait([&]{return !cider.crossfadeBusy();})&&cider.crossfade()&&fadeWrites==1&&fadePatch.size()==1&&fadePatch.contains("enabled"),"crossfade toggle is serialized and preserves other fields");
    cider.setCrossfadeSeconds(8);
    check(wait([&]{return !cider.crossfadeBusy();})&&cider.crossfadeSeconds()==8&&fadePatch.size()==1&&fadePatch.contains("durationSec"),"duration changes only duration and reads back Cider");
    int writesBefore=fadeWrites;cider.setCrossfadeSeconds(0);cider.setCrossfadeSeconds(99);cider.setCrossfadeSeconds(8);
    check(fadeWrites==writesBefore,"invalid or unchanged crossfade durations do not write");
    rejectCrossfade=true;cider.setCrossfade(false);
    check(wait([&]{return !cider.crossfadeBusy();})&&!cider.crossfadeReady()&&!cider.crossfadeError().isEmpty()&&cider.crossfade(),"denied crossfade edit keeps last confirmed state and exposes retry");
    rejectCrossfade=false;malformedCrossfade=true;cider.refreshCrossfade();
    check(wait([&]{return !cider.crossfadeBusy();})&&!cider.crossfadeReady(),"malformed crossfade state never enables editing");
    malformedCrossfade=false;cider.refreshCrossfade();wait([&]{return !cider.crossfadeBusy();});
    int oldFadeReads=fadeReads;QTest::qWait(100);check(fadeReads==oldFadeReads,"crossfade does not poll in the background");
    browser.setSection("albums");wait([&]{return !browser.busy();});

    // Connection approval uses only the fixture, never the user's Cider session.
    {
        const QString authConfig=temp+"/auth/connection.json";
        Cider approval(false,authConfig,QUrl(QString("http://127.0.0.1:%1").arg(server.serverPort())));
        authCode=403; approval.authorize();
        check(wait([&]{return !approval.authorizing();})&&!QFile::exists(authConfig)&&approval.connectionMessage().contains("approved"),"denied authorization never replaces stored access");
        authCode=200; malformedAuth=true; approval.authorize();
        check(wait([&]{return !approval.authorizing();})&&!QFile::exists(authConfig),"malformed approval cannot save an empty token");
        malformedAuth=false; authDelay=500; approval.authorize(); approval.authorize(); QTest::qWait(50);
        const int requests=authRequests; approval.cancelAuthorization(); QTest::qWait(600);
        check(!approval.authorizing()&&!QFile::exists(authConfig)&&authRequests==requests,"cancel and duplicate clicks cannot accept a late approval");
        authDelay=0; approval.authorize();
        check(wait([&]{return !approval.authorizing()&&approval.queueReady();})&&QFile::exists(authConfig),"approved connection persists and loads the queue");
        check(authBody["app_name"]=="Spun"&&authBody["scopes"].toArray().size()==4,"approval identifies Spun and requests only its four feature scopes");
        check(!(QFile::permissions(authConfig)&(QFileDevice::ReadGroup|QFileDevice::ReadOther|QFileDevice::WriteGroup|QFileDevice::WriteOther)),"saved Cider token is owner-only");
        Cider restored(false,authConfig,QUrl(QString("http://127.0.0.1:%1").arg(server.serverPort()))); restored.reconnect();
        check(wait([&]{return restored.connectionState()=="connected";}),"saved approval reconnects on a new app instance");
    }
    {
        const auto row=browser.items().first().toMap(); const int beforePins=plays;
        browser.togglePin(row);
        check(browser.pins().size()==1&&browser.isPinned(row)&&plays==beforePins,"pinning a collection does not change playback");
        Library restored(&cider); check(restored.pins().size()==1,"pins survive a new browser instance");
        auto hostile=row; hostile["path"]="https://example.invalid/album"; browser.togglePin(hostile);
        check(browser.pins().size()==1,"pin validation rejects external collection paths");
        browser.playPin(0); check(wait([&]{return !browser.starting();})&&playedId==row["id"]&&playedType==row["type"],"pin playback preserves library ID and resource type");
        browser.openPin(0); check(wait([&]{return !browser.busy();})&&browser.collection()["path"]==row["path"],"pinned collection opens its own track list");browser.back();
        const auto rows=browser.items(); rejectRead=true;browser.reload();
        check(wait([&]{return !browser.busy();})&&browser.items()==rows&&browser.needsConnection(),"access failure preserves the visible browsing results");
        const int beforeRecovery=plays; rejectRead=false;cider.reconnect();
        check(wait([&]{return !browser.busy()&&browser.error().isEmpty();})&&plays==beforeRecovery,"reconnection reloads failed reads without replaying a music command");
        const auto port=server.serverPort(); server.close(); cider.reconnect();
        check(wait([&]{return cider.recovering();}),"lost local API is distinguished from expired permission");
        const int offlinePlays=plays;check(server.listen(QHostAddress::LocalHost,port),"fixture API returns on the same port");
        check(wait([&]{return cider.connectionState()=="connected";})&&plays==offlinePlays,"visible browser recovers automatically without resuming music");
        const int settledProbes=probes; QTest::qWait(150);
        check(probes==settledProbes,"successful recovery stops connection probing");
        browser.togglePin(row); Library unpinned(&cider);
        check(browser.pins().isEmpty()&&unpinned.pins().isEmpty(),"unpin persists without leaving a stale saved collection");
    }

    browser.setSection("recent");
    check(wait([&]{return !browser.busy();})&&browser.items().value(0).toMap()["id"]=="recent-new"&&browser.hasMore(),"recent history loads lazily in API order");
    browser.setQuery("older");
    check(wait([&]{return !browser.busy()&&!browser.hasMore();})&&browser.items().size()==1&&browser.items()[0].toMap()["id"]=="i.older","history search finds a song on an unloaded page");
    browser.play(0);check(wait([&]{return !browser.starting();})&&playedType=="library-songs"&&playedId=="i.older","history replay preserves the returned resource type");
    browser.setQuery("");check(browser.items().size()==2,"clearing history search restores the loaded history");
    browser.setActive(false);const int oldHistory=historyReads;QTest::qWait(100);check(historyReads==oldHistory,"history makes no background requests while closed");
    browser.setActive(true);check(wait([&]{return !browser.busy();})&&historyReads==oldHistory+1,"reopening history fetches fresh listening history");
    browser.setSection("playlists");wait([&]{return !browser.busy();});pagedTracks=true;browser.open(0);wait([&]{return !browser.busy();});
    check(browser.items().size()==3&&browser.hasMore(),"playlist detail starts with one page");
    browser.setCollectionQuery("cafe AFTERGLOW");
    check(wait([&]{return !browser.busy()&&!browser.hasMore();})&&browser.items().size()==1&&browser.items()[0].toMap()["id"]=="i.hidden","collection search finds unloaded tracks ignoring accents and case");
    browser.play(0);check(wait([&]{return !browser.starting();})&&playedId=="i.hidden","filtered track playback uses its real ID rather than the unfiltered row index");
    browser.setCollectionQuery("");check(browser.items().size()==4,"clearing collection search restores loaded tracks");
    browser.setCollectionQuery("missing");check(browser.items().isEmpty()&&browser.error().isEmpty(),"completed unmatched search is a normal empty state");
    renamedTracks=true;browser.reload();browser.setCollectionQuery("renamed CAFE");
    check(wait([&]{return !browser.busy()&&!browser.hasMore();})&&browser.items().size()==1&&browser.items()[0].toMap()["id"]=="i.first","refresh replaces cached search text when track metadata changes");
    renamedTracks=false;browser.reload();browser.setCollectionQuery("cafe");
    check(wait([&]{return !browser.busy()&&!browser.hasMore();})&&browser.items().size()==1&&browser.items()[0].toMap()["id"]=="i.hidden","repeated refresh does not retain matches from older metadata");
    browser.back();check(browser.collectionQuery().isEmpty()&&browser.items().size()==1,"Back clears the collection filter and restores playlists");
    failTracks=true;browser.open(0);wait([&]{return !browser.busy();});browser.setCollectionQuery("light");
    check(wait([&]{return !browser.error().isEmpty();})&&browser.items().size()==2&&browser.hasMore(),"failed search page keeps earlier matches and a retry target");
    failTracks=false;browser.more();check(wait([&]{return !browser.busy()&&!browser.hasMore();})&&browser.error().isEmpty()&&browser.items().size()==2,"retry completes the search without losing existing matches");
    browser.back();hostileTracks=true;browser.open(0);wait([&]{return !browser.busy();});browser.setCollectionQuery("light");
    check(wait([&]{return !browser.error().isEmpty();})&&browser.items().size()==2,"collection pagination rejects foreign URLs and retains matches");
    browser.back();hostileTracks=false;slowTracks=true;browser.open(0);wait([&]{return !browser.busy();});browser.setCollectionQuery("afterglow");QTest::qWait(50);browser.back();QTest::qWait(450);
    check(browser.collection().isEmpty()&&browser.items().size()==1,"leaving a collection discards a delayed search page");
    slowTracks=false;pagedTracks=false;browser.setSection("albums");wait([&]{return !browser.busy();});

    MusicActions actions(&cider);
    QSignalSpy notices(&actions,&MusicActions::feedback);
    actions.setObserving(true);
    check(wait([&]{return actions.ready();})&&!actions.favorite()&&!actions.saved(),"song actions load confirmed favorite and library states");
    editDelay=200;int editBefore=edits;
    actions.toggleFavorite();actions.toggleFavorite();
    check(wait([&]{return !actions.busy()&&actions.ready();})&&actions.favorite()&&edits==editBefore+1&&editPath.endsWith("/love")&&editMethod=="POST","Favorite prevents duplicate requests and reads back server state");
    editDelay=0;actions.toggleFavorite();
    check(wait([&]{return !actions.busy()&&actions.ready();})&&!actions.favorite()&&editMethod=="PUT"&&editBody["rating"].toInt(-1)==0,"unfavorite clears the rating without disliking the song");
    actions.save();check(wait([&]{return !actions.busy()&&actions.ready();})&&actions.saved(),"Save reads back the actual library state");
    editBefore=edits;actions.save();QTest::qWait(50);check(edits==editBefore,"already saved songs cannot be added twice");
    QVariantMap chosen{{"id","i.second"},{"type","library-songs"},{"playable",true}};
    const int playBefore=plays;
    actions.enqueue(chosen,true);check(wait([&]{return !actions.busy();})&&editPath.endsWith("/add-next")&&editBody["id"]=="i.second"&&editBody["type"]=="library-songs"&&plays==playBefore,"Play Next preserves identity and does not replace playback");
    chosen["id"]="p.first";chosen["type"]="library-playlists";
    actions.enqueue(chosen,false);check(wait([&]{return !actions.busy();})&&editPath.endsWith("/add-later")&&editBody["type"]=="library-playlists","Add to Queue supports complete library playlists");
    editBefore=edits;chosen["playable"]=false;actions.enqueue(chosen,true);QTest::qWait(50);check(edits==editBefore,"unavailable items cannot be queued");
    chosen["playable"]=true;rejectEdit=true;actions.enqueue(chosen,false);
    check(wait([&]{return !actions.busy();})&&!actions.error().isEmpty()&&notices.last().at(1).toBool(),"denied edits give visible feedback without reporting success");rejectEdit=false;
    const QVariantList batch{QVariantMap{{"id","one"},{"type","songs"},{"playable",true}},QVariantMap{{"id","i.two"},{"type","library-songs"},{"playable",true}},QVariantMap{{"id","three"},{"type","songs"},{"playable",true}}};
    queueIds.clear();actions.enqueueMany(batch,true);actions.enqueueMany(batch,true);
    check(wait([&]{return !actions.busy();})&&queueIds==QStringList{"three","i.two","one"},"batch Play Next reverses requests to preserve selected order and rejects duplicate submission");
    queueIds.clear();actions.enqueueMany(batch,false);
    check(wait([&]{return !actions.busy();})&&queueIds==QStringList{"one","i.two","three"},"batch Add to Queue preserves visual order and mixed resource identity");
    queueIds.clear();rejectQueueAt=2;actions.enqueueMany(batch,false);
    check(wait([&]{return !actions.busy();})&&queueIds.size()==2&&actions.error().contains("1 of 3"),"partial batch failure stops without replay and reports confirmed count");rejectQueueAt=-1;
    auto invalidBatch=batch;invalidBatch.append(QVariantMap{{"id","bad"},{"type","albums"},{"playable",true}});queueIds.clear();actions.enqueueMany(invalidBatch,false);QTest::qWait(50);
    check(queueIds.isEmpty()&&!actions.busy(),"batch validates every song before issuing any queue writes");
    actions.setObserving(false);const int readsBefore=statusReads;QTest::qWait(2100);check(statusReads==readsBefore,"closed song menu does not poll Cider");
    malformedStatus=true;actions.setObserving(true);QTest::qWait(100);
    check(!actions.ready()&&!actions.error().isEmpty(),"unreadable status never appears as an unfavorited song");
    malformedStatus=false;actions.setObserving(false);editDelay=300;actions.setObserving(true);QTest::qWait(50);
    actions.setObserving(false);QMetaObject::invokeMethod(&cider,"trackChanged");QTest::qWait(350);
    check(!actions.ready()&&!actions.saved(),"late status cannot restore the previous track state");editDelay=0;

    browser.setSection("songs");wait([&]{return !browser.busy();});browser.setNewestFirst(true);
    check(wait([&]{return !browser.busy();})&&lastBrowsePath.contains("sort=-dateAdded"),"recently added songs request server-side newest-first order");
    browser.more();check(wait([&]{return !browser.busy();})&&lastBrowsePath.contains("sort=-dateAdded"),"recently added pagination preserves the requested order");
    { Library restoredSort(&cider);check(restoredSort.newestFirst(),"library sort preference survives relaunch"); }
    browser.setSection("albums");check(wait([&]{return !browser.busy();})&&lastBrowsePath.contains("sort=-dateAdded"),"recently added albums use the same bounded server sort");browser.setNewestFirst(false);wait([&]{return !browser.busy();});
    check(Library::songLink("https://music.apple.com/ca/album/test/100?i=123&ls=1")=="https://music.apple.com/ca/song/123","share links keep the song identity and remove tracking parameters");
    check(Library::songLink("https://music.apple.com.evil.test/ca/song/123").isEmpty(),"share links reject lookalike domains");
    browser.copyLink({{"id","123"},{"type","songs"},{"path","/v1/catalog/ca/songs/123"}});
    check(QGuiApplication::clipboard()->text()=="https://music.apple.com/ca/song/123","catalog song link is copied without a network request");
    browser.copyLink({{"id","i.example"},{"type","library-songs"},{"catalogId","456"}});
    check(wait([&]{return !browser.busy();})&&QGuiApplication::clipboard()->text()=="https://music.apple.com/ca/song/456","library song sharing uses the catalog ID instead of the private library ID");
    cider.copySongLink();check(wait([&]{return QGuiApplication::clipboard()->text()=="https://music.apple.com/ca/song/123";}),"current song sharing uses Cider's song URL");
    shareUrl="https://example.invalid/private";cider.copySongLink();QTest::qWait(100);
    check(QGuiApplication::clipboard()->text()=="https://music.apple.com/ca/song/123","unshareable current tracks preserve the clipboard");shareUrl="https://music.apple.com/ca/album/example/100?i=123";
    seekFixture=true;
    auto desktopState=[&](QVariantMap values) {QMetaObject::invokeMethod(&cider,"propertiesChanged",Q_ARG(QString,QString("org.mpris.MediaPlayer2.Player")),Q_ARG(QVariantMap,values),Q_ARG(QStringList,QStringList{}));};
    desktopState({{"Metadata",QVariantMap{{"mpris:trackid","/spun/seek"},{"mpris:length",32000000LL}}},{"CanSeek",true},{"PlaybackStatus","Paused"},{"Position",5000000LL}});
    cider.seek(12000);check(wait([&]{return cider.position()==12000;})&&snapshotPosition==12,"seek confirms Cider's actual playback time through the API");
    desktopState({{"Position",5000000LL}});check(cider.position()==12000,"stale desktop position cannot undo a confirmed seek");
    snapshotPosition=18;desktopState({{"Position",18000000LL}});check(wait([&]{return cider.position()==18000;}),"a later external seek updates the progress ring normally");
    desktopState({{"Metadata",QVariantMap{}},{"CanSeek",false},{"Position",0LL}});
    seekFixture=false;seekWrites=0;snapshotPosition=12.345;
    cider.setVolume(.55);check(wait([&]{return cider.volume()==.55;})&&normalizedVolume==.55,"paired volume uses the normalized API scale");
    QMetaObject::invokeMethod(&cider,"propertiesChanged",Q_ARG(QString,QString("org.mpris.MediaPlayer2.Player")),Q_ARG(QVariantMap,(QVariantMap{{"Volume",.37046}})),Q_ARG(QStringList,QStringList{}));
    check(wait([&]{return volumeReads>=2;})&&cider.volume()==.55,"scaled desktop volume updates never overwrite the normalized slider value");
    cider.setVolume(.3);cider.setVolume(.6);cider.setVolume(.55);QTest::qWait(1000);
    check(normalizedVolume==.55&&volumeWrites==2,"rapid volume changes coalesce to the latest slider value");
    rejectVolume=true;cider.setVolume(.9);QTest::qWait(900);
    check(normalizedVolume==.55&&cider.volume()==.55,"denied volume writes retain the confirmed value");rejectVolume=false;
    cider.refreshAudioOptions();check(wait([&]{return !cider.audioBusy();})&&cider.audioOptions().contains("automix")&&cider.audioOptions()["listeningMode"]=="off","extra audio settings discover supported controls");
    cider.setAudioOption("automix",true);cider.setAudioOption("automix",true);
    check(wait([&]{return !cider.audioBusy();})&&automix&&audioWrites==1&&audioPatch.size()==1&&audioPatch.contains("enabled"),"Automix writes only its toggle and confirms state without duplicate writes");
    cider.setAudioOption("listeningMode","unwind");check(wait([&]{return !cider.audioBusy();})&&listeningMode=="antifatigue"&&cider.audioOptions()["listeningMode"]=="unwind","Unwind sends Cider's antifatigue value and reads back its confirmed state");
    cider.refreshAudioOptions();check(wait([&]{return !cider.audioBusy();})&&cider.audioOptions()["listeningMode"]=="unwind","Cider's stored antifatigue mode maps back to Unwind");
    const int writtenAudio=audioWrites;cider.setAudioOption("listeningMode","unknown");check(audioWrites==writtenAudio,"unknown listening modes cannot be written");
    rejectAudio=true;cider.setAudioOption("automix",false);check(wait([&]{return !cider.audioBusy();})&&!cider.audioOptions().contains("automix")&&automix&&!cider.audioError().isEmpty(),"denied audio writes never show an unconfirmed toggle");rejectAudio=false;
    audioUnavailable=true;cider.refreshAudioOptions();check(wait([&]{return !cider.audioBusy();})&&cider.audioOptions().isEmpty(),"unsupported audio endpoints hide their controls");audioUnavailable=false;
    cider.refreshAudioOptions();wait([&]{return !cider.audioBusy();});const int quietAudio=audioReads;QTest::qWait(150);check(audioReads==quietAudio,"extra audio settings never poll in the background");
    fixtureQueue={QJsonObject{{"track",resource("history","songs","Already heard")}},QJsonObject{{"track",resource("123","songs","First Light")}},QJsonObject{{"track",resource("i.two","library-songs","Second Light")}}};fixturePosition=1;
    cider.refreshQueue();check(wait([&]{return !cider.queueBusy();})&&cider.queueReady(),"saved queue fixture loads with current position");
    const int savePlays=plays;check(browser.saveQueue("Evening drive")&&plays==savePlays,"saving a queue never changes playback");
    browser.setSection("sessions");wait([&]{return !browser.busy();});check(browser.items().size()==1&&browser.items().first().toMap()["trackCount"].toInt()==2,"saved queues exclude already played history");
    const auto session=browser.items().first().toMap();const auto snapshot=browser.savedTracks(session["id"].toString());
    check(snapshot.size()==2&&snapshot[0].toMap()["id"]=="123"&&snapshot[1].toMap()["type"]=="library-songs","saved queues preserve order and library identities");
    const QString snapshotPath=temp+"/saved-queues/"+session["id"].toString()+".json";
    check(!(QFile::permissions(snapshotPath)&(QFileDevice::ReadGroup|QFileDevice::ReadOther)),"saved queue references are owner-only");
    QFile snapshotFile(snapshotPath);check(snapshotFile.open(QIODevice::ReadOnly),"saved queue file opens for inspection");const auto snapshotBytes=snapshotFile.readAll();snapshotFile.close();
    check(!snapshotBytes.contains("test-library-token"),"saved queues never include connection credentials");
    check(snapshotFile.open(QIODevice::WriteOnly),"saved queue fixture opens for corruption test");snapshotFile.write("broken");snapshotFile.close();check(browser.savedTracks(session["id"].toString()).isEmpty(),"corrupt saved queues never produce partial playback lists");
    check(snapshotFile.open(QIODevice::WriteOnly),"saved queue fixture opens for corruption test");snapshotFile.write(snapshotBytes);snapshotFile.close();
    QFile manifest(temp+"/saved-queues/index.json");check(manifest.open(QIODevice::ReadOnly),"saved queue index opens");const auto manifestBytes=manifest.readAll();manifest.close();
    check(manifest.open(QIODevice::WriteOnly),"saved queue index fixture opens");manifest.write("broken");manifest.close();
    check(!browser.saveQueue("Do not overwrite"),"a corrupt index cannot be overwritten by saving another queue");
    check(manifest.open(QIODevice::ReadOnly),"saved queue index can be checked after failed save");check(manifest.readAll()=="broken","failed save preserves the existing index");manifest.close();
    check(manifest.open(QIODevice::WriteOnly),"saved queue index fixture restores");manifest.write(manifestBytes);manifest.close();
    check(browser.savedTracks("../cider-connection").isEmpty(),"saved queue IDs cannot escape the storage directory");
    Library restoredSessions(&cider);restoredSessions.setSection("sessions");restoredSessions.setActive(true);
    check(restoredSessions.items().size()==1,"saved queue metadata survives a new browser instance");restoredSessions.setActive(false);
    browser.open(0);check(browser.items().size()==2&&browser.collection()["type"]=="saved-queues","saved queue opens its tracks without autoplay or API reads");
    const auto renameBefore=browser.collection();
    const int editsBeforeSaved=edits,playsBeforeSaved=plays;
    check(browser.renameSavedQueue(renameBefore,"  Night drive  ")&&browser.collection()["title"]=="Night drive","saved queue rename trims and persists the new title");
    check(!browser.renameSavedQueue(renameBefore,"Stale")&&!browser.renameSavedQueue(browser.collection(),"  "),"stale and empty saved-queue names cannot overwrite the current title");
    check(browser.editSavedTrack(1,0,false)&&browser.items().first().toMap()["id"]=="i.two","saved track move preserves library identity and duplicate-safe order");
    const auto versioned=browser.collection();const QString versionPath=temp+"/saved-queues/"+versioned["dataId"].toString()+".json";
    check(versioned["dataId"]!=session["dataId"]&&QFile::exists(versionPath)&&QFile::exists(snapshotPath)&&browser.canUndoSavedQueue(),"saved track edit publishes a new version and briefly retains its predecessor for Undo");
    check(!(QFile::permissions(versionPath)&(QFileDevice::ReadGroup|QFileDevice::ReadOther)),"edited saved queues remain owner-only");
    const auto beforeFailedEdit=browser.savedTracks(session["id"].toString());
    const auto filesBeforeFailed=QDir(temp+"/saved-queues").entryList(QDir::Files);
    QFile::setPermissions(temp+"/saved-queues/index.json",QFileDevice::ReadOwner);
    check(!browser.editSavedTrack(0,1,false)&&browser.savedTracks(session["id"].toString())==beforeFailedEdit&&QDir(temp+"/saved-queues").entryList(QDir::Files)==filesBeforeFailed,"failed index commit preserves the old snapshot and leaves no orphan edit");
    QFile::setPermissions(temp+"/saved-queues/index.json",QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    browser.setCollectionQuery("First Light");
    check(browser.items().size()==1&&browser.items().first().toMap()["savedIndex"]==1&&!browser.editSavedTrack(1,0,false),"filtered saved rows retain original positions and disable ambiguous reordering");
    check(browser.editSavedTrack(browser.items().first().toMap()["savedIndex"].toInt(),0,true)&&browser.collection()["trackCount"]==1&&browser.items().isEmpty(),"filtered removal edits the matching source track only");
    browser.setCollectionQuery("");
    check(browser.items().first().toMap()["id"]=="i.two"&&browser.editSavedTrack(0,0,true)&&browser.collection()["trackCount"]==0&&!browser.collection()["playable"].toBool(),"removing the last saved song leaves an editable empty queue");
    check(browser.renameSavedQueue(browser.collection(),"Empty queue"),"empty saved queues can still be renamed");
    browser.back();check(browser.items().first().toMap()["title"]=="Empty queue"&&browser.items().first().toMap()["trackCount"]==0,"Back refreshes saved queue names and counts");
    Library editedReload(&cider);editedReload.setSection("sessions");editedReload.setActive(true);
    check(editedReload.items().first().toMap()["trackCount"]==0&&editedReload.savedTracks(session["id"].toString()).isEmpty(),"edited empty queue survives a new browser instance");editedReload.setActive(false);
    check(edits==editsBeforeSaved&&plays==playsBeforeSaved,"saved queue edits never issue Cider playback or queue writes");
    // Append to local snapshots without touching the live Cider queue.
    auto savedChoice=[&]{return browser.savedQueueChoices()["rows"].toList().first().toMap();};
    auto appendSong=Library::item(resource("fresh","songs","Fresh song"));
    const auto emptyChoice=savedChoice();const int appendWrites=queueWrites+edits+plays;
    auto appended=browser.appendSavedQueue(emptyChoice,{appendSong,appendSong},true);
    check(appended["ok"].toBool()&&appended["added"]==1&&appended["skipped"]==1&&browser.savedTracks(session["id"].toString()).size()==1,"append fills an empty saved queue and skips repeated incoming identities");
    check(!browser.appendSavedQueue(emptyChoice,{appendSong},false)["ok"].toBool(),"a stale saved-queue picker cannot overwrite a newer version");
    const auto duplicateChoice=savedChoice();const auto duplicateFiles=QDir(temp+"/saved-queues").entryList(QDir::Files);
    appended=browser.appendSavedQueue(duplicateChoice,{appendSong},true);
    check(appended["ok"].toBool()&&appended["added"]==0&&appended["skipped"]==1&&savedChoice()==duplicateChoice&&duplicateFiles==QDir(temp+"/saved-queues").entryList(QDir::Files),"an all-duplicate append succeeds without rewriting files");
    auto alias=appendSong;alias["id"]="i.alias";alias["type"]="library-songs";alias["catalogId"]="fresh";
    check(browser.appendSavedQueue(savedChoice(),{alias},true)["skipped"]==1,"saved appends match catalog and library identities");
    appended=browser.appendSavedQueue(savedChoice(),{alias,appendSong},false);
    const auto repeatedSaved=browser.savedTracks(session["id"].toString());
    check(appended["added"]==2&&repeatedSaved.size()==3&&repeatedSaved[1].toMap()["id"]=="i.alias"&&repeatedSaved[2].toMap()["id"]=="fresh","turning off duplicate skipping preserves deliberate repeats in order");
    const auto beforeInvalidAppend=savedChoice();auto invalidAppend=appendSong;invalidAppend["id"]="../unsafe";
    check(!browser.appendSavedQueue(beforeInvalidAppend,{appendSong,invalidAppend},false)["ok"].toBool()&&savedChoice()==beforeInvalidAppend,"one invalid song rejects the entire saved append without partial changes");
    QFile::setPermissions(temp+"/saved-queues/index.json",QFileDevice::ReadOwner);
    check(!browser.appendSavedQueue(beforeInvalidAppend,{appendSong},false)["ok"].toBool()&&browser.savedTracks(session["id"].toString())==repeatedSaved,"a failed append commit preserves the previous saved tracks");
    QFile::setPermissions(temp+"/saved-queues/index.json",QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    check(queueWrites+edits+plays==appendWrites,"saved appends never issue Cider queue or playback writes");
    const auto undoName=savedChoice()["title"].toString();
    check(browser.renameSavedQueue(savedChoice(),"Temporary name")&&browser.canUndoSavedQueue()&&browser.undoSavedQueue()&&savedChoice()["title"]==undoName&&!browser.canUndoSavedQueue(),"Undo restores a saved queue name once");
    check(browser.openQuickTarget("saved",session["id"].toString())&&browser.collection()["id"]==session["id"],"Quick jump opens a saved queue by its stable ID");
    const auto undoTracks=browser.savedTracks(session["id"].toString());
    check(browser.editSavedTrack(0,2,false)&&browser.undoSavedQueue()&&browser.savedTracks(session["id"].toString())==undoTracks,"Undo restores the exact saved order including duplicates");
    const auto undoFile=browser.collection()["dataId"].toString();
    check(browser.editSavedTrack(1,0,true)&&browser.undoSavedQueue()&&browser.savedTracks(session["id"].toString())==undoTracks&&browser.collection()["dataId"]==undoFile,"Undo restores a removed occurrence by switching back to its previous file");
    check(browser.renameSavedQueue(browser.collection(),"Retry undo"),"Undo failure fixture renames locally");
    QFile::setPermissions(temp+"/saved-queues/index.json",QFileDevice::ReadOwner);
    check(!browser.undoSavedQueue()&&browser.canUndoSavedQueue()&&savedChoice()["title"]=="Retry undo","failed Undo keeps the current queue and allows a retry");
    QFile::setPermissions(temp+"/saved-queues/index.json",QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    check(browser.undoSavedQueue()&&savedChoice()["title"]==undoName,"Undo can be retried after a failed index commit");
    browser.renameSavedQueue(browser.collection(),"First editor");
    { Library otherEditor(&cider);otherEditor.renameSavedQueue(savedChoice(),"Second editor");
      check(!browser.undoSavedQueue()&&savedChoice()["title"]=="Second editor","stale Undo never overwrites another editor's changes"); }
    browser.reload();const auto expiresFile=browser.collection()["dataId"].toString();
    browser.editSavedTrack(0,1,false);wait([&]{return !browser.canUndoSavedQueue()&&!QFile::exists(temp+"/saved-queues/"+expiresFile+".json");});
    check(!browser.canUndoSavedQueue()&&!QFile::exists(temp+"/saved-queues/"+expiresFile+".json")&&!browser.undoSavedQueue(),"Undo expires and retires the old track file without retaining a track list in RAM");
    { Library inactiveJump(&cider);check(inactiveJump.openQuickTarget("saved",session["id"].toString()),"Quick jump can prepare a saved target while its browser is inactive");inactiveJump.setActive(true);
      check(inactiveJump.items().size()==browser.savedTracks(session["id"].toString()).size(),"activating a prepared Quick jump target loads its tracks");inactiveJump.setActive(false); }
    auto jumpArtist=Library::item(resource("123","artists","Quick artist"));jumpArtist["path"]="/v1/catalog/ca/artists/123";const bool jumpPinned=browser.isPinned(jumpArtist);if(!jumpPinned)browser.togglePin(jumpArtist);
    check(browser.openQuickTarget("pin",jumpArtist["path"].toString())&&wait([&]{return !browser.busy();})&&browser.collection()["id"]=="123"&&browser.artistView()=="songs","Quick jump opens a pinned artist's Top songs");
    browser.back();check(browser.collection().isEmpty(),"Back from a Quick jump artist returns to browsing");if(!jumpPinned)browser.togglePin(jumpArtist);
    browser.openQuickTarget("saved",session["id"].toString());
    const auto quickBefore=browser.collection();check(!browser.openQuickTarget("saved","missing")&&browser.collection()==quickBefore,"missing Quick jump targets leave browsing unchanged");
    browser.back();check(browser.collection().isEmpty()&&browser.section()=="sessions","Back from a jumped saved queue returns to its saved-queue list");
    check(queueWrites+edits+plays==appendWrites,"Undo and saved-queue navigation never write to Cider or start playback");
    // Restore the fixture expected by the subsequent UI tests through normal Save.
    browser.deleteSavedQueue(session["id"].toString());wait([&]{return !cider.queueBusy();});
    check(!QFile::exists(versionPath)&&browser.saveQueue("Evening drive"),"deleting an edited queue cleans its current data and a new queue can be saved");
    browser.open(0);
    queueIds.clear();actions.enqueueMany(snapshot,false);check(wait([&]{return !actions.busy();})&&queueIds==QStringList{"123","i.two"},"saved queue appends its original track sequence");browser.back();
    fixtureQueue={};fixturePosition=-1;cider.refreshQueue();wait([&]{return !cider.queueBusy();});
    browser.setSection("albums");wait([&]{return !browser.busy();});
    const int discoveryWrites=plays+edits;
    browser.setSection("search");browser.setKind("artists");browser.setQuery("Seed artist");wait([&]{return !browser.busy();});browser.open(0);wait([&]{return !browser.busy();});
    const auto seedArtist=browser.collection();browser.setArtistView("similar");
    check(wait([&]{return !browser.busy();})&&browser.items().first().toMap()["type"]=="artists","similar artists use Cider catalog views");
    browser.open(0);wait([&]{return !browser.busy();});check(browser.collection()["id"]=="456","similar artist opens a real artist page");
    browser.back();check(browser.artistView()=="similar"&&browser.collection()["id"]=="123","Back restores similar artist results and view");
    browser.togglePin(seedArtist);auto secondArtist=seedArtist;secondArtist["id"]="456";secondArtist["path"]="/v1/catalog/ca/artists/456";browser.togglePin(secondArtist);
    browser.setSection("releases");check(wait([&]{return !browser.busy();})&&browser.items().size()==2&&releaseReads==1,"latest releases batch artist pins into one request");
    check(browser.items().first().toMap()["id"]=="album456","latest releases sort newest first");
    browser.open(0);wait([&]{return !browser.busy();});check(browser.collection()["type"]=="albums","release opens its album track list");browser.back();
    check(browser.items().size()==2&&browser.collection().isEmpty(),"Back restores releases without refetching");
    browser.setQuery("123");check(browser.items().size()==1,"release search filters the loaded list");browser.setQuery("");
    browser.setSection("search");browser.setSection("releases");check(releaseReads==1&&browser.items().size()==2,"reopening releases uses bounded session cache");
    failReleases=true;browser.reload();check(wait([&]{return !browser.busy();})&&browser.items().size()==2&&!browser.releaseNotice().isEmpty()&&!browser.error().isEmpty(),"release outage preserves previous results with explicit retry state");
    failReleases=false;browser.reload();check(wait([&]{return !browser.busy();})&&browser.error().isEmpty()&&browser.releaseNotice().isEmpty(),"release retry clears stale status");
    slowReleases=true;browser.reload();QTest::qWait(50);browser.setSection("songs");wait([&]{return !browser.busy();});QTest::qWait(550);
    check(browser.section()=="songs"&&browser.items().first().toMap()["type"]=="library-songs","late releases cannot replace a different page");slowReleases=false;
    browser.togglePin(seedArtist);browser.togglePin(secondArtist);browser.setSection("releases");check(browser.items().isEmpty()&&!browser.busy(),"unpinning all artists invalidates the release cache");
    check(plays+edits==discoveryWrites,"artist discovery never changes playback or library");
    const QVariantMap radioSong{{"id","123"},{"type","songs"},{"path","/v1/catalog/ca/songs/123"}};
    browser.prepareRadio(radioSong);check(wait([&]{return !browser.radioBusy();})&&browser.radioAvailable(),"song radio resolves through existing Cider access");
    const int beforeCachedRadio=radioReads;browser.prepareRadio(radioSong);check(!browser.radioBusy()&&radioReads==beforeCachedRadio,"radio lookup reuses cached station metadata");
    browser.playRadio();check(wait([&]{return !browser.radioBusy();})&&browser.radioError().isEmpty()&&playedType=="stations"&&playedId=="ra.seed"&&playedPath.endsWith("play-href")&&stationChecks>0,"radio uses the regular station route and verifies actual playback");
    stationDelayChecks=2;const int beforeStation=plays;browser.playRadio();browser.playRadio();
    check(wait([&]{return !browser.radioBusy();})&&browser.radioError().isEmpty()&&stationChecks>2&&plays==beforeStation+1,"radio waits for delayed station state without submitting playback twice");stationDelayChecks=0;
    rejectStation=true;browser.playRadio();check(wait([&]{return !browser.radioBusy();})&&!browser.radioError().isEmpty(),"denied radio writes never claim playback success");rejectStation=false;
    unconfirmedStation=true;browser.playRadio();QTest::qWait(10500);
    check(wait([&]{return !browser.radioBusy();})&&!browser.radioError().isEmpty(),"an accepted station command with unchanged playback reports failure");unconfirmedStation=false;
    auto noRadio=radioSong;noRadio["id"]="999";browser.prepareRadio(noRadio);check(wait([&]{return !browser.radioBusy();})&&!browser.radioAvailable()&&browser.radioError().isEmpty(),"song without radio has a normal unavailable state");
    auto slowRadio=radioSong;slowRadio["id"]="777";browser.prepareRadio(slowRadio);QTest::qWait(50);browser.prepareRadio(noRadio);QTest::qWait(600);
    check(!browser.radioAvailable()&&!browser.radioBusy(),"late radio lookup cannot overwrite another song menu");
    browser.prepareRadio({{"id","i.upload"},{"type","library-songs"}});check(!browser.radioBusy()&&!browser.radioAvailable(),"uploaded songs without catalog identity do not guess a station");
    browser.setSection("albums");wait([&]{return !browser.busy();});
    std::function<QQuickItem*(QQuickItem*,QString)> find=[&](QQuickItem *parent,QString name)->QQuickItem* {if(parent->objectName()==name)return parent;for(auto *child:parent->childItems())if(auto *hit=find(child,name))return hit;return nullptr;};
    auto click=[&](QString name) {focusTestWindow(window);auto *hit=find(window->contentItem(),name);check(hit!=nullptr,"browser control exists");if(hit)QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,hit->mapToScene(QPointF(hit->width()/2,hit->height()/2)).toPoint());QTest::qWait(180);};
    manyRows=true;browser.reload();wait([&]{return !browser.busy();});
    check(!find(window->contentItem(),"libraryPanel"),"unopened browser does not allocate its controls at startup");
    auto *openingPlayer=qobject_cast<Player *>(QQmlEngine::contextForObject(window)->contextProperty("player").value<QObject *>());
    const bool openingMotion=openingPlayer->motion();openingPlayer->setMotion(true);
    window->setProperty("useCider",true);window->setProperty("libraryOpen",true);
    auto *panel=find(window->contentItem(),"libraryPanel");
    check(panel!=nullptr,"first browser opening creates its controls synchronously");
    if (!panel) return failures + 1;
    QTest::qWait(30);
    check(panel->opacity()<1,"first browser opening preserves the entrance animation");
    const auto original=panel->property("browser");
    if(!qEnvironmentVariableIsSet("SPUN_TEST_NO_COMPOSITOR_FOCUS"))window->requestActivate();
    QTest::qWait(100);
    const auto originalSavedService=window->property("savedService");window->setProperty("savedService",QVariant::fromValue(&browser));
    const auto originalActions=window->property("actionService");window->setProperty("actionService",QVariant::fromValue(&actions));
    panel->setProperty("browser",QVariant::fromValue(&browser));window->setProperty("useCider",true);window->setProperty("libraryOpen",true);QTest::qWait(250);
    openingPlayer->setMotion(openingMotion);
    check(window->width()==860&&!window->property("queueOpen").toBool(),"music browser reuses the queue panel footprint");
    auto *bar=find(window->contentItem(),"sourceBar");
    auto *deck=find(window->contentItem(),"playerDeck");
    auto *footer=find(window->contentItem(),"libraryFooter");
    auto *play=find(window->contentItem(),"playButton");
    check(bar && deck && qAbs(panel->mapToScene(QPointF()).y()-bar->mapToScene(QPointF()).y())<.1
          && qAbs(panel->mapToScene(QPointF(0,panel->height())).y()-deck->mapToScene(QPointF(0,deck->height())).y())<.1,
          "created library aligns with the source bar top and player card bottom");
    check(footer && play && qAbs(footer->mapToScene(QPointF(0,footer->height()/2)).y()-play->mapToScene(QPointF(0,play->height()/2)).y())<.1,
          "created library footer actions align with playback controls");
    auto *songMenu=window->findChild<QObject *>("currentSongMenu");
    check(songMenu!=nullptr,"current-song menu exists");
    if(songMenu) {
        songMenu->setProperty("x",230);songMenu->setProperty("y",420);
        QMetaObject::invokeMethod(songMenu,"open");
        check(wait([&]{return actions.ready();}),"current-song menu requests fresh status on opening");QTest::qWait(150);
        auto *saveItem=find(window->contentItem(),"saveLibraryAction");
        check(saveItem&&saveItem->property("text").toString()=="Saved to library"&&!saveItem->isEnabled(),"current-song menu shows confirmed saved state");
        if(!captures.isEmpty())window->grabWindow().save(captures+"/07-song-actions.png");
        click("favoriteAction");check(wait([&]{return !actions.busy();})&&rating==1,"Favorite menu invokes only the fixture library action");
        QMetaObject::invokeMethod(songMenu,"open");wait([&]{return actions.ready();});
        QMetaObject::invokeMethod(&cider,"trackChanged");QTest::qWait(150);
        check(!songMenu->property("visible").toBool(),"song changes close the menu before another action can target stale metadata");
    }
    auto *list=find(window->contentItem(),"libraryList");list->setProperty("contentY",120.0);QTest::qWait(100);
    browser.more();wait([&]{return !browser.busy();});QTest::qWait(100);
    check(qAbs(list->property("contentY").toDouble()-120)<1,"loading another page preserves the list reading position");
    int beforeUi=plays;
    click("libraryActions3");
    check(panel->property("actionsOpen").toBool()&&plays==beforeUi,"row overflow opens queue actions without starting playback");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/07-queue-actions.png");
    click("playNextAction");check(wait([&]{return !actions.busy();})&&editPath.endsWith("/add-next")&&plays==beforeUi,"Play Next menu dispatches a queue addition only");
    click("libraryActions3");testKeyClick(window,Qt::Key_Escape);QTest::qWait(100);
    check(wait([&]{return !panel->property("actionsOpen").toBool();})&&window->property("libraryOpen").toBool(),"Escape dismisses the track menu before closing the browser");
    openingPlayer->setMotion(true);
    click("libraryRow3");wait([&]{return !browser.busy();});
    check(!browser.collection().isEmpty()&&plays==beforeUi,"album row opens its track list without autoplay");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/06-library-album.png");
    click("libraryPlayCollection");check(wait([&]{return !browser.starting();})&&plays==beforeUi+1,"collection Play button dispatches playback");
    testKeyClick(window,Qt::Key_Escape);check(browser.collection().isEmpty(),"Escape returns from collection detail");QTest::qWait(100);
    check(qAbs(list->property("contentY").toDouble()-120)<1,"Back restores the library scroll position");
    check(list->property("currentIndex").toInt()==3&&list->hasActiveFocus(),"Back restores keyboard focus to the album that opened the collection");
    auto *listMotion=window->findChild<QObject *>("libraryListEntrance");
    check(listMotion&&listMotion->property("startOffset").toReal()==-12,"Back uses the return direction for the library transition");
    QTest::qWait(200);
    if(!captures.isEmpty())window->grabWindow().save(captures+"/24-library-return-focus.png");
    openingPlayer->setMotion(openingMotion);
    click("libraryTab_search");click("librarySearchInput");
    auto *search=find(window->contentItem(),"librarySearchInput");
    check(search && search->hasActiveFocus(),"browser search receives keyboard focus");
    auto *searchTab=find(window->contentItem(),"libraryTab_search");
    auto *songsTab=find(window->contentItem(),"libraryTab_songs");
    searchTab->forceActiveFocus(Qt::TabFocusReason);testKeyClick(window,Qt::Key_Right);
    check(songsTab->hasActiveFocus()&&browser.section()=="search","library arrows move focus without fetching a different section");
    testKeyClick(window,Qt::Key_Return);wait([&]{return !browser.busy();});
    check(browser.section()=="songs"&&search->hasActiveFocus(),"Enter activates the focused library destination and focuses its search");
    songsTab->forceActiveFocus(Qt::TabFocusReason);testKeyClick(window,Qt::Key_End);
    check(find(window->contentItem(),"libraryTab_playlists")->hasActiveFocus()&&browser.section()=="songs","End focuses the last library tab without activating it");
    testKeyClick(window,Qt::Key_Tab);
    check(search->hasActiveFocus(),"Tab exits the library tab group into search");
    songsTab->forceActiveFocus(Qt::TabFocusReason);testKeyClick(window,Qt::Key_Home);
    check(searchTab->hasActiveFocus(),"Home focuses the first library destination");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/20-library-keyboard-focus.png");
    testKeyClick(window,Qt::Key_Space);wait([&]{return !browser.busy();});
    check(browser.section()=="search"&&search->hasActiveFocus(),"Space activates the focused library destination");
    for(char c:QByteArray("music"))testKeyClick(window,c);
    check(wait([&]{return !browser.busy();})&&browser.query()=="music","search field updates the debounced catalog request");

    if(!captures.isEmpty())window->grabWindow().save(captures+"/06-library-search.png");
    click("libraryTab_songs");wait([&]{return !browser.busy();});
    check(browser.section()=="songs"&&list->property("count").toInt()==1,"Songs tab displays saved songs in the existing panel");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/08-library-songs.png");
    browser.openLink("https://music.apple.com/ca/song/title/456");wait([&]{return !browser.busy();});QTest::qWait(150);
    if(QGuiApplication::platformName()=="offscreen") {
        QGuiApplication::clipboard()->setText("https://music.apple.com/ca/song/title/457");
        list->forceActiveFocus();testKeyClick(window,Qt::Key_V,Qt::ControlModifier);
        check(wait([&]{return !browser.busy();})&&browser.items().value(0).toMap()["id"]=="457","Ctrl+V opens a copied link outside text input");
        QGuiApplication::clipboard()->clear();
        QMimeData mime;mime.setUrls({QUrl("https://music.apple.com/ca/song/title/456")});
        QDragEnterEvent enter(QPoint(260,280),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
        QGuiApplication::sendEvent(window,&enter);
        QDropEvent drop(QPointF(260,280),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
        QGuiApplication::sendEvent(window,&drop);
        check(wait([&]{return !browser.busy();})&&browser.items().value(0).toMap()["id"]=="456","dropping a link onto the CD resolves a preview");
    }
    const int beforeLinkQueue=plays;click("libraryActions0");click("addQueueAction");
    check(wait([&]{return !actions.busy();})&&editBody["id"]=="456"&&editBody["type"]=="songs"&&plays==beforeLinkQueue,"link result exposes Add to Queue without starting playback");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/08-link-preview.png");
    auto *fadeMenu=window->findChild<QObject *>("crossfadeMenu");
    check(fadeMenu!=nullptr,"crossfade popup exists");
    if(fadeMenu) {
        const auto originalFade=fadeMenu->property("service");fadeMenu->setProperty("service",QVariant::fromValue(&cider));
        QMetaObject::invokeMethod(fadeMenu,"open");
        check(wait([&]{return fadeMenu->property("opened").toBool() && cider.crossfadeReady() && !cider.crossfadeBusy();}),
              "crossfade popup finishes entering and loads its state before interaction");
        if(!captures.isEmpty())window->grabWindow().save(captures+"/08-crossfade.png");
        check(wait([&]{return !cider.audioBusy();}),"audio popup finishes discovering extras");
        if(!captures.isEmpty())window->grabWindow().save(captures+"/12-audio-settings.png");
        click("automixToggle");check(wait([&]{return !cider.audioBusy();})&&!automix,"Automix toggles from the themed audio popup");
        click("listeningMode_gaming");check(wait([&]{return !cider.audioBusy();})&&listeningMode=="game","Gaming sends Cider's game value from its segmented control");
        click("crossfadeToggle");check(wait([&]{return !cider.crossfadeBusy();})&&!crossfade,"crossfade toggle operates from the themed popup");
        auto *duration=find(window->contentItem(),"crossfadeDuration");check(duration&&!duration->isEnabled(),"duration is disabled when crossfade is off");
        click("crossfadeToggle");wait([&]{return !cider.crossfadeBusy();});
        if(duration) { duration->forceActiveFocus();testKeyClick(window,Qt::Key_Right); }
        check(wait([&]{return !cider.crossfadeBusy();})&&fadeSeconds==9,"keyboard adjusts crossfade duration and confirms it");
        testKeyClick(window,Qt::Key_Escape);check(wait([&]{return !fadeMenu->property("visible").toBool();}),"Escape dismisses crossfade settings");
        check(!window->findChild<QObject *>("crossfadeToggle"), "closed audio popup releases its controls");
        QMetaObject::invokeMethod(fadeMenu,"open");
        check(wait([&]{return fadeMenu->property("opened").toBool()&&!cider.crossfadeBusy()&&!cider.audioBusy();}), "audio popup reopens and reloads confirmed state");
        auto *reopenedToggle=window->findChild<QObject *>("crossfadeToggle");
        check(reopenedToggle&&reopenedToggle->property("checked").toBool()==cider.crossfade(), "recreated audio controls retain the confirmed Cider value");
        QMetaObject::invokeMethod(fadeMenu,"close");
        wait([&]{return !fadeMenu->property("visible").toBool();});
        fadeMenu->setProperty("service",originalFade);
    }
    click("libraryTab_songs");wait([&]{return !browser.busy();});click("recentSongsTab");wait([&]{return !browser.busy();});
    check(browser.section()=="recent"&&list->property("count").toInt()==1,"Recently played opens inside Songs without another panel");
    QTest::qWait(200);if(!captures.isEmpty())window->grabWindow().save(captures+"/09-recently-played.png");
    click("libraryActions0");click("addQueueAction");check(wait([&]{return !actions.busy();})&&editBody["id"]=="recent-new","recently played rows expose Add to Queue");
    click("libraryTab_playlists");wait([&]{return !browser.busy();});pagedTracks=true;click("libraryRow0");wait([&]{return !browser.busy();});
    testKeyClick(window,Qt::Key_F,Qt::ControlModifier);QTest::qWait(100);
    auto *collectionSearch=find(window->contentItem(),"collectionSearchInput");
    check(collectionSearch&&collectionSearch->hasActiveFocus()&&!browser.collection().isEmpty(),"Ctrl+F focuses playlist search without navigating away");
    for(char c:QByteArray("cafe"))testKeyClick(window,c);
    check(wait([&]{return !browser.busy()&&!browser.hasMore();})&&browser.items().size()==1&&browser.items()[0].toMap()["id"]=="i.hidden","playlist search UI finds a track beyond its first page");
    QTest::qWait(200);if(!captures.isEmpty())window->grabWindow().save(captures+"/09-playlist-search.png");
    click("libraryActions0");click("playNextAction");check(wait([&]{return !actions.busy();})&&editBody["id"]=="i.hidden","filtered playlist row queues the matching song");
    testKeyClick(window,Qt::Key_Escape);QTest::qWait(100);check(browser.collectionQuery().isEmpty()&&!browser.collection().isEmpty()&&browser.items().size()==4,"first Escape clears the playlist filter");
    testKeyClick(window,Qt::Key_Escape);QTest::qWait(100);check(browser.collection().isEmpty(),"second Escape returns to playlists");pagedTracks=false;
    click("libraryRow0");wait([&]{return !browser.busy();});QTest::qWait(200);
    const int selectionPlays=plays;
    auto selectRow=[&](int index,Qt::KeyboardModifiers mods) {
        auto *row=find(window->contentItem(),"libraryRow"+QString::number(index));
        if(row)QTest::mouseClick(window,Qt::LeftButton,mods,row->mapToScene(QPointF(35,row->height()/2)).toPoint());
        QTest::qWait(100);
    };
    selectRow(0,Qt::ControlModifier);selectRow(1,Qt::ShiftModifier);
    check(panel->property("selectionCount").toInt()==2&&plays==selectionPlays,"Ctrl-click and Shift-click select a range without playback");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/11-multi-selection.png");
    queueIds.clear();click("selectedTrackActions");click("playNextAction");
    check(wait([&]{return !actions.busy();})&&queueIds==QStringList{"i.second","i.first"},"selection toolbar queues all selected tracks in displayed order");
    testKeyClick(window,Qt::Key_Escape);QTest::qWait(100);
    check(panel->property("selectionCount").toInt()==0&&!browser.collection().isEmpty(),"Escape clears selection before leaving the album");
    click("libraryActions0");click("selectTrackAction");
    check(panel->property("selectionCount").toInt()==1,"song menu offers a discoverable selection entry");
    list->forceActiveFocus();testKeyClick(window,Qt::Key_A,Qt::ControlModifier);QTest::qWait(100);
    check(panel->property("selectionCount").toInt()==2,"Ctrl+A selects loaded playable songs and skips unavailable tracks");
    list->setProperty("currentIndex",0);testKeyClick(window,Qt::Key_Space);QTest::qWait(100);
    check(panel->property("selectionCount").toInt()==1&&plays==selectionPlays,"Space toggles the focused selection instead of playback");
    selectRow(0,Qt::ControlModifier);pagedTracks=true;browser.reload();wait([&]{return !browser.busy();});QTest::qWait(200);
    selectRow(0,Qt::ControlModifier);browser.more();wait([&]{return !browser.busy();});QTest::qWait(200);
    check(panel->property("selectionCount").toInt()==1,"loading more tracks preserves the selected row identity");pagedTracks=false;
    browser.setCollectionQuery("Second");QTest::qWait(100);
    check(panel->property("selectionCount").toInt()==0,"filtering clears selection so hidden tracks cannot be queued by accident");
    browser.setCollectionQuery("");QTest::qWait(200);
    auto *artistLink=find(window->contentItem(),"libraryArtwork0Artist");
    check(artistLink&&artistLink->height()>=24,"artist links provide a usable hit area without overlapping the song action");
    if(artistLink) {
        const auto edge=artistLink->mapToScene(QPointF(artistLink->width()/2,artistLink->height()-2)).toPoint();
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,edge);QTest::qWait(150);
    }

    check(wait([&]{return !browser.busy();})&&browser.collection()["type"]=="artists"&&plays==selectionPlays,"clicking a track artist opens its page without playing the track");
    QTest::qWait(250);if(!captures.isEmpty())window->grabWindow().save(captures+"/11-artist-page.png");
    click("pinArtistButton");check(browser.isPinned(browser.collection()),"artist page pin button saves the artist");
    browser.back();browser.setQuery("");wait([&]{return !browser.busy();});QTest::qWait(220);
    if(!captures.isEmpty())window->grabWindow().save(captures+"/16-pinned-artist.png");
    const int beforeArtistPin=plays;click("pinnedCollection0");
    check(wait([&]{return !browser.busy();})&&browser.collection()["type"]=="artists"&&browser.artistView()=="songs"&&plays==beforeArtistPin,"artist pin opens Top songs without starting playback");
    click("pinArtistButton");check(browser.pins().isEmpty(),"artist page pin button also unpins");
    click("artistSimilarTab");check(wait([&]{return !browser.busy();})&&panel->property("artistSimilar").toBool(),"similar artist tab opens without adding another panel");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/18-similar-artists.png");
    click("libraryRow0");wait([&]{return !browser.busy();});click("libraryBack");check(browser.artistView()=="similar","similar artist navigation restores the selected tab");
    click("artistAlbumsTab");wait([&]{return !browser.busy();});
    click("artistTopSongsTab");check(wait([&]{return !browser.busy();})&&panel->property("artistSongs").toBool()&&browser.items().first().toMap()["type"]=="songs","artist Top songs tab displays playable song rows");
    QTest::qWait(200);if(!captures.isEmpty())window->grabWindow().save(captures+"/15-artist-top-songs.png");
    const int beforeTopPlay=plays;click("libraryRow0");check(wait([&]{return !browser.starting();})&&plays==beforeTopPlay+1&&playedId=="top1"&&playedPath.endsWith("play-item"),"artist top song starts through Cider without leaving the artist page");
    click("libraryActions0");click("playNextAction");check(wait([&]{return !actions.busy();})&&editBody["id"]=="top1","artist top songs retain Play next actions");
    click("artistAlbumsTab");wait([&]{return !browser.busy();});
    click("libraryRow0");check(wait([&]{return !browser.busy();})&&browser.collection()["type"]=="albums","artist album row opens its songs");
    click("libraryBack");check(browser.collection()["type"]=="artists"&&browser.artistView()=="albums","UI Back restores artist and its Albums view");
    click("libraryBack");click("libraryTab_playlists");wait([&]{return !browser.busy();});QTest::qWait(200);
    click("libraryActions0");click("pinCollectionAction");
    check(browser.pins().size()==1,"collection menu pins without another panel");
    click("libraryTab_search");wait([&]{return !browser.busy();});QTest::qWait(250);
    auto *pins=find(window->contentItem(),"pinnedCollections");
    check(pins&&pins->isVisible(),"empty search page displays pinned collections");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/10-pinned-collections.png");
    const int pinPlays=plays; click("pinnedCollection0");
    check(wait([&]{return !browser.starting();})&&plays==pinPlays+1,"pinned cover starts its collection with one click");
    browser.togglePin(browser.pins().first().toMap());QTest::qWait(100);
    check(!pins->isVisible(),"unpinning the final collection restores the empty search view");

    window->setProperty("libraryOpen",false);window->setProperty("queueOpen",true);QTest::qWait(150);
    auto *form=find(window->contentItem(),"ciderConnectionForm");
    check(form!=nullptr,"connection controls exist inside the queue");
    if(form) {
        const auto originalService=form->property("service"); form->setProperty("service",QVariant::fromValue(&cider));
        rejectProbe=true;cider.reconnect();wait([&]{return cider.needsToken();});QTest::qWait(150);
        check(form->isVisible(),"expired permission presents the connection controls");
        auto *manual=find(window->contentItem(),"ciderTokenInput");
        check(manual&&!manual->isVisible(),"manual token entry stays tucked away by default");
        if(!captures.isEmpty())window->grabWindow().save(captures+"/10-cider-connection.png");
        authDelay=500; click("authorizeCiderButton");check(cider.authorizing(),"Connect to Cider opens only the fixture approval request");
        click("authorizeCiderButton");check(!cider.authorizing(),"Cancel remains usable during approval");QTest::qWait(600);
        click("manualCiderTokenButton");check(manual->isVisible(),"manual token fallback can be expanded");
        authDelay=0;rejectProbe=false;click("authorizeCiderButton");
        check(wait([&]{return cider.queueReady();})&&!form->isVisible(),"successful approval replaces the form with the queue");
        form->setProperty("service",originalService);
    }
    window->setProperty("queueOpen",false);window->setProperty("libraryOpen",true);QTest::qWait(200);
    auto *player=qobject_cast<Player *>(QQmlEngine::contextForObject(window)->contextProperty("player").value<QObject *>());
    const bool motion=player->motion();player->setMotion(true);
    window->setProperty("libraryOpen",false);window->setProperty("libraryOpen",true);QTest::qWait(30);
    check(find(window->contentItem(),"libraryPanel")==panel,"reopening retains the browser and its navigation state");
    check(panel->opacity()<1,"browser entrance uses a short opacity transition");QTest::qWait(200);
    check(panel->opacity()==1,"browser entrance settles without lingering animation");
    window->setProperty("libraryOpen",true);browser.setSection("search");browser.setKind("stations");browser.setQuery("Chill");wait([&]{return !browser.busy();});QTest::qWait(300);
    if(!captures.isEmpty())window->grabWindow().save(captures+"/12-stations.png");
    click("libraryActions0");click("pinCollectionAction");check(browser.isPinned(browser.items().first().toMap()),"station row menu exposes pinning");browser.togglePin(browser.items().first().toMap());
    browser.setSection("songs");wait([&]{return !browser.busy();});click("recentlyAddedTab");wait([&]{return !browser.busy();});
    check(browser.newestFirst(),"Recently added chip switches the songs sort");QTest::qWait(250);if(!captures.isEmpty())window->grabWindow().save(captures+"/12-recently-added.png");
    browser.setSection("sessions");QTest::qWait(250);click("libraryRow0");
    check(browser.collection()["type"]=="saved-queues"&&browser.items().size()==2,"saved queue opens inside the existing sidebar");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/12-saved-queue.png");
    click("libraryActions0");click("copySongLinkAction");check(QGuiApplication::clipboard()->text()=="https://music.apple.com/ca/song/123","row menu copies the selected song link");
    check(wait([&]{return !panel->property("actionsOpen").toBool();}),"copy action closes before reopening collection menu");
    const int beforeSavedUiWrites=edits,beforeSavedUiPlays=plays;
    click("collectionActions");click("renameSavedQueueAction");
    auto *renameField=find(window->contentItem(),"savedQueueName");
    check(renameField&&renameField->property("text").toString()==browser.collection()["title"],"Rename opens the shared name dialog with the current title");
    auto *saveAction=find(window->contentItem(),"confirmSaveQueue");
    auto *cancelSave=find(window->contentItem(),"cancelSaveQueue");
    auto *saveDialog=window->findChild<QObject *>("saveQueuePopup");
    check(saveAction&&cancelSave&&saveDialog&&saveDialog->property("modal").toBool()&&saveAction->y()==cancelSave->y()&&cancelSave->x()+cancelSave->width()<saveAction->x(),"Save dialog is modal with aligned, separated Cancel and Save actions");
    check(saveAction->property("text").toString()=="Rename","rename dialog names its confirmation action correctly");
    check(qAbs(saveDialog->property("y").toReal()+saveDialog->property("height").toReal()/2-window->property("layoutHeight").toReal()/2)<1,"save and rename dialogs align to the logical window center");
    const auto dialogSize=window->size();
    const bool dialogLibraryOpen=window->property("libraryOpen").toBool(),dialogQueueOpen=window->property("queueOpen").toBool();
    testKeyClick(window,Qt::Key_M,Qt::ControlModifier);
    testKeyClick(window,Qt::Key_L,Qt::ControlModifier);
    testKeyClick(window,Qt::Key_B,Qt::ControlModifier);QTest::qWait(200);
    check(saveDialog->property("visible").toBool()&&window->size()==dialogSize&&!window->property("miniMode").toBool(),"dialog blocks Mini shortcut so its content cannot be clipped by a window resize");
    check(window->property("libraryOpen").toBool()==dialogLibraryOpen&&window->property("queueOpen").toBool()==dialogQueueOpen,"dialog shortcuts cannot switch background library or queue panels");
    if(renameField) {
        renameField->setProperty("text","   ");
        check(!saveAction->isEnabled()&&cancelSave->isEnabled(),"blank queue names disable Save while Cancel remains available");
        renameField->setProperty("text","Late night mix");
        renameField->forceActiveFocus(Qt::TabFocusReason);
        testKeyClick(window,Qt::Key_Tab);
        check(cancelSave->hasActiveFocus(),"Save dialog keyboard order reaches Cancel after the name field");
        testKeyClick(window,Qt::Key_Tab);
        check(saveAction->hasActiveFocus(),"Save dialog keyboard order follows the visible Cancel then Save actions");
    }
    if(!captures.isEmpty())window->grabWindow().save(captures+"/17-rename-queue.png");
    click("confirmSaveQueue");check(browser.collection()["title"]=="Late night mix","Rename saves and updates the collection header");
    const auto firstSavedId=browser.items().first().toMap()["id"];
    click("libraryActions0");click("savedTrackDown");check(browser.items().last().toMap()["id"]==firstSavedId,"saved song menu moves the selected occurrence down");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/17-edited-queue.png");
    click("libraryActions0");click("savedTrackRemove");check(browser.items().size()==1&&browser.collection()["trackCount"]==1,"saved song menu removes the song and updates its count");
    check(edits==beforeSavedUiWrites&&plays==beforeSavedUiPlays,"saved queue UI editing leaves Cider playback and queue untouched");
    click("collectionActions");if(!captures.isEmpty())window->grabWindow().save(captures+"/12-saved-queue-menu.png");click("deleteSavedQueueAction");QTest::qWait(250);
    auto *deleteCancel=find(window->contentItem(),"cancelDeleteQueue");
    check(deleteCancel&&deleteCancel->hasActiveFocus(),"delete confirmation initially focuses Cancel");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/26-delete-queue.png");
    click("confirmDeleteQueue");
    check(browser.items().isEmpty()&&browser.collection().isEmpty(),"saved queue deletion requires confirmation and returns to the list");
    fixtureQueue={QJsonObject{{"track",resource("123","songs","First Light")}}};fixturePosition=0;cider.refreshQueue();wait([&]{return !cider.queueBusy();});
    auto *savePopup=window->findChild<QObject *>("saveQueuePopup");check(savePopup!=nullptr,"save queue dialog exists");
    if(savePopup) {
        const auto originalService=savePopup->property("service");savePopup->setProperty("service",QVariant::fromValue(&browser));QMetaObject::invokeMethod(savePopup,"open");QTest::qWait(250);
        auto *name=find(window->contentItem(),"savedQueueName");if(name)name->setProperty("text","Night drive");
        if(!captures.isEmpty())window->grabWindow().save(captures+"/12-save-queue.png");
        click("confirmSaveQueue");QTest::qWait(200);
        check(!savePopup->property("visible").toBool()&&browser.items().size()==1,"named queue saves through the dialog without changing playback");
        if(!browser.items().isEmpty())browser.deleteSavedQueue(browser.items().first().toMap()["id"].toString());
        savePopup->setProperty("service",originalService);
    }
    // New workflows use only the isolated server, including queue mutations.
    browser.setSection("albums");wait([&]{return !browser.busy();});browser.open(0);wait([&]{return !browser.busy();});
    browser.shuffleCollection(browser.collection());check(wait([&]{return !browser.starting();})&&playedShuffle&&playedPath.endsWith("play-collection"),"Shuffle collection sends explicit shuffle without changing the global mode first");
    browser.playCollection();check(wait([&]{return !browser.starting();})&&!playedShuffle,"normal collection Play explicitly disables shuffle");browser.back();
    browser.setSection("search");browser.setQuery("unsubmitted");wait([&]{return !browser.busy();});
    check(!browser.recentSearches().contains("unsubmitted"),"typing alone does not persist search history");
    browser.rememberSearch();Library searchesReload(&cider);
    check(searchesReload.recentSearches().first()=="unsubmitted","intentional search persists locally");
    browser.setQuery("UNSUBMITTED");browser.rememberSearch();
    check(browser.recentSearches().count("unsubmitted")==0 && browser.recentSearches().first()=="UNSUBMITTED","recent searches deduplicate case-insensitively");
    for(int i=0;i<12;++i){browser.setQuery(QString("term %1").arg(i));browser.rememberSearch();}
    check(browser.recentSearches().size()==8 && browser.recentSearches().first()=="term 11","history is bounded to eight most recent searches");
    browser.removeRecentSearch("term 11");Library removedSearch(&cider);
    check(!removedSearch.recentSearches().contains("term 11"),"removing a recent search survives relaunch");
    const auto recentBefore=browser.recentSearches();browser.setQuery("https://music.apple.com/ca/song/123");browser.rememberSearch();
    check(browser.recentSearches()==recentBefore,"pasted links are not stored as search terms");
    browser.setQuery("");wait([&]{return !browser.busy();});
    actions.setObserving(true);wait([&]{return actions.ready();});actions.toggleDislike();
    check(wait([&]{return !actions.busy()&&actions.ready();})&&rating==-1&&actions.disliked()&&!actions.favorite(),"Suggest less writes dislike and reads back the rating");
    actions.toggleDislike();check(wait([&]{return !actions.busy()&&actions.ready();})&&rating==0&&!actions.disliked()&&editMethod=="DELETE","Clear dislike uses the rating delete endpoint");
    rejectEdit=true;actions.toggleDislike();check(wait([&]{return !actions.busy();})&&rating==0&&!actions.disliked(),"rejected dislike never changes the confirmed rating");rejectEdit=false;actions.setObserving(false);
    cider.refreshAudioQuality();check(wait([&]{return !cider.qualityBusy();})&&cider.audioQuality()=="AAC 256kbps\nOutput: 48 kHz · 2 channels","audio quality separates stream encoding from device output");
    quality={};cider.refreshAudioQuality();check(wait([&]{return !cider.qualityBusy();})&&cider.audioQuality().contains("unavailable"),"missing quality fields never imply lossless or Atmos");
    const int quietQuality=qualityReads;QTest::qWait(150);check(qualityReads==quietQuality,"quality details never poll while closed");
    quality={{"flavorLabel","AAC 256kbps"},{"deviceAudioConfig",QJsonObject{{"sampleRate",48000},{"channelCount",2}}}};
    transactionQueue=true;rejectQueueAt=-1;queueIds.clear();
    auto queueTrack=[&](QString id){return QJsonObject{{"track",resource(id,"songs",id)}};};
    auto ids=[&]{QStringList values;for(const auto &v:fixtureQueue)values.append(v.toObject()["track"].toObject()["id"].toString());return values;};
    auto syncQueue=[&]{cider.refreshQueue();return wait([&]{return !cider.queueBusy()&&!cider.controlBusy();});};
    auto track=[&](QString id){return Library::item(resource(id,"songs",id));};
    auto resetBatch=[&]{fixtureQueue={queueTrack("h"),queueTrack("now"),queueTrack("a"),queueTrack("b"),queueTrack("c"),queueTrack("d"),queueTrack("e")};fixturePosition=1;return syncQueue();};
    auto batchIdle=[&]{return wait([&]{return !cider.controlBusy()&&!cider.queueBusy();});};
    resetBatch();cider.editQueueSelection({3,4},"up",cider.queueRevision());
    check(batchIdle()&&ids()==QStringList{"h","now","b","c","a","d","e"}&&fixturePosition==1,"moving a selected block up preserves its order, history and current playback");
    resetBatch();cider.editQueueSelection({2,4},"down",cider.queueRevision());
    check(batchIdle()&&ids()==QStringList{"h","now","b","a","d","c","e"},"moving disjoint selections down moves each occurrence exactly once");
    resetBatch();cider.editQueueSelection({4,6},"next",cider.queueRevision());
    check(batchIdle()&&ids()==QStringList{"h","now","c","e","a","b","d"},"Play next gathers selected upcoming songs in their original order");
    resetBatch();cider.editQueueSelection({2,4},"end",cider.queueRevision());
    check(batchIdle()&&ids()==QStringList{"h","now","b","d","e","a","c"},"Move to end preserves both selected and unselected relative order");
    resetBatch();const int batchBoundaryWrites=queueWrites+deleteWrites;
    cider.editQueueSelection({2,3},"up",cider.queueRevision());cider.editQueueSelection({0,2},"remove",cider.queueRevision());cider.editQueueSelection({1},"remove",cider.queueRevision());cider.editQueueSelection({3,3},"remove",cider.queueRevision());cider.editQueueSelection({2.5},"remove",cider.queueRevision());
    check(!cider.controlBusy()&&queueWrites+deleteWrites==batchBoundaryWrites,"batch boundaries, duplicate indices and history/current selection cannot issue writes");
    fixtureQueue={queueTrack("now"),queueTrack("same"),queueTrack("other"),queueTrack("same")};fixturePosition=0;syncQueue();deletedIndices.clear();
    cider.editQueueSelection({1,3},"remove",cider.queueRevision());
    check(batchIdle()&&ids()==QStringList{"now","other"}&&deletedIndices==QList<int>{3,1},"batch removal handles repeated songs as distinct occurrences and deletes backwards");
    resetBatch();const int preflightBatchWrites=deleteWrites;const int oldBatchRevision=cider.queueRevision();fixtureQueue.append(queueTrack("extra"));syncQueue();
    cider.editQueueSelection({2,3},"remove",oldBatchRevision);changeQueueOnRead=true;cider.editQueueSelection({2,3},"remove",cider.queueRevision());
    check(batchIdle()&&deleteWrites==preflightBatchWrites,"stale selection and a changed fresh preflight prevent every batch write");
    resetBatch();const int partialBatchWrites=deleteWrites;rejectDeleteAt=deleteWrites+2;cider.editQueueSelection({2,3,4},"remove",cider.queueRevision());
    check(batchIdle()&&deleteWrites==partialBatchWrites+2&&ids()==QStringList{"h","now","a","b","d","e"},"a rejected batch write stops remaining operations without replaying confirmed removals");rejectDeleteAt=-1;
    resetBatch();const int raceBatchWrites=deleteWrites;changeAfterDelete=true;cider.editQueueSelection({2,3},"remove",cider.queueRevision());
    check(batchIdle()&&deleteWrites==raceBatchWrites+1,"an external edit during a batch stops the next removal");
    resetBatch();const int advanceBatchWrites=deleteWrites;advanceAfterDelete=true;cider.editQueueSelection({2,3},"remove",cider.queueRevision());
    check(batchIdle()&&deleteWrites==advanceBatchWrites+1,"playback advancing during a batch stops subsequent writes");
    resetBatch();const int failedBatchReads=deleteWrites;failAfterDelete=true;cider.editQueueSelection({2,3},"remove",cider.queueRevision());
    check(batchIdle()&&deleteWrites==failedBatchReads+1,"failed batch verification releases controls without sending further writes");failQueueRead=false;syncQueue();
    resetBatch();const int reauthBatchWrites=deleteWrites;cider.editQueueSelection({2,3},"remove",cider.queueRevision());cider.connectQueue("test-library-token");
    check(batchIdle()&&deleteWrites==reauthBatchWrites,"reconnecting Cider cancels pending batch edits before further writes");
    fixtureQueue={queueTrack("a"),queueTrack("b"),queueTrack("c")};fixturePosition=0;syncQueue();
    cider.insertQueue({track("x"),track("x"),track("y")},1,cider.queueRevision());
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&ids()==QStringList{"a","x","x","y","b","c"}&&fixturePosition==0,"queue insertion preserves batch order and duplicates without changing playback");
    fixtureQueue={queueTrack("a"),queueTrack("b"),queueTrack("c")};fixturePosition=0;radioTailInsertion=true;syncQueue();
    cider.insertQueue({track("x"),track("x"),track("y")},3,cider.queueRevision());
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&ids()==QStringList{"a","b","c","x","x","y"},"insertion detects additions before Cider's radio tail and moves them to the requested slot");
    cider.removeQueue(1,cider.queueRevision());wait([&]{return !cider.controlBusy()&&!cider.queueBusy();});cider.undoQueueRemoval();
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&ids()==QStringList{"a","b","c","x","x","y"},"Undo restores an occurrence when Cider inserts before its radio tail");radioTailInsertion=false;
    const int beforeStale=queueWrites;const int stale=cider.queueRevision();fixtureQueue.append(queueTrack("external"));syncQueue();cider.insertQueue({track("z")},1,stale);
    check(!cider.controlBusy()&&queueWrites==beforeStale,"stale drop cannot mutate the queue");
    changeQueueOnRead=true;cider.insertQueue({track("z")},1,cider.queueRevision());
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&queueWrites==beforeStale,"fresh preflight catches external edits before insertion");
    const int writesBeforeRace=queueWrites;changeAfterAppend=true;cider.insertQueue({track("z")},1,cider.queueRevision());
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&queueWrites==writesBeforeRace+1&&ids().contains("z"),"external edit after append prevents moving the wrong row and never replays the append");
    fixtureQueue={queueTrack("a"),queueTrack("b"),queueTrack("c")};fixturePosition=0;syncQueue();
    cider.removeQueue(1,cider.queueRevision());check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&cider.canUndoQueue()&&ids()==QStringList{"a","c"},"confirmed upcoming removal offers Undo");
    cider.undoQueueRemoval();check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&ids()==QStringList{"a","b","c"}&&fixturePosition==0&&!cider.canUndoQueue(),"Undo restores original queue position exactly once");
    const int afterUndo=queueWrites;cider.undoQueueRemoval();QTest::qWait(50);check(queueWrites==afterUndo,"repeating Undo does not duplicate a track");
    fixturePosition=2;syncQueue();cider.removeQueue(0,cider.queueRevision());wait([&]{return !cider.controlBusy()&&!cider.queueBusy();});cider.undoQueueRemoval();
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&ids()==QStringList{"a","b","c"}&&fixturePosition==2,"Undo of history removal preserves the currently playing track");
    cider.removeQueue(2,cider.queueRevision());check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&!cider.canUndoQueue(),"removing the current song does not offer a misleading playback Undo");
    fixturePosition=0;syncQueue();cider.removeQueue(1,cider.queueRevision());wait([&]{return !cider.controlBusy()&&!cider.queueBusy();});fixtureQueue.append(queueTrack("external"));syncQueue();
    check(!cider.canUndoQueue(),"external queue edits invalidate Undo");
    fixtureQueue={queueTrack("history"),queueTrack("a"),queueTrack("b"),queueTrack("a"),queueTrack("b"),queueTrack("c")};fixturePosition=1;syncQueue();
    const int beforeCleanup=queueWrites;const auto cleanup=cider.previewCleanup("duplicates");
    check(cleanup["indices"].toList()==QVariantList{3,4}&&queueWrites==beforeCleanup,"duplicate preview lists upcoming repeats without writing or touching history");
    deletedIndices.clear();cider.cleanQueue("duplicates",cleanup["revision"].toInt());
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&ids()==QStringList{"history","a","b","c"}&&fixturePosition==1&&deletedIndices==QList<int>{4,3},"cleanup removes duplicates from the end and preserves current playback and history");
    check(!cider.canUndoQueue(),"batch cleanup does not expose an unrelated single-track Undo");
    const int afterCleanup=queueWrites;cider.cleanQueue("duplicates",cider.queueRevision());
    check(queueWrites==afterCleanup&&cider.previewCleanup("duplicates")["rows"].toList().isEmpty(),"clean queues require no mutation requests");
    const int cleanRevision=cider.queueRevision();fixtureQueue.append(queueTrack("c"));syncQueue();cider.cleanQueue("upcoming",cleanRevision);
    check(queueWrites==afterCleanup,"stale cleanup previews cannot mutate the queue");
    changeQueueOnRead=true;cider.cleanQueue("upcoming",cider.queueRevision());
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&queueWrites==afterCleanup,"cleanup revalidates the preview against Cider before its first removal");
    const int beforeReauthCleanup=deleteWrites;cider.cleanQueue("upcoming",cider.queueRevision());cider.connectQueue("test-library-token");
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&deleteWrites==beforeReauthCleanup,"replacing Cider credentials cancels cleanup before further writes");
    // The same catalog song may appear under a library identifier.
    auto catalogDuplicate=resource("i.a","library-songs","Different title");auto duplicateAttrs=catalogDuplicate["attributes"].toObject();duplicateAttrs["playParams"]=QJsonObject{{"id","i.a"},{"catalogId","a"}};catalogDuplicate["attributes"]=duplicateAttrs;
    fixtureQueue={queueTrack("a"),QJsonObject{{"track",catalogDuplicate}},queueTrack(""),queueTrack(""),queueTrack("b")};fixturePosition=0;syncQueue();
    check(cider.previewCleanup("duplicates")["indices"].toList()==QVariantList{1},"cleanup matches catalog identities and never merges unknown IDs by title");
    fixtureQueue={queueTrack("h"),queueTrack("a"),queueTrack("b"),queueTrack("c"),queueTrack("d")};fixturePosition=1;syncQueue();
    const int beforePartial=deleteWrites;rejectDeleteAt=deleteWrites+2;cider.cleanQueue("upcoming",cider.queueRevision());
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&deleteWrites==beforePartial+2&&ids()==QStringList{"h","a","b","c"},"cleanup stops after a rejected removal and does not replay confirmed writes");rejectDeleteAt=-1;
    fixtureQueue={queueTrack("a"),queueTrack("b"),queueTrack("c")};fixturePosition=0;syncQueue();
    const int beforeChangedCleanup=deleteWrites;changeAfterDelete=true;cider.cleanQueue("upcoming",cider.queueRevision());
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&deleteWrites==beforeChangedCleanup+1&&ids()==QStringList{"a","b","external"},"an external queue edit stops cleanup before the next deletion");
    fixtureQueue={queueTrack("a"),queueTrack("b"),queueTrack("c")};fixturePosition=0;syncQueue();
    const int beforeAdvance=deleteWrites;advanceAfterDelete=true;cider.cleanQueue("upcoming",cider.queueRevision());
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&deleteWrites==beforeAdvance+1&&ids()==QStringList{"a","b"}&&fixturePosition==1,"cleanup stops when playback advances to an upcoming song");
    fixtureQueue={queueTrack("a"),queueTrack("b"),queueTrack("c")};fixturePosition=0;syncQueue();
    const int beforeReadFailure=deleteWrites;failAfterDelete=true;cider.cleanQueue("upcoming",cider.queueRevision());
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&deleteWrites==beforeReadFailure+1,"failed cleanup verification releases busy state and stops further writes");failQueueRead=false;syncQueue();
    fixtureQueue={queueTrack("h"),queueTrack("a"),queueTrack("b"),queueTrack("c")};fixturePosition=1;syncQueue();cider.cleanQueue("upcoming",cider.queueRevision());
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&ids()==QStringList{"h","a"}&&fixturePosition==1,"clear upcoming keeps history and the playing song");
    fixtureQueue={queueTrack("a"),queueTrack("a"),queueTrack("b")};fixturePosition=-1;syncQueue();cider.cleanQueue("duplicates",cider.queueRevision());
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&ids()==QStringList{"a","b"},"with no current song cleanup keeps the first occurrence");
    fixtureQueue={};fixturePosition=-1;syncQueue();
    check(cider.previewCleanup("upcoming")["rows"].toList().isEmpty()&&cider.previewCleanup("invalid").isEmpty(),"empty queues and unsupported cleanup modes are harmless");

    const int beforeReject=queueWrites;rejectEdit=true;cider.insertQueue({track("z")},1,cider.queueRevision());
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&queueWrites==beforeReject,"rejected append is not retried or followed by a move");rejectEdit=false;
    // Exercise actual pointer and popup wiring against the same mock service.
    const auto originalCider=window->property("ciderService");window->setProperty("ciderService",QVariant::fromValue(&cider));
    window->setProperty("useCider",true);window->setProperty("libraryOpen",true);
    browser.setSection("songs");wait([&]{return !browser.busy();});QTest::qWait(250);
    auto *dragArea=find(window->contentItem(),"libraryDragArea");check(dragArea!=nullptr,"library artwork has a drag target");
    if(dragArea){
        fixtureQueue={queueTrack("a"),queueTrack("b"),queueTrack("c")};fixturePosition=0;syncQueue();
        const auto origin=dragArea->mapToScene(QPointF(dragArea->width()/2,32)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,origin);QTest::mouseMove(window,origin+QPoint(18,0),50);QTest::qWait(250);
        check(window->property("libraryDragging").toBool()&&window->property("queueOpen").toBool(),"dragging artwork reveals Queue without losing the pointer grab");
        auto *queueList=find(window->contentItem(),"trackList");const auto drop=queueList->mapToScene(QPointF(120,76)).toPoint();
        QTest::mouseMove(window,drop,50);QTest::qWait(100);
        check(window->property("libraryDropIndex").toInt()==1,"drag shows an insertion slot in the upcoming queue");
        if(!captures.isEmpty())window->grabWindow().save(captures+"/13-drag-queue.png");
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,drop);check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&!window->property("libraryDragging").toBool()&&fixtureQueue.size()==4,"dropping artwork inserts the selected song");
    }
    window->setProperty("libraryOpen",true);QTest::qWait(200);
    if(dragArea) {
        const int beforeCancel=queueWrites;
        const auto origin=dragArea->mapToScene(QPointF(dragArea->width()/2,32)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,origin);QTest::mouseMove(window,origin+QPoint(18,0),50);QTest::qWait(150);
        testKeyClick(window,Qt::Key_Escape);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,origin+QPoint(18,0));QTest::qWait(100);
        check(!window->property("libraryDragging").toBool()&&window->property("libraryOpen").toBool()&&queueWrites==beforeCancel,"Escape cancels a drag and restores the browser without queue writes");
    }
    browser.more();wait([&]{return !browser.busy();});QTest::qWait(150);QMetaObject::invokeMethod(panel,"clearSelection");
    selectRow(0,Qt::ControlModifier);selectRow(1,Qt::ShiftModifier);
    check(panel->property("selectionCount").toInt()==2,"two browser songs can be selected for dragging");
    if(dragArea) {
        fixtureQueue={queueTrack("a"),queueTrack("b"),queueTrack("c")};fixturePosition=0;syncQueue();
        const auto origin=dragArea->mapToScene(QPointF(dragArea->width()/2,32)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,origin);QTest::mouseMove(window,origin+QPoint(18,0),50);QTest::qWait(200);
        auto *queueList=find(window->contentItem(),"trackList");const auto drop=queueList->mapToScene(QPointF(120,76)).toPoint();
        QTest::mouseMove(window,drop,50);QTest::qWait(100);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,drop);
        check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&ids()==QStringList{"a","i.saved1","i.saved2","b","c"},"dragging a selected artwork inserts the entire selection in display order");
    }
    window->setProperty("libraryOpen",true);browser.setSection("search");browser.setQuery("");wait([&]{return !browser.busy();});QTest::qWait(200);
    if(!captures.isEmpty())window->grabWindow().save(captures+"/13-recent-searches.png");
    auto *recentItem=find(window->contentItem(),"recentSearch0");check(recentItem&&recentItem->isVisible(),"empty Search exposes recent searches");
    if(recentItem){click("recentSearch0");check(!browser.query().isEmpty(),"recent search restores its query");}
    QMetaObject::invokeMethod(window,"showAudioQuality");wait([&]{return !cider.qualityBusy();});QTest::qWait(250);
    if(!captures.isEmpty())window->grabWindow().save(captures+"/13-audio-quality.png");
    auto *qualityPopup=window->findChild<QObject *>("audioQualityPopup");check(qualityPopup&&qualityPopup->property("visible").toBool(),"audio quality opens in a themed popup");if(qualityPopup)QMetaObject::invokeMethod(qualityPopup,"close");
    window->setProperty("queueOpen",true);syncQueue();cider.removeQueue(1,cider.queueRevision());wait([&]{return !cider.controlBusy()&&!cider.queueBusy();});QTest::qWait(200);
    auto *undoButton=find(window->contentItem(),"undoQueueButton");check(undoButton&&undoButton->isVisible(),"confirmed removal exposes the Undo action");
    auto *feedbackTimer=window->findChild<QObject *>("noticeTimer");
    check(feedbackTimer&&!feedbackTimer->property("running").toBool(),"actionable Undo feedback does not auto-dismiss");
    testKeyClick(window,Qt::Key_G,Qt::AltModifier);
    check(undoButton->hasActiveFocus(),"Alt+G reaches Undo without opening another panel");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/13-undo.png");
    if(undoButton&&undoButton->isVisible()){click("undoQueueButton");check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&!cider.canUndoQueue(),"Undo button restores the removed item");}
    fixtureQueue={queueTrack("History"),queueTrack("Current"),queueTrack("Upcoming"),queueTrack("Current"),queueTrack("Upcoming")};fixturePosition=1;syncQueue();QTest::qWait(100);
    const int beforePreviewUi=queueWrites;
    click("savedQueueMenuButton");click("deduplicateQueueAction");
    auto *cleanupPopup=window->findChild<QObject *>("cleanupPopup");
    check(cleanupPopup&&cleanupPopup->property("visible").toBool()&&cleanupPopup->property("preview").toMap()["rows"].toList().size()==2&&queueWrites==beforePreviewUi,"queue menu opens a read-only duplicate preview");
    check(wait([&]{return find(window->contentItem(),"cancelCleanup")->hasActiveFocus();}),"cleanup confirmation initially focuses Cancel");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/16-cleanup-preview.png");
    click("cancelCleanup");check(!cleanupPopup->property("visible").toBool()&&queueWrites==beforePreviewUi,"Cancel leaves the Cider queue untouched");
    click("savedQueueMenuButton");click("deduplicateQueueAction");
    fixtureQueue.append(queueTrack("External"));syncQueue();QTest::qWait(100);
    check(cleanupPopup->property("stale").toBool()&&!find(window->contentItem(),"confirmCleanup")->isEnabled(),"changing the queue invalidates the visible cleanup preview");
    testKeyClick(window,Qt::Key_Escape);check(wait([&]{return !cleanupPopup->property("visible").toBool();}),"Escape dismisses the cleanup preview");
    click("savedQueueMenuButton");click("deduplicateQueueAction");click("confirmCleanup");
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&ids()==QStringList{"History","Current","Upcoming","External"},"confirming the preview removes only its duplicate songs");
    click("savedQueueMenuButton");click("clearUpcomingAction");click("confirmCleanup");
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&ids()==QStringList{"History","Current"},"clear upcoming works through its preview and confirmation");
    click("savedQueueMenuButton");click("deduplicateQueueAction");
    check(!find(window->contentItem(),"confirmCleanup")->isEnabled(),"empty cleanup preview disables Remove");click("cancelCleanup");
    auto durationTrack=[&](QString id,int ms){auto row=queueTrack(id);auto data=row["track"].toObject();auto attr=data["attributes"].toObject();attr["durationInMillis"]=ms;data["attributes"]=attr;row["track"]=data;return row;};
    fixtureQueue={durationTrack("history",600000),durationTrack("current",120000),durationTrack("next",180000)};fixturePosition=1;syncQueue();QTest::qWait(80);
    check(window->property("queueTimeSummary").toString()=="2 songs · 5 min left","queue time excludes playback history and includes the current song");
    window->setProperty("queueQuery","no-match");QTest::qWait(30);check(window->property("queueTimeSummary").toString()=="2 songs · 5 min left","queue filtering does not change listening time");window->setProperty("queueQuery","");
    fixtureQueue.append(durationTrack("unknown",0));syncQueue();
    check(window->property("queueTimeSummary").toString()=="3 songs · ≥ 5 min left","unknown durations are shown as a lower bound");
    resetBatch();window->setProperty("queueOpen",true);QTest::qWait(200);
    auto selectQueue=[&](int index,Qt::KeyboardModifiers modifiers){auto *row=find(window->contentItem(),"queueRow"+QString::number(index));if(row)QTest::mouseClick(window,Qt::LeftButton,modifiers,row->mapToScene(QPointF(130,row->height()/2)).toPoint());QTest::qWait(80);};
    const int queueSelectionPlays=plays;
    selectQueue(2,Qt::ControlModifier);selectQueue(4,Qt::ShiftModifier);
    check(window->property("queueSelectionCount").toInt()==3&&plays==queueSelectionPlays,"Ctrl and Shift clicks select upcoming queue ranges without playback");
    auto *selectionBar=find(window->contentItem(),"queueSelectionBar");
    check(selectionBar&&selectionBar->isVisible(),"queue selection exposes its compact action bar");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/18-queue-selection.png");
    selectQueue(1,Qt::ControlModifier);check(window->property("queueSelectionCount").toInt()==3,"current queue song cannot join the selection");
    QTest::keyClick(window,Qt::Key_A,Qt::ControlModifier);QTest::qWait(100);
    check(window->property("queueSelectionCount").toInt()==5,"Ctrl+A selects upcoming queue songs only");
    QTest::keyClick(window,Qt::Key_Space);QTest::qWait(100);
    check(window->property("queueSelectionCount").toInt()==5&&plays==queueSelectionPlays,"Space on the current queue song neither selects it nor starts playback");
    click("queueSelectionActions");click("queueBatchRemove");
    const int previewBatchWrites=deleteWrites;
    check(cleanupPopup->property("visible").toBool()&&cleanupPopup->property("preview").toMap()["indices"].toList().size()==5,"batch removal shows a preview of precisely the selected tracks");
    click("cancelCleanup");check(deleteWrites==previewBatchWrites,"cancelling selected removal sends no deletion");
    QTest::keyClick(window,Qt::Key_Escape);QTest::qWait(100);check(window->property("queueSelectionCount").toInt()==0&&window->property("queueOpen").toBool(),"Escape clears queue selection before closing the panel");
    selectQueue(3,Qt::ControlModifier);QTest::keyClick(window,Qt::Key_Space);QTest::qWait(100);
    check(window->property("queueSelectionCount").toInt()==0&&plays==queueSelectionPlays,"Space toggles the focused upcoming track without triggering global playback");
    selectQueue(3,Qt::ControlModifier);fixtureQueue.append(queueTrack("external"));syncQueue();QTest::qWait(100);
    check(window->property("queueSelectionCount").toInt()==0,"an external queue revision clears stale UI selection");
    window->setProperty("queueQuery","external");QMetaObject::invokeMethod(window,"selectUpcomingQueue");QTest::qWait(80);
    check(window->property("queueSelectionCount").toInt()==1,"filtered Select all includes only visible upcoming songs");
    window->setProperty("queueQuery","");resetBatch();QTest::qWait(150);selectQueue(3,Qt::ControlModifier);selectQueue(4,Qt::ControlModifier);
    click("queueSelectionActions");
    auto *batchCopy=find(window->contentItem(),"queueCopyLink");check(!batchCopy||!batchCopy->isVisible(),"batch actions do not offer an ambiguous single-song Copy link");
    click("queueBatchNext");check(batchIdle()&&ids()==QStringList{"h","now","b","c","a","d","e"}&&window->property("queueSelectionCount").toInt()==0,"selection menu moves the chosen songs next and clears the completed selection");
    selectQueue(2,Qt::ControlModifier);click("queueActions4");
    check(window->property("queueSelectionCount").toInt()==0,"opening an unselected row menu clears the previous selection");
    auto *queueEditPopup=window->findChild<QObject *>("queueEditMenu");if(queueEditPopup)QMetaObject::invokeMethod(queueEditPopup,"close");QTest::qWait(150);
    check(browser.saveQueue("Late night"),"picker fixture saves an isolated queue");
    // Open the picker using isolated local storage, with no live library writes.
    QVariant pickerSongs=QVariantList{track("picker-new")};QVariant pickerService=QVariant::fromValue(&browser);
    QMetaObject::invokeMethod(window,"openSavedQueuePicker",Q_ARG(QVariant,pickerSongs),Q_ARG(QVariant,pickerService));QTest::qWait(200);
    auto *appendPicker=window->findChild<QObject *>("savedQueuePicker");
    check(appendPicker&&appendPicker->property("visible").toBool()&&find(window->contentItem(),"savedQueueChoice0"),"saved queue picker lazily displays existing snapshots");
    const auto pickerTarget=browser.savedQueueChoices()["rows"].toList().first().toMap();const int pickerBefore=browser.savedTracks(pickerTarget["id"].toString()).size();
    auto *pickerClose=find(window->contentItem(),"closeSavedQueuePicker");
    check(pickerClose&&wait([&]{return pickerClose->hasActiveFocus();}),"saved queue picker initially focuses its first interactive control");
    auto *pickerCancel=find(window->contentItem(),"cancelSavedQueuePicker");
    auto cancelPixels=[&] {
        const auto image=window->grabWindow();const auto dpr=image.devicePixelRatio();
        const auto point=pickerCancel->mapToScene(QPointF());
        return image.copy(QRect(qRound(point.x()*dpr),qRound(point.y()*dpr),qRound(pickerCancel->width()*dpr),qRound(pickerCancel->height()*dpr)));
    };
    click("savedQueueChoice0");const auto normalCancelPixels=cancelPixels();
    check(browser.renameSavedQueue(pickerTarget,"Refreshed queue"),"picker fixture changes the saved queue after selection");
    click("appendSavedQueueConfirm");
    auto *pickerRefresh=find(window->contentItem(),"refreshSavedQueueChoices");
    auto *pickerConfirm=find(window->contentItem(),"appendSavedQueueConfirm");
    check(appendPicker->property("visible").toBool()&&!appendPicker->property("error").toString().isEmpty()&&!pickerConfirm->isEnabled(),"stale picker selection shows a recoverable error without appending songs");
    check(pickerCancel&&pickerCancel->isVisible()&&pickerCancel->isEnabled()&&pickerRefresh&&pickerRefresh->isVisible(),"saved queue errors preserve Cancel alongside the inline Refresh action");
    check(pickerRefresh&&pickerCancel&&pickerRefresh->y()+pickerRefresh->height()<=pickerCancel->y(),"inline recovery stays separate from the dialog footer");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/21-saved-queue-recovery.png");
    bool cancelHasInk=false;
    if(!normalCancelPixels.isNull())for(int y=0;y<normalCancelPixels.height();++y)for(int x=0;x<normalCancelPixels.width();++x)
        cancelHasInk|=normalCancelPixels.pixel(x,y)!=normalCancelPixels.pixel(0,0);
    check(cancelHasInk&&cancelPixels()==normalCancelPixels,"Cancel remains visibly rendered when an inline error appears");
    auto *pickerList=find(window->contentItem(),"savedQueueChoices");
    pickerList->forceActiveFocus(Qt::TabFocusReason);testKeyClick(window,Qt::Key_Enter);
    check(appendPicker->property("error").toString().isEmpty()&&pickerConfirm->isEnabled(),"keypad Enter selects a saved queue and clears the previous error like a click");
    click("appendSavedQueueConfirm");
    check(!appendPicker->property("error").toString().isEmpty()&&browser.savedTracks(pickerTarget["id"].toString()).size()==pickerBefore,"keyboard reselection still revalidates stale data before writing");
    click("refreshSavedQueueChoices");
    check(appendPicker->property("error").toString().isEmpty()&&!pickerConfirm->isEnabled()&&appendPicker->property("rows").toList().first().toMap()["title"]=="Refreshed queue","Refresh reads current snapshots and requires a fresh selection");
    pickerList->setProperty("currentIndex",0);pickerList->forceActiveFocus(Qt::TabFocusReason);testKeyClick(window,Qt::Key_Space);
    check(pickerConfirm->isEnabled(),"Space selects the refreshed saved queue with the keyboard");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/21-saved-queue-focus.png");
    click("cancelSavedQueuePicker");
    check(!appendPicker->property("visible").toBool()&&browser.savedTracks(pickerTarget["id"].toString()).size()==pickerBefore,"Cancel closes the recovered picker without saving changes");
    const auto refreshedTarget=browser.savedQueueChoices()["rows"].toList().first().toMap();
    check(browser.renameSavedQueue(refreshedTarget,pickerTarget["title"].toString()),"picker fixture restores its original title");
    QMetaObject::invokeMethod(window,"openSavedQueuePicker",Q_ARG(QVariant,pickerSongs),Q_ARG(QVariant,pickerService));QTest::qWait(200);
    click("savedQueueChoice0");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/18-saved-queue-picker.png");
    click("skipSavedDuplicates");check(!appendPicker->property("skipDuplicates").toBool(),"saved picker exposes an optional duplicate filter");
    click("appendSavedQueueConfirm");check(!appendPicker->property("visible").toBool()&&browser.savedTracks(pickerTarget["id"].toString()).size()==pickerBefore+1,"picker confirmation appends to the selected local snapshot");
    auto *savedUndoButton=find(window->contentItem(),"undoQueueButton");
    check(savedUndoButton&&savedUndoButton->isVisible()&&browser.canUndoSavedQueue(),"saved edits expose Undo in the existing notice");
    click("undoQueueButton");check(browser.savedTracks(pickerTarget["id"].toString()).size()==pickerBefore&&!browser.canUndoSavedQueue(),"notice Undo restores the saved snapshot without changing Cider");
    fixtureQueue={queueTrack("history"),queueTrack("current"),queueTrack("jump-target"),queueTrack("last")};fixturePosition=1;syncQueue();window->setProperty("queueOpen",false);window->setProperty("libraryOpen",false);QTest::qWait(150);
    const int jumpPlays=plays,jumpWrites=queueWrites+deleteWrites,jumpStart=jumps;
    window->contentItem()->forceActiveFocus();testKeyClick(window,Qt::Key_K,Qt::ControlModifier);QTest::qWait(250);
    auto *jump=window->findChild<QObject *>("quickJump");
    check(jump&&jump->property("visible").toBool()&&wait([&]{return !jump->property("waitingForQueue").toBool();}),"Ctrl+K opens Quick jump and refreshes the queue once");
    auto *jumpSearch=find(window->contentItem(),"quickJumpSearch");
    check(jumpSearch&&jumpSearch->hasActiveFocus(),"Quick jump focuses its search field");
    if(jumpSearch)for(char c:QByteArray("jump-target"))QTest::keyClick(window,c);
    QTest::qWait(100);auto *jumpList=find(window->contentItem(),"quickJumpResults");
    check(jumpList&&jumpList->property("count").toInt()==1,"Quick jump filters the current queue through typed text");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/19-quick-jump.png");
    QTest::keyClick(window,Qt::Key_Return);QTest::qWait(250);
    check(wait([&]{return jumps==jumpStart+1&&!cider.controlBusy()&&!cider.queueBusy();})&&!jump->property("visible").toBool()&&window->property("queueOpen").toBool()&&find(window->contentItem(),"trackList")->property("keyboardIndex").toInt()==2&&jumpedIndex==2&&fixturePosition==2&&cider.currentIndex()==2,"Quick jump Enter plays the matching provider index and confirms the queue highlight");
    QMetaObject::invokeMethod(window,"openQuickJump");wait([&]{return !jump->property("waitingForQueue").toBool();});jump->setProperty("query","jump-target");QTest::qWait(100);
    fixtureQueue.append(queueTrack("external"));syncQueue();QTest::keyClick(window,Qt::Key_Return);QTest::qWait(100);
    check(jump->property("visible").toBool()&&!jump->property("notice").toString().isEmpty()&&plays==jumpPlays&&jumps==jumpStart+1,"stale Quick jump queue results require a refresh instead of activating a different row");
    click("quickJumpRefresh");wait([&]{return !jump->property("waitingForQueue").toBool();});
    check(jump->property("query").toString()=="jump-target"&&jump->property("notice").toString().isEmpty(),"refreshing a stale Quick jump preserves the search and reloads confirmed results");
    QMetaObject::invokeMethod(jump,"close");QTest::qWait(200);
    QMetaObject::invokeMethod(window,"openQuickJump");wait([&]{return !jump->property("waitingForQueue").toBool();});jump->setProperty("query","Late night");QTest::qWait(100);QTest::keyClick(window,Qt::Key_Return);QTest::qWait(250);
    check(!jump->property("visible").toBool()&&browser.collection()["id"]==pickerTarget["id"]&&window->property("libraryOpen").toBool(),"Quick jump opens a saved queue without replacing playback");
    check(queueWrites+deleteWrites==jumpWrites&&plays==jumpPlays&&jumps==jumpStart+1,"opening a saved Quick jump result never replaces the queue or starts another song");
    window->setProperty("libraryOpen",false);browser.setActive(false);
    auto showJump=[&](const QString &term){QMetaObject::invokeMethod(window,"openQuickJump");wait([&]{return !jump->property("waitingForQueue").toBool();});jump->setProperty("query",term);QTest::qWait(180);};
    showJump("last");click("quickJumpRow0");
    check(wait([&]{return jumps==jumpStart+2&&!cider.controlBusy()&&!cider.queueBusy();})&&fixturePosition==3&&cider.currentIndex()==3,"clicking a Quick jump result plays its exact queue index");
    showJump("no-match-at-all");QTest::keyClick(window,Qt::Key_Return);QTest::qWait(100);
    check(jump->property("visible").toBool()&&jumps==jumpStart+2,"Enter with no Quick jump matches does not change playback");
    QTest::keyClick(window,Qt::Key_Escape);QTest::qWait(200);
    check(!jump->property("visible").toBool()&&jumps==jumpStart+2,"Escape dismisses Quick jump without playing");
    showJump("current");QSignalSpy jumpFeedback(&cider,&Cider::apiFeedback);rejectJump=true;jumpDelay=500;click("quickJumpRow0");cider.select(0);
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&jumps==jumpStart+3&&fixturePosition==3&&!jumpFeedback.isEmpty()&&jumpFeedback.last()[1].toBool(),"a rejected jump reports an error, preserves playback and blocks overlapping requests");
    rejectJump=false;jumpDelay=0;showJump("current");click("quickJumpRow0");
    check(wait([&]{return jumps==jumpStart+4&&!cider.controlBusy()&&!cider.queueBusy();})&&fixturePosition==1,"Quick jump can retry successfully after a rejected playback request");
    player->setMiniMode(true);QTest::qWait(250);showJump("jump-target");click("quickJumpRow0");
    check(wait([&]{return jumps==jumpStart+5&&!cider.controlBusy()&&!cider.queueBusy();})&&fixturePosition==2&&player->miniMode()&&!jump->property("visible").toBool(),"Mini Quick jump plays the selected track while preserving Mini mode");
    rejectJump=true;showJump("last");click("quickJumpRow0");
    auto *jumpNotice=find(window->contentItem(),"actionNotice");
    check(wait([&]{return !cider.controlBusy()&&!cider.queueBusy();})&&fixturePosition==2&&jumpNotice&&jumpNotice->isVisible()&&jumpNotice->property("failed").toBool()&&jumpNotice->x()>=0&&jumpNotice->x()+jumpNotice->width()<=window->width(),"Mini jump failures remain visible inside the compact player without changing playback");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/19-mini-jump-error.png");
    auto *dismissNotice=find(window->contentItem(),"dismissActionNotice");
    check(dismissNotice&&jumpNotice&&jumpNotice->contains(dismissNotice->mapToItem(jumpNotice,QPointF(0,0)))&&jumpNotice->contains(dismissNotice->mapToItem(jumpNotice,QPointF(dismissNotice->width()-1,dismissNotice->height()-1))),"Mini feedback dismiss target stays entirely inside its visible surface");
    click("dismissActionNotice");
    check(!jumpNotice->isVisible()&&player->miniMode()&&fixturePosition==2,"Mini feedback can be dismissed without changing playback or leaving Mini mode");

    rejectJump=false;fixturePosition=1;syncQueue();
    window->setProperty("libraryOpen",false);browser.setActive(false);player->setMiniMode(true);QTest::qWait(250);
    auto *miniNext=find(window->contentItem(),"miniNext");const bool nextEnabled=miniNext->isEnabled();QQmlProperty::write(miniNext,"enabled",true);
    const auto miniPoint=miniNext->mapToScene(QPointF(20,20)).toPoint();QTest::mouseMove(window,QPoint(2,2));QTest::qWait(80);const int peekReads=queueReads;
    QTest::mouseMove(window,miniPoint);QTest::qWait(800);auto *peek=miniNext->findChild<QObject *>("nextTrackTip");
    check(peek&&wait([&]{return peek->property("visible").toBool()&&!cider.queueBusy();})&&peek->property("summary").toString()=="jump-target","Mini hover previews the confirmed next song");
    check(queueReads==peekReads+1,"Mini preview makes a single on-demand queue request");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/19-mini-up-next.png");
    const int quietPeek=queueReads;QTest::qWait(1100);check(queueReads==quietPeek,"holding the Mini preview open does not poll the queue");
    fixturePosition=2;QMetaObject::invokeMethod(&cider,"trackChanged");
    check(wait([&]{return !cider.queueBusy();})&&peek->property("summary").toString()=="last"&&queueReads==quietPeek+1,"Mini preview refreshes once when the current song changes while hovered");
    QTest::mouseMove(window,QPoint(2,2));window->contentItem()->forceActiveFocus();QTest::qWait(250);
    check(!peek->property("visible").toBool(),"Mini preview dismisses when the pointer leaves");
    failQueueRead=true;QTest::mouseMove(window,miniPoint);QTest::qWait(800);wait([&]{return !cider.queueBusy();});
    check(peek->property("summary").toString()=="Next track unavailable","Mini preview never presents stale artwork as a confirmed next song after a failed read");
    QTest::mouseMove(window,QPoint(2,2));QTest::qWait(200);failQueueRead=false;syncQueue();
    QMetaObject::invokeMethod(window,"openQuickJump");QTest::qWait(200);
    check(jump->property("visible").toBool()&&jump->property("width").toDouble()<=window->width()&&jump->property("height").toDouble()<=window->height(),"Quick jump fits inside Mini mode");
    QMetaObject::invokeMethod(jump,"close");QTest::qWait(200);check(player->miniMode(),"dismissing Quick jump preserves Mini mode");
    queueDelay=900;QMetaObject::invokeMethod(window,"openQuickJump");jump->setProperty("query","no-matching-title");QTest::qWait(100);
    auto *jumpEmpty=find(window->contentItem(),"quickJumpEmptyState");
    check(jumpEmpty&&jumpEmpty->isVisible()&&jump->property("waitingForQueue").toBool()&&jumpEmpty->property("text").toString()=="Loading queue…","Quick jump distinguishes a pending queue read from an empty result");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/22-mini-jump-loading.png");
    check(wait([&]{return !jump->property("waitingForQueue").toBool();})&&jumpEmpty->property("text").toString()=="No matches","Quick jump reports no matches only after the queue read completes");
    QMetaObject::invokeMethod(jump,"close");QTest::qWait(200);queueDelay=0;
    QQmlProperty::write(miniNext,"enabled",nextEnabled);player->setMiniMode(false);browser.setActive(true);window->setProperty("queueOpen",true);QTest::qWait(200);
    const QString literalTitle="<b>Night & Day</b> — a very long live recording\nSecond line";
    const QString literalArtist="<i>Spun Sound Lab</i>\nGuest artist";
    auto unusualTrack=resource("literal-title","songs",literalTitle);auto unusualAttributes=unusualTrack["attributes"].toObject();unusualAttributes["artistName"]=literalArtist;unusualTrack["attributes"]=unusualAttributes;
    fixtureQueue={QJsonObject{{"track",unusualTrack}}};fixturePosition=0;syncQueue();QTest::qWait(200);
    auto *literalLabel=find(window->contentItem(),"queueArtwork0Title");
    auto *literalArtistButton=find(window->contentItem(),"queueArtwork0Artist");
    auto *literalArtistLabel=literalArtistButton?literalArtistButton->property("contentItem").value<QQuickItem *>():nullptr;
    check(literalLabel&&literalLabel->property("text").toString()==literalTitle&&literalLabel->property("textFormat").toInt()==0,"queue titles retain literal markup characters instead of changing typography");
    check(literalLabel&&literalArtistLabel&&literalLabel->property("lineCount").toInt()==1&&literalArtistLabel->property("lineCount").toInt()==1&&literalLabel->height()==20,"multiline metadata stays within the title and artist lines of a fixed-height queue row");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/22-literal-queue-title.png");
    player->setMiniMode(true);QTest::qWait(250);showJump("recording");
    auto *miniLiteralTitle=find(window->contentItem(),"quickJumpTitle0");
    check(miniLiteralTitle&&miniLiteralTitle->property("text").toString()==literalTitle&&miniLiteralTitle->property("textFormat").toInt()==0&&miniLiteralTitle->property("lineCount").toInt()==1&&miniLiteralTitle->property("truncated").toBool(),"Mini Quick jump truncates long literal titles without overlapping their subtitles");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/22-mini-long-title.png");
    auto *spaceJumpList=find(window->contentItem(),"quickJumpResults");focusTestWindow(window);spaceJumpList->setProperty("currentIndex",0);spaceJumpList->forceActiveFocus(Qt::TabFocusReason);
    const int beforeSpaceJump=jumps;testKeyClick(window,Qt::Key_Space);
    const bool spaceIdle=wait([&]{return !cider.controlBusy()&&!cider.queueBusy()&&!jump->property("visible").toBool();});
    check(spaceIdle&&jumps==beforeSpaceJump+1&&fixturePosition==0&&!jump->property("visible").toBool()&&player->miniMode(),"Space activates the focused Quick jump result without leaving Mini mode");
    player->setMiniMode(false);window->setProperty("queueOpen",true);QTest::qWait(200);

    fixtureQueue={durationTrack("unknown",0)};fixturePosition=-1;syncQueue();
    check(window->property("queueTimeSummary").toString()=="1 song · Duration incomplete","an entirely unknown duration is never shown as zero listening time");
    fixtureQueue={durationTrack("long",3720000)};fixturePosition=-1;syncQueue();
    check(window->property("queueTimeSummary").toString()=="1 song · 1 h 2 min left","long queue durations format as hours and minutes");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/15-queue-time.png");
    fixtureQueue={};fixturePosition=-1;syncQueue();check(window->property("queueTimeSummary").toString()=="0 songs","empty queue does not invent a remaining duration");
    // Current/upcoming identities are indexed once for all rendered browse rows.
    window->setProperty("libraryOpen",true);browser.setSection("search");browser.setKind("songs");browser.setQuery("queued match");wait([&]{return !browser.busy();});QTest::qWait(200);
    fixtureQueue={queueTrack("history"),queueTrack("123"),queueTrack("next")};fixturePosition=1;syncQueue();QTest::qWait(120);
    auto *queuedMark=find(window->contentItem(),"alreadyQueued0");
    check(queuedMark&&queuedMark->isVisible(),"current song is marked Already queued in search results");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/17-already-queued.png");
    fixturePosition=2;syncQueue();QTest::qWait(100);check(!queuedMark->isVisible(),"history-only matches lose the Already queued marker");
    auto queuedLibrary=resource("i.same","library-songs","Different display name");auto queuedAttrs=queuedLibrary["attributes"].toObject();queuedAttrs["playParams"]=QJsonObject{{"id","i.same"},{"catalogId","123"}};queuedLibrary["attributes"]=queuedAttrs;
    fixtureQueue={queueTrack("current"),QJsonObject{{"track",queuedLibrary}}};fixturePosition=0;syncQueue();QTest::qWait(100);
    check(queuedMark->isVisible(),"library and catalog IDs share the queued marker without title matching");
    click("libraryActions0");const int beforeRepeatEnqueue=edits;click("addQueueAction");wait([&]{return !actions.busy();});
    check(edits==beforeRepeatEnqueue+1,"Already queued does not block intentional repeated additions");
    failQueueRead=true;syncQueue();QTest::qWait(100);check(!queuedMark->isVisible(),"unavailable queue status hides stale queued markers");failQueueRead=false;syncQueue();
    window->setProperty("libraryOpen",false);browser.setActive(false);cider.setQueueVisible(false);
    const int beforeObserverRead=queueReads;cider.setLibraryVisible(true);wait([&]{return !cider.queueBusy();});
    check(queueReads==beforeObserverRead+1,"opening browsing refreshes queue membership without opening Queue");
    cider.setLibraryVisible(false);const int hiddenReads=queueReads;QTest::qWait(150);
    check(queueReads==hiddenReads,"closing both panels makes no new membership request");browser.setActive(true);
    const int discoveryPlays=plays;browser.setSection("for-you");wait([&]{return !browser.busy();});
    check(browser.error().isEmpty()&&browser.items().size()==3&&browser.hasMore(),"For You flattens curated album, playlist and station recommendations");
    check(plays==discoveryPlays&&browser.items().first().toMap()["recommendation"]=="Made for you","recommendations preserve their context without starting playback");
    check(browser.items().first().toMap()["path"]=="/v1/catalog/ca/albums/recommended","recommendation links retain the returned storefront and safe catalog route");
    browser.open(0);wait([&]{return !browser.busy();});check(browser.collection()["type"]=="albums","recommended albums open inside the existing browser");browser.back();
    check(browser.items().size()==3&&browser.hasMore(),"Back restores the recommendation feed and pending content pages");
    failRecommendationPage=true;browser.more();wait([&]{return !browser.busy();});check(browser.items().size()==3&&!browser.error().isEmpty()&&browser.hasMore(),"failed recommendation pagination preserves prior results and retry state");
    failRecommendationPage=false;browser.more();wait([&]{return !browser.busy();});check(browser.items().size()==4&&browser.error().isEmpty(),"retry reads the failed recommendation contents page without duplicating cards");
    browser.more();wait([&]{return !browser.busy();});check(browser.items().size()==5&&!browser.hasMore(),"For You loads both group contents and top-level recommendation pages");
    const int cachedRecommendations=recommendationReads;browser.setActive(false);browser.setActive(true);QTest::qWait(50);check(recommendationReads==cachedRecommendations,"reopening a loaded recommendation feed performs no extra fetch");
    browser.setQuery("later");check(browser.items().size()==2,"recommendations can be searched locally");browser.setQuery("");
    hostileRecommendations=true;browser.reload();wait([&]{return !browser.busy();});const int beforeHostile=recommendationReads;browser.more();QTest::qWait(100);
    check(recommendationReads==beforeHostile&&!browser.error().isEmpty()&&browser.items().size()==3,"hostile recommendation pagination is rejected before sending credentials");hostileRecommendations=false;
    emptyRecommendations=true;browser.reload();wait([&]{return !browser.busy();});check(browser.error().isEmpty()&&browser.items().isEmpty()&&!browser.hasMore(),"an empty For You feed is a normal empty state");emptyRecommendations=false;
    malformedRecommendations=true;browser.reload();wait([&]{return !browser.busy();});check(!browser.error().isEmpty(),"malformed recommendation responses are reported explicitly");malformedRecommendations=false;
    discoveryStatus=403;browser.reload();wait([&]{return !browser.busy();});check(browser.needsConnection(),"For You permission failures offer connection recovery");check(cider.queueReady()&&!cider.needsToken(),"a denied recommendation capability leaves working queue access intact");discoveryStatus=200;browser.reload();wait([&]{return !browser.busy();});
    discoveryDelay=450;browser.reload();QTest::qWait(25);browser.setSection("songs");wait([&]{return !browser.busy();});QTest::qWait(500);check(browser.section()=="songs"&&browser.items().first().toMap()["type"]=="library-songs","leaving For You discards late personalized responses");discoveryDelay=0;
    browser.setSection("search");browser.setKind("artists");browser.setQuery("Neon Atlas");wait([&]{return !browser.busy();});browser.open(0);wait([&]{return !browser.busy();});
    for(const auto &filter:QStringList{"full-albums","singles","live-albums"}) {
        browser.setDiscography(filter);wait([&]{return !browser.busy();});check(browser.error().isEmpty()&&lastBrowsePath.contains("/view/"+filter)&&browser.items().size()==1,"discography filter fetches its explicit artist catalog view");
        browser.more();wait([&]{return !browser.busy();});check(browser.items().size()==2,"discography filters preserve pagination");
    }
    browser.open(0);wait([&]{return !browser.busy();});browser.back();check(browser.discography()=="live-albums"&&browser.items().size()==2,"Back from an album restores the selected discography filter and rows");
    const auto beforeInvalidFilter=lastBrowsePath;browser.setDiscography("../bad");check(lastBrowsePath==beforeInvalidFilter&&browser.discography()=="live-albums","unsupported discography filters do not issue requests");
    emptyDiscography=true;browser.setDiscography("singles");wait([&]{return !browser.busy();});check(browser.error().isEmpty()&&browser.items().isEmpty(),"artists with no releases in a category show an empty list");emptyDiscography=false;
    rejectDiscography=true;browser.setDiscography("full-albums");wait([&]{return !browser.busy();});check(!browser.error().isEmpty(),"unavailable discography views report failure rather than showing another category");rejectDiscography=false;
    discographyDelay=450;browser.setDiscography("singles");QTest::qWait(25);browser.setArtistView("songs");wait([&]{return !browser.busy();});QTest::qWait(500);check(browser.artistView()=="songs"&&browser.items().first().toMap()["type"]=="songs","a late discography response cannot overwrite Top songs");discographyDelay=0;
    window->setProperty("libraryOpen",true);browser.setSection("albums");browser.setSection("search");browser.setQuery("");QTest::qWait(250);click("forYouButton");wait([&]{return !browser.busy();});
    check(browser.section()=="for-you"&&browser.items().size()==3,"For You landing-page button opens personalized browsing");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/20-for-you.png");
    click("libraryArtwork0Artist");wait([&]{return !browser.busy();});
    check(browser.collection().value("type")=="albums","clicking recommendation context opens its album rather than an artist search");
    browser.back();
    browser.setSection("search");browser.setKind("artists");browser.setQuery("Neon Atlas");wait([&]{return !browser.busy();});browser.open(0);wait([&]{return !browser.busy();});QTest::qWait(150);
    click("discographyFilter");auto *filterMenu=panel->findChild<QObject *>("discographyMenu");check(filterMenu&&filterMenu->property("visible").toBool()&&window->property("menuOpen").toBool(),"discography menu participates in the shared modal shortcut guard");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/21-discography-menu.png");
    click("discography_singles");wait([&]{return !browser.busy();});check(browser.discography()=="singles"&&!filterMenu->property("visible").toBool(),"choosing a discography filter updates the list and closes its menu");
    click("discographyFilter");QTest::keyClick(window,Qt::Key_Escape);QTest::qWait(180);check(!filterMenu->property("visible").toBool()&&browser.collection()["type"]=="artists","Escape dismisses the filter without leaving the artist");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/22-discography-filtered.png");
    browser.setSection("albums");wait([&]{return !browser.busy();});

    // New collection action uses canonical occurrence indices, not filtered list positions.
    fixtureQueue={queueTrack("a")};fixturePosition=0;syncQueue();
    tailFixture=true;pagedTracks=true;browser.setSection("albums");wait([&]{return !browser.busy();});browser.open(0);wait([&]{return !browser.busy();});
    QSignalSpy tails(&browser,&Library::tailReady);
    const auto secondOccurrence=browser.items().value(1).toMap();
    browser.queueFromHere(secondOccurrence);
    check(wait([&]{return !browser.preparingTail()&&!browser.busy()&&!actions.busy()&&!cider.controlBusy()&&!cider.queueBusy();})&&tails.size()==1,"Queue from here fetches unloaded collection pages before enqueueing");
    if(tails.size()) {const auto rows=tails.last().first().toList();check(rows.size()==3&&rows[0].toMap()["id"]=="repeat"&&rows[1].toMap()["id"]=="middle"&&rows[2].toMap()["id"]=="i.hidden","Queue from here keeps the chosen duplicate occurrence and album order");}
    browser.setCollectionQuery("middle");wait([&]{return !browser.busy();});browser.queueFromHere(browser.items().first().toMap());
    check(wait([&]{return !actions.busy()&&!cider.controlBusy()&&!cider.queueBusy()&&!browser.preparingTail();})&&tails.size()==2&&tails.last().first().toList().size()==2,"Queue from here includes following tracks hidden by the collection filter");
    browser.back();wait([&]{return !browser.busy();});failTracks=true;browser.open(0);wait([&]{return !browser.busy();});browser.queueFromHere(browser.items().first().toMap());
    check(wait([&]{return !browser.busy()&&!browser.preparingTail();})&&tails.size()==2,"a later collection page failure queues nothing");failTracks=false;
    browser.back();browser.open(0);wait([&]{return !browser.busy();});slowTracks=true;browser.queueFromHere(browser.items().first().toMap());browser.back();QTest::qWait(500);
    check(!browser.preparingTail()&&tails.size()==2,"leaving the collection cancels preparation without queue writes");slowTracks=false;
    tailFixture=false;pagedTracks=false;browser.open(0);wait([&]{return !browser.busy();});browser.queueFromHere(browser.items().first().toMap());
    check(!browser.preparingTail()&&tails.size()==2,"unavailable tracks reject the whole range rather than silently skipping songs");
    browser.back();tailFixture=true;browser.open(0);wait([&]{return !browser.busy();});window->setProperty("libraryOpen",true);QTest::qWait(200);
    click("libraryActions1");auto *tailMenu=panel->findChild<QObject *>("browserTrackMenu");auto *tailAction=tailMenu?tailMenu->findChild<QObject *>("queueFromHereAction"):nullptr;
    check(tailAction&&tailAction->property("visible").toBool()&&tailAction->property("enabled").toBool(),"collection song menu exposes Queue from here");click("queueFromHereAction");
    check(wait([&]{return !actions.busy()&&!cider.controlBusy()&&!cider.queueBusy()&&!browser.preparingTail();})&&tails.size()==3,"Queue from here menu activates the complete workflow");
    tailFixture=false;browser.back();

    listeningFixture=true;fixtureQueue={queueTrack("a"),queueTrack("b"),queueTrack("c")};fixturePosition=1;syncQueue();
    Listening listeningState(&cider);QSignalSpy listeningNotices(&listeningState,&Listening::feedback);
    const auto originalListening=window->property("listeningService");window->setProperty("listeningService",QVariant::fromValue(&listeningState));
    const int disabledReads=snapshotReads;listeningState.checkpoint();QTest::qWait(30);
    check(snapshotReads==disabledReads&&!listeningState.rememberSession(),"session recovery is opt-in and disabled checkpoints make no API requests");
    if(songMenu) {songMenu->setProperty("x",230);songMenu->setProperty("y",160);QMetaObject::invokeMethod(songMenu,"open");QTest::qWait(200);}
    click("bookmarkMomentAction");check(wait([&]{return !listeningState.busy();})&&listeningState.bookmarks().size()==1,"current-song menu bookmarks one atomic song and position snapshot");
    const auto bookmark=listeningState.bookmarks().value(0).toMap();const auto bookmarkKey=bookmark["key"].toString();
    check(bookmark["position"].toLongLong()==12345&&bookmark["title"]=="Bookmarked <song>","bookmark preserves millisecond position and literal title");
    listeningState.addBookmark();wait([&]{return !listeningState.busy();});check(listeningState.bookmarks().size()==1,"repeated bookmark action deduplicates the same moment");
    {Listening restored(&cider);check(restored.bookmarks()==listeningState.bookmarks(),"bookmarks survive a fresh backend instance");}
    QFile listeningFile(temp+"/listening.json");check(listeningFile.exists()&&!(listeningFile.permissions()&(QFileDevice::ReadGroup|QFileDevice::ReadOther)),"listening references are stored with private permissions");
    listeningState.playBookmark(bookmarkKey);check(wait([&]{return !listeningState.busy()&&!cider.controlBusy()&&!cider.queueBusy();})&&listeningState.error().isEmpty()&&snapshotPosition==12.345&&seekWrites==1,"bookmark playback verifies the song, seeks, and reads back its position");
    wrongSong=true;const int beforeWrongSeek=seekWrites;listeningState.playBookmark(bookmarkKey);
    check(listeningState.busy(),"bookmark mismatch check starts after the previous readback settles");
    check(wait([&]{return !listeningState.busy();})&&!listeningState.error().isEmpty()&&seekWrites==beforeWrongSeek,"bookmark never seeks when Cider reports a different song");wrongSong=false;
    ignoreSeek=true;listeningState.playBookmark(bookmarkKey);check(listeningState.busy(),"bookmark seek-error check starts a fresh playback operation");check(wait([&]{return !listeningState.busy()&&!cider.controlBusy()&&!cider.queueBusy();})&&!listeningState.error().isEmpty(),"an acknowledged but ineffective seek is reported honestly");ignoreSeek=false;
    snapshotFailure=true;listeningState.addBookmark();check(wait([&]{return !listeningState.busy();})&&listeningState.bookmarks().size()==1,"failed playback snapshot cannot create an invalid bookmark");snapshotFailure=false;
    snapshotId="b";snapshotPosition=12.345;fixtureQueue={queueTrack("a"),queueTrack("b"),queueTrack("c")};fixturePosition=1;syncQueue();
    listeningState.setRememberSession(true);check(wait([&]{return listeningState.session()["trackCount"].toInt()==2;}),"optional checkpoint saves the current and upcoming songs without history");
    check(listeningState.session()["position"].toLongLong()==12345,"session checkpoint preserves exact playback position");
    listeningState.prepareRecovery();wait([&]{return !cider.queueBusy();});const int beforeActiveRestore=queueWrites;
    listeningState.restoreSession();check(wait([&]{return !listeningState.busy();})&&queueWrites==beforeActiveRestore&&!listeningState.error().isEmpty(),"session restore refuses an existing Cider queue without any mutations");
    fixtureQueue={};fixturePosition=-1;syncQueue();listeningState.prepareRecovery();wait([&]{return !cider.queueBusy();});
    changeQueueOnRead=true;listeningState.restoreSession();check(wait([&]{return !listeningState.busy();})&&queueWrites==beforeActiveRestore,"a queue created after confirmation is protected by a fresh read");
    fixtureQueue={};fixturePosition=-1;syncQueue();listeningState.prepareRecovery();wait([&]{return !cider.queueBusy();});rejectEdit=true;
    listeningState.restoreSession();check(wait([&]{return !listeningState.busy()&&!cider.queueBusy();})&&!listeningState.error().isEmpty()&&listeningState.session()["trackCount"].toInt()==2,"failed restoration retains the original checkpoint and does not retry writes");rejectEdit=false;
    fixtureQueue={};fixturePosition=-1;syncQueue();listeningState.prepareRecovery();wait([&]{return !cider.queueBusy();});delayedAppendMs=350;const int writesBeforeDelayedRestore=queueWrites;listeningState.restoreSession();
    check(wait([&]{return !listeningState.busy()&&!cider.queueBusy();})&&listeningState.error().isEmpty()&&ids()==QStringList{"b","c"}&&fixturePosition==0&&snapshotPosition==12.345,"recovery restores an empty queue in order and resumes the saved moment");
    check(queueWrites==writesBeforeDelayedRestore+2,"delayed Cider queue writes are verified without repeating mutations");delayedAppendMs=0;
    fixtureQueue={};fixturePosition=-1;syncQueue();window->setProperty("miniMode",true);QTest::qWait(250);
    QMetaObject::invokeMethod(window,"openQuickJump");QTest::qWait(250);jump->setProperty("query","bookmark");QTest::qWait(150);
    const auto bookmarkResults=jump->property("results").toList();bool foundBookmark=false;
    for(const auto &value:bookmarkResults)if(value.toMap().value("kind")=="bookmark"&&value.toMap().value("key")==bookmarkKey)foundBookmark=true;
    check(foundBookmark,"Quick jump finds listening bookmarks in Mini mode");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/18-mini-bookmarks.png");
    click("quickJumpRow0");check(wait([&]{return !listeningState.busy();})&&window->property("miniMode").toBool(),"Quick jump bookmark playback preserves Mini mode");
    QMetaObject::invokeMethod(window,"showRecovery");QTest::qWait(250);auto *recovery=window->findChild<QObject *>("recoveryPopup");
    check(recovery&&recovery->property("visible").toBool(),"session recovery opens an explicit confirmation dialog");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/19-mini-recovery.png");
    const int beforeCancel=queueWrites;click("recoveryCancel");QTest::qWait(200);check(queueWrites==beforeCancel&&!recovery->property("visible").toBool(),"canceling recovery makes no queue changes");
    QMetaObject::invokeMethod(window,"openQuickJump");QTest::qWait(200);jump->setProperty("query","bookmark");QTest::qWait(120);click("removeBookmark0");QTest::qWait(150);
    check(listeningState.bookmarks().isEmpty(),"Quick jump remove control deletes a bookmark without playing it");QMetaObject::invokeMethod(jump,"close");QTest::qWait(150);
    listeningState.setRememberSession(false);check(!listeningState.rememberSession()&&listeningState.session().isEmpty(),"turning off recovery deletes its local checkpoint");
    window->setProperty("listeningService",originalListening);window->setProperty("miniMode",false);listeningFixture=false;
    window->setProperty("ciderService",originalCider);window->setProperty("libraryOpen",true);transactionQueue=false;
    fixtureQueue={};fixturePosition=-1;
    browser.setNewestFirst(false);browser.setSection("search");QTest::qWait(150);
    QGuiApplication::clipboard()->setText(originalClipboard);
    auto *browseButton=find(window->contentItem(),"addMusicButton");
    const auto center=browseButton->mapToScene(QPointF(18,18)).toPoint();
    auto *buttonContent=browseButton->property("contentItem").value<QQuickItem *>();
    QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,center);QTest::qWait(100);
    check(buttonContent && buttonContent->scale()<1 && browseButton->scale()==1,"button feedback leaves the pointer target fixed");
    QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,center);
    player->setMotion(false);window->setProperty("libraryOpen",true);
    check(panel->opacity()==1,"reduced motion opens the browser immediately");
    QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,center);QTest::qWait(100);
    check(buttonContent && buttonContent->scale()==1 && !browseButton->property("motionEnabled").toBool(),"reduced motion disables icon press scaling");
    QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,center);player->setMotion(motion);
    window->setProperty("libraryOpen",true);QTest::qWait(200);
    QTest::mouseMove(window,center);QTest::qWait(800);
    auto *tooltip=browseButton->findChild<QObject *>("spunToolTip");
    check(tooltip && tooltip->property("visible").toBool(),"shared themed tooltip appears after hovering");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/06-tooltip-dark.png");
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,center);QTest::qWait(100);
    check(!window->property("libraryOpen").toBool(),"visible tooltips do not intercept button clicks");
    window->setProperty("libraryOpen",true);QTest::qWait(200);
    QTest::mouseMove(window,QPoint(2,2));QTest::qWait(100);
    QFile palette(temp+"/config/gtk-4.0/noctalia.css");
    if(palette.open(QIODevice::ReadWrite)) {
        const auto old=palette.readAll();palette.resize(0);
        palette.write("@define-color window_bg_color #f6f0e8;\n@define-color window_fg_color #342d29;\n@define-color accent_bg_color #8b492d;\n@define-color accent_fg_color #ffffff;\n@define-color card_bg_color #e9e0d5;\n");palette.flush();QTest::qWait(400);
        QTest::mouseMove(window,center);QTest::qWait(800);
        if(!captures.isEmpty())window->grabWindow().save(captures+"/06-tooltip-light.png");
        palette.resize(0);palette.seek(0);palette.write(old);palette.close();QTest::qWait(200);
    }
    window->setProperty("libraryOpen",false);panel->setProperty("browser",original);window->setProperty("savedService",originalSavedService);window->setProperty("actionService",originalActions);window->setProperty("useCider",false);browser.setActive(false);
    failures += exerciseCiderEvents(temp);
    std::cout<<"LIBRARY RESULT "<<failures<<" failures"<<std::endl;
    return failures;
}
