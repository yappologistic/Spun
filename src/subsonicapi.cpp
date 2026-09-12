#include "subsonicapi.h"
#include "jellyfinlrc.h"
#include <QBuffer>
#include <QDateTime>
static QString playlistRevision(const QVariantList &);
#include <QCryptographicHash>
#include <QFile>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <memory>

static QString hash(const QString &text) {
  return QString::fromLatin1(
      QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256)
          .toHex());
}
static QString apiError(const QVariantMap &data) {
  switch (data.value("error").toMap().value("code").toInt()) {
  case 40:
    return "Incorrect server username or password.";
  case 41:
    return "This account does not support token authentication.";
  case 50:
    return "Your server account does not allow this action.";
  case 60:
    return "This Subsonic server requires an active Premium license.";
  case 70:
    return "This item no longer exists on the server.";
  case 20:
  case 30:
    return "The server API version is incompatible.";
  default:
    return "The server could not complete this action.";
  }
}
SubsonicApi::SubsonicApi(bool restore, const QString &settingsPath)
    : m_settings(settingsPath.isEmpty() ? QSettings().fileName() : settingsPath,
                 QSettings::IniFormat) {
  if (!restore)
    return;
  m_address = m_settings.value("subsonic/address").toString();
  m_username = m_settings.value("subsonic/username").toString();
  if (!m_address.isEmpty() && !m_username.isEmpty()) {
    m_identity = hash("subsonic\n" + m_address + "\n" + m_username);
    if (m_settings.value("subsonic/remember", false).toBool())
      QTimer::singleShot(0, this, [this] {
        const auto generation = m_generation;
        secret({"lookup", "application", "spun", "account", m_identity}, {},
               [this, generation](bool ok, QByteArray password) {
                 if (generation != m_generation)
                   return;
                 if (ok && !password.trimmed().isEmpty()) {
                   if (password.endsWith('\n'))
                     password.chop(1);
                   m_restoring = true;
                   connectServer(m_address, m_username,
                                 QString::fromUtf8(password), false);
                   m_restoring = false;
                 } else {
                   m_error = "Sign in to reconnect to your music server.";
                   emit changed();
                 }
               });
      });
  }
}
SubsonicApi::~SubsonicApi() { stopRequests(); }
bool SubsonicApi::keyringAvailable() const {
  return !QStandardPaths::findExecutable("secret-tool").isEmpty();
}
void SubsonicApi::secret(const QStringList &args, const QByteArray &input,
                         std::function<void(bool, QByteArray)> callback) {
  if (!keyringAvailable()) {
    callback(false, {});
    return;
  }
  auto p = new QProcess(this);
  auto timer = new QTimer(p);
  timer->setSingleShot(true);
  auto done = std::make_shared<bool>(false);
  auto finish = [p, done, callback](bool ok) {
    if (*done)
      return;
    *done = true;
    callback(ok, p->readAllStandardOutput());
    p->deleteLater();
  };
  connect(p, &QProcess::errorOccurred, this,
          [finish](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart)
              finish(false);
          });
  connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [finish](int code, QProcess::ExitStatus s) {
            finish(code == 0 && s == QProcess::NormalExit);
          });
  connect(timer, &QTimer::timeout, p, [p] { p->kill(); });
  p->start("secret-tool", args);
  p->write(input);
  p->closeWriteChannel();
  timer->start(15000);
}
void SubsonicApi::stopRequests() {
  ++m_generation;
  for (auto r : m_network.findChildren<QNetworkReply *>()) {
    r->disconnect(this);
    r->abort();
    r->deleteLater();
  }
  m_channels.clear();
}
void SubsonicApi::cancel(const QString &channel) {
  auto r = m_channels.take(channel);
  if (r) {
    r->disconnect(this);
    r->abort();
    r->deleteLater();
  }
}
void SubsonicApi::connectServer(const QString &address, const QString &user,
                                const QString &password, bool remember) {
  QUrl base(address.trimmed());
  if (!base.isValid() ||
      !QStringList{"http", "https"}.contains(base.scheme()) ||
      base.host().isEmpty() || !base.userInfo().isEmpty() || base.hasQuery() ||
      base.hasFragment() || user.trimmed().isEmpty() || password.isEmpty()) {
    m_error = "Enter an HTTP(S) server address, username and password. Use the "
              "server root, without /rest.";
    emit changed();
    return;
  }
  QString path = base.path();
  while (path.endsWith('/'))
    path.chop(1);
  if (path.endsWith("/rest")) {
    m_error = "Use the server address without /rest.";
    emit changed();
    return;
  }
  base.setPath(path);
  const bool restoring = m_restoring;
  const auto oldIdentity = m_identity;
  stopRequests();
  m_connected = false;
  m_connecting = true;
  m_error.clear();
  m_stars.clear();
  m_playlists.clear();
  m_folders.clear();
  m_address = base.toString();
  m_username = user.trimmed();
  m_password = password;
  m_identity = hash("subsonic\n" + m_address + "\n" + m_username);
  m_lyricsExtension = false;
  m_formPost = false;
  emit accountChanged();
  emit changed();
  call(
      "ping", {},
      [this, remember, restoring, oldIdentity](const QVariantMap &info,
                                               const QString &error) {
        m_connecting = false;
        m_error = error;
        m_connected = error.isEmpty();
        if (m_connected) {
          m_serviceName = info.value("type").toString().compare(
                              "navidrome", Qt::CaseInsensitive) == 0
                              ? "Navidrome"
                              : "Subsonic";
          if (m_settings.value("subsonic/remember", false).toBool() &&
              !oldIdentity.isEmpty() && oldIdentity != m_identity)
            secret({"clear", "application", "spun", "account", oldIdentity}, {},
                   [](bool, QByteArray) {});
          const bool same = m_settings.value("subsonic/address") == m_address &&
                            m_settings.value("subsonic/username") == m_username;
          if (!same)
            m_settings.remove("subsonic/folder");
          m_settings.setValue("subsonic/address", m_address);
          m_settings.setValue("subsonic/username", m_username);
          if (!remember && !restoring &&
              m_settings.value("subsonic/remember", false).toBool()) {
            m_settings.setValue("subsonic/remember", false);
            secret({"clear", "application", "spun", "account", m_identity}, {},
                   [](bool, QByteArray) {});
          }
          if (remember) {
            const auto generation = m_generation;
            secret({"store", "--label=Spun Subsonic account", "application",
                    "spun", "account", m_identity},
                   m_password.toUtf8(),
                   [this, generation](bool ok, QByteArray) {
                     if (generation != m_generation)
                       return;
                     m_settings.setValue("subsonic/remember", ok);
                     if (!ok)
                       emit message("Connected for this session. The desktop "
                                    "keyring could not save the password.");
                   });
          }
          discover();
        } else
          m_password.clear();
        emit changed();
      },
      "login");
}
void SubsonicApi::disconnectServer() {
  const auto old = m_identity;
  const bool remembered = m_settings.value("subsonic/remember", false).toBool();
  stopRequests();
  m_password.clear();
  m_connected = false;
  m_connecting = false;
  m_error.clear();
  m_stars.clear();
  m_playlists.clear();
  m_folders.clear();
  m_settings.remove("subsonic/remember");
  m_settings.remove("subsonic/address");
  m_settings.remove("subsonic/username");
  m_settings.remove("subsonic/folder");
  if (remembered && !old.isEmpty())
    secret({"clear", "application", "spun", "account", old}, {},
           [](bool, QByteArray) {});
  m_address.clear();
  m_username.clear();
  m_identity.clear();
  emit accountChanged();
  emit changed();
}
QUrl SubsonicApi::url(const QString &method, const Params &params) const {
  if (m_password.isEmpty())
    return {};
  QUrl result(m_address);
  result.setPath(result.path() + "/rest/" + method + ".view");
  const auto salt = QUuid::createUuid().toString(QUuid::Id128);
  Params all{{"u", m_username},
             {"t", QString::fromLatin1(
                       QCryptographicHash::hash((m_password + salt).toUtf8(),
                                                QCryptographicHash::Md5)
                           .toHex())},
             {"s", salt},
             {"v", "1.16.1"},
             {"c", "Spun"},
             {"f", "json"}};
  all.append(params);
  QStringList encoded;
  for (const auto &p : all)
    encoded.append(QString::fromLatin1(QUrl::toPercentEncoding(p.first)) + "=" +
                   QString::fromLatin1(QUrl::toPercentEncoding(p.second)));
  result.setQuery(encoded.join('&'), QUrl::StrictMode);
  return result;
}
void SubsonicApi::call(const QString &method, const Params &params,
                       Reply callback, const QString &channel) {
  if (!channel.isEmpty())
    cancel(channel);
  const auto target = url(method, params);
  if (target.isEmpty()) {
    QTimer::singleShot(0, this, [callback] {
      callback({}, "Connect to your music server in Settings.");
    });
    return;
  }
  const auto body = target.query(QUrl::FullyEncoded).toUtf8();
  if (body.size() > 2 * 1024 * 1024 || (!m_formPost && body.size() > 7500)) {
    QTimer::singleShot(0, this, [callback] {
      callback({},
               "This request is too large for the server. Select fewer songs.");
    });
    return;
  }
  QUrl endpoint = target;
  if (m_formPost)
    endpoint.setQuery(QString());
  QNetworkRequest request(endpoint);
  request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
  request.setTransferTimeout(20000);
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    "application/x-www-form-urlencoded");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::ManualRedirectPolicy);
  auto r = m_formPost ? m_network.post(request, body) : m_network.get(request);
  if (!channel.isEmpty())
    m_channels[channel] = r;
  const auto generation = m_generation;
  connect(r, &QNetworkReply::readyRead, this, [r] {
    if (r->bytesAvailable() > 16 * 1024 * 1024)
      r->abort();
  });
  auto timer = new QTimer(r);
  timer->setSingleShot(true);
  connect(timer, &QTimer::timeout, r, &QNetworkReply::abort);
  timer->start(30000);
  connect(r, &QNetworkReply::finished, this,
          [this, r, generation, channel, callback] {
            if (!channel.isEmpty() && m_channels.value(channel) == r)
              m_channels.remove(channel);
            r->deleteLater();
            if (generation != m_generation)
              return;
            const int status =
                r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status >= 300 && status < 400) {
              callback({}, "The server redirected the request. Enter its final "
                           "address in Settings.");
              return;
            }
            if (r->error() != QNetworkReply::NoError) {
              callback({}, status == 401 || status == 403
                               ? "The server refused access. Check your "
                                 "account and server address."
                               : "Could not reach the music server. Check the "
                                 "connection and retry.");
              return;
            }
            auto bytes = r->readAll();
            if (bytes.size() > 16 * 1024 * 1024) {
              callback({}, "The server response exceeds the 16 MiB limit.");
              return;
            }
            const auto data = QJsonDocument::fromJson(bytes)
                                  .object()
                                  .value("subsonic-response")
                                  .toObject()
                                  .toVariantMap();
            if (data.value("status") != "ok") {
              callback(
                  data,
                  data.isEmpty()
                      ? "This address did not return a Subsonic API response."
                      : apiError(data));
              return;
            }
            callback(data, {});
          });
}
void SubsonicApi::discover() {
  call("getOpenSubsonicExtensions", {},
       [this](const QVariantMap &d, const QString &) {
         for (const auto &v : d.value("openSubsonicExtensions").toList()) {
           if (v.toMap().value("name") == "songLyrics")
             m_lyricsExtension = true;
           if (v.toMap().value("name") == "formPost")
             m_formPost = true;
         }
       });
  call("getMusicFolders", {}, [this](const QVariantMap &d, const QString &) {
    m_folders = d.value("musicFolders").toMap().value("musicFolder").toList();
    emit changed();
  });
  call("getStarred2", {}, [this](const QVariantMap &d, const QString &e) {
    if (!e.isEmpty())
      return;
    for (const auto &kind :
         {QString("song"), QString("album"), QString("artist")})
      for (const auto &v : items(d.value("starred2").toMap().value(kind), kind))
        m_stars.insert(v.toMap().value("id").toString());
    emit changed();
  });
  reloadPlaylists();
}
void SubsonicApi::setFolder(const QString &id) {
  m_settings.setValue("subsonic/folder", id);
  emit changed();
}
void SubsonicApi::setScrobbling(bool value) {
  m_settings.setValue("subsonic/scrobble", value);
  if (!value) {
    m_playing.clear();
    m_lastReport = 0;
    m_listened = 0;
    m_wasPlaying = false;
  }
  emit changed();
}
void SubsonicApi::setBitrate(int value) {
  if (value != 0 && value != 128 && value != 192 && value != 320)
    return;
  m_settings.setValue("subsonic/bitrate", value);
  emit changed();
}
bool SubsonicApi::owns(const QVariantMap &track) const {
  return connected() && track.value("source") == "subsonic" &&
         track.value("server") == m_identity &&
         !track.value("remoteId").toString().isEmpty();
}
QVariantMap SubsonicApi::item(const QVariantMap &raw,
                              const QString &kind) const {
  const auto remote = raw.value("id").toString();
  if (remote.isEmpty())
    return {};
  QVariantMap out{
      {"id", "sub_" + hash(m_identity + "\n" + kind + "\n" + remote)},
      {"source", "subsonic"},
      {"server", m_identity},
      {"remoteId", remote},
      {"kind", kind},
      {"title", raw.value("title", raw.value("name"))},
      {"artist", raw.value("artist", raw.value("owner"))},
      {"album", raw.value("album")},
      {"seconds", raw.value("duration", 0)},
      {"discNumber", raw.value("discNumber", 1)},
      {"year", raw.value("year")},
      {"trackNumber", raw.value("track")},
      {"count", raw.value("songCount", raw.value("albumCount", 0))},
      {"rating", raw.value("userRating", 0)},
      {"available", true}};
  if (kind == "song") {
    out["serverSong"] = true;
    out["artistId"] = raw.value("artistId");
    out["albumId"] = raw.value("albumId");
  }
  if (raw.contains("coverArt")) {
    QUrl cover;
    cover.setScheme("spunsubcover");
    cover.setHost("account");
    cover.setPath("/" + m_identity + "/" + raw.value("coverArt").toString());
    out["art"] = cover.toString();
  }
  if (kind == "playlist")
    out["editable"] = raw.value("owner").toString() == m_username;
  return out;
}
QVariantList SubsonicApi::items(const QVariant &rows,
                                const QString &kind) const {
  QVariantList result;
  for (const auto &v : rows.toList()) {
    auto t = item(v.toMap(), kind);
    if (!t.isEmpty())
      result.append(t);
  }
  return result;
}
QUrl SubsonicApi::artworkUrl(const QUrl &reference) const {
  if (!connected() || reference.scheme() != "spunsubcover" ||
      reference.host() != "account" ||
      !reference.path().startsWith("/" + m_identity + "/"))
    return {};
  return url(
      "getCoverArt",
      {{"id", reference.path().mid(m_identity.size() + 2)}, {"size", "600"}});
}
void SubsonicApi::reloadPlaylists() {
  if (!connected())
    return;
  call(
      "getPlaylists", {},
      [this](const QVariantMap &d, const QString &e) {
        if (e.isEmpty()) {
          m_playlists =
              items(d.value("playlists").toMap().value("playlist"), "playlist");
          emit changed();
        } else
          emit message(e);
      },
      "playlists");
}
void SubsonicApi::createPlaylist(const QString &name, Reply callback) {
  if (name.trimmed().isEmpty()) {
    if (callback)
      callback({}, "Enter a playlist name.");
    return;
  }
  call("createPlaylist", {{"name", name.trimmed().left(120)}},
       [this, callback](const QVariantMap &d, const QString &e) {
         if (e.isEmpty())
           reloadPlaylists();
         if (callback)
           callback(d, e);
       });
}
void SubsonicApi::editPlaylist(const QString &id, const Params &params,
                               Reply callback) {
  playlistAccess(id, [this, id, params, callback](const QVariantMap &playlist,
                                                  const QString &error) {
    if (!error.isEmpty()) {
      callback({}, error);
      return;
    }
    Params p{{"playlistId", id}};
    for (const auto &param : params) {
      if (param.first == "entryIdToRemove") {
        const auto rows = playlist.value("entry").toList();
        bool ok = false;
        const int index = param.second.section(':', 1).toInt(&ok);
        if (!ok || index < 0 || index >= rows.size() ||
            param.second.section(':', 0, 0) != playlistRevision(rows)) {
          callback({},
                   "The playlist changed. Refresh it before removing songs.");
          return;
        }
        p.append({"songIndexToRemove", QString::number(index)});
      } else
        p.append(param);
    }
    call("updatePlaylist", p,
         [this, callback](const QVariantMap &d, const QString &e) {
           if (e.isEmpty())
             reloadPlaylists();
           callback(d, e);
         });
  });
}
void SubsonicApi::removePlaylist(const QString &id, Reply callback) {
  playlistAccess(
      id, [this, id, callback](const QVariantMap &, const QString &e) {
        if (!e.isEmpty()) {
          callback({}, e);
          return;
        }
        call("deletePlaylist", {{"id", id}},
             [this, callback](const QVariantMap &d, const QString &e) {
               if (e.isEmpty())
                 reloadPlaylists();
               callback(d, e);
             });
      });
}
void SubsonicApi::star(const QVariantMap &track, bool starred, Reply callback) {
  if (!owns(track)) {
    callback({}, "Reconnect to this song’s server first.");
    return;
  }
  const auto id = track.value("id").toString();
  call(starred ? "star" : "unstar",
       {{track.value("kind") == "album"    ? QString("albumId")
         : track.value("kind") == "artist" ? QString("artistId")
                                           : QString("id"),
         track.value("remoteId").toString()}},
       [this, id, starred, callback](const QVariantMap &d, const QString &e) {
         if (e.isEmpty()) {
           if (starred)
             m_stars.insert(id);
           else
             m_stars.remove(id);
           emit changed();
         }
         callback(d, e);
       });
}
void SubsonicApi::rate(const QVariantMap &track, int rating, Reply callback) {
  if (!owns(track) || rating < 0 || rating > 5) {
    callback({}, "Reconnect to this song’s server first.");
    return;
  }
  call("setRating",
       {{"id", track.value("remoteId").toString()},
        {"rating", QString::number(rating)}},
       callback);
}
void SubsonicApi::scrobble(const QVariantMap &track, bool submission,
                           qint64 timestamp, Reply callback) {
  if (!scrobbling() || !owns(track))
    return;
  call("scrobble",
       {{"id", track.value("remoteId").toString()},
        {"submission", submission ? "true" : "false"},
        {"time", QString::number(timestamp)}},
       callback);
}
void SubsonicApi::browse(const QVariantMap &req, Reply callback,
                         const QString &channel) {
  if (!connected()) {
    callback({}, "Connect to your music server in Settings.");
    return;
  }
  const auto mode = req.value("mode", "albums").toString();
  const int offset = qMax(0, req.value("offset").toInt());
  Params p;
  QString method, root, key, kind = "song";
  if (!folder().isEmpty())
    p.append(QPair<QString, QString>{"musicFolderId", folder()});
  if (mode == "albums" || mode == "recent") {
    method = "getAlbumList2";
    root = "albumList2";
    key = "album";
    kind = "album";
    p.append(QPair<QString, QString>{
        "type", mode == "recent"
                    ? QString("recent")
                    : req.value("sort", "alphabeticalByName").toString()});
    p.append(QPair<QString, QString>{"size", "100"});
    p.append(QPair<QString, QString>{"offset", QString::number(offset)});
  } else if (mode == "artists") {
    method = "getArtists";
    root = "artists";
    kind = "artist";
  } else if (mode == "genres") {
    method = "getGenres";
    root = "genres";
    key = "genre";
    kind = "genre";
  } else if (mode == "genre") {
    method = "getSongsByGenre";
    root = "songsByGenre";
    key = "song";
    p.append(QPair<QString, QString>{
        "genre", req.value("genre", req.value("remoteId")).toString()});
    p.append(QPair<QString, QString>{"count", "100"});
    p.append(QPair<QString, QString>{"offset", QString::number(offset)});
  } else if (mode == "favorites") {
    method = "getStarred2";
    root = "starred2";
    key = "song";
  } else if (mode == "random") {
    method = "getRandomSongs";
    root = "randomSongs";
    key = "song";
    p.append(QPair<QString, QString>{"size", "100"});
  } else if (mode == "playlists" ||
             (mode == "search" && req.value("filter") == "playlists")) {
    method = "getPlaylists";
    root = "playlists";
    key = "playlist";
    kind = "playlist";
  } else if (mode == "album") {
    method = "getAlbum";
    root = "album";
    key = "song";
    p = {{"id", req.value("remoteId").toString()}};
  } else if (mode == "artist") {
    method = "getArtist";
    root = "artist";
    key = "album";
    kind = "album";
    p = {{"id", req.value("remoteId").toString()}};
  } else if (mode == "playlist") {
    method = "getPlaylist";
    root = "playlist";
    key = "entry";
    p = {{"id", req.value("remoteId").toString()}};
  } else if (mode == "search" || mode == "songs") {
    method = "search3";
    root = "searchResult3";
    const auto filter = req.value("filter", "songs").toString();
    kind = filter == "albums"    ? "album"
           : filter == "artists" ? "artist"
                                 : "song";
    key = kind;
    p.append(QPair<QString, QString>{
        "query", mode == "songs"
                     ? (m_serviceName == "Navidrome" ? QString() : QString("*"))
                     : req.value("query").toString()});
    for (const auto &type :
         {QString("song"), QString("album"), QString("artist")}) {
      p.append(
          QPair<QString, QString>{type + "Count", type == kind ? "100" : "0"});
      p.append(
          QPair<QString, QString>{type + "Offset", QString::number(offset)});
    }
  } else {
    callback({}, "This server view is not supported.");
    return;
  }
  call(
      method, p,
      [this, callback, root, key, kind, mode, offset,
       req](const QVariantMap &data, const QString &error) {
        if (!error.isEmpty()) {
          callback({}, error);
          return;
        }
        const auto container = data.value(root).toMap();
        QVariantList raw = container.value(key).toList();
        if (mode == "artists")
          for (const auto &index : container.value("index").toList())
            raw.append(index.toMap().value("artist").toList());
        if (mode == "genres")
          for (auto &v : raw) {
            auto m = v.toMap();
            m["id"] = m.value("value");
            m["name"] = m.value("value");
            v = m;
          }
        if (mode == "search" && kind == "playlist") {
          const auto query = req.value("query").toString().trimmed();
          raw.erase(std::remove_if(
                        raw.begin(), raw.end(),
                        [&query](const QVariant &v) {
                          return !v.toMap().value("name").toString().contains(
                              query, Qt::CaseInsensitive);
                        }),
                    raw.end());
        }
        const auto revision = playlistRevision(raw);
        const bool paged = mode == "albums" || mode == "recent" ||
                           (mode == "search" && kind != "playlist") ||
                           mode == "songs" || mode == "genre";
        const bool more = paged ? raw.size() == 100 : raw.size() > offset + 100;
        if (!paged)
          raw = raw.mid(offset, 100);
        auto rows = items(raw, kind);
        if (mode == "playlist")
          for (int i = 0; i < rows.size(); ++i) {
            auto row = rows[i].toMap();
            row["entryId"] = revision + ":" + QString::number(offset + i);
            rows[i] = row;
          }

        for (const auto &v : raw) {
          const auto t = v.toMap();
          if (t.contains("starred"))
            m_stars.insert(item(t, kind).value("id").toString());
        }
        if (mode == "favorites") {
          m_stars.clear();
          for (const auto &starKind :
               {QString("song"), QString("album"), QString("artist")})
            for (const auto &v : items(container.value(starKind), starKind))
              m_stars.insert(v.toMap().value("id").toString());
          emit changed();
        }
        QVariantMap result{{"items", rows},
                           {"more", more && offset + rows.size() < 20000}};
        if (container.contains("name"))
          result["title"] = container.value("name");
        if (mode == "album") {
          result["artist"] = container.value("artist");
          result["year"] = container.value("year");
        }
        if (mode == "playlist")
          result["editable"] =
              container.value("owner").toString() == m_username;
        callback(result, {});
      },
      channel);
}
void SubsonicApi::lyrics(const QVariantMap &track, Reply callback) {
  if (!owns(track)) {
    callback({}, "Reconnect to this song’s server first.");
    return;
  }
  auto plain = [this, track, callback] {
    call(
        "getLyrics",
        {{"artist", track.value("artist").toString()},
         {"title", track.value("title").toString()}},
        [callback](const QVariantMap &d, const QString &e) {
          callback({{"ok", e.isEmpty()},
                    {"lyrics", d.value("lyrics").toMap().value("value")},
                    {"source", "Music server"}},
                   e);
        },
        "lyrics");
  };
  if (!m_lyricsExtension) {
    plain();
    return;
  }
  call(
      "getLyricsBySongId", {{"id", track.value("remoteId").toString()}},
      [plain, callback](const QVariantMap &d, const QString &e) {
        const auto variants =
            d.value("lyricsList").toMap().value("structuredLyrics").toList();
        if (!e.isEmpty() || variants.isEmpty()) {
          plain();
          return;
        }
        QVariantMap chosen = variants.first().toMap();
        for (const auto &v : variants)
          if (v.toMap().value("synced").toBool()) {
            chosen = v.toMap();
            break;
          }
        QVariantList lines;
        QStringList text;
        QString lrc;
        const auto offset = chosen.value("offset").toLongLong();
        for (const auto &v : chosen.value("line").toList()) {
          const auto line = v.toMap();
          text.append(line.value("value").toString());
          if (chosen.value("synced").toBool()) {
            const qint64 ms =
                qMax<qint64>(0, line.value("start").toLongLong() - offset);
            lrc += QString("[%1:%2.%3]%4\n")
                       .arg(ms / 60000)
                       .arg(ms / 1000 % 60, 2, 10, QChar('0'))
                       .arg(ms % 1000, 3, 10, QChar('0'))
                       .arg(line.value("value").toString());
          }
        }
        lines = Lrc::parse(lrc);
        callback({{"ok", true},
                  {"lines", lines},
                  {"lyrics", text.join('\n')},
                  {"source", "Music server"}},
                 {});
      },
      "lyrics");
}
void SubsonicApi::download(const QVariantMap &track, const QString &path,
                           Reply callback, const QString &channel) {
  cancel(channel);
  if (!owns(track)) {
    callback({}, "Connect to this song’s server in Settings.");
    return;
  }
  auto file = std::make_shared<QFile>(path);
  if (!file->open(QIODevice::WriteOnly)) {
    callback({}, "Could not create the audio buffer.");
    return;
  }
  file->setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  Params p{{"id", track.value("remoteId").toString()},
           {"format", bitrate() ? "mp3" : "raw"}};
  if (bitrate())
    p.append(QPair<QString, QString>{"maxBitRate", QString::number(bitrate())});
  QNetworkRequest request(url("stream", p));
  request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
  request.setTransferTimeout(30000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::ManualRedirectPolicy);
  auto r = m_network.get(request);
  m_channels[channel] = r;
  r->setReadBufferSize(256 * 1024);
  const auto generation = m_generation;
  auto failed = std::make_shared<bool>(false);
  auto drain = [r, file, failed] {
    auto bytes = r->readAll();
    if (file->size() + bytes.size() > 512LL * 1024 * 1024 ||
        file->write(bytes) != bytes.size()) {
      *failed = true;
      r->abort();
    }
  };
  connect(r, &QNetworkReply::readyRead, this, drain);
  auto timer = new QTimer(r);
  timer->setSingleShot(true);
  connect(timer, &QTimer::timeout, r, &QNetworkReply::abort);
  timer->start(180000);
  connect(
      r, &QNetworkReply::finished, this,
      [this, r, file, failed, drain, generation, callback, path, channel] {
        if (m_channels.value(channel) == r) {
          m_channels.remove(channel);
        }
        drain();
        file->close();
        r->deleteLater();
        if (generation != m_generation)
          return;
        const auto type =
            r->header(QNetworkRequest::ContentTypeHeader).toString();
        const int status =
            r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (*failed || r->error() != QNetworkReply::NoError || status != 200 ||
            type.contains("json") || type.contains("xml") ||
            type.contains("html") || file->size() == 0) {
          file->remove();
          callback({}, *failed
                           ? "Audio exceeds the 512 MiB buffer limit, or the "
                             "disk is full. Choose a lower server bitrate."
                           : "Could not load this song from the server. Check "
                             "the connection and retry.");
          return;
        }
        callback({{"ok", true}, {"file", path}}, {});
      });
}

QNetworkRequest SubsonicApi::artworkRequest(const QUrl &u) const {
  QNetworkRequest request(u);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::ManualRedirectPolicy);
  request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
  request.setTransferTimeout(15000);
  return request;
}
void SubsonicApi::cover(const QVariantMap &track, const QString &path,
                        Reply callback, const QString &channel) {
  cancel(channel);
  auto target = artworkUrl(QUrl(track.value("art").toString()));
  if (target.isEmpty()) {
    callback({}, {});
    return;
  }
  const int pixels = channel == "thumb" ? 256 : 1024;
  QUrlQuery query(target);
  query.removeAllQueryItems("size");
  query.addQueryItem("size", QString::number(pixels));
  target.setQuery(query);
  auto *r = m_network.get(artworkRequest(target));
  m_channels[channel] = r;
  const auto generation = m_generation;
  connect(r, &QNetworkReply::readyRead, this, [r] {
    if (r->bytesAvailable() > 4 * 1024 * 1024)
      r->abort();
  });
  auto *timer = new QTimer(r);
  timer->setSingleShot(true);
  connect(timer, &QTimer::timeout, r, &QNetworkReply::abort);
  timer->start(20000);
  connect(
      r, &QNetworkReply::finished, this,
      [this, r, generation, path, callback, channel, pixels] {
        if (m_channels.value(channel) == r)
          m_channels.remove(channel);
        r->deleteLater();
        if (generation != m_generation)
          return;
        if (r->error() != QNetworkReply::NoError ||
            r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() !=
                200) {
          callback({}, "Artwork is unavailable.");
          return;
        }
        auto bytes = r->readAll();
        if (bytes.size() > 4 * 1024 * 1024) {
          callback({}, "Artwork exceeds the size limit.");
          return;
        }
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        const auto size = reader.size();
        if (!size.isValid() || size.width() > 10000 || size.height() > 10000) {
          callback({}, "Artwork could not be decoded.");
          return;
        }
        reader.setScaledSize(size.scaled(pixels, pixels, Qt::KeepAspectRatio));
        const auto image = reader.read();
        QSaveFile file(path);
        if (image.isNull() || !file.open(QIODevice::WriteOnly)) {
          callback({}, "Artwork could not be saved.");
          return;
        }
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        if (!image.save(&file, "PNG") || !file.commit()) {
          callback({}, "Artwork could not be saved.");
          return;
        }
        callback({{"file", path}}, {});
      });
}
QUrl SubsonicApi::shareUrl(const QVariantMap &row) const {
  if (!owns(row))
    return {};
  // Server-specific web routes are not part of Subsonic. Share the clean server
  // address on other implementations, never a signed stream or artwork URL.
  QUrl u(address());
  if (m_serviceName == "Navidrome") {
    const auto kind = row.value("kind").toString();
    const auto id = kind == "song" ? row.value("albumId").toString()
                                   : row.value("remoteId").toString();
    if (!id.isEmpty() &&
        QStringList{"song", "album", "artist", "playlist"}.contains(kind))
      u.setFragment("/" + (kind == "song" ? QString("album") : kind) + "/" +
                    QString::fromLatin1(QUrl::toPercentEncoding(id)) + "/show");
  }
  return u;
}
static QString playlistRevision(const QVariantList &rows) {
  QStringList ids;
  for (const auto &v : rows)
    ids.append(v.toMap().value("id").toString());
  return hash(ids.join('\n'));
}
void SubsonicApi::playlistAccess(const QString &id, Reply callback) {
  if (!connected() || id.isEmpty()) {
    callback({}, "Reconnect and refresh the playlist.");
    return;
  }
  call("getPlaylist", {{"id", id}},
       [this, callback](const QVariantMap &d, const QString &e) {
         if (!e.isEmpty()) {
           callback({}, e);
           return;
         }
         const auto p = d.value("playlist").toMap();
         if (p.value("owner").toString() != m_username) {
           callback({}, "Only the playlist owner can edit this playlist.");
           return;
         }
         if (p.value("entry").toList().size() > 20000) {
           callback({}, "This playlist exceeds the 20,000-song limit.");
           return;
         }
         callback(p, {});
       });
}
void SubsonicApi::movePlaylistItem(const QString &id, const QString &entry,
                                   int to, Reply callback) {
  playlistAccess(id, [this, id, entry, to, callback](const QVariantMap &p,
                                                     const QString &e) {
    if (!e.isEmpty()) {
      callback({}, e);
      return;
    }
    auto rows = p.value("entry").toList();
    bool ok = false;
    const int from = entry.section(':', 1).toInt(&ok);
    if (!ok || entry.section(':', 0, 0) != playlistRevision(rows) || from < 0 ||
        to < 0 || from >= rows.size() || to >= rows.size()) {
      callback({}, "The playlist changed. Refresh it before reordering.");
      return;
    }
    rows.move(from, to);
    Params params{{"playlistId", id}};
    for (const auto &v : rows)
      params.append({"songId", v.toMap().value("id").toString()});
    call("createPlaylist", params,
         [this, callback](const QVariantMap &d, const QString &e) {
           if (e.isEmpty())
             reloadPlaylists();
           callback(d, e);
         });
  });
}
void SubsonicApi::reportPlayback(const QVariantMap &track, qint64 position,
                                 bool paused, bool stopped) {
  const auto now = QDateTime::currentMSecsSinceEpoch();
  const auto id = track.value("id").toString();
  if (!scrobbling() || !owns(track)) {
    m_playing.clear();
    m_lastReport = 0;
    m_listened = 0;
    m_wasPlaying = false;
    return;
  }
  if (m_playing != id) {
    m_playing = id;
    m_playStarted = now;
    m_listened = 0;
    m_submitted = false;
    m_wasPlaying = false;
    m_lastReport = 0;
  }
  if (m_lastReport && m_wasPlaying)
    m_listened += qBound<qint64>(0LL, now - m_lastReport, 15000LL);
  m_lastReport = now;
  m_wasPlaying = !paused && !stopped;
  const auto duration = track.value("seconds").toLongLong() * 1000;
  const auto threshold =
      duration > 0 ? qMin<qint64>(duration / 2, 240000) : 240000;
  auto done = [this](const QVariantMap &, const QString &error) {
    if (!error.isEmpty())
      emit message(error);
  };
  if (!m_submitted && m_listened >= threshold && threshold > 0) {
    m_submitted = true;
    scrobble(track, true, m_playStarted, done);
  } else if (!paused && !stopped && position >= 0)
    scrobble(track, false, now, done);
  if (stopped) {
    m_playing.clear();
    m_lastReport = 0;
    m_wasPlaying = false;
  }
}
