#include "mediauitest.h"
#include "player.h"
#include "disc.h"
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QTest>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <cmath>
#include <functional>
#include <iostream>

int exerciseMediaUi(Player &player, QQuickWindow *window, const QString &temp, const QString &captures) {
    int failures=0;
    auto check=[&](bool ok,const char *name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<std::endl;if(!ok)++failures;};
    auto wait=[](std::function<bool()> condition){QElapsedTimer t;t.start();while(!condition()&&t.elapsed()<5000)QTest::qWait(20);return condition();};
    std::function<QQuickItem*(QQuickItem*,const QString&)> find=[&](QQuickItem *parent,const QString &name)->QQuickItem*{
        if(parent->objectName()==name)return parent;
        for(auto *child:parent->childItems())if(auto *result=find(child,name))return result;
        return nullptr;
    };
    auto item=[&](const QString &name){return find(window->contentItem(),name);};
    auto capture=[&](const QString &name){
        if(captures.isEmpty())return;
        if(!window->isExposed()){std::cout<<"SKIP capture on hidden native workspace"<<std::endl;return;}
        QDir().mkpath(captures);
        auto grab=window->contentItem()->grabToImage();
        check(grab&&wait([&]{return !grab->image().isNull();})&&grab->image().save(captures+"/"+name+".png"),"media capture saved");
    };
    player.setShowPlayerBody(true);player.setVolume(0);player.setMotion(false);window->setProperty("useCider",false);
    QList<QUrl> files;
    for(int i=0;i<3;++i){
        const auto path=temp+QString("/record-%1.flac").arg(i);
        QFile::copy(QStringLiteral(SPUN_SOURCE_DIR "/assets/First-Light.flac"),path);
        TagLib::FileRef file(QFile::encodeName(path).constData());
        file.tag()->setAlbum("Evening record");file.tag()->setTrack(i+1);file.tag()->setTitle(TagLib::String(QString("Track %1").arg(i+1).toUtf8().constData(),TagLib::String::UTF8));file.save();
        files.append(QUrl::fromLocalFile(path));
    }
    player.addUrls(files,false);check(wait([&]{return !player.busy();})&&player.count()==3,"album fixture imports three tracks");
    player.setVinyl(true);player.setVinylAlbumMode(true);QTest::qWait(350);
    check(!window->property("recordKey").toString().isEmpty(),"album grooves expose a stable gesture identity");
    auto *arm=item("vinylTonearm"),*shaft=item("tonearmShaft");
    check(arm&&shaft&&item("albumGrooves"),"album needle and groove boundaries are visible");
    if(arm&&shaft){
        const auto from=shaft->mapToScene(QPointF(0,197)).toPoint();
        const double angle=16*std::acos(-1.)/180.;
        const auto to=arm->mapToScene(QPointF(378-197*std::sin(angle),100+197*std::cos(angle))).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);
        QMouseEvent move(QEvent::MouseMove,to,window->mapToGlobal(to),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QGuiApplication::sendEvent(window,&move);
        check(arm->property("dragging").toBool(),"native pointer grabs the album needle");
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,to);
        check(wait([&]{return player.currentIndex()==1&&player.playing()&&qAbs(player.position()-16000)<1500;}),"album needle drop starts the second track halfway through");
        capture("album-needle");
        player.pause();QTest::qWait(60);
        const auto parked=shaft->mapToScene(QPointF(0,197)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,parked);
        player.setVinylAlbumMode(false);QTest::qWait(20);
        check(!arm->property("dragging").toBool(),"changing record mode cancels an active needle gesture");
        const auto position=player.position();QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,parked);
        check(qAbs(player.position()-position)<100,"cancelled record gesture leaves playback position unchanged");
    }
    player.setMedium("cassette");player.setMotion(true);player.seek(3200);player.play();QTest::qWait(250);
    const double left=window->property("cassetteLeftAngle").toDouble(),right=window->property("cassetteRightAngle").toDouble();QTest::qWait(60);
    const double dl=std::remainder(window->property("cassetteLeftAngle").toDouble()-left,360.),dr=std::remainder(window->property("cassetteRightAngle").toDouble()-right,360.);
    if(window->isExposed())check(dl<0&&dr<0&&std::abs(dr)>std::abs(dl),"forward tape turns both reels counterclockwise with the smaller pack faster");
    else check(dl==0&&dr==0,"hidden native workspace suspends reel animation");
    player.pause();QTest::qWait(80);auto *seek=item("cassetteSeek");
    if(seek){
        const auto from=seek->mapToScene(QPointF(seek->width()*.75,18)).toPoint(),to=seek->mapToScene(QPointF(seek->width()*.3,18)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);
        QMouseEvent move(QEvent::MouseMove,to,window->mapToGlobal(to),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QGuiApplication::sendEvent(window,&move);
        const double angle=window->property("cassetteLeftAngle").toDouble();QTest::qWait(65);
        const double delta=std::remainder(window->property("cassetteLeftAngle").toDouble()-angle,360.);
        if(window->isExposed())check(delta>0,"rewind preview turns the cassette reels clockwise");
        else check(delta==0,"hidden rewind preview does not redraw the cassette");
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,to);
    }else check(false,"cassette seeking is available");
    capture("cassette");player.setMiniMode(true);QTest::qWait(250);capture("cassette-mini");
    check(item("cassetteReel0")&&window->mask().contains(QPoint(window->width()/2,window->height()/2)),"Mini cassette keeps its reels inside the native input mask");
    player.setMotion(false);const double stopped=window->property("cassetteLeftAngle").toDouble();QTest::qWait(100);
    check(window->property("cassetteLeftAngle").toDouble()==stopped,"reduced motion freezes cassette winding");
    player.setMiniMode(false);player.setVinylAlbumMode(false);player.pause();
    const auto deckPos=item("playerDeck")->mapToScene(QPointF());
    const auto barPos=item("sourceBar")->mapToScene(QPointF());
    for(const auto &medium:QStringList{"cd","vinyl","cassette"}) {
        player.setMedium(medium);player.setShowPlayerBody(true);QTest::qWait(200);
        check(item("playerBody")&&item("playerBody")->isVisible(),"full mode shows the selected player housing");
        check(item("playerDeck")->mapToScene(QPointF())==deckPos&&item("sourceBar")->mapToScene(QPointF())==barPos,"hardware keeps the existing playback and source controls aligned");
        capture("body-"+medium);
        window->setProperty("discFlipped",true);QTest::qWait(100);capture("booklet-"+medium);
        check(!item("playerLid")->isVisible(),"album booklet keeps the lid away from track and lyric controls");
        check(item("albumTrackList")->height()>140,"album booklet gives tracks a full reading area");
        check(item("discBackFooter")->mapToScene(QPointF(0,36)).y()<item("albumBooklet")->mapToScene(QPointF(0,400)).y(),"booklet footer stays inside the physical case with bottom padding");
        check(item("cassetteSeek")->isVisible()&&!item("progressRing")->isVisible(),"booklet uses a horizontal seek bar within the case");
        const auto bookSeek=item("cassetteSeek")->mapToScene(QPointF(80,18)).toPoint();
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,bookSeek);
        check(wait([&]{return qAbs(player.position()-player.duration()*.25)<1200;}),"booklet progress bar seeks the current song");
        window->setProperty("discFlipped",false);
        player.setShowPlayerBody(false);QTest::qWait(50);
        check(!item("playerBody")->isVisible()&&window->property("mediumScale").toDouble()==1.,"body toggle restores the original medium geometry");
        player.setShowPlayerBody(true);player.setMiniMode(true);QTest::qWait(80);
        check(!item("playerBody")->isVisible(),"Mini mode retains its compact silhouette");player.setMiniMode(false);
    }
    player.setMedium("cassette");
    for(const auto &finish:QStringList{"clear","smoke","cream"}) { player.setCassetteFinish(finish);QTest::qWait(100);capture("cassette-"+finish); }
    player.setCassetteFinish("invalid");check(player.cassetteFinish()=="cream","invalid cassette finishes cannot replace the saved selection");
    {Player restored(temp+"/player.ini");check(restored.showPlayerBody()&&restored.cassetteFinish()=="cream","player body and cassette finish preferences survive relaunch");}
    auto *typography=qmlContext(window)->contextProperty("typography").value<QObject*>();
    player.setMedium("vinyl");
    for(double zoom:{.85,1.,1.5}) {
        typography->setProperty("uiScale",zoom);QTest::qWait(100);
        auto *scrub=item("scrubber");const auto point=scrub->mapToScene(QPointF(436,220)).toPoint();
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point);
        check(wait([&]{return qAbs(player.position()-player.duration()*.25)<700;}),"scaled player housing preserves accurate circular seeking");
    }
    typography->setProperty("uiScale",1.);QTest::qWait(100);
    check(item("tonearmLoader")->z()>item("progressRing")->z(),"tonearm hardware occludes the waveform instead of being crossed by it");
    QTest::mouseMove(window,QPoint(25,15));
    player.setMotion(true);player.play();QTest::qWait(200);capture("vinyl-playing");player.pause();

    auto *presentation=qobject_cast<DiscPresentation*>(qmlContext(window)->contextProperty("presentation").value<QObject*>());
    int swaps=0;QObject::connect(presentation,&DiscPresentation::swapRequested,presentation,[&]{++swaps;});
    player.setMotion(true);player.setMedium("cd");
    check(!player.cd500Rpm()&&window->property("cdDegreesPerSecond").toDouble()==9,"CD retains its gentle rotation by default");
    auto *preferences=window->findChild<QObject*>("preferencesPopup");
    if(preferences)QMetaObject::invokeMethod(preferences,"open");
    QTest::qWait(220);
    auto *speedToggle=item("cd500RpmToggle");
    check(speedToggle&&speedToggle->isVisible(),"CD preferences expose the 500 RPM toggle");
    if(speedToggle)QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,speedToggle->mapToScene(QPointF(speedToggle->width()-32,speedToggle->height()/2)).toPoint());
    check(player.cd500Rpm(),"clicking the CD speed toggle enables fast rotation");QTest::qWait(300);capture("cd-speed-settings");
    player.setMedium("vinyl");QTest::qWait(30);
    check(speedToggle&&!speedToggle->isVisible()&&window->property("spinSpeed").toDouble()==0,"CD setting stays hidden in vinyl mode and does not carry over its rotation");
    player.setMedium("cd");if(preferences)QMetaObject::invokeMethod(preferences,"close");QTest::qWait(220);
    check(window->property("cdDegreesPerSecond").toDouble()==3000,"500 RPM converts to 3000 degrees per second");
    {Player restored(temp+"/player.ini");check(restored.cd500Rpm(),"CD speed preference survives relaunch");}
    player.play();
    if(window->isExposed())check(wait([&]{return window->property("spinSpeed").toDouble()>2900;}),"playing CD accelerates to the selected 500 RPM");
    else {const auto angle=window->property("spinAngle");QTest::qWait(80);check(window->property("spinAngle")==angle,"hidden 500 RPM CD does not animate");}
    player.setMotion(false);const auto cdAngle=window->property("spinAngle");QTest::qWait(80);
    check(window->property("spinAngle")==cdAngle,"reduced motion freezes the 500 RPM CD");
    player.pause();player.setCd500Rpm(false);player.setMotion(true);

    presentation->present(player.artwork(),"fixture-album-one",false);
    presentation->present(player.artwork(),"fixture-album-two",true);QTest::qWait(150);
    if(window->isExposed())check(window->property("swapRunning").toBool()&&window->property("packageOpacity").toDouble()>0,"album exchange presents its packaging and opens the lid");
    capture("album-loading");
    player.play();check(wait([&]{return player.playing();}),"packaging animation never delays playback");player.pause();
    presentation->present(player.artwork(),"fixture-album-two",true);
    check(swaps==1,"same-album updates do not exchange the physical medium");
    player.setMotion(false);QTest::qWait(30);
    check(!window->property("swapRunning").toBool()&&window->property("lidOpen").toDouble()==0,"reduced motion interrupts loading and closes the lid");
    QObject::disconnect(presentation,&DiscPresentation::swapRequested,presentation,nullptr);
    QFile palette(temp+"/config/gtk-4.0/noctalia.css");
    if(palette.open(QIODevice::ReadOnly)) {
        const auto original=palette.readAll();palette.close();auto light=original;
        light.replace("#17191f","#f6f0e8").replace("#eee5dc","#342d29").replace("#e6b599","#8b492d").replace("#392619","#ffffff").replace("#24252b","#e9e0d5");
        if(palette.open(QIODevice::WriteOnly|QIODevice::Truncate)){palette.write(light);palette.close();}
        check(wait([&]{return window->property("surface").value<QColor>().lightness()>128;}),"physical housings follow the light Noctalia palette");
        for(const auto &medium:QStringList{"cd","vinyl","cassette"}){player.setMedium(medium);QTest::qWait(100);capture("light-"+medium);}
        if(palette.open(QIODevice::WriteOnly|QIODevice::Truncate)){palette.write(original);palette.close();}
    }

    std::cout<<"MEDIA UI RESULT "<<failures<<" failures"<<std::endl;return failures?1:0;
}
