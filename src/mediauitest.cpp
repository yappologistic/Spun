#include "mediauitest.h"
#include "player.h"
#include "disc.h"
#include "theme.h"
#include "testcapture.h"
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QTest>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QSaveFile>
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
    auto item=[&](const QString &name){auto *result=find(window->contentItem(),name);return result ? result : window->findChild<QQuickItem*>(name);};
    auto capture=[&](const QString &name){
        if(captures.isEmpty())return;
        if(!window->isExposed()){std::cout<<"SKIP capture on hidden native workspace"<<std::endl;return;}
        QDir().mkpath(captures);
        if(window->property("threeDActive").toBool()) {
            const auto frame=window->grabWindow();
            int pixels=0;
            for(int y=100;y<qMin(510,frame.height());y+=3)for(int x=40;x<qMin(490,frame.width());x+=3)if(qAlpha(frame.pixel(x,y))>128)++pixels;
            check(pixels>3000,"3D frame contains rendered physical geometry");
            check(!frame.isNull()&&frame.save(captures+"/"+name+".png"),"media capture saved");return;
        }
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
    // Immersion reuses the physical player and never changes playback state.
    window->setProperty("queueOpen", true);
    window->contentItem()->forceActiveFocus(Qt::MouseFocusReason);
    QTest::mouseMove(window, item("playButton")->mapToScene(QPointF(28,24)).toPoint());
    QTest::keyClick(window, Qt::Key_I, Qt::ControlModifier);
    check(wait([&]{return window->property("immersive").toBool();}), "Ctrl+I enters immersive mode");
    check(!window->property("sideOpen").toBool(), "immersion closes secondary panels");
    check(wait([&]{return window->property("chromeHidden").toBool();}), "idle immersion hides chrome");
    check(item("sourceBar")->opacity()==0 && item("playerDeck")->opacity()==0, "reduced motion hides chrome without animation");
    QTest::qWait(1300);
    check(window->property("chromeHidden").toBool(), "stationary synthetic hover does not reveal idle controls");
    const auto *idleTip = item("playButton")->findChild<QObject *>("spunToolTip");
    check(idleTip && !idleTip->property("visible").toBool(), "hidden immersive controls also dismiss their tooltips");
    capture("immersive-idle");
    QTest::keyClick(window, Qt::Key_Space);
    check(!window->property("chromeHidden").toBool(), "keyboard playback shortcut reveals immersive controls");
    player.pause();
    window->setProperty("chromeIdle", true);
    QTest::mouseMove(window, QPoint(270, 560));
    check(wait([&]{return !window->property("chromeHidden").toBool();}), "pointer movement restores immersive controls");
    QTest::keyClick(window, Qt::Key_Tab);
    QTest::qWait(3100);
    check(!window->property("chromeHidden").toBool(), "keyboard focus keeps immersive controls visible");
    capture("immersive-controls");
    QTest::keyClick(window, Qt::Key_Escape);
    check(!window->property("immersive").toBool() && window->property("queueOpen").toBool(), "Escape exits immersion and restores previous queue panel");
    window->setProperty("queueOpen", false);
    // Repeated entry must refresh the saved panels and preserve transport.
    const auto immersionTrack = player.trackKey();
    const int immersionCount = player.count();
    player.setMiniMode(true);
    QTest::qWait(100);
    window->contentItem()->forceActiveFocus(Qt::MouseFocusReason);
    QTest::keyClick(window, Qt::Key_I, Qt::ControlModifier);
    check(window->property("immersive").toBool() && !player.miniMode(), "immersion exits mini mode");
    QTest::keyClick(window, Qt::Key_I, Qt::ControlModifier);
    check(!window->property("immersive").toBool() && !window->property("sideOpen").toBool(), "leaving immersion from mini keeps panels closed");
    window->setProperty("libraryOpen", true);
    QTest::keyClick(window, Qt::Key_I, Qt::ControlModifier);
    check(window->property("immersive").toBool() && !window->property("sideOpen").toBool(), "immersion closes the previously open library");
    QTest::keyClick(window, Qt::Key_F1);
    check(window->property("helpOpen").toBool(), "keyboard help opens during immersion");
    window->setProperty("chromeIdle", true);
    check(!window->property("chromeHidden").toBool(), "open help keeps immersive controls visible");
    QTest::keyClick(window, Qt::Key_I, Qt::ControlModifier);
    check(window->property("immersive").toBool(), "immersion shortcut is suppressed while help is open");
    window->setProperty("helpOpen", false);
    QTest::keyClick(window, Qt::Key_I, Qt::ControlModifier);
    check(!window->property("immersive").toBool() && window->property("libraryOpen").toBool() && !window->property("queueOpen").toBool(), "second immersion restores library instead of stale queue state");
    window->setProperty("libraryOpen", false);
    for (int cycle = 0; cycle < 3; ++cycle) {
        QTest::keyClick(window, Qt::Key_I, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_I, Qt::ControlModifier);
    }
    check(!window->property("immersive").toBool() && !window->property("sideOpen").toBool(), "repeated immersion toggles leave panels closed");
    check(player.trackKey() == immersionTrack && player.count() == immersionCount && !player.playing(), "immersion toggles preserve the paused song and queue");
    // Check actual reverse pixels: valid theme properties alone missed a
    // hardcoded silver backing behind light ink in the CD/cassette booklets.
    {
    auto *theme=qmlContext(window)->contextProperty("theme").value<Theme*>();
    QFile palette(temp+"/config/gtk-4.0/noctalia.css");
    if(!theme||!palette.open(QIODevice::ReadOnly))return 2;
    const auto original=palette.readAll();palette.close();
    const auto contrast=[](QColor a,QColor b) {
        const auto luminance=[](QColor c) {
            const auto linear=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
            return .2126*linear(c.redF())+.7152*linear(c.greenF())+.0722*linear(c.blueF());
        };
        const double x=luminance(a),y=luminance(b);return (qMax(x,y)+.05)/(qMin(x,y)+.05);
    };
    for(bool light:{false,true}) {
        auto css=original;
        if(light){css.replace("#17191f","#f6f0e8");css.replace("#eee5dc","#342d29");css.replace("#24252b","#e9e0d5");css.replace("#e6b599","#8b492d");}
        QSaveFile replacement(palette.fileName());if(!replacement.open(QIODevice::WriteOnly))return 2;
        replacement.write(css);if(!replacement.commit())return 2;
        check(wait([&]{return theme->colors()["card"].value<QColor>()==QColor(light?"#e9e0d5":"#24252b");}),"reverse follows live light/dark theme changes");
        for(const QString medium:{"cd","vinyl","cassette","tp7"}) {
            player.setMedium(medium);window->setProperty("discFlipped",false);QMetaObject::invokeMethod(window,"flipDisc");QTest::qWait(100);
            auto *booklet=item("albumBooklet"),*title=item("discAlbumTitle");
            const auto frame=captureTestWindow(window);
            if(!booklet||!title||frame.isNull()){check(false,"reverse frame is available");continue;}
            const double scale=frame.width()/double(window->width());
            const auto point=booklet->mapToScene(QPointF(205,325))*scale;
            const auto background=frame.pixelColor(point.toPoint());
            check(background.alpha()==255,"reverse text surface is opaque");
            check(contrast(title->property("color").value<QColor>(),background)>=4.5,"rendered reverse title has readable contrast");
            check(contrast(window->property("mutedInk").value<QColor>(),background)>=4.5,"rendered reverse supporting text has readable contrast");
            check(contrast(window->property("accent").value<QColor>(),background)>=4.5,"rendered reverse active track has readable contrast");
            auto *list=item("albumTrackList");check(list&&list->hasActiveFocus(),"flip moves keyboard focus into album details");
            if(!captures.isEmpty()){QDir().mkpath(captures);check(frame.save(captures+"/reverse-"+medium+(light?"-light":"-dark")+".png"),"reverse capture saved");}
            QMetaObject::invokeMethod(window,"flipDisc");QTest::qWait(30);
            check(!list->hasActiveFocus(),"closing album details releases its keyboard focus");
        }
    }
    QSaveFile restore(palette.fileName());if(!restore.open(QIODevice::WriteOnly))return 2;
    restore.write(original);if(!restore.commit())return 2;
    check(wait([&]{return theme->colors()["card"].value<QColor>()==QColor("#24252b");}),"test theme restored");
    if(qEnvironmentVariableIsSet("SPUN_TEST_REVERSE_ONLY"))return failures?1:0;
    }
    player.setVinyl(true);player.setVinylAlbumMode(true);QTest::qWait(350);
    check(!window->property("recordKey").toString().isEmpty(),"album grooves expose a stable gesture identity");
    player.setHorizontalSeek(true);QTest::qWait(100);
    auto *horizontal=item("horizontalSeek"),*albumRim=item("scrubber"),*previewArm=item("vinylTonearm");
    if(horizontal&&albumRim&&previewArm) {
        previewArm->setProperty("landingProgress",.05);previewArm->setProperty("landing",true);
        const auto point=horizontal->mapToScene(QPointF(horizontal->width()*.6,horizontal->height()/2)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,point);
        check(!previewArm->property("landing").toBool(),"new seek preview supersedes a pending needle landing");
        const double trackFraction=horizontal->property("previewValue").toDouble();
        check(std::abs(window->property("recordVisualProgress").toDouble()-trackFraction)<.01,"horizontal preview matches the whole album timeline");
        check(std::abs(previewArm->property("armAngle").toDouble()-(6+20*trackFraction))<.3,"paused album tonearm follows the horizontal preview immediately");
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,point);
        const auto ringStart=albumRim->mapToScene(QPointF(436,220)).toPoint(),ringEnd=albumRim->mapToScene(QPointF(220,436)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,ringStart);
        QMouseEvent move(QEvent::MouseMove,ringEnd,window->mapToGlobal(ringEnd),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QGuiApplication::sendEvent(window,&move);
        check(albumRim->property("scrubbing").toBool()&&std::abs(window->property("recordVisualProgress").toDouble()-.5)<.01,"album ring retains an actual pointer drag");
        check(std::abs(horizontal->property("value").toDouble()-.5)<.01&&std::abs(previewArm->property("armAngle").toDouble()-16)<.3,"album ring preview synchronizes the next song position and tonearm");
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,ringEnd);
    } else check(false,"shared album seeking controls exist");
    player.pause();player.select(0,false);player.seek(0);player.setHorizontalSeek(false);QTest::qWait(100);
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

    if(qmlContext(window)->contextProperty("supports3D").toBool()) {
        player.pause();player.setMotion(false);player.setThreeD(true);
        check(wait([&]{return window->property("threeDActive").toBool();}),"optional 3D view loads successfully");
        {Player restored(temp+"/player.ini");check(restored.threeD(),"3D preference survives relaunch");}
        for(const auto &medium:QStringList{"vinyl","cd","cassette"}) {
            player.setMedium(medium);QTest::qWait(300);
            capture("3d-"+medium);
            auto *view=item("player3DView");
            check(view&&view->isVisible(),"each physical medium has a 3D view");

            check(view&&view->property("camera").value<QObject*>(),"3D scene has an active camera");
            check(item("playerDeck")->mapToScene(QPointF())==deckPos&&item("sourceBar")->mapToScene(QPointF())==barPos,"3D leaves playback and source controls in their familiar positions");
            if(view&&window->isExposed()) {
                player.seek(0);QTest::qWait(80);
                const auto sourcePoint=medium=="cassette" ? item("cassetteSeek")->mapToItem(item("mediaSurface"),QPointF(80,18)) : item("scrubber")->mapToItem(item("mediaSurface"),QPointF(436,220));
                QVariant mapped;
                QMetaObject::invokeMethod(view,"projectSurface",Q_RETURN_ARG(QVariant,mapped),Q_ARG(QVariant,sourcePoint.x()),Q_ARG(QVariant,sourcePoint.y()));
                QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,view->mapToScene(mapped.toPointF()).toPoint());
                check(wait([&]{return qAbs(player.position()-player.duration()*.25)<1200;}),"projected 3D progress control seeks accurately");
                if(medium=="cd") {
                    for(double zoom:{.85,1.5}) {
                        typography->setProperty("uiScale",zoom);QTest::qWait(120);player.seek(0);
                        const auto point=item("scrubber")->mapToItem(item("mediaSurface"),QPointF(436,220));QVariant out;
                        QMetaObject::invokeMethod(view,"projectSurface",Q_RETURN_ARG(QVariant,out),Q_ARG(QVariant,point.x()),Q_ARG(QVariant,point.y()));
                        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,view->mapToScene(out.toPointF()).toPoint());
                        check(wait([&]{return qAbs(player.position()-player.duration()*.25)<1200;}),"3D seeking remains accurate after interface scaling");
                    }
                    typography->setProperty("uiScale",1.);QTest::qWait(120);
                }
                if(medium=="vinyl") {
                    player.seek(8000);QTest::qWait(80);
                    const auto oldRim=item("scrubber")->mapToScene(QPointF(220,2)).toPoint();
                    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,oldRim);QTest::qWait(80);
                    check(qAbs(player.position()-8000)<700,"empty 3D space cannot activate the hidden flat seek control");
                    const auto project=[&](QPointF p){QVariant out;QMetaObject::invokeMethod(view,"projectSurface",Q_RETURN_ARG(QVariant,out),Q_ARG(QVariant,p.x()),Q_ARG(QVariant,p.y()));return view->mapToScene(out.toPointF()).toPoint();};
                    auto *needle=item("needleHandle");auto *tonearm=item("vinylTonearm");
                    const auto start=project(tonearm->mapToItem(item("mediaSurface"),needle->property("tip").toPointF()));
                    const double angle=16.*M_PI/180.;
                    const auto finish=project(tonearm->mapToItem(item("mediaSurface"),QPointF(378-197*std::sin(angle),100+197*std::cos(angle))));
                    QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);QTest::qWait(30);
                    check(tonearm->property("dragging").toBool(),"3D needle accepts a grab at its projected position");
                    for(int step=1;step<=8;++step){QTest::mouseMove(window,start+(finish-start)*step/8);QTest::qWait(20);}
                    QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,finish);
                    check(wait([&]{return player.playing()&&qAbs(player.position()-player.duration()*.5)<1300;}),"3D needle drop seeks and resumes playback");player.pause();
                }

            }
        }
        player.setMotion(true);auto *view=item("player3DView");
        if(view&&window->isExposed()) {
            QTest::mouseMove(window,view->mapToScene(QPointF(430,180)).toPoint());QTest::qWait(250);
            check(std::abs(view->property("yawOffset").toDouble())>0.1,"3D responds to gentle pointer tilt");
            player.setMotion(false);QTest::qWait(50);
            check(view->property("yawOffset").toDouble()==0&&view->property("pitchOffset").toDouble()==0,"reduced motion disables perspective tilt");
        }
        player.setMiniMode(true);QTest::qWait(100);
        check(!window->property("threeDActive").toBool(),"Mini mode unloads the 3D view");
        player.setMiniMode(false);QTest::qWait(150);
        window->setProperty("discFlipped",true);QTest::qWait(150);
        check(!window->property("threeDActive").toBool(),"reverse details remain flat and readable");
        window->setProperty("discFlipped",false);player.setThreeD(false);QTest::qWait(150);
        check(!window->property("threeDActive").toBool()&&!item("scene3DLoader")->property("active").toBool(),"disabling 3D releases its scene");
    }

    player.setThreeD(false);player.setMotion(false);
    auto *notice=item("actionNotice");auto *dismiss=item("dismissActionNotice");
    for(const QString &medium:{QString("cd"),QString("vinyl"),QString("cassette")})
        for(bool mini:{false,true})for(bool horizontal:{false,true}) {
            player.setMedium(medium);player.setMiniMode(mini);player.setHorizontalSeek(horizontal);
            QMetaObject::invokeMethod(window,"notifyAction",Q_ARG(QVariant,QString("Playback message")),Q_ARG(QVariant,true));
            QTest::qWait(180);
            for(double scale:{.65,1.,1.5}) {
                typography->setProperty("uiScale",scale);QTest::qWait(60);
                const auto bounds=notice->mapRectToScene(notice->boundingRect());
                const auto *controls=item(mini?"miniControls":"playerDeck");
                const auto controlsBounds=controls->mapRectToScene(controls->boundingRect());
                check(notice->isVisible()&&bounds.top()>=controlsBounds.bottom()
                    &&QRectF(0,0,window->width(),window->height()).contains(bounds),
                    qPrintable(medium+QString(" %1 horizontal %2 scale %3: feedback fits below controls").arg(mini?"Mini":"full").arg(horizontal).arg(scale)));
            }
            QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,dismiss->mapToScene(QPointF(dismiss->width()/2,dismiss->height()/2)).toPoint());
            check(wait([&]{return !notice->isVisible();}),"scaled feedback dismissal remains clickable");
        }
    typography->setProperty("uiScale",1.);player.setMiniMode(false);player.setHorizontalSeek(false);QTest::qWait(100);
    check(window->property("layoutHeight").toInt()==730,"full player returns to its regular height after feedback closes");
    std::cout<<"MEDIA UI RESULT "<<failures<<" failures"<<std::endl;return failures?1:0;
}
