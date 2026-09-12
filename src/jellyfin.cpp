#include "jellyfin.h"
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

Jellyfin::Jellyfin(const QString &directory, bool restore, QObject *parent)
    : QObject(parent), m_api(restore, directory + "/connection.ini"),
      m_player(directory + "/player.ini", this, true), m_directory(directory) {
  m_player.setExternalName("Jellyfin");
  m_save.setSingleShot(true);
  m_save.setInterval(200);
  connect(&m_save, &QTimer::timeout, this, &Jellyfin::persist);
  connect(&m_api, &JellyfinApi::message, this, [this](const QString &s) {
    emit feedback(s, !m_api.error().isEmpty());
  });
  connect(&m_api, &JellyfinApi::accountChanged, this, [this] {
    persist();
    ++m_generation;
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
  connect(&m_api, &JellyfinApi::changed, this, [this] {
    if (m_api.connected() && m_identity != m_api.identity()) {
      m_identity = m_api.identity();
      restoreQueue();
      if (m_enabled)
        show("albums");
    }
    emit changed();
  });
  connect(&m_player, &Player::externalRequested, this, &Jellyfin::loadCurrent);
  connect(&m_player, &Player::externalCancelled, this,
          [this] { m_api.cancel("audio"); });
  connect(&m_player, &Player::trackChanged, this, [this] {
    report(true);
    m_audio.reset();
    m_api.cancel("audio");
    loadArt();
    refresh();
    m_save.start();
    emit changed();
  });
  connect(&m_player, &Player::queueChanged, this, [this] { m_save.start(); });
  connect(&m_player, &Player::playingChanged, this, [this] {
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
      emit feedback(m_player.error(), true);
    }
  });
  m_reportTimer.setInterval(10000);
  connect(&m_reportTimer, &QTimer::timeout, this, [this] { report(); });
  m_reportTimer.start();
}
Jellyfin::~Jellyfin() {
  persist();
  report(true);
  m_player.stop();
}
void Jellyfin::setEnabled(bool value) {
  if (m_enabled == value)
    return;
  m_enabled = value;
  if (!value) {
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
QVariantMap Jellyfin::original(const QVariantMap &row) const {
  return m_catalog.value(row.value("id").toString(), row);
}
QVariantMap Jellyfin::current() const {
  return m_catalog.value(m_player.currentUrl().path().mid(1));
}
void Jellyfin::show(const QString &mode) {
  m_back.clear();
  m_collection.clear();
  browse({{"mode", mode}});
}
void Jellyfin::search(const QString &query, const QString &filter) {
  m_back.clear();
  m_collection.clear();
  browse({{"mode", "search"},
          {"query", query.trimmed().left(512)},
          {"filter", filter}});
}
void Jellyfin::open(const QVariantMap &row) {
  if (!m_api.owns(row))
    return;
  if (row.value("kind") == "song") {
    playItem(row);
    return;
  }
  m_back.append(
      QVariantMap{{"request", m_request}, {"collection", m_collection}});
  if (m_back.size() > 30)
    m_back.removeFirst();
  m_collection = row;
  browse({{"mode", row.value("kind")}, {"remoteId", row.value("remoteId")}});
}
void Jellyfin::back() {
  if (m_back.isEmpty())
    return;
  auto state = m_back.takeLast().toMap();
  m_collection = state.value("collection").toMap();
  browse(state.value("request").toMap());
}
void Jellyfin::reload() { browse(m_request); }
void Jellyfin::loadMore() {
  if (m_more && !m_busy)
    browse(m_request, true);
}
void Jellyfin::browse(const QVariantMap &request, bool append) {
  if (!m_api.connected())
    return;
  ++m_generation;
  const auto generation = m_generation;
  m_api.cancel("thumb");
  m_thumbBusy = false;
  m_thumbPending.clear();
  m_thumbActive.clear();
  m_request = request;
  m_busy = true;
  m_error.clear();
  if (!append)
    m_items.clear();
  auto req = request;
  req["offset"] = append ? m_items.size() : 0;
  const auto mode = page();
  m_heading = m_collection.value("title").toString();
  if (m_heading.isEmpty())
    m_heading = mode == "search" ? request.value("query").toString()
                                 : mode.left(1).toUpper() + mode.mid(1);
  emit changed();
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
  });
}
QList<Track> Jellyfin::tracks(const QVariantList &rows) {
  QList<Track> out;
  for (const auto &v : rows) {
    auto row = original(v.toMap());
    if (!m_api.owns(row) || row.value("kind") != "song")
      continue;
    const auto id = row.value("id").toString();
    m_catalog[id] = row;
    Track t;
    t.path = "jellyfin://" + m_api.identity() + "/" + id;
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
void Jellyfin::playItems(const QVariantList &rows, int index) {
  auto queue = tracks(rows);
  if (!queue.isEmpty())
    m_player.setExternalTracks(queue, qBound(0, index, int(queue.size() - 1)),
                               true);
}
void Jellyfin::playItem(const QVariantMap &row) { playItems({row}); }
void Jellyfin::enqueue(const QVariantMap &row) {
  auto queue = tracks({row});
  if (!queue.isEmpty()) {
    m_player.appendExternalTracks(queue);
    emit feedback("Added to queue", false);
  }
}
void Jellyfin::loadCurrent() {
  const auto row = current();
  const auto key = m_player.trackKey();
  if (!m_api.owns(row)) {
    m_player.failExternal(key, "Connect to this song's Jellyfin server first.");
    return;
  }
  auto buffer = std::make_shared<QTemporaryDir>();
  const auto path = buffer->filePath("audio");
  m_api.download(
      row, path,
      [this, buffer, key](const QVariantMap &, const QString &error) {
        if (key != m_player.trackKey())
          return;
        if (!error.isEmpty()) {
          m_player.failExternal(key, error);
          return;
        }
        m_audio = buffer;
        m_player.resolveExternal(
            key, QUrl::fromLocalFile(buffer->filePath("audio")));
      });
}
void Jellyfin::loadArt() {
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
QString Jellyfin::artwork(const QString &id) {
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
    QTimer::singleShot(0, this, &Jellyfin::loadThumbs);
  }
  return {};
}
void Jellyfin::loadThumbs() {
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
          QTimer::singleShot(0, this, &Jellyfin::loadThumbs);
        },
        "thumb");
    return;
  }
}
void Jellyfin::finishAction(const QString &error, bool refreshPage) {
  m_actionBusy = false;
  if (!error.isEmpty())
    emit feedback(error, true);
  else if (refreshPage)
    reload();
  emit changed();
}
void Jellyfin::toggleFavorite(const QVariantMap &row) {
  if (m_actionBusy)
    return;
  m_actionBusy = true;
  emit changed();
  m_api.star(original(row), !favorite(row.value("id").toString()),
             [this](const QVariantMap &, const QString &e) {
               finishAction(e, page() == "favorites");
             });
}
void Jellyfin::createPlaylist(const QString &name) {
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
void Jellyfin::renamePlaylist(const QString &id, const QString &name) {
  if (m_actionBusy || name.trimmed().isEmpty())
    return;
  m_actionBusy = true;
  emit changed();
  m_api.editPlaylist(
      id, {{"name", name.trimmed().left(120)}},
      [this](const QVariantMap &, const QString &e) { finishAction(e, true); });
}
void Jellyfin::addToPlaylist(const QString &id, const QVariantMap &row) {
  if (m_actionBusy || !m_api.owns(row))
    return;
  m_actionBusy = true;
  emit changed();
  m_api.editPlaylist(id, {{"songIdToAdd", row.value("remoteId").toString()}},
                     [this](const QVariantMap &, const QString &e) {
                       finishAction(e, page() == "playlist");
                     });
}
void Jellyfin::removeFromPlaylist(const QVariantMap &row) {
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
void Jellyfin::movePlaylistItem(int from, int to) {
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
void Jellyfin::deletePlaylist(const QString &id) {
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
void Jellyfin::copyLink(const QVariantMap &row) {
  if (!m_api.owns(row))
    return;
  QUrl u(m_api.address() + "/web/index.html");
  u.setFragment("/details?id=" + QString::fromLatin1(QUrl::toPercentEncoding(
                                     row.value("remoteId").toString())));
  QGuiApplication::clipboard()->setText(u.toString());
  emit feedback("Link copied", false);
}
void Jellyfin::setActive(bool active) {
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
void Jellyfin::refresh() {
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
void Jellyfin::updateLyrics() {
  const auto i = m_timeline.indexAt(m_player.position());
  if (i != m_lyricIndex) {
    m_lyricIndex = i;
    emit currentIndexChanged();
  }
}
bool Jellyfin::seekToLine(int index) {
  if (!m_timed || index < 0 || index >= m_lines.size())
    return false;
  m_player.seek(m_lines[index].toMap().value("start").toLongLong());
  return true;
}
void Jellyfin::report(bool stopped) {
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
void Jellyfin::persist() {
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
void Jellyfin::restoreQueue() {
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
        "Saved Jellyfin queue could not be read. It has been left untouched.",
        true);
    return;
  }
  auto rows = doc.object().value("queue").toArray().toVariantList();
  m_player.setExternalTracks(tracks(rows), doc.object().value("index").toInt(),
                             false);
}
