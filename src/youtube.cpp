#include "youtube.h"
#include <QBuffer>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <signal.h>
#include <unistd.h>

static QString keyFor(const QString &id) {
  return "https://music.youtube.com/watch?v=" + id;
}
static bool videoId(const QString &id) {
  static const QRegularExpression pattern("^[A-Za-z0-9_-]{11}$");
  return pattern.match(id).hasMatch();
}
static bool artUrl(const QUrl &url) {
  const auto host = url.host().toLower();
  return url.scheme() == "https" &&
         (host == "i.ytimg.com" || host.endsWith(".googleusercontent.com") ||
          host.endsWith(".ggpht.com"));
}
QVariantMap Youtube::cleanItem(const QVariantMap &row) {
  const auto id = row.value("id", row.value("videoId")).toString();
  const auto kind = row.value("kind", "song").toString();
  if (id.isEmpty() || id.size() > 256 ||
      !QRegularExpression("^[A-Za-z0-9_-]+$").match(id).hasMatch())
    return {};
  if (!QStringList{"song", "video", "album", "artist", "playlist"}.contains(
          kind))
    return {};
  if ((kind == "song" || kind == "video") && !videoId(id))
    return {};
  QVariantMap result{
      {"id", id},
      {"kind", kind},
      {"title", row.value("title", "Untitled").toString().left(512)},
      {"artist", row.value("artist").toString().left(512)},
      {"album", row.value("album").toString().left(512)},
      {"albumId", row.value("albumId").toString().left(256)},
      {"artistId", row.value("artistId").toString().left(256)},
      {"available", row.value("available", true).toBool()}};
  if (artUrl(QUrl(row.value("art").toString())))
    result["art"] = row.value("art").toString().left(2048);
  qint64 seconds = row.value("seconds").toLongLong();
  if (seconds <= 0) {
    const auto parts = row.value("duration").toString().split(':');
    for (const auto &part : parts)
      seconds = seconds * 60 + part.toInt();
  }
  result["seconds"] = qBound(qint64(0), seconds, qint64(86400));
  return result;
}
Youtube::Youtube(const QString &directory, QObject *parent)
    : QObject(parent), m_player(directory + "/transport.ini", nullptr, true),
      m_directory(directory), m_path(directory + "/library.json") {
  QDir().mkpath(directory);
  m_save.setSingleShot(true);
  m_save.setInterval(250);
  connect(&m_save, &QTimer::timeout, this, &Youtube::persist);
  QFile file(m_path);
  if (file.exists()) {
    if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024)
      m_storageValid = false;
    else {
      QJsonParseError error;
      auto doc = QJsonDocument::fromJson(file.readAll(), &error);
      m_storageValid = error.error == QJsonParseError::NoError &&
                       doc.isObject() &&
                       doc.object().value("version").toInt() == 1;
      if (m_storageValid) {
        const auto root = doc.object();
        const auto clean = [](const QJsonArray &array) {
          QVariantList rows;
          for (const auto &v : array) {
            auto row = Youtube::cleanItem(v.toObject().toVariantMap());
            if (!row.isEmpty() && rows.size() < 5000)
              rows.append(row);
          }
          return rows;
        };
        m_favorites = clean(root.value("favorites").toArray());
        m_history = clean(root.value("history").toArray());
        for (const auto &v : root.value("playlists").toArray()) {
          auto p = v.toObject();
          const auto id = p.value("id").toString();
          if (QUuid(id).isNull() || m_playlists.size() >= 100)
            continue;
          m_playlists.append(
              QVariantMap{{"id", id},
                          {"name", p.value("name").toString().left(100)},
                          {"items", clean(p.value("items").toArray())}});
        }
        auto rows = clean(root.value("queue").toArray());
        m_player.setExternalTracks(tracks(rows), root.value("index").toInt(),
                                   false);
      }
    }
    if (!m_storageValid)
      m_error =
          "Your YouTube library could not be read. It has been left untouched.";
  }
  connect(&m_player, &Player::externalRequested, this,
          [this] { loadCurrent(); });
  connect(&m_player, &Player::externalCancelled, this,
          [this] { cancel("play"); });
  connect(&m_player, &Player::trackChanged, this, [this] {
    m_recordedKey.clear();
    m_audio.reset();
    cancel("prepare");
    m_save.start();
    loadArt();
    refresh();
    emit changed();
  });
  connect(&m_player, &Player::queueChanged, this, [this] {
    m_save.start();
    cancel("prepare");
    m_prepared.reset();
    m_preparedKey.clear();
  });
  connect(&m_player, &Player::positionChanged, this, [this] {
    updateLyricIndex();
    if (m_player.playing() && m_player.position() > 1000 &&
        m_recordedKey != m_player.trackKey()) {
      auto row = current();
      if (row.isEmpty())
        return;
      m_recordedKey = m_player.trackKey();
      for (int i = m_history.size() - 1; i >= 0; --i)
        if (m_history[i].toMap().value("id") == row.value("id"))
          m_history.removeAt(i);
      m_history.prepend(row);
      while (m_history.size() > 200)
        m_history.removeLast();
      m_save.start();
      if (m_page == "history")
        localPage();
      prepareNext();
    }
  });
}
Youtube::~Youtube() {
  m_save.stop();
  persist();
  for (const auto &key : m_jobs.keys())
    cancel(key);
  m_player.stop();
}
void Youtube::fail(const QString &message) {
  m_error = message;
  emit changed();
  emit feedback(message, true);
}
void Youtube::cancel(const QString &channel) {
  const auto p = m_jobs.take(channel);
  if (!p)
    return;
  p->disconnect(this);
  if (p->processId() > 0)
    ::kill(-p->processId(), SIGKILL);
  if (p->state() == QProcess::Starting)
    connect(p, &QProcess::started, p, [p] {
      if (p->processId() > 0)
        ::kill(-p->processId(), SIGKILL);
      p->kill();
    });
  p->kill();
  connect(p, &QProcess::finished, p, &QObject::deleteLater);
  if (p->state() == QProcess::NotRunning)
    p->deleteLater();
}
void Youtube::request(const QString &channel, const QVariantMap &args,
                      Callback done, std::shared_ptr<QTemporaryDir> directory) {
  cancel(channel);
  auto *p = new QProcess(this);
  m_jobs.insert(channel, p);
  p->setChildProcessModifier([] { ::setsid(); });
  if (directory)
    connect(p, &QObject::destroyed, [directory] {});
  auto *timer = new QTimer(p);
  timer->setSingleShot(true);
  timer->setInterval(channel == "play" || channel == "prepare" ? 90000 : 45000);
  auto output = std::make_shared<QByteArray>();
  connect(p, &QProcess::readyReadStandardOutput, this,
          [this, p, channel, output] {
            output->append(p->readAllStandardOutput());
            if (output->size() > 8 * 1024 * 1024) {
              p->kill();
            }
          });
  connect(p, &QProcess::readyReadStandardError, this,
          [p] { p->readAllStandardError(); });
  connect(timer, &QTimer::timeout, this, [this, channel, done] {
    cancel(channel);
    done({{"ok", false},
          {"error", "YouTube timed out. Check your connection and retry."}});
  });
  connect(p, &QProcess::errorOccurred, this,
          [this, p, channel, done](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart)
              return;
            m_jobs.remove(channel);
            done({{"ok", false},
                  {"error", "YouTube support is not installed. Run "
                            "scripts/setup-youtube.sh in the Spun folder."}});
            p->deleteLater();
          });
  connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this, p, channel, done, output](int, QProcess::ExitStatus) {
            if (m_jobs.value(channel) != p)
              return;
            m_jobs.remove(channel);
            output->append(p->readAllStandardOutput());
            auto data =
                QJsonDocument::fromJson(*output).object().toVariantMap();
            if (data.isEmpty() || output->size() > 8 * 1024 * 1024)
              data = {{"ok", false},
                      {"error", "YouTube returned an unreadable response. "
                                "Retry or update YouTube support."}};
            p->deleteLater();
            done(data);
          });
  QString python = qEnvironmentVariable("SPUN_YOUTUBE_PYTHON");
  if (python.isEmpty())
    python = QStringLiteral(SPUN_SOURCE_DIR) + "/runtime/youtube/bin/python";
  QString helper = qEnvironmentVariable("SPUN_YOUTUBE_HELPER",
                                        QStringLiteral(SPUN_SOURCE_DIR) +
                                            "/helper/youtube.py");
  p->start(python, {helper});
  p->write(QJsonDocument::fromVariant(args).toJson(QJsonDocument::Compact));
  p->closeWriteChannel();
  timer->start();
}
void Youtube::setEnabled(bool enabled) {
  if (m_enabled == enabled)
    return;
  m_enabled = enabled;
  if (!enabled) {
    m_player.pause();
    cancel("prepare");
    m_prepared.reset();
    m_preparedKey.clear();
  } else {
    if (!m_checked)
      check();
    if (!m_player.trackKey().isEmpty() && m_player.artwork().isNull())
      loadArt();
  }
  emit changed();
}
void Youtube::check() {
  m_checked = true;
  m_busy = true;
  emit changed();
  request("browse", {{"op", "check"}}, [this](const auto &r) {
    m_busy = false;
    m_ready = r.value("ok").toBool();
    m_error = m_ready
                  ? QString()
                  : "YouTube support needs setup. Run scripts/setup-youtube.sh "
                    "in the Spun folder, then Retry.";
    emit changed();
  });
}
void Youtube::browse(const QVariantMap &requestData, const QString &title,
                     bool remember) {
  if (remember)
    m_back.append(QVariantMap{
        {"items", m_items}, {"heading", m_heading}, {"page", m_page}});
  while (m_back.size() > 20)
    m_back.removeFirst();
  m_page = "browse";
  m_heading = title;
  m_busy = true;
  m_error.clear();
  m_items.clear();
  emit changed();
  request("browse", requestData, [this](const auto &r) {
    m_busy = false;
    if (!r.value("ok").toBool()) {
      fail(r.value("error").toString());
      return;
    }
    m_ready = true;
    auto rows = r.value("items").toList();
    if (rows.isEmpty())
      for (const auto &s : r.value("sections").toList())
        rows.append(s.toMap().value("items").toList());
    for (const auto &v : rows) {
      auto row = cleanItem(v.toMap());
      if (!row.isEmpty() && m_items.size() < 5000)
        m_items.append(row);
    }
    if (!r.value("title").toString().isEmpty())
      m_heading = r.value("title").toString();
    emit changed();
  });
}
void Youtube::search(const QString &query, const QString &filter) {
  auto q = query.trimmed().left(2048);
  if (q.isEmpty())
    return;
  if (q.startsWith("https://")) {
    browse({{"op", "link"}, {"url", q}}, "YouTube link");
    return;
  }
  if (!QStringList{"songs", "albums", "artists", "playlists", "videos"}
           .contains(filter))
    return;
  browse({{"op", "search"}, {"query", q}, {"filter", filter}, {"limit", 50}},
         q);
}
void Youtube::show(const QString &page) {
  cancel("browse");
  m_busy = false;
  m_error.clear();
  m_back.clear();
  m_page = page;
  if (page == "home") {
    browse({{"op", "home"}}, "Discover", false);
    return;
  }
  if (page == "search") {
    m_heading = "Search YouTube Music";
    m_items.clear();
    emit changed();
    return;
  }
  localPage();
}
void Youtube::localPage() {
  if (m_page == "favorites") {
    m_heading = "Favorites · on this device";
    m_items = m_favorites;
  } else if (m_page == "history") {
    m_heading = "Recently played";
    m_items = m_history;
  } else if (m_page == "playlists") {
    m_heading = "Your playlists";
    m_items.clear();
    for (const auto &v : m_playlists) {
      auto p = v.toMap();
      m_items.append(QVariantMap{
          {"id", p["id"]},
          {"kind", "local-playlist"},
          {"title", p["name"]},
          {"artist", QString::number(p["items"].toList().size()) + " songs"}});
    }
  } else if (m_page.startsWith("playlist:")) {
    m_items.clear();
    for (const auto &v : m_playlists) {
      auto p = v.toMap();
      if (p["id"].toString() == m_page.mid(9)) {
        m_heading = p["name"].toString();
        m_items = p["items"].toList();
      }
    }
  }
  emit changed();
}
void Youtube::open(const QVariantMap &row) {
  auto kind = row.value("kind").toString();
  if (kind == "local-playlist") {
    m_back.append(QVariantMap{
        {"items", m_items}, {"heading", m_heading}, {"page", m_page}});
    m_page = "playlist:" + row.value("id").toString();
    localPage();
    return;
  }
  const auto item = cleanItem(row);
  if (item.isEmpty())
    return;
  if (kind == "song" || kind == "video") {
    playItem(item);
    return;
  }
  if (QStringList{"album", "artist", "playlist"}.contains(kind))
    browse({{"op", kind}, {"id", item["id"]}, {"limit", 5000}},
           item["title"].toString());
}
void Youtube::back() {
  if (m_back.isEmpty())
    return;
  cancel("browse");
  m_busy = false;
  m_error.clear();
  const auto state = m_back.takeLast().toMap();
  m_items = state["items"].toList();
  m_page = state["page"].toString();
  m_heading = state["heading"].toString();
  if (m_page == "favorites" || m_page == "history" || m_page == "playlists" ||
      m_page.startsWith("playlist:"))
    localPage();
  else
    emit changed();
}
QList<Track> Youtube::tracks(const QVariantList &rows) {
  QList<Track> result;
  for (const auto &v : rows) {
    auto r = cleanItem(v.toMap());
    if (r.isEmpty() || !videoId(r["id"].toString()) ||
        !r["available"].toBool() || result.size() >= 5000)
      continue;
    const auto key = keyFor(r["id"].toString());
    m_catalog[key] = r;
    Track t;
    t.path = key;
    t.title = r["title"].toString();
    t.artist = r["artist"].toString();
    t.album = r["album"].toString();
    t.albumArtist = r["albumId"].toString();
    t.cover = r["art"].toString();
    t.duration = r["seconds"].toLongLong() * 1000;
    t.number = result.size() + 1;
    result.append(t);
  }
  return result;
}
QVariantList Youtube::queueItems() const {
  QVariantList rows;
  for (const auto &v : m_player.queue())
    rows.append(m_catalog.value(v.toMap().value("path").toString()));
  return rows;
}
QVariantMap Youtube::current() const {
  return m_catalog.value(m_player.trackKey());
}
void Youtube::playItems(const QVariantList &rows, int index) {
  auto list = tracks(rows);
  if (list.isEmpty()) {
    fail("No playable songs in this selection.");
    return;
  }
  setEnabled(true);
  m_player.setExternalTracks(list, index, true);
}
void Youtube::playItem(const QVariantMap &row) { playItems({row}); }
void Youtube::enqueue(const QVariantMap &row) {
  auto list = tracks({row});
  if (list.isEmpty())
    return;
  if (m_player.count() >= 5000) {
    fail("The queue is full.");
    return;
  }
  m_player.appendExternalTracks(list);
  emit feedback("Added to YouTube queue", false);
}
void Youtube::radio(const QVariantMap &row) {
  const auto item = cleanItem(row);
  if (!videoId(item.value("id").toString()))
    return;
  browse({{"op", "radio"}, {"id", item["id"]}}, "Song radio");
}
void Youtube::loadCurrent() {
  if (!m_enabled) {
    m_player.pause();
    return;
  }
  const auto key = m_player.trackKey();
  const auto row = current();
  if (row.isEmpty())
    return;
  if (key == m_preparedKey && m_prepared) {
    auto directory = std::move(m_prepared);
    m_preparedKey.clear();
    const auto files = QDir(directory->path()).entryList(QDir::Files);
    if (!files.isEmpty()) {
      m_player.resolveExternal(
          key, QUrl::fromLocalFile(directory->path() + "/" + files.first()));
      m_audio = directory;
      return;
    }
  }
  auto directory = std::make_shared<QTemporaryDir>(QDir::tempPath() +
                                                   "/spun-youtube-XXXXXX");
  if (!directory->isValid()) {
    m_player.failExternal(key, "Could not create the YouTube playback buffer.");
    return;
  }
  request(
      "play",
      {{"op", "buffer"}, {"id", row["id"]}, {"directory", directory->path()}},
      [this, key, directory](const auto &r) {
        if (key != m_player.trackKey() || !m_enabled)
          return;
        const auto file =
            QFileInfo(r.value("file").toString()).canonicalFilePath();
        if (!r.value("ok").toBool() || file.isEmpty() ||
            !file.startsWith(directory->path() + "/")) {
          m_player.failExternal(
              key, "Could not play this song anonymously. It may be "
                   "unavailable, restricted, or YouTube may need a resolver "
                   "update. Try another song or retry.");
          return;
        }
        m_player.resolveExternal(key, QUrl::fromLocalFile(file));
        m_audio = directory;
      },
      directory);
}
void Youtube::prepareNext() {
  if (!m_enabled || m_player.shuffle() || m_player.repeatMode() == 2)
    return;
  const auto rows = queueItems();
  const auto next = m_player.currentIndex() + 1;
  if (next >= rows.size())
    return;
  const auto row = rows[next].toMap();
  const auto key = keyFor(row.value("id").toString());
  if (key == m_preparedKey)
    return;
  auto directory = std::make_shared<QTemporaryDir>(QDir::tempPath() +
                                                   "/spun-youtube-XXXXXX");
  if (!directory->isValid())
    return;
  const auto currentKey = m_player.trackKey();
  request(
      "prepare",
      {{"op", "buffer"}, {"id", row["id"]}, {"directory", directory->path()}},
      [this, key, currentKey, directory](const auto &r) {
        if (!m_enabled || m_player.trackKey() != currentKey ||
            !r.value("ok").toBool())
          return;
        auto file = QFileInfo(r.value("file").toString()).canonicalFilePath();
        if (!file.startsWith(directory->path() + "/"))
          return;
        m_preparedKey = key;
        m_prepared = directory;
      },
      directory);
}
void Youtube::loadArt() {
  if (m_artReply) {
    m_artReply->abort();
    m_artReply->deleteLater();
    m_artReply = nullptr;
  }
  const auto key = m_player.trackKey();
  const QUrl url(current().value("art").toString());
  if (!artUrl(url)) {
    m_player.setExternalArtwork(key, {});
    return;
  }
  QNetworkRequest request(url);
  request.setTransferTimeout(15000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::ManualRedirectPolicy);
  auto *reply = m_network.get(request);
  m_artReply = reply;
  reply->setReadBufferSize(8 * 1024 * 1024 + 1);
  auto data = std::make_shared<QByteArray>();
  connect(reply, &QNetworkReply::readyRead, this, [reply, data] {
    data->append(reply->readAll());
    if (data->size() > 8 * 1024 * 1024)
      reply->abort();
  });
  connect(reply, &QNetworkReply::finished, this, [this, key, reply, data] {
    data->append(reply->readAll());
    QImage image;
    if (reply->error() == QNetworkReply::NoError &&
        data->size() <= 8 * 1024 * 1024) {
      QBuffer buffer(data.get());
      buffer.open(QIODevice::ReadOnly);
      QImageReader reader(&buffer);
      if (reader.size().isValid() && reader.size().width() <= 8192 &&
          reader.size().height() <= 8192) {
        reader.setScaledSize(
            reader.size().scaled(1200, 1200, Qt::KeepAspectRatio));
        image = reader.read();
      }
    }
    if (m_artReply == reply) {
      m_artReply = nullptr;
      m_player.setExternalArtwork(key, image);
    }
    reply->deleteLater();
  });
}
bool Youtube::favorite(const QString &id) const {
  for (const auto &v : m_favorites)
    if (v.toMap().value("id").toString() == id)
      return true;
  return false;
}
void Youtube::toggleFavorite(const QVariantMap &item) {
  auto row = cleanItem(item);
  if (row.isEmpty())
    return;
  const auto id = row["id"].toString();
  for (int i = 0; i < m_favorites.size(); ++i)
    if (m_favorites[i].toMap().value("id").toString() == id) {
      m_favorites.removeAt(i);
      m_save.start();
      if (m_page == "favorites")
        localPage();
      else
        emit changed();
      return;
    }
  if (m_favorites.size() >= 5000) {
    fail("Your favorites list is full.");
    return;
  }
  m_favorites.prepend(row);
  m_save.start();
  if (m_page == "favorites")
    localPage();
  else
    emit changed();
}
QVariantList Youtube::playlists() const {
  QVariantList rows;
  for (const auto &v : m_playlists) {
    auto p = v.toMap();
    rows.append(QVariantMap{{"id", p["id"]}, {"name", p["name"]}});
  }
  return rows;
}
QString Youtube::createPlaylist(const QString &name) {
  if (name.trimmed().isEmpty() || m_playlists.size() >= 100)
    return {};
  const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  m_playlists.append(QVariantMap{{"id", id},
                                 {"name", name.trimmed().left(100)},
                                 {"items", QVariantList{}}});
  m_save.start();
  if (m_page == "playlists")
    localPage();
  else
    emit changed();
  return id;
}
void Youtube::renamePlaylist(const QString &id, const QString &name) {
  if (name.trimmed().isEmpty())
    return;
  for (auto &v : m_playlists) {
    auto p = v.toMap();
    if (p["id"].toString() == id) {
      p["name"] = name.trimmed().left(100);
      v = p;
    }
  }
  m_save.start();
  localPage();
}
void Youtube::deletePlaylist(const QString &id) {
  for (int i = 0; i < m_playlists.size(); ++i)
    if (m_playlists[i].toMap().value("id").toString() == id) {
      m_playlists.removeAt(i);
      break;
    }
  m_save.start();
  show("playlists");
}
void Youtube::addToPlaylist(const QString &id, const QVariantMap &item) {
  auto row = cleanItem(item);
  if (row.isEmpty() || !videoId(row["id"].toString()))
    return;
  for (auto &v : m_playlists) {
    auto p = v.toMap();
    if (p["id"].toString() != id)
      continue;
    auto rows = p["items"].toList();
    if (rows.size() >= 5000) {
      fail("This playlist is full.");
      return;
    }
    rows.append(row);
    p["items"] = rows;
    v = p;
    m_save.start();
    localPage();
    emit feedback("Added to " + p["name"].toString(), false);
    return;
  }
}
void Youtube::removeFromPlaylist(const QString &id, int index) {
  for (auto &v : m_playlists) {
    auto p = v.toMap();
    if (p["id"].toString() != id)
      continue;
    auto rows = p["items"].toList();
    if (index < 0 || index >= rows.size())
      return;
    rows.removeAt(index);
    p["items"] = rows;
    v = p;
  }
  m_save.start();
  localPage();
}
void Youtube::clearHistory() {
  m_history.clear();
  m_save.start();
  if (m_page == "history")
    localPage();
}
void Youtube::copyLink(const QVariantMap &item) {
  const auto r = cleanItem(item);
  if (r.isEmpty())
    return;
  const auto kind = r["kind"].toString();
  const auto link =
      (kind == "song" || kind == "video")
          ? keyFor(r["id"].toString())
          : QStringLiteral("https://music.youtube.com/") +
                (kind == "playlist" ? "playlist?list=" : "browse/") +
                r["id"].toString();
  QGuiApplication::clipboard()->setText(link);
  emit feedback("Link copied", false);
}
void Youtube::openClipboardLink() {
  search(QGuiApplication::clipboard()->text());
}
void Youtube::persist() {
  QSet<QString> keys;
  for (const auto &v : m_player.queue())
    keys.insert(v.toMap().value("path").toString());
  for (auto it = m_catalog.begin(); it != m_catalog.end();) {
    if (!keys.contains(it.key()))
      it = m_catalog.erase(it);
    else
      ++it;
  }
  if (!m_storageValid)
    return;
  QSaveFile file(m_path);
  auto data = QJsonDocument::fromVariant(
                  QVariantMap{{"version", 1},
                              {"favorites", m_favorites},
                              {"history", m_history},
                              {"playlists", m_playlists},
                              {"queue", queueItems()},
                              {"index", m_player.currentIndex()}})
                  .toJson(QJsonDocument::Compact);
  if (data.size() > 8 * 1024 * 1024) {
    fail("The local YouTube library is too large to save.");
    return;
  }
  if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() ||
      !file.commit())
    fail("Could not save your local YouTube library.");
}
void Youtube::setActive(bool active) {
  if (m_lyricsActive == active)
    return;
  m_lyricsActive = active;
  if (active)
    refresh();
  else {
    cancel("lyrics");
    m_lyricsLoading = false;
  }
  emit changed();
}
void Youtube::refresh() {
  cancel("lyrics");
  m_lines.clear();
  m_timed = false;
  m_timeline.reset({});
  m_lyricsMessage = "No lyrics available";
  m_lyricsLoading = false;
  updateLyricIndex();
  if (!m_lyricsActive || current().isEmpty()) {
    emit changed();
    return;
  }
  m_lyricsLoading = true;
  emit changed();
  const auto key = m_player.trackKey();
  request("lyrics", {{"op", "lyrics"}, {"id", current()["id"]}},
          [this, key](const auto &r) {
            if (key != m_player.trackKey() || !m_lyricsActive)
              return;
            m_lyricsLoading = false;
            if (!r.value("ok").toBool())
              m_lyricsMessage = "Could not load lyrics. Try again.";
            else {
              m_lines = r.value("lines").toList();
              m_timed = !m_lines.isEmpty();
              if (m_lines.isEmpty())
                m_lines = Lyrics::parse(r.value("lyrics").toString());
            }
            m_timeline.reset(m_lines);
            updateLyricIndex();
            emit changed();
          });
}
void Youtube::updateLyricIndex() {
  const int index = m_timed ? m_timeline.indexAt(m_player.position()) : -1;
  if (index != m_lyricIndex) {
    m_lyricIndex = index;
    emit currentIndexChanged();
  }
}
bool Youtube::seekToLine(int index) {
  if (!m_timed || index < 0 || index >= m_lines.size())
    return false;
  const auto pos = m_lines[index].toMap().value("start").toLongLong();
  if (pos < 0 || pos >= m_player.duration())
    return false;
  m_player.seek(pos);
  return true;
}
