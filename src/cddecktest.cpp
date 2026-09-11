#include "testcapture.h"
#include "cddecktest.h"
#include "player.h"
#include "cider.h"
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QDir>
#include <QFile>
#include <functional>
#include <iostream>
#include <cmath>

class CdRemote : public Cider {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY trackChanged)
    Q_PROPERTY(bool canSeek MEMBER seekable NOTIFY trackChanged)
    Q_PROPERTY(bool playing MEMBER running NOTIFY playingChanged)
    Q_PROPERTY(qint64 position MEMBER at NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY trackChanged)
    Q_PROPERTY(QString trackKey READ key NOTIFY trackChanged)
    Q_PROPERTY(double volume MEMBER level NOTIFY volumeChanged)
    Q_PROPERTY(bool shuffle MEMBER shuffled NOTIFY settingsChanged)
    Q_PROPERTY(int repeatMode MEMBER repeat NOTIFY settingsChanged)
public:
    CdRemote():Cider(false){}
    int count()const{return 3;} qint64 duration()const{return 32000;}
    QString key()const{return QString("cd-fixture-%1").arg(track);}
    Q_INVOKABLE void toggle(){running=!running;emit playingChanged();}
    Q_INVOKABLE void pause(){running=false;emit playingChanged();}
    Q_INVOKABLE void seek(qint64 value){at=qBound<qint64>(0,value,duration());emit positionChanged();}
    Q_INVOKABLE void previous(){track=qMax(0,track-1);at=0;emit trackChanged();emit positionChanged();}
    Q_INVOKABLE void next(){track=qMin(2,track+1);at=0;emit trackChanged();emit positionChanged();}
    bool running=false,seekable=true,shuffled=false;int track=0,repeat=0;qint64 at=0;double level=.5;
};

int exerciseCdDeck(Player &player,QQuickWindow *window,const QString &temp,const QString &captures) {
    int checks=0,failures=0;QString context="setup";
    const auto check=[&](bool ok,const char *name){++checks;if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<context.toStdString()<<": "<<name<<std::endl;return ok;};
    const auto until=[](const std::function<bool()> &f){QElapsedTimer t;t.start();while(!f()&&t.elapsed()<3000)QTest::qWait(20);return f();};
    std::function<QQuickItem*(QQuickItem*,QString)> find=[&](QQuickItem *p,QString n)->QQuickItem*{if(p->objectName()==n)return p;for(auto *c:p->childItems())if(auto *r=find(c,n))return r;return nullptr;};
    const auto item=[&](QString n){return find(window->contentItem(),n);};
    const auto invoke=[](QObject *o,const char *m){return QMetaObject::invokeMethod(o,m);};
    const auto capture=[&](QString name){
        if(captures.isEmpty())return;
        QTest::mouseMove(window,{3,3});QTest::qWait(100);QDir().mkpath(captures);
        auto frame=captureTestWindow(window);
        int clean=0;for(int y:{110,160,210,260,310})if(frame.rect().contains(8,y)&&frame.pixelColor(8,y)==QColor("#242a30"))++clean;
        check(clean>=4,"rendered frame has an intact background");
        check(frame.save(captures+"/"+name+".png"),"rendered frame saved");
    };
    const auto move=[&](QPoint p){QMouseEvent e(QEvent::MouseMove,p,window->mapToGlobal(p),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QGuiApplication::sendEvent(window,&e);QTest::qWait(20);};
    auto *typography=qmlContext(window)->contextProperty("typography").value<QObject*>();
    player.setVolume(0);player.setMotion(false);player.setMiniMode(false);player.setThreeD(false);player.setMedium("cd");player.setShowPlayerBody(true);player.setHorizontalSeek(true);player.clear();player.setShuffle(false);player.setRepeatMode(0);window->setProperty("useCider",false);window->setColor(QColor("#242a30"));
    QList<QUrl> files;for(int i=0;i<3;++i){const auto path=temp+QString("/cd-%1.flac").arg(i);QFile::copy(QStringLiteral(SPUN_SOURCE_DIR "/assets/First-Light.flac"),path);files<<QUrl::fromLocalFile(path);}player.addUrls(files,false);
    if(!check(until([&]{return !player.busy()&&player.count()==3;}),"local fixtures import"))return 2;
    player.select(0,false);if(qEnvironmentVariableIsSet("SPUN_TEST_CAPTURE_ONLY"))typography->setProperty("uiScale",1.5);CdRemote remote;const auto original=window->property("ciderService");window->setProperty("ciderService",QVariant::fromValue(static_cast<QObject*>(&remote)));
    const bool supports=qmlContext(window)->contextProperty("supports3D").toBool();
    for(bool threeD:{false,true}) {
        if((threeD&&!supports)||(!threeD&&qEnvironmentVariableIsSet("SPUN_TEST_3D_ONLY")))continue;
        player.setThreeD(threeD);QTest::qWait(150);
        auto *controls=item("cdControls"),*view=item("player3DView");
        if(!check(controls&&(!threeD||(view&&window->property("threeDActive").toBool())),"CD controls and renderer load"))return 2;
        if(threeD)capture("cd-3d-initial");
        if(qEnvironmentVariableIsSet("SPUN_TEST_CAPTURE_ONLY")){
            capture(threeD?"cd-3d":"cd-2d");
            if(threeD){controls->setProperty("doorOpen",true);QTest::qWait(100);capture("cd-open");controls->setProperty("doorOpen",false);view->setProperty("cdYaw",160.);QTest::qWait(100);capture("cd-back");view->setProperty("cdYaw",45.);view->setProperty("cdPitch",18.);QTest::qWait(100);capture("cd-side");}
            continue;
        }
        const QStringList keys={"cdPlay","cdStop","cdSettings","cdPrevious","cdNext","cdMute","cdVolume","cdLatch","cdShuffle","cdRepeat"};
        const auto point=[&](int index){if(threeD){QVariant r;QMetaObject::invokeMethod(view,"projectCdControl",Q_RETURN_ARG(QVariant,r),Q_ARG(QVariant,index));return view->mapToScene(r.toPointF()).toPoint();}auto *key=item(keys[index]);return key->mapToScene({key->width()/2,key->height()/2}).toPoint();};
        const auto click=[&](int index){const auto p=point(index);if(threeD){QVariant r;const auto local=view->mapFromScene(p);QMetaObject::invokeMethod(view,"cdHit",Q_RETURN_ARG(QVariant,r),Q_ARG(QVariant,local.x()),Q_ARG(QVariant,local.y()));if(r.toMap().value("hardware").toInt()!=index)std::cout<<"TARGET "<<index<<" hit "<<r.toMap().value("hardware").toInt()<<" at "<<p.x()<<","<<p.y()<<std::endl;}QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,p);QTest::qWait(50);};
        for(bool cider:{false,true})for(double scale:{1.,1.25}) {
            context=QString("%1 %2 %3").arg(threeD?"3D":"2D",cider?"Cider":"local").arg(scale);
            if(threeD)invoke(view,"resetCdOrientation");
            controls->setProperty("doorOpen",false);typography->setProperty("uiScale",scale);window->setProperty("useCider",cider);player.select(0,false);remote.track=0;remote.pause();QTest::qWait(100);
            const auto playing=[&]{return cider?remote.running:player.playing();};const auto position=[&]{return cider?remote.at:player.position();};const auto level=[&]{return cider?remote.level:player.volume();};const auto track=[&]{return cider?remote.track:player.currentIndex();};
            click(0);check(until(playing),"play starts the active source");click(0);check(!playing(),"play pauses the active source");
            click(4);check(until([&]{return track()==1;}),"next selects the next track");if(cider)remote.seek(0);else player.seek(0);click(3);check(until([&]{return track()==0;}),"previous selects the preceding track");
            if(cider)remote.seek(7000);else player.seek(7000);click(1);check(!playing()&&until([&]{return position()<600;}),"stop pauses and returns to the start");
            click(0);check(until(playing),"play resumes before opening lid");click(7);check(!playing()&&controls->property("doorOpen").toBool(),"latch pauses and opens the glass lid");click(7);check(!controls->property("doorOpen").toBool()&&!playing(),"closing lid does not resume playback");
            click(7);click(0);check(until(playing)&&!controls->property("doorOpen").toBool(),"play closes an open lid");click(1);
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,point(0));check(controls->property("pressedControl").toInt()==0&&!playing(),"action waits for release");check(item("cdPlay")->property("pressure").toDouble()>.95,"button depresses with reduced motion");move({3,3});QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,{3,3});check(!playing()&&controls->property("pressedControl").toInt()==-1,"drag off cancels the button");
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,point(0));QTest::keyClick(window,Qt::Key_Escape);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,point(0));check(!playing()&&controls->property("pressedControl").toInt()==-1,"Escape clears button pressure");
            if(cider){remote.level=.5;emit remote.volumeChanged();}else player.setVolume(.5);
            const auto knob=point(6);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,knob);move(knob-QPoint(0,35));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,knob-QPoint(0,35));check(level()>.6&&level()<=1,"knob adjusts the active source volume");const auto before=level();click(5);check(level()==0,"mute silences the active source");click(5);check(std::abs(level()-before)<.02,"mute restores the previous volume");
            click(8);check(cider?remote.shuffled:player.shuffle(),"shuffle selector enables shuffle");click(8);check(!(cider?remote.shuffled:player.shuffle()),"shuffle selector disables shuffle");for(int r=1;r<=3;++r){click(9);check((cider?remote.repeat:player.repeatMode())==r%3,"repeat selector cycles all modes");}
            click(2);check(window->property("menuOpen").toBool(),"settings key opens settings");QTest::keyClick(window,Qt::Key_Escape);QTest::qWait(70);
            auto *rim=item("scrubber");const auto seekPoint=[&](double fraction){const double a=fraction*2*std::acos(-1.);const QPointF ring(220+216*std::sin(a),220-216*std::cos(a));if(!threeD)return rim->mapToScene(ring).toPoint();const auto local=rim->mapToItem(item("mediaSurface"),ring);QVariant r;QMetaObject::invokeMethod(view,"projectSurface",Q_RETURN_ARG(QVariant,r),Q_ARG(QVariant,local.x()),Q_ARG(QVariant,local.y()));return view->mapToScene(r.toPointF()).toPoint();};
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,seekPoint(.25));move(seekPoint(.5));check(rim->property("scrubbing").toBool()&&std::abs(window->property("recordVisualProgress").toDouble()-.5)<.025,"rim drag previews the shared playback position");check(std::abs(item("horizontalSeek")->property("value").toDouble()-.5)<.025,"horizontal progress follows the rim preview");QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,seekPoint(.5));check(until([&]{return std::abs(position()-(cider?remote.duration():player.duration())*.5)<900;}),"rim drag commits the same position");
            if(cider){remote.seekable=false;emit remote.trackChanged();check(!rim->property("canSeek").toBool(),"unseekable source disables the rim");remote.seekable=true;emit remote.trackChanged();}
            if(!threeD){auto *key=item("cdPlay");key->forceActiveFocus();QTest::keyClick(window,Qt::Key_Return);check(until(playing),"keyboard activates the hardware button");click(1);}
        }
        typography->setProperty("uiScale",1.);window->setProperty("useCider",false);player.pause();player.setVolume(0);QTest::qWait(100);
        if(threeD){
            invoke(view,"resetCdOrientation");const auto camera=view->property("cdCamera");click(7);capture("cd-3d-lid-open");QVariant bounds;QMetaObject::invokeMethod(view,"lidBounds",Q_RETURN_ARG(QVariant,bounds));check(QRectF(0,0,view->width(),view->height()).contains(bounds.toRectF()),"open lid fits inside viewport");check(view->property("cdCamera")==camera,"lid opening keeps camera fixed");click(7);
            view->setProperty("cdYaw",160.);capture("cd-3d-back");view->setProperty("cdYaw",45.);view->setProperty("cdPitch",18.);capture("cd-3d-side");invoke(view,"resetCdOrientation");
            const QPoint from=view->mapToScene({view->width()/2,view->height()/2}).toPoint();QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);move(from+QPoint(65,30));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,from+QPoint(65,30));check(std::abs(view->property("cdYaw").toDouble())>15&&std::abs(view->property("cdPitch").toDouble())>5,"housing drag rotates on both axes");check(!window->property("discFlipped").toBool(),"rotation is separate from F reverse");invoke(view,"resetCdOrientation");
            player.setMotion(true);QTest::qWait(160);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,point(0));check(until([&]{return item("cdPlay")->property("pressure").toDouble()>.9;}),"spring reaches button stop");move({3,3});QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,{3,3});check(until([&]{return item("cdPlay")->property("pressure").toDouble()<.01;}),"spring settles on release");
            controls->setProperty("doorOpen",true);QTest::qWait(400);
            auto *presenter=qmlContext(window)->contextProperty("presentation").value<QObject*>();
            invoke(presenter,"swapRequested");QTest::qWait(40);
            check(window->property("swapRunning").toBool(),"album exchange starts with an already open lid");
            bool stayedOpen=true;QElapsedTimer exchange;exchange.start();
            while(window->property("swapRunning").toBool()&&exchange.elapsed()<2500){stayedOpen&=controls->property("doorAngle").toDouble()>69.;QTest::qWait(20);}
            check(stayedOpen&&!window->property("swapRunning").toBool()&&controls->property("doorOpen").toBool(),"album exchange keeps a manually opened lid raised");
            controls->setProperty("doorOpen",false);player.setMotion(false);
        }else capture("cd-2d");
        invoke(window,"flipDisc");check(window->property("discFlipped").toBool()&&!window->property("threeDActive").toBool(),"reverse stays readable");QTest::qWait(100);invoke(window,"flipDisc");QTest::qWait(100);player.setMiniMode(true);QTest::qWait(100);check(!window->property("threeDActive").toBool()&&!item("cdControls"),"Mini unloads the detailed hardware");player.setMiniMode(false);QTest::qWait(100);
        for(const QString &medium:QStringList{"tp7","vinyl","cassette","cd"}){player.setMedium(medium);QTest::qWait(100);check(player.medium()==medium,"medium switch releases and restores the CD assembly");}
    }
    window->setProperty("ciderService",original);window->setProperty("useCider",false);if(qEnvironmentVariableIsSet("SPUN_TEST_CAPTURE_ONLY"))return failures?1:0;player.clear();QTest::qWait(100);capture("cd-empty");check(!item("cdPlay")->isEnabled()&&item("cdVolume")->isEnabled()&&item("cdLatch")->isEnabled(),"empty transport keeps volume and lid available");
    player.setThreeD(false);std::cout<<"CD DECK RESULT "<<checks<<" checks, "<<failures<<" failures"<<std::endl;return failures?1:0;
}
#include "cddecktest.moc"
