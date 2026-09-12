#include "remotelibrary.h"
#include "subsonicapi.h"
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

class ProtocolTest : public QObject {
  Q_OBJECT
  QTemporaryDir storage;
  QTcpServer http;
  QString mode, received;
  QString base;
  QStringList requests;
  std::function<QVariantMap(const QString &, const QUrlQuery &)> responder;

private slots:
  void initTestCase() {
    qputenv("XDG_CONFIG_HOME", storage.path().toUtf8());
    QCoreApplication::setOrganizationName("SpunTests");
    QCoreApplication::setApplicationName("protocol");
    QVERIFY(http.listen(QHostAddress::LocalHost));
    base = "http://127.0.0.1:" + QString::number(http.serverPort());
    connect(&http, &QTcpServer::newConnection, this, [this] {
      while (http.hasPendingConnections()) {
        auto socket = http.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket,
                &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
          auto buffer =
              socket->property("request").toByteArray() + socket->readAll();
          socket->setProperty("request", buffer);
          if (!buffer.contains("\r\n\r\n") || socket->property("sent").toBool())
            return;
          const int end = buffer.indexOf("\r\n\r\n");
          int length = 0;
          for (const auto &line : buffer.left(end).split('\n'))
            if (line.toLower().startsWith("content-length:"))
              length = line.mid(15).trimmed().toInt();
          if (buffer.size() < end + 4 + length)
            return;
          socket->setProperty("sent", true);
          received = QString::fromUtf8(buffer);
          requests.append(received);
          if (mode == "hold")
            return;
          QByteArray status = "200 OK",
                     body = "{\"subsonic-response\":{\"status\":\"ok\","
                            "\"version\":\"1.16.1\"}}",
                     extra;
          if (responder) {
            const QUrl target("http://fixture" +
                              QString::fromUtf8(buffer.split(' ').value(1)));
            QUrlQuery params(target);
            if (buffer.startsWith("POST "))
              params = QUrlQuery(QString::fromUtf8(buffer.mid(end + 4)));
            auto payload = responder(
                target.path().section('/', -1).section('.', 0, 0), params);
            if (!payload.contains("status"))
              payload["status"] = "ok";
            body = QJsonDocument(
                       QJsonObject{{"subsonic-response",
                                    QJsonObject::fromVariantMap(payload)}})
                       .toJson();
          }
          if (mode == "image" && received.contains("/getCoverArt.view")) {
            QImage image(1000, 500, QImage::Format_RGB32);
            image.fill(Qt::blue);
            QBuffer output(&body);
            output.open(QIODevice::WriteOnly | QIODevice::Truncate);
            image.save(&output, "PNG");
          }
          if (mode == "redirect") {
            status = "302 Found";
            extra = "Location: http://127.0.0.1:1/credential-leak\r\n";
          }
          if (mode == "malformed")
            body = "<html>Not a music server</html>";
          if (mode == "denied")
            body = "{\"subsonic-response\":{\"status\":\"failed\",\"error\":{"
                   "\"code\":50}}}";
          if (mode == "disconnect") {
            socket->abort();
            return;
          }
          if (mode == "oversized")
            body = QByteArray(17 * 1024 * 1024, 'x');
          socket->write("HTTP/1.1 " + status + "\r\n" + extra +
                        "Content-Type: application/json\r\nContent-Length: " +
                        QByteArray::number(body.size()) +
                        "\r\nConnection: close\r\n\r\n" + body);
          socket->disconnectFromHost();
        });
      }
    });
  }
  void init() {
    mode = "ok";
    received.clear();
    requests.clear();
    responder = {};
  }
  void validatesAddress() {
    SubsonicApi client;
    client.connectServer("https://user:secret@example.com", "user", "password",
                         false);
    QVERIFY(!client.connecting());
    QVERIFY(!client.error().isEmpty());
    client.connectServer(base + "/rest", "user", "password", false);
    QVERIFY(!client.connecting());
    QVERIFY(!client.error().isEmpty());
  }
  void rejectsUnsafeAndInvalidResponses_data() {
    QTest::addColumn<QString>("behavior");
    for (const auto &v :
         {"redirect", "malformed", "denied", "disconnect", "oversized"})
      QTest::newRow(v) << QString(v);
  }
  void rejectsUnsafeAndInvalidResponses() {
    QFETCH(QString, behavior);
    mode = behavior;
    SubsonicApi client;
    client.connectServer(base, "fixture", "test-secret", false);
    QTRY_VERIFY_WITH_TIMEOUT(!client.connecting(), 5000);
    QVERIFY(!client.connected());
    QVERIFY(!client.error().isEmpty());
    QVERIFY(!client.error().contains("test-secret"));
  }
  void encodesSpecialCharactersAndFreshSalt() {
    mode = "hold";
    SubsonicApi client;
    client.connectServer(base + "/music", "a+b & café", "never-in-url", false);
    QTRY_VERIFY(received.contains("/music/rest/ping.view"));
    QVERIFY(received.contains("a%2Bb%20%26%20caf%C3%A9"));
    QVERIFY(!received.contains("never-in-url"));
    const auto first = received;
    received.clear();
    client.connectServer(base + "/music", "a+b & café", "never-in-url", false);
    QTRY_VERIFY(!received.isEmpty());
    QVERIFY(received != first);
    client.disconnectServer();
    QVERIFY(!client.connecting());
  }
  void keyringSaveRestoreAndForget() {
    mode = "ok";
    const auto oldPath = qgetenv("PATH");
    const auto bin = storage.filePath("bin");
    QDir().mkpath(bin);
    const auto secretFile = storage.filePath("keyring-value");
    QFile tool(bin + "/secret-tool");
    QVERIFY(tool.open(QIODevice::WriteOnly));
    tool.write(R"PY(#!/usr/bin/python3
import os, pathlib, sys
p=pathlib.Path(os.environ['SPUN_TEST_KEYRING'])
if sys.argv[1]=='store':
    p.write_bytes(sys.stdin.buffer.read())
    p.chmod(0o600)
elif sys.argv[1]=='lookup':
    if not p.exists(): sys.exit(1)
    sys.stdout.buffer.write(p.read_bytes()+b'\n')
elif sys.argv[1]=='clear':
    p.unlink(missing_ok=True)
else:
    sys.exit(2)
)PY");
    tool.close();
    QVERIFY(tool.setPermissions(QFileDevice::ReadOwner |
                                QFileDevice::WriteOwner |
                                QFileDevice::ExeOwner));
    qputenv("PATH", bin.toUtf8() + ":" + oldPath);
    qputenv("SPUN_TEST_KEYRING", secretFile.toUtf8());
    const auto restore = qScopeGuard([&] {
      qputenv("PATH", oldPath);
      qunsetenv("SPUN_TEST_KEYRING");
    });
    {
      SubsonicApi client;
      client.connectServer(base, "fixture", "keyring fixture + café", true);
      QTRY_VERIFY(client.connected());
      QTRY_VERIFY(QSettings().value("subsonic/remember").toBool());
      QVERIFY(QFile::exists(secretFile));
      QFile config(QSettings().fileName());
      QSettings().sync();
      QVERIFY(config.open(QIODevice::ReadOnly));
      QVERIFY(!config.readAll().contains("keyring fixture"));
    }
    {
      SubsonicApi client;
      QTRY_VERIFY(client.connected());
      QCOMPARE(client.username(), QString("fixture"));
      client.disconnectServer();
      QTRY_VERIFY(!QFile::exists(secretFile));
      QVERIFY(!QSettings().contains("subsonic/remember"));
    }
  }
  void cancelAndReconnect() {
    mode = "hold";
    SubsonicApi client;
    client.connectServer(base, "fixture", "secret", false);
    QTest::qWait(50);
    client.disconnectServer();
    mode = "ok";
    QTest::qWait(50);
    QVERIFY(!client.connected());
    client.connectServer(base, "fixture", "secret", false);
    QTRY_VERIFY(client.connected());
    bool callback = false;
    mode = "hold";
    client.call(
        "search3", {},
        [&](const QVariantMap &, const QString &) { callback = true; },
        "catalog");
    QTest::qWait(50);
    client.cancel("catalog");
    QTest::qWait(50);
    QVERIFY(!callback);
    mode = "ok";
    client.call(
        "search3", {},
        [&](const QVariantMap &, const QString &e) { callback = e.isEmpty(); },
        "catalog");
    QTRY_VERIFY(callback);
  }
  void formPostAndLegacyFallback() {
    responder = [](const QString &method, const QUrlQuery &) -> QVariantMap {
      if (method == "getOpenSubsonicExtensions")
        return {{"openSubsonicExtensions",
                 QVariantList{QVariantMap{{"name", "formPost"},
                                          {"versions", QVariantList{1}}}}}};
      return {};
    };
    SubsonicApi client(false);
    client.connectServer(base + "/music", "fixture", "private", false);
    QTRY_VERIFY(client.connected());
    QTest::qWait(150);
    requests.clear();
    bool done = false;
    client.call(
        "createPlaylist", {{"name", "a+b & café"}},
        [&](const QVariantMap &, const QString &e) { done = e.isEmpty(); });
    QTRY_VERIFY(done);
    QVERIFY(received.startsWith("POST /music/rest/createPlaylist.view HTTP"));
    QVERIFY(received.contains("a%2Bb%20%26%20caf%C3%A9"));
    QVERIFY(!received.contains("private"));
    QVERIFY(!received.section("\r\n", 0, 0).contains("t="));
    client.disconnectServer();
    responder = {};
    client.connectServer(base, "fixture", "private", false);
    QTRY_VERIFY(client.connected());
    QTest::qWait(100);
    done = false;
    client.call(
        "search3", {{"query", "*"}},
        [&](const QVariantMap &, const QString &e) { done = e.isEmpty(); });
    QTRY_VERIFY(done);
    QVERIFY(received.startsWith("GET /rest/search3.view?"));
  }
  void preciseApiErrors_data() {
    QTest::addColumn<int>("code");
    QTest::addColumn<QString>("text");
    QTest::newRow("credentials") << 40 << QString("password");
    QTest::newRow("token-auth") << 41 << QString("token authentication");
    QTest::newRow("permission") << 50 << QString("allow");
    QTest::newRow("premium") << 60 << QString("Premium");
    QTest::newRow("missing") << 70 << QString("no longer exists");
    QTest::newRow("version") << 20 << QString("incompatible");
  }
  void preciseApiErrors() {
    QFETCH(int, code);
    QFETCH(QString, text);
    responder = [code](const QString &, const QUrlQuery &) -> QVariantMap {
      return {{"status", "failed"},
              {"error",
               QVariantMap{{"code", code},
                           {"message", "DO NOT DISPLAY signed-url-token"}}}};
    };
    SubsonicApi client(false);
    client.connectServer(base, "fixture", "private", false);
    QTRY_VERIFY(!client.connecting());
    QVERIFY(!client.connected());
    QVERIFY(client.error().contains(text));
    QVERIFY(!client.error().contains("signed-url"));
  }
  void lyricsExtensionAndFallback() {
    bool legacy = false;
    responder = [&](const QString &method, const QUrlQuery &) -> QVariantMap {
      if (method == "getOpenSubsonicExtensions")
        return {{"openSubsonicExtensions",
                 QVariantList{QVariantMap{{"name", "songLyrics"},
                                          {"versions", QVariantList{1}}}}}};
      if (method == "getLyricsBySongId" && !legacy)
        return {{"lyricsList",
                 QVariantMap{
                     {"structuredLyrics",
                      QVariantList{QVariantMap{
                          {"synced", true},
                          {"offset", 1000},
                          {"line",
                           QVariantList{
                               QVariantMap{{"start", 2000}, {"value", "One"}},
                               QVariantMap{{"start", 5000},
                                           {"value", "Two"}}}}}}}}}};
      if (method == "getLyrics")
        return {{"lyrics", QVariantMap{{"value", "Legacy lyrics"}}}};
      return {};
    };
    SubsonicApi client(false);
    client.connectServer(base, "fixture", "private", false);
    QTRY_VERIFY(client.connected());
    QTest::qWait(150);
    auto song = client.item(
        {{"id", "song"}, {"title", "Title"}, {"artist", "Artist"}}, "song");
    QVariantMap result;
    bool done = false;
    client.lyrics(song, [&](const QVariantMap &d, const QString &e) {
      result = d;
      done = e.isEmpty();
    });
    QTRY_VERIFY(done);
    auto lines = result.value("lines").toList();
    QCOMPARE(lines.size(), 2);
    QCOMPARE(lines.first().toMap().value("start").toLongLong(), 1000LL);
    legacy = true;
    done = false;
    client.lyrics(song, [&](const QVariantMap &d, const QString &e) {
      result = d;
      done = e.isEmpty();
    });
    QTRY_VERIFY(done);
    QCOMPARE(result.value("lyrics").toString(), QString("Legacy lyrics"));
  }
  void staleAndDuplicatePlaylistEntries() {
    QVariantList entries{QVariantMap{{"id", "a"}}, QVariantMap{{"id", "b"}},
                         QVariantMap{{"id", "a"}}};
    QString owner = "fixture";
    QUrlQuery mutation;
    int mutations = 0;
    responder = [&](const QString &method, const QUrlQuery &p) -> QVariantMap {
      if (method == "getPlaylist")
        return {
            {"playlist",
             QVariantMap{{"id", "p"}, {"owner", owner}, {"entry", entries}}}};
      if (method == "createPlaylist" || method == "updatePlaylist" ||
          method == "deletePlaylist") {
        mutation = p;
        ++mutations;
      }
      return {};
    };
    SubsonicApi client(false);
    client.connectServer(base, "fixture", "private", false);
    QTRY_VERIFY(client.connected());
    QTest::qWait(100);
    QVariantMap result;
    bool done = false;
    QString error;
    auto callback = [&](const QVariantMap &d, const QString &e) {
      result = d;
      error = e;
      done = true;
    };
    client.browse({{"mode", "playlist"}, {"remoteId", "p"}}, callback);
    QTRY_VERIFY(done);
    auto rows = result.value("items").toList();
    QCOMPARE(rows.size(), 3);
    auto first = rows[0].toMap().value("entryId").toString(),
         last = rows[2].toMap().value("entryId").toString();
    QVERIFY(first != last);
    done = false;
    client.editPlaylist("p", {{"entryIdToRemove", last}}, callback);
    QTRY_VERIFY(done);
    QVERIFY(error.isEmpty());
    QCOMPARE(mutation.queryItemValue("songIndexToRemove"), QString("2"));
    done = false;
    client.movePlaylistItem("p", first, 1, callback);
    QTRY_VERIFY(done);
    QVERIFY(error.isEmpty());
    QCOMPARE(mutation.allQueryItemValues("songId"),
             QStringList({"b", "a", "a"}));
    entries.prepend(QVariantMap{{"id", "new"}});
    const int count = mutations;
    done = false;
    client.movePlaylistItem("p", first, 1, callback);
    QTRY_VERIFY(done);
    QVERIFY(error.contains("changed"));
    QCOMPARE(mutations, count);
    done = false;
    client.editPlaylist("p", {{"entryIdToRemove", last}}, callback);
    QTRY_VERIFY(done);
    QVERIFY(error.contains("changed"));
    QCOMPARE(mutations, count);
    owner = "someone-else";
    done = false;
    client.removePlaylist("p", callback);
    QTRY_VERIFY(done);
    QVERIFY(error.contains("owner"));
    QCOMPARE(mutations, count);
  }
  void listeningTimeIgnoresSeeksAndPauses() {
    int submissions = 0, nowPlaying = 0;
    responder = [&](const QString &method, const QUrlQuery &p) -> QVariantMap {
      if (method == "scrobble") {
        if (p.queryItemValue("submission") == "true")
          ++submissions;
        else
          ++nowPlaying;
      }
      return {};
    };
    SubsonicApi client(false);
    client.connectServer(base, "fixture", "private", false);
    QTRY_VERIFY(client.connected());
    QTest::qWait(100);
    client.setScrobbling(true);
    auto song = client.item({{"id", "song"}, {"duration", 2}}, "song");
    client.reportPlayback(song, 0, false, false);
    QTRY_COMPARE(nowPlaying, 1);
    client.reportPlayback(song, 1900, true, false);
    QTest::qWait(1100);
    client.reportPlayback(song, 1900, true, false);
    QCOMPARE(submissions, 0);
    client.reportPlayback(song, 0, false, false);
    QTest::qWait(1100);
    client.reportPlayback(song, 1100, false, false);
    QTRY_COMPARE(submissions, 1);
    client.reportPlayback(song, 1500, false, false);
    QTest::qWait(100);
    QCOMPARE(submissions, 1);
    client.setScrobbling(false);
    const int reports = nowPlaying;
    client.reportPlayback(song, 0, false, false);
    QTest::qWait(1100);
    client.reportPlayback(song, 1500, false, false);
    QTest::qWait(100);
    QCOMPARE(submissions, 1);
    QCOMPARE(nowPlaying, reports);
  }
  void accountScopedReferencesAndMissingArt() {
    SubsonicApi client(false);
    client.connectServer(base, "fixture", "private", false);
    QTRY_VERIFY(client.connected());
    QTest::qWait(100);
    auto song = client.item(
        {{"id", "1"}, {"title", "Song"}, {"coverArt", "art"}}, "song");
    const auto serialized = QJsonDocument::fromVariant(song).toJson();
    QVERIFY(!serialized.contains("private"));
    QVERIFY(!serialized.contains("t="));
    QVERIFY(!serialized.contains("127.0.0.1"));
    QVERIFY(!client.artworkUrl(QUrl(song.value("art").toString())).isEmpty());
    bool done = false;
    client.cover(
        client.item({{"id", "2"}}, "song"), storage.filePath("missing.png"),
        [&](const QVariantMap &, const QString &) { done = true; }, "thumb");
    QVERIFY(done);
    auto old = client.identity();
    client.connectServer(base, "reader", "private", false);
    QTRY_VERIFY(client.connected());
    QVERIFY(client.identity() != old);
    QVERIFY(!client.owns(song));
    QVERIFY(client.artworkUrl(QUrl(song.value("art").toString())).isEmpty());
    done = false;
    client.download(
        song, storage.filePath("bad.audio"),
        [&](const QVariantMap &, const QString &e) { done = !e.isEmpty(); });
    QVERIFY(done);
    QVERIFY(!QFile::exists(storage.filePath("bad.audio")));
  }

  void coversRecoverAndUseIndependentChannels() {
    SubsonicApi client(false);
    client.connectServer(base, "fixture", "private", false);
    QTRY_VERIFY(client.connected());
    QTest::qWait(100);
    auto song = client.item({{"id", "1"}, {"coverArt", "art"}}, "song");
    bool done = false;
    QString error;
    mode = "malformed";
    client.cover(
        song, storage.filePath("cover-error.png"),
        [&](const QVariantMap &, const QString &e) {
          done = true;
          error = e;
        },
        "thumb");
    QTRY_VERIFY(done);
    QVERIFY(!error.isEmpty());
    mode = "image";
    int completed = 0;
    for (const auto &channel : {QString("cover"), QString("thumb")})
      client.cover(
          song, storage.filePath(channel + ".png"),
          [&](const QVariantMap &, const QString &e) {
            if (e.isEmpty())
              ++completed;
          },
          channel);
    QTRY_COMPARE(completed, 2);
    QCOMPARE(QImage(storage.filePath("thumb.png")).size(), QSize(256, 128));
    QCOMPARE(QImage(storage.filePath("cover.png")).size(), QSize(1024, 512));
    mode = "hold";
    done = false;
    client.cover(
        song, storage.filePath("stale.png"),
        [&](const QVariantMap &, const QString &) { done = true; }, "thumb");
    QTest::qWait(50);
    client.cancel("thumb");
    QTest::qWait(50);
    QVERIFY(!done);
    QVERIFY(!QFile::exists(storage.filePath("stale.png")));
  }
  void classicPagingPlaylistSearchAndFavorites() {
    responder = [](const QString &method, const QUrlQuery &p) -> QVariantMap {
      if (method == "getPlaylists")
        return {{"playlists",
                 QVariantMap{{"playlist",
                              QVariantList{QVariantMap{{"id", "p"},
                                                       {"name", "Evening Mix"},
                                                       {"owner", "fixture"}},
                                           QVariantMap{{"id", "q"},
                                                       {"name", "Morning Mix"},
                                                       {"owner", "other"}}}}}}};
      if (method == "search3") {
        QVariantList songs;
        const int start = p.queryItemValue("songOffset").toInt();
        for (int i = start; i < qMin(start + 100, 105); ++i)
          songs.append(QVariantMap{{"id", QString::number(i)},
                                   {"title", QString::number(i)}});
        return {{"searchResult3", QVariantMap{{"song", songs}}}};
      }
      if (method == "getStarred2") {
        QVariantList songs;
        for (int i = 0; i < 105; ++i)
          songs.append(QVariantMap{{"id", QString::number(i)}});
        return {{"starred2", QVariantMap{{"song", songs},
                                         {"album", QVariantList{QVariantMap{
                                                       {"id", "album"}}}}}}};
      }
      return {};
    };
    SubsonicApi client(false);
    client.connectServer(base, "fixture", "private", false);
    QTRY_VERIFY(client.connected());
    QTest::qWait(100);
    QVariantMap result;
    bool done = false;
    auto cb = [&](const QVariantMap &d, const QString &e) {
      result = d;
      done = e.isEmpty();
    };
    client.browse({{"mode", "songs"}}, cb);
    QTRY_VERIFY(done);
    QCOMPARE(result.value("items").toList().size(), 100);
    QVERIFY(result.value("more").toBool());
    QVERIFY(received.contains("query=%2A"));
    done = false;
    client.browse({{"mode", "songs"}, {"offset", 100}}, cb);
    QTRY_VERIFY(done);
    QCOMPARE(result.value("items").toList().size(), 5);
    QVERIFY(!result.value("more").toBool());
    done = false;
    client.browse(
        {{"mode", "search"}, {"filter", "playlists"}, {"query", "evening"}},
        cb);
    QTRY_VERIFY(done);
    QCOMPARE(result.value("items").toList().size(), 1);
    QCOMPARE(
        result.value("items").toList().first().toMap().value("kind").toString(),
        QString("playlist"));
    done = false;
    client.browse({{"mode", "favorites"}, {"offset", 100}}, cb);
    QTRY_VERIFY(done);
    QCOMPARE(result.value("items").toList().size(), 5);
    QVERIFY(client.isStarred(
        client.item({{"id", "0"}}, "song").value("id").toString()));
    QVERIFY(client.isStarred(
        client.item({{"id", "album"}}, "album").value("id").toString()));
  }
};
QTEST_GUILESS_MAIN(ProtocolTest)
#include "subsonicprotocoltest.moc"
