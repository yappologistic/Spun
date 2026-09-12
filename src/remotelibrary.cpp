#include "remotelibrary.h"
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

RemoteLibrary::RemoteLibrary(const QString &directory, bool restore,
                             QObject *parent, RemoteMusicApi *api)
    : QObject(parent),
      m_apiOwner(api ? api
                     : new JellyfinApi(restore, directory + "/connection.ini")),
      m_api(*m_apiOwner), m_player(directory + "/player.ini", this, true),
      m_directory(directory) {
  m_player.setExternalName(m_api.serviceName());
  m_prefetchTimer.setSingleShot(true);
  m_prefetchTimer.setInterval(300);
  connect(&m_prefetchTimer, &QTimer::timeout, this, &RemoteLibrary::prefetch);
  m_save.setSingleShot(true);
  m_save.setInterval(200);
  connect(&m_save, &QTimer::timeout, this, &RemoteLibrary::persist);
  connect(&m_api, &RemoteMusicApi::message, this, [this](const QString &s) {
    emit feedback(s, !m_api.error().isEmpty());
  });
  connect(&m_api, &RemoteMusicApi::accountChanged, this, [this] {
    persist();
    ++m_generation;
    cancelPrefetch();
    m_pages.clear();
    m_pageOrder.clear();
    m_viewState.clear();
    emit viewRestored();
    m_player.stop();
    m_player.setExternalTracks({}, 0, false);
    m_audio.reset();
    m_art.reset();
    m_items.clear();
    m_back.clear();
    m_catalog.clear();
    for (const auto &path : m_thumbs)
      QFile::remove(QUrl(path).toLocalFile());
    m_thumbs.clear();
    m_thumbOrder.clear();
    m_thumbPending.clear();
    m_thumbActive.clear();
    m_identity.clear();
    m_reported.clear();
    m_lines.clear();
    m_timeline.reset({});
    m_busy = m_more = m_actionBusy = m_thumbBusy = m_loading = false;
    m_collection.clear();
    m_error.clear();
    emit changed();
  });
  connect(&m_api, &RemoteMusicApi::changed, this, [this] {
    m_player.setExternalName(m_api.serviceName());
    if (m_pageFolder != m_api.folder()) {
      m_pageFolder = m_api.folder();
      m_pages.clear();
      m_pageOrder.clear();
      ++m_generation;
      m_api.cancel("catalog");
      m_busy = false;
      m_items.clear();
      m_back.clear();
      m_viewState.clear();
      emit viewRestored();
    }
    if (m_prefetchBitrate != m_api.bitrate()) {
      cancelPrefetch();
      m_prefetchTimer.start();
    }
    if (m_api.connected() && m_identity != m_api.identity()) {
      m_identity = m_api.identity();
      restoreQueue();
      if (m_enabled)
        show("albums");
    }
    emit changed();
  });
  connect(&m_player, &Player::externalRequested, this,
          &RemoteLibrary::loadCurrent);
  connect(&m_player, &Player::externalCancelled, this, [this] {
    ++m_audioGeneration;
    m_api.cancel("audio");
    m_buffering = false;
    emit changed();
  });
  connect(&m_player, &Player::trackChanged, this, [this] {
    if (m_observedKey == m_player.trackKey()) {
      if (m_player.artwork().isNull())
        loadArt();
      m_prefetchTimer.start();
      emit changed();
      return;
    }
    m_observedKey = m_player.trackKey();
    report(true);
    // Queue reordering can emit trackChanged without replacing the playing
    // song.
    if (m_audioKey != m_player.trackKey())
      m_audio.reset();
    ++m_audioGeneration;
    m_api.cancel("audio");
    m_buffering = false;
    loadArt();
    refresh();
    m_save.start();
    emit changed();
  });
  connect(&m_player, &Player::queueChanged, this, [this] {
    m_save.start();
    m_prefetchTimer.start();
  });
  connect(&m_player, &Player::settingsChanged, this,
          [this] { m_prefetchTimer.start(); });
  connect(&m_player, &Player::playingChanged, this, [this] {
    m_prefetchTimer.start();
    report(m_player.playbackState() == QMediaPlayer::StoppedState &&
           !m_player.busy());
  });
  connect(&m_player, &Player::positionChanged, this, [this] {
    updateLyrics();
    if (m_reported.isEmpty() && m_player.playing())
      report();
    else if (!m_reported.isEmpty() && m_player.position() > 0)
      m_reportedPosition = m_player.position();
  });
  connect(&m_player, &Player::seeked, this, [this] { report(); });
  connect(&m_player, &Player::errorChanged, this, [this] {
    if (!m_player.error().isEmpty()) {
      report(true);
      cancelPrefetch();
      emit changed();
    }
  });
  m_reportTimer.setInterval(10000);
  connect(&m_reportTimer, &QTimer::timeout, this, [this] { report(); });
  m_reportTimer.start();
}
RemoteLibrary::~RemoteLibrary() {
  cancelPrefetch();
  persist();
  report(true);
  m_player.stop();
}
void RemoteLibrary::setEnabled(bool value) {
  if (m_enabled == value)
    return;
  m_enabled = value;
  if (!value) {
    cancelPrefetch();
    ++m_generation;
    m_player.pause();
    report(true);
    m_api.cancel("catalog");
    m_api.cancel("thumb");
    m_busy = m_thumbBusy = false;
    m_thumbActive.clear();
  } else if (m_api.connected()) {
    if (m_items.isEmpty())
      reload();
    else
      loadThumbs();
  }
  emit changed();
}
QVariantMap RemoteLibrary::original(const QVariantMap &row) const {
  return m_catalog.value(row.value("id").toString(), row);
}
QVariantMap RemoteLibrary::current() const {
  return m_catalog.value(m_player.currentUrl().path().mid(1));
}
void RemoteLibrary::show(const QString &mode) {
  savePage();
  m_back.clear();
  m_collection.clear();
  browse({{"mode", mode}});
}
void RemoteLibrary::search(const QString &query, const QString &filter) {
  savePage();
  m_back.clear();
  m_collection.clear();
  browse({{"mode", "search"},
          {"query", query.trimmed().left(512)},
          {"filter", filter}});
}
void RemoteLibrary::open(const QVariantMap &row) {
  if (!m_api.owns(row))
    return;
  if (row.value("kind") == "song") {
    playItem(row);
    return;
  }
  savePage();
  m_back.append(
      QVariantMap{{"request", m_request}, {"collection", m_collection}});
  if (m_back.size() > 30)
    m_back.removeFirst();
  m_collection = row;
  browse({{"mode", row.value("kind")}, {"remoteId", row.value("remoteId")}});
}
void RemoteLibrary::back() {
  if (m_back.isEmpty())
    return;
  savePage();
  auto state = m_back.takeLast().toMap();
  m_collection = state.value("collection").toMap();
  browse(state.value("request").toMap());
}
void RemoteLibrary::reload() { browse(m_request, false, true); }
void RemoteLibrary::loadMore() {
  if (m_more && !m_busy)
    browse(m_request, true);
}
void RemoteLibrary::savePage() {
  if (m_busy || !m_error.isEmpty() || m_items.isEmpty())
    return;
  const auto key = QString::fromUtf8(
      QJsonDocument::fromVariant(m_request).toJson(QJsonDocument::Compact));
  m_pages[key] = {{"items", m_items},
                  {"collection", m_collection},
                  {"heading", m_heading},
                  {"more", m_more},
                  {"view", m_viewState}};
  m_pageOrder.removeAll(key);
  m_pageOrder.append(key);
  qsizetype total = 0;
  for (const auto &page : m_pages)
    total += page.value("items").toList().size();
  while (!m_pageOrder.isEmpty() && (m_pages.size() > 6 || total > 20000)) {
    auto old = m_pageOrder.takeFirst();
    total -= m_pages.take(old).value("items").toList().size();
  }
}
void RemoteLibrary::browse(const QVariantMap &request, bool append,
                           bool refresh) {
  if (!m_api.connected())
    return;
  ++m_generation;
  const auto generation = m_generation;
  m_api.cancel("thumb");
  m_thumbBusy = false;
  m_thumbPending.clear();
  m_thumbActive.clear();
  m_api.cancel("catalog");
  const bool same = request == m_request;
  m_request = request;
  const auto key = QString::fromUtf8(
      QJsonDocument::fromVariant(request).toJson(QJsonDocument::Compact));
  if (!append && !refresh && m_pages.contains(key)) {
    const auto state = m_pages.value(key);
    m_items = state.value("items").toList();
    m_collection = state.value("collection").toMap();
    m_heading = state.value("heading").toString();
    m_more = state.value("more").toBool();
    m_viewState = state.value("view").toMap();
    m_error.clear();
    m_busy = false;
    for (const auto &row : m_items)
      m_catalog[row.toMap().value("id").toString()] = row.toMap();
    emit changed();
    emit viewRestored();
    loadThumbs();
    return;
  }
  if (!append && !refresh)
    m_viewState = {{"query", request.value("query")},
                   {"filter", request.value("filter", request.value("mode"))}};
  m_busy = true;
  m_error.clear();
  if (!append && !(refresh && same))
    m_items.clear();
  auto req = request;
  req["offset"] = append ? m_items.size() : 0;
  const auto mode = page();
  m_heading = m_collection.value("title").toString();
  if (m_heading.isEmpty())
    m_heading = mode == "search" ? request.value("query").toString()
                                 : mode.left(1).toUpper() + mode.mid(1);
  emit changed();
  emit viewRestored();
  m_api.browse(req, [this, generation, append](const QVariantMap &data,
                                               const QString &error) {
    if (generation != m_generation)
      return;
    m_busy = false;
    m_error = error;
    if (error.isEmpty()) {
      const auto rows = data.value("items").toList();
      for (const auto &v : rows) {
        auto row = v.toMap();
        if (m_api.owns(row))
          m_catalog[row.value("id").toString()] = row;
      }
      if (!append)
        m_items = rows;
      else
        m_items.append(rows);
      m_more = data.value("more").toBool();
      if (data.contains("editable"))
        m_collection["editable"] = data.value("editable");
      if (data.contains("deletable"))
        m_collection["deletable"] = data.value("deletable");
      if (data.contains("title"))
        m_heading = data.value("title").toString();
      // Retain only displayed rows and queued tracks, not every visited page.
      QSet<QString> keep;
      for (const auto &v : m_items)
        keep.insert(v.toMap().value("id").toString());
      for (const auto &v : m_player.queue())
        keep.insert(QUrl(v.toMap().value("path").toString()).path().mid(1));
      for (auto it = m_catalog.begin(); it != m_catalog.end();)
        if (!keep.contains(it.key()))
          it = m_catalog.erase(it);
        else
          ++it;
      loadThumbs();
    }
    emit changed();
    emit viewRestored();
  });
}
QList<Track> RemoteLibrary::tracks(const QVariantList &rows) {
  QList<Track> out;
  for (const auto &v : rows) {
    auto row = original(v.toMap());
    if (!m_api.owns(row) || row.value("kind") != "song")
      continue;
    const auto id = row.value("id").toString();
    m_catalog[id] = row;
    Track t;
    t.path = m_api.scheme() + "://" + m_api.identity() + "/" + id;
    t.title = row.value("title").toString();
    t.artist = row.value("artist").toString();
    t.album = row.value("album").toString();
    t.duration = row.value("seconds").toLongLong() * 1000;
    t.number = row.value("trackNumber").toInt();
    t.discNumber = row.value("discNumber", 1).toInt();
    t.year = row.value("year").toInt();
    t.cover = m_thumbs.value(id);
    out.append(t);
    if (out.size() >= 20000)
      break;
  }
  return out;
}
void RemoteLibrary::playItems(const QVariantList &rows, int index) {
  auto queue = tracks(rows);
  if (!queue.isEmpty())
    m_player.setExternalTracks(queue, qBound(0, index, int(queue.size() - 1)),
                               true);
}
void RemoteLibrary::playItem(const QVariantMap &row) { playItems({row}); }
void RemoteLibrary::enqueue(const QVariantMap &row) {
  auto queue = tracks({row});
  if (!queue.isEmpty()) {
    m_player.appendExternalTracks(queue);
    emit feedback("Added to queue", false);
  }
}
void RemoteLibrary::playNext(const QVariantMap &row) {
  const auto queue = tracks({row});
  if (queue.isEmpty())
    return;
  m_player.insertExternalNext(queue);
  emit feedback("Playing next", false);
}
void RemoteLibrary::retry() {
  cancelPrefetch();
  m_audio.reset();
  m_player.retryExternal();
}
void RemoteLibrary::cancelPrefetch() {
  m_prefetchTimer.stop();
  ++m_prefetchGeneration;
  m_api.cancel("prefetch");
  m_prefetched.reset();
  m_prefetchKey.clear();
  m_prefetchReady = false;
  m_prefetchBitrate = m_api.bitrate();
}
void RemoteLibrary::prefetch() {
  const auto key = m_player.queuedTrackKey(m_player.nextTrackIndex());
  if (!m_enabled || !m_api.connected() || key.isEmpty()) {
    cancelPrefetch();
    return;
  }
  if (key == m_prefetchKey && m_prefetchBitrate == m_api.bitrate())
    return;
  cancelPrefetch();
  if (!m_audio || !m_player.playing() || m_buffering)
    return;
  const auto row = m_catalog.value(QUrl(key).path().mid(1));
  if (key.isEmpty() || !m_api.owns(row))
    return;
  m_prefetchKey = key;
  const auto generation = m_prefetchGeneration;
  auto buffer = std::make_shared<QTemporaryDir>();
  m_prefetched = buffer;
  m_api.download(
      row, buffer->filePath("audio"),
      [this, buffer, key, generation](const QVariantMap &,
                                      const QString &error) {
        if (generation != m_prefetchGeneration || key != m_prefetchKey)
          return;
        m_prefetchReady =
            error.isEmpty() && QFileInfo::exists(buffer->filePath("audio"));
        if (!m_prefetchReady)
          m_prefetched.reset();
      },
      "prefetch");
}
void RemoteLibrary::loadCurrent() {
  const auto row = current();
  const auto key = m_player.trackKey();
  if (!m_api.owns(row)) {
    m_player.failExternal(key, "Connect to this song's music server first.");
    return;
  }
  const auto generation = ++m_audioGeneration;
  if (key == m_prefetchKey && m_prefetchReady &&
      m_prefetchBitrate == m_api.bitrate()) {
    m_audio = m_prefetched;
    m_audioKey = key;
    cancelPrefetch();
    m_player.resolveExternal(key,
                             QUrl::fromLocalFile(m_audio->filePath("audio")));
    m_prefetchTimer.start();
    return;
  }
  cancelPrefetch();
  auto buffer = std::make_shared<QTemporaryDir>();
  m_buffering = true;
  emit changed();
  m_api.download(row, buffer->filePath("audio"),
                 [this, buffer, key, generation](const QVariantMap &,
                                                 const QString &error) {
                   if (generation != m_audioGeneration ||
                       key != m_player.trackKey())
                     return;
                   m_buffering = false;
                   emit changed();
                   if (!error.isEmpty()) {
                     m_player.failExternal(key, error);
                     return;
                   }
                   m_audio = buffer;
                   m_audioKey = key;
                   m_player.resolveExternal(
                       key, QUrl::fromLocalFile(buffer->filePath("audio")));
                   m_prefetchTimer.start();
                 });
}
void RemoteLibrary::loadArt() {
  m_api.cancel("cover");
  const auto row = current();
  const auto key = m_player.trackKey();
  if (!m_api.owns(row))
    return;
  auto dir = std::make_shared<QTemporaryDir>();
  m_api.cover(row, dir->filePath("cover.png"),
              [this, dir, key](const QVariantMap &d, const QString &) {
                if (key != m_player.trackKey())
                  return;
                QImage image(d.value("file").toString());
                if (!image.isNull()) {
                  m_art = dir;
                  m_player.setExternalArtwork(key, image);
                }
              });
}
QString RemoteLibrary::artwork(const QString &id) {
  if (m_thumbs.contains(id)) {
    m_thumbOrder.removeAll(id);
    m_thumbOrder.append(id);
    return m_thumbs.value(id);
  }
  if (m_enabled && m_catalog.contains(id) &&
      !m_catalog.value(id).value("art").toString().isEmpty() &&
      id != m_thumbActive && !m_thumbPending.contains(id) &&
      m_thumbPending.size() < 64) {
    m_thumbPending.append(id);
    QTimer::singleShot(0, this, &RemoteLibrary::loadThumbs);
  }
  return {};
}
void RemoteLibrary::loadThumbs() {
  if (m_thumbBusy || !m_enabled || !m_thumbnailDirectory.isValid())
    return;
  while (!m_thumbPending.isEmpty()) {
    const auto id = m_thumbPending.takeFirst();
    if (!m_catalog.contains(id) || m_thumbs.contains(id))
      continue;
    m_thumbBusy = true;
    m_thumbActive = id;
    const auto generation = m_generation;
    m_api.cover(
        m_catalog.value(id), m_thumbnailDirectory.filePath(id + ".png"),
        [this, generation, id](const QVariantMap &d, const QString &) {
          if (generation != m_generation)
            return;
          m_thumbBusy = false;
          m_thumbActive.clear();
          while (m_thumbs.size() >= 256 && !m_thumbOrder.isEmpty()) {
            const auto evicted = m_thumbOrder.takeFirst();
            QFile::remove(QUrl(m_thumbs.take(evicted)).toLocalFile());
          }
          const auto file = d.value("file").toString();
          m_thumbs[id] =
              file.isEmpty() ? QString() : QUrl::fromLocalFile(file).toString();
          m_thumbOrder.append(id);
          emit artworkChanged();
          QTimer::singleShot(0, this, &RemoteLibrary::loadThumbs);
        },
        "thumb");
    return;
  }
}
void RemoteLibrary::finishAction(const QString &error, bool refreshPage) {
  m_pages.clear();
  m_pageOrder.clear();
  m_actionBusy = false;
  if (!error.isEmpty())
    emit feedback(error, true);
  else if (refreshPage)
    reload();
  emit changed();
}
void RemoteLibrary::toggleFavorite(const QVariantMap &row) {
  if (m_actionBusy)
    return;
  m_actionBusy = true;
  emit changed();
  m_api.star(original(row), !favorite(row.value("id").toString()),
             [this](const QVariantMap &, const QString &e) {
               finishAction(e, page() == "favorites");
             });
}
void RemoteLibrary::createPlaylist(const QString &name) {
  if (m_actionBusy || name.trimmed().isEmpty())
    return;
  m_actionBusy = true;
  emit changed();
  m_api.createPlaylist(name, [this](const QVariantMap &, const QString &error) {
    finishAction(error);
    if (error.isEmpty())
      show("playlists");
  });
}
void RemoteLibrary::renamePlaylist(const QString &id, const QString &name) {
  if (m_actionBusy || name.trimmed().isEmpty())
    return;
  m_actionBusy = true;
  emit changed();
  m_api.editPlaylist(
      id, {{"name", name.trimmed().left(120)}},
      [this](const QVariantMap &, const QString &e) { finishAction(e, true); });
}
void RemoteLibrary::addToPlaylist(const QString &id, const QVariantMap &row) {
  if (m_actionBusy || !m_api.owns(row))
    return;
  m_actionBusy = true;
  emit changed();
  m_api.editPlaylist(id, {{"songIdToAdd", row.value("remoteId").toString()}},
                     [this](const QVariantMap &, const QString &e) {
                       finishAction(e, page() == "playlist");
                     });
}
void RemoteLibrary::removeFromPlaylist(const QVariantMap &row) {
  if (m_actionBusy || page() != "playlist" ||
      !m_collection.value("editable").toBool())
    return;
  m_actionBusy = true;
  emit changed();
  m_api.editPlaylist(
      m_collection.value("remoteId").toString(),
      {{"entryIdToRemove", row.value("entryId").toString()}},
      [this](const QVariantMap &, const QString &e) { finishAction(e, true); });
}
void RemoteLibrary::movePlaylistItem(int from, int to) {
  if (m_actionBusy || page() != "playlist" ||
      !m_collection.value("editable").toBool() || from < 0 || to < 0 ||
      from >= m_items.size() || to >= m_items.size() || from == to)
    return;
  const auto entry = m_items[from].toMap().value("entryId").toString();
  if (entry.isEmpty())
    return;
  m_actionBusy = true;
  emit changed();
  m_api.movePlaylistItem(
      m_collection.value("remoteId").toString(), entry, to,
      [this](const QVariantMap &, const QString &e) { finishAction(e, true); });
}
void RemoteLibrary::deletePlaylist(const QString &id) {
  if (m_actionBusy)
    return;
  m_actionBusy = true;
  emit changed();
  m_api.removePlaylist(id, [this](const QVariantMap &, const QString &e) {
    finishAction(e);
    if (e.isEmpty())
      show("playlists");
  });
}
void RemoteLibrary::copyLink(const QVariantMap &row) {
  if (!m_api.owns(row))
    return;
  const QUrl u = m_api.shareUrl(row);
  if (u.isEmpty())
    return;
  QGuiApplication::clipboard()->setText(u.toString());
  emit feedback("Link copied", false);
}
void RemoteLibrary::setActive(bool active) {
  if (m_active == active)
    return;
  m_active = active;
  if (active)
    refresh();
  else {
    m_api.cancel("lyrics");
    m_loading = false;
  }
  emit changed();
}
void RemoteLibrary::refresh() {
  m_lines.clear();
  m_timeline.reset({});
  m_lyricIndex = -1;
  m_timed = false;
  m_loading = false;
  m_message = "No lyrics available";
  emit currentIndexChanged();
  const auto row = current();
  const auto key = m_player.trackKey();
  if (!m_active || !m_api.owns(row)) {
    emit changed();
    return;
  }
  m_loading = true;
  emit changed();
  m_api.lyrics(row, [this, key](const QVariantMap &d, const QString &error) {
    if (key != m_player.trackKey())
      return;
    m_loading = false;
    m_lines = d.value("lines").toList();
    m_timed = !m_lines.isEmpty();
    if (!m_timed)
      for (const auto &line :
           d.value("lyrics").toString().split('\n', Qt::SkipEmptyParts))
        m_lines.append(QVariantMap{{"text", line}, {"start", -1}, {"end", -1}});
    m_message = error.isEmpty()
                    ? (m_lines.isEmpty() ? "No lyrics available" : "")
                    : error;
    m_timeline.reset(m_timed ? m_lines : QVariantList{});
    updateLyrics();
    emit changed();
  });
}
void RemoteLibrary::updateLyrics() {
  const auto i = m_timeline.indexAt(m_player.position());
  if (i != m_lyricIndex) {
    m_lyricIndex = i;
    emit currentIndexChanged();
  }
}
bool RemoteLibrary::seekToLine(int index) {
  if (!m_timed || index < 0 || index >= m_lines.size())
    return false;
  m_player.seek(m_lines[index].toMap().value("start").toLongLong());
  return true;
}
void RemoteLibrary::report(bool stopped) {
  if (stopped) {
    if (!m_reported.isEmpty())
      m_api.reportPlayback(m_reported, m_reportedPosition, true, true);
    m_reported.clear();
    return;
  }
  if (!m_enabled || m_player.busy() ||
      m_player.playbackState() == QMediaPlayer::StoppedState ||
      (m_reported.isEmpty() && !m_player.playing()))
    return;
  const auto row = current();
  if (!m_api.owns(row))
    return;
  m_reported = row;
  m_reportedPosition = m_player.position();
  m_api.reportPlayback(row, m_reportedPosition, !m_player.playing(), false);
}
void RemoteLibrary::persist() {
  if (m_identity.isEmpty() || !m_storageValid)
    return;
  QDir().mkpath(m_directory);
  QVariantList rows;
  for (const auto &v : m_player.queue()) {
    auto id = QUrl(v.toMap().value("path").toString()).path().mid(1);
    if (m_catalog.contains(id))
      rows.append(m_catalog[id]);
  }
  QSaveFile f(m_directory + "/queue-" + m_identity + ".json");
  if (!f.open(QIODevice::WriteOnly))
    return;
  f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  f.write(
      QJsonDocument(QJsonObject{{"version", 1},
                                {"queue", QJsonArray::fromVariantList(rows)},
                                {"index", m_player.currentIndex()}})
          .toJson(QJsonDocument::Compact));
  f.commit();
}
void RemoteLibrary::restoreQueue() {
  m_storageValid = true;
  QFile f(m_directory + "/queue-" + m_identity + ".json");
  if (!f.exists())
    return;
  if (!f.open(QIODevice::ReadOnly) || f.size() > 16 * 1024 * 1024) {
    m_storageValid = false;
    return;
  }
  QJsonParseError error;
  auto doc = QJsonDocument::fromJson(f.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject() ||
      doc.object().value("version").toInt() != 1) {
    m_storageValid = false;
    emit feedback(
        "Saved server queue could not be read. It has been left untouched.",
        true);
    return;
  }
  auto rows = doc.object().value("queue").toArray().toVariantList();
  m_player.setExternalTracks(tracks(rows), doc.object().value("index").toInt(),
                             false);
}
