#include "threedtest.h"
#include "player.h"
#include "disc.h"
#include "theme.h"
#include "testinput.h"
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointer>
#include <QTest>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QWheelEvent>
#ifdef SPUN_WITH_3D
#include <QQuick3DGeometry>
#endif
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <cmath>
#include <functional>
#include <iostream>

namespace {
bool until(const std::function<bool()> &condition,int timeout=2500) {
    QElapsedTimer t;t.start();while(!condition()&&t.elapsed()<timeout)QTest::qWait(20);return condition();
}
QQuickItem *findItem(QQuickItem *parent,const QString &name) {
    if(parent->objectName()==name)return parent;
    for(auto *child:parent->childItems())if(auto *found=findItem(child,name))return found;
    return nullptr;
}
}

int exerciseThreeD(Player &player,QQuickWindow *window,const QString &temp,const QString &captures) {
    int failures=0,checks=0;
    QString context="setup";
    const auto check=[&](bool ok,const char *name){++checks;if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<context.toStdString()<<": "<<name<<std::endl;return ok;};
    if(!check(qmlContext(window)->contextProperty("supports3D").toBool(),"Quick 3D is available"))return 2;
    if(!check(until([&]{return window->isExposed();}),"native test window is exposed"))return 2;
    auto *typography=qmlContext(window)->contextProperty("typography").value<QObject*>();
    const auto item=[&](const QString &name){auto *result=findItem(window->contentItem(),name);return result ? result : window->findChild<QQuickItem*>(name);};
    const auto invoke=[&](const char *name){QMetaObject::invokeMethod(window,name);};
    const auto click=[&](const QString &name){auto *control=item(name);if(!check(control&&control->isVisible()&&control->isEnabled(),qPrintable(name+" is actionable")))return;QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,control->mapToScene(QPointF(control->width()/2,control->height()/2)).toPoint());QTest::qWait(40);};
    const auto project=[&](QQuickItem *control,QPointF point){
        auto *view=item("player3DView");auto *surface=item("mediaSurface");
        if(!view||!surface||!control)return QPoint(-1000,-1000);
        const auto local=control->mapToItem(surface,point);QVariant result;
        QMetaObject::invokeMethod(view,"projectSurface",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,local.x()),Q_ARG(QVariant,local.y()));
        return view->mapToScene(result.toPointF()).toPoint();
    };
    const auto hoverOutside=[&]{QTest::mouseMove(window,QPoint(12,12));QTest::qWait(60);};
    const auto drag=[&](QPoint from,QPoint to,Qt::KeyboardModifiers mods=Qt::NoModifier){
        QTest::mousePress(window,Qt::LeftButton,mods,from);
        for(int step=1;step<=8;++step){const auto p=from+(to-from)*step/8;QMouseEvent event(QEvent::MouseMove,p,window->mapToGlobal(p),Qt::NoButton,Qt::LeftButton,mods);QGuiApplication::sendEvent(window,&event);QTest::qWait(15);}
        QTest::mouseRelease(window,Qt::LeftButton,mods,to);QTest::qWait(60);
    };
    const auto render=[&](const QString &name){
        const auto frame=window->grabWindow();
        if(!check(!frame.isNull(),"rendered frame is available"))return;
        const auto sx=frame.width()/double(window->width());const auto scale=window->property("uiScale").toDouble()*sx;
        int opaque=0,total=0;
        for(int y=130;y<490;y+=8)for(int x=65;x<465;x+=8){++total;const QPoint p(qRound(x*scale),qRound(y*scale));if(frame.rect().contains(p)&&frame.pixelColor(p).alpha()>128)++opaque;}
        check(opaque>total/3,"3D medium is actually rendered");
        if(!captures.isEmpty()){QDir().mkpath(captures);check(frame.save(captures+"/"+name+".png"),"capture saved");}
    };
    player.setVolume(0);player.setMotion(false);player.setThreeD(false);player.setMiniMode(false);player.setShuffle(false);player.setRepeatMode(0);
    window->setProperty("useCider",false);player.clear();
    QList<QUrl> files;
    for(int i=0;i<4;++i){const auto path=temp+QString("/three-d-%1.flac").arg(i);QFile::copy(QStringLiteral(SPUN_SOURCE_DIR "/assets/First-Light.flac"),path);TagLib::FileRef file(QFile::encodeName(path).constData());file.tag()->setAlbum(i<3?"Test record":"Next record");file.tag()->setTitle(TagLib::String(QString("Test track %1").arg(i+1).toUtf8().constData(),TagLib::String::UTF8));file.tag()->setTrack(i+1);file.save();files.append(QUrl::fromLocalFile(path));}
    player.addUrls(files,false);if(!check(until([&]{return !player.busy()&&player.count()==4;}),"isolated album fixtures imported"))return 1;
    player.setThreeD(true);if(!check(until([&]{return window->property("threeDActive").toBool();}),"3D scene loads"))return 1;
    QTest::qWait(300);
    for(const auto &medium:QStringList{"cd","vinyl","cassette"}) {
        player.setMedium(medium);player.setVinylAlbumMode(false);player.setHorizontalSeek(false);player.select(0,false);QTest::qWait(200);
        for(double zoom:{.85,1.,1.5}) {
            context=medium+QString(" scale %1").arg(zoom);typography->setProperty("uiScale",zoom);QTest::qWait(200);
            auto *control=item(medium=="cassette"?"cassetteSeek":"scrubber");
            if(!check(control!=nullptr,"projected seek control exists"))continue;
            player.seek(0);
            const auto quarter=project(control,medium=="cassette"?QPointF(80,18):QPointF(436,220));
            QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,quarter);
            check(until([&]{return std::abs(player.position()-player.duration()*.25)<1200;}),"projected click seeks to 25 percent");
            const auto threeQuarter=project(control,medium=="cassette"?QPointF(224,18):QPointF(4,220));
            if(medium=="cassette")drag(quarter,threeQuarter);
            else {
                QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,quarter);
                for(int step=1;step<=16;++step){const double angle=(.5+step/16.)*std::acos(-1.);const auto p=project(control,QPointF(220+216*std::sin(angle),220-216*std::cos(angle)));QMouseEvent event(QEvent::MouseMove,p,window->mapToGlobal(p),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QGuiApplication::sendEvent(window,&event);QTest::qWait(12);}
                QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,threeQuarter);QTest::qWait(60);
            }
            check(until([&]{return std::abs(player.position()-player.duration()*.75)<1400;}),"projected drag seeks to 75 percent");
            check(!window->property("threeDGesture").toBool(),"release clears seek gesture");
            hoverOutside();
            player.seek(0);click("playButton");check(until([&]{return player.playing();}),"play works after 3D drag");
            click("playButton");check(!player.playing(),"pause works after 3D drag");
            player.select(0,false);click("nextButton");check(until([&]{return player.currentIndex()==1;}),"next selects the next song");player.pause();player.seek(0);
            click("previousButton");check(until([&]{return player.currentIndex()==0;}),"previous selects the preceding song");player.pause();
            click("shuffleButton");check(player.shuffle(),"shuffle toggles on");click("shuffleButton");check(!player.shuffle(),"shuffle toggles off");
            player.setRepeatMode(0);for(int n=1;n<=3;++n){click("repeatButton");check(player.repeatMode()==n%3,"repeat cycles correctly");}
            click("volumeSlider");check(player.volume()>0,"volume slider still receives untransformed input");player.setVolume(0);
            click("queueButton");check(window->property("queueOpen").toBool(),"queue opens alongside 3D");QTest::qWait(150);
            player.seek(0);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,project(control,medium=="cassette"?QPointF(80,18):QPointF(436,220)));
            check(until([&]{return std::abs(player.position()-player.duration()*.25)<1200;}),"seeking stays aligned with queue open");
            click("queueButton");check(!window->property("queueOpen").toBool(),"queue closes after projected interaction");
            hoverOutside();
        }
        typography->setProperty("uiScale",1.);player.pause();player.seek(4000);QTest::qWait(180);context=medium+" lifecycle";render(medium);
#ifdef SPUN_WITH_3D
        const auto geometryFor=[&](const char *name){auto *model=window->findChild<QObject*>(name);return model?qobject_cast<QQuick3DGeometry*>(model->property("geometry").value<QObject*>()):nullptr;};
        if(medium!="cassette") {
            auto *record=geometryFor("threeDRecord"),*labelMesh=geometryFor("threeDRecordLabel");
            check(record&&record->boundsMax().z()>record->boundsMin().z(),"record has physical thickness");
            check(labelMesh&&std::abs(labelMesh->boundsMax().x()-(medium=="vinyl"?62:169))<.1,"label mesh has the intended physical proportions");
        } else {
            auto *cartridge=window->findChild<QObject*>("threeDReel0");check(cartridge!=nullptr,"cassette has modeled reels");
        }
#endif

        for(bool motion:{false,true}) {
            player.setMotion(motion);player.play();QTest::qWait(150);
            invoke("flipDisc");check(until([&]{return window->property("discFlipped").toBool()&&!window->property("threeDActive").toBool();}),"flip opens readable reverse while playing");
            check(player.playing(),"flipping preserves playback");
            hoverOutside();invoke("flipDisc");check(until([&]{return window->property("threeDActive").toBool();}),"return from reverse restores 3D");
            player.setMiniMode(true);check(until([&]{return !window->property("threeDActive").toBool();}),"Mini unloads 3D");hoverOutside();
            player.setMiniMode(false);check(until([&]{return window->property("threeDActive").toBool();}),"full mode restores 3D");
            player.setThreeD(false);hoverOutside();check(!window->property("threeDActive").toBool()&&item("mediaSurface")->parentItem()->objectName()!="threeDTextureRoot","flat controls survive 3D destruction");
            player.setThreeD(true);QTest::qWait(180);hoverOutside();check(window->property("threeDActive").toBool(),"3D can be enabled again after hovering flat controls");
        }
        player.pause();player.setMotion(false);
    }
    context="needle combinations";player.setMedium("vinyl");QTest::qWait(150);
    for(bool album:{false,true}) {
        player.setVinylAlbumMode(album);player.select(0,false);player.seek(0);QTest::qWait(180);
        auto *arm=item("vinylTonearm");auto *needle=item("needleHandle");
        if(!check(arm&&needle,"tonearm exists"))continue;
        const auto from=project(arm,needle->property("tip").toPointF());const double angle=16*std::acos(-1.)/180.;const auto to=project(arm,QPointF(378-197*std::sin(angle),100+197*std::cos(angle)));
        auto *bands=window->findChild<QObject*>("threeDTrackBands");
        check(bands&&bands->property("count").toInt()==(album?2:0),"album track boundaries appear only in record mode");
        drag(from,to);check(until([&]{return player.playing()&&player.currentIndex()==(album?1:0)&&std::abs(player.position()-16000)<1800;}),"needle maps song or album position correctly");player.pause();
        for(int transition=0;transition<3;++transition){
            QTest::qWait(100);needle=item("needleHandle");arm=item("vinylTonearm");
            const auto tip=project(arm,needle->property("tip").toPointF());QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,tip);QTest::qWait(30);
            check(arm->property("dragging").toBool(),"needle grab begins before mode change");
            if(transition==0)player.setThreeD(false);else if(transition==1)player.setMiniMode(true);else window->setProperty("discFlipped",true);
            QTest::qWait(70);check(!arm->property("dragging").toBool(),"mode change cancels needle grab");
            QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,tip);hoverOutside();
            player.setMiniMode(false);window->setProperty("discFlipped",false);player.setThreeD(true);QTest::qWait(140);
        }
    }
    context="projected shortcuts and hints";player.setMedium("cd");player.setMotion(false);player.setVinylAlbumMode(false);QTest::qWait(150);
    auto *view=item("player3DView");
    const QPoint center=view->mapToScene(QPointF(view->width()/2,view->height()/2)).toPoint();
    QTest::mouseDClick(window,Qt::LeftButton,Qt::NoModifier,center);
    check(until([&]{return window->property("discFlipped").toBool();}),"double clicking the 3D medium opens its reverse");
    invoke("flipDisc");QTest::qWait(180);
    QTest::mouseClick(window,Qt::RightButton,Qt::NoModifier,center);
    check(until([&]{return window->property("menuOpen").toBool();}),"right click opens settings in 3D");
    testKeyClick(window,Qt::Key_Escape);QTest::qWait(180);
    check(!window->property("menuOpen").toBool(),"Escape dismisses the 3D context menu");
    hoverOutside();auto *rim=item("scrubber");
    const QPoint rimPoint=project(rim,QPointF(436,220));QTest::mouseMove(window,rimPoint);QTest::qWait(850);
    auto *hint=window->findChild<QObject*>("threeDToolTip");
    check(hint&&hint->property("visible").toBool(),"projected seek hover has a correctly placed hint");
    QTest::qWait(2700);check(hint&&!hint->property("visible").toBool(),"projected hint expires while stationary");
    hoverOutside();QTest::mouseMove(window,rimPoint);QTest::qWait(850);
    check(hint&&hint->property("visible").toBool(),"leaving and returning allows the hint again");
    hoverOutside();QTest::qWait(150);check(hint&&!hint->property("visible").toBool(),"leaving the player dismisses the hint");
    player.seek(0);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,rimPoint);testKeyClick(window,Qt::Key_Escape);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,rimPoint);
    check(!window->property("threeDGesture").toBool()&&player.position()<1000,"Escape cancels a projected drag without seeking");
    QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,rimPoint);player.select(1,false);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,rimPoint);
    check(!window->property("threeDGesture").toBool()&&player.position()<1000,"song changes cancel a projected drag without seeking the next song");
    context="wallpaper lighting";
    auto *theme=qobject_cast<Theme*>(qmlContext(window)->contextProperty("theme").value<QObject*>());
    QFile palette(temp+"/config/gtk-4.0/noctalia.css");if(!check(palette.open(QIODevice::ReadOnly),"isolated theme fixture opens"))return 1;const auto originalPalette=palette.readAll();palette.close();
    for(const QByteArray &accent:{QByteArray("#82bed5"),QByteArray("#e4a17f")}) {
        auto css=originalPalette;css.replace("#e6b599",accent);if(!check(palette.open(QIODevice::WriteOnly),"theme fixture can change"))return 1;palette.write(css);palette.close();if(theme)theme->reload();QTest::qWait(180);
        auto *light=window->findChild<QObject*>("threeDFillLight");const QColor tint=light?light->property("color").value<QColor>():QColor();
        check(tint.isValid()&&(accent=="#82bed5"?tint.blue()>tint.red():tint.red()>tint.blue()),"3D fill light follows the live Noctalia accent");
        check(tint.redF()>.6&&tint.greenF()>.6&&tint.blueF()>.6,"wallpaper lighting keeps a neutral base for readable materials");
        render(accent=="#82bed5"?"cool-lighting":"warm-lighting");
    }
    if(!check(palette.open(QIODevice::WriteOnly),"theme fixture can be restored"))return 1;
    palette.write(originalPalette);palette.close();if(theme)theme->reload();
    context="CD lid and album transitions";player.setMedium("cd");player.setVinylAlbumMode(false);player.setMotion(true);player.setThreeD(true);QTest::qWait(200);
    auto *presentation=qobject_cast<DiscPresentation*>(qmlContext(window)->contextProperty("presentation").value<QObject*>());
    if(check(presentation!=nullptr,"album presenter is available")) {
        presentation->present(player.artwork(),"3d-first-record",false);
        presentation->present(player.artwork(),"3d-second-record",true);
        check(window->property("swapRunning").toBool(),"album change starts the lid sequence");
        player.play();check(player.playing(),"lid motion does not delay playback");
        bool stayedInside=true,sawOpen=false;QElapsedTimer timer;timer.start();
        while(timer.elapsed()<1400) {
            QTest::qWait(30);auto *view=item("player3DView");if(!view){stayedInside=false;break;}
            QVariant result;QMetaObject::invokeMethod(view,"lidBounds",Q_RETURN_ARG(QVariant,result));const auto bounds=result.toRectF();
            stayedInside=stayedInside&&bounds.left()>=0&&bounds.top()>=0&&bounds.right()<=view->width()&&bounds.bottom()<=view->height();
            if(!sawOpen&&window->property("lidOpen").toDouble()>.95){sawOpen=true;render("cd-lid-open");}
        }
        check(sawOpen,"lid reaches its open position");check(stayedInside,"lid remains below top controls and inside its viewport throughout motion");
        check(!window->property("swapRunning").toBool()&&window->property("lidOpen").toDouble()==0&&window->property("packageOpacity").toDouble()==0,"completed transition clears lid and packaging state");
        for(int interruption=0;interruption<4;++interruption) {
            player.setMotion(true);presentation->present(player.artwork(),QString("3d-interrupt-%1").arg(interruption),true);QTest::qWait(80+interruption*60);
            if(interruption==0)player.setThreeD(false);else if(interruption==1)player.setMiniMode(true);else if(interruption==2)window->setProperty("discFlipped",true);else player.setMotion(false);
            QTest::qWait(90);check(!window->property("swapRunning").toBool()&&window->property("lidOpen").toDouble()==0,"interrupting the lid sequence leaves no stale overlay");
            hoverOutside();player.setMiniMode(false);window->setProperty("discFlipped",false);player.setThreeD(true);QTest::qWait(140);
        }
        player.pause();
    }
    context="lifecycle stress";player.setVinylAlbumMode(false);
    for(int cycle=0;cycle<30;++cycle) {
        player.setMotion(cycle%2);player.setMedium(QStringList{"cd","vinyl","cassette"}[cycle%3]);player.setHorizontalSeek(cycle%2);player.setCd500Rpm(cycle%2);player.setCassetteFinish(QStringList{"clear","cream","smoke"}[cycle%3]);
        player.select(cycle%4,true);player.setThreeD(true);QTest::qWait(35);hoverOutside();
        if(cycle%3==0)window->setProperty("discFlipped",true);else if(cycle%3==1)player.setMiniMode(true);else player.setThreeD(false);
        QTest::qWait(35);hoverOutside();player.setMiniMode(false);window->setProperty("discFlipped",false);player.setThreeD(true);QTest::qWait(35);
        check(window->property("threeDActive").toBool(),"mixed medium, playback, motion and presentation cycle completes");
    }
    player.pause();player.setMotion(false);player.setThreeD(false);hoverOutside();player.setThreeD(true);QTest::qWait(200);
    window->hide();QTest::qWait(100);window->show();check(until([&]{return window->isExposed();}),"3D window survives hide and show");QTest::qWait(150);hoverOutside();render("after-stress");
    player.clear();check(player.count()==0,"clearing the queue leaves a valid empty 3D player");hoverOutside();
    player.setThreeD(false);QTest::qWait(100);hoverOutside();
    std::cout<<"3D RESULT "<<failures<<" failures / "<<checks<<" checks"<<std::endl;return failures?1:0;
}

int exerciseThreeDLighting(Player &player,QQuickWindow *window,const QString &temp,const QString &captures) {
    int failures=0,checks=0;
    const auto check=[&](bool ok,const char *name){++checks;if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<name<<std::endl;return ok;};
    auto *theme=qobject_cast<Theme*>(qmlContext(window)->contextProperty("theme").value<QObject*>());
    if(!check(theme&&qmlContext(window)->contextProperty("supports3D").toBool(),"native 3D lighting is available"))return 2;
    player.setVolume(0);player.setMotion(false);window->setProperty("useCider",false);player.demo();player.pause();player.setThreeD(true);
    if(!check(until([&]{return window->isExposed()&&window->property("threeDActive").toBool();}),"3D scene is exposed"))return 2;
    QFile palette(temp+"/config/gtk-4.0/noctalia.css");if(!palette.open(QIODevice::ReadOnly))return 2;const auto original=palette.readAll();palette.close();
    for(const auto &medium:QStringList{"cd","vinyl","cassette"}) {
        player.setMedium(medium);QTest::qWait(200);double coolBalance=0;
        for(const auto &accent:QStringList{"#82bed5","#e4a17f","#d9bafa","#916ba8"}) {
            auto css=original;css.replace("#e6b599",accent.toUtf8());
            const bool lightPalette=accent=="#916ba8";
            if(lightPalette){css.replace("#17191f","#f6f0e8");css.replace("#eee5dc","#342d29");css.replace("#24252b","#e9e0d5");}
            QSaveFile replacement(palette.fileName());
            if(!replacement.open(QIODevice::WriteOnly))return 2;
            replacement.write(css);if(!replacement.commit())return 2;
            check(until([&]{return theme->colors().value("accent").value<QColor>()==QColor(accent);}),"atomic palette replacement reaches the live theme watcher");
            QTest::qWait(550);
            auto *view=findItem(window->contentItem(),"player3DView");QVariant projected;
            // Sample the physical medium, away from artwork labels, controls,
            // accent-colored progress indicators and the tonearm.
            const QPointF surfacePoint=medium=="cassette"?QPointF(320,369):QPointF(150,349);
            if(!view)return 2;
            if(lightPalette)check(view->property("caseTint").value<QColor>().lightnessF()>.75,"light Noctalia palette updates the physical enclosure material");
            QMetaObject::invokeMethod(view,"projectSurface",Q_RETURN_ARG(QVariant,projected),Q_ARG(QVariant,surfacePoint.x()),Q_ARG(QVariant,surfacePoint.y()));
            const auto scenePoint=view->mapToScene(projected.toPointF());const QImage frame=window->grabWindow();
            if(!check(!frame.isNull(),"lighting test captures rendered pixels"))return 2;
            const double scale=frame.width()/double(window->width());const QPoint center(qRound(scenePoint.x()*scale),qRound(scenePoint.y()*scale));
            double red=0,blue=0;int samples=0;
            for(int y=-5;y<=5;++y)for(int x=-5;x<=5;++x){const auto pos=center+QPoint(x,y);if(!frame.rect().contains(pos))continue;const auto c=frame.pixelColor(pos);if(c.alpha()<240)continue;red+=c.red();blue+=c.blue();++samples;}
            check(samples>=90,"lighting sample is on an opaque physical surface");
            const double balance=samples?(red-blue)/samples:0;
            std::cout<<"LIGHTING "<<medium.toStdString()<<" "<<accent.toStdString()<<" red-minus-blue "<<balance<<std::endl;
            if(accent=="#82bed5")coolBalance=balance;
            if(accent=="#e4a17f")check(balance-coolBalance>8,"changing cool to warm visibly changes the material pixels");
            if(!captures.isEmpty()){QDir().mkpath(captures);frame.save(captures+"/"+medium+"-"+accent.mid(1)+".png");}
        }
    }
    player.setMedium("vinyl");player.setHorizontalSeek(true);player.pause();player.seek(4000);QTest::qWait(180);
    auto *bar=findItem(window->contentItem(),"horizontalSeek");
    auto *arm=findItem(window->contentItem(),"vinylTonearm");
    auto *rim=findItem(window->contentItem(),"scrubber");
    auto *view=findItem(window->contentItem(),"player3DView");
    if(check(bar&&arm&&rim&&view,"all synchronized seek surfaces are available")) {
        const auto consistent=[&](double fraction) {
            return std::abs(window->property("recordVisualProgress").toDouble()-fraction)<.015
                && std::abs(view->property("progress").toDouble()-fraction)<.015
                && std::abs(bar->property("value").toDouble()-fraction)<.015
                && std::abs(arm->property("armAngle").toDouble()-(6+20*fraction))<.35;
        };
        const QPoint start=bar->mapToScene(QPointF(bar->width()*.25,bar->height()/2)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);
        const QPoint finish=bar->mapToScene(QPointF(bar->width()*.7,bar->height()/2)).toPoint();
        QMouseEvent move(QEvent::MouseMove,finish,window->mapToGlobal(finish),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QGuiApplication::sendEvent(window,&move);
        check(consistent(bar->property("previewValue").toDouble()),"horizontal drag updates the 3D ring and paused tonearm before release");
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,finish);QTest::qWait(100);
        auto *surface=findItem(window->contentItem(),"mediaSurface");
        const auto projected=[&](QPointF point){const auto local=rim->mapToItem(surface,point);QVariant result;QMetaObject::invokeMethod(view,"projectSurface",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,local.x()),Q_ARG(QVariant,local.y()));return view->mapToScene(result.toPointF()).toPoint();};
        const QPoint ringStart=projected(QPointF(436,220));QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,ringStart);
        QPoint last=ringStart;
        for(int step=1;step<=8;++step){const double angle=(.5+step/8.)*std::acos(-1.);last=projected(QPointF(220+216*std::sin(angle),220-216*std::cos(angle)));QMouseEvent event(QEvent::MouseMove,last,window->mapToGlobal(last),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QGuiApplication::sendEvent(window,&event);}
        check(rim->property("scrubbing").toBool()&&consistent(.75),"ring retains its drag and updates the horizontal bar and tonearm before release");
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,last);
        check(until([&]{return std::abs(player.position()-player.duration()*.75)<1200;}),"releasing the ring commits the selected position");
        check(!window->property("seekPreviewActive").toBool(),"release clears the shared preview");
    }
    player.setThreeD(false);QTest::qWait(100);
    std::cout<<"LIGHTING RESULT "<<failures<<" failures / "<<checks<<" checks"<<std::endl;return failures?1:0;
}
