#include "subsonictest.h"
#include "mpris.h"
#include "subsonicapi.h"
#include "testcapture.h"
#include "testinput.h"
#include "tx6.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QQmlContext>
#include <QSignalSpy>
#include <QTest>
#include <iostream>

namespace {
bool until(const std::function<bool()> &fn, int timeout = 15000) {
  QElapsedTimer t;
  t.start();
  do {
    if (fn()) return true;
    QTest::qWait(20);
  } while (t.elapsed() < timeout);
  return fn();
}
QQuickItem *find(QQuickItem *item, const QString &name) {
  if (item->objectName() == name)
    return item;
  for (auto *child : item->childItems())
    if (auto *found = find(child, name))
      return found;
  return nullptr;
}
} // namespace
int exerciseSubsonic(Player &local, RemoteLibrary &jf, QQuickWindow *window,
                     const QString &temp, const QString &captures) {
  int checks = 0, failures = 0;
  auto check = [&](bool ok, const char *text) {
    ++checks;
    if (!ok)
      ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << text << std::endl;
    return ok;
  };
  auto capture = [&](const QString &name) {
    if (captures.isEmpty())
      return;
    QTest::qWait(250);
    QDir().mkpath(captures);
    check(until([&] {
            return qAbs(window->width() -
                        qRound(window->property("layoutWidth").toDouble() *
                               window->property("uiScale").toDouble())) < 2;
          }),
          "native window fits complete layout");
    QImage image;
    check(
        until(
            [&] {
              window->requestUpdate();
              image = captureTestWindow(window);
              return image.size() ==
                     QSize(
                         qRound(window->width() * window->devicePixelRatio()),
                         qRound(window->height() * window->devicePixelRatio()));
            },
            5000),
        "capture matches current window dimensions");
    check(!image.isNull() && image.save(captures + "/" + name + ".png"),
          "rendered capture saved");
  };
  auto click = [&](const QString &name) {
    auto *item = find(window->contentItem(), name);
    if (!check(item != nullptr, qPrintable("control exists: " + name)))
      return;
    QTest::mouseClick(
        window, Qt::LeftButton, Qt::NoModifier,
        item->mapToScene(QPointF(item->width() / 2, item->height() / 2))
            .toPoint());
    QTest::qWait(100);
  };
  window->setColor(QColor("#242a30"));
  auto *api = static_cast<SubsonicApi *>(jf.server());
  auto *player = jf.transport();
  player->setVolume(0);
  local.setVolume(0);
  const auto base = qEnvironmentVariable("SPUN_TEST_SERVER"),
             user = qEnvironmentVariable("SPUN_TEST_USER"),
             password = qEnvironmentVariable("SPUN_TEST_PASSWORD");
  if (!check(!base.isEmpty() && !password.isEmpty(),
             "disposable server environment configured"))
    return 1;
  auto fixtureCall = [&](const QString &method,
                         const SubsonicApi::Params &params =
                             SubsonicApi::Params{}) {
    bool done = false;
    QVariantMap result;
    QString error;
    api->call(method, params, [&](const QVariantMap &d, const QString &e) {
      result = d;
      error = e;
      done = true;
    });
    check(until([&] { return done; }) && error.isEmpty(),
          qPrintable("fixture API: " + method));
    return result;
  };
  local.demo();
  check(until([&] { return local.count() > 0 && !local.busy(); }),
        "local queue fixture loaded");
  local.pause();
  auto localKey = local.trackKey();
  click("subsonicSourceButton");
  check(window->property("useSubsonic").toBool() && jf.enabled(),
        "Subsonic source selected");
  capture("subsonic-disconnected");
  click("subsonicConnect");
  check(until([&] {
          auto *f = find(window->contentItem(), "subsonicAddress");
          return f && f->isVisible();
        }),
        "connection dialog opens");
  check(
      until([&] {
        return find(window->contentItem(), "subsonicAddress")->hasActiveFocus();
      }),
      "connection starts with first input focused");
  QTest::keyClick(window, Qt::Key_Tab);
  check(find(window->contentItem(), "subsonicUsername")->hasActiveFocus(),
        "Tab moves to username");
  QTest::keyClick(window, Qt::Key_Tab, Qt::ShiftModifier);
  check(
      until([&] {
        return find(window->contentItem(), "subsonicAddress")->hasActiveFocus();
      }),
      "Shift Tab returns to server field");
  capture("subsonic-connect-dialog");
  for (const auto &entry :
       QList<QPair<QString, QString>>{{"subsonicAddress", base},
                                      {"subsonicUsername", user},
                                      {"subsonicPassword", password}}) {
    auto *field = find(window->contentItem(), entry.first);
    if (field)
      field->setProperty("text", entry.second);
  }
  if (auto *remember = find(window->contentItem(), "subsonicRemember"))
    remember->setProperty("checked", false);
  click("subsonicSignIn");
  if (!check(until([&] { return api->connected(); }),
             "real server sign-in from UI"))
    return 1;
  check(api->serviceName() == "Navidrome",
        "Navidrome capability identification");
  check(until([&] { return !api->folders().isEmpty(); }),
        "music libraries discovered");
  check(find(window->contentItem(), "subsonicPassword")
            ->property("text")
            .toString()
            .isEmpty(),
        "password field cleared after sign-in");
  QMetaObject::invokeMethod(find(window->contentItem(), "subsonicPanel"),
                            "closeActions");
  QTest::qWait(250);
  check(until([&] { return !jf.busy() && !jf.items().isEmpty(); }),
        "albums load after connection");
  capture("subsonic-albums");
  click("subsonicPageActions");
  click("subsonicSettings");
  capture("subsonic-settings");
  click("subsonicQuality");
  click("subsonicQuality_192");
  check(api->bitrate() == 192, "quality menu applies selected bitrate");
  api->setBitrate(0);
  QMetaObject::invokeMethod(find(window->contentItem(), "subsonicPanel"),
                            "closeActions");
  const auto folder = api->folders().first().toMap().value("id").toString();
  api->setFolder(folder);
  jf.show("songs");
  check(!folder.isEmpty() && until([&] { return !jf.busy(); }) &&
            jf.items().size() == 100,
        "music folder selection scopes catalog");
  api->setFolder("");
  jf.search("Fixture");
  check(until([&] { return !jf.busy(); }) && jf.items().size() == 100 &&
            jf.more(),
        "search first page and continuation");
  jf.loadMore();
  check(until([&] { return !jf.busy(); }) && jf.items().size() == 105 &&
            !jf.more(),
        "search pagination returns all 105 songs");
  QTest::qWait(100);
  auto *results = find(window->contentItem(), "subsonicResults");
  results->setProperty("contentY", 700);
  QMetaObject::invokeMethod(find(window->contentItem(), "subsonicPanel"), "saveView");
  click("localSourceButton");
  click("subsonicSourceButton");
  check(until([&] { auto *view = find(window->contentItem(), "subsonicResults"); return view && qAbs(view->property("contentY").toReal()-700) < 2; }), "source switch restores library scroll position");
  auto *searchField = find(window->contentItem(), "subsonicSearch");
  check(searchField && searchField->property("text").toString() == "Fixture", "source switch restores search query");
  QSet<QString> ids;
  for (const auto &v : jf.items())
    ids.insert(v.toMap().value("id").toString());
  check(ids.size() == 105, "pagination preserves unique identities");
  if (jf.items().size() < 3)
    return 1;
  const auto songs = jf.items();
  auto first = songs[0].toMap(), second = songs[1].toMap(),
       third = songs[2].toMap();
  jf.search("Fixture 001");
  jf.search("Fixture 002");
  check(until([&] { return !jf.busy(); }) && jf.items().size() == 1 &&
            jf.items()[0].toMap().value("title") == "Fixture 002",
        "latest search wins");
  jf.show("albums");
  check(until([&] { return !jf.busy(); }) && !jf.items().isEmpty(),
        "album browser");
  auto album = jf.items().first().toMap();
  jf.open(album);
  check(until([&] { return !jf.busy(); }) && jf.items().size() == 35,
        "album opens complete ordered tracks");
  jf.back();
  check(until([&] { return !jf.busy(); }) && jf.page() == "albums",
        "back returns to albums");
  jf.show("artists");
  check(until([&] { return !jf.busy(); }) && !jf.items().isEmpty(),
        "artist browser");
  jf.open(jf.items().first().toMap());
  check(until([&] { return !jf.busy(); }) && jf.items().size() == 3,
        "artist opens albums");
  jf.search("Fixture Album", "albums");
  check(until([&] { return !jf.busy(); }) && jf.items().size() == 3,
        "album search");
  jf.search("Spun Test", "artists");
  check(until([&] { return !jf.busy(); }) && jf.items().size() == 1,
        "artist search");
  jf.show("genres");
  check(until([&] { return !jf.busy(); }) && !jf.items().isEmpty(),
        "genre browser");
  jf.open(jf.items().first().toMap());
  check(until([&] { return !jf.busy(); }) && jf.items().size() == 100 &&
            jf.more(),
        "genre opens paginated songs");
  jf.toggleFavorite(first);
  check(until([&] { return !jf.actionBusy(); }) &&
            jf.favorite(first.value("id").toString()),
        "server favorite saved");
  jf.show("favorites");
  check(until([&] { return !jf.busy(); }) && jf.items().size() == 1,
        "favorites browser");
  jf.toggleFavorite(first);
  check(until([&] { return !jf.actionBusy() && !jf.busy(); }) &&
            jf.items().isEmpty(),
        "server favorite removed");
  click("subsonicPageActions");
  click("subsonicNewPlaylist");
  check(until([&] {
          auto *f = find(window->contentItem(), "subsonicPlaylistName");
          return f && f->hasActiveFocus();
        }),
        "playlist dialog receives keyboard focus");
  capture("subsonic-playlist-dialog");
  bool focusTrapped = true;
  auto *dialog = find(window->contentItem(), "subsonicNameDialog");
  for (int i = 0; i < 8; ++i) {
    QTest::keyClick(window, Qt::Key_Tab);
    QQuickItem *focus = window->activeFocusItem();
    while (focus && focus != dialog)
      focus = focus->parentItem();
    focusTrapped &= focus == dialog;
  }
  check(focusTrapped,
        "dialog retains keyboard focus through repeated Tab navigation");
  if (auto *f = find(window->contentItem(), "subsonicPlaylistName"))
    f->setProperty("text", "Spun fixture playlist");
  click("subsonicNameDialogAccept");
  check(until([&] { return api->playlists().size() == 1; }),
        "playlist created through UI");
  if (api->playlists().isEmpty())
    return 1;
  check(until([&] { return !jf.busy(); }) && jf.page() == "playlists" &&
            jf.items().size() == 1,
        "new playlist appears without manual refresh");
  check(find(window->contentItem(), "subsonicSearch")
                ->property("placeholderText")
                .toString() == "Search playlists",
        "playlist view keeps search filter in sync");
  jf.search("Spun fixture", "playlists");
  check(until([&] { return !jf.busy(); }) && jf.items().size() == 1 &&
            jf.items()[0].toMap().value("kind") == "playlist",
        "playlist search returns playlists");
  auto playlist = api->playlists().first().toMap();
  const auto playlistId = playlist.value("remoteId").toString();
  check(playlist.value("editable").toBool(),
        "owner playlist editing available");
  jf.addToPlaylist(playlistId, first);
  check(until([&] { return !jf.actionBusy(); }), "first playlist song added");
  jf.addToPlaylist(playlistId, second);
  check(until([&] { return !jf.actionBusy(); }), "second playlist song added");
  jf.open(playlist);
  check(until([&] { return !jf.busy(); }) && jf.items().size() == 2,
        "playlist contains added songs");
  jf.movePlaylistItem(0, 1);
  check(until([&] { return !jf.actionBusy() && !jf.busy(); }) &&
            jf.items().last().toMap().value("remoteId") ==
                first.value("remoteId"),
        "playlist order updated on server");
  jf.removeFromPlaylist(jf.items().first().toMap());
  check(until([&] { return !jf.actionBusy() && !jf.busy(); }) &&
            jf.items().size() == 1,
        "playlist removal uses entry identity");
  jf.renamePlaylist(playlistId, "Renamed fixture");
  check(until([&] { return !jf.actionBusy() && !jf.busy(); }) &&
            jf.heading() == "Renamed fixture",
        "playlist renamed on server");
  check(jf.collection().value("deletable").toBool(), "owner can delete playlist from shared controller");
  click("subsonicPageActions");
  auto *deleteAction = find(window->contentItem(), "subsonicDeletePlaylist");
  check(deleteAction && deleteAction->isVisible() && deleteAction->height() >= 48,
        "owner delete action visible with Material touch target");
  capture("subsonic-playlist-menu");
  QMetaObject::invokeMethod(find(window->contentItem(), "subsonicPanel"), "closeActions");
  capture("subsonic-playlist");
  for (const auto &song : QVariantList{first, second, third}) {
    jf.playItem(song.toMap());
    check(until([&] { return player->position() > 500; }),
          "native audio playback advances");
    check(player->duration() > 11000 &&
              player->title() == song.toMap().value("title"),
          "native duration and public metadata");
    player->pause();
    auto pos = player->position();
    QTest::qWait(100);
    check(!player->playing() && qAbs(player->position() - pos) < 100,
          "pause freezes transport");
    player->seek(5000);
    check(until([&] { return qAbs(player->position() - 5000) < 400; }),
          "buffered seek");
  }
  check(fixtureCall("getNowPlaying")
                .value("nowPlaying")
                .toMap()
                .value("entry")
                .toList()
                .size() > 0,
        "server receives now-playing reports");
  player->stop();
  jf.playItem(first);
  jf.playItem(second);
  jf.playItem(third);
  check(until([&] { return player->position() > 200; }) &&
            player->title() == third.value("title"),
        "rapid track changes discard obsolete audio downloads");
  player->pause();
  jf.playItems({first, second});
  check(until([&] { return player->position() > 200; }),
        "queue plays first song");
  player->pause();
  player->next(false, false);
  check(player->title() == second.value("title") && !player->playing(),
        "next preserves pause");
  player->play();
  check(until([&] { return player->position() > 200; }), "next song resolves");
  player->pause();
  jf.setActive(true);
  check(until([&] { return !jf.loading(); }) && jf.timed() &&
            jf.lines().size() == 3,
        "timed Subsonic lyrics");
  check(jf.seekToLine(1) &&
            until([&] { return qAbs(player->position() - 4000) < 400; }),
        "lyric seek uses player timeline");
  check(until([&] {
          const auto art = player->artwork();
          if (art.isNull())
            return false;
          const auto pixel = art.pixelColor(art.width() / 2, art.height() / 2);
          return qAbs(pixel.red() - 136) < 12 &&
                 qAbs(pixel.green() - 204) < 12 &&
                 qAbs(pixel.blue() - 238) < 12;
        }),
        "authenticated current artwork decoded");
  PlayerAdaptor media(&local, nullptr, nullptr, nullptr, player);
  media.setSourceWindow(window);
  check(media.metadata().value("xesam:title") == player->title(),
        "MPRIS uses Subsonic metadata");
  media.Play();
  check(until([&] { return player->playing(); }), "MPRIS plays Subsonic");
  media.Pause();
  check(!player->playing(), "MPRIS pauses Subsonic");
  click("localSourceButton");
  check(!jf.enabled() && !player->playing() && local.trackKey() == localKey,
        "source switch preserves local queue and pauses Subsonic");
  click("jellyfinSourceButton");
  check(window->property("useJellyfin").toBool() && !jf.enabled() &&
            !player->playing(),
        "Jellyfin and Subsonic remain separate sources");
  click("subsonicSourceButton");
  check(player->count() == 2, "Subsonic queue survives source switching");
  const auto beforeNextKey = player->trackKey();
  auto *panel = find(window->contentItem(), "subsonicPanel");
  QMetaObject::invokeMethod(panel, "menuFor", Q_ARG(QVariant, QVariant(first)), Q_ARG(QVariant, QVariant(-1)), Q_ARG(QVariant, QVariant()));
  check(until([&] { auto *action = find(window->contentItem(), "subsonicPlayNext"); return action && action->isVisible(); }), "Play next is available in remote song menu");
  capture("subsonic-play-next-menu");
  click("subsonicPlayNext");
  check(player->count() == 3 && player->trackKey() == beforeNextKey && player->queue()[player->currentIndex()+1].toMap().value("title") == first.value("title"), "Play next inserts without restarting current song");
  player->remove(player->currentIndex()+1);
  player->failExternal(player->trackKey(), "Connection interrupted. Retry this song.");
  check(until([&] { auto *retry = find(window->contentItem(), "remotePlaybackRetry"); return retry && retry->isVisible(); }), "failed remote playback exposes inline Retry");
  capture("subsonic-retry");
  click("remotePlaybackRetry");
  check(player->trackKey() == beforeNextKey && player->count() == 2 && until([&] { return player->position() > 200 && player->error().isEmpty(); }), "Retry reloads same song and preserves queue");
  player->pause();
  window->setProperty("libraryOpen", false);
  window->setProperty("immersive", true);
  window->setProperty("discFlipped", true);
  window->setProperty("lyricsView", true);
  check(until([&] { return !jf.loading() && jf.lines().size() == 3; }), "immersive lyrics use the current server timeline");
  QTest::qWait(200);
  capture("subsonic-immersive-lyrics");
  window->setProperty("discFlipped", false);
  window->setProperty("immersive", false);
  local.setShowPlayerBody(true);
  for (const auto &medium : QStringList{"cd", "vinyl", "cassette", "tp7"}) {
    local.setMedium(medium);
    local.setThreeD(false);
    QTest::qWait(180);
    check(window->property("deckPlayer").value<QObject *>() == player,
          "2D medium uses Subsonic transport");
    capture("subsonic-" + medium + "-2d");
    if (qEnvironmentVariable("QT_QUICK_BACKEND") != "software") {
      local.setThreeD(true);
      check(until([&] { return window->property("threeDActive").toBool(); }),
            "3D medium activates");
      QTest::qWait(500);
      capture("subsonic-" + medium + "-3d");
      window->setProperty("immersive", true);
      window->setProperty("chromeIdle", true);
      QTest::qWait(300);
      check(window->property("chromeHidden").toBool() && window->property("threeDActive").toBool(), "immersive mode retains active 3D medium");
      capture("subsonic-immersive-" + medium + "-3d");
      window->setProperty("immersive", false);
      player->seek(2000);
      click("playButton");
      check(until([&] { return player->position() > 2200; }) &&
                player->playing(),
            "3D transport starts Subsonic audio");
      click("playButton");
      check(!player->playing(), "3D transport pauses Subsonic audio");
    }
  }
  auto *mixer = qobject_cast<Tx6 *>(
      qmlContext(window)->contextProperty("tx6").value<QObject *>());
  if (check(mixer != nullptr, "TX-6 integration is available")) {
    mixer->setVisible(true);
    mixer->setPowered(true);
    mixer->reset();
    check(mixer->active(), "TX-6 routes Subsonic in TP-7 view");
    player->play();
    check(until([&] { return mixer->meters()[0].toDouble() > 0.00001; }),
          "decoded Subsonic audio reaches TX-6 mixer");
    player->pause();
    capture("subsonic-tx6");
    mixer->setVisible(false);
  }
  local.setMotion(false);
  QTest::qWait(100);
  check(find(window->contentItem(), "subsonicPanel")->opacity() == 1,
        "reduced motion leaves browser fully visible");
  local.setThreeD(false);
  fixtureCall("updatePlaylist",
              {{"playlistId", playlistId}, {"public", "true"}});
  api->connectServer(base, qEnvironmentVariable("SPUN_TEST_READER"), password,
                     false);
  check(until([&] { return api->connected() && !api->playlists().isEmpty(); }),
        "shared playlist available to reader account");
  if (!api->playlists().isEmpty()) {
    jf.open(api->playlists().first().toMap());
    check(until([&] { return !jf.busy(); }) &&
              !jf.collection().value("editable").toBool() &&
              jf.items().size() == 1,
          "read-only playlist displays tracks without edit permission");
    check(!jf.collection().value("deletable").toBool(), "reader has no playlist delete permission");
    window->setProperty("libraryOpen", true);
    click("subsonicPageActions");
    auto *removeAction = find(window->contentItem(), "subsonicDeletePlaylist");
    check(!removeAction || !removeAction->isVisible(), "read-only playlist hides Delete action");
    QMetaObject::invokeMethod(find(window->contentItem(), "subsonicPanel"), "closeActions");
    jf.removeFromPlaylist(jf.items().value(0).toMap());
    check(!jf.actionBusy() && jf.items().size() == 1,
          "read-only playlist removal blocked locally");
    bool finished = false;
    QString permissionError;
    api->editPlaylist(playlistId, {{"name", "Not allowed"}},
                      [&](const QVariantMap &, const QString &error) {
                        permissionError = error;
                        finished = true;
                      });
    check(until([&] { return finished; }) && !permissionError.isEmpty(),
          "server rejects unauthorized playlist mutation");
  }
  api->connectServer(base, user, password, false);
  check(until([&] { return api->connected() && !api->playlists().isEmpty(); }),
        "owner account reconnects after permission checks");
  jf.open(api->playlists().first().toMap());
  check(until([&] { return !jf.busy(); }), "owner playlist opens for deletion");
  window->setProperty("libraryOpen", true);
  click("subsonicPageActions");
  click("subsonicDeletePlaylist");
  capture("subsonic-delete-confirmation");
  click("subsonicDeleteDialogAccept");
  check(until([&] { return !jf.actionBusy() && api->playlists().isEmpty(); }),
        "playlist deleted on server");
  api->setBitrate(128);
  jf.playItem(first);
  check(until([&] { return player->position() > 300; }, 30000),
        "server transcoding plays at 128 kbps");
  const auto playedBefore =
      fixtureCall("getSong", {{"id", first.value("remoteId").toString()}})
          .value("song")
          .toMap()
          .value("playCount")
          .toInt();
  QTest::qWait(6500);
  player->pause();
  check(until(
            [&] {
              return fixtureCall("getSong",
                                 {{"id", first.value("remoteId").toString()}})
                         .value("song")
                         .toMap()
                         .value("playCount")
                         .toInt() > playedBefore;
            },
            4000),
        "actual listening submits play count to server");
  jf.show("recent");
  check(until([&] { return !jf.busy(); }) && !jf.items().isEmpty(),
        "recently played albums");
  api->setScrobbling(false);
  player->play();
  const auto before =
      fixtureCall("getSong", {{"id", first.value("remoteId").toString()}})
          .value("song")
          .toMap()
          .value("playCount")
          .toInt();
  player->seek(3000);
  QTest::qWait(6500);
  const auto after =
      fixtureCall("getSong", {{"id", first.value("remoteId").toString()}})
          .value("song")
          .toMap()
          .value("playCount")
          .toInt();
  check(before == after, "disabled reporting does not submit listens");
  player->pause();
  api->setScrobbling(true);
  api->setBitrate(0);
  const auto identity = api->identity();
  api->disconnectServer();
  check(!api->connected() && player->count() == 0 && jf.items().isEmpty(),
        "disconnect clears private playback and browsing state");
  api->connectServer(base, user, password, false);
  check(until([&] { return api->connected(); }) &&
            api->identity() == identity && player->count() == 1 &&
            !player->playing(),
        "same account restores queue without autoplay");
  QFile config(temp + "/subsonic/connection.ini");
  if (config.open(QIODevice::ReadOnly)) {
    const auto bytes = config.readAll();
    check(!bytes.contains(password.toUtf8()) && !bytes.contains("AccessToken"),
          "settings contain no password or token");
  }
  api->connectServer(base, qEnvironmentVariable("SPUN_TEST_READER"), password,
                     false);
  check(until([&] { return api->connected(); }) && player->count() == 0,
        "different account cannot inherit private queue");
  api->connectServer(base, user, "wrong-password", false);
  check(until([&] { return !api->connecting(); }) && !api->connected() &&
            !api->error().isEmpty(),
        "wrong password rejected with recoverable error");
  std::cout << "SUBSONIC_RESULT " << checks << " checks, " << failures
            << " failures" << std::endl;
  return failures ? 1 : 0;
}
