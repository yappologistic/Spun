#include "cider.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTest>
#include <QElapsedTimer>
#include <QFile>
#include <iostream>
#include <utility>

int exerciseCiderEvents(const QString &temp) {
    int failures=0,requests=0,queueReads=0,pongs=0;bool reject=false;
    QByteArray pending;
    const QByteArray queueEvent="42[\"API:Playback\",{\"type\":\"queueStatus.queueChanged\",\"data\":{}}]";
    auto check=[&](bool ok,const char *label) { std::cout<<(ok?"PASS ":"FAIL ")<<label<<std::endl;if(!ok)++failures; };
    auto wait=[&](auto condition) { QElapsedTimer clock;clock.start();while(!condition()&&clock.elapsed()<5000)QTest::qWait(20);return condition(); };
    QTcpServer server;check(server.listen(QHostAddress::LocalHost),"event fixture starts");
    QObject::connect(&server,&QTcpServer::newConnection,&server,[&] {
        auto *socket=server.nextPendingConnection();QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        QObject::connect(socket,&QTcpSocket::readyRead,socket,[&,socket] {
            auto bytes=socket->property("buffer").toByteArray()+socket->readAll();socket->setProperty("buffer",bytes);
            const int split=bytes.indexOf("\r\n\r\n");if(split<0)return;
            int size=0;for(const auto &line:bytes.left(split).toLower().split('\n'))if(line.startsWith("content-length:"))size=line.mid(15).trimmed().toInt();
            if(bytes.size()<split+4+size)return;
            ++requests;const auto path=bytes.split(' ').value(1),body=bytes.mid(split+4,size);QByteArray result;int status=200,delay=15;
            if(!bytes.left(split).contains("event-test-token"))status=403;
            else if(path.startsWith("/socket.io/")) {
                if(reject)status=503;
                else if(bytes.startsWith("POST")) {
                    if(body.contains("40"))pending="40{}";
                    if(body.contains('3'))++pongs;
                    result="ok";
                } else if(!path.contains("sid="))result="0{\"sid\":\"test-session\",\"pingInterval\":1000,\"pingTimeout\":1000}";
                else { result=pending.isEmpty()?QByteArray("2"):std::exchange(pending,{});delay=60; }
            } else if(path.startsWith("/api/v2/queue?")) {
                ++queueReads;result="{\"data\":{\"position\":-1,\"items\":[]},\"meta\":{\"total\":0}}";
            } else { status=404;result="{}"; }
            QTimer::singleShot(delay,socket,[socket,result,status] {
                socket->write("HTTP/1.1 "+QByteArray::number(status)+" Result\r\nContent-Length: "+QByteArray::number(result.size())+"\r\nConnection: close\r\n\r\n"+result);socket->disconnectFromHost();
            });
        });
    });
    const QString config=temp+"/events-connection.json";QFile file(config);check(file.open(QIODevice::WriteOnly),"event fixture token file opens");file.write("{\"token\":\"event-test-token\"}");file.close();
    Cider cider(false,config,QUrl("http://127.0.0.1:"+QString::number(server.serverPort())));
    cider.setQueueVisible(true);cider.setLiveVisible(true);
    check(wait([&]{return cider.liveConnected()&&!cider.queueBusy();}),"event stream authenticates and reconciles queue on connect");
    check(wait([&]{return pongs>0;}),"event stream answers Engine.IO heartbeat");
    QTest::qWait(200);const int before=queueReads;
    for(int i=0;i<20;++i) { if(!pending.isEmpty())pending+='\x1e';pending+=queueEvent; }
    check(wait([&]{return queueReads>before;}),"external queue event refreshes the visible queue");QTest::qWait(350);
    check(queueReads==before+1,"burst of queue events produces one readback");
    const int beforeUnknown=queueReads;pending="42[\"API:Playback\",{\"type\":\"playbackStatus.playbackTimeDidChange\"}]";QTest::qWait(350);
    check(queueReads==beforeUnknown,"playback ticks do not trigger queue requests");
    reject=true;check(wait([&]{return !cider.liveConnected();}),"failed event transport falls back without clearing queue");
    check(cider.queueReady(),"event outage retains the confirmed queue");reject=false;
    check(wait([&]{return cider.liveConnected();}),"event connection recovers with backoff");
    cider.setLiveVisible(false);cider.setQueueVisible(false);QTest::qWait(100);const int stopped=requests;QTest::qWait(300);
    check(!cider.liveConnected()&&requests==stopped,"hidden player stops event requests and reconnect timers");
    server.close();return failures;
}
