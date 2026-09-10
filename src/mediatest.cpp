#include "player.h"
#include "cider.h"
#include "mpris.h"
#include <QGuiApplication>
#include <QDBusVirtualObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QTest>
#include <QElapsedTimer>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <iostream>
#include <functional>

class DesktopFixture : public QDBusVirtualObject {
public:
    QVariantMap values{{"PlaybackStatus","Playing"},{"Volume",.4},{"Position",qlonglong(2000000)},
        {"CanSeek",true},{"CanGoNext",true},{"CanGoPrevious",true},{"Shuffle",false},{"LoopStatus","None"},
        {"Metadata",QVariantMap{{"mpris:trackid",QVariant::fromValue(QDBusObjectPath("/cider/first"))},
            {"xesam:title","Remote song"},{"xesam:artist",QStringList{"Remote artist"}},{"xesam:album","Remote album"},{"mpris:length",qlonglong(32000000)}}}};
    QStringList calls;
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message,const QDBusConnection &bus) override {
        if (message.interface()=="org.freedesktop.DBus.Properties") {
            if (message.member()=="GetAll") { bus.send(message.createReply(QVariantList{values})); return true; }
            if (message.member()=="Set") {
                values[message.arguments()[1].toString()]=message.arguments()[2].value<QDBusVariant>().variant();
            }
        } else {
            calls.append(message.member());
            if(message.member()=="Pause")values["PlaybackStatus"]="Paused";
            if(message.member()=="Play")values["PlaybackStatus"]="Playing";
            if(message.member()=="Stop"){values["PlaybackStatus"]="Stopped";values["Position"]=qlonglong(0);}
            if(message.member()=="SetPosition") {
                values["Position"]=message.arguments()[1];
                auto seek=QDBusMessage::createSignal("/org/mpris/MediaPlayer2","org.mpris.MediaPlayer2.Player","Seeked");
                seek << message.arguments()[1];bus.send(seek);
            }
        }
        bus.send(message.createReply()); publish(); return true;
    }
    void publish() {
        auto event=QDBusMessage::createSignal("/org/mpris/MediaPlayer2","org.freedesktop.DBus.Properties","PropertiesChanged");
        event << QString("org.mpris.MediaPlayer2.Player") << values << QStringList();QDBusConnection::sessionBus().send(event);
    }
};
static bool waitFor(std::function<bool()> condition) {
    QElapsedTimer timer;timer.start();
    while(!condition()&&timer.elapsed()<6000)QTest::qWait(10);
    return condition();
}
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);QTemporaryDir temp;int failures=0;
    auto check=[&](bool pass,const char *name){std::cout<<(pass?"PASS ":"FAIL ")<<name<<std::endl;if(!pass)++failures;};
    auto bus=QDBusConnection::sessionBus();DesktopFixture fixture;
    if(!bus.registerService("org.mpris.MediaPlayer2.cider") || !bus.registerVirtualObject("/org/mpris/MediaPlayer2",&fixture))return 2;
    Player player(temp.path()+"/player.ini");player.setVolume(0);QList<QUrl> files;
    for(int i=0;i<3;++i) {
        const auto file=temp.path()+QString("/%1.flac").arg(i);
        QFile::copy(QStringLiteral(SPUN_SOURCE_DIR "/assets/First-Light.flac"),file);
        TagLib::FileRef tags(QFile::encodeName(file).constData());tags.tag()->setAlbum("Test record");tags.tag()->setTrack(3-i);tags.save();
        files.append(QUrl::fromLocalFile(file));
    }
    player.addUrls(files,false);check(waitFor([&]{return !player.busy();})&&player.count()==3,"local media fixture imports");
    Cider cider(true,temp.path()+"/cider.json",QUrl("http://127.0.0.1:1"));
    check(waitFor([&]{return cider.available()&&cider.count()==1;}),"isolated Cider desktop service connects");
    PlayerAdaptor media(&player,&cider);QSignalSpy seekSignals(&media,&PlayerAdaptor::Seeked);
    const auto localId=media.trackId();const auto localIndex=player.currentIndex();
    media.Next();check(!player.playing()&&player.currentIndex()!=localIndex,"desktop Next preserves paused local playback");
    player.seek(2000);const auto before=player.position();
    media.SetPosition(QDBusObjectPath(localId),3000000);check(player.position()==before,"stale desktop track IDs cannot seek a new song");
    media.SetPosition(QDBusObjectPath(media.trackId()),-1);check(player.position()==before,"negative desktop SetPosition is rejected");
    media.SetPosition(QDBusObjectPath(media.trackId()),(player.duration()+1000)*1000);check(player.position()==before,"out-of-range desktop SetPosition is rejected");
    QQmlEngine engine;QQmlComponent component(&engine);
    component.setData("import QtQml; QtObject { property bool useCider: false }",QUrl());
    std::unique_ptr<QObject> source(component.create());
    check(bool(source),"source selection fixture creates");
    if(!source)return 2;
    media.setSourceWindow(source.get());source->setProperty("useCider",true);

    check(media.metadata().value("xesam:title")=="Remote song"&&media.status()=="Playing"&&media.trackId()!=localId,"desktop metadata and state follow Cider selection");
    check(!media.metadata().contains("xesam:url"),"remote metadata never exposes the local track path");
    media.Pause();check(waitFor([&]{return !cider.playing();})&&fixture.calls.contains("Pause"),"desktop Pause reaches Cider");
    media.setVolume(.25);check(waitFor([&]{return qAbs(cider.volume()-.25)<.001;})&&player.volume()!=.25,"desktop volume changes only the selected provider");
    const auto oldSignals=seekSignals.size();media.SetPosition(QDBusObjectPath(media.trackId()),7000000);
    check(waitFor([&]{return cider.position()==7000&&seekSignals.size()>oldSignals;}),"confirmed Cider seek updates desktop position");
    fixture.values["CanSeek"]=false;fixture.values["CanGoNext"]=false;fixture.publish();
    check(waitFor([&]{return !media.canSeek()&&!media.canNext();}),"desktop capabilities update without a track change");
    const auto calls=fixture.calls.size();media.Seek(1000000);media.Next();QTest::qWait(100);check(fixture.calls.size()==calls,"unsupported desktop commands are ignored");
    media.Stop();check(waitFor([&]{return media.status()=="Stopped";}),"desktop Stop reports stopped rather than paused");
    source->setProperty("useCider",false);check(media.metadata().value("xesam:title")==player.title(),"switching back restores local desktop metadata");
    check(bus.registerService("org.mpris.MediaPlayer2.spun")&&bus.registerObject("/org/mpris/MediaPlayer2/spun",&player,QDBusConnection::ExportAdaptors),"Spun desktop adaptor exports on the isolated bus");
    auto get=QDBusMessage::createMethodCall("org.mpris.MediaPlayer2.spun","/org/mpris/MediaPlayer2/spun","org.freedesktop.DBus.Properties","GetAll");
    get << QString("org.mpris.MediaPlayer2.Player");
    QDBusPendingCallWatcher read(bus.asyncCall(get));
    check(waitFor([&]{return read.isFinished();}),"desktop property read completes over D-Bus");
    QDBusPendingReply<QVariantMap> reply=read;
    check(!reply.isError()&&reply.value().value("Metadata").isValid()&&reply.value().value("CanControl").toBool(),"desktop clients can read the exported metadata and controls");
    player.setVinyl(true);player.setVinylAlbumMode(true);
    const auto rows=player.discDetails().value("tracks").toList();
    check(rows.size()==3&&rows.first().toMap().value("index").toInt()==2,"record order uses album track numbers");
    check(player.playAlbumPosition(rows[1].toMap().value("path").toString(),2500),"needle can select a loaded album song and timestamp");
    player.pause();check(player.currentIndex()==1&&player.position()==2500,"local album seek preserves the requested pending position");
    check(!player.playAlbumPosition(temp.path()+"/missing.flac",0)&&!player.playAlbumPosition(rows[0].toMap().value("path").toString(),-1),"album drops reject stale files and invalid positions");
    player.next();player.pause();check(player.currentIndex()==0,"record advances in album order rather than import order");
    {Player restored(temp.path()+"/player.ini");check(restored.vinylAlbumMode(),"album mode preference survives relaunch");}
    for(int i=0;i<files.size();++i) {
        TagLib::FileRef tags(QFile::encodeName(files[i].toLocalFile()).constData());
        tags.tag()->setAlbum(TagLib::String(QString("Separate album %1").arg(i).toUtf8().constData(),TagLib::String::UTF8));tags.save();
    }
    Player separate(temp.path()+"/separate.ini");separate.addUrls(files,false);
    check(waitFor([&]{return !separate.busy();}),"separate-album fixture imports");
    separate.setVinyl(true);separate.setVinylAlbumMode(true);separate.next(false,false);
    check(separate.currentIndex()==1&&!separate.playing(),"single-track album falls back to ordinary queue navigation");
    separate.previous(false);check(separate.currentIndex()==0,"single-track album fallback also supports Previous");
    std::cout<<"MEDIA RESULT "<<failures<<" failures"<<std::endl;return failures?1:0;
}
