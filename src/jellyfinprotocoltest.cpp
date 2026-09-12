#include "jellyfin.h"
#include "jellyfinapi.h"
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QtTest>
class JellyfinProtocolTest : public QObject {
  Q_OBJECT
  QTemporaryDir storage;
  QTcpServer http;
  QString mode = "ok", base;
  QByteArray received;
  int requests = 0;
private slots:
  void initTestCase() {
    qputenv("XDG_CONFIG_HOME", storage.path().toUtf8());
    QCoreApplication::setOrganizationName("SpunTests");
    QCoreApplication::setApplicationName("jellyfin-protocol");
    QVERIFY(http.listen(QHostAddress::LocalHost));
    base = "http://127.0.0.1:" + QString::number(http.serverPort()) + "/music";
    connect(&http, &QTcpServer::newConnection, this, [this] {
      while (http.hasPendingConnections()) {
        auto *socket = http.nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, socket,
                &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
          auto data =
              socket->property("data").toByteArray() + socket->readAll();
          socket->setProperty("data", data);
          const int end = data.indexOf("\r\n\r\n");
          if (end < 0 || socket->property("sent").toBool())
            return;
          int length = 0;
          for (const auto &line : data.left(end).split('\n'))
            if (line.toLower().startsWith("content-length:"))
              length = line.mid(15).trimmed().toInt();
          if (data.size() < end + 4 + length)
            return;
          socket->setProperty("sent", true);
          received = data;
          ++requests;
          if (mode == "hold")
            return;
          QByteArray status = "200 OK",
                     body = "{\"Items\":[],\"TotalRecordCount\":0}", extra, contentType = "application/json";
          if (data.startsWith("POST /music/Users/AuthenticateByName"))
            body = "{\"User\":{\"Id\":\"fixture-user\"},\"AccessToken\":"
                   "\"fixture-token\"}";
          if (data.startsWith("GET /music/Users/Me"))
            body = "{\"Id\":\"fixture-user\"}";
          if (mode == "redirect") {
            status = "302 Found";
            extra = "Location: http://127.0.0.1:1/leak\r\n";
          }
          if (mode == "malformed")
            body = "<html>Wrong server</html>";
          if (mode == "expired")
            status = "401 Unauthorized";
          if (mode == "forbidden")
            status = "403 Forbidden";
          if (mode == "missing")
            status = "404 Not Found";
          if (mode == "oversized")
            body = QByteArray(17 * 1024 * 1024, 'x');
          if (mode == "disconnect") {
            socket->abort();
            return;
          }
          if (mode == "lyrics")
            body = "{\"Metadata\":{\"IsSynced\":true,\"Offset\":10000000},"
                   "\"Lyrics\":[{\"Text\":\"One\",\"Start\":20000000},{"
                   "\"Text\":\"Two\",\"Start\":50000000}]}";
          if (mode == "thumbnails") {
            const QUrl requestUrl(
                base + QString::fromUtf8(data.split(' ').value(1)).mid(6));
            if (requestUrl.path().contains("/Images/")) {
              contentType = "image/png";
              QImage image(8, 8, QImage::Format_RGB32);
              image.fill(Qt::blue);
              QBuffer buffer(&body);
              buffer.open(QIODevice::WriteOnly | QIODevice::Truncate);
              image.save(&buffer, "PNG");
            } else if (requestUrl.path().endsWith("/Items")) {
              const int start =
                  QUrlQuery(requestUrl).queryItemValue("StartIndex").toInt();
              QJsonArray rows;
              for (int i = start; i < qMin(start + 100, 300); ++i)
                rows.append(QJsonObject{
                    {"Id", QString::number(i)},
                    {"Name", QString("Song %1").arg(i)},
                    {"Type", "Audio"},
                    {"ImageTags", QJsonObject{{"Primary", "fixture"}}}});
              body = QJsonDocument(QJsonObject{{"Items", rows},
                                               {"TotalRecordCount", 300}})
                         .toJson();
            }
          }
          socket->write("HTTP/1.1 " + status + "\r\n" + extra +
                        "Content-Type: " + contentType + "\r\nContent-Length: " +
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
    requests = 0;
  }
  void rejectsBadResponses_data() {
    QTest::addColumn<QString>("behavior");
    for (auto v : {"redirect", "malformed", "expired", "forbidden", "oversized",
                   "disconnect"})
      QTest::newRow(v) << QString(v);
  }
  void rejectsBadResponses() {
    QFETCH(QString, behavior);
    mode = behavior;
    JellyfinApi client(false);
    client.connectServer(base, "fixture", "test-secret", false);
    QTRY_VERIFY_WITH_TIMEOUT(!client.connecting(), 5000);
    QVERIFY(!client.connected());
    QVERIFY(!client.error().isEmpty());
    QVERIFY(!client.error().contains("test-secret"));
  }
  void credentialsAndBasePath() {
    mode = "hold";
    JellyfinApi client(false);
    client.connectServer(base, "café + &", "test-secret", false);
    QTRY_VERIFY(!received.isEmpty());
    const auto header = received.left(received.indexOf("\r\n\r\n"));
    QVERIFY(header.startsWith("POST /music/Users/AuthenticateByName "));
    QVERIFY(!header.contains("test-secret"));
    QVERIFY(received.contains("test-secret"));
    client.disconnectServer();
    QVERIFY(!client.connecting());
  }
  void invalidAddresses() {
    JellyfinApi client(false);
    for (auto s : {"file:///tmp/server", "http://user:secret@localhost",
                   "http://localhost/?token=x", "http://localhost/#web"}) {
      client.connectServer(s, "a", "", false);
      QVERIFY(!client.connecting());
      QVERIFY(!client.error().isEmpty());
    }
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
      JellyfinApi client;
      client.connectServer(base, "fixture", "keyring fixture + café", true);
      QTRY_VERIFY(client.connected());
      QTRY_VERIFY(QSettings().value("jellyfin/remember").toBool());
      QVERIFY(QFile::exists(secretFile));
      QFile config(QSettings().fileName());
      QSettings().sync();
      QVERIFY(config.open(QIODevice::ReadOnly));
      QVERIFY(!config.readAll().contains("keyring fixture"));
    }
    {
      JellyfinApi client;
      QTRY_VERIFY(client.connected());
      QCOMPARE(client.username(), QString("fixture"));
      client.disconnectServer();
      QTRY_VERIFY(!QFile::exists(secretFile));
      QVERIFY(!QSettings().contains("jellyfin/remember"));
    }
  }
  void cancelledLogin() {
    mode = "hold";
    JellyfinApi client(false);
    client.connectServer(base, "a", "b", false);
    QTRY_VERIFY(!received.isEmpty());
    client.disconnectServer();
    QTest::qWait(100);
    QVERIFY(!client.connected());
    QVERIFY(client.error().isEmpty());
  }
  void artworkCredentialsAreEphemeral() {
    JellyfinApi client(false);
    client.connectServer(base, "a", "b", false);
    QTRY_VERIFY(client.connected());
    auto item = client.item(
        {{"Id", "song"}, {"ImageTags", QVariantMap{{"Primary", "tag"}}}},
        "song");
    QVERIFY(!item.value("art").toString().contains("fixture-token"));
    const auto req = client.artworkRequest(QUrl(item.value("art").toString()));
    QVERIFY(!req.url().toString().contains("fixture-token"));
    QVERIFY(req.rawHeader("Authorization").contains("fixture-token"));
    client.disconnectServer();
    QVERIFY(client.artworkUrl(QUrl(item.value("art").toString())).isEmpty());
  }
  void lyricOffsetAndMissing() {
    JellyfinApi client(false);
    client.connectServer(base, "a", "b", false);
    QTRY_VERIFY(client.connected());
    QTest::qWait(100);
    auto song = client.item({{"Id", "song"}}, "song");
    mode = "lyrics";
    bool done = false;
    QVariantMap result;
    QString error;
    client.lyrics(song, [&](const QVariantMap &d, const QString &e) {
      done = true;
      result = d;
      error = e;
    });
    QTRY_VERIFY(done);
    QVERIFY(error.isEmpty());
    QCOMPARE(result.value("lines").toList().size(), 2);
    QCOMPARE(result.value("lines")
                 .toList()
                 .first()
                 .toMap()
                 .value("start")
                 .toLongLong(),
             1000);
    mode = "missing";
    done = false;
    client.lyrics(song, [&](const QVariantMap &d, const QString &e) {
      done = true;
      result = d;
      error = e;
    });
    QTRY_VERIFY(done);
    QVERIFY(error.isEmpty());
    QVERIFY(result.value("lines").toList().isEmpty());
  }
  void failedAudioCleansBuffer() {
    JellyfinApi client(false);
    client.connectServer(base, "a", "b", false);
    QTRY_VERIFY(client.connected());
    QTest::qWait(100);
    mode = "forbidden";
    const auto path = storage.filePath("failed-audio");
    bool done = false;
    QString error;
    client.download(client.item({{"Id", "song"}}, "song"), path,
                    [&](const QVariantMap &, const QString &e) {
                      done = true;
                      error = e;
                    });
    QTRY_VERIFY(done);
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFile::exists(path));
  }
  void boundedThumbnailCache() {
    QTemporaryDir profile;
    Jellyfin client(profile.path(), false);
    client.setEnabled(true);
    client.server()->connectServer(base, "a", "b", false);
    QTRY_VERIFY(client.server()->connected());
    QTRY_VERIFY(!client.busy());
    mode = "thumbnails";
    client.show("songs");
    QTRY_VERIFY(!client.busy());
    client.loadMore();
    QTRY_VERIFY(!client.busy());
    client.loadMore();
    QTRY_VERIFY(!client.busy());
    QCOMPARE(client.items().size(), 300);
    QString lastFile;
    for (const auto &v : client.items()) {
      const auto id = v.toMap().value("id").toString();
      QTRY_VERIFY(!client.artwork(id).isEmpty());
      lastFile = QUrl(client.artwork(id)).toLocalFile();
    }
    const auto directory = QFileInfo(lastFile).absoluteDir();
    QCOMPARE(directory.entryList({"*.png"}, QDir::Files).size(), 256);
    const int completed = requests;
    QTest::qWait(200);
    QCOMPARE(requests, completed);
    const auto first = client.items().first().toMap().value("id").toString();
    QVERIFY(client.artwork(first).isEmpty());
    QTRY_VERIFY(!client.artwork(first).isEmpty());
    QCOMPARE(directory.entryList({"*.png"}, QDir::Files).size(), 256);
    QVERIFY(requests > completed);
    client.server()->disconnectServer();
    QVERIFY(directory.entryList({"*.png"}, QDir::Files).isEmpty());
  }
  void accountIdentity() {
    JellyfinApi a(false), b(false);
    a.connectServer(base, "a", "b", false);
    b.connectServer(base + "/other", "a", "b", false);
    QTRY_VERIFY(a.connected());
    const auto song = a.item({{"Id", "song"}}, "song");
    QVERIFY(!b.owns(song));
    a.disconnectServer();
    QVERIFY(!a.owns(song));
  }
};
QTEST_MAIN(JellyfinProtocolTest)
#include "jellyfinprotocoltest.moc"
