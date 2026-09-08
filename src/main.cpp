#include "player.h"
#include "library.h"
#include "listening.h"
#include "musicactions.h"
#ifdef SPUN_DIAGNOSTICS
#include "librarytest.h"
#include "testinput.h"
#endif
#include "lyrics.h"
#include "artwork.h"
#include "disc.h"
#include "symbol.h"
#include "mpris.h"
#include "theme.h"
#include "typography.h"
#include "cider.h"
#include <QGuiApplication>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFont>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSignalMapper>
#include <QQuickWindow>
#include <QQuickItem>
#include <QScreen>
#include <QStandardPaths>
#include <QCommandLineParser>
#include <QPainterPath>
#include <QRegion>
#include <QFile>
#include <QDateTime>
#include <QDir>
#include <QTemporaryDir>
#include <QSaveFile>
#ifdef SPUN_DIAGNOSTICS
#include <QTest>
#include <QQuickItemGrabResult>
#endif
#include <QTimer>
#include <QElapsedTimer>
#include <QIcon>
#include <QProcess>
#include <QPointer>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>
#include <QWheelEvent>
#include <iostream>
#include <sys/resource.h>
#include <unistd.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>

class Native : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hyprland READ hyprland CONSTANT)
    Q_PROPERTY(bool supportsBlur READ supportsBlur CONSTANT)
    Q_PROPERTY(bool exposed READ exposed NOTIFY exposureChanged)
public:
    explicit Native(QObject *parent = nullptr) : QObject(parent) {
        m_effectTimer.setSingleShot(true);
        connect(&m_effectTimer, &QTimer::timeout, this, [this] {
            if (!m_window || !m_window->isVisible() || !supportsBlur()) return;
            if (m_effectProcess) { m_effectTimer.start(80); return; }
            const bool pinned = m_window->property("miniPinned").toBool() || m_window->property("benchmarkPinned").toBool();
            const auto script = QString(
                "local w=hl.get_window(\"pid:%3\"); if not w then return end; "
                "for _,p in ipairs({{\"no_blur\",\"%1\"},{\"no_shadow\",\"1\"},{\"border_size\",\"0\"},{\"rounding\",\"%2\"},{\"decorate\",\"0\"}}) do "
                "hl.dispatch(hl.dsp.window.set_prop({window=w,prop=p[1],value=p[2]})) end; "
                "local pinned=%4; if w.pinned ~= pinned then "
                "if pinned and not w.floating then hl.dispatch(hl.dsp.window.float({window=w,action=\"set\"})) end; "
                "hl.dispatch(hl.dsp.window.pin({window=w,action=pinned and \"set\" or \"unset\"})) end")
                .arg(m_blur ? 0 : 1).arg(m_blur ? 21 : 0).arg(QCoreApplication::applicationPid())
                .arg(pinned ? "true" : "false");
            auto *process = new QProcess(this);
            m_effectProcess = process;
            connect(process, &QProcess::finished, process, &QObject::deleteLater);
            connect(process, &QProcess::errorOccurred, process, &QObject::deleteLater);
            QTimer::singleShot(3000, process, [process] { if (process->state() != QProcess::NotRunning) process->kill(); });
            process->start("hyprctl", {"eval", script});
        });
    }
    bool hyprland() const { return !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE"); }
    bool supportsBlur() const { return hyprland() && QGuiApplication::platformName() == "wayland"; }
    Q_INVOKABLE void effects(QWindow *window, bool blur) {
        m_window = window; m_blur = blur;
        m_effectTimer.start(80);
    }
    bool exposed() const { return m_exposed; }
    bool eventFilter(QObject *object, QEvent *event) override {
        if (event->type() == QEvent::Expose) {
            if (auto *window = qobject_cast<QWindow *>(object)) {
                const bool exposed = window->isExposed();
                if (exposed != m_exposed) { m_exposed = exposed; emit exposureChanged(); }
                if (exposed) m_effectTimer.start(80);
            }
        }
        return QObject::eventFilter(object, event);
    }
    Q_INVOKABLE void shape(QWindow *window, bool queue) {
        if (!window) return;
        const bool mini = window->property("miniMode").toBool();
        const int targetWidth = mini ? 300 : queue ? 860 : 530;
        const int targetHeight = mini ? 354 : 730;
        window->setMinimumWidth(0);
        window->setMinimumHeight(0);
        window->setMaximumWidth(targetWidth);
        window->setMaximumHeight(targetHeight);
        window->resize(targetWidth, targetHeight);
        window->setMinimumWidth(targetWidth);
        window->setMinimumHeight(targetHeight);
        if (mini && window->property("menuOpen").toBool()) { window->setMask(QRegion(0,0,targetWidth,targetHeight));return; }
        if (mini) {
            QRegion region(6,6,288,288,QRegion::Ellipse);
            if (window->property("backgroundBlur").toBool()) {
                QPainterPath backdrop; backdrop.addRoundedRect(QRectF(0,0,300,targetHeight),21,21);
                region=QRegion(backdrop.toFillPolygon().toPolygon());
            }
            else region-=window->property("vinyl").toBool()?QRegion(146,146,8,8,QRegion::Ellipse):QRegion(136,136,28,28,QRegion::Ellipse);
            region |= QRegion(50,268,200,84);
            if (auto *notice=window->findChild<QQuickItem *>("actionNotice"); notice && notice->isVisible())
                region |= QRegion(notice->mapRectToScene(notice->boundingRect()).toAlignedRect());
            window->setMask(region); return;
        }
        if (window->property("helpOpen").toBool() || window->property("menuOpen").toBool()) {
            window->setMask(QRegion(0, 0, window->width(), window->height())); return;
        }
        if (window->property("backgroundBlur").toBool()) {
            QPainterPath backdrop; backdrop.addRoundedRect(QRectF(0, 0, window->width(), window->height()), 21, 21);
            window->setMask(QRegion(backdrop.toFillPolygon().toPolygon())); return;
        }
        QRegion region(44, 73, 442, 442, QRegion::Ellipse);
        region -= window->property("vinyl").toBool()?QRegion(259,288,12,12,QRegion::Ellipse):QRegion(243,272,44,44,QRegion::Ellipse);
        if (auto *bar = window->findChild<QQuickItem *>("sourceBar"))
            region |= QRegion(bar->mapRectToScene(bar->boundingRect()).toAlignedRect());
        if (auto *deck = window->findChild<QQuickItem *>("playerDeck"))
            region |= QRegion(deck->mapRectToScene(deck->boundingRect()).toAlignedRect());
        region |= QRegion(65, 701, 400, 26);
        if (auto *notice=window->findChild<QQuickItem *>("actionNotice"); notice && notice->isVisible())
            region |= QRegion(notice->mapRectToScene(notice->boundingRect()).toAlignedRect());
        // Error notices can overlap the otherwise click-through perimeter.
        region |= QRegion(73, 419, 384, 80);
        if (queue) {
            const auto *panelName = window->property("libraryOpen").toBool() ? "libraryPanel" : "queuePanel";
            if (auto *panel = window->findChild<QQuickItem *>(panelName))
                region |= QRegion(panel->mapRectToScene(panel->boundingRect()).toAlignedRect());
        }
        window->setMask(region);
    }
    Q_INVOKABLE void place(QWindow *window) {
        if (!window) return;
        window->installEventFilter(this);
        m_exposed = window->isExposed(); emit exposureChanged();
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen) screen = QGuiApplication::primaryScreen();
        const QRect area = screen->availableGeometry();
        window->setPosition(area.center() - QPoint(window->width()/2, window->height()/2));
        effects(window, window->property("backgroundBlur").toBool());
        m_effectTimer.start(350);
    }
signals:
    void exposureChanged();
private:
    bool m_exposed = false, m_blur = false;
    QPointer<QWindow> m_window;
    QPointer<QProcess> m_effectProcess;
    QTimer m_effectTimer;
};

static bool write(const QString &path, const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(data); return file.commit();
}
static QByteArray themeCss(bool light) {
    return QByteArray("@define-color window_bg_color ") + (light ? "#f6f0e8" : "#17191f") + ";\n"
        "@define-color window_fg_color " + (light ? "#342d29" : "#eee5dc") + ";\n"
        "@define-color accent_bg_color " + (light ? "#8b492d" : "#e6b599") + ";\n"
        "@define-color accent_fg_color " + (light ? "#ffffff" : "#392619") + ";\n"
        "@define-color card_bg_color " + (light ? "#e9e0d5" : "#24252b") + ";\n";
}

#ifdef SPUN_DIAGNOSTICS
static bool waitFor(const std::function<bool()> &condition, int timeout = 6000) {
    QElapsedTimer timer; timer.start();
    while (!condition() && timer.elapsed() < timeout) QTest::qWait(30);
    return condition();
}

static int exerciseCiderQueue(const QString &temp) {
    int failures=0;
    auto check = [&](bool ok, const char *message) {
        std::cout << (ok ? "PASS " : "FAIL ") << message << std::endl;
        if (!ok) ++failures;
    };
    QTcpServer server;
    check(server.listen(QHostAddress::LocalHost), "isolated Cider API fixture starts");
    int selected=-1, queuePosition=1;
    bool failSecondPage=false, rejectControl=false, shuffleMode=false, autoplayMode=false;
    int queueEdits=0, modeEdits=0;
    int pagesSeen=0, albumRequests=0;
    bool invalidAlbumLink=false, emptyLyrics=false;
    const QByteArray body=R"([{"id":"one","attributes":{"name":"First Light","artistName":"Spun Sound Lab","durationInMillis":32000,"artwork":{"url":"https://example.invalid/cover/{w}x{h}.{f}"}}},{"id":"two","attributes":{"name":"Second Light","artistName":"Spun Sound Lab","durationInMillis":42000}}])";
    QJsonArray queueTracks=QJsonDocument::fromJson(body).array();
    QObject::connect(&server,&QTcpServer::newConnection,&server,[&] {
        auto *socket=server.nextPendingConnection();
        QObject::connect(socket,&QTcpSocket::readyRead,socket,[&,socket] {
            QByteArray request=socket->property("buffer").toByteArray()+socket->readAll();
            socket->setProperty("buffer",request);
            const auto end=request.indexOf("\r\n\r\n");
            if (end<0) return;
            const auto headers=request.left(end).toLower();
            int length=0;
            for (const auto &line:headers.split('\n')) if (line.startsWith("content-length:")) length=line.mid(15).trimmed().toInt();
            if (request.size()<end+4+length) return;
            const bool auth=headers.contains("apptoken: test-spun-token");
            const QUrl requestUrl(QString::fromUtf8(request.split(' ').value(1)));
            const int offset=QUrlQuery(requestUrl).queryItemValue("offset").toInt();
            const bool failed=auth && ((headers.startsWith("get ") && failSecondPage && offset==1) || (rejectControl && !headers.startsWith("get ")));
            QByteArray response="{}";
            if (auth && headers.startsWith("get ")) {
                const auto tracks=queueTracks;
                QJsonArray items;
                if (offset<tracks.size()) items.append(QJsonObject{{"track",tracks[offset]}});
                response=QJsonDocument(QJsonObject{{"data",QJsonObject{{"items",items},{"position",queuePosition}}},
                    {"meta",QJsonObject{{"offset",offset},{"limit",1},{"total",tracks.size()}}}}).toJson();
                ++pagesSeen;
            }
            const auto payload=QJsonDocument::fromJson(request.mid(end+4,length)).object();
            if (auth && requestUrl.path()=="/api/v2/playback") response=QJsonDocument(QJsonObject{{"data",QJsonObject{{"shuffleMode",shuffleMode},{"autoplay",autoplayMode}}}}).toJson();
            if (auth && !failed && requestUrl.path().endsWith("/toggle")) {
                ++modeEdits;
                if (requestUrl.path().contains("/shuffle/")) shuffleMode=!shuffleMode;
                if (requestUrl.path().contains("/autoplay/")) autoplayMode=!autoplayMode;
                response="{}";
            }
            if (auth && !failed && requestUrl.path()=="/api/v2/queue/move") {
                const int from=payload["from"].toInt(-1),to=payload["to"].toInt(-1);
                if(from>=0&&to>=0&&from<queueTracks.size()&&to<queueTracks.size()) {
                    const auto moving=queueTracks.takeAt(from);queueTracks.insert(to,moving);++queueEdits;
                    if(queuePosition==from)queuePosition=to;
                    else if(from<queuePosition&&to>=queuePosition)--queuePosition;
                    else if(from>queuePosition&&to<=queuePosition)++queuePosition;
                }
                response="{}";
            }
            if (auth && !failed && headers.startsWith("delete /api/v2/queue/items/")) {
                const int index=requestUrl.path().section('/',-1).toInt();
                if(index>=0&&index<queueTracks.size()) { queueTracks.removeAt(index);++queueEdits;queuePosition=qMin(queuePosition,int(queueTracks.size())-1); }
                response="{}";
            }
            if (auth && requestUrl.path()=="/api/v2/playback/now-playing") {
                response=QJsonDocument(QJsonObject{{"data",QJsonObject{{"url",invalidAlbumLink ? "" : "https://music.apple.com/ca/album/first-light/123?i=one"},
                    {"playParams",QJsonObject{{"id","one"},{"catalogId","1234"}}}}}}).toJson();
            }
            if (auth && requestUrl.path()=="/api/v1/amapi/run-v3") {
                ++albumRequests;
                const auto apiPath=QJsonDocument::fromJson(request.mid(end+4,length)).object().value("path").toString();
                const auto songs=QJsonDocument::fromJson(body).array();
                QJsonObject payload;
                if (apiPath.contains("/tracks")) payload={{"data",QJsonArray{songs[1]}}};
                else payload={{"data",QJsonArray{QJsonObject{{"attributes",QJsonObject{{"name","The first mixtape"},{"artistName","Spun Sound Lab"},{"releaseDate","2026-09-05"},{"trackCount",2}}},
                    {"relationships",QJsonObject{{"tracks",QJsonObject{{"data",QJsonArray{songs[0]}},{"next","/v1/catalog/ca/albums/123/tracks?offset=1"}}}}}}}}};
                if (apiPath.endsWith("/lyrics")) {
                    --albumRequests;
                    const QString ttml="<tt xmlns='http://www.w3.org/ns/ttml'><body><div><p begin='0s' end='5s'>Soft light on the water</p><p begin='5s' end='10s'>Another turn around the sun</p></div></body></tt>";
                    QJsonArray documents;
                    if(!emptyLyrics)documents.append(QJsonObject{{"attributes",QJsonObject{{"ttml",ttml}}}});
                    payload={{"data",documents}};
                }
                response=QJsonDocument(QJsonObject{{"data",payload}}).toJson();
            }
            if (auth && headers.startsWith("post /api/v2/queue/jump")) {
                selected=QJsonDocument::fromJson(request.mid(end+4,length)).object().value("index").toInt(-1);
                response="{\"status\":\"ok\"}";
            }
            socket->write(QByteArray(!auth ? "HTTP/1.1 403 Forbidden\r\n" : failed ? "HTTP/1.1 503 Service Unavailable\r\n" : "HTTP/1.1 200 OK\r\n")+
                "Content-Type: application/json\r\nConnection: close\r\nContent-Length: "+QByteArray::number(response.size())+"\r\n\r\n"+response);
            socket->disconnectFromHost();
        });
        QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
    });
    const QUrl base(QString("http://127.0.0.1:%1").arg(server.serverPort()));
    const QString file=temp+"/cider-connection.json";
    Cider remote(false,file,base);
    remote.setQueueVisible(true);
    check(waitFor([&]{return remote.needsToken();}), "Cider queue requests authentication without disrupting playback");
    remote.connectQueue("test-spun-token");
    check(waitFor([&]{return remote.queueReady();}), "Cider queue accepts an app token");
    check(remote.queue().size()==2 && remote.queue()[1].toMap()["title"]=="Second Light", "Cider queue rows preserve order and metadata");
    check(remote.queue()[0].toMap()["artwork"] == "https://example.invalid/cover/96x96.jpg", "Cider queue artwork URLs request thumbnail dimensions");
    check(pagesSeen>=2 && remote.currentIndex()==1, "Cider v2 pagination loads all pages and the authoritative queue position");
    int modelChanges=0, indexChanges=0;
    QObject::connect(&remote,&Cider::queueChanged,&remote,[&]{++modelChanges;});
    QObject::connect(&remote,&Cider::currentIndexChanged,&remote,[&]{++indexChanges;});
    queuePosition=0; remote.refreshQueue();
    check(waitFor([&]{return !remote.queueBusy();}) && remote.currentIndex()==0 && indexChanges==1 && modelChanges==0,
          "Cider track advancement updates the highlight without rebuilding queue rows");
    queuePosition=1;
    failSecondPage=true; remote.refreshQueue();
    check(waitFor([&]{return !remote.queueBusy() && !remote.queueError().isEmpty();}), "Cider queue reports failed page refresh");
    check(remote.queueReady() && remote.queue().size()==2 && !remote.needsToken(), "failed queue refresh preserves rows without misreporting authentication");
    failSecondPage=false; remote.refreshQueue();
    check(waitFor([&]{return !remote.queueBusy() && remote.queueError().isEmpty();}), "Cider queue recovers after endpoint failure");
    remote.select(1);
    check(waitFor([&]{return selected==1;}), "Cider queue selection sends the correct provider index");
    check(waitFor([&]{return !remote.controlBusy()&&!remote.queueBusy();}),"queue selection refresh settles before editing");
    remote.refreshModes();check(waitFor([&]{return remote.modesReady();})&&!remote.shuffle()&&!remote.autoplay(),"Cider v2 reports shuffle and autoplay modes");
    remote.setShuffle(true);remote.setShuffle(true);
    check(waitFor([&]{return !remote.controlBusy();})&&remote.shuffle()&&shuffleMode&&modeEdits==1,"shuffle uses one API toggle and confirms the actual mode");
    waitFor([&]{return !remote.queueBusy();});
    remote.setAutoplay(true);check(waitFor([&]{return !remote.controlBusy();})&&remote.autoplay()&&autoplayMode,"autoplay reads back the enabled mode");
    waitFor([&]{return !remote.queueBusy();});
    remote.setAutoplay(false);check(waitFor([&]{return !remote.controlBusy();})&&!remote.autoplay(),"autoplay can be turned off again");
    waitFor([&]{return !remote.queueBusy();});
    const int modeCount=modeEdits;remote.setShuffle(true);waitFor([&]{return !remote.controlBusy();});check(modeEdits==modeCount,"setting an already active mode does not toggle it off");
    const int oldRevision=remote.queueRevision();
    remote.moveQueue(0,1,oldRevision);remote.moveQueue(0,1,oldRevision);
    check(waitFor([&]{return !remote.controlBusy()&&!remote.queueBusy();})&&queueEdits==1&&remote.queue()[1].toMap()["id"]=="one"&&remote.currentIndex()==0,"queue move preserves current song and ignores duplicate clicks");
    remote.removeQueue(0,oldRevision);QTest::qWait(50);check(queueEdits==1,"stale queue menu cannot remove a different song");
    const int revision=remote.queueRevision();queueTracks=QJsonDocument::fromJson(body).array();queuePosition=1;
    remote.removeQueue(0,revision);
    check(waitFor([&]{return !remote.controlBusy()&&!remote.queueBusy();})&&queueEdits==1&&remote.queue()[0].toMap()["id"]=="one","preflight catches external queue changes before removal");
    rejectControl=true;remote.removeQueue(0,remote.queueRevision());
    check(waitFor([&]{return !remote.controlBusy()&&!remote.queueBusy();})&&remote.queue().size()==2&&queueEdits==1,"failed queue edits preserve the displayed tracks");rejectControl=false;
    remote.removeQueue(0,remote.queueRevision());
    check(waitFor([&]{return !remote.controlBusy()&&!remote.queueBusy();})&&remote.queue().size()==1&&remote.queue()[0].toMap()["id"]=="two","queue removal displays the authoritative remaining track");
    queueTracks=QJsonDocument::fromJson(body).array();queuePosition=1;remote.refreshQueue();waitFor([&]{return !remote.queueBusy();});
    remote.setDiscVisible(true);
    check(waitFor([&]{return !remote.discLoading();}), "Cider album request finishes");
    check(remote.discDetails().value("year")=="2026" && remote.discDetails().value("tracks").toList().size()==2 && albumRequests==2, "Cider reverse loads release year and all album track pages");
    remote.setDiscVisible(false); remote.setDiscVisible(true);
    check(waitFor([&]{return !remote.discLoading();}) && albumRequests==2, "reopening the same album reuses cached details");
    remote.setDiscVisible(false); invalidAlbumLink=true; remote.setDiscVisible(true);
    check(waitFor([&]{return !remote.discLoading();}) && !remote.discError().isEmpty() && remote.discDetails().isEmpty(), "tracks without catalog metadata never show stale album details");
    remote.setDiscVisible(false); invalidAlbumLink=false; remote.setDiscVisible(true); remote.setDiscVisible(false);
    QTest::qWait(100);
    check(!remote.discLoading() && remote.discDetails().isEmpty(), "closing the reverse discards in-flight album responses");
    Player lyricsPlayer(temp+"/lyrics-player.ini");
    Lyrics remoteLyrics(&lyricsPlayer,&remote); remoteLyrics.setRemote(true); remoteLyrics.setActive(true);
    check(waitFor([&]{return !remoteLyrics.loading();}) && remoteLyrics.lines().size()==2 && remoteLyrics.timed(), "Cider lyrics use its authenticated catalog proxy and parse timed lines");
    remoteLyrics.setActive(false);
    emptyLyrics=true;
    Lyrics absentLyrics(&lyricsPlayer,&remote); absentLyrics.setRemote(true); absentLyrics.setActive(true);
    check(waitFor([&]{return !absentLyrics.loading();}) && absentLyrics.lines().isEmpty() && absentLyrics.message()=="No lyrics available", "instrumental or missing lyrics have a quiet empty state");
    absentLyrics.setActive(false);
    Lyrics cancelledLyrics(&lyricsPlayer,&remote); cancelledLyrics.setRemote(true); cancelledLyrics.setActive(true); cancelledLyrics.setActive(false);
    QTest::qWait(100);
    check(!cancelledLyrics.loading() && cancelledLyrics.lines().isEmpty(), "closing lyrics discards pending replies");
    remote.setQueueVisible(false);
    const auto permissions=QFile::permissions(file);
    check(QFile::exists(file) && !(permissions & (QFileDevice::ReadGroup|QFileDevice::WriteGroup|QFileDevice::ReadOther|QFileDevice::WriteOther)), "Cider token is saved with private file permissions");
    Cider restored(false,file,base); restored.refreshQueue();
    check(waitFor([&]{return restored.queueReady() && restored.queue().size()==2;}), "Cider queue connection survives relaunch");
    remote.setDiscVisible(false);
    QImage red(24,24,QImage::Format_ARGB32), blue=red;
    red.fill(Qt::red); blue.fill(Qt::blue);
    red.save(temp+"/red.png"); blue.save(temp+"/blue.png");
    auto artworkUrl = [&](const QUrl &url) {
        const QVariantMap changes{{"Metadata",QVariantMap{{"mpris:artUrl",url.toString()}}}};
        return QMetaObject::invokeMethod(&remote,"propertiesChanged",Qt::DirectConnection,
            Q_ARG(QString,QString("org.mpris.MediaPlayer2.Player")),Q_ARG(QVariantMap,changes),Q_ARG(QStringList,QStringList{}));
    };
    check(artworkUrl(QUrl::fromLocalFile(temp+"/red.png")), "Cider artwork fixture delivers metadata");
    artworkUrl(QUrl::fromLocalFile(temp+"/blue.png"));
    check(waitFor([&]{return !remote.artwork().isNull() && remote.artwork().pixelColor(0,0)==QColor(Qt::blue);}),
          "asynchronous Cider artwork keeps the latest cover and original pixels");
    QImage large(3600,2400,QImage::Format_RGB32); large.fill(Qt::cyan);
    large.save(temp+"/large-cover.jpg","JPEG",95);
    artworkUrl(QUrl::fromLocalFile(temp+"/large-cover.jpg"));
    check(waitFor([&]{return remote.artwork().size()==QSize(1200,800);}),
          "oversized Cider covers use the full disc resolution with correct proportions");
    artworkUrl(QUrl::fromLocalFile(temp+"/red.png")); artworkUrl({}); QTest::qWait(150);
    check(remote.artwork().isNull(), "disconnecting while Cider artwork decodes rejects the stale image");
    return failures;
}

static int exercise(Player &player, Theme &theme, Lyrics &lyrics, QQuickWindow *window,
                    const QString &temp, const QString &captures, bool live) {
    int failures = 0;
    auto check = [&](bool ok, const char *message) {
        std::cout << (ok ? "PASS " : "FAIL ") << message << std::endl;
        if (!ok) ++failures;
    };
    std::function<QQuickItem *(QQuickItem *, const QString &)> findItem = [&](QQuickItem *parent, const QString &name) -> QQuickItem * {
        if (parent->objectName() == name) return parent;
        for (auto *child : parent->childItems()) if (auto *item = findItem(child, name)) return item;
        return nullptr;
    };
    auto click = [&](const char *name) {
        focusTestWindow(window);
        auto *item = findItem(window->contentItem(), QString::fromLatin1(name));
        if (!item) { check(false, name); return; }
        const auto point = item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point); QTest::qWait(150);
    };
    auto capture = [&](const QString &name) {
        if (captures.isEmpty()) return;
        QTest::mouseMove(window, window->property("miniMode").toBool() ? QPoint(2, 2) : QPoint(15, 15));
        QDir().mkpath(captures); QTest::qWait(300);
        check(window->grabWindow().save(captures + "/" + name + ".png"), "capture saved");
    };
    const auto parsed=Lyrics::parse("[offset:250]\n[00:01.00][00:03.50]A quiet room\n[00:05.00]An open window");
    check(parsed.size()==3 && parsed[1].toMap()["start"].toLongLong()==3750 && Lyrics::indexAt(parsed,1300)==0 && Lyrics::indexAt(parsed,3900)==1, "LRC offsets and repeated timestamps align with playback");
    check(Lyrics::parse("First line\nSecond line").size()==2 && Lyrics::indexAt(Lyrics::parse("Plain words"),5000)==-1, "plain lyrics remain readable without invented timing");
    check(Lyrics::parse("<tt><body><p>Broken").isEmpty(), "malformed lyrics are rejected");
    const QVariantList overlappingLyrics{
        QVariantMap{{"start",100},{"end",900}},
        QVariantMap{{"start",300},{"end",500}},
        QVariantMap{{"start",200},{"end",400}},
        QVariantMap{{"start",300},{"end",300}},
        QVariantMap{{"start",1000},{"end",-1}},
        QVariantMap{{"start",1000},{"end",1100}},
        QVariantMap{{"start",-1},{"end",2000}}};
    LyricTimeline timeline;timeline.reset(overlappingLyrics);
    bool timelineMatches=true;
    for (qint64 position : {-1,0,99,100,199,200,299,300,399,400,499,500,899,900,999,1000,1099,1100,2000})
        timelineMatches &= timeline.indexAt(position)==Lyrics::indexAt(overlappingLyrics,position);
    check(timelineMatches, "indexed lyrics preserve gaps, overlap precedence, duplicate starts and exclusive ends");
    timeline.reset(parsed);
    check(timeline.indexAt(1300)==0 && timeline.indexAt(3900)==1, "replacing lyrics rebuilds the timing index");
    timeline.reset({});
    check(timeline.indexAt(3900)==-1, "clearing lyrics removes the previous timing index");
    DiscPresentation staged;
    int swaps=0; QObject::connect(&staged,&DiscPresentation::swapRequested,&staged,[&]{++swaps;});
    QImage first(16,16,QImage::Format_ARGB32);first.fill(Qt::red);QImage second=first;second.fill(Qt::blue);
    staged.present(first,"album-one",true); staged.present(first,"album-one",true);
    check(swaps==0, "startup and tracks from the same album do not swap discs");
    staged.present({},"album-two",true,true);check(staged.artwork()==first, "old artwork stays until the incoming cover arrives");
    staged.present(second,"album-two",true,true);check(swaps==1 && staged.outgoing()==first && staged.artwork()==second, "album swap retains the outgoing artwork separately");
    staged.present(first,"album-three",false);check(swaps==1 && staged.artwork()==first, "reduced motion replaces artwork immediately");
    staged.releaseOutgoing();check(staged.outgoing().isNull() && staged.artwork()==first,
          "finished disc swaps release the previous cover without changing the current disc");
    check(player.count() == 0, "isolated empty queue");
    auto *bar = window->findChild<QQuickItem *>("sourceBar");
    check(bar && qAbs(bar->x()+bar->width()/2-265) < .1, "source bar centered over CD");
    auto *songTitle = window->findChild<QQuickItem *>("songTitle");
    check(songTitle && songTitle->property("font").value<QFont>().weight() == QFont::Normal, "song title uses regular weight");
    check(window->property("font").value<QFont>().family() == QFontInfo(QFontDatabase::systemFont(QFontDatabase::GeneralFont)).family(), "new profiles follow the system font");
    auto *type = qobject_cast<Typography *>(QQmlEngine::contextForObject(window)->contextProperty("typography").value<QObject *>());
    check(type && type->families().isEmpty(), "installed fonts are listed only when requested");
    check(!window->findChild<QObject *>("fontPicker"), "font picker objects are absent from startup");
    if (type) {
        type->loadFamilies();
        QString chosen;
        for (const auto &family : type->families()) if (family != type->systemFamily()) { chosen = family; break; }
        if (type->systemFamily() != "Google Sans Flex" && QFontDatabase::hasFamily("Google Sans Flex")) chosen = "Google Sans Flex";
        else if (type->systemFamily() != "Noto Sans" && QFontDatabase::hasFamily("Noto Sans")) chosen = "Noto Sans";
        check(!chosen.isEmpty() && chosen != type->systemFamily(), "an installed alternate font is available for the picker check");
        window->setProperty("queueOpen",true);QTest::qWait(200);
        click("menuButton");QTest::qWait(200);
        auto *settingsPopup=window->findChild<QObject *>("settingsMenu");
        const auto menuVolume=player.volume();
        testKeyClick(window,Qt::Key_Down);testKeyClick(window,Qt::Key_Down);
        check(settingsPopup&&settingsPopup->property("currentIndex").toInt()>=0&&player.volume()==menuVolume,"menu arrow navigation moves focus without changing playback volume");
        testKeyClick(window,Qt::Key_Escape);QTest::qWait(200);
        check(!settingsPopup->property("visible").toBool()&&window->property("queueOpen").toBool(),"Escape dismisses the settings menu without closing the queue behind it");
        window->setProperty("queueOpen",false);QTest::qWait(200);
        click("menuButton"); QTest::qWait(250);
        auto *fontLoader=window->findChild<QObject *>("fontPickerLoader");
        auto *fontPreferences=window->findChild<QObject *>("preferencesPopup");
        QSignalMapper fontLoaded;
        QElapsedTimer fontPrepareClock; int fontPrepareMs=-1; bool readyBeforePreferences=false;
        if (fontLoader) {
            fontLoaded.setMapping(fontLoader,1);
            QObject::connect(fontLoader,SIGNAL(loaded()),&fontLoaded,SLOT(map()));
            QObject::connect(&fontLoaded,&QSignalMapper::mappedInt,window,[&] {
                fontPrepareMs=fontPrepareClock.elapsed();
                readyBeforePreferences=!fontPreferences->property("opened").toBool();
            });
        }
        fontPrepareClock.start();click("preferencesAction");QTest::qWait(250);
        check(waitFor([&] { return window->findChild<QObject *>("fontPicker") != nullptr; }), "Preferences prepares the font picker asynchronously");
        std::cout << "FONT_PREPARE_MS " << fontPrepareMs << std::endl;
        check(readyBeforePreferences, "cold font picker preparation finishes during the Preferences entrance");
        auto *cdChoice=findItem(window->contentItem(),"cdStyleButton");
        auto *vinylChoice=findItem(window->contentItem(),"vinylStyleButton");
        const bool priorVinyl=player.vinyl();
        cdChoice->forceActiveFocus(Qt::TabFocusReason);testKeyClick(window,Qt::Key_Right);
        check(vinylChoice->hasActiveFocus()&&player.vinyl()==priorVinyl,"appearance arrows move focus without changing the disc");
        testKeyClick(window,Qt::Key_Return);check(player.vinyl(),"Enter selects the focused Vinyl appearance");
        testKeyClick(window,Qt::Key_Left);testKeyClick(window,Qt::Key_Return);
        check(!player.vinyl(),"keyboard returns to the CD appearance");player.setVinyl(priorVinyl);
        auto *motionSwitch=findItem(window->contentItem(),"motionToggle");
        if (motionSwitch) {
            motionSwitch->forceActiveFocus(Qt::TabFocusReason);
            const bool priorMotion=player.motion();
            testKeyClick(window,Qt::Key_Space);
            check(player.motion()!=priorMotion,"Preferences switch toggles its setting with Space");
            testKeyClick(window,Qt::Key_Space);
            check(player.motion()==priorMotion,"Preferences switch restores its setting with the keyboard");
        } else check(false,"Preferences motion switch exists");
        click("fontChoice");
        auto *picker = window->findChild<QObject *>("fontPicker");
        check(picker && waitFor([&] { return picker->property("opened").toBool(); }), "font picker opens from Preferences");
        auto *search = window->findChild<QQuickItem *>("fontSearch");
        auto *list = window->findChild<QQuickItem *>("fontList");
        if (search && list && picker && !chosen.isEmpty()) {
            search->setProperty("text", "spun-font-that-does-not-exist");
            check(list->property("count").toInt() == 0, "font search handles an empty result");
            search->setProperty("text", chosen); QTest::qWait(50);
            check(list->property("count").toInt() > 0 && search->hasActiveFocus(), "installed fonts can be searched with keyboard focus");
            capture("00-font-picker");
            testKeyClick(window, Qt::Key_Down);
            check(list->hasActiveFocus() && list->property("currentIndex").toInt()==0,"font keyboard navigation focuses the first result without selecting it");
            capture("20-font-keyboard-focus");
            testKeyClick(window, Qt::Key_Return);
            check(waitFor([&] { return !picker->property("visible").toBool(); }) && type->selectedFamily() == chosen,
                  "keyboard selection applies the installed font and closes the picker");
            check(window->property("font").value<QFont>().family() == chosen && songTitle->property("font").value<QFont>().family() == chosen,
                  "font choice updates the window and inherited song typography immediately");
            capture("00-font-preferences");
            { Typography restored(temp + "/player.ini"); check(restored.selectedFamily() == chosen && restored.family() == chosen, "font choice survives relaunch"); }
            check(!type->select("spun-font-that-does-not-exist") && type->selectedFamily() == chosen, "invalid font choices cannot replace the active font");
            click("fontChoice"); waitFor([&] { return picker->property("opened").toBool(); });
            search->setProperty("text", "spun-font-that-does-not-exist");
            testKeyClick(window, Qt::Key_Escape);
            check(waitFor([&] { return !picker->property("visible").toBool(); }) && type->selectedFamily() == chosen
                  && window->findChild<QObject *>("preferencesPopup")->property("visible").toBool(),
                  "Escape dismisses font search without changing the selection or closing Preferences");
            click("fontChoice"); waitFor([&] { return picker->property("opened").toBool(); });
            click("systemFontChoice"); waitFor([&] { return !picker->property("visible").toBool(); });
            check(type->selectedFamily().isEmpty() && type->family() == type->systemFamily(), "System default resets the saved font override");
        }
        type->select("");
        if (picker) QMetaObject::invokeMethod(picker, "close");
        click("closePreferences"); QTest::qWait(200);
        const auto unavailableFile = temp + "/missing-font.ini";
        { QSettings prefs(unavailableFile, QSettings::IniFormat); prefs.setValue("fontFamily", "spun-font-that-does-not-exist"); }
        Typography unavailable(unavailableFile);
        check(unavailable.missing() && unavailable.family() == unavailable.systemFamily()
              && unavailable.selectedFamily() == "spun-font-that-does-not-exist", "a removed font falls back safely without losing the saved choice");
        check(!QFile::exists(":/assets/fonts/GoogleSansFlex.ttf"), "release resources contain no bundled font");
    }
    const auto centerY = [&](const char *name) {
        auto *item = findItem(window->contentItem(), QString::fromLatin1(name));
        return item ? item->mapToScene(QPointF(item->width()/2,item->height()/2)).y() : -1.;
    };
    const auto playY = centerY("playButton");
    check(playY > 0 && qAbs(centerY("previousButton") - playY) < .1 && qAbs(centerY("nextButton") - playY) < .1
          && qAbs(centerY("shuffleButton") - playY) < .1 && qAbs(centerY("repeatButton") - playY) < .1,
          "transport icons share one centerline despite the larger play button");
    const auto bottomY = [&](const char *name) {
        auto *item = findItem(window->contentItem(), QString::fromLatin1(name));
        return item ? item->mapToScene(QPointF(0, item->height())).y() : -1.;
    };
    const auto deckBottom = bottomY("playerDeck");
    auto *queuePanel = findItem(window->contentItem(), "queuePanel");
    check(bar && queuePanel && deckBottom > 0
          && qAbs(queuePanel->y() - bar->y()) < .1 && qAbs(bottomY("queuePanel") - deckBottom) < .1,
          "queue aligns with the source bar top and player card bottom");
    check(qAbs(centerY("queueFooter") - playY) < .1,
          "queue footer actions align with playback controls");
    auto *localTab=findItem(window->contentItem(),"localSourceButton");
    auto *ciderTab=findItem(window->contentItem(),"ciderSourceButton");
    localTab->forceActiveFocus(Qt::TabFocusReason);
    testKeyClick(window,Qt::Key_Right);
    check(ciderTab->hasActiveFocus()&&!window->property("useCider").toBool(),"source arrows move focus without switching playback source");
    testKeyClick(window,Qt::Key_Return);
    check(window->property("useCider").toBool(),"Enter activates the focused Cider source");
    testKeyClick(window,Qt::Key_Left);testKeyClick(window,Qt::Key_Return);
    check(localTab->hasActiveFocus()&&!window->property("useCider").toBool(),"source keyboard navigation returns to Local");
    auto *volumeControl=findItem(window->contentItem(),"volumeSlider");
    const qreal originalVolume=player.volume();player.setVolume(.5);QTest::qWait(20);
    volumeControl->forceActiveFocus(Qt::TabFocusReason);testKeyClick(window,Qt::Key_Right);
    check(player.volume()>.5,"focused volume slider receives Right instead of the global seek shortcut");
    testKeyClick(window,Qt::Key_Left);
    check(qAbs(player.volume()-.5)<.01,"focused volume slider receives Left and restores the previous level");
    player.setVolume(originalVolume);
    click("ciderSourceButton"); check(window->property("useCider").toBool(), "Cider source selector");
    click("localSourceButton"); check(!window->property("useCider").toBool(), "local source selector");
    auto *sourceIndicator = findItem(window->contentItem(), "sourceIndicator");
    check(sourceIndicator && waitFor([&] { return qAbs(sourceIndicator->x() - 56) < .25; }),
          "source selection spring settles after reversing direction");
    window->setProperty("useCider", true); QTest::qWait(20); player.setMotion(false); QTest::qWait(20);
    check(sourceIndicator && qAbs(sourceIndicator->x() - 128) < .1,
          "reduced motion settles an in-flight selection spring immediately");
    window->setProperty("useCider", false); player.setMotion(true);
    auto *hoverButton = findItem(window->contentItem(), "menuButton");
    auto *hoverLayer = hoverButton ? findItem(hoverButton, "interactionStateLayer") : nullptr;
    if (hoverLayer) {
        QTest::mouseMove(window, QPoint(15, 15)); QTest::qWait(160);
        const auto ink = hoverLayer->property("color").value<QColor>();
        const auto point = hoverButton->mapToScene(QPointF(20, 20)).toPoint();
        qreal previous = hoverLayer->opacity(); bool smooth = previous < .001;
        QTest::mouseMove(window, point);
        for (int i = 0; i < 10; ++i) {
            QTest::qWait(16);
            const auto opacity = hoverLayer->opacity();
            smooth &= opacity + .001 >= previous && opacity <= .081 && hoverLayer->property("color").value<QColor>() == ink;
            previous = opacity;
        }
        check(smooth && qAbs(previous - .08) < .001, "hover fades monotonically with fixed RGB and no intermediate color flash");
        for (int i = 0; i < 3; ++i) {
            QTest::mouseMove(window, QPoint(15, 15)); QTest::qWait(24);
            QTest::mouseMove(window, point); QTest::qWait(24);
        }
        QTest::mouseMove(window, QPoint(15, 15)); QTest::qWait(160);
        check(hoverLayer->opacity() < .001 && hoverLayer->property("color").value<QColor>() == ink,
              "rapid hover reversals settle without changing the hover hue");
    } else check(false, "hover state layer exists");
    capture("01-empty");
    const auto demo = QStringLiteral(SPUN_SOURCE_DIR "/assets/First-Light.flac");
    const auto other = temp + "/Second-Light.flac";
    QFile::copy(demo, other);
    { TagLib::FileRef tagged(other.toUtf8().constData()); tagged.tag()->setTitle("Second Light"); tagged.tag()->setYear(2026); tagged.tag()->setTrack(2); tagged.save(); }
    {
        QueueArtworkProvider thumbnails;
        auto thumb = [&](const QString &track, const QString &cover, int size=96) {
            return thumbnails.requestImage(queueArtworkUrl(track,cover).mid(QStringLiteral("image://queueart/").size()),nullptr,QSize(size,size));
        };
        const auto first = thumb(demo,{}), second = thumb(other,{});
        check(!first.isNull() && first.cacheKey()==second.cacheKey(),
              "tracks with identical embedded artwork share decoded thumbnail pixels");
        const auto small = thumb(demo,{},64);
        check(small.size()==QSize(64,64) && first.size()==QSize(96,96),
              "thumbnail cache separates requested resolutions");
        const auto coverPath=temp+"/cache-cover.png";
        QImage colored(128,128,QImage::Format_ARGB32);colored.fill(Qt::red);colored.save(coverPath);
        const auto redThumb=thumb(demo,coverPath);
        colored.fill(Qt::blue);colored.save(coverPath);
        QFile changedCover(coverPath);
        check(changedCover.open(QIODevice::ReadWrite) && changedCover.setFileTime(QDateTime::currentDateTime().addSecs(2),QFileDevice::FileModificationTime),
              "cover edit fixture updates its modification time");
        changedCover.close();
        const auto blueThumb=thumb(demo,coverPath);
        check(redThumb.pixelColor(0,0)==QColor(Qt::red) && blueThumb.pixelColor(0,0)==QColor(Qt::blue),
              "edited cover files invalidate cached thumbnails");
        for (int i=0;i<40;++i) {const auto path=temp+"/cache-"+QString::number(i)+".png";colored.save(path);thumb(demo,path);}
        const auto reloaded=thumb(demo,coverPath);
        check(reloaded==blueThumb && reloaded.cacheKey()!=blueThumb.cacheKey(),
              "thumbnail cache evicts old entries within its memory budget and reloads identical pixels");
    }
    write(temp+"/Second-Light.lrc", "[00:00.00]Soft light on the water\n[00:05.00]Another turn around the sun\n[00:10.00]The quiet room begins to glow\n[00:15.00]We let the evening slowly go\n[00:20.00]A silver circle turns again\n[00:25.00]The light comes home\n");
    player.setVolume(live ? .12 : 0);
    player.addUrls({QUrl::fromLocalFile(demo), QUrl::fromLocalFile(other)}, false);
    check(waitFor([&] { return !player.busy(); }), "asynchronous import completes");
    check(player.count() == 2 && player.currentIndex() == 0, "multiple files populate queue");
    check(player.title() == "First Light", "embedded track title");
    check(player.artist() == "Spun Sound Lab", "embedded artist");
    check(waitFor([&] { return !player.artworkLoading() && !player.artwork().isNull(); }), "embedded cover decoded onto disc asynchronously");
    check(waitFor([&] { return player.duration() > 10000; }), "audio duration decoded");
    QElapsedTimer firstPlayCall; firstPlayCall.start();
    player.play(); player.pause();
    check(firstPlayCall.elapsed()<250 && !player.playing(), "first Play stays responsive and can be cancelled during audio initialization");
    QTest::qWait(1300);
    check(!player.playing() && player.position()==0, "cancelled audio initialization never starts playback later");
    player.addUrls({QUrl::fromLocalFile(demo)}, false);
    waitFor([&] { return !player.busy(); });
    check(player.count() == 2, "duplicate paths excluded");
    window->requestActivate(); QTest::qWait(250);
    click("playButton");
    check(waitFor([&] { return player.playing() && player.position() > 300; }), "play button starts real audio decoding");
    QTest::qWait(350);
    check(window->property("spinAngle").toDouble() > 0, "disc rotates with playback");
    auto *ring = window->findChild<ProgressRing *>("progressRing");
    check(ring && ring->amplitude() > 2 && ring->phase() > 0, "outer progress ring waves during playback");
    player.seek(12000); QTest::qWait(250);
    capture("02-wavy-rim");
    click("playButton");
    QTest::qWait(300);
    check(ring && ring->amplitude() == 0, "outer progress ring settles on pause");
    check(!player.playing(), "play button pauses");
    player.seek(7000);
    check(waitFor([&] { return qAbs(player.position()-7000) < 300; }), "seek changes actual media position");
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(481, 294));
    check(waitFor([&] { return qAbs(player.position()-player.duration()/4) < 350; }), "outer CD rim scrubs playback");
    click("shuffleButton"); check(player.shuffle(), "shuffle control toggles");
    click("repeatButton"); check(player.repeatMode() == 1, "repeat control cycles");
    player.setShuffle(false); player.setRepeatMode(0);
    click("nextButton"); check(player.currentIndex() == 1, "next track control");
    player.pause();
    click("queueButton");
    check(window->property("queueOpen").toBool() && window->width() == 860, "jewel case opens and window expands");
    check(waitFor([&] {
        auto *thumb = findItem(window->contentItem(), "queueArtwork0");
        return thumb && thumb->property("status").toInt() == 1;
    }), "local queue artwork loads asynchronously beside the title");
    check(waitFor([&]{ return !player.artworkLoading() && !window->property("swapRunning").toBool(); }), "cover and disc swap settle before disc interaction");
    const auto queueResumePosition=player.position();
    player.seek(10000);QTest::qWait(120);
    qint64 expectedRemaining=0;const auto localRows=player.queue();
    for(int i=player.currentIndex();i<localRows.size();++i)expectedRemaining+=localRows[i].toMap().value("duration").toLongLong();
    expectedRemaining-=player.position();
    check(qAbs(window->property("queueRemainingMs").toDouble()-expectedRemaining)<250,"queue listening time subtracts elapsed local playback");
    player.seek(queueResumePosition);QTest::qWait(120);
    capture("02-playing-queue-dark");
    check(!findItem(window->contentItem(),"vinylTonearm"),"CD mode does not allocate a tonearm");
    const auto vinylPosition=player.position(); const auto vinylTrack=player.currentIndex();
    player.setVinyl(true);QTest::qWait(250);
    check(window->property("vinyl").toBool()&&player.currentIndex()==vinylTrack&&player.position()==vinylPosition&&!player.playing(),"Vinyl switches appearance without changing playback");
    { Player restored(temp+"/player.ini"); check(restored.vinyl(),"Vinyl preference survives relaunch"); }
    check(!window->mask().contains(QPoint(265,294))&&window->mask().contains(QPoint(280,294)),"Vinyl uses a small spindle hole in the input mask");
    auto *tonearm=findItem(window->contentItem(),"vinylTonearm");
    auto *shaft=findItem(window->contentItem(),"tonearmShaft");
    check(tonearm&&shaft&&tonearm->isVisible(),"vinyl creates a decorative tonearm");
    if(tonearm&&shaft) {
        check(tonearm->property("lowered").toDouble()==0,"paused vinyl parks its tonearm");
        capture("14-tonearm-parked");
        player.play();player.seek(0);QTest::qWait(650);
        const double outerAngle=tonearm->property("armAngle").toDouble();
        check(tonearm->property("lowered").toDouble()>.99,"playing lowers the tonearm onto the grooves");
        capture("14-tonearm-playing");
        player.seek(player.duration()*.85);QTest::qWait(650);
        check(tonearm->property("armAngle").toDouble()>outerAngle+12,"tonearm tracks inward as song progress advances");
        capture("14-tonearm-inner");
        player.setMiniMode(true);QTest::qWait(450);
        const auto tip=shaft->mapToScene(QPointF(0,213));
        check(tonearm->isVisible()&&tip.x()>0&&tip.x()<300&&tip.y()>0&&tip.y()<268,"Mini tonearm stays on the record above its controls");
        capture("14-tonearm-mini");player.setMiniMode(false);QTest::qWait(250);
        player.pause();QTest::qWait(500);
        check(tonearm->property("lowered").toDouble()==0&&qAbs(tonearm->property("armAngle").toDouble()+4)<.01,"pausing lifts and parks the arm");
        const bool motion=player.motion();player.setMotion(false);player.play();QTest::qWait(80);
        check(tonearm->property("lowered").toDouble()==1,"reduced motion lowers the arm without animation");
        player.pause();QTest::qWait(30);check(tonearm->property("lowered").toDouble()==0,"reduced motion parks the arm immediately");
        player.setMotion(motion);player.seek(vinylPosition);QTest::qWait(150);
        window->setProperty("discFlipped",true);QTest::qWait(400);
        check(!tonearm->isVisible(),"reverse view hides the tonearm and preserves its content");
        window->setProperty("discFlipped",false);QTest::qWait(400);
        const auto shaftPoint=shaft->mapToScene(QPointF(0,140)).toPoint();
        QTest::mouseDClick(window,Qt::LeftButton,Qt::NoModifier,shaftPoint);QTest::qWait(400);
        check(window->property("discFlipped").toBool(),"tonearm decoration does not intercept disc gestures");
        window->setProperty("discFlipped",false);QTest::qWait(400);
    }
    capture("11-vinyl-front");
    QTest::mouseMove(window,QPoint(265,185)); QTest::qWait(100);
    QTest::mouseDClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(265,185));
    check(waitFor([&]{return window->property("discFlipped").toBool();}), "double-click flips the CD without playing audio");
    QTest::qWait(400);
    check(!player.playing() && player.discDetails().value("year")=="2026" && player.discDetails().value("tracks").toList().size()==2, "local reverse shows tagged album year and its loaded tracks");
    check(window->findChild<QQuickItem *>("discSides")->property("side").toInt()==1, "flip animation reveals the back face");
    check(!window->mask().contains(QPoint(265,294)), "reverse preserves the spindle input hole");
    auto *albumYear=window->findChild<QQuickItem *>("discAlbumYear");
    auto *albumArtist=window->findChild<QQuickItem *>("discAlbumArtist");
    auto *albumTitle=window->findChild<QQuickItem *>("discAlbumTitle");
    check(albumTitle->y()+albumTitle->height()<=albumArtist->y() && albumArtist->y()+albumArtist->height()<=albumYear->y() && albumYear->y()+albumYear->height()<=150, "album heading has clear spacing above the center hub");
    for (int row=0; row<2; ++row) {
        auto *number=findItem(window->contentItem(), "albumNumber"+QString::number(row));
        auto *title=findItem(window->contentItem(), "albumTitle"+QString::number(row));
        auto *duration=findItem(window->contentItem(), "albumDuration"+QString::number(row));
        check(number && title && duration && number->x()+number->width()<=title->x() && title->x()+title->width()<duration->x(), "album track columns have non-overlapping bounds");
    }
    capture("02-disc-back");capture("11-vinyl-reverse");
    click("lyricsViewButton");
    check(waitFor([&]{return !lyrics.loading() && lyrics.lines().size()==6;}), "lyrics toggle loads a matching local LRC file");
    player.seek(5500);check(waitFor([&]{return lyrics.currentIndex()==1;}), "timed lyrics follow seeking");
    capture("02-lyrics");
    click("lyricHit2");
    check(waitFor([&]{return qAbs(player.position()-10000)<200;}), "clicking a timed lyric seeks to its timestamp");
    check(!player.playing() && window->property("discFlipped").toBool(), "lyric seeking preserves pause and the reverse view");
    auto *clickedLine=findItem(window->contentItem(),"lyricHit2");
    const auto lyricPoint=clickedLine->mapToScene(QPointF(clickedLine->width()/2,clickedLine->height()/2)).toPoint();
    QTest::mouseDClick(window,Qt::LeftButton,Qt::NoModifier,lyricPoint);QTest::qWait(150);
    check(window->property("discFlipped").toBool(), "double-clicking lyrics does not flip the CD");
    const auto beforeLyricDrag=player.position();
    QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,lyricPoint);
    QTest::mouseMove(window,lyricPoint-QPoint(0,15),50);
    QTest::mouseMove(window,lyricPoint-QPoint(0,45),50);
    QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,lyricPoint-QPoint(0,45));QTest::qWait(150);
    check(player.position()==beforeLyricDrag, "dragging the lyric list scrolls without seeking");
    player.seek(5500);
    auto *lyricList=window->findChild<QQuickItem *>("lyricList");
    lyricList->forceActiveFocus();const auto lyricsVolume=player.volume();
    testKeyClick(window,Qt::Key_Down);QTest::qWait(100);
    check(!lyricList->property("following").toBool() && player.volume()==lyricsVolume, "manual lyric browsing pauses following without adjusting volume");
    click("followLyricsButton");check(lyricList->property("following").toBool(), "follow control returns to the current lyric");
    player.setMiniMode(true);QTest::qWait(400);capture("02-mini-lyrics");capture("11-vinyl-mini-lyrics");
    check(window->width()==300 && lyrics.active(), "lyrics remain readable and active in Mini mode");
    click("lyricHit1");check(waitFor([&]{return qAbs(player.position()-5000)<200;}), "timed lyric clicks work at Mini mode scale");
    player.setMiniMode(false);QTest::qWait(200);
    click("lyricsViewButton");check(!lyrics.active(), "returning to album tracks stops lyric requests");
    check(!lyrics.seekToLine(0) && !lyrics.seekToLine(-1), "hidden or invalid lyric targets cannot seek");
    window->setProperty("queueOpen",true);QTest::qWait(200);
    click("discReturnButton"); QTest::qWait(400);
    check(!window->property("discFlipped").toBool(), "visible return control restores artwork");
    testKeyClick(window, Qt::Key_F); QTest::qWait(400);
    testKeyClick(window, Qt::Key_F); QTest::qWait(400);
    check(!window->property("discFlipped").toBool(), "F returns to artwork");
    click("queueSearchButton");
    auto *search = window->findChild<QQuickItem *>("queueSearchInput");
    auto *list = window->findChild<QQuickItem *>("trackList");
    check(search && search->hasActiveFocus(), "queue search opens with text focus");
    for (char ch : QByteArray("LIGHT second")) testKeyClick(window, ch);
    QTest::qWait(100);
    check(list->property("count").toInt()==1 && findItem(window->contentItem(), "queueRow1"), "search matches words in any order and preserves provider indices");
    const auto searchPosition = player.position();
    const auto searchVolume = player.volume();
    testKeyClick(window, Qt::Key_Left); testKeyClick(window, Qt::Key_M); testKeyClick(window, Qt::Key_Space);
    check(!player.playing() && player.position()==searchPosition && player.volume()==searchVolume, "typing and cursor keys never trigger playback shortcuts");
    search->setProperty("text", QString::fromUtf8("sÉcond lab")); QTest::qWait(100);
    check(list->property("count").toInt()==1, "queue search matches artist and title without accent sensitivity");
    player.select(0); player.pause();
    testKeyClick(window, Qt::Key_Return);
    check(waitFor([&]{return player.currentIndex()==1;}), "Enter on filtered queue selects the original track index");
    player.pause();
    // Exercise Quick jump through real keyboard/mouse input and the audio backend.
    QMetaObject::invokeMethod(window,"closeQueueSearch");player.select(0,false);player.pause();
    window->contentItem()->forceActiveFocus();testKeyClick(window,Qt::Key_K,Qt::ControlModifier);QTest::qWait(250);
    auto *localJump=window->findChild<QObject *>("quickJump");
    auto *localJumpSearch=findItem(window->contentItem(),"quickJumpSearch");
    check(localJump&&localJump->property("visible").toBool()&&localJumpSearch&&localJumpSearch->hasActiveFocus(),"local Ctrl+K opens and focuses Quick jump");
    for(char ch:QByteArray("second"))testKeyClick(window,ch);
    check(localJump->property("query").toString()=="second","local Quick jump receives typed song search without invoking player shortcuts");
    testKeyClick(window,Qt::Key_Return);
    check(waitFor([&]{return player.currentIndex()==1&&player.playing()&&!localJump->property("visible").toBool();}),"local Quick jump Enter selects and actually starts audio playback");
    player.pause();player.setMiniMode(true);QTest::qWait(250);
    QMetaObject::invokeMethod(window,"openQuickJump");QTest::qWait(200);localJump->setProperty("query","First Light");QTest::qWait(100);click("quickJumpRow0");
    check(waitFor([&]{return player.currentIndex()==0&&player.playing();})&&player.miniMode(),"local Mini Quick jump click plays the selected song without expanding");
    player.pause();player.setMiniMode(false);QTest::qWait(250);
    QMetaObject::invokeMethod(window,"openQuickJump");QTest::qWait(200);localJump->setProperty("query","second");
    player.move(1,0);QTest::qWait(100);testKeyClick(window,Qt::Key_Return);
    check(localJump->property("visible").toBool()&&!localJump->property("notice").toString().isEmpty()&&!player.playing()&&player.title()=="First Light","local Quick jump refuses a result whose queue position has changed");
    QMetaObject::invokeMethod(localJump,"close");QTest::qWait(200);player.move(0,1);player.select(1,false);player.pause();
    QMetaObject::invokeMethod(window,"openQueueSearch");QTest::qWait(150);
    player.setVinyl(false);QTest::qWait(150);
    check(!window->property("vinyl").toBool(),"switching back to CD restores the original renderer");
    check(!findItem(window->contentItem(),"vinylTonearm"),"switching to CD releases tonearm geometry");
    capture("02-queue-search");
    search->setProperty("text", "no-such-track"); QTest::qWait(100);
    check(list->property("count").toInt()==0, "unmatched search shows an empty result set");
    testKeyClick(window, Qt::Key_Return);
    check(player.currentIndex()==1 && !player.playing(), "Enter with no matches leaves playback unchanged");
    search->setProperty("text", "second"); QTest::qWait(100);
    click("queueActions1"); click("queueRemove");
    check(player.count()==1 && player.title()=="First Light" && list->property("count").toInt()==0, "removing a filtered track uses its original index and updates results");
    testKeyClick(window, Qt::Key_Escape); QTest::qWait(100);
    check(!window->property("queueSearchOpen").toBool() && window->property("queueOpen").toBool() && list->property("count").toInt()==1, "Escape restores full queue without closing its panel");
    player.addUrls({QUrl::fromLocalFile(other)}, false);
    check(waitFor([&]{return !player.busy() && player.count()==2;}), "queue remains usable after filtered removal");
    waitFor([&]{return !player.artworkLoading()&&!window->property("swapRunning").toBool();});
    const auto currentPath=player.currentUrl();const auto currentPosition=player.position();
    auto *dragHandle=findItem(window->contentItem(),"queueDragArea");
    check(dragHandle!=nullptr,"queue exposes a drag handle");
    if(dragHandle) {
        focusTestWindow(window);const auto point=dragHandle->mapToScene(QPointF(11,29)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,point);
        QTest::mouseMove(window,point+QPoint(0,65),100);QTest::qWait(80);
        check(window->property("queueDropIndex").toInt()==1,"queue drag previews its destination");if(!captures.isEmpty())window->grabWindow().save(captures+"/02-queue-drag.png");
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,point+QPoint(0,65));QTest::qWait(150);
    }
    check(player.queue()[1].toMap()["title"]=="First Light"&&player.currentUrl()==currentPath&&player.position()==currentPosition&&!player.playing(),"drag reorders local queue without restarting the selected song");
    click("queueActions1");capture("02-queue-actions");click("queueMoveUp");
    check(player.queue()[0].toMap()["title"]=="First Light","queue menu provides a non-drag reorder action");
    player.setCiderAutoStart(true);{ Player saved(temp+"/player.ini"); check(saved.ciderAutoStart(),"automatic Cider launch preference persists"); }player.setCiderAutoStart(false);
    testKeyClick(window, Qt::Key_F, Qt::ControlModifier); QTest::qWait(100);
    check(search->hasActiveFocus(), "Ctrl+F opens queue search");
    search->setProperty("text", "second");
    click("ciderSourceButton");
    check(!window->property("queueSearchOpen").toBool() && window->property("queueQuery").toString().isEmpty(), "changing sources clears the previous queue search");
    click("localSourceButton");
    waitFor([&]{return !player.artworkLoading()&&!window->property("swapRunning").toBool();});
    const auto beforePreview=player.position();
    QTest::mouseMove(window,QPoint(481,294)); QTest::qWait(80);
    auto *preview=window->findChild<QQuickItem *>("rimPreview");
    check(preview && preview->isVisible() && player.position()==beforePreview, "rim hover previews time without seeking");
    if (!captures.isEmpty()) window->grabWindow().save(captures+"/03-rim-preview.png");
    QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,QPoint(481,294));
    QTest::mouseMove(window,QPoint(265,508)); QTest::qWait(100);
    check(player.position()==beforePreview, "rim drag defers seeking until release");
    QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,QPoint(265,508));
    check(waitFor([&]{return qAbs(player.position()-player.duration()/2)<350;}), "rim release seeks to the previewed time");
    auto pinMatches = [&](bool expected) {
        if (!qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE") && QGuiApplication::platformName() == "wayland") {
            QProcess query; query.start("hyprctl", {"clients", "-j"});
            if (!query.waitForFinished(2000) || query.exitCode() != 0) return false;
            for (const auto &entry : QJsonDocument::fromJson(query.readAllStandardOutput()).array()) {
                const auto client = entry.toObject();
                if (client["pid"].toInteger() == QCoreApplication::applicationPid())
                    return client["mapped"].toBool() && client["pinned"].toBool() == expected;
            }
            return false;
        }
        return window->flags().testFlag(Qt::WindowStaysOnTopHint) == expected;
    };
    check(!player.miniOnTop() && pinMatches(false), "Mini pin defaults off");
    click("menuButton"); QTest::qWait(200); click("preferencesAction"); QTest::qWait(250);
    click("vinylStyleButton");check(player.vinyl(),"preferences selects Vinyl");click("vinylStyleButton");
    check(player.vinyl()&&findItem(window->contentItem(),"vinylStyleButton")->property("checked").toBool(),"selected appearance cannot be unchecked");
    capture("11-vinyl-settings");click("cdStyleButton");check(!player.vinyl(),"preferences restores CD");
    auto *pinToggle = findItem(window->contentItem(), "miniPinToggle");
    check(pinToggle && pinToggle->mapToScene(QPointF()).y() >= 0, "Mini pin toggle fits inside preferences");
    if (!captures.isEmpty()) window->grabWindow().save(captures+"/03-pin-settings.png");
    click("miniPinToggle");
    check(player.miniOnTop() && pinMatches(false), "settings enables Mini pin without pinning the full player");
    click("closePreferences"); QTest::qWait(150); click("menuButton"); QTest::qWait(200); click("miniToggle"); QTest::qWait(200);
    check(waitFor([&] { return pinMatches(true); }), "Mini stays pinned in the compositor when enabled");
    player.setMiniOnTop(false);
    check(waitFor([&] { return pinMatches(false); }), "disabling the preference immediately unpins Mini");
    player.setMiniOnTop(true);
    check(waitFor([&] { return pinMatches(true); }), "Mini pin can be enabled again");
    window->hide(); QTest::qWait(150); window->show();
    check(waitFor([&] { return pinMatches(true); }), "Mini pin survives hiding and showing the window");
    check(player.miniMode() && window->width()==300 && window->height()==354 && !bar->isVisible(), "Mini mode hides chrome and resizes to a small CD");
    check(window->findChild<QQuickItem *>("miniControls")->y() >= 300, "Mini controls sit completely below the CD");
    check(!window->mask().contains(QPoint(150,150)), "Mini mode preserves the transparent spindle hole");
    { Player restored(temp+"/player.ini"); check(restored.miniMode() && restored.miniOnTop(), "Mini mode and pin preferences survive relaunch"); }
    focusTestWindow(window);
    QTest::mouseMove(window,QPoint(150,100)); QTest::qWait(50);
    QTest::mouseMove(window,QPoint(2,2)); QTest::qWait(1750);
    auto *miniControls=window->findChild<QQuickItem *>("miniControls");
    check(miniControls && miniControls->opacity()<.01, "Mini controls hide when the pointer leaves");
    capture("03-mini-idle");
    QTest::mouseMove(window,QPoint(150,324)); QTest::qWait(250);
    check(miniControls && miniControls->opacity()>.99, "Mini controls appear on hover");
    click("miniPlay"); check(waitFor([&]{return player.playing();}), "Mini play control operates the active player");
    click("miniPlay"); check(!player.playing(), "Mini pause control");
    // Keep the pointer over the disc for a visible-controls capture.
    if (!captures.isEmpty()) { QTest::mouseMove(window,QPoint(150,324)); QTest::qWait(250); window->grabWindow().save(captures+"/03-mini-controls.png"); }
    const auto beforeMiniSeek=player.position();
    QTest::mouseMove(window,QPoint(287,150)); QTest::qWait(100);
    check(preview && preview->isVisible() && player.position()==beforeMiniSeek, "Mini rim previews time without seeking");
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,QPoint(287,150));
    check(waitFor([&]{return qAbs(player.position()-player.duration()/4)<350;}), "Mini rim seeks at the scaled pointer position");
    testKeyClick(window, Qt::Key_F); QTest::qWait(400);
    check(window->property("discFlipped").toBool() && window->width()==300 && window->height()==354, "disc flip works in Mini mode without expanding the window");
    capture("03-mini-back");
    testKeyClick(window, Qt::Key_Escape); QTest::qWait(400);
    check(!window->property("discFlipped").toBool() && player.miniMode(), "Escape returns to artwork while preserving Mini mode");
    click("miniRestore"); QTest::qWait(200);
    check(!player.miniMode() && window->width()==530 && window->height()==730, "Mini restore returns to the full player");
    check(waitFor([&] { return pinMatches(false); }) && player.miniOnTop(), "restoring the full player unpins it while remembering the preference");
    testKeyClick(window,Qt::Key_M,Qt::ControlModifier); QTest::qWait(200);
    check(player.miniMode(), "Ctrl+M toggles Mini mode");
    check(waitFor([&] { return pinMatches(true); }), "returning to Mini reapplies the remembered pin");
    testKeyClick(window,Qt::Key_L,Qt::ControlModifier); QTest::qWait(200);
    check(!player.miniMode() && window->width()==860 && window->property("queueOpen").toBool(), "opening queue exits Mini mode without hiding the queue");
    check(waitFor([&] { return pinMatches(false); }), "opening the queue removes Mini pinning");
    window->setProperty("queueOpen", false);
    click("menuButton"); QTest::qWait(200); click("preferencesAction"); QTest::qWait(250); click("miniPinToggle"); click("closePreferences"); QTest::qWait(150);
    { Player restored(temp+"/player.ini"); check(!restored.miniOnTop(), "settings toggle off is remembered after relaunch"); }
    window->setProperty("queueOpen", true);

    write(temp + "/config/gtk-4.0/noctalia.css", themeCss(true));
    check(waitFor([&] { return theme.colors()["surface"].value<QColor>() == QColor("#f6f0e8"); }), "Noctalia palette updates after atomic replacement");
    check(window->property("inset").value<QColor>() == QColor("#f6f0e8"), "UI follows live light palette");
    capture("03-light");
    testKeyClick(window, Qt::Key_F); QTest::qWait(400); capture("03-disc-back-light");
    testKeyClick(window, Qt::Key_F); QTest::qWait(400);
    write(temp + "/config/gtk-4.0/noctalia.css", "incomplete export");
    QTest::qWait(200);
    check(theme.colors()["surface"].value<QColor>() == QColor("#f6f0e8"), "partial theme writes retain complete palette");
    write(temp + "/state/noctalia/settings.toml", "[shell]\nfont_family = \"Adwaita Sans\"\ncorner_radius_scale = 0.75\n[shell.animation]\nenabled = false\n");
    check(waitFor([&] { return theme.motionScale() == 0; }), "Noctalia reduced motion followed");
    write(temp + "/config/gtk-4.0/noctalia.css", themeCss(false));
    waitFor([&] { return theme.colors()["surface"].value<QColor>() == QColor("#17191f"); });
    player.remove(0); check(player.count() == 1 && player.currentIndex() == 0, "remove preceding track preserves selection");
    testKeyClick(window, Qt::Key_F); QTest::qWait(10);
    check(window->findChild<QQuickItem *>("discSides")->property("side").toInt()==1, "reduced motion flips without animation");
    testKeyClick(window, Qt::Key_F);
    player.setRepeatMode(2); player.next(true); check(player.currentIndex() == 0, "repeat-one preserves track");
    player.pause();
    player.setRepeatMode(0); player.next(true); check(!player.playing(), "end of queue stops with repeat off");
    const auto before = player.count();
    player.addUrls({QUrl("https://example.invalid/music.mp3")}, false);
    waitFor([&] { return !player.busy(); });
    check(player.count() == before && !player.error().isEmpty(), "unsupported input produces visible error");
    player.dismissError();
    if (QGuiApplication::platformName() == "wayland") {
        click("menuButton"); QTest::qWait(200); capture("04-settings-menu"); click("preferencesAction"); QTest::qWait(250); capture("04-preferences"); click("blurToggle"); click("closePreferences"); QTest::qWait(400);
        check(player.backgroundBlur() && window->property("backgroundBlur").toBool(), "settings toggle enables blur backdrop");
        check(window->mask().contains(QPoint(265,294)), "blur backdrop fills spindle region when enabled");
        capture("04-blur-enabled");
        { Player restored(temp + "/player.ini"); check(restored.backgroundBlur(), "blur preference persists across launch"); }
        click("menuButton"); QTest::qWait(200); click("preferencesAction"); QTest::qWait(250); click("blurToggle"); click("closePreferences"); QTest::qWait(400);
        check(!player.backgroundBlur() && !window->mask().contains(QPoint(265,294)), "settings toggle restores transparent CD silhouette");
    }
    player.setVolume(.43); player.save();
    { Player restored(temp + "/player.ini"); check(restored.count() == 1 && !restored.playing() && qAbs(restored.volume()-.43) < .01, "queue and volume persist without autoplay"); }
    check(player.discDetails().value("year")=="2026", "album year survives queue changes");
    const auto nextAlbum=temp+"/Night-Drive.flac";QFile::copy(demo,nextAlbum);
    {TagLib::FileRef tagged(nextAlbum.toUtf8().constData());tagged.tag()->setAlbum("Night Drive");
        auto properties=tagged.file()->properties();properties.replace("LYRICS",TagLib::StringList{"The sky folds into blue\nThe evening carries through"});tagged.file()->setProperties(properties);tagged.save();}
    player.addUrls({QUrl::fromLocalFile(nextAlbum)},false);waitFor([&]{return !player.busy();});
    write(temp+"/state/noctalia/settings.toml", "[shell.animation]\nenabled = true\n");
    waitFor([&]{return theme.motionScale()>0;});
    player.select(1,false);waitFor([&]{return !player.artworkLoading();});QTest::qWait(80);
    check(window->property("swapRunning").toBool(), "changing albums slides out the previous disc");
    auto *outgoingDisc = window->findChild<QObject *>("outgoingDiscLoader");
    check(outgoingDisc && outgoingDisc->property("active").toBool()
          && outgoingDisc->property("item").value<QObject *>(), "outgoing disc remains rendered during its exit animation");
    if(!captures.isEmpty())window->grabWindow().save(captures+"/05-disc-outgoing.png");
    QTest::qWait(180);
    if(!captures.isEmpty())window->grabWindow().save(captures+"/05-disc-incoming.png");
    check(waitFor([&]{return !window->property("swapRunning").toBool();}), "new album settles after the swap");
    auto *presenter = qobject_cast<DiscPresentation *>(qmlContext(window)->contextProperty("presentation").value<QObject *>());
    check(outgoingDisc && !outgoingDisc->property("active").toBool()
          && !outgoingDisc->property("item").value<QObject *>() && presenter && presenter->outgoing().isNull(),
          "finished swap destroys outgoing painted items and releases the previous image");
    window->setProperty("discFlipped",true);window->setProperty("lyricsView",true);
    check(waitFor([&]{return !lyrics.loading() && lyrics.lines().size()==2;}) && !lyrics.timed(), "local embedded plain lyrics load without artificial timestamps");
    const auto beforePlainLyric=player.position();
    check(!lyrics.seekToLine(0) && player.position()==beforePlainLyric, "untimed lyrics cannot seek");
    window->setProperty("lyricsView",false);window->setProperty("discFlipped",false);QTest::qWait(400);
    player.select(0,false);QTest::qWait(50);player.select(1,false);
    check(waitFor([&] { return !player.artworkLoading() && !window->property("swapRunning").toBool()
              && window->findChild<Disc *>("discFace")->artwork()==player.artwork(); }, 3000),
          "rapid album changes settle on the latest cover");
    player.select(0,false);QTest::qWait(60);player.setMotion(false);QTest::qWait(50);
    const bool swapStillRunning = window->findChild<QObject *>("discSwap")->property("running").toBool();
    check(!swapStillRunning && !window->property("swapRunning").toBool() && window->property("swapOffset").toDouble()==0, "disabling motion finishes an active swap immediately");
    const auto restoreFile=temp+"/lazy-restore.ini";
    {
        Player saved(restoreFile); saved.setVolume(0);
        saved.addUrls({QUrl::fromLocalFile(demo)},false);
        check(waitFor([&]{return !saved.busy() && !saved.artworkLoading();}), "paused local import finishes without starting audio");
        saved.seek(7000); saved.save();
    }
    {
        Player restored(restoreFile);
        check(!restored.playing() && restored.position()==7000 && restored.duration()>7000, "lazy playback restores position and duration before loading audio");
        restored.setVolume(0); restored.play();
        check(waitFor([&]{return restored.playing() && restored.position()>7200;}), "first playback resumes the saved position after loading audio");
        restored.pause(); restored.seek(9000); restored.save();
        restored.select(0,false); restored.clear();
        QTest::qWait(300);
        check(restored.count()==0 && restored.artwork().isNull() && !restored.artworkLoading(), "clearing during artwork loading rejects the stale cover");
    }
    player.clear(); check(player.count() == 0 && player.currentIndex() == -1, "clear stops and resets player");
    click("addMusicButton");
    auto *dialog = window->findChild<QObject *>("musicDialog");
    check(dialog && waitFor([&] { return dialog->property("visible").toBool(); }), "add button opens music chooser");
    if (dialog) QMetaObject::invokeMethod(dialog, "close");
    window->setProperty("queueOpen", false); QTest::qWait(200);
    const QImage grab = window->grabWindow();
    if (!grab.isNull()) {
        const auto scale = grab.width() / double(window->width());
        check(grab.pixelColor(qRound(265*scale), qRound(294*scale)).alpha() == 0, "CD spindle hole remains transparent");
    }
    failures += exerciseCiderQueue(temp);
    failures += exerciseLibrary(window, temp, captures);
    std::cout << "RESULT " << failures << " failures" << std::endl;
    return failures ? 1 : 0;
}

#endif

int main(int argc, char **argv) {
#ifndef SPUN_DIAGNOSTICS
    // Keep the installed CLI checks available without mapping QtTest or test fixtures
    // into the music player. exec preserves arguments, signals, and exit status.
    for (int i=1; i<argc; ++i) {
        const QByteArray option = QByteArray(argv[i]).split('=').first();
        if (option == "--") break;
        if (option == "--self-test" || option == "--test-library" || option == "--smoke-live"
            || option == "--verify-cider" || option == "--verify-cider-writes" || option == "--inspect-cider" || option == "--inspect-library") {
            const auto executable = QFileInfo(QStringLiteral("/proc/self/exe")).symLinkTarget();
            const auto diagnostics = QFile::encodeName(QFileInfo(executable).absolutePath() + "/spun-diagnostics");
            execv(diagnostics.constData(), argv);
            std::cerr << "Build diagnostics with scripts/build.sh -DBUILD_TESTING=ON before running checks.\n";
            return 1;
        }
    }
#endif
    [[maybe_unused]] QElapsedTimer startup; startup.start();
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    // Audio-only playback does not need FFmpeg to initialize video hardware.
    // Qt Quick keeps its normal GPU renderer; explicit user overrides win.
    // https://doc.qt.io/qt-6/advanced-ffmpeg-configuration.html
    if (!qEnvironmentVariableIsSet("QT_FFMPEG_DECODING_HW_DEVICE_TYPES"))
        qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", ",");
    if (!qEnvironmentVariableIsSet("QT_FFMPEG_ENCODING_HW_DEVICE_TYPES"))
        qputenv("QT_FFMPEG_ENCODING_HW_DEVICE_TYPES", ",");
    QGuiApplication app(argc, argv);
    const qint64 applicationReady = startup.elapsed();
    app.setApplicationName("spun"); app.setApplicationDisplayName("Spun");
    app.setOrganizationName("Spun"); app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("spun"); app.setWindowIcon(QIcon(":/assets/spun.svg"));
    QCommandLineParser parser; parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"benchmark", "Measure an isolated startup, idle, playing, or mini scene", "scene"});
    parser.addOption({"test-library", "Run isolated music browser checks"});
    parser.addOption({"self-test", "Run isolated playback and UI checks"});
    parser.addOption({"smoke-live", "Run isolated checks on the live desktop"});
    parser.addOption({"verify-cider", "Verify a live Cider connection, briefly testing and restoring playback"});
    parser.addOption({"verify-cider-writes", "Exercise live Cider controls, reversible queue/audio edits and start song radio"});
    parser.addOption({"inspect-library", "Verify Cider music browsing without changing playback"});
    parser.addOption({"inspect-cider", "Verify live Cider metadata and artwork without changing playback"});
    parser.addOption({"capture-dir", "Save test captures", "directory"});
    parser.addOption({"config", "Use a specific settings file", "path"});
    parser.addOption({"export-cover", "Write the original fallback artwork", "path"});
    parser.addPositionalArgument("files", "Music files or album folders to play", "[files…]");
    parser.process(app);
    if (parser.isSet("export-cover")) return Disc::fallbackArt().save(parser.value("export-cover")) ? 0 : 1;
    const bool test = parser.isSet("benchmark") || parser.isSet("self-test") || parser.isSet("smoke-live") || parser.isSet("test-library");
    if (!test && !parser.isSet("config")) {
        auto bus = QDBusConnection::sessionBus();
        if (bus.interface() && bus.interface()->isServiceRegistered("org.mpris.MediaPlayer2.spun")) {
            QDBusInterface shell("org.mpris.MediaPlayer2.spun", "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2", bus);
            QStringList urls;
            for (const auto &arg : parser.positionalArguments())
                urls.append(QUrl::fromUserInput(arg, QDir::currentPath(), QUrl::AssumeLocalFile).toString());
            if (!urls.isEmpty()) shell.call("OpenFiles", urls);
            shell.call("Raise");
            return 0;
        }
    }
    QTemporaryDir temp;
    QString configRoot = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    QString stateRoot = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation);
    QString settings = parser.value("config");
    if (test) {
        configRoot = temp.path() + "/config"; stateRoot = temp.path() + "/state";
        write(configRoot + "/gtk-4.0/noctalia.css", themeCss(false));
        write(stateRoot + "/noctalia/settings.toml", "[shell]\nfont_family = \"Adwaita Sans\"\ncorner_radius_scale = 0.75\n");
        settings = temp.path() + "/player.ini";
    }
    if (settings.isEmpty()) settings = configRoot + "/spun/settings.ini";
    Typography typography(settings);
    const auto applyFont = [&] {
        QFont font(typography.family()); font.setPixelSize(14); app.setFont(font);
    };
    QObject::connect(&typography, &Typography::changed, &app, applyFont);
    applyFont();
    const qint64 fontReady = startup.elapsed();
    Theme theme(configRoot, stateRoot);
    Player player(settings);
    Cider cider(!test, QFileInfo(settings).absolutePath() + "/cider-connection.json");
    Lyrics lyrics(&player, &cider);
    Library library(&cider);
    MusicActions musicActions(&cider);
    Listening listening(&cider);
    DiscPresentation presentation;
    Native native;
    const qint64 backendReady = startup.elapsed();
    if (!test) registerMpris(&player);
    qmlRegisterType<Symbol>("Spun", 1, 0, "Symbol");
    qmlRegisterType<Disc>("Spun", 1, 0, "Disc");
    qmlRegisterType<ProgressRing>("Spun", 1, 0, "ProgressRing");
    QQmlApplicationEngine engine;
    engine.addImageProvider("queueart", new QueueArtworkProvider);
    engine.rootContext()->setContextProperty("player", &player);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("typography", &typography);
    engine.rootContext()->setContextProperty("native", &native);
    engine.rootContext()->setContextProperty("lyrics", &lyrics);
    engine.rootContext()->setContextProperty("presentation", &presentation);
    engine.rootContext()->setContextProperty("cider", &cider);
    engine.rootContext()->setContextProperty("library", &library);
    engine.rootContext()->setContextProperty("musicActions", &musicActions);
    engine.rootContext()->setContextProperty("listening", &listening);
    engine.rootContext()->setContextProperty("testMode", test);
    qint64 firstFrame = -1;
    int frames = 0;
    qint64 qmlReady = -1;
    if (parser.isSet("benchmark")) QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [&](QObject *object) {
        if (auto *view = qobject_cast<QQuickWindow *>(object))
            QObject::connect(view, &QQuickWindow::frameSwapped, &app, [&] {
                if (firstFrame < 0) {
                    firstFrame = startup.elapsed();
                    if (parser.value("benchmark") == "startup") {
                        const QJsonObject result{{"scene", "startup"}, {"firstFrameMs", firstFrame},
                            {"applicationReadyMs", applicationReady}, {"fontReadyMs", fontReady},
                            {"backendReadyMs", backendReady}, {"qmlReadyMs", qmlReady}};
                        std::cout << "BENCHMARK " << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
                        QTimer::singleShot(0, &app, &QCoreApplication::quit);
                    }
                }
                ++frames;
            });
    });
    engine.load(QUrl("qrc:/qml/Main.qml"));
    qmlReady = startup.elapsed();
    if (engine.rootObjects().isEmpty()) return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &player, &Player::save);
    if (parser.isSet("benchmark")) {
        window->setProperty("benchmarkPinned", true);
        player.setVolume(0);
        const auto scene = parser.value("benchmark");
        if (scene != "startup" && scene != "idle" && scene != "playing" && scene != "mini") return 2;
        if (scene == "playing" || scene == "mini") {
            player.setRepeatMode(2);
            player.demo();
            window->setProperty("progress", 0.75);
        }
        if (scene == "mini") player.setMiniMode(true);
        QTimer::singleShot(2000, &app, [&, scene] {
            struct rusage before{}; getrusage(RUSAGE_SELF, &before);
            const int startFrames = frames;
            QTimer::singleShot(6000, &app, [&, scene, before, startFrames] {
                struct rusage after{}; getrusage(RUSAGE_SELF, &after);
                auto cpu = [](const rusage &r) { return r.ru_utime.tv_sec + r.ru_stime.tv_sec + (r.ru_utime.tv_usec + r.ru_stime.tv_usec) / 1e6; };
                QFile memory("/proc/self/smaps_rollup");
                if (!memory.open(QIODevice::ReadOnly)) { app.exit(1); return; }
                QJsonObject result{{"scene",scene},{"firstFrameMs",firstFrame},{"cpuSeconds",cpu(after)-cpu(before)},
                    {"frames",frames-startFrames},{"memory",QString::fromUtf8(memory.readAll())}};
                std::cout << "BENCHMARK " << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
                app.quit();
            });
        });
    }
#ifdef SPUN_DIAGNOSTICS
    else if (parser.isSet("inspect-library")) QTimer::singleShot(650, &app, [&] {
        int failures=0;
        auto check=[&](bool ok,const char *name) { std::cout<<(ok?"PASS ":"FAIL ")<<name<<std::endl;if(!ok)++failures; };
        auto capture=[&](const QString &name) {
            if(!parser.isSet("capture-dir"))return;
            QDir().mkpath(parser.value("capture-dir"));QTest::qWait(900);
            // Hidden Wayland workspaces can retain a surface at its old size.
            // Capture the full current item tree, including the popup overlay.
            auto grab=window->contentItem()->grabToImage();
            const bool ready=grab&&waitFor([&]{if(grab->image().isNull())window->grabWindow();return !grab->image().isNull();},5000);
            check(ready&&grab->image().save(parser.value("capture-dir")+"/"+name+".png"),"library capture saved");
        };
        window->setProperty("useCider",true);window->setProperty("libraryOpen",true);
        library.setSection("albums");
        check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty()&&!library.items().isEmpty(),"live Cider library albums load");
        capture("albums");
        auto *hoverButton=window->findChild<QQuickItem *>("addMusicButton");
        if(hoverButton) {
            focusTestWindow(window); QTest::mouseMove(window,QPoint(2,2)); QTest::qWait(80);
            QTest::mouseMove(window,hoverButton->mapToScene(QPointF(18,18)).toPoint());QTest::qWait(800);
            auto *tip=hoverButton->findChild<QObject *>("spunToolTip");
            check(tip && tip->property("visible").toBool(),"themed control tooltip appears on hover");capture("tooltip-controls");
            QTest::mouseMove(window,QPoint(2,2));QTest::qWait(100);
        }
        int first=library.items().size();
        if(library.hasMore()) { library.more();check(waitFor([&]{return !library.busy();},15000)&&library.items().size()>first,"live library pagination appends albums"); }
        const bool firstAlbumEmpty=library.items().first().toMap().value("trackCount",-1).toInt()==0;
        library.open(0);check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty()&&(firstAlbumEmpty?library.items().isEmpty():!library.items().isEmpty()),firstAlbumEmpty?"live empty library album shows a normal empty state":"live library album tracks load");capture("album-tracks");
        if(firstAlbumEmpty) {
            check(!library.collection().value("playable").toBool(),"empty library album does not offer unavailable playback");
            library.back();int populated=-1;for(int i=0;i<library.items().size();++i)if(library.items()[i].toMap().value("trackCount").toInt()>0){populated=i;break;}
            if(populated>=0) {library.open(populated);check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty()&&!library.items().isEmpty(),"live populated library album tracks load");capture("populated-album-tracks");}
        }
        library.back();library.setSection("playlists");
        check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty(),"live Cider playlists load");capture("playlists");
        if(!library.items().isEmpty()) {library.open(0);check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty(),"live playlist tracks load");capture("playlist-tracks");library.back();}
        library.setSection("search");
        auto *search=window->findChild<QQuickItem *>("librarySearchInput");
        focusTestWindow(window);QTest::qWait(100);
        if (search) { QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,search->mapToScene(QPointF(90,19)).toPoint());if(search->hasActiveFocus())for(char c:QByteArray("Daft Punk"))testKeyClick(window,c); }
        check(library.query()=="Daft Punk","live search field accepts keyboard input");
        check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty()&&!library.items().isEmpty(),"live catalog song search works");capture("search-songs");
        library.setKind("albums");check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty()&&!library.items().isEmpty(),"live catalog album search works");
        library.open(0);check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty()&&!library.items().isEmpty(),"live catalog album tracks load");capture("catalog-album");library.back();
        library.setKind("playlists");check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty()&&!library.items().isEmpty(),"live catalog playlist search works");capture("search-playlists");
        library.open(0);check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty()&&!library.items().isEmpty(),"live catalog playlist tracks load");
        library.back();library.showArtist("Daft Punk");
        check(waitFor([&]{return !library.busy();},15000)&&library.collection().value("type")=="artists","live Cider connection opens an artist page");
        if(library.collection().value("type")=="artists") {
            library.setArtistView("songs");
            check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty()&&!library.items().isEmpty()&&library.items().first().toMap().value("type")=="songs","live artist top songs work with the existing Cider token");capture("artist-top-songs");
            if(!library.items().isEmpty()) {
                library.prepareRadio(library.items().first().toMap());
                check(waitFor([&]{return !library.radioBusy();},15000)&&library.radioError().isEmpty(),"live song radio availability resolves without playback");
                std::function<QObject*(QObject*)> findMenu=[&](QObject *parent)->QObject* {
                    if(parent->objectName()=="browserTrackMenu")return parent;
                    for(auto *child:parent->children())if(auto *found=findMenu(child))return found;
                    return nullptr;
                };
                if(auto *menu=findMenu(window)) {
                    menu->setProperty("selection",library.items().first());menu->setProperty("x",24);menu->setProperty("y",220);
                    QMetaObject::invokeMethod(menu,"open");waitFor([&]{return !library.radioBusy();},15000);capture("song-radio");QMetaObject::invokeMethod(menu,"close");
                }
            }
            library.setArtistView("similar");
            check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty()&&!library.items().isEmpty(),"live similar artists resolve through Cider");capture("similar-artists");
            library.setArtistView("albums");check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty(),"live artist Albums switch remains available");
            for(const auto &category:QStringList{"full-albums","singles","live-albums"}) {
                library.setDiscography(category);
                check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty(),qPrintable("live discography category loads: "+category));
                if(library.hasMore()) {
                    const auto count=library.items().size();library.more();
                    check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty()&&library.items().size()>count,"live filtered discography pagination appends releases");
                }
                if(!library.items().isEmpty()) {
                    library.open(0);
                    check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty(),"live filtered release opens its tracks");
                    library.back();check(library.discography()==category,"live Back restores the discography category");
                }
                capture("discography-"+category);
            }
            library.setDiscography("all");waitFor([&]{return !library.busy();},15000);
            // Exercise local pins only with an explicitly isolated settings file.
            if(parser.isSet("config")) {
                const auto artist=library.collection();const bool wasPinned=library.isPinned(artist);
                if(!wasPinned)library.togglePin(artist);
                library.back();library.setQuery("");waitFor([&]{return !library.busy();},15000);capture("pinned-artist");
                int pin=-1;const auto pins=library.pins();for(int i=0;i<pins.size();++i)if(pins[i].toMap().value("path")==artist.value("path"))pin=i;
                library.playPin(pin);
                check(waitFor([&]{return !library.busy();},15000)&&library.collection().value("type")=="artists"&&library.artistView()=="songs","live artist pin opens Top songs");capture("pinned-artist-open");
                library.setSection("releases");
                check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty(),"live pinned artist releases load through batched Cider lookup");capture("pinned-releases");
                if(!library.items().isEmpty()) { library.open(0);check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty(),"live release opens album tracks");library.back(); }
                library.setSection("search");
                if(!wasPinned)library.togglePin(artist);
            }
            library.back();
        }
        library.setKind("stations");library.setQuery("Chill");
        check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty()&&!library.items().isEmpty()&&library.items().first().toMap()["type"]=="stations","live station search works with the existing token");capture("stations");
        library.setSection("songs");library.setNewestFirst(true);
        check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty(),"live recently added songs load");capture("recently-added-songs");
        library.setSection("albums");check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty(),"live recently added albums load");capture("recently-added-albums");
        library.setSection("recent");
        check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty(),"live recently played history loads");
        library.setSection("for-you");
        check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty(),"live For You recommendations load through Spun");capture("for-you");
        if(library.hasMore()) {
            const auto count=library.items().size();library.more();
            check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty()&&library.items().size()>=count,"live For You pagination preserves existing recommendations");
        }
        for(int i=0;i<library.items().size();++i)if(library.items()[i].toMap().value("type")!="stations") {
            library.open(i);check(waitFor([&]{return !library.busy();},15000)&&library.error().isEmpty(),"live recommended collection opens its tracks");
            library.back();check(library.section()=="for-you","live Back restores For You");break;
        }
        musicActions.setObserving(true);
        check(waitFor([&]{return musicActions.ready()||!musicActions.error().isEmpty();},15000)&&musicActions.ready(),"live song favorite, dislike and library status are readable");
        musicActions.setObserving(false);
        cider.refreshAudioQuality();
        check(waitFor([&]{return !cider.qualityBusy();},15000)&&!cider.audioQuality().isEmpty(),"live audio quality reports details or an explicit unavailable state");
        auto *audio=window->findChild<QObject *>("crossfadeMenu");
        if(audio) { QMetaObject::invokeMethod(audio,"open");check(waitFor([&]{return !cider.audioBusy()&&!cider.crossfadeBusy();},15000),"live audio settings finish loading");capture("audio-settings");QMetaObject::invokeMethod(audio,"close"); }
        window->setProperty("queueOpen",true);
        check(waitFor([&]{return !cider.queueBusy()&&cider.queueReady();},15000),"live queue loads for cleanup preview");
        QMetaObject::invokeMethod(window,"previewQueueCleanup",Q_ARG(QVariant,"upcoming"));
        auto *cleanup=window->findChild<QObject *>("cleanupPopup");
        check(cleanup&&cleanup->property("visible").toBool(),"live cleanup preview opens without removing songs");capture("cleanup-preview");
        if(cleanup)QMetaObject::invokeMethod(cleanup,"close");
        if(parser.isSet("config") && cider.queueReady() && !cider.queue().isEmpty()) {
            listening.addBookmark();
            check(waitFor([&]{return !listening.busy();},15000)&&!listening.bookmarks().isEmpty(),"live song bookmark saves in isolated storage");
            for(const auto &bookmark:listening.bookmarks())listening.removeBookmark(bookmark.toMap().value("key").toString());
            check(listening.bookmarks().isEmpty(),"live test bookmark can be removed locally");
            listening.setRememberSession(true);
            check(waitFor([&]{return !listening.session().isEmpty();},15000),"live session checkpoint saves without changing playback");
            listening.setRememberSession(false);
            check(listening.session().isEmpty(),"disabling isolated session recovery clears its checkpoint");
            window->setProperty("libraryOpen",true);library.setSection("sessions");
            waitFor([&]{return !cider.queueBusy();},15000);
            const bool saved=library.saveQueue("Spun interface check");check(saved,"live queue can be saved in isolated test storage");
            if(saved) {
                library.open(0);QTest::qWait(200);const auto snapshot=library.collection();const auto id=snapshot.value("id").toString();
                std::function<QQuickItem *(QQuickItem *)> findMark=[&](QQuickItem *parent)->QQuickItem * {
                    if(parent->objectName()=="alreadyQueued0")return parent;
                    for(auto *child:parent->childItems())if(auto *found=findMark(child))return found;
                    return nullptr;
                };
                auto *mark=findMark(window->contentItem());
                check(mark && mark->isVisible(),"live current song is marked Already queued in the saved queue");capture("already-queued");
                check(library.renameSavedQueue(snapshot,"Evening mix"),"isolated saved queue rename works with live metadata");
                check(library.canUndoSavedQueue()&&library.undoSavedQueue()&&library.collection()["title"]==snapshot["title"],"native saved queue Undo restores a local name without changing playback");
                library.renameSavedQueue(library.collection(),"Evening mix");
                if(library.items().size()>1) {
                    check(library.editSavedTrack(0,1,false),"isolated saved queue reorders without changing Cider");
                    check(library.editSavedTrack(0,0,true),"isolated saved queue removes a local reference");
                }
                capture("edited-saved-queue");
                const auto localTracks=library.savedTracks(id);
                if(!localTracks.isEmpty()) {
                    QMetaObject::invokeMethod(window,"openSavedQueuePicker",Q_ARG(QVariant,QVariant(localTracks)),Q_ARG(QVariant,QVariant::fromValue(&library)));QTest::qWait(250);
                    auto *picker=window->findChild<QObject *>("savedQueuePicker");
                    check(picker&&picker->property("visible").toBool(),"native saved queue picker opens with live song metadata");
                    if(picker) {
                        picker->setProperty("selected",library.savedQueueChoices()["rows"].toList().first());
                        auto centered=[&] { return qAbs(picker->property("x").toDouble()+picker->property("width").toDouble()/2-window->width()/2.)<1 && qAbs(picker->property("y").toDouble()+picker->property("height").toDouble()/2-window->height()/2.)<1; };
                        check(centered(),"native saved picker is centered in the expanded player");capture("append-saved-queue");
                        window->setProperty("libraryOpen",false);QTest::qWait(250);
                        check(centered(),"native saved picker remains centered after the sidebar closes");capture("append-saved-queue-compact");
                        const auto before=library.savedTracks(id);QMetaObject::invokeMethod(picker,"submit");
                        waitFor([&]{return !picker->property("visible").toBool();},2000);
                        check(!picker->property("visible").toBool()&&library.savedTracks(id)==before,"native picker skips duplicates without changing the isolated snapshot");
                    }
                }
                window->setProperty("queueOpen",true);waitFor([&]{return !cider.queueBusy();},15000);
                QMetaObject::invokeMethod(window,"selectUpcomingQueue");QTest::qWait(150);
                check(window->property("queueSelectionCount").toInt()==qMax(0,cider.queue().size()-cider.currentIndex()-1),"native queue selection excludes history and the current song");capture("queue-selection");
                QMetaObject::invokeMethod(window,"clearQueueSelection");
                QMetaObject::invokeMethod(window,"openQuickJump");QTest::qWait(300);
                auto *jump=window->findChild<QObject *>("quickJump");
                check(jump&&jump->property("visible").toBool()&&waitFor([&]{return !jump->property("waitingForQueue").toBool();},10000),"native Quick jump loads pins, saved queues and current queue");
                if(jump) {
                    jump->setProperty("query","Evening mix");capture("quick-jump");
                    QMetaObject::invokeMethod(jump,"activate",Q_ARG(QVariant,QVariant(QVariantMap{{"kind","saved"},{"key",id}})));QTest::qWait(250);
                    check(window->property("libraryOpen").toBool()&&library.collection()["id"]==id,"native Quick jump opens the chosen saved queue");
                }
                library.deleteSavedQueue(id);
            }
        }
        player.setMiniMode(true);QTest::qWait(250);
        auto *next=window->findChild<QQuickItem *>("miniNext");
        if(next&&next->isEnabled()) {
            QTest::mouseMove(window,next->mapToScene(QPointF(20,20)).toPoint());QTest::qWait(850);
            auto *peek=next->findChild<QObject *>("nextTrackTip");
            check(peek&&peek->property("visible").toBool()&&waitFor([&]{return !cider.queueBusy();},10000),"native Mini hover opens its next-song preview");capture("mini-next-preview");
            QTest::mouseMove(window,QPoint(2,2));QTest::qWait(200);
        }
        QMetaObject::invokeMethod(window,"openQuickJump");QTest::qWait(300);capture("mini-quick-jump");
        auto *miniJump=window->findChild<QObject *>("quickJump");
        check(miniJump&&miniJump->property("visible").toBool()&&miniJump->property("width").toDouble()<=window->width(),"native Quick jump fits Mini mode");
        if(miniJump)QMetaObject::invokeMethod(miniJump,"close");
        check(waitFor([&]{return cider.liveConnected();},8000),"live Cider change stream is connected");
        library.setActive(false);std::cout<<"RESULT "<<failures<<" failures"<<std::endl;app.exit(failures?1:0);
    });
    else if (parser.isSet("verify-cider") || parser.isSet("verify-cider-writes") || parser.isSet("inspect-cider")) QTimer::singleShot(600, &app, [&] {
        int failures = 0;
        auto check = [&](bool ok, const char *name) { std::cout << (ok ? "PASS " : "FAIL ") << name << std::endl; if (!ok) ++failures; };
        check(waitFor([&] { return cider.available() && cider.count() > 0; }), "live Cider track detected");
        if (!cider.available() || !cider.count()) { app.exit(1); return; }
        window->setProperty("useCider", true);
        const bool wasPlaying = cider.playing();
        const qint64 oldPosition = cider.position();
        check(waitFor([&]{return cider.volumeReady();},8000),"Cider normalized volume is ready");
        const double oldVolume = cider.volume();
        const int oldRepeat = cider.repeatMode();
        check(waitFor([&] { return !cider.artwork().isNull(); }, 15000), "Cider artwork decoded onto CD");
        check(window->property("useCider").toBool(), "UI selects Cider source");
        if (!parser.isSet("inspect-cider")) {
        cider.setVolume(0);
        check(waitFor([&] { return cider.volume() == 0; }), "Cider volume control");
        cider.play();
        check(waitFor([&] { return cider.playing(); }), "Cider play control");
        QTest::qWait(500);
        cider.pause();
        check(waitFor([&] { return !cider.playing(); }), "Cider pause control");
        if (cider.canSeek()) {
            const auto target = qMin<qint64>(oldPosition + 3000, cider.duration()-1000);
            cider.seek(target);
            const bool sought=waitFor([&] { return qAbs(cider.position()-target) < 700; });
            check(sought, "Cider seek control");
            if(!sought)std::cout<<"SEEK target="<<target<<" observed="<<cider.position()<<" duration="<<cider.duration()<<" playing="<<cider.playing()<<std::endl;
            QTest::qWait(2200);
            check(qAbs(cider.position()-target)<700,"confirmed seek survives desktop position polling");
            cider.seek(oldPosition);
            check(waitFor([&] { return qAbs(cider.position()-oldPosition) < 700; }), "Cider position restored");
        }
        cider.setRepeatMode((oldRepeat+1)%3);
        check(waitFor([&] { return cider.repeatMode() != oldRepeat; }), "Cider repeat control");
        cider.setRepeatMode(oldRepeat);
        check(waitFor([&] { return cider.repeatMode() == oldRepeat; }), "Cider repeat restored");
        cider.setVolume(oldVolume);
        check(waitFor([&] { return qAbs(cider.volume()-oldVolume) < .01; }), "Cider volume restored");
        if (wasPlaying) { cider.play(); waitFor([&] { return cider.playing(); }); }
        }
        if(parser.isSet("verify-cider-writes")) {
            window->setProperty("queueOpen",true);
            check(waitFor([&]{return !cider.queueBusy()&&cider.queueReady();},15000),"live write checks load the current queue");
            const auto originalQueue=cider.queue();const int originalIndex=cider.currentIndex();
            const auto keys=[](const QVariantList &rows) {QStringList result;for(const auto &value:rows){const auto row=value.toMap();result.append(row["type"].toString()+":"+row["id"].toString());}return result;};
            const auto originalKeys=keys(originalQueue);
            auto idle=[&] {return waitFor([&]{return !cider.controlBusy()&&!cider.queueBusy();},15000);};
            if(originalIndex>=0&&originalIndex<originalQueue.size()) {
                cider.insertQueue({originalQueue[originalIndex]},originalQueue.size(),cider.queueRevision());
                check(idle()&&keys(cider.queue())==originalKeys+QStringList{originalKeys[originalIndex]},"live insertion places a duplicate at the requested slot even before a radio tail");
                if(keys(cider.queue())==originalKeys+QStringList{originalKeys[originalIndex]}) {
                    cider.removeQueue(originalQueue.size(),cider.queueRevision());
                    check(idle()&&keys(cider.queue())==originalKeys&&cider.canUndoQueue(),"live DELETE removes the test occurrence and offers Undo");
                    if(keys(cider.queue())==originalKeys&&cider.canUndoQueue()) {
                        cider.undoQueueRemoval();check(idle()&&keys(cider.queue())==originalKeys+QStringList{originalKeys[originalIndex]},"live Undo restores the test occurrence to its original position");
                    }
                }
                // Remove only an identifiable extra test occurrence, including a
                // partially completed insertion; never replace the user's queue.
                auto actual=keys(cider.queue());
                for(int i=actual.size()-1;i>=0;--i) {auto without=actual;without.removeAt(i);if(i!=cider.currentIndex()&&without==originalKeys) {cider.removeQueue(i,cider.queueRevision());idle();break;}}
                check(keys(cider.queue())==originalKeys&&cider.currentIndex()==originalIndex,"live queue write checks restore the original order and current occurrence");
            }
            cider.refreshAudioOptions();waitFor([&]{return !cider.audioBusy();},15000);
            for(const auto &key:QStringList{"automix","listeningMode"})if(cider.audioOptions().contains(key)) {
                const auto original=cider.audioOptions()[key];
                const QVariant target=key=="automix"?QVariant(!original.toBool()):QVariant(original=="gaming"?"unwind":"gaming");
                cider.setAudioOption(key,target);
                check(waitFor([&]{return !cider.audioBusy();},15000)&&cider.audioOptions()[key]==target,qPrintable("live Spun audio action confirms "+key));
                cider.refreshAudioOptions();waitFor([&]{return !cider.audioBusy();},15000);cider.setAudioOption(key,original);
                check(waitFor([&]{return !cider.audioBusy();},15000)&&cider.audioOptions()[key]==original,qPrintable("live Spun audio action restores "+key));
            }
            library.prepareRadio({},true);
            check(waitFor([&]{return !library.radioBusy();},15000)&&library.radioAvailable(),"live current song resolves a station before playback");
            if(library.radioAvailable()) {
                library.playRadio();
                check(waitFor([&]{return !library.radioBusy();},20000)&&library.radioError().isEmpty(),"live Spun radio action verifies that its requested station is playing");
            }
        }
        if (parser.isSet("capture-dir")) {
            QDir().mkpath(parser.value("capture-dir")); QTest::qWait(600);
            check(window->grabWindow().save(parser.value("capture-dir")+"/cider-noctalia.png"), "Cider and current Noctalia theme captured");
            if (parser.isSet("inspect-cider")) {
                window->setProperty("queueOpen", true);
                check(waitFor([&] { return cider.queueReady() || cider.needsToken() || !cider.queueError().isEmpty(); }), "Cider queue reports live connection state");
                QTest::qWait(300);
                check(window->width() == 860, "Cider queue panel opens inside Spun");
                check(cider.queueReady(), "Cider complete queue retrieved");
                std::cout << "QUEUE " << cider.queue().size() << " tracks" << std::endl;
                check(window->grabWindow().save(parser.value("capture-dir")+"/cider-queue.png"), "Cider queue panel captured");
                QMetaObject::invokeMethod(window, "openQueueSearch"); QTest::qWait(100);
                auto *search = window->findChild<QQuickItem *>("queueSearchInput");
                auto *list = window->findChild<QQuickItem *>("trackList");
                const auto query = cider.queue().isEmpty() ? QString() : cider.queue().last().toMap()["title"].toString();
                search->setProperty("text", query); QTest::qWait(800);
                int expected = 0;
                for (const auto &value : cider.queue()) {
                    const auto row=value.toMap();
                    if (row["title"].toString().compare(query, Qt::CaseInsensitive)==0) ++expected;
                }
                check(!query.isEmpty() && list->property("count").toInt()>=expected && list->property("count").toInt()>0, "search finds live Cider tracks without changing playback");
                check(window->grabWindow().save(parser.value("capture-dir")+"/cider-search.png"), "Cider search panel captured");
                QMetaObject::invokeMethod(window, "closeQueueSearch");
                window->setProperty("queueOpen", false);
                window->setProperty("discFlipped", true);
                check(waitFor([&]{return !cider.discLoading();},20000) && !cider.discDetails().value("tracks").toList().isEmpty(), "live Cider reverse loads the actual album track list");
                QTest::qWait(500);
                check(window->grabWindow().save(parser.value("capture-dir")+"/cider-disc-back.png"), "Cider album reverse captured");
                auto *albumList=window->findChild<QQuickItem *>("albumTrackList");
                const auto volume=cider.volume();
                const auto beforeScroll=albumList->property("contentY").toDouble();
                const QPointF at=albumList->mapToScene(QPointF(130,40));
                QWheelEvent wheel(at,window->mapToGlobal(at.toPoint()),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
                QGuiApplication::sendEvent(window,&wheel); QTest::qWait(400);
                check(albumList->property("count").toInt()<=3 || albumList->property("contentY").toDouble()!=beforeScroll || albumList->property("atYEnd").toBool(), "reverse track list scrolls on the CD");
                check(cider.volume()==volume, "scrolling the track list leaves volume unchanged");
                albumList->forceActiveFocus();
                testKeyClick(window,Qt::Key_Home); QTest::qWait(100);
                check(albumList->property("contentY").toDouble()==0, "Home returns the album list to its first track");
                testKeyClick(window,Qt::Key_Down); QTest::qWait(100);
                check(albumList->property("count").toInt()<=3 || albumList->property("contentY").toDouble()>0, "arrow keys browse a focused album list");
                check(cider.volume()==volume, "keyboard album browsing leaves volume unchanged");
                const auto rememberedRow=albumList->property("rememberedRow").toInt();
                player.setMiniMode(true); QTest::qWait(500);
                check(albumList->property("rememberedRow").toInt()==rememberedRow, "Mini mode preserves the album reading position");
                check(window->grabWindow().save(parser.value("capture-dir")+"/cider-mini-back.png"), "Mini album reverse captured");
                window->setProperty("lyricsView",true);
                check(waitFor([&]{return !lyrics.loading();},20000), "live Cider lyric request completes");
                check(!lyrics.lines().isEmpty() || !lyrics.message().isEmpty(), "live lyrics show content or a clear unavailable state");
                check(window->grabWindow().save(parser.value("capture-dir")+"/cider-lyrics.png"), "live lyrics view captured");
                window->setProperty("discFlipped",false);
                check(!lyrics.active(), "closing reverse deactivates lyrics");
                check(!cider.discVisible(), "returning to artwork stops album requests");
            }
        }
        std::cout << "RESULT " << failures << " failures" << std::endl;
        app.exit(failures ? 1 : 0);
    });
    else if (parser.isSet("test-library")) QTimer::singleShot(650, &app, [&] { app.exit(exerciseLibrary(window,temp.path(),parser.value("capture-dir")) ? 1 : 0); });
    else if (test) QTimer::singleShot(650, &app, [&] {
        app.exit(exercise(player, theme, lyrics, window, temp.path(), parser.value("capture-dir"), parser.isSet("smoke-live")));
    });
#endif
    else if (!parser.positionalArguments().isEmpty()) {
        window->setProperty("useCider", false);
        QList<QUrl> urls;
        for (const auto &arg : parser.positionalArguments()) urls.append(QUrl::fromUserInput(arg, QDir::currentPath(), QUrl::AssumeLocalFile));
        player.addUrls(urls);
    }
    return app.exec();
}

#include "main.moc"
