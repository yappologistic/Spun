#include "youtubetest.h"
#include "artwork.h"
#include "mpris.h"
#include "testcapture.h"
#include "testinput.h"
#include "tx6.h"
#include "youtube.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlContext>
#include <QSignalSpy>
#include <QTest>
#include <iostream>

static bool until(const std::function<bool()> &predicate, int timeout = 4000) {
  QElapsedTimer timer;
  timer.start();
  while (!predicate() && timer.elapsed() < timeout)
    QTest::qWait(20);
  return predicate();
}
static QQuickItem *find(QQuickItem *item, const QString &name) {
  if (item->objectName() == name)
    return item;
  for (auto *child : item->childItems())
    if (auto *result = find(child, name))
      return result;
  return nullptr;
}
int exerciseYoutube(Player &local, Youtube &yt, QQuickWindow *window,
                    const QString &temp, const QString &captures) {
  int checks = 0, failures = 0;
  auto check = [&](bool ok, const char *label) {
    ++checks;
    if (!ok)
      ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label << std::endl;
    return ok;
  };
  qputenv("SPUN_YOUTUBE_PYTHON", "/usr/bin/python3");
  qputenv("SPUN_YOUTUBE_HELPER", SPUN_SOURCE_DIR "/tests/youtube_fixture.py");
  qputenv("SPUN_YOUTUBE_FIXTURE_AUDIO", SPUN_DEMO_FILE);
  auto *p = yt.transport();
  p->setVolume(0);
  local.setVolume(0);
  auto click = [&](const QString &name) {
    auto *item = find(window->contentItem(), name);
    if (!check(item != nullptr, qPrintable("control exists: " + name)))
      return;
    QTest::mouseClick(
        window, Qt::LeftButton, Qt::NoModifier,
        item->mapToScene(QPointF(item->width() / 2, item->height() / 2))
            .toPoint());
    QTest::qWait(70);
  };
  auto capture = [&](const QString &name) {
    if (captures.isEmpty())
      return;
    QTest::qWait(200);
    QDir().mkpath(captures);
    auto image = captureTestWindow(window);
    check(!image.isNull() && image.save(captures + "/" + name + ".png"),
          "render capture saved");
  };
  local.demo();
  check(until([&] { return local.count() > 0 && !local.busy(); }),
        "local fixture loaded");
  local.pause();
  const auto localKey = local.trackKey();
  click("youtubeSourceButton");
  check(window->property("useYoutube").toBool() && yt.enabled(),
        "YouTube source selected by pointer");
  check(until([&] { return !yt.busy() && yt.ready(); }),
        "anonymous provider setup check");
  auto *searchTab = find(window->contentItem(), "youtubeTab_search");
  auto *favoritesTab = find(window->contentItem(), "youtubeTab_favorites");
  if (check(searchTab && favoritesTab, "library tabs available")) {
    searchTab->forceActiveFocus(Qt::TabFocusReason);
    const auto previousPage = yt.page();
    QTest::keyClick(window, Qt::Key_Right);
    check(favoritesTab->hasActiveFocus() && yt.page() == previousPage,
          "tab arrows move focus without changing the page");
    QTest::keyClick(window, Qt::Key_Return);
    check(yt.page() == "favorites" &&
              favoritesTab->property("selected").toBool(),
          "keyboard activation selects library tab");
    QTest::keyClick(window, Qt::Key_Home);
    check(searchTab->hasActiveFocus(), "Home focuses first library tab");
    QTest::keyClick(window, Qt::Key_Return);
  }
  yt.search("slow");
  auto *loading = find(window->contentItem(), "youtubeLoading");
  check(loading && loading->isVisible(), "pending search shows progress");
  const bool motion = local.motion();
  local.setMotion(false);
  QTest::qWait(30);
  check(loading && !loading->property("animating").toBool(),
        "reduced motion stops progress animation");
  capture("youtube-loading-reduced-motion");
  local.setMotion(motion);
  yt.search("fixture");
  check(until([&] { return !yt.busy() && yt.items().size() == 2; }),
        "latest search replaces pending request");
  QTest::qWait(2200);
  check(yt.heading() == "fixture", "stale search does not replace results");
  check(loading && !loading->isVisible() &&
            !loading->property("animating").toBool(),
        "completed search stops and hides progress");
  capture("youtube-search");
  QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);
  check(find(window->contentItem(), "youtubeSearch")->hasActiveFocus(),
        "Ctrl+F focuses YouTube search");
  window->contentItem()->forceActiveFocus();
  click("youtubeRowMenu_0");
  check(window->property("menuOpen").toBool(),
        "YouTube menu blocks underlying playback shortcuts");
  capture("youtube-song-menu");
  QTest::keyClick(window, Qt::Key_Escape);
  QTest::qWait(60);

  if (yt.items().size() != 2)
    return 1;
  auto first = yt.items()[0].toMap(), second = yt.items()[1].toMap();
  for (const auto &filter : QStringList{"albums", "artists", "playlists"}) {
    yt.search("fixture", filter);
    check(until([&] { return !yt.busy() && !yt.items().isEmpty(); }),
          qPrintable(filter + " search"));
    yt.open(yt.items().first().toMap());
    check(until([&] { return !yt.busy() && yt.items().size() == 2; }),
          qPrintable(filter + " browsing"));
    yt.back();
    check(yt.items().size() == 1, "back restores previous collection");
  }
  yt.show("home");
  check(until([&] { return !yt.busy() && yt.items().size() == 2; }),
        "anonymous discovery");
  yt.radio(first);
  check(until([&] { return !yt.busy() && yt.items().size() == 2; }),
        "song radio queue");
  yt.search("https://music.youtube.com/watch?v=fixture0001");
  check(until([&] { return !yt.busy() && yt.items().size() == 2; }),
        "YouTube link lookup");
  yt.toggleFavorite(first);
  check(yt.favorite(first["id"].toString()), "local favorite saved");
  yt.show("favorites");
  check(yt.items().size() == 1, "favorites page");
  yt.toggleFavorite(first);
  check(!yt.favorite(first["id"].toString()) && yt.items().isEmpty(),
        "favorite removed");
  yt.toggleFavorite(first);
  click("youtubePageActions");
  click("youtubeNewPlaylist");
  check(window->property("menuOpen").toBool(), "playlist dialog is modal");
  check(until([&] {
          auto *field = find(window->contentItem(), "youtubePlaylistName");
          return field && field->hasActiveFocus();
        }),
        "playlist dialog focuses its field after entering");
  capture("youtube-playlist-dialog");
  for (char letter : QByteArray("UI playlist"))
    QTest::keyClick(window, letter);
  click("youtubeNameDialogAccept");
  check(yt.playlists().size() == 1 &&
            yt.playlists().first().toMap().value("name").toString() ==
                "UI playlist",
        "playlist saved through dialog keyboard input and button");
  if (!yt.playlists().isEmpty())
    yt.deletePlaylist(yt.playlists().first().toMap().value("id").toString());
  QTest::keyClick(window, Qt::Key_Escape);
  QTest::qWait(100);
  auto playlist = yt.createPlaylist("A local playlist");
  check(!playlist.isEmpty(), "local playlist created");
  yt.addToPlaylist(playlist, first);
  yt.addToPlaylist(playlist, second);
  yt.show("playlists");
  check(yt.items().size() == 1, "playlist listing");
  yt.open(yt.items().first().toMap());
  check(yt.items().size() == 2, "playlist songs");
  yt.renamePlaylist(playlist, "Renamed playlist");
  check(yt.heading() == "Renamed playlist", "playlist rename");
  yt.removeFromPlaylist(playlist, 0);
  check(yt.items().size() == 1, "playlist song removal");
  capture("youtube-playlist");
  yt.playItems({first, second});
  check(until([&] { return p->position() > 1200; }),
        "buffered audio plays with native transport");
  check(p->title() == first["title"].toString() && p->count() == 2,
        "public metadata and queue survive resolution");
  p->pause();
  click("playButton");
  check(until([&] { return p->playing(); }),
        "main play button controls YouTube");
  click("playButton");
  check(!p->playing(), "main pause button controls YouTube");
  local.setMiniMode(true);
  QTest::qWait(180);
  click("miniPlay");
  check(until([&] { return p->playing(); }), "Mini play controls YouTube");
  click("miniPlay");
  local.setMiniMode(false);
  QTest::qWait(100);
  QTest::keyClick(window, Qt::Key_Space);
  check(until([&] { return p->playing(); }), "Space controls YouTube");
  p->pause();
  const auto position = p->position();
  QTest::qWait(160);
  check(!p->playing() && qAbs(p->position() - position) < 100,
        "pause stops playback clock");
  p->seek(9000);
  check(until([&] { return qAbs(p->position() - 9000) < 300; }),
        "seek uses same transport timeline");
  yt.setActive(true);
  check(until([&] { return !yt.loading() && yt.lines().size() == 2; }),
        "timed lyrics load");
  check(yt.seekToLine(1) &&
            until([&] { return qAbs(p->position() - 5000) < 300; }),
        "lyric seek");
  p->next(false, false);
  check(p->title() == second["title"].toString() && !p->playing(),
        "next while paused preserves pause");
  p->play();
  check(until([&] { return p->position() > 100; }),
        "next selected song resolves");
  p->pause();
  p->seek(0);
  p->previous(false);
  check(p->title() == first["title"].toString(), "previous selects first song");
  p->move(0, 1);
  check(p->currentIndex() == 1 && p->title() == first["title"].toString(),
        "queue reorder retains current identity");
  p->remove(0);
  check(p->count() == 1 && p->currentIndex() == 0,
        "queue removal keeps current song");
  yt.enqueue(second);
  check(p->count() == 2, "enqueue adds song");
  p->setShuffle(true);
  p->next(false, false);
  check(p->currentIndex() == 1, "shuffle chooses another song");
  p->setShuffle(false);
  p->setRepeatMode(2);
  p->next(true);
  check(p->currentIndex() == 1, "repeat one retains song");
  p->setRepeatMode(1);
  p->next(true, false);
  check(p->currentIndex() == 0, "repeat queue wraps");
  p->setRepeatMode(0);
  p->select(1, false);
  p->next(true);
  check(!p->playing(), "queue ends without repeat");
  auto slow = first;
  slow["id"] = "fixture0003";
  yt.playItem(slow);
  p->seek(10000);
  p->pause();
  QTest::qWait(2400);
  check(!p->playing(), "pause cancels pending playback");
  yt.playItem(slow);
  yt.playItem(first);
  check(until([&] { return p->position() > 100; }),
        "new song replaces slow resolution");
  QTest::qWait(2100);
  check(p->trackKey().endsWith("fixture0001"),
        "stale resolver cannot change song");
  yt.playItem(slow);
  window->setProperty("useYoutube", false);
  QTest::qWait(2300);
  check(!p->playing() && local.trackKey() == localKey,
        "switch to Local cancels YouTube without changing local queue");
  window->setProperty("useYoutube", true);
  auto bad = first;
  bad["id"] = "fixture0004";
  yt.playItem(bad);
  check(until([&] { return !p->error().isEmpty(); }),
        "unavailable song displays error");
  check(!p->playing(), "failed playback clears pending state");
  yt.playItems({first, second});
  check(until([&] { return p->position() > 100; }),
        "playback recovers after error");
  p->pause();
  p->setVolume(.4);
  check(qAbs(p->volume() - .4) < .001, "volume control");
  p->setVolume(0);
  PlayerAdaptor media(&local, nullptr, p);
  media.setSourceWindow(window);
  check(media.metadata()["xesam:title"].toString() == p->title(),
        "media keys expose active YouTube metadata");
  media.SetPosition(QDBusObjectPath(media.trackId()), 7000000);
  check(until([&] { return qAbs(p->position() - 7000) < 300; }),
        "MPRIS seek routed to YouTube");
  media.PlayPause();
  check(until([&] { return p->playing(); }),
        "MPRIS playback routed to YouTube");
  media.Pause();
  auto *mixer = qmlContext(window)->contextProperty("tx6").value<Tx6 *>();
  local.setMedium("tp7");
  mixer->setVisible(true);
  check(mixer->active(), "TX-6 routes YouTube through native mixer");
  mixer->setEq(0, 1, 3);
  mixer->setDelay(true);
  mixer->setCompressor(true);
  check(mixer->delay() && mixer->compressor(),
        "TX-6 effects accept YouTube route");
  mixer->reset();
  yt.show("history");
  check(!yt.items().isEmpty(), "listening history recorded locally");
  yt.clearHistory();
  check(yt.items().isEmpty(), "history cleared");
  yt.search("error");
  check(until([&] { return !yt.busy() && !yt.error().isEmpty(); }),
        "network failure visible");
  capture("youtube-error");
  yt.search("empty");
  check(until([&] { return !yt.busy() && yt.error().isEmpty(); }) &&
            yt.items().isEmpty(),
        "empty search clears old results");
  yt.search("fixture");
  check(until([&] { return !yt.busy() && yt.items().size() == 2; }),
        "retry recovers search");
  window->setProperty("libraryOpen", true);
  p->setExternalArtwork(
      p->trackKey(),
      readTrackArtwork(QStringLiteral(SPUN_DEMO_FILE), {}, QSize(800, 800)));
  for (const auto &medium : QStringList{"cd", "vinyl", "cassette", "tp7"}) {
    local.setMedium(medium);
    for (bool threeD : {false, true}) {
      if (threeD && !qmlContext(window)->contextProperty("supports3D").toBool())
        continue;
      local.setThreeD(threeD);
      QTest::qWait(500);
      check(window->property("deckPlayer").value<QObject *>() == p,
            "physical deck uses YouTube transport");
      if (threeD)
        check(until([&] { return window->property("threeDActive").toBool(); }),
              "3D scene ready");
      p->seek(4000);
      p->play();
      check(until([&] { return p->position() > 4100; }),
            "deck playback clock advances");
      p->pause();
      capture(medium + (threeD ? "-3d" : "-2d"));
    }
  }
  local.setMedium("vinyl");
  local.setVinylAlbumMode(true);
  p->setVinylAlbumMode(true);
  check(p->playAlbumPosition(QString("https://music.youtube.com/watch?v=") +
                                 second["id"].toString(),
                             6000),
        "vinyl album needle accepts YouTube track");
  check(until([&] { return p->position() >= 6000; }),
        "vinyl album seek applies after resolution");
  p->pause();
  QTest::qWait(350);
  {
    Youtube restored(QFileInfo(temp + "/player.ini").absolutePath() +
                     "/youtube");
    check(restored.favorite(first["id"].toString()) &&
              restored.playlists().size() == 1,
          "favorites and playlists persist without account");
    check(restored.transport()->count() == 2,
          "queue references restored without auto playback");
  }
  yt.deletePlaylist(playlist);
  check(yt.playlists().isEmpty(), "playlist deleted");
  p->clear();
  check(p->count() == 0 && !p->playing(), "queue clears safely");
  const auto corrupt = temp + "/corrupt-youtube";
  QDir().mkpath(corrupt);
  {
    QFile f(corrupt + "/library.json");
    check(f.open(QIODevice::WriteOnly), "corrupt-library fixture writable");
    f.write("broken fixture");
  }
  {
    Youtube corruptLibrary(corrupt);
    corruptLibrary.createPlaylist("Do not overwrite");
  }
  QFile original(corrupt + "/library.json");
  check(original.open(QIODevice::ReadOnly), "corrupt-library fixture readable");
  check(original.readAll() == "broken fixture",
        "corrupt library is not overwritten");
  check(Youtube::cleanItem({{"id", "../bad"}, {"kind", "song"}}).isEmpty(),
        "invalid provider identity rejected");
  std::cout << "YouTube checks " << checks << ", failures " << failures
            << std::endl;
  return failures ? 1 : 0;
}

int exerciseYoutubeLive(Player &local, Youtube &yt, QQuickWindow *window,
                        const QString &captures) {
  int checks = 0, failures = 0;
  auto check = [&](bool ok, const char *label) {
    ++checks;
    if (!ok)
      ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label << std::endl;
    return ok;
  };
  auto *p = yt.transport();
  p->setVolume(0);
  window->setProperty("useYoutube", true);
  window->setProperty("libraryOpen", true);
  if (!check(until([&] { return !yt.busy() && yt.ready(); }, 45000),
             "live provider ready without account"))
    return 1;
  yt.search("Kevin MacLeod Carefree", "songs");
  if (!check(until([&] { return !yt.busy() && !yt.items().isEmpty(); }, 45000),
             "live catalog search"))
    return 1;
  auto first = yt.items().first().toMap();
  yt.playItem(first);
  if (!check(
          until([&] { return p->position() > 1000 || !p->error().isEmpty(); },
                90000) &&
              p->error().isEmpty(),
          "live anonymous audio playback"))
    return 1;
  check(until([&] { return !p->artwork().isNull(); }, 16000),
        "live artwork is decoded for physical players");
  p->pause();
  p->seek(30000);
  check(until([&] { return qAbs(p->position() - 30000) < 500; }),
        "live buffered audio seeking");
  p->play();
  check(until([&] { return p->position() > 30500; }), "live resume after seek");
  p->pause();
  auto capture = [&](const QString &name) {
    if (captures.isEmpty())
      return;
    QDir().mkpath(captures);
    QTest::qWait(350);
    auto frame = captureTestWindow(window);
    check(!frame.isNull() && frame.save(captures + "/" + name + ".png"),
          "live rendered capture saved");
  };
  capture("live-search");
  for (const auto &medium : QStringList{"cd", "vinyl", "cassette", "tp7"}) {
    local.setMedium(medium);
    for (bool threeD : {false, true}) {
      if (threeD && !qmlContext(window)->contextProperty("supports3D").toBool())
        continue;
      local.setThreeD(threeD);
      QTest::qWait(600);
      if (threeD)
        check(until([&] { return window->property("threeDActive").toBool(); }),
              "live 3D player ready");
      capture(medium + (threeD ? "-3d" : "-2d"));
    }
  }
  auto *mixer = qmlContext(window)->contextProperty("tx6").value<Tx6 *>();
  mixer->setVisible(true);
  capture("tp7-tx6");
  yt.setActive(true);
  check(until([&] { return !yt.loading(); }, 45000),
        "live lyrics or empty instrumental lyrics handled");
  std::cout << "Live YouTube checks " << checks << ", failures " << failures
            << std::endl;
  return failures ? 1 : 0;
}
