#include "performancetest.h"
#include "player.h"
#include "testcapture.h"
#include <QQmlContext>
#include <QScreen>
#include <QSignalSpy>
#include <QMutex>
#include <QMutexLocker>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include <functional>
#include <atomic>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <memory>

namespace {
bool until(const std::function<bool()> &ready, int ms=15000) {
    QElapsedTimer t;t.start();
    while(!ready()&&t.elapsed()<ms)QTest::qWait(10);
    return ready();
}
QJsonObject memory() {
    QJsonObject result;QFile file("/proc/self/smaps_rollup");
    if(file.open(QIODevice::ReadOnly))for(const auto &line:file.readAll().split('\n'))
        for(const auto &key:{QByteArray("Rss"),QByteArray("Pss"),QByteArray("Private_Dirty")})
            if(line.startsWith(key+':'))result[QString::fromLatin1(key)+"KiB"]=line.mid(key.size()+1).simplified().split(' ').first().toInt();
    return result;
}
int verifyMotion(Player &player,QQuickWindow *window) {
    int checks=0,failures=0;
    const auto check=[&](bool ok,const char *label) {
        ++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<label<<std::endl;
    };
    player.setMotion(false);player.setThreeD(false);player.setRepeatMode(2);
    for(const QString medium:{"cd","vinyl","cassette","tp7"})for(bool fast:{false,true}) {
        if(fast&&medium!="cd")continue;
        player.setMedium(medium);player.setCd500Rpm(fast);player.play();
        if(!until([&]{return player.playing();}))return 2;
        const double speed=medium=="tp7"?36:medium=="cd"&&fast?3000:medium=="vinyl"?window->property("vinylDegreesPerSecond").toDouble():9;
        double left=0,right=0;
        for(int hz:{60,120,165,240}) {
            window->setProperty("discFlipped",false);window->setProperty("spinSpeed",speed);
            for(const char *property:{"spinAngle","cassetteLeftAngle","cassetteRightAngle","wavePhase","tapeWindSpeed"})window->setProperty(property,0.);
            for(int i=0;i<hz*2;++i) {
                QVariant result;
                if(!QMetaObject::invokeMethod(window,"advanceMediaFrame",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,1./hz),Q_ARG(QVariant,1000.+i*1000./hz)))return 2;
            }
            const double angle=window->property("spinAngle").toDouble();
            check(std::abs(std::remainder(angle-speed*2,360.))<.001,"media rotation keeps its speed at 60/120/165/240 Hz");
            check(std::abs(window->property("wavePhase").toDouble()-5.6)<.001,"wave motion is independent of refresh rate");
            if(medium=="cassette") {
                if(hz==60){left=window->property("cassetteLeftAngle").toDouble();right=window->property("cassetteRightAngle").toDouble();}
                check(std::abs(window->property("cassetteLeftAngle").toDouble()-left)<.001&&std::abs(window->property("cassetteRightAngle").toDouble()-right)<.001,"both tape reels retain their elapsed-time motion across refresh rates");
            }
        }
        player.pause();
    }
    auto *clock=window->findChild<QObject*>("mediaFrameAnimation");
    check(clock&&!clock->property("running").toBool(),"reduced motion stops the frame callback");
    std::cout<<"MOTION_RESULT "<<checks<<" checks, "<<failures<<" failures"<<std::endl;
    return failures?1:0;
}

}

int exercisePerformance(Player &player,QQuickWindow *window,const QString &captures) {
    auto *mixer=qmlContext(window)->contextProperty("tx6").value<QObject*>();
    if(!mixer)return 2;
    window->setProperty("benchmarkPinned",true);window->setProperty("useCider",false);
    player.setVolume(0);player.setMotion(false);player.setMiniMode(false);
    player.setMedium("cd");player.setThreeD(false);player.demo();player.pause();
    if(!until([&]{return player.count()>0&&!player.busy()&&player.duration()>0&&window->isExposed();}))return 2;
    player.pause();
    QTest::qWait(600);
    if(qEnvironmentVariableIsSet("SPUN_PERF_TIMING_ONLY"))return verifyMotion(player,window);
    const bool pacing=qEnvironmentVariableIsSet("SPUN_PERF_PACING");
    const auto media=qEnvironmentVariable("SPUN_PERF_MEDIA","cd,vinyl,cassette,tp7,tx6").split(',');
    QList<QPair<QString,bool>> stages;
    for(const auto &medium:media)for(bool threeD:{false,true})stages.append({medium,threeD});
    if(!pacing)for(const auto &medium:media)stages.append({medium,true});
    QJsonArray rows;int failures=0;
    for(int round=0;round<(pacing?1:2);++round)for(const auto &stage:stages) {
        const auto &medium=stage.first;const bool threeD=stage.second;
        // Direct render-thread callbacks own their sample. Disconnecting a
        // signal need not wait for a callback already in flight.
        struct Presentation { std::atomic<bool> synchronized=false; std::atomic<qint64> presented=-1; QElapsedTimer clock; };
        const auto presentation=std::make_shared<Presentation>();presentation->clock.start();
        const auto sync=QObject::connect(window,&QQuickWindow::afterSynchronizing,window,[presentation]{presentation->synchronized=true;},Qt::DirectConnection);
        const auto swap=QObject::connect(window,&QQuickWindow::frameSwapped,window,[presentation]{
            qint64 expected=-1;
            if(presentation->synchronized.load())presentation->presented.compare_exchange_strong(expected,presentation->clock.nsecsElapsed());
        },Qt::DirectConnection);
        mixer->setProperty("visible",medium=="tx6");
        player.setMedium(medium=="tx6"?"tp7":medium);player.setThreeD(threeD);
        const auto setMs=presentation->clock.nsecsElapsed()/1e6;
        window->update();
        const bool ready=until([&]{return presentation->presented.load()>=0;});
        QObject::disconnect(sync);QObject::disconnect(swap);
        const bool correct=window->property("threeDActive").toBool()==threeD;
        failures+=!ready||!correct;
        QJsonObject row{{"medium",medium},{"threeD",threeD},{"round",round},{"setMs",setMs},
            {"firstPresentMs",presentation->presented.load()/1e6},{"correct",ready&&correct}};
        QTest::qWait(600);
        if(!captures.isEmpty()&&round==0) {
            QDir().mkpath(captures);
            if(!captureTestWindow(window).save(captures+"/"+medium+(threeD?"-3d":"-2d")+".png"))++failures;
        }
        if(pacing) {
            player.setMotion(true);player.setRepeatMode(2);player.play();QTest::qWait(1500);
            struct Frames { QList<double> intervals;QMutex lock;QElapsedTimer clock;qint64 previous=0; };
            const auto frames=std::make_shared<Frames>();frames->clock.start();
            QSignalSpy motion(window,SIGNAL(spinAngleChanged()));
            const auto sample=QObject::connect(window,&QQuickWindow::frameSwapped,window,[frames]{
                const auto now=frames->clock.nsecsElapsed();QMutexLocker guard(&frames->lock);
                if(frames->previous)frames->intervals.append((now-frames->previous)/1e6);
                frames->previous=now;
            },Qt::DirectConnection);
            QEventLoop measurement;QTimer::singleShot(3000,&measurement,&QEventLoop::quit);
            measurement.exec();const double seconds=frames->clock.elapsed()/1000.;
            QObject::disconnect(sample);
            {QMutexLocker guard(&frames->lock);
                auto &intervals=frames->intervals;
                // An interrupted nested event loop is not a valid pacing
                // sample. Do not silently report NaN/null rates as a pass.
                const bool validSample=seconds>=2.5&&!intervals.isEmpty();
                row["validPacingSample"]=validSample;
                failures+=!validSample;
                std::sort(intervals.begin(),intervals.end());
                if(validSample) {
                    row["presentHz"]=(intervals.size()+1)/seconds;
                    row["motionHz"]=motion.count()/seconds;
                }
                row["screenHz"]=window->screen()->refreshRate();
                if(!intervals.isEmpty()) {
                    row["frameMedianMs"]=intervals[intervals.size()/2];
                    row["frameP95Ms"]=intervals[qMin(intervals.size()-1,qsizetype(intervals.size()*.95))];
                }
            }
            player.setMotion(false);QTest::qWait(100);
            auto *animation=window->findChild<QObject*>("mediaFrameAnimation");
            const auto still=window->property("spinAngle");QTest::qWait(120);
            row["reducedMotionStopped"]=animation&&!animation->property("running").toBool()&&still==window->property("spinAngle");
            failures+=!row["reducedMotionStopped"].toBool();
            player.setMotion(true);window->hide();QTest::qWait(100);
            const auto hidden=window->property("spinAngle");QTest::qWait(120);
            row["hiddenMotionStopped"]=animation&&!animation->property("running").toBool()&&hidden==window->property("spinAngle");
            failures+=!row["hiddenMotionStopped"].toBool();
            player.pause();player.setMotion(false);window->show();until([&]{return window->isExposed();});QTest::qWait(200);
        }
        row["memory"]=memory();rows.append(row);
        std::cout<<"PERFORMANCE "<<QJsonDocument(row).toJson(QJsonDocument::Compact).constData()<<std::endl;
    }
    player.pause();player.setThreeD(false);
    std::cout<<"PERFORMANCE_RESULT "<<QJsonDocument(QJsonObject{{"failures",failures},{"rows",rows}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
    return failures?1:0;
}
