#include "librarytest.h"
#include "testinput.h"
#include "library.h"
#include "musicactions.h"
#include "cider.h"
#include "player.h"
#include <QQmlContext>
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

int exerciseLibrary(QQuickWindow *window, const QString &temp, const QString &captures) {
    if(!captures.isEmpty())QDir().mkpath(captures);
    int failures=0, plays=0, reads=0; bool failPage=false, hostilePage=false, rejectPlay=false, rejectRead=false, manyRows=false;
    bool pagedTracks=false, failTracks=false, slowTracks=false, hostileTracks=false, renamedTracks=false; int historyReads=0;
    QString playedType, playedId, playedPath, lastTerm;
    int edits=0, statusReads=0, rating=0, editDelay=0;
    bool crossfade=false, rejectCrossfade=false, malformedCrossfade=false; double fadeSeconds=5; int fadeReads=0, fadeWrites=0; QJsonObject fadePatch;
    bool saved=false, rejectEdit=false, malformedStatus=false;
    QString editPath, editMethod; QJsonObject editBody;
    auto check=[&](bool ok,const char *name) { std::cout<<(ok?"PASS ":"FAIL ")<<name<<std::endl; if(!ok)++failures; };
    auto wait=[&](const std::function<bool()> &condition) { QElapsedTimer t;t.start();while(!condition()&&t.elapsed()<5000)QTest::qWait(25);return condition(); };
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
            if(!request.left(split).contains("test-library-token"))code=403;
            else if(endpoint=="/api/v2/library/now-playing/status") {
                ++statusReads;
                response["data"]=malformedStatus ? QJsonObject{} : QJsonObject{{"inLibrary",saved},{"rating",rating}};
                delay=editDelay;
            } else if(endpoint.startsWith("/api/v2/library/now-playing/") || endpoint.startsWith("/api/v2/queue/add-")) {
                ++edits; editPath=endpoint; editBody=body; editMethod=QString::fromUtf8(request.split(' ').first()); delay=editDelay;
                if(rejectEdit) code=403;
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
                ++reads; const QUrl path(body["path"].toString()); const QUrlQuery query(path); const int offset=query.queryItemValue("offset").toInt();
                QJsonObject data; QJsonArray rows;
                if(path.path()=="/v1/me/storefront") rows.append(QJsonObject{{"id","ca"}});
                else if(path.path().endsWith("/search")) {
                    const QString type=query.queryItemValue("types");lastTerm=query.queryItemValue("term",QUrl::FullyDecoded);
                    if(lastTerm=="slow")delay=600;
                    if(lastTerm!="empty")rows.append(resource("123",type,lastTerm));
                    data={{"results",QJsonObject{{type,QJsonObject{{"data",rows}}}}}};
                } else if(path.path()=="/v1/me/recent/played/tracks") {
                    ++historyReads;
                    rows={resource(offset?"i.older":"recent-new","songs",offset?"Older memory":"Latest song")};
                    if(offset)rows[0]=resource("i.older","library-songs","Older memory");
                    if(!offset)data["next"]="/v1/me/recent/played/tracks?types=songs,library-songs&offset=1";
                } else if(path.path().endsWith("/tracks")) {
                    const auto type=path.path().contains("/me/")?"library-songs":"songs";
                    rows={resource("i.first",type,renamedTracks?"Renamed café":"First Light"),resource("i.second",type,"Second Light"),resource("i.unavailable",type,"Unavailable",false)};
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
                if(!data.contains("results"))data["data"]=rows;
                response["data"]=data;if(rejectRead)code=403;
            } else if(endpoint.startsWith("/api/v2/playback/")) {
                ++plays;playedId=body["id"].toString();playedType=body["type"].toString();playedPath=endpoint;if(rejectPlay)code=422;
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
    actions.setObserving(false);const int readsBefore=statusReads;QTest::qWait(2100);check(statusReads==readsBefore,"closed song menu does not poll Cider");
    malformedStatus=true;actions.setObserving(true);QTest::qWait(100);
    check(!actions.ready()&&!actions.error().isEmpty(),"unreadable status never appears as an unfavorited song");
    malformedStatus=false;actions.setObserving(false);editDelay=300;actions.setObserving(true);QTest::qWait(50);
    actions.setObserving(false);QMetaObject::invokeMethod(&cider,"trackChanged");QTest::qWait(350);
    check(!actions.ready()&&!actions.saved(),"late status cannot restore the previous track state");editDelay=0;

    std::function<QQuickItem*(QQuickItem*,QString)> find=[&](QQuickItem *parent,QString name)->QQuickItem* {if(parent->objectName()==name)return parent;for(auto *child:parent->childItems())if(auto *hit=find(child,name))return hit;return nullptr;};
    auto click=[&](QString name) {focusTestWindow(window);auto *hit=find(window->contentItem(),name);check(hit!=nullptr,"browser control exists");if(hit)QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,hit->mapToScene(QPointF(hit->width()/2,hit->height()/2)).toPoint());QTest::qWait(100);};
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
    window->requestActivate(); QTest::qWait(100);
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
    click("libraryRow3");wait([&]{return !browser.busy();});
    check(!browser.collection().isEmpty()&&plays==beforeUi,"album row opens its track list without autoplay");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/06-library-album.png");
    click("libraryPlayCollection");check(wait([&]{return !browser.starting();})&&plays==beforeUi+1,"collection Play button dispatches playback");
    testKeyClick(window,Qt::Key_Escape);check(browser.collection().isEmpty(),"Escape returns from collection detail");QTest::qWait(100);
    check(qAbs(list->property("contentY").toDouble()-120)<1,"Back restores the library scroll position");
    click("libraryTab_search");click("librarySearchInput");
    auto *search=find(window->contentItem(),"librarySearchInput");
    check(search && search->hasActiveFocus(),"browser search receives keyboard focus");
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
        click("crossfadeToggle");check(wait([&]{return !cider.crossfadeBusy();})&&!crossfade,"crossfade toggle operates from the themed popup");
        auto *duration=find(window->contentItem(),"crossfadeDuration");check(duration&&!duration->isEnabled(),"duration is disabled when crossfade is off");
        click("crossfadeToggle");wait([&]{return !cider.crossfadeBusy();});
        if(duration) { duration->forceActiveFocus();testKeyClick(window,Qt::Key_Right); }
        check(wait([&]{return !cider.crossfadeBusy();})&&fadeSeconds==9,"keyboard adjusts crossfade duration and confirms it");
        testKeyClick(window,Qt::Key_Escape);check(wait([&]{return !fadeMenu->property("visible").toBool();}),"Escape dismisses crossfade settings");
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
    auto *player=qobject_cast<Player *>(QQmlEngine::contextForObject(window)->contextProperty("player").value<QObject *>());
    const bool motion=player->motion();player->setMotion(true);
    window->setProperty("libraryOpen",false);window->setProperty("libraryOpen",true);QTest::qWait(30);
    check(find(window->contentItem(),"libraryPanel")==panel,"reopening retains the browser and its navigation state");
    check(panel->opacity()<1,"browser entrance uses a short opacity transition");QTest::qWait(200);
    check(panel->opacity()==1,"browser entrance settles without lingering animation");
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
    window->setProperty("libraryOpen",false);panel->setProperty("browser",original);window->setProperty("actionService",originalActions);window->setProperty("useCider",false);browser.setActive(false);
    std::cout<<"LIBRARY RESULT "<<failures<<" failures"<<std::endl;
    return failures;
}
