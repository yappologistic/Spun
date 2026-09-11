#include "testcapture.h"
#include "cassettedecktest.h"
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

class CassetteRemote : public Cider {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY trackChanged)
    Q_PROPERTY(bool canSeek MEMBER seekable NOTIFY trackChanged)
    Q_PROPERTY(bool playing MEMBER running NOTIFY playingChanged)
    Q_PROPERTY(qint64 position MEMBER at NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY trackChanged)
    Q_PROPERTY(QString trackKey READ key NOTIFY trackChanged)
    Q_PROPERTY(double volume MEMBER level NOTIFY volumeChanged)
public:
    CassetteRemote():Cider(false){}
    int count() const {return 1;} qint64 duration() const {return 32000;}
    QString key() const {return "cassette-fixture";}
    Q_INVOKABLE void toggle(){running=!running;emit playingChanged();}
    Q_INVOKABLE void pause(){running=false;emit playingChanged();}
    Q_INVOKABLE void seek(qint64 value){at=qBound<qint64>(0,value,duration());emit positionChanged();}
    bool running=false,seekable=true;qint64 at=0;double level=.5;
};

int exerciseCassetteDeck(Player &player,QQuickWindow *window,const QString &temp,const QString &captures) {
    int checks=0,failures=0;QString context;
    const auto check=[&](bool ok,const char *name){++checks;if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<context.toStdString()<<": "<<name<<std::endl;return ok;};
    const auto until=[](const std::function<bool()> &f){QElapsedTimer t;t.start();while(!f()&&t.elapsed()<3000)QTest::qWait(20);return f();};
    std::function<QQuickItem*(QQuickItem*,QString)> find=[&](QQuickItem *p,QString name)->QQuickItem*{if(p->objectName()==name)return p;for(auto *c:p->childItems())if(auto *result=find(c,name))return result;return nullptr;};
    const auto item=[&](QString n){return find(window->contentItem(),n);};
    const auto invoke=[](QObject *o,const char *method){return QMetaObject::invokeMethod(o,method);};
    const auto capture=[&](QString name){if(captures.isEmpty())return;QTest::mouseMove(window,{3,3});QTest::qWait(130);QDir().mkpath(captures);check(captureTestWindow(window).save(captures+"/"+name+".png"),"rendered frame saved");};
    const auto move=[&](QPoint p){QMouseEvent e(QEvent::MouseMove,p,window->mapToGlobal(p),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QGuiApplication::sendEvent(window,&e);QTest::qWait(20);};
    auto *typography=qmlContext(window)->contextProperty("typography").value<QObject*>();
    player.setVolume(0);player.setMotion(false);player.setMiniMode(false);player.setThreeD(false);player.setMedium("cassette");player.setShowPlayerBody(true);player.clear();window->setProperty("useCider",false);window->setColor(QColor("#242a30"));
    const QString path=temp+"/cassette.flac";QFile::copy(QStringLiteral(SPUN_SOURCE_DIR "/assets/First-Light.flac"),path);player.addUrls({QUrl::fromLocalFile(path)},false);
    if(!check(until([&]{return !player.busy()&&player.count()==1;}),"local fixture imports"))return 2;
    player.select(0,false);
    CassetteRemote remote;const auto original=window->property("ciderService");window->setProperty("ciderService",QVariant::fromValue(static_cast<QObject*>(&remote)));
    const bool supports=qmlContext(window)->contextProperty("supports3D").toBool();
    for(bool threeD:{false,true}) {
        if((threeD&&!supports)||(!threeD&&qEnvironmentVariableIsSet("SPUN_TEST_3D_ONLY")))continue;
        player.setThreeD(threeD);QTest::qWait(180);
        auto *controls=item("cassetteControls");auto *view=item("player3DView");
        if(!check(controls&&(!threeD||(view&&window->property("threeDActive").toBool())),"cassette controls and renderer load"))return 2;
        if(threeD)capture("cassette-3d-initial");
        if(qEnvironmentVariableIsSet("SPUN_TEST_CAPTURE_ONLY")) {
            if(threeD) {
                for(const QString &finish:{QString("clear"),QString("smoke"),QString("cream")}) {player.setCassetteFinish(finish);capture("cassette-"+finish);}
                controls->setProperty("doorOpen",true);capture("cassette-open");controls->setProperty("doorOpen",false);
                view->setProperty("cassetteYaw",180.);view->setProperty("cassettePitch",0.);capture("cassette-back");
            }
            continue;
        }
        const auto point=[&](int index){
            if(threeD){QVariant r;QMetaObject::invokeMethod(view,"projectCassetteControl",Q_RETURN_ARG(QVariant,r),Q_ARG(QVariant,index));return view->mapToScene(r.toPointF()).toPoint();}
            auto *key=item(QStringList{"cassettePlay","cassetteRewind","cassetteForward","cassetteStop","cassetteVolume","cassetteLatch"}[index]);return key->mapToScene(QPointF(key->width()/2,key->height()/2)).toPoint();
        };
        const auto click=[&](int index){
            const auto at=point(index);
            if(threeD) { const auto local=view->mapFromScene(at);QVariant hit;QMetaObject::invokeMethod(view,"cassetteHit",Q_RETURN_ARG(QVariant,hit),Q_ARG(QVariant,local.x()),Q_ARG(QVariant,local.y()));const auto result=hit.toMap();if(result.value("hardware").toInt()!=index)std::cout<<"TARGET "<<index<<" hit "<<result.value("hardware").toInt()<<" at "<<at.x()<<","<<at.y()<<std::endl; }
            QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,at);QTest::qWait(65);
        };
        for(bool cider:{false,true})for(double scale:{1.,1.25}) {
            context=QString("%1 %2 %3").arg(threeD?"3D":"2D",cider?"Cider":"local").arg(scale);
            if(threeD)invoke(view,"resetCassetteOrientation");
            controls->setProperty("doorOpen",false);
            typography->setProperty("uiScale",scale);window->setProperty("useCider",cider);player.pause();remote.pause();QTest::qWait(100);
            const auto playing=[&]{return cider?remote.running:player.playing();};
            const auto position=[&]{return cider?remote.at:player.position();};
            const auto level=[&]{return cider?remote.level:player.volume();};
            click(0);check(until(playing),"play starts playback");click(0);check(!playing(),"play pauses playback");
            if(cider)remote.seek(0);else player.seek(0);
            click(2);check(until([&]{return std::abs(position()-10000)<800;}),"forward seeks ten seconds");click(1);check(until([&]{return position()<800;}),"rewind clamps at the start");
            click(0);check(until(playing),"play settles before testing stop");click(3);check(!playing()&&!controls->property("doorOpen").toBool(),"stop pauses before ejecting");
            click(3);check(controls->property("doorOpen").toBool(),"second stop opens the door");
            click(0);check(until(playing)&&!controls->property("doorOpen").toBool(),"play closes the door and starts immediately");
            click(5);check(!playing()&&controls->property("doorOpen").toBool(),"door latch pauses transport and opens");click(5);check(!controls->property("doorOpen").toBool(),"door latch closes without resuming");
            const auto p=point(0);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,p);
            check(controls->property("pressedControl").toInt()==0&&!playing(),"button action waits for release");
            check(item("cassettePlay")->property("pressure").toDouble()>.95,"button depresses with reduced motion");
            move({4,4});QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,{4,4});check(!playing()&&controls->property("pressedControl").toInt()==-1,"dragging off cancels pressure and activation");
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,point(0));QTest::keyClick(window,Qt::Key_Escape);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,point(0));check(!playing()&&controls->property("pressedControl").toInt()==-1,"Escape cancels hardware input");
            if(cider){remote.level=.5;emit remote.volumeChanged();}else player.setVolume(.5);
            const auto knob=point(4);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,knob);move(knob-QPoint(0,35));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,knob-QPoint(0,35));check(level()>.6&&level()<=1,"side knob controls the active source volume");
            if(!threeD){auto *key=item("cassettePlay");key->forceActiveFocus();QTest::keyClick(window,Qt::Key_Return);check(until(playing),"keyboard activates transport");click(0);}
            auto *slider=item("cassetteSeek");const auto seekPoint=[&](double fraction){const auto local=slider->mapToItem(item("mediaSurface"),{slider->width()*fraction,slider->height()/2});if(!threeD)return slider->mapToScene({slider->width()*fraction,slider->height()/2}).toPoint();QVariant r;QMetaObject::invokeMethod(view,"projectSurface",Q_RETURN_ARG(QVariant,r),Q_ARG(QVariant,local.x()),Q_ARG(QVariant,local.y()));return view->mapToScene(r.toPointF()).toPoint();};
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,seekPoint(.2));move(seekPoint(.7));check(slider->property("scrubbing").toBool()&&window->property("cassetteVisualProgress").toDouble()>.65,"progress drag previews the same tape position");QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,seekPoint(.7));check(until([&]{return position()>20000;}),"progress drag commits to playback");
            if(cider){remote.seekable=false;emit remote.trackChanged();check(!item("cassetteForward")->isEnabled()&&!slider->isEnabled(),"unseekable source disables seeking");remote.seekable=true;emit remote.trackChanged();}
        }
        typography->setProperty("uiScale",1.);window->setProperty("useCider",false);player.pause();player.setVolume(0);QTest::qWait(100);
        if(threeD) {
            invoke(view,"resetCassetteOrientation");controls->setProperty("doorOpen",false);
            for(const QString &finish:{QString("clear"),QString("smoke"),QString("cream")}) {player.setCassetteFinish(finish);QTest::qWait(100);capture("cassette-3d-"+finish);}
            player.setCassetteFinish("clear");
            const auto closedCamera=view->property("cassetteCamera");
            click(5);capture("cassette-3d-door-open");
            QVariant doorBounds;QMetaObject::invokeMethod(view,"lidBounds",Q_RETURN_ARG(QVariant,doorBounds));
            check(QRectF(0,0,view->width(),view->height()).contains(doorBounds.toRectF()),"open door remains inside the viewport");
            check(view->property("cassetteCamera")==closedCamera,"door travel keeps the camera steady");click(5);
            view->setProperty("cassetteYaw",180.);view->setProperty("cassettePitch",0.);QTest::qWait(100);capture("cassette-3d-back");
            view->setProperty("cassetteYaw",-45.);view->setProperty("cassettePitch",15.);capture("cassette-3d-side");invoke(view,"resetCassetteOrientation");
            const QPoint from=view->mapToScene({view->width()/2,view->height()/2}).toPoint();QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);move(from+QPoint(60,25));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,from+QPoint(60,25));check(std::abs(view->property("cassetteYaw").toDouble())>15&&std::abs(view->property("cassettePitch").toDouble())>5,"body drag rotates on both axes");check(!window->property("discFlipped").toBool(),"rotation leaves reverse details separate");invoke(view,"resetCassetteOrientation");
            player.setMotion(true);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,point(0));check(until([&]{return item("cassettePlay")->property("pressure").toDouble()>.9;}),"animated button pressure reaches its stop");capture("cassette-3d-key-held");move({4,4});QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,{4,4});check(until([&]{return item("cassettePlay")->property("pressure").toDouble()<.01;}),"button spring settles after release");player.setMotion(false);
        }else capture("cassette-2d");
        std::cout<<"CHECK scene reverse lifecycle"<<std::endl;
        invoke(window,"flipDisc");check(window->property("discFlipped").toBool()&&!window->property("threeDActive").toBool(),"F reverse remains readable and separate");
        QTest::qWait(120);invoke(window,"flipDisc");QTest::qWait(120);
        std::cout<<"CHECK Mini lifecycle"<<std::endl;
        player.setMiniMode(true);QTest::qWait(100);check(!window->property("threeDActive").toBool()&&!item("cassetteControls"),"Mini keeps the compact cassette");player.setMiniMode(false);QTest::qWait(120);
        for(const QString &medium:QStringList{"tp7","vinyl","cd","cassette"}){player.setMedium(medium);QTest::qWait(100);check(player.medium()==medium,"switching medium releases the cassette scene safely");}
    }
    window->setProperty("ciderService",original);window->setProperty("useCider",false);player.setThreeD(false);player.clear();QTest::qWait(100);
    check(!item("cassettePlay")->isEnabled()&&item("cassetteVolume")->isEnabled()&&item("cassetteStop")->isEnabled(),"empty player keeps only volume and door controls available");
    std::cout<<"CASSETTE DECK RESULT "<<checks<<" checks, "<<failures<<" failures"<<std::endl;return failures?1:0;
}
#include "cassettedecktest.moc"
