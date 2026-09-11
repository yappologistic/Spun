#include "testcapture.h"
#include "artworktest.h"
#include "player.h"
#include "cider.h"
#include "disc.h"
#include "recorder.h"
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTest>
#include <QBuffer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QPointer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <functional>
#include <iostream>
#ifdef SPUN_WITH_3D
#include "mediageometry.h"
#endif

// Connection availability is fixed; metadata and image requests use Cider's
// production implementation without connecting to a desktop account.
class ArtworkCider : public Cider {
    Q_OBJECT
    Q_PROPERTY(int count READ fixtureCount NOTIFY trackChanged)
    Q_PROPERTY(QString trackKey READ fixtureKey NOTIFY trackChanged)
public:
    ArtworkCider(const QString &connection={},const QUrl &base=QUrl("http://localhost:10767")):Cider(false,connection,base){}
    int fixtureCount() const { return title()=="Cider"?0:1; }
    QString fixtureKey() const { return identity; }
    QString identity;
    void track(const QString &id,const QUrl &url,const QString &album="Remote album") {
        identity=id;
        const QVariantMap changes{{"Metadata",QVariantMap{
            {"mpris:trackid",id},{"xesam:title",id},{"xesam:artist",QStringList{"Remote artist"}},
            {"xesam:album",album},{"mpris:artUrl",url.toString()},{"mpris:length",32000000LL}}}};
        QMetaObject::invokeMethod(this,"propertiesChanged",Qt::DirectConnection,
            Q_ARG(QString,QString("org.mpris.MediaPlayer2.Player")),Q_ARG(QVariantMap,changes),Q_ARG(QStringList,QStringList{}));
    }
};

int exerciseArtwork(Player &player,QQuickWindow *window,const QString &temp,const QString &captures) {
    int checks=0,failures=0;QString context;
    const auto check=[&](bool ok,const char *name){++checks;if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<context.toStdString()<<": "<<name<<std::endl;return ok;};
    const auto wait=[](const std::function<bool()> &f,int timeout=2500){QElapsedTimer t;t.start();while(!f()&&t.elapsed()<timeout)QTest::qWait(20);return f();};
    auto *presenter=qobject_cast<DiscPresentation*>(qmlContext(window)->contextProperty("presentation").value<QObject*>());
    if(!presenter)return 2;
    ArtworkCider remote;const auto originalSource=window->property("ciderService");
    window->setProperty("ciderService",QVariant::fromValue(static_cast<QObject*>(&remote)));
    player.setMotion(false);player.setVolume(0);player.demo();player.pause();window->setProperty("useCider",false);
    if(!check(wait([&]{return player.count()>0&&!player.busy()&&!player.artworkLoading();}),"local fixture imports"))return 2;
    player.pause();
    check(Disc::placeholderArt()!=Disc::fallbackArt(512),"missing-art placeholder is distinct from demo artwork");
    QImage red(64,64,QImage::Format_RGBA8888),blue=red,green=red;
    red.fill(Qt::red);blue.fill(Qt::blue);green.fill(Qt::green);
    red.save(temp+"/art-red.png");blue.save(temp+"/art-blue.png");green.save(temp+"/art-green.png");
    const auto secondTrack=temp+"/second-local.flac";
    if(!QFile::copy(player.currentUrl().toLocalFile(),secondTrack))return 2;
    player.addUrls({QUrl::fromLocalFile(secondTrack)},false);
    if(!check(wait([&]{return player.count()==2&&!player.busy();}),"second local track imports"))return 2;
    player.select(1,false);player.setCover(QUrl::fromLocalFile(temp+"/art-blue.png"));
    if(!wait([&]{return !player.artworkLoading();}))return 2;
    player.select(0,false);
    const auto colorIs=[](const QImage &image,QColor c){return !image.isNull()&&image.pixelColor(0,0)==c;};
    const auto publish=[&](const QString &id,const QString &file,const QString &album="Remote album") {remote.track(id,file.isEmpty()?QUrl():QUrl::fromLocalFile(file),album);};
    const auto inspect=[&](QColor color,bool threeD) {
        // Presentation and its QML texture bindings can settle on separate
        // event-loop turns. Keep the same deadline and inspect the whole path.
        return wait([&] {
        if(!colorIs(presenter->artwork(),color))return false;
        if(!threeD&&player.medium()=="tp7"){auto *face=window->findChild<RecorderSurface*>("recorderWheelSurface");return face&&colorIs(face->property("artwork").value<QImage>(),color);}
        if(!threeD){auto *disc=window->findChild<Disc*>("discFace");return disc&&colorIs(disc->artwork(),color);}
#ifdef SPUN_WITH_3D
        auto *view=window->findChild<QObject*>("player3DView");
        if(!view)return false;
        auto *cover=view->findChild<CoverTexture*>("currentCoverTexture");
        auto *paper=view->findChild<CoverTexture*>("cassetteCoverTexture");
        if(!cover||!paper)return false;
        const auto pixels=cover->textureData();
        const auto label=paper->textureData();
        if(pixels.isEmpty()||label.isEmpty())return false;
        QImage image(reinterpret_cast<const uchar*>(pixels.constData()),512,512,QImage::Format_RGBA8888);
        QImage paperImage(reinterpret_cast<const uchar*>(label.constData()),1024,170,QImage::Format_RGBA8888);
        return image.pixelColor(200,200)==color&&paperImage.pixelColor(60,60)==color;
#else
        return false;
#endif
        });
    };
    QList<bool> modes{false};if(qmlContext(window)->contextProperty("supports3D").toBool())modes.append(true);
    for(bool threeD:modes)for(const QString &medium:{QString("cd"),QString("vinyl"),QString("cassette"),QString("tp7")}) {
        context=medium+(threeD?" 3D":" 2D");player.setMedium(medium);player.setThreeD(threeD);QTest::qWait(80);
        window->setProperty("useCider",false);player.setCover(QUrl::fromLocalFile(temp+"/art-green.png"));
        check(wait([&]{return !player.artworkLoading();})&&inspect(Qt::green,threeD),"local artwork reaches the active medium");
        player.select(1,false);check(inspect(Qt::blue,threeD),"selecting the next local song updates its cover");
        player.select(0,false);check(inspect(Qt::green,threeD),"returning to the previous local song restores its cover");
        publish("remote-red",temp+"/art-red.png");window->setProperty("useCider",true);
        check(inspect(Qt::red,threeD),"switching to Cider replaces local artwork");
        publish("remote-blue",temp+"/art-blue.png");check(inspect(Qt::blue,threeD),"same-album track change updates artwork");
        const auto reused=temp+"/reused.png";red.save(reused);publish("reused-one",reused);check(inspect(Qt::red,threeD),"first reusable URL loads");
        blue.save(reused);publish("reused-two",reused);check(inspect(Qt::blue,threeD),"new track reloads artwork when the URL is reused");
        publish("missing",{});QTest::qWait(50);check(presenter->artwork().isNull(),"missing artwork never retains the previous song cover");
        publish("rapid-red",temp+"/art-red.png");publish("rapid-blue",temp+"/art-blue.png");publish("rapid-green",temp+"/art-green.png");
        check(inspect(Qt::green,threeD),"rapid track changes keep only the latest decoded image");
        window->setProperty("useCider",false);check(inspect(Qt::green,threeD),"returning to Local restores its current artwork");
        publish("no-remote-cover",{});window->setProperty("useCider",true);QTest::qWait(50);
        check(presenter->artwork().isNull(),"source switch without artwork does not borrow Local's cover");
        publish("capture-blue",temp+"/art-blue.png");check(inspect(Qt::blue,threeD),"artwork recovers after an empty result");
        if(!captures.isEmpty()){QDir().mkpath(captures);captureTestWindow(window).save(captures+"/"+medium+(threeD?"-3d":"-2d")+".png");}
    }
#ifdef SPUN_WITH_3D
    // Inspect the rendered label as well as its CPU-side pixel buffer. A stale
    // scene-graph texture can have correct textureData() and still show an old cover.
    const auto renderedCover=[&](QColor expected,const QString &name) {
        auto *view=window->findChild<QQuickItem*>("player3DView");
        if(!view)return false;
        const bool tape=player.medium()=="cassette";
        QVariant projected;
        QMetaObject::invokeMethod(view,"projectSurface",Q_RETURN_ARG(QVariant,projected),
            Q_ARG(QVariant,tape?128.:player.medium()=="cd"?345.:player.medium()=="tp7"?276.:285.),Q_ARG(QVariant,tape?203.:player.medium()=="tp7"?249.:294.));
        const QPointF position=view->mapToScene(projected.toPointF());
        const QImage frame=captureTestWindow(window);
        if(frame.isNull())return false;
        const double scale=frame.width()/double(window->width());
        const QPoint center=(position*scale).toPoint();
        int matches=0,total=0;
        for(int y=-4;y<=4;++y)for(int x=-4;x<=4;++x) {
            const QPoint p=center+QPoint(x,y);if(!frame.rect().contains(p))continue;
            const QColor actual=frame.pixelColor(p);++total;
            const int channel=expected.red()?actual.red():expected.green()?actual.green():actual.blue();
            const int otherA=expected.red()?actual.green():actual.red();
            const int otherB=expected.blue()?actual.green():actual.blue();
            if(actual.alpha()>128&&channel>40&&channel>otherA*1.4&&channel>otherB*1.4)++matches;
        }
        if(!captures.isEmpty()){QDir().mkpath(captures);frame.save(captures+"/render-"+name+".png");}
        return total>0&&matches>total*.7;
    };
    for(const QString &medium:{QString("vinyl"),QString("cd"),QString("cassette"),QString("tp7")}) {
        context=medium+" rendered artwork";
        player.setThreeD(false);player.setMedium(medium);player.setMotion(false);
        publish("render-start",temp+"/art-red.png","First render album");
        check(wait([&]{return colorIs(presenter->artwork(),Qt::red);}),"first render cover loads");
        player.setThreeD(true);QTest::qWait(300);
        QPointer<QObject> originalView=window->findChild<QObject*>("player3DView");
        check(renderedCover(Qt::red,medium+"-initial"),"initial cover is visible on the 3D medium");
        publish("render-next",temp+"/art-blue.png","First render album");
        check(inspect(Qt::blue,true),"next cover reaches texture data without switching modes");QTest::qWait(200);
        check(renderedCover(Qt::blue,medium+"-next"),"next song changes the visible cover without switching modes");
        player.setMotion(true);
        publish("render-album",temp+"/art-green.png","Second render album");
        check(inspect(Qt::green,true),"album change reaches texture data with animations enabled");QTest::qWait(1200);
        check(renderedCover(Qt::green,medium+"-album"),"album animation ends with the new cover visible");
        check(originalView&&originalView==window->findChild<QObject*>("player3DView"),"artwork refresh preserves the active 3D scene");
        player.setMotion(false);
    }
    for(const QString &origin:{QString("cd"),QString("cassette"),QString("tp7")}) {
        context=origin+" to vinyl";
        player.setMedium(origin);publish("switch-start",temp+"/art-red.png");
        check(inspect(Qt::red,true),"cover loads before switching medium");
        player.setMedium("vinyl");QTest::qWait(250);
        check(renderedCover(Qt::red,origin+"-to-vinyl"),"switching to vinyl displays the current artwork");
        publish("switch-next",temp+"/art-blue.png");
        check(inspect(Qt::blue,true),"next track loads after switching to vinyl");QTest::qWait(200);
        check(renderedCover(Qt::blue,origin+"-to-vinyl-next"),"vinyl keeps updating after the mode switch");
    }
#endif
    context="album animation";
    DiscPresentation staged;int swaps=0;
    QObject::connect(&staged,&DiscPresentation::swapRequested,&staged,[&]{++swaps;});
    staged.present(red,"album-one",true);staged.present({},"album-two",true);
    check(staged.artwork().isNull()&&staged.outgoing()==red&&swaps==1,"loading cover retains the old image only as the outgoing disc");
    staged.present(blue,"album-two",true);
    check(staged.artwork()==blue&&staged.outgoing()==red&&swaps==1,"arriving artwork updates without replaying the album swap");
    staged.present(green,"album-two",true);
    check(staged.artwork()==green&&swaps==1,"same-album cover changes never exchange the medium");
    staged.clear();check(staged.artwork().isNull()&&staged.outgoing().isNull(),"changing source clears both presentation images");
    // Serve a deliberately late old image, followed by an immediate new one.
    context="delayed network";QTcpServer server;QByteArray redBytes,blueBytes;
    QBuffer rb(&redBytes),bb(&blueBytes);rb.open(QIODevice::WriteOnly);bb.open(QIODevice::WriteOnly);red.save(&rb,"PNG");blue.save(&bb,"PNG");
    check(server.listen(QHostAddress::LocalHost),"local artwork fixture listens");
    QJsonObject apiCurrent;int apiDelay=0,apiRequests=0,apiStatus=200;
    QObject::connect(&server,&QTcpServer::newConnection,&server,[&]{
        while(auto *socket=server.nextPendingConnection()){
            QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            QObject::connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{
                const auto request=socket->readAll();if(socket->property("sent").toBool())return;socket->setProperty("sent",true);
                const bool api=request.contains("/api/v2/playback/now-playing");
                const bool slow=request.contains("/slow");QPointer<QTcpSocket> guarded=socket;
                if(api)++apiRequests;
                const QByteArray bytes=api?QJsonDocument(QJsonObject{{"data",apiCurrent}}).toJson(QJsonDocument::Compact):slow?redBytes:blueBytes;
                const int status=api?apiStatus:200;
                QTimer::singleShot(api?apiDelay:slow?450:0,&server,[guarded,bytes,status]{if(!guarded)return;guarded->write("HTTP/1.1 "+QByteArray::number(status)+" Reply\r\nContent-Type: application/octet-stream\r\nContent-Length: "+QByteArray::number(bytes.size())+"\r\nConnection: close\r\n\r\n"+bytes);guarded->disconnectFromHost();});
            });
        }
    });
    const QString base="http://127.0.0.1:"+QString::number(server.serverPort());
    remote.track("slow",QUrl(base+"/slow"));QTest::qWait(50);remote.track("fast",QUrl(base+"/fast"));
    check(wait([&]{return colorIs(remote.artwork(),Qt::blue);}),"new network cover wins over an older request");QTest::qWait(500);
    check(colorIs(remote.artwork(),Qt::blue)&&colorIs(presenter->artwork(),Qt::blue),"late response cannot replace the current cover");
    remote.track("slow-clear",QUrl(base+"/slow"));QTest::qWait(30);remote.track("empty",{});QTest::qWait(500);
    check(remote.artwork().isNull()&&presenter->artwork().isNull(),"clearing artwork cancels pending network results");
    const auto connection=temp+"/artwork-api.json";
    {QFile file(connection);if(!file.open(QIODevice::WriteOnly))return 2;file.write("{\"token\":\"isolated-test-token\"}");}
    ArtworkCider paired(connection,QUrl(base));
    window->setProperty("ciderService",QVariant::fromValue(static_cast<QObject*>(&paired)));
    paired.setLiveVisible(false);
    const auto current=[&](const QString &id,const QString &cover){return QJsonObject{
        {"name",id},{"albumName","Album"},{"artistName","Remote artist"},
        {"playParams",QJsonObject{{"id","library-"+id},{"catalogId",id}}},
        {"artwork",QJsonObject{{"url",cover}}}};};
    for(bool threeD:modes)for(const QString &medium:{QString("cd"),QString("vinyl"),QString("cassette"),QString("tp7")}) {
        context=medium+(threeD?" paired 3D":" paired 2D");player.setMedium(medium);player.setThreeD(threeD);QTest::qWait(60);
        apiCurrent=current("album-a",base+"/slow");paired.track("album-a",QUrl(base+"/slow"),"Album A");
        check(inspect(Qt::red,threeD),"paired artwork uses the API's matching catalog identity");
        const auto stableAlbum=paired.albumKey();
        apiCurrent=current("album-a-next",base+"/fast");paired.track("album-a-next",QUrl(base+"/slow"),"Album A");
        check(paired.albumKey()==stableAlbum,"starting a new cover request preserves the album identity");
        check(inspect(Qt::blue,threeD)&&paired.albumKey()==stableAlbum,"finishing a same-album cover does not trigger another disc exchange");
        apiCurrent=current("album-a",base+"/slow");
        paired.track("album-b",QUrl(base+"/slow"),"Album B");QTest::qWait(120);
        check(paired.artwork().isNull()&&presenter->artwork().isNull(),"new album with old desktop URL and old API item stays empty");
        apiCurrent=current("album-b",base+"/fast");
        check(inspect(Qt::blue,threeD),"retry accepts the new album cover once the API catches up");
        paired.track("album-b",QUrl(base+"/slow?stale=1"),"Album B");
        check(inspect(Qt::blue,threeD),"late desktop artwork cannot overwrite the identity-checked API cover");
        apiCurrent=current("album-c",base+"/slow");paired.track("album-b",QUrl(base+"/slow?ahead=1"),"Album B");QTest::qWait(120);
        check(presenter->artwork().isNull(),"API arriving before desktop track identity cannot show the next album early");
        paired.track("album-c",QUrl(base+"/fast"),"Album C");
        check(inspect(Qt::red,threeD),"desktop catching up resolves an API-first album change");
        apiCurrent=current("album-old",base+"/slow");apiDelay=400;paired.track("album-old",QUrl(base+"/slow"));QTest::qWait(50);
        apiDelay=0;apiCurrent=current("album-latest",base+"/fast");paired.track("album-latest",QUrl(base+"/slow"));
        check(inspect(Qt::blue,threeD),"rapid album selection cancels the previous API lookup");QTest::qWait(450);
        check(inspect(Qt::blue,threeD),"late album lookup cannot replace the latest cover");
    }
    context="paired retry limits";const int before=apiRequests;
    apiCurrent=current("never-current",base+"/slow");paired.track("no-match",QUrl(base+"/slow"));QTest::qWait(2000);
    check(paired.artwork().isNull()&&presenter->artwork().isNull(),"unmatched API item never becomes current artwork");
    check(wait([&]{return apiRequests-before>=4;},6500),"bounded retries reach the final attempt");
    QTest::qWait(400);check(apiRequests-before==4,"mismatched artwork lookup stops after four attempts");
    apiCurrent=current("still-old",base+"/slow");paired.track("late-ready",QUrl(base+"/slow"));QTest::qWait(1200);
    apiCurrent=current("late-ready",base+"/fast");
    check(wait([&]{return colorIs(paired.artwork(),Qt::blue);},4500),"album metadata arriving after one second still recovers");
    apiStatus=404;paired.track("legacy-api",QUrl(base+"/fast"));
    check(wait([&]{return colorIs(paired.artwork(),Qt::blue);}),"unsupported now-playing API retains desktop artwork support");
    window->setProperty("useCider",false);window->setProperty("ciderService",originalSource);player.setThreeD(false);QTest::qWait(30);
    std::cout<<"ARTWORK RESULT "<<failures<<" failures / "<<checks<<" checks"<<std::endl;return failures?1:0;
}
#include "artworktest.moc"
