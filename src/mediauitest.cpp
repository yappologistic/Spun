#include "mediauitest.h"
#include "player.h"
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
    player.setVolume(0);player.setMotion(false);window->setProperty("useCider",false);
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
    std::cout<<"MEDIA UI RESULT "<<failures<<" failures"<<std::endl;return failures?1:0;
}
