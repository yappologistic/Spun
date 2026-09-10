#include "recordertest.h"
#include "player.h"
#include "cider.h"
#include "disc.h"
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QTest>
#include <QDir>
#include <QFile>
#include <QPointer>
#include <functional>
#include <iostream>
#include <cmath>

// Same public playback contract as Cider. No requests reach a desktop account.
class RecorderRemote : public Cider {
    Q_OBJECT
    Q_PROPERTY(int count READ fixtureCount NOTIFY trackChanged)
    Q_PROPERTY(bool canSeek READ yes NOTIFY trackChanged)
    Q_PROPERTY(bool canPrevious READ yes NOTIFY trackChanged)
    Q_PROPERTY(bool canNext READ yes NOTIFY trackChanged)
    Q_PROPERTY(bool playing MEMBER running NOTIFY playingChanged)
    Q_PROPERTY(qint64 position MEMBER at NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ length NOTIFY trackChanged)
    Q_PROPERTY(double volume MEMBER level NOTIFY volumeChanged)
    Q_PROPERTY(QString trackKey READ identity NOTIFY trackChanged)
public:
    RecorderRemote():Cider(false){}
    bool yes() const { return true; }
    int fixtureCount() const { return 3; }
    qint64 length() const { return 32000; }
    QString identity() const { return QString::number(index); }
    Q_INVOKABLE void toggle() { running=!running;emit playingChanged(); }
    Q_INVOKABLE void next() { ++index;at=0;emit trackChanged();emit positionChanged(); }
    Q_INVOKABLE void previous() { --index;at=0;emit trackChanged();emit positionChanged(); }
    Q_INVOKABLE void seek(qint64 value) { at=qBound<qint64>(0,value,length());emit positionChanged(); }
    bool running=false;
    qint64 at=0;
    double level=.5;
    int index=0;
};

int exerciseRecorder(Player &player,QQuickWindow *window,const QString &temp,const QString &captures) {
    int checks=0,failures=0;QString context;
    const bool pressureOnly=qEnvironmentVariableIsSet("SPUN_TEST_PRESSURE_ONLY");
    const auto check=[&](bool ok,const char *name){++checks;if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<context.toStdString()<<": "<<name<<std::endl;return ok;};
    const auto until=[](const std::function<bool()> &f){QElapsedTimer t;t.start();while(!f()&&t.elapsed()<3500)QTest::qWait(20);return f();};
    std::function<QQuickItem*(QQuickItem*,const QString&)> find=[&](QQuickItem *parent,const QString &name)->QQuickItem*{if(parent->objectName()==name)return parent;for(auto *child:parent->childItems())if(auto *p=find(child,name))return p;return nullptr;};
    const auto item=[&](const QString &name){return find(window->contentItem(),name);};
    const auto invoke=[&](const char *name){QMetaObject::invokeMethod(window,name);QTest::qWait(100);};
    auto *typography=qmlContext(window)->contextProperty("typography").value<QObject*>();
    const auto point=[&](QQuickItem *control,QPointF p){
        if(!control)return QPoint(-999,-999);
        if(!window->property("threeDActive").toBool())return control->mapToScene(p).toPoint();
        auto *surface=item("mediaSurface"),*view=item("player3DView");if(!surface||!view)return QPoint(-999,-999);
        const auto local=control->mapToItem(surface,p);QVariant result;
        if(control->objectName()=="recorderVolumeKnob")QMetaObject::invokeMethod(view,"projectSurfaceDepth",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,local.x()),Q_ARG(QVariant,local.y()),Q_ARG(QVariant,-9.));
        else QMetaObject::invokeMethod(view,"projectSurface",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,local.x()),Q_ARG(QVariant,local.y()));
        return view->mapToScene(result.toPointF()).toPoint();
    };
    const auto click=[&](const char *name){auto *key=item(name);if(!check(key&&key->isEnabled(),qPrintable(QString(name)+" is enabled")))return;QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point(key,QPointF(key->width()/2,key->height()/2)));QTest::qWait(70);};
    const auto move=[&](QPoint p,Qt::KeyboardModifiers modifier=Qt::NoModifier){QMouseEvent e(QEvent::MouseMove,p,window->mapToGlobal(p),Qt::NoButton,Qt::LeftButton,modifier);QGuiApplication::sendEvent(window,&e);QTest::qWait(15);};
    const auto capture=[&](QString name){if(captures.isEmpty())return;QTest::mouseMove(window,QPoint(8,8));QTest::qWait(180);const QImage image=window->grabWindow();check(!image.isNull(),"frame renders");if(!image.isNull()){QDir().mkpath(captures);check(image.save(captures+"/"+name+".png"),"capture saved");}};
    player.setVolume(0);player.setMotion(false);player.setThreeD(false);player.setMiniMode(false);player.setShowPlayerBody(false);player.clear();
    QList<QUrl> files;for(int i=0;i<3;++i){const QString path=temp+QString("/recorder-%1.flac").arg(i);QFile::copy(QStringLiteral(SPUN_SOURCE_DIR "/assets/First-Light.flac"),path);files.append(QUrl::fromLocalFile(path));}
    player.addUrls(files,false);if(!check(until([&]{return !player.busy()&&player.count()==3;}),"local fixtures import"))return 2;
    player.select(0,false);player.setMedium("tp7");
    {Player restored(temp+"/player.ini");check(restored.medium()=="tp7","appearance persists across relaunch");}
    RecorderRemote remote;const auto original=window->property("ciderService");window->setProperty("ciderService",QVariant::fromValue(static_cast<QObject*>(&remote)));
    const bool supports=qmlContext(window)->contextProperty("supports3D").toBool();
    for(bool threeD:{false,true}) {
        if(threeD&&!supports)continue;
        player.setThreeD(threeD);check(until([&]{return window->property("threeDActive").toBool()==threeD;}),"requested renderer loads");
        if(!pressureOnly)for(bool cider:{false,true})for(double scale:{.85,1.,1.5}) {
            context=QString("TP-7 %1 %2 %3").arg(threeD?"3D":"2D",cider?"Cider":"Local").arg(scale);
            window->setProperty("useCider",cider);typography->setProperty("uiScale",scale);QTest::qWait(160);
            player.pause();player.select(0,false);remote.running=false;remote.index=0;remote.seek(0);emit remote.playingChanged();
            auto position=[&]{return cider?remote.at:player.position();};auto duration=[&]{return cider?remote.length():player.duration();};
            auto seek=[&](qint64 at){if(cider)remote.seek(at);else player.seek(at);QTest::qWait(35);};
            click("recorderPlay");check(until([&]{return cider?remote.running:player.playing();}),"hardware play starts playback");
            click("recorderPlay");check(!(cider?remote.running:player.playing()),"hardware pause stops playback");
            auto *playButton=item("recorderPlay");
            const QPoint playCenter=point(playButton,QPointF(playButton->width()/2,playButton->height()/2));
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,playCenter);
            check(!(cider?remote.running:player.playing()),"press waits for release before activating");
            check(std::abs(playButton->property("pressDepth").toDouble()-1.)<.001,"reduced motion applies the pressed state immediately");
            move(point(playButton,QPointF(-40,playButton->height()/2)));
            QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,point(playButton,QPointF(-40,playButton->height()/2)));
            check(!(cider?remote.running:player.playing())&&!playButton->property("held").toBool(),"dragging off a key cancels its action and pressed state");
            check(std::abs(playButton->property("pressDepth").toDouble())<.001,"reduced motion clears pressure without a spring");
            playButton->forceActiveFocus();QTest::keyClick(window,Qt::Key_Return);
            check(until([&]{return cider?remote.running:player.playing();}),"Enter activates the focused transport key");
            QTest::keyClick(window,Qt::Key_Space);QTest::qWait(60);
            check(!(cider?remote.running:player.playing()),"Space toggles playback once");
            click("recorderNext");check(until([&]{return (cider?remote.index:player.currentIndex())==1;}),"hardware next changes track");
            player.pause();seek(0);click("recorderPrevious");check(until([&]{return (cider?remote.index:player.currentIndex())==0;}),"hardware previous changes track");player.pause();
            const double volume=cider?remote.level:player.volume();click("recorderLouder");check(std::abs((cider?remote.level:player.volume())-qMin(1.,volume+.05))<.001,"plus raises volume");click("recorderQuieter");check(std::abs((cider?remote.level:player.volume())-volume)<.001,"minus lowers volume");
            if(cider){remote.level=1.;emit remote.volumeChanged();}else player.setVolume(1.);
            click("recorderLouder");check((cider?remote.level:player.volume())==1.,"volume cannot exceed its maximum");
            if(cider){remote.level=0.;emit remote.volumeChanged();}else player.setVolume(0.);
            click("recorderQuieter");check((cider?remote.level:player.volume())==0.,"volume cannot fall below zero");
            seek(0);click("recorderForward");check(until([&]{return std::abs(position()-10000)<600;}),"rocker seeks forward ten seconds");click("recorderRewind");check(until([&]{return position()<600;}),"rocker seeks back ten seconds");
            auto *wheel=item("recorderWheel");if(!check(wheel!=nullptr,"wheel control exists"))continue;
            const auto quarter=point(wheel,QPointF(232,122)),threeQuarter=point(wheel,QPointF(12,122));
            QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,quarter);check(until([&]{return std::abs(position()-duration()*.25)<600;}),"wheel click seeks to a quarter");
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,quarter);
            for(int step=1;step<=16;++step){const double a=(.5+step/16.)*std::acos(-1.);move(point(wheel,QPointF(122+110*std::sin(a),122-110*std::cos(a))));}
            check(std::abs(window->property("trackVisualProgress").toDouble()-.75)<.025,"wheel and progress share the drag preview");
            QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,threeQuarter);check(until([&]{return std::abs(position()-duration()*.75)<700;}),"wheel drag commits three quarters");
            check(!window->property("threeDGesture").toBool(),"release clears the gesture");
            seek(duration()/2);QTest::mousePress(window,Qt::LeftButton,Qt::ShiftModifier,quarter);move(point(wheel,QPointF(122,232)),Qt::ShiftModifier);QTest::mouseRelease(window,Qt::LeftButton,Qt::ShiftModifier,point(wheel,QPointF(122,232)));
            check(until([&]{return std::abs(position()-duration()*.525)<700;}),"Shift provides precision seeking");
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,quarter);invoke("cancel3DPointer");invoke("cancelSeekPreview");QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,quarter);check(!window->property("seekPreviewActive").toBool(),"cancelling clears the shared preview");
            auto *knob=item("recorderVolumeKnob");const double before=cider?remote.level:player.volume();
            const QPoint bottom=point(knob,QPointF(18,18)),top=point(knob,QPointF(18,-12));QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,bottom);move(top);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,top);
            check((cider?remote.level:player.volume())>before+.12,"knob drag raises volume");
            if(cider){remote.level=.5;emit remote.volumeChanged();}else player.setVolume(0);
            player.setHorizontalSeek(true);QTest::qWait(60);auto *horizontal=item("horizontalSeek");
            if(check(horizontal&&horizontal->isVisible(),"horizontal seek remains available")){
                const auto h=horizontal->mapToScene(QPointF(horizontal->width()*.4,18)).toPoint();QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,h);
                check(window->property("activeSeekControl").value<QObject*>()!=nullptr&&std::abs(item("recorderProgress")->property("progress").toDouble()-window->property("trackVisualProgress").toDouble())<.0001,"horizontal drag updates the recorder progress");QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,h);
            }
            player.setHorizontalSeek(false);
        }
        typography->setProperty("uiScale",1.);window->setProperty("useCider",false);player.pause();QTest::qWait(100);capture(threeD?"tp7-3d":"tp7-2d");
        if(threeD&&!pressureOnly) {
            auto *view=item("player3DView");
            const auto bodyPoint=[&]{QVariant p;QMetaObject::invokeMethod(view,"projectSurface",Q_RETURN_ARG(QVariant,p),Q_ARG(QVariant,200.),Q_ARG(QVariant,130.));return view->mapToScene(p.toPointF()).toPoint();};
            const QPoint start=bodyPoint();QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);move(start+QPoint(185,65));
            check(view->property("orbiting").toBool()&&std::abs(view->property("recorderYaw").toDouble())>80&&std::abs(view->property("recorderPitch").toDouble())>20,"body drag rotates horizontally and vertically");
            QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(185,65));
            check(!window->property("discFlipped").toBool(),"physical rotation does not open the F reverse view");
            view->setProperty("recorderYaw",180.);view->setProperty("recorderPitch",0.);QTest::qWait(150);capture("tp7-3d-back");
            auto *playKey=item("recorderPlay");const auto hidden=point(playKey,QPointF(playKey->width()/2,playKey->height()/2));
            QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,hidden);check(!player.playing(),"front buttons cannot activate through the back");
            QMetaObject::invokeMethod(view,"resetRecorderOrientation");QTest::qWait(100);click("recorderPlay");check(until([&]{return player.playing();}),"front controls work after returning from rotation");player.pause();
        }
        if(threeD) {
            auto *view=item("player3DView");
            for(const auto &angle:QList<QPair<double,double>>{{0.,-45.},{38.,42.}}) {
                view->setProperty("recorderYaw",angle.first);view->setProperty("recorderPitch",angle.second);QTest::qWait(120);
                click("recorderPlay");check(until([&]{return player.playing();}),"transport remains actionable at an inspection angle");player.pause();
                capture(angle.second<0?"tp7-3d-key-mechanism":"tp7-3d-top-detail");
            }
            QMetaObject::invokeMethod(view,"resetRecorderOrientation");
        }
        if(!pressureOnly)for(bool cider:{false,true}) {
            window->setProperty("useCider",cider);QTest::qWait(80);
            // Side controls use their actual physical depth, not the front plane.
            const auto sideClick=[&](const char *name,int index){
                if(!threeD){click(name);return;}
                auto *view=item("player3DView");view->setProperty("recorderYaw",-75.);QTest::qWait(120);QVariant result;
                QMetaObject::invokeMethod(view,"projectRecorderControl",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,index));
                QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,view->mapToScene(result.toPointF()).toPoint());QTest::qWait(100);
            };
            sideClick("recorderQueue",9);check(window->property("queueOpen").toBool(),"side key opens the queue");window->setProperty("queueOpen",false);QTest::qWait(100);
            sideClick("recorderSettings",10);check(window->property("menuOpen").toBool(),"side key opens settings");QTest::keyClick(window,Qt::Key_Escape);QTest::qWait(160);
            sideClick("recorderDetails",8);check(window->property("discFlipped").toBool(),"side key opens album details");if(window->property("discFlipped").toBool())invoke("flipDisc");
            if(threeD){auto *view=item("player3DView");if(view)QMetaObject::invokeMethod(view,"resetRecorderOrientation");}
        }
        window->setProperty("useCider",false);
        player.setMotion(true);player.pause();
        context=threeD?"TP-7 3D pressure":"TP-7 2D pressure";
        for(const auto &entry:QList<QPair<QString,int>>{{"recorderPlay",1},{"recorderQuieter",3},{"recorderForward",6},{"recorderQueue",9}}) {
            auto *key=item(entry.first);QPoint at;
            if(threeD){auto *view=item("player3DView");view->setProperty("recorderYaw",entry.second>=8?-75.:0.);QTest::qWait(80);QVariant p;QMetaObject::invokeMethod(view,"projectRecorderControl",Q_RETURN_ARG(QVariant,p),Q_ARG(QVariant,entry.second));at=view->mapToScene(p.toPointF()).toPoint();}
            else at=point(key,QPointF(key->width()/2,key->height()/2));
            const QImage resting=window->grabWindow();QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,at);
            check(key->property("held").toBool()&&key->property("armed").toBool(),"press reaches the physical button face");
            // Software 3D frames can be much slower than the animation clock.
            // Wait for the rendered state instead of treating CPU render time as input latency.
            until([&]{return key->property("pressDepth").toDouble()>.9;});
            const double depth=key->property("pressDepth").toDouble();
            std::cout<<"PRESSURE "<<entry.first.toStdString()<<" depth "<<depth<<std::endl;
            check(depth>.65&&depth<1.15,qPrintable(entry.first+" develops bounded physical travel"));
            const QImage pressed=window->grabWindow();check(!pressed.isNull()&&pressed!=resting,"button travel changes the rendered frame");
            if(entry.second==1&&!captures.isEmpty())pressed.save(captures+(threeD?"/tp7-3d-pressed.png":"/tp7-2d-pressed.png"));
            move(QPoint(8,8));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,QPoint(8,8));
            check(until([&]{return std::abs(key->property("pressDepth").toDouble())<.01;}),"released button settles back into its housing");
        }
        player.setMotion(false);
        if(threeD)QMetaObject::invokeMethod(item("player3DView"),"resetRecorderOrientation");
        if(pressureOnly)continue;
        invoke("flipDisc");check(window->property("discFlipped").toBool()&&!window->property("threeDActive").toBool(),"reverse opens album details");capture(threeD?"tp7-3d-reverse":"tp7-2d-reverse");invoke("flipDisc");
        player.setMiniMode(true);QTest::qWait(150);check(!window->property("threeDActive").toBool()&&item("recorderFace"),"Mini uses the compact recorder");click("recorderPlay");check(until([&]{return player.playing();}),"Mini hardware controls remain actionable");player.pause();capture(threeD?"tp7-mini-from3d":"tp7-mini");player.setMiniMode(false);QTest::qWait(160);
        for(const QString &medium:QStringList{"cd","vinyl","cassette","tp7"}){player.setMedium(medium);QTest::qWait(60);check(player.medium()==medium,"all appearances can be selected");}
        player.setMotion(true);player.play();QTest::qWait(120);const double angle=window->property("spinAngle").toDouble();check(until([&]{return window->property("spinAngle").toDouble()!=angle;}),"wheel rotates during playback");player.setMotion(false);const double still=window->property("spinAngle").toDouble();QTest::qWait(100);check(window->property("spinAngle").toDouble()==still,"reduced motion freezes the wheel");player.pause();
    }
    player.clear();player.setMedium("tp7");
    for(bool threeD:{false,true}) {
        if(threeD&&!supports)continue;
        player.setThreeD(threeD);QTest::qWait(150);context=threeD?"TP-7 3D empty library":"TP-7 2D empty library";
        for(const char *name:{"recorderPlay","recorderPrevious","recorderNext","recorderForward","recorderRewind","recorderDetails"})
            check(item(name)&&!item(name)->isEnabled(),qPrintable(QString(name)+" is disabled without a track"));
        player.setVolume(.5);click("recorderLouder");check(std::abs(player.volume()-.55)<.001,"volume stays available without a track");
        if(threeD){auto *view=item("player3DView");view->setProperty("recorderYaw",-75.);QTest::qWait(100);QVariant p;QMetaObject::invokeMethod(view,"projectRecorderControl",Q_RETURN_ARG(QVariant,p),Q_ARG(QVariant,10));QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,view->mapToScene(p.toPointF()).toPoint());}
        else click("recorderSettings");
        check(until([&]{return window->property("menuOpen").toBool();}),"player menu stays available without a track");QTest::keyClick(window,Qt::Key_Escape);QTest::qWait(100);
    }
    window->setProperty("useCider",false);window->setProperty("ciderService",original);player.setThreeD(false);player.setMedium("cd");
    std::cout<<"RECORDER RESULT "<<checks<<" checks, "<<failures<<" failures"<<std::endl;return failures?1:0;
}
#include "recordertest.moc"
