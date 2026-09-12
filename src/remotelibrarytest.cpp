#include "remotelibrary.h"
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

// Controllable replies deliberately permit late completion after cancellation.
// This exercises controller guards independently of a protocol adapter's
// guards.
class FixtureMusicApi final : public RemoteMusicApi {
public:
  explicit FixtureMusicApi(const QString &source = "jellyfin")
      : protocol(source) {}
  QString protocol, selectedFolder;
  struct Download {
    QString key, path, channel;
    Reply reply;
  };
  QList<Download> downloads;
  QList<QPair<QVariantMap, Reply>> pages;
  QStringList cancelled;
  QString account = "fixture";
  int quality = 0;
  bool online = true;
  QString serviceName() const override { return "Fixture"; }
  QString scheme() const override { return protocol; }
  bool connected() const override { return online; }
  bool connecting() const override { return false; }
  QString address() const override { return {}; }
  QString username() const override { return "listener"; }
  QString identity() const override { return account; }
  QString error() const override { return {}; }
  QVariantList playlists() const override { return {}; }
  QVariantList folders() const override { return {}; }
  QString folder() const override { return selectedFolder; }
  bool scrobbling() const override { return false; }
  int bitrate() const override { return quality; }
  bool keyringAvailable() const override { return false; }
  void setFolder(const QString &value) override {
    selectedFolder = value;
    emit changed();
  }
  void setScrobbling(bool) override {}
  void setBitrate(int n) override {
    quality = n;
    emit changed();
  }
  void connectServer(const QString &, const QString &, const QString &,
                     bool) override {}
  void disconnectServer() override {
    online = false;
    account = "other";
    emit accountChanged();
    emit changed();
  }
  void reloadPlaylists() override {}
  void createPlaylist(const QString &, Reply reply) override {
    if (reply)
      reply({}, {});
  }
  void cancel(const QString &channel) override { cancelled.append(channel); }
  void browse(const QVariantMap &request, Reply reply,
              const QString &) override {
    pages.append({request, reply});
  }
  void cover(const QVariantMap &, const QString &path, Reply reply,
             const QString &) override {
    QImage image(12, 12, QImage::Format_RGB32);
    image.fill(QColor("#88ccee"));
    image.save(path, "PNG");
    reply({{"file", path}}, {});
  }
  void lyrics(const QVariantMap &, Reply reply) override { reply({}, {}); }
  void download(const QVariantMap &row, const QString &path, Reply reply,
                const QString &channel) override {
    downloads.append({row.value("id").toString(), path, channel, reply});
  }
  void star(const QVariantMap &, bool, Reply reply) override { reply({}, {}); }
  void editPlaylist(const QString &, const Params &, Reply reply) override {
    reply({}, {});
  }
  void movePlaylistItem(const QString &, const QString &, int,
                        Reply reply) override {
    reply({}, {});
  }
  void removePlaylist(const QString &, Reply reply) override { reply({}, {}); }
  bool owns(const QVariantMap &row) const override {
    return online && row.value("server") == account &&
           row.value("source") == scheme();
  }
  bool isStarred(const QString &) const override { return false; }
  QVariantMap item(const QVariantMap &raw, const QString &) const override {
    return raw;
  }
  QUrl artworkUrl(const QUrl &) const override { return {}; }
  QNetworkRequest artworkRequest(const QUrl &) const override { return {}; }
  QUrl shareUrl(const QVariantMap &) const override { return {}; }
  void reportPlayback(const QVariantMap &, qint64, bool, bool) override {}
  QVariantMap row(const QString &id, const QString &kind = "song") const {
    return {{"id", id},          {"remoteId", id},     {"source", scheme()},
            {"server", account}, {"kind", kind},       {"title", id},
            {"seconds", 180},    {"artist", "Fixture"}};
  }
  void finishPage(int i, const QVariantList &rows, bool more = false,
                  const QString &error = {}) {
    pages[i].second({{"items", rows}, {"more", more}}, error);
  }
  void finishDownload(int i, const QString &error = {}) {
    const auto request = downloads[i];
    if (error.isEmpty()) {
      QFile::remove(request.path);
      QVERIFY(QFile::copy(QStringLiteral(SPUN_DEMO_FILE), request.path));
    }
    request.reply({}, error);
  }
};

class RemoteLibraryTest : public QObject {
  Q_OBJECT
  static void providerRows() {
    QTest::addColumn<QString>("source");
    QTest::newRow("jellyfin") << QString("jellyfin");
    QTest::newRow("subsonic-navidrome") << QString("subsonic");
  }
private slots:
  void navigationAndRefresh() {
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi;
    RemoteLibrary library(dir.path(), false, nullptr, api);
    emit api->changed();
    library.setEnabled(true);
    QCOMPARE(api->pages.size(), 1);
    const auto album = api->row("album", "album");
    api->finishPage(0, {album});
    library.setViewState(
        {{"query", "draft"}, {"filter", "albums"}, {"y", 220}, {"index", 3}});
    library.open(album);
    api->finishPage(1, {api->row("a"), api->row("b")});
    library.back();
    QCOMPARE(api->pages.size(), 2);
    QCOMPARE(library.items(), QVariantList{album});
    QCOMPARE(library.viewState().value("query").toString(), QString("draft"));
    QCOMPARE(library.viewState().value("y").toInt(), 220);
    library.reload();
    QVERIFY(library.busy());
    QCOMPARE(library.items(), QVariantList{album});
    api->finishPage(2, {}, false, "Offline");
    QCOMPARE(library.items(), QVariantList{album});
    QCOMPARE(library.error(), QString("Offline"));
    library.reload();
    api->finishPage(3, {album, api->row("second", "album")}, true);
    QCOMPARE(library.items().size(), 2);
    library.loadMore();
    QCOMPARE(api->pages.last().first.value("offset").toInt(), 2);
    api->finishPage(4, {api->row("third", "album")});
    QCOMPARE(library.items().size(), 3);
    library.setEnabled(false);
    library.setEnabled(true);
    QCOMPARE(api->pages.size(), 5);
    QCOMPARE(library.viewState().value("y").toInt(), 220);
  }
  void stalePagesAndAccount() {
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi;
    RemoteLibrary library(dir.path(), false, nullptr, api);
    emit api->changed();
    library.setEnabled(true);
    library.search("wanted", "songs");
    api->finishPage(1, {api->row("new")});
    api->finishPage(0, {api->row("old")});
    QCOMPARE(library.items().first().toMap().value("id").toString(),
             QString("new"));
    library.show("artists");
    library.setEnabled(false);
    api->finishPage(2, {api->row("late")});
    QVERIFY(library.items().isEmpty());
    api->disconnectServer();
    QVERIFY(library.items().isEmpty());
    QVERIFY(library.viewState().isEmpty());
    QVERIFY(!library.canBack());
  }
  void boundedPageHistory() {
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi;
    RemoteLibrary library(dir.path(), false, nullptr, api);
    emit api->changed();
    library.setEnabled(true);
    api->finishPage(0, {api->row("first", "album")});
    for (int i = 0; i < 8; ++i) {
      library.search(QString::number(i), "songs");
      api->finishPage(i + 1, {api->row(QString::number(i))});
    }
    library.search("6", "songs");
    QCOMPARE(api->pages.size(), 9); // Recent query is restored immediately.
    library.show("albums");
    QCOMPARE(api->pages.size(), 10); // Oldest page was evicted.
  }
  void nextPreservesTransportAndOverridesShuffle() {
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi;
    RemoteLibrary library(dir.path(), false, nullptr, api);
    emit api->changed();
    auto *player = library.transport();
    player->setVolume(0);
    library.playItems({api->row("a"), api->row("b"), api->row("c")});
    QCOMPARE(api->downloads.size(), 1);
    const auto key = player->trackKey();
    player->setShuffle(true);
    library.playNext(api->row("next"));
    QCOMPARE(player->trackKey(), key);
    QCOMPARE(api->downloads.size(), 1);
    QCOMPARE(player->count(), 4);
    QCOMPARE(player->queue()[1].toMap().value("title").toString(),
             QString("next"));
    player->next(false, false);
    QVERIFY(player->trackKey().endsWith("/next"));
    QVERIFY(!player->playing());
    library.enqueue(api->row("last"));
    QCOMPARE(player->queue().last().toMap().value("title").toString(),
             QString("last"));
  }
  void selectingSameTrackKeepsArtwork() {
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi;
    RemoteLibrary library(dir.path(), false, nullptr, api);
    library.playItem(api->row("a"));
    auto *player = library.transport();
    QVERIFY(!player->artwork().isNull());
    player->select(0, false);
    QVERIFY(!player->artwork().isNull());
  }
  void retryAndSameTrackLateReply() {
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi;
    RemoteLibrary library(dir.path(), false, nullptr, api);
    emit api->changed();
    auto *player = library.transport();
    player->setVolume(0);
    library.playItems({api->row("a"), api->row("b")});
    QVERIFY(library.buffering());
    api->finishDownload(0, "Temporary failure");
    QVERIFY(!library.buffering());
    QCOMPARE(player->error(), QString("Temporary failure"));
    library.retry();
    QCOMPARE(api->downloads.size(), 2);
    QCOMPARE(player->currentIndex(), 0);
    QCOMPARE(player->count(), 2);
    QVERIFY(player->error().isEmpty());
    api->finishDownload(0, "Stale failure");
    QVERIFY(player->error().isEmpty());
    QVERIFY(library.buffering());
    player->pause();
    api->finishDownload(1, "Cancelled failure");
    QVERIFY(!library.buffering());
    QVERIFY(player->error().isEmpty());
  }
  void prefetchConsumesOnlyOneAndInvalidates() {
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi;
    RemoteLibrary library(dir.path(), false, nullptr, api);
    emit api->changed();
    library.setEnabled(true);
    auto *player = library.transport();
    player->setVolume(0);
    library.playItems({api->row("a"), api->row("b"), api->row("c")});
    api->finishDownload(0);
    QTRY_COMPARE_WITH_TIMEOUT(api->downloads.size(), 2, 5000);
    QCOMPARE(api->downloads[1].channel, QString("prefetch"));
    QCOMPARE(api->downloads[1].key, QString("b"));
    api->finishDownload(1);
    QTest::qWait(400);
    QCOMPARE(api->downloads.size(), 2);
    player->next();
    QVERIFY(player->trackKey().endsWith("/b"));
    QVERIFY(!library.buffering());
    QCOMPARE(api->downloads.size(),
             2); // Consumed cache, no foreground download.
    QTRY_COMPARE_WITH_TIMEOUT(api->downloads.size(), 3, 5000);
    QCOMPARE(api->downloads[2].key, QString("c"));
    library.playNext(api->row("priority"));
    QTRY_COMPARE_WITH_TIMEOUT(api->downloads.size(), 4, 5000);
    QCOMPARE(api->downloads[3].key, QString("priority"));
    api->finishDownload(2); // Completion of superseded c must not become ready.
    api->finishDownload(3);
    api->setBitrate(128);
    QTRY_COMPARE_WITH_TIMEOUT(api->downloads.size(), 5, 5000);
    api->finishDownload(4);
    const auto prefetchedPath = api->downloads[4].path;
    library.setEnabled(false);
    // Fixture retains callbacks, so release their captured buffers too.
    api->downloads.clear();
    QVERIFY(!QFile::exists(prefetchedPath));
    QVERIFY(!player->playing());
  }
  void reorderingKeepsBufferedAudio() {
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi;
    RemoteLibrary library(dir.path(), false, nullptr, api);
    auto *player = library.transport();
    player->setVolume(0);
    library.playItems({api->row("a"), api->row("b")});
    api->finishDownload(0);
    QTRY_VERIFY_WITH_TIMEOUT(player->position() > 100, 5000);
    const auto key = player->trackKey();
    const auto file = api->downloads[0].path;
    player->move(0, 1);
    QCOMPARE(player->trackKey(), key);
    QCOMPARE(api->downloads.size(), 1);
    QVERIFY(QFile::exists(file));
    QVERIFY(player->playing());
    QTRY_VERIFY_WITH_TIMEOUT(player->position() > 200, 5000);
  }
  void shufflePlanMatchesPlayback() {
    QTemporaryDir dir;
    Player player(dir.filePath("player.ini"), nullptr, true);
    QList<Track> tracks;
    for (int i = 0; i < 5; ++i) {
      Track t;
      t.path = "jellyfin://fixture/" + QString::number(i);
      tracks.append(t);
    }
    player.setExternalTracks(tracks, 0, false);
    QCOMPARE(player.queuedTrackKey(-1), QString());
    QCOMPARE(player.queuedTrackKey(5), QString());
    QCOMPARE(player.queuedTrackKey(3), tracks[3].path);
    player.setShuffle(true);
    QSet<int> visited{0};
    for (int i = 0; i < 4; ++i) {
      const auto next = player.nextTrackIndex();
      QVERIFY(next >= 0);
      QVERIFY(!visited.contains(next));
      QCOMPARE(player.nextTrackIndex(), next);
      player.next(true, false);
      QCOMPARE(player.currentIndex(), next);
      visited.insert(next);
    }
    QCOMPARE(player.nextTrackIndex(), -1);
    player.setRepeatMode(1);
    const auto next = player.nextTrackIndex();
    player.next(true, false);
    QCOMPARE(player.currentIndex(), next);
    player.setRepeatMode(2);
    QCOMPARE(player.nextTrackIndex(), -1);
  }
  void failedPrefetchFallsBack_data() { providerRows(); }
  void failedPrefetchFallsBack() {
    QFETCH(QString, source);
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi(source);
    RemoteLibrary library(dir.path(), false, nullptr, api);
    emit api->changed();
    library.setEnabled(true);
    auto *player = library.transport();
    player->setVolume(0);
    library.playItems({api->row("a"), api->row("b")});
    api->finishDownload(0);
    QTRY_COMPARE_WITH_TIMEOUT(api->downloads.size(), 2, 5000);
    QCOMPARE(api->downloads[1].channel, QString("prefetch"));
    api->finishDownload(1, "Preload disconnected");
    QVERIFY(player->error().isEmpty());
    QVERIFY(player->playing());
    QVERIFY(!library.buffering());
    QTRY_VERIFY_WITH_TIMEOUT(player->position() > 100, 5000);
    player->next();
    QCOMPARE(api->downloads.size(), 3);
    QCOMPARE(api->downloads[2].channel, QString("audio"));
    QCOMPARE(api->downloads[2].key, QString("b"));
    QVERIFY(library.buffering());
    api->finishDownload(2);
    QTRY_VERIFY_WITH_TIMEOUT(player->position() > 100, 5000);
    QVERIFY(!library.buffering());
    QVERIFY(player->error().isEmpty());
  }
  void removingUpcomingTrackDiscardsItsBuffer_data() { providerRows(); }
  void removingUpcomingTrackDiscardsItsBuffer() {
    QFETCH(QString, source);
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi(source);
    RemoteLibrary library(dir.path(), false, nullptr, api);
    emit api->changed();
    library.setEnabled(true);
    auto *player = library.transport();
    player->setVolume(0);
    library.playItems({api->row("a"), api->row("b"), api->row("c")});
    api->finishDownload(0);
    QTRY_COMPARE_WITH_TIMEOUT(api->downloads.size(), 2, 5000);
    api->finishDownload(1);
    const auto discarded = api->downloads[1].path;
    api->downloads[1].reply = {};
    player->remove(1);
    QTRY_COMPARE_WITH_TIMEOUT(api->downloads.size(), 3, 5000);
    QCOMPARE(api->downloads[2].key, QString("c"));
    QVERIFY(!QFile::exists(discarded));
    QVERIFY(player->trackKey().endsWith("/a"));
    QVERIFY(player->playing());
    player->clear();
    api->finishDownload(
        2); // A successful late reply cannot repopulate a cleared queue.
    const auto latePath = api->downloads[2].path;
    api->downloads.clear();
    QTest::qWait(400);
    QCOMPARE(player->count(), 0);
    QVERIFY(!player->playing());
    QVERIFY(!library.buffering());
    QVERIFY(!QFile::exists(latePath));
  }
  void disconnectRejectsLateAudio_data() { providerRows(); }
  void disconnectRejectsLateAudio() {
    QFETCH(QString, source);
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi(source);
    RemoteLibrary library(dir.path(), false, nullptr, api);
    emit api->changed();
    library.setEnabled(true);
    auto *player = library.transport();
    player->setVolume(0);
    library.playItems({api->row("a"), api->row("b")});
    QVERIFY(library.buffering());
    api->disconnectServer();
    api->finishDownload(0);
    const auto discarded = api->downloads[0].path;
    api->downloads.clear();
    QTest::qWait(400);
    QCOMPARE(player->count(), 0);
    QVERIFY(!player->playing());
    QVERIFY(!library.buffering());
    QVERIFY(player->error().isEmpty());
    QVERIFY(player->artwork().isNull());
    QVERIFY(!QFile::exists(discarded));
    api->online = true;
    emit api->changed();
    library.playItem(
        api->row("a")); // Same id in a different account is a new song.
    QCOMPARE(api->downloads.size(), 1);
    QVERIFY(player->trackKey().contains("://other/"));
    api->finishDownload(0);
    QTRY_VERIFY_WITH_TIMEOUT(player->position() > 100, 5000);
  }
  void retryRejectsLateSuccess_data() { providerRows(); }
  void retryRejectsLateSuccess() {
    QFETCH(QString, source);
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi(source);
    RemoteLibrary library(dir.path(), false, nullptr, api);
    auto *player = library.transport();
    player->setVolume(0);
    library.playItems({api->row("a"), api->row("b")});
    library.retry();
    library.retry();
    QCOMPARE(api->downloads.size(), 3);
    api->finishDownload(0);
    api->finishDownload(1);
    QVERIFY(library.buffering());
    QCOMPARE(player->position(), 0);
    QCOMPARE(player->count(), 2);
    QCOMPARE(player->currentIndex(), 0);
    api->finishDownload(2);
    QTRY_VERIFY_WITH_TIMEOUT(player->position() > 100, 5000);
    QVERIFY(!library.buffering());
    api->finishDownload(0, "Stale failure after recovery");
    QVERIFY(player->error().isEmpty());
    QVERIFY(player->playing());
  }
  void reorderWhileDownloading_data() { providerRows(); }
  void reorderWhileDownloading() {
    QFETCH(QString, source);
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi(source);
    RemoteLibrary library(dir.path(), false, nullptr, api);
    auto *player = library.transport();
    player->setVolume(0);
    library.playItems({api->row("a"), api->row("b"), api->row("c")});
    const auto key = player->trackKey();
    player->move(0, 2);
    QCOMPARE(player->trackKey(), key);
    QCOMPARE(player->currentIndex(), 2);
    QCOMPARE(api->downloads.size(), 1);
    QVERIFY(library.buffering());
    api->finishDownload(0);
    QTRY_VERIFY_WITH_TIMEOUT(player->position() > 100, 5000);
    QVERIFY(!library.buffering());
    player->remove(0);
    QCOMPARE(player->trackKey(), key);
    QCOMPARE(player->currentIndex(), 1);
    QCOMPARE(api->downloads.size(), 1);
    QVERIFY(player->playing());
  }
  void pageCacheRowBudget_data() { providerRows(); }
  void pageCacheRowBudget() {
    QFETCH(QString, source);
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi(source);
    RemoteLibrary library(dir.path(), false, nullptr, api);
    emit api->changed();
    library.setEnabled(true);
    QVariantList rows;
    for (int i = 0; i < 11000; ++i)
      rows.append(api->row(QString::number(i)));
    api->finishPage(0, rows);
    library.search("second", "songs");
    api->finishPage(1, rows);
    library.search("third", "songs");
    api->finishPage(2, {api->row("third")});
    library.search("second", "songs");
    QCOMPARE(api->pages.size(), 3); // Recent large page is still cached.
    QCOMPARE(library.items().size(), 11000);
    library.show("albums");
    QCOMPARE(api->pages.size(),
             4); // Row budget evicts before the six-page limit.
    QVERIFY(library.items().isEmpty());
  }
  void folderChangeInvalidatesHistory_data() { providerRows(); }
  void folderChangeInvalidatesHistory() {
    QFETCH(QString, source);
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi(source);
    RemoteLibrary library(dir.path(), false, nullptr, api);
    emit api->changed();
    library.setEnabled(true);
    const auto album = api->row("album", "album");
    api->finishPage(0, {album});
    library.setViewState({{"query", "private"}, {"y", 600}});
    library.open(album);
    api->setFolder("new-folder");
    api->finishPage(1, {api->row("late-old-folder")});
    QVERIFY(library.items().isEmpty());
    QVERIFY(library.viewState().isEmpty());
    QVERIFY(!library.canBack());
    QVERIFY(!library.busy());
    library.show("albums");
    QCOMPARE(api->pages.size(), 3);
    api->finishPage(2, {api->row("new-folder-album", "album")});
    QCOMPARE(library.items().first().toMap().value("id").toString(),
             QString("new-folder-album"));
  }
  void playlistPermissionsRefresh_data() { providerRows(); }
  void playlistPermissionsRefresh() {
    QFETCH(QString, source);
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi(source);
    RemoteLibrary library(dir.path(), false, nullptr, api);
    emit api->changed();
    library.setEnabled(true);
    const auto playlist = api->row("list", "playlist");
    api->finishPage(0, {playlist});
    library.open(playlist);
    api->pages[1].second({{"items", QVariantList{api->row("a")}},
                          {"editable", true},
                          {"deletable", true}},
                         {});
    QVERIFY(library.collection().value("editable").toBool());
    QVERIFY(library.collection().value("deletable").toBool());
    library.back();
    library.open(playlist);
    QCOMPARE(api->pages.size(), 2);
    QVERIFY(library.collection().value("deletable").toBool());
    library.reload();
    api->pages[2].second({{"items", QVariantList{api->row("a")}},
                          {"editable", false},
                          {"deletable", false}},
                         {});
    QVERIFY(!library.collection().value("editable").toBool());
    QVERIFY(!library.collection().value("deletable").toBool());
    library.back();
    library.open(playlist);
    QCOMPARE(api->pages.size(), 3);
    QVERIFY(!library.collection().value("deletable").toBool());
  }
  void playNextRejectsForeignRowsAndHonorsManualSkip_data() { providerRows(); }
  void playNextRejectsForeignRowsAndHonorsManualSkip() {
    QFETCH(QString, source);
    QTemporaryDir dir;
    auto *api = new FixtureMusicApi(source);
    RemoteLibrary library(dir.path(), false, nullptr, api);
    auto *player = library.transport();
    player->setVolume(0);
    library.playNext(api->row("a"));
    QCOMPARE(player->count(), 1);
    QVERIFY(!player->playing());
    QVERIFY(api->downloads.isEmpty());
    auto foreign = api->row("foreign");
    foreign["server"] = "another-account";
    library.playNext(foreign);
    library.playNext(api->row("album", "album"));
    QCOMPARE(player->count(), 1);
    library.enqueue(api->row("tail"));
    library.playNext(api->row("first"));
    library.playNext(api->row("second"));
    QCOMPARE(player->queue()[1].toMap().value("title").toString(),
             QString("second"));
    QCOMPARE(player->queue()[2].toMap().value("title").toString(),
             QString("first"));
    player->setShuffle(true);
    player->setRepeatMode(2);
    QCOMPARE(player->nextTrackIndex(),
             -1); // Automatic repeat keeps the current song.
    QCOMPARE(player->nextTrackIndex(false), 1);
    player->next(false, false);
    QVERIFY(player->trackKey().endsWith("/second"));
    QVERIFY(!player->playing());
    QVERIFY(api->downloads.isEmpty());
  }
};
QTEST_MAIN(RemoteLibraryTest)
#include "remotelibrarytest.moc"
