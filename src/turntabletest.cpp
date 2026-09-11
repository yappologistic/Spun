#include "testcapture.h"
#include "turntabletest.h"
#include "player.h"
#include "cider.h"
#include "listening.h"
#include "disc.h"
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QDir>
#include <QFile>
#include <functional>
#include <iostream>
#include <cmath>
#include <taglib/fileref.h>
#include <taglib/tag.h>

class TurntableRemote : public Cider {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY trackChanged)
    Q_PROPERTY(bool canSeek READ canSeek NOTIFY trackChanged)
    Q_PROPERTY(bool playing MEMBER running NOTIFY playingChanged)
    Q_PROPERTY(qint64 position MEMBER at NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ length NOTIFY trackChanged)
    Q_PROPERTY(QString trackKey READ key NOTIFY trackChanged)
    Q_PROPERTY(QVariantMap discDetails READ album NOTIFY discDetailsChanged)
    Q_PROPERTY(bool discLoading READ albumLoading NOTIFY discDetailsChanged)
    Q_PROPERTY(QString discError READ albumError NOTIFY discDetailsChanged)
public:
    TurntableRemote():Cider(false){}
    int count() const {return 1;} bool canSeek() const {return true;}
    qint64 length() const {return albumFixture?QList<qint64>{32000,48000,24000}[selected]:32000;}
    QString key() const {return QString("turntable-fixture-%1").arg(selected);}
    bool albumLoading() const {return false;} QString albumError() const {return {};}
    QVariantMap album() const {
        if(!albumFixture)return {};
        QVariantList tracks;
        for(int i=0;i<3;++i)tracks.append(QVariantMap{{"id",QString::number(101+i)},{"duration",QList<int>{32000,48000,24000}[i]},{"playable",true},{"title",QString("Track %1").arg(i+1)}});
        return {{"tracks",tracks},{"albumId","100"},{"currentId",QString::number(101+selected)}};
    }
    void select(int index,qint64 position=0) {selected=index;at=position;emit trackChanged();emit discDetailsChanged();emit positionChanged();}
    Q_INVOKABLE void play(){running=true;emit playingChanged();}
    Q_INVOKABLE void pause(){running=false;emit playingChanged();}
    Q_INVOKABLE void toggle(){running=!running;emit playingChanged();}
    Q_INVOKABLE void seek(qint64 value){at=qBound<qint64>(0,value,length());emit positionChanged();}
    bool running=false,albumFixture=false;int selected=0;qint64 at=0;
};
// Delayed album changes reproduce remote metadata arriving before the seek tick.
class TurntableListening : public Listening {
    Q_OBJECT
    Q_PROPERTY(bool busy MEMBER pending NOTIFY changed)
public:
    explicit TurntableListening(TurntableRemote *source):Listening(source),remote(source){}
    Q_INVOKABLE void cancelAlbumPosition(){++generation;pending=false;emit changed();}
    Q_INVOKABLE bool playAlbumPosition(const QString &id,const QVariantList &tracks,int index,qint64 position) {
        if(id!="100"||index<0||index>=tracks.size())return false;
        pending=true;emit changed();const auto token=++generation;
        QTimer::singleShot(40,this,[=,this]{if(token!=generation)return;remote->select(index);
            QTimer::singleShot(40,this,[=,this]{if(token!=generation)return;remote->seek(position);remote->play();pending=false;emit changed();});});
        return true;
    }
    TurntableRemote *remote;bool pending=false;int generation=0;
};
int exerciseTurntable(Player &player,QQuickWindow *window,const QString &temp,const QString &captures) {
    int checks=0,failures=0;
    const auto check=[&](bool ok,const char *name){++checks;if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<name<<std::endl;return ok;};
    const auto until=[](const std::function<bool()> &f){QElapsedTimer t;t.start();while(!f()&&t.elapsed()<3500)QTest::qWait(20);return f();};
    std::function<QQuickItem*(QQuickItem*,const QString&)> find=[&](QQuickItem *p,const QString &n)->QQuickItem*{if(p->objectName()==n)return p;for(auto *child:p->childItems())if(auto *v=find(child,n))return v;return nullptr;};
    const auto item=[&](QString name){return find(window->contentItem(),name);};
    const auto object=[&](const char *name){return window->findChild<QObject*>(name);};
    const auto invoke=[&](QObject *o,const char *name){return QMetaObject::invokeMethod(o,name);};
    auto *typography=qmlContext(window)->contextProperty("typography").value<QObject*>();
    player.setVolume(0);player.setMotion(false);player.setThreeD(false);player.setMiniMode(false);player.setMedium("vinyl");player.setVinylAlbumMode(false);player.clear();window->setProperty("useCider",false);
    QList<QUrl> files;
    for(int i=0;i<3;++i){const QString path=temp+QString("/record-%1.flac").arg(i);QFile::copy(QStringLiteral(SPUN_SOURCE_DIR "/assets/First-Light.flac"),path);TagLib::FileRef tag(QFile::encodeName(path).constData());tag.tag()->setAlbum("Turntable fixture");tag.tag()->setTrack(i+1);tag.save();files.append(QUrl::fromLocalFile(path));}
    player.addUrls(files,false);
    check(until([&]{return !player.busy()&&player.count()==3;}),"album fixtures import");
    player.setThreeD(true);
    if(!check(until([&]{return !player.busy()&&player.count()==3&&window->property("threeDActive").toBool();}),"turntable and isolated album load"))return 2;
    player.select(0,false);QTest::qWait(300);
    auto *view=item("player3DView");
    const auto hardware=[&](int index){QVariant r;QMetaObject::invokeMethod(view,"projectVinylControl",Q_RETURN_ARG(QVariant,r),Q_ARG(QVariant,index));return view->mapToScene(r.toPointF()).toPoint();};
    const auto projected=[&](QQuickItem *c,QPointF p){auto *surface=item("mediaSurface");const auto local=c->mapToItem(surface,p);QVariant r;QMetaObject::invokeMethod(view,"projectSurface",Q_RETURN_ARG(QVariant,r),Q_ARG(QVariant,local.x()),Q_ARG(QVariant,local.y()));return view->mapToScene(r.toPointF()).toPoint();};
    const auto move=[&](QPoint p,Qt::KeyboardModifiers m=Qt::NoModifier){QMouseEvent e(QEvent::MouseMove,p,window->mapToGlobal(p),Qt::NoButton,Qt::LeftButton,m);QGuiApplication::sendEvent(window,&e);QTest::qWait(20);};
    const auto clickHardware=[&](int index){QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,hardware(index));QTest::qWait(80);};
    const auto capture=[&](const QString &name){if(captures.isEmpty())return;QTest::mouseMove(window,QPoint(5,5));QTest::qWait(150);QDir().mkpath(captures);check(captureTestWindow(window).save(captures+"/"+name+".png"),"turntable capture saved");};
    capture("vinyl-front");
    if(qEnvironmentVariableIsSet("SPUN_TEST_VISUAL_ONLY")) {
        window->setColor(QColor("#25272c"));
        capture("vinyl-front-solid");
        for(const auto &pose:QList<QPair<double,double>>{{35.,0.},{-40.,10.},{180.,0.}}) {
            view->setProperty("vinylYaw",pose.first);view->setProperty("vinylPitch",pose.second);
            capture(QString("vinyl-solid-%1").arg(pose.first));
        }
        return failures;
    }
    TurntableRemote remote;const auto original=window->property("ciderService");window->setProperty("ciderService",QVariant::fromValue(static_cast<QObject*>(&remote)));
    if(!qEnvironmentVariableIsSet("SPUN_TEST_ALBUM_ONLY"))for(bool cider:{false,true})for(double zoom:{.85,1.,1.5}) {
        window->setProperty("useCider",cider);typography->setProperty("uiScale",zoom);player.setHorizontalSeek(true);QTest::qWait(150);
        player.pause();remote.pause();
        clickHardware(0);check(player.vinylSpeed()==33,"33 button selects 33 and one third RPM");
        clickHardware(1);check(player.vinylSpeed()==45,"45 button selects 45 RPM");
        clickHardware(2);check(until([&]{return cider?remote.running:player.playing();}),"cue lever lowers needle and plays");
        clickHardware(2);check(!(cider?remote.running:player.playing()),"cue lever lifts needle and pauses");
        player.setVinylSpeed(33);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,hardware(1));check(player.vinylSpeed()==33,"speed change waits for release");check(object("vinylSpeedKey1")&&object("vinylSpeedKey1")->property("z").toDouble()<1.,"speed key depresses into its housing");move(QPoint(10,10));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,QPoint(10,10));check(player.vinylSpeed()==33,"dragging off a speed button cancels activation");
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,hardware(1));QTest::keyClick(window,Qt::Key_Escape);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,hardware(1));check(player.vinylSpeed()==33&&!window->property("threeDHardwarePressed").toBool(),"Escape cancels hardware pressure and activation");
        auto *rim=item("scrubber");
        const auto start=projected(rim,{436,220});QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);
        for(int n=1;n<=12;++n){const double a=(.5+n/12.)*M_PI;move(projected(rim,{220+216*std::sin(a),220-216*std::cos(a)}));}
        check(std::abs(window->property("recordVisualProgress").toDouble()-.75)<.025,"ring drag previews three quarters");
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,projected(rim,{4,220}));
        check(until([&]{return std::abs((cider?remote.at:player.position())-(cider?remote.length():player.duration())*.75)<900;}),"ring drag commits the same playback position");
        auto *arm=item("vinylTonearm"),*needle=item("needleHandle");
        const auto tip=projected(arm,needle->property("tip").toPointF());QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,tip);
        check(arm->property("dragging").toBool(),"modeled needle can be grabbed");
        const double a=(arm->property("startAngle").toDouble()+arm->property("angleRange").toDouble()*.5)*M_PI/180;
        const auto mid=projected(arm,{arm->property("pivotX").toDouble()-arm->property("armLength").toDouble()*std::sin(a),arm->property("pivotY").toDouble()+arm->property("armLength").toDouble()*std::cos(a)});
        for(int n=1;n<=8;++n)move(tip+(mid-tip)*n/8);
        check(std::abs(window->property("recordVisualProgress").toDouble()-.5)<.025,"needle and ring share the same preview");
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,mid);
        check(until([&]{return (cider?remote.running:player.playing())&&std::abs((cider?remote.at:player.position())-16000)<1100;}),"needle drop seeks and plays");player.pause();remote.pause();
        auto *slider=item("horizontalSeek");const auto p=slider->mapToScene({slider->width()*.35,18}).toPoint();QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,p);
        check(window->property("seekPreviewActive").toBool()&&std::abs(arm->property("groove").toDouble()-window->property("recordVisualProgress").toDouble())<.001,"horizontal seek also moves the tonearm");QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,p);
    }
    TurntableListening listening(&remote);const auto originalListening=window->property("listeningService");
    window->setProperty("listeningService",QVariant::fromValue(static_cast<QObject*>(&listening)));
    remote.albumFixture=true;remote.select(0);player.setVinylAlbumMode(true);typography->setProperty("uiScale",1.);
    for(bool threeD:{false,true}) {
        player.setThreeD(threeD);QTest::qWait(250);view=item("player3DView");
        for(bool cider:{false,true}) {
            window->setProperty("useCider",cider);player.setHorizontalSeek(true);QTest::qWait(80);
            auto *slider=item("horizontalSeek"),*rim=item("scrubber"),*arm=item("vinylTonearm");
            const auto synced=[&]{return std::abs(slider->property("value").toDouble()-window->property("recordVisualProgress").toDouble())<.001 && std::abs(arm->property("groove").toDouble()-slider->property("value").toDouble())<.001;};
            for(int index:{0,1,2,0}) {
                if(cider)remote.select(index,2000);else {player.select(index,false);player.seek(2000);}
                QTest::qWait(80);
                const double expected=cider?(QList<double>{0,32000,80000}[index]+remote.at)/104000.:(index*32000.+player.position())/96000.;
                if(!check(synced()&&std::abs(slider->property("value").toDouble()-expected)<.01,"song changes keep both bars and the needle on one album timeline"))
                    std::cout<<"timeline cider="<<cider<<" index="<<index<<" expected="<<expected<<" horizontal="<<slider->property("value").toDouble()<<" ring="<<window->property("recordVisualProgress").toDouble()<<" needle="<<arm->property("groove").toDouble()<<" duration="<<window->property("seekDuration").toDouble()<<std::endl;
            }
            const auto p=slider->mapToScene({slider->width()*.9,18}).toPoint();
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,p);const double target=slider->property("previewValue").toDouble();
            check(synced(),"horizontal album drag synchronizes ring and needle");
            QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,p);
            check(until([&]{return synced()&&std::abs(window->property("recordProgress").toDouble()-target)<.02;}),"horizontal album seek reaches the selected album position");
            player.pause();remote.pause();
            const auto ringPoint=[&](QPointF point){return threeD?projected(rim,point):rim->mapToScene(point).toPoint();};
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,ringPoint({436,220}));
            for(int i=1;i<=8;++i){const double a=(.5+i*.08)*M_PI;move(ringPoint({220+216*std::sin(a),220-216*std::cos(a)}));check(synced(),"album ring drag keeps the horizontal bar and needle synchronized");}
            const double ringTarget=rim->property("previewFraction").toDouble();
            QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,ringPoint({220+216*std::sin(1.14*M_PI),220-216*std::cos(1.14*M_PI)}));
            check(until([&]{return synced()&&std::abs(window->property("recordProgress").toDouble()-ringTarget)<.02;}),"ring album seek stays synchronized after delayed track updates");
            player.pause();remote.pause();
        }
    }
    player.setVinylAlbumMode(false);window->setProperty("listeningService",originalListening);
    if(qEnvironmentVariableIsSet("SPUN_TEST_ALBUM_ONLY")) {
        window->setProperty("useCider",false);window->setProperty("ciderService",original);
        std::cout<<"ALBUM RESULT "<<checks<<" checks, "<<failures<<" failures"<<std::endl;return failures?1:0;
    }
    window->setProperty("useCider",false);window->setProperty("ciderService",original);typography->setProperty("uiScale",1.);player.setHorizontalSeek(false);player.pause();
    for(bool running:{false,true}) {
        if(running)player.play();else player.pause();QTest::qWait(90);
        QVariant p;QMetaObject::invokeMethod(view,"projectVinylNeedle",Q_RETURN_ARG(QVariant,p));const auto tip=view->mapToScene(p.toPointF()).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,tip);
        check(item("vinylTonearm")->property("dragging").toBool(),"visible raised or lowered stylus is directly draggable");
        invoke(window,"cancel3DPointer");QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,tip);player.pause();
    }
    auto *arm=item("vinylTonearm");const double inner=(arm->property("startAngle").toDouble()+arm->property("angleRange").toDouble())*M_PI/180;
    const double radius=std::hypot((arm->property("pivotX").toDouble()-220-arm->property("armLength").toDouble()*std::sin(inner))*.86,(arm->property("pivotY").toDouble()-220+arm->property("armLength").toDouble()*std::cos(inner))*.86);
    check(radius>63&&radius<79,"innermost groove keeps the stylus outside the paper label");
    player.setVinylAlbumMode(true);QTest::qWait(150);check(object("threeDTrackBands")&&object("threeDTrackBands")->property("count").toInt()==2,"album mode retains track bands");player.setVinylAlbumMode(false);
    for(const auto &pose:QList<QPair<double,double>>{{35.,0.},{-40.,10.},{180.,0.}}){view->setProperty("vinylYaw",pose.first);view->setProperty("vinylPitch",pose.second);QTest::qWait(150);capture(QString("vinyl-angle-%1").arg(pose.first));}
    invoke(view,"resetVinylOrientation");QTest::qWait(100);
    clickHardware(3);auto *cover=object("vinylDustCover");check(cover&&std::abs(cover->property("eulerRotation").value<QVector3D>().x())<.01,"dust cover closes");capture("vinyl-closed");
    clickHardware(3);check(cover&&object("vinylTurntable")->property("coverOpen").toBool()&&cover->property("eulerRotation").value<QVector3D>().x()<-60,"dust cover opens");
    const auto from=projected(item("scrubber"),{450,410});QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);move(from+QPoint(90,-35));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,from+QPoint(90,-35));check(std::abs(view->property("vinylYaw").toDouble())>10,"housing drag rotates the whole turntable");check(!window->property("discFlipped").toBool(),"inspection remains separate from F reverse");invoke(view,"resetVinylOrientation");
    player.setMotion(true);player.play();const double spin=window->property("spinAngle").toDouble();check(until([&]{return window->property("spinAngle").toDouble()!=spin;}),"record spins during playback");player.setMotion(false);player.pause();
    for(int i=0;i<6;++i){player.setMedium(i%2?"tp7":"cd");QTest::qWait(40);player.setMedium("vinyl");QTest::qWait(80);check(object("threeDRecord")&&object("vinylDustCover"),"turntable survives medium changes");}
    invoke(window,"flipDisc");check(window->property("discFlipped").toBool(),"F reverse opens album details");invoke(window,"flipDisc");player.setMiniMode(true);check(!window->property("threeDActive").toBool(),"Mini falls back to 2D");player.setMiniMode(false);check(until([&]{return window->property("threeDActive").toBool();}),"return from Mini restores turntable");
    capture("vinyl-final");
    std::cout<<"TURNTABLE RESULT "<<checks<<" checks, "<<failures<<" failures"<<std::endl;return failures?1:0;
}
#include "turntabletest.moc"
