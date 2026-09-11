#include "tx6test.h"
#include "tx6.h"
#include "player.h"
#include "testcapture.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlContext>
#include <QTest>
#include <QDataStream>
#include <QDir>
#include <iostream>
#include <cmath>

namespace {
QAudioBuffer tone(double hz,double amplitude=.1,int frames=48000,qint64 start=0){QByteArray b(frames*8,0);auto *p=reinterpret_cast<float*>(b.data());for(int i=0;i<frames;++i)p[i*2]=p[i*2+1]=amplitude*std::sin(i*2*M_PI*hz/48000.);return QAudioBuffer(b,Tx6::format(),start);}
double rms(const QByteArray &b){if(b.isEmpty())return 0;const auto *p=reinterpret_cast<const float*>(b.constData());int size=b.size()/4;double power=0;for(int i=size/2;i<size;++i)power+=p[i]*p[i];return std::sqrt(power/(size-size/2));}
bool until(const std::function<bool()> &f){QElapsedTimer t;t.start();while(!f()&&t.elapsed()<6000)QTest::qWait(20);return f();}
QQuickItem *find(QQuickItem *root,const QString &name){if(root->objectName()==name)return root;for(auto *child:root->childItems())if(auto *result=find(child,name))return result;return nullptr;}
}
int exerciseTx6(Player &player,Tx6 &mixer,QQuickWindow *window,const QString &temp,const QString &captures){
    int checks=0,failures=0;auto check=[&](bool ok,const char *text){++checks;if(!ok)++failures;std::cout<<(ok?"PASS ":"FAIL ")<<text<<std::endl;return ok;};
    auto invoke=[](QObject *o,const char *method){return o&&QMetaObject::invokeMethod(o,method);};
    player.setMedium("tp7");player.setMiniMode(false);player.setThreeD(false);player.setVolume(1);window->setProperty("useCider",false);window->setColor(QColor("#242a30"));mixer.setVisible(true);mixer.setPowered(true);mixer.reset();
    check(mixer.active(),"USB software route is active only in TP-7 view");
    auto dry=tone(1000);check(std::abs(rms(mixer.process(dry))-.07071)<.001,"neutral mixer preserves PCM level");
    for(int band=0;band<3;++band){mixer.reset();mixer.setEq(0,band,-12);check(rms(mixer.process(tone(band==0?18000:band==1?1000:40)))<.025,"each EQ band attenuates its frequency range");}
    mixer.reset();mixer.setLevel(0,.25);check(std::abs(rms(mixer.process(dry))-.01768)<.001,"channel fader changes actual PCM gain");
    mixer.toggleMute(0);check(rms(mixer.process(dry))<.00001,"mute silences PCM after de-click ramp");
    mixer.reset();mixer.toggleSolo(1);check(rms(mixer.process(dry))<.00001,"solo suppresses all other channels");
    mixer.reset();mixer.process(tone(1000,0));mixer.setDelay(true);auto impulse=tone(1000,0,48000);auto impulseData=impulse.data<float>();impulseData[0]=impulseData[1]=.4;mixer.process(impulse);auto tail=mixer.process(tone(1000,0,48000));check(rms(tail)>0.000001,"delay produces a decaying audible tail");
    mixer.reset();mixer.setCompressor(true);check(rms(mixer.process(tone(1000,.9)))<.42,"compressor reduces hot signals");
    mixer.reset();mixer.setEq(0,1,12);auto loud=mixer.process(tone(1000,1));bool finite=true;for(int i=0;i<loud.size()/4;++i){const auto x=reinterpret_cast<const float*>(loud.constData())[i];finite&=std::isfinite(x)&&std::abs(x)<=1;}check(finite,"master output remains finite and bounded");
    mixer.reset();player.setVolume(0);check(rms(mixer.process(dry))<.00001,"master volume reaches silence");
    const auto stem=temp+"/tx6-fixture.wav";QFile f(stem);if(!f.open(QIODevice::WriteOnly))return 2;QDataStream out(&f);out.setByteOrder(QDataStream::LittleEndian);const int n=48000*4;out.writeRawData("RIFF",4);out<<quint32(36+n*4);out.writeRawData("WAVEfmt ",8);out<<quint32(16)<<quint16(1)<<quint16(2)<<quint32(48000)<<quint32(192000)<<quint16(4)<<quint16(16);out.writeRawData("data",4);out<<quint32(n*4);for(int i=0;i<n;++i){qint16 sample=3200*std::sin(i*2*M_PI*440/48000.);out<<sample<<sample;}f.close();
    for(int c=1;c<6;++c)mixer.load(c,QUrl::fromLocalFile(stem));
    check(until([&]{for(const auto &v:mixer.channels())if(v.toMap().value("loading").toBool())return false;return true;}),"five optional inputs finish decoding");
    player.setVolume(1);mixer.flush();
    for(int c=1;c<6;++c){mixer.reset();mixer.toggleSolo(c);check(rms(mixer.process(tone(1000,0)))>.06,"each imported channel contributes real samples");check(rms(mixer.process(tone(1000,0,48000,6000000)))<.0001,"stem ends silently and follows transport timestamp");}
    mixer.reset();for(int c=1;c<6;++c)mixer.unload(c);check(!mixer.channels()[1].toMap().value("loaded").toBool(),"unloading releases input");
    player.addUrls({QUrl::fromLocalFile(stem)},false);check(until([&]{return player.count()>0&&!player.busy();}),"TP-7 local fixture loaded");player.setVolume(0);mixer.flush();player.play();check(until([&]{return mixer.meters()[0].toDouble()>.01;}),"QMediaPlayer decoded audio actually reaches mixer");if(qEnvironmentVariableIsSet("SPUN_TEST_MIXER_OUTPUT"))check(until([&]{return mixer.writtenFrames()>0;}),"processed PCM reaches the native audio output");player.pause();
    player.setMotion(true);auto *typography=qmlContext(window)->contextProperty("typography").value<QObject*>();typography->setProperty("uiScale",1.25);
    auto capture=[&](const QString &name){if(captures.isEmpty())return;QTest::mouseMove(window,{4,4});QTest::qWait(100);QDir().mkpath(captures);auto image=captureTestWindow(window);check(!image.isNull()&&image.save(captures+"/"+name+".png"),"rendered capture saved");};
    for(bool threeD:{false,true}){
        if(threeD&&!qmlContext(window)->contextProperty("supports3D").toBool())continue;
        player.setThreeD(threeD);QTest::qWait(400);auto *controls=find(window->contentItem(),"tx6Controls"),*view=find(window->contentItem(),"player3DView");
        if(!check(controls&&(!threeD||view),"mixer and renderer load"))return 2;
        capture(threeD?"tx6-3d":"tx6-2d");
        const auto point=[&](int index){if(threeD){QVariant r;QMetaObject::invokeMethod(view,"projectTx6Control",Q_RETURN_ARG(QVariant,r),Q_ARG(QVariant,index));return view->mapToScene(r.toPointF()).toPoint();}auto *key=find(window->contentItem(),"tx6Control"+QString::number(index));return key->mapToScene({key->width()/2,key->height()/2}).toPoint();};
        if(qEnvironmentVariableIsSet("SPUN_TEST_CAPTURE_ONLY")){if(threeD){view->setProperty("recorderYaw",70.);QTest::qWait(180);capture("tx6-side");view->setProperty("recorderYaw",172.);QTest::qWait(180);capture("tx6-back");}continue;}
        for(int i=0;i<34;++i){if(threeD)QTest::qWait(450);const auto p=point(i);auto *key=find(window->contentItem(),"tx6Control"+QString::number(i));
            if(threeD){QVariant hit;const auto loc=view->mapFromScene(p);QMetaObject::invokeMethod(view,"recorderHit",Q_RETURN_ARG(QVariant,hit),Q_ARG(QVariant,loc.x()),Q_ARG(QVariant,loc.y()));check(hit.toMap().value("tx6",-1).toInt()==i,"3D visible control is correctly picked");}
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,p);check(until([&]{return controls->property("pressedControl").toInt()==i&&key->property("pressure").toDouble()>.4;}),"press animation reaches travel");check(controls->property("pressedControl").toInt()==i&&key->property("pressure").toDouble()>.4,"hardware pressure is applied while held");
            if(i==31)capture(threeD?"tx6-3d-pressed":"tx6-2d-pressed");
            QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,p);check(until([&]{return controls->property("pressedControl").toInt()==-1&&key->property("pressure").toDouble()<.4;}),"return animation settles");check(controls->property("pressedControl").toInt()==-1&&key->property("pressure").toDouble()<.4,"hardware springs back after release");
        }
        QTest::qWait(450);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point(34));check(!mixer.powered(),"side power button bypasses mixer");QTest::qWait(450);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point(34));check(mixer.powered(),"side power button reconnects mixer");
        mixer.reset();
        QTest::qWait(450);auto p=point(18);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,p);const auto dest=p+QPoint(0,45);QMouseEvent e(QEvent::MouseMove,dest,window->mapToGlobal(dest),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QGuiApplication::sendEvent(window,&e);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,dest);check(mixer.channels()[0].toMap().value("level").toDouble()<.8,"dragging physical fader changes channel gain");
        QTest::qWait(450);p=point(24);QTest::mouseClick(window,Qt::LeftButton,Qt::ShiftModifier,p);check(mixer.channels()[0].toMap().value("solo").toBool(),"Shift click solos a channel");
        QTest::qWait(450);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point(33));check(controls->property("shiftHeld").toBool(),"physical Shift button arms solo with a mouse");
        QTest::qWait(450);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point(25));check(mixer.channels()[1].toMap().value("solo").toBool()&&!controls->property("shiftHeld").toBool(),"next channel consumes the one-shot solo modifier");
        p=point(30);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,p);invoke(controls,"cancel");check(controls->property("pressedControl").toInt()==-1,"cancel clears pressure and gesture state");QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,p);
        if(threeD){
            check(window->property("layoutWidth").toInt()==740 && view->width()==740,"paired view reserves a wider interaction area");
            const auto face=[&](double x,double y){QVariant r;QMetaObject::invokeMethod(view,"projectTx6Face",Q_RETURN_ARG(QVariant,r),Q_ARG(QVariant,x),Q_ARG(QVariant,y));return view->mapToScene(r.toPointF()).toPoint();};
            const auto hitAt=[&](QPoint p){QVariant r;const auto local=view->mapFromScene(p);QMetaObject::invokeMethod(view,"recorderHit",Q_RETURN_ARG(QVariant,r),Q_ARG(QVariant,local.x()),Q_ARG(QVariant,local.y()));return r.toMap().value("tx6",-1).toInt();};
            const auto faceWidth=QLineF(face(0,176),face(244,176)).length()/window->property("uiScale").toDouble();
            std::cout<<"Mixer face width: "<<faceWidth<<" logical pixels"<<std::endl;
            check(faceWidth>210,"mixer is visibly larger than the former compact model");
            for(int index=0;index<18;++index)check(hitAt(point(index)+QPoint(11,0))==index,"near-edge knob click picks its nearest channel");
            mixer.reset();
            for(int channel=0;channel<6;++channel){
                QTest::qWait(450);const auto rail=face(22+channel*29.3,260);QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,rail);
                check(std::abs(mixer.channels()[channel].toMap().value("level").toDouble()-25./77)<.04,"clicking anywhere along a fader rail sets that channel only");
            }
            const auto nearKnob=point(0)+QPoint(11,0);QTest::qWait(450);QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,nearKnob);
            check(controls->property("pressedControl").toInt()==0 && !view->property("orbiting").toBool(),"forgiving knob grab does not start orbiting");
            const auto knobDest=nearKnob+QPoint(0,-30);QMouseEvent knobMove(QEvent::MouseMove,knobDest,window->mapToGlobal(knobDest),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QGuiApplication::sendEvent(window,&knobMove);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,knobDest);
            check(mixer.channels()[0].toMap().value("high").toDouble()>1 && mixer.channels()[1].toMap().value("high").toDouble()==0,"near-edge drag changes only the intended EQ knob");
            QTest::qWait(450);QTest::mouseClick(window,Qt::RightButton,Qt::NoModifier,point(1));
            auto *channelMenu=controls->findChild<QObject*>("tx6ChannelMenu");auto *settingsMenu=window->findChild<QObject*>("settingsMenu");
            check(until([&]{return channelMenu&&channelMenu->property("visible").toBool();}) && settingsMenu && !settingsMenu->property("visible").toBool(),"channel right-click stays open without the main menu stealing it");
            QTest::qWait(250);auto *soloItem=find(window->contentItem(),"tx6SoloAction");
            if(check(soloItem&&soloItem->isVisible()&&soloItem->isEnabled(),"channel menu action is usable in 3D"))QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,soloItem->mapToScene({soloItem->width()/2,soloItem->height()/2}).toPoint());
            check(until([&]{return mixer.channels()[1].toMap().value("solo").toBool();}),"channel context menu acts on the selected channel");
            if(channelMenu)invoke(channelMenu,"close");
            QTest::qWait(150);
            view->setProperty("recorderYaw",45.);QTest::qWait(450);
            check(hitAt(point(6)+QPoint(7,0))==6,"assisted picking follows the rotated mixer");
            view->setProperty("recorderYaw",180.);QTest::qWait(450);
            check(hitAt(face(22,60))<0,"rear-face clicks cannot reach the front knobs");
            invoke(view,"resetRecorderOrientation");QTest::qWait(450);
            QTest::qWait(450);QVariant r;QMetaObject::invokeMethod(view,"projectRecorderControl",Q_RETURN_ARG(QVariant,r),Q_ARG(QVariant,1));
            const auto playPoint=view->mapToScene(r.toPointF()).toPoint();QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,playPoint);
            check(until([&]{return player.playing();}),"paired TP-7 transport still plays through the mixer");player.pause();
            view->setProperty("recorderYaw",58.);view->setProperty("recorderPitch",14.);QTest::qWait(200);capture("tx6-side");view->setProperty("recorderYaw",172.);QTest::qWait(200);capture("tx6-back");invoke(view,"resetRecorderOrientation");}
        window->setProperty("useCider",true);check(!mixer.active(),"Cider source bypasses unsupported local DSP");window->setProperty("useCider",false);
        mixer.setVisible(false);check(!mixer.active(),"hiding mixer restores normal playback route");
        check(window->property("layoutWidth").toInt()==530,"hiding mixer restores compact window width");
        mixer.setVisible(true);QTest::qWait(150);
    }
    player.pause();mixer.reset();player.setMedium("cd");check(!mixer.active(),"leaving TP-7 restores direct audio");
    std::cout<<checks<<" checks, "<<failures<<" failures"<<std::endl;return failures?1:0;
}
