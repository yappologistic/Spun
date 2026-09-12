#include "jellyfinapi.h"
#include "jellyfinlrc.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <memory>

namespace {
QString digest(const QString &s) {
  return QString::fromLatin1(
      QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Sha256).toHex());
}
QString part(const QString &s) {
  return QString::fromLatin1(QUrl::toPercentEncoding(s));
}
QString credentialKey(const QString &address, const QString &user) {
  return "jellyfin_" + digest(address + "\n" + user);
}
QStringList values(const JellyfinApi::Params &params, const QString &key) {
  QStringList out;
  for (const auto &p : params)
    if (p.first == key)
      out << p.second;
  return out;
}
QString value(const JellyfinApi::Params &params, const QString &key) {
  return values(params, key).value(0);
}
QString networkError(QNetworkReply *r) {
  const int status =
      r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (status == 401)
    return "Jellyfin sign-in failed or the session expired. Reconnect in "
           "Settings.";
  if (status == 403)
    return "Your Jellyfin account does not allow this action.";
  if (status == 404)
    return "This item or endpoint is no longer available on Jellyfin.";
  if (status >= 300 && status < 400)
    return "Jellyfin redirected the request. Use the final server address in "
           "Settings.";
  if (r->error() == QNetworkReply::SslHandshakeFailedError)
    return "Jellyfin's TLS certificate could not be verified.";
  return "Could not reach Jellyfin or complete the request. Check the "
         "connection and retry.";
}
} // namespace
JellyfinApi::JellyfinApi(bool restore, const QString &settingsPath)
    : m_settings(settingsPath.isEmpty() ? QSettings().fileName() : settingsPath,
                 QSettings::IniFormat) {
  m_device = m_settings.value("jellyfin/device").toString();
  if (m_device.isEmpty()) {
    m_device = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_settings.setValue("jellyfin/device", m_device);
  }
  if (!restore)
    return;
  m_address = m_settings.value("jellyfin/address").toString();
  m_username = m_settings.value("jellyfin/username").toString();
  if (m_address.isEmpty() || !m_settings.value("jellyfin/remember").toBool())
    return;
  QTimer::singleShot(0, this, [this] {
    const auto generation = m_generation;
    secret({"lookup", "application", "spun", "account",
            credentialKey(m_address, m_username)},
           {}, [this, generation](bool ok, QByteArray token) {
             if (generation != m_generation)
               return;
             if (!ok || token.trimmed().isEmpty()) {
               m_error = "Sign in to reconnect to Jellyfin.";
               emit changed();
               return;
             }
             m_token = QString::fromUtf8(token.trimmed());
             m_connecting = true;
             emit changed();
             json(
                 "Users/Me", {}, "GET", {},
                 [this](const QVariantMap &d, const QString &e) {
                   m_connecting = false;
                   if (!e.isEmpty()) {
                     m_error = e;
                     m_token.clear();
                     emit changed();
                     return;
                   }
                   acceptLogin({{"User", d}, {"AccessToken", m_token}}, false);
                 },
                 "login");
           });
  });
}
JellyfinApi::~JellyfinApi() { stopRequests(); }
void JellyfinApi::stopRequests() {
  m_reports.clear();
  ++m_generation;
  for (auto *r : m_network.findChildren<QNetworkReply *>()) {
    r->disconnect(this);
    r->abort();
    r->deleteLater();
  }
  m_channels.clear();
}
void JellyfinApi::cancel(const QString &channel) {
  auto r = m_channels.take(channel);
  if (r) {
    r->disconnect(this);
    r->abort();
    r->deleteLater();
  }
}
QUrl JellyfinApi::url(const QString &path, const Params &params) const {
  QUrl u(m_address);
  u.setPath(u.path() + "/" + path, QUrl::TolerantMode);
  QUrlQuery q;
  for (const auto &p : params)
    q.addQueryItem(p.first, p.second);
  u.setQuery(q);
  return u;
}
QNetworkRequest JellyfinApi::request(const QUrl &u) const {
  QNetworkRequest r(u);
  r.setTransferTimeout(15000);
  r.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                 QNetworkRequest::ManualRedirectPolicy);
  r.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
  QByteArray auth =
      "MediaBrowser Client=\"Spun\", Device=\"Spun Linux\", DeviceId=\"" +
      m_device.toUtf8() + "\", Version=\"1.0\"";
  if (!m_token.isEmpty())
    auth += ", Token=\"" + m_token.toUtf8() + "\"";
  r.setRawHeader("Authorization", auth);
  r.setRawHeader("Accept", "application/json");
  return r;
}
void JellyfinApi::json(const QString &path, const Params &params,
                       const QByteArray &method, const QJsonObject &body,
                       Reply callback, const QString &channel) {
  if (!channel.isEmpty())
    cancel(channel);
  auto req = request(url(path, params));
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  auto *r = m_network.sendCustomRequest(
      req, method,
      method == "GET" ? QByteArray()
                      : QJsonDocument(body).toJson(QJsonDocument::Compact));
  if (!channel.isEmpty())
    m_channels[channel] = r;
  const auto generation = m_generation;
  auto oversized = std::make_shared<bool>(false);
  connect(r, &QNetworkReply::readyRead, this, [r, oversized] {
    if (r->bytesAvailable() > 16 * 1024 * 1024) {
      *oversized = true;
      r->abort();
    }
  });
  auto timer = new QTimer(r);
  timer->setSingleShot(true);
  connect(timer, &QTimer::timeout, r, &QNetworkReply::abort);
  timer->start(30000);
  connect(r, &QNetworkReply::finished, this,
          [this, r, generation, callback, channel, oversized] {
            if (m_channels.value(channel) == r)
              m_channels.remove(channel);
            r->deleteLater();
            if (generation != m_generation)
              return;
            const int status =
                r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (*oversized) {
              callback({}, "Jellyfin response exceeds the 16 MiB limit.");
              return;
            }
            if (r->error() != QNetworkReply::NoError || status < 200 ||
                status >= 300) {
              callback({{"httpStatus", status}}, networkError(r));
              return;
            }
            const auto bytes = r->readAll();
            if (bytes.isEmpty()) {
              callback({}, {});
              return;
            }
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(bytes, &error);
            if (error.error != QJsonParseError::NoError ||
                (!doc.isObject() && !doc.isArray())) {
              callback({},
                       "This address did not return a Jellyfin API response.");
              return;
            }
            callback(doc.isObject()
                         ? doc.object().toVariantMap()
                         : QVariantMap{{"Items", doc.array().toVariantList()}},
                     {});
          });
}
void JellyfinApi::connectServer(const QString &address, const QString &user,
                                const QString &password, bool remember) {
  QUrl base(address.trimmed());
  if (!base.isValid() ||
      !QStringList{"http", "https"}.contains(base.scheme()) ||
      base.host().isEmpty() || !base.userInfo().isEmpty() || base.hasQuery() ||
      base.hasFragment() || user.trimmed().isEmpty()) {
    m_error = "Enter an HTTP(S) Jellyfin server address and username.";
    emit changed();
    return;
  }
  auto path = base.path();
  while (path.endsWith('/'))
    path.chop(1);
  base.setPath(path);
  if (!remember && m_settings.value("jellyfin/remember").toBool()) {
    secret({"clear", "application", "spun", "account",
            credentialKey(m_settings.value("jellyfin/address").toString(),
                          m_settings.value("jellyfin/username").toString())},
           {}, [](bool, QByteArray) {});
    m_settings.setValue("jellyfin/remember", false);
  }
  stopRequests();
  m_connected = false;
  m_connecting = true;
  m_error.clear();
  m_token.clear();
  m_user.clear();
  m_identity.clear();
  m_stars.clear();
  m_playlists.clear();
  m_folders.clear();
  m_playing.clear();
  m_address = base.toString();
  m_username = user.trimmed();
  emit accountChanged();
  emit changed();
  json(
      "Users/AuthenticateByName", {}, "POST",
      {{"Username", m_username}, {"Pw", password}},
      [this, remember](const QVariantMap &d, const QString &e) {
        m_connecting = false;
        if (!e.isEmpty()) {
          m_error = e;
          emit changed();
          return;
        }
        acceptLogin(d, remember);
      },
      "login");
}
void JellyfinApi::acceptLogin(const QVariantMap &d, bool remember) {
  const auto user = d.value("User").toMap();
  const auto token = d.value("AccessToken").toString();
  if (user.value("Id").toString().isEmpty() || token.isEmpty() ||
      token.contains('"') || token.contains('\n') || token.contains('\r')) {
    m_error = "Jellyfin returned an invalid sign-in response.";
    m_token.clear();
    emit changed();
    return;
  }
  m_user = user.value("Id").toString();
  m_token = token;
  m_identity = digest(m_address + "\n" + m_user);
  m_connected = true;
  m_error.clear();
  const auto oldAddress = m_settings.value("jellyfin/address").toString(),
             oldUser = m_settings.value("jellyfin/username").toString();
  if (oldAddress != m_address || oldUser != m_username) {
    m_settings.remove("jellyfin/folder");
    if (!oldAddress.isEmpty())
      secret({"clear", "application", "spun", "account",
              credentialKey(oldAddress, oldUser)},
             {}, [](bool, QByteArray) {});
    m_settings.setValue("jellyfin/remember", false);
  }
  m_settings.setValue("jellyfin/address", m_address);
  m_settings.setValue("jellyfin/username", m_username);
  if (remember) {
    const auto generation = m_generation;
    secret({"store", "--label=Spun Jellyfin", "application", "spun", "account",
            credentialKey(m_address, m_username)},
           m_token.toUtf8(), [this, generation](bool ok, QByteArray) {
             if (generation != m_generation)
               return;
             m_settings.setValue("jellyfin/remember", ok);
             if (!ok)
               emit message("Connected for this session. The keyring could not "
                            "save the Jellyfin session.");
           });
  }
  emit changed();
  discover();
}
void JellyfinApi::disconnectServer() {
  const auto old = credentialKey(m_address, m_username);
  const bool remembered = m_settings.value("jellyfin/remember").toBool();
  stopRequests();
  m_token.clear();
  m_user.clear();
  m_identity.clear();
  m_connected = false;
  m_connecting = false;
  m_error.clear();
  m_stars.clear();
  m_playlists.clear();
  m_folders.clear();
  m_playing.clear();
  m_address.clear();
  m_username.clear();
  for (const auto &key : {"address", "username", "remember", "folder"})
    m_settings.remove(QString("jellyfin/") + key);
  if (remembered)
    secret({"clear", "application", "spun", "account", old}, {},
           [](bool, QByteArray) {});
  emit accountChanged();
  emit changed();
}
void JellyfinApi::setFolder(const QString &id) {
  m_settings.setValue("jellyfin/folder", id);
  emit changed();
}
void JellyfinApi::setScrobbling(bool value) {
  if (!value && !m_playing.isEmpty()) {
    const bool idle = m_reports.isEmpty();
    m_reports.append(
        {"Sessions/Playing/Stopped", QJsonObject{{"ItemId", m_playing}}});
    m_playing.clear();
    if (idle)
      sendPlaybackReport();
  }
  m_settings.setValue("jellyfin/scrobble", value);
  emit changed();
}
void JellyfinApi::setBitrate(int value) {
  if (!QList<int>{0, 128, 192, 320}.contains(value))
    return;
  m_settings.setValue("jellyfin/bitrate", value);
  emit changed();
}
bool JellyfinApi::owns(const QVariantMap &t) const {
  return connected() && t.value("source") == "jellyfin" &&
         t.value("server") == m_identity &&
         !t.value("remoteId").toString().isEmpty();
}
QVariantMap JellyfinApi::item(const QVariantMap &raw,
                              const QString &kind) const {
  const auto id = raw.value("Id", raw.value("id")).toString();
  if (id.isEmpty())
    return {};
  QStringList artists = raw.value("Artists").toStringList();
  if (artists.isEmpty())
    artists = raw.value("AlbumArtists").toStringList();
  QVariantMap out{
      {"id", "jf_" + digest(m_identity + "\n" + kind + "\n" + id)},
      {"source", "jellyfin"},
      {"server", m_identity},
      {"remoteId", id},
      {"kind", kind},
      {"title", raw.value("Name", raw.value("name"))},
      {"artist", artists.join(", ")},
      {"album", raw.value("Album")},
      {"seconds", raw.value("RunTimeTicks").toLongLong() / 10000000},
      {"discNumber", raw.value("ParentIndexNumber", 1)},
      {"trackNumber", raw.value("IndexNumber")},
      {"year", raw.value("ProductionYear")},
      {"count", raw.value("ChildCount", raw.value("SongCount", 0))},
      {"available", true}};
  if (kind == "song") {
    out["serverSong"] = true;
    out["albumId"] = raw.value("AlbumId");
    const auto artistItems = raw.value("ArtistItems").toList();
    if (!artistItems.isEmpty())
      out["artistId"] = artistItems.first().toMap().value("Id");
  }
  if (raw.contains("PlaylistItemId"))
    out["entryId"] = raw.value("PlaylistItemId");
  if (kind == "playlist")
    out["editable"] = raw.value("CanDelete", false);
  if (kind == "playlist")
    out["deletable"] = raw.value("CanDelete", false);
  auto imageId = id;
  if (!raw.value("ImageTags").toMap().contains("Primary"))
    imageId = raw.value("AlbumPrimaryImageTag").toString().isEmpty()
                  ? QString()
                  : raw.value("AlbumId").toString();
  if (!imageId.isEmpty()) {
    QUrl art;
    art.setScheme("spuncover");
    art.setHost("jellyfin");
    art.setPath("/" + m_identity + "/" + imageId);
    out["art"] = art.toString();
  }
  return out;
}
QUrl JellyfinApi::artworkUrl(const QUrl &ref) const {
  if (!connected() || ref.scheme() != "spuncover" || ref.host() != "jellyfin" ||
      !ref.path().startsWith("/" + m_identity + "/"))
    return {};
  return url("Items/" + part(ref.path().mid(m_identity.size() + 2)) +
                 "/Images/Primary",
             {{"MaxWidth", "600"}, {"Quality", "90"}});
}
QNetworkRequest JellyfinApi::artworkRequest(const QUrl &ref) const {
  return request(artworkUrl(ref));
}
void JellyfinApi::fetchRows(const QString &path, Params params, bool all,
                            Reply callback, const QString &channel) {
  auto rows = std::make_shared<QVariantList>();
  auto step = std::make_shared<std::function<void(int)>>();
  std::weak_ptr<std::function<void(int)>> weak = step;
  const int start = value(params, "StartIndex").toInt();
  *step = [this, path, params, all, callback, channel, rows, weak](int offset) {
    auto next = weak.lock();
    if (!next)
      return;
    auto p = params;
    for (auto it = p.begin(); it != p.end();)
      if (it->first == "StartIndex" || it->first == "Limit")
        it = p.erase(it);
      else
        ++it;
    p.append(QPair<QString, QString>{"StartIndex", QString::number(offset)});
    p.append(QPair<QString, QString>{"Limit", "100"});
    json(
        path, p, "GET", {},
        [this, all, callback, rows, next, offset](const QVariantMap &d,
                                                  const QString &e) {
          if (!e.isEmpty()) {
            callback({}, e);
            return;
          }
          const auto page = d.value("Items").toList();
          rows->append(page);
          const int total =
              d.value("TotalRecordCount", offset + page.size()).toInt();
          const bool more = !page.isEmpty() && offset + page.size() < total;
          if (all && more) {
            if (rows->size() >= 20000) {
              callback({}, "This collection exceeds the 20,000-item limit. "
                           "Open a smaller collection.");
              return;
            }
            (*next)(offset + page.size());
            return;
          }
          callback({{"Items", *rows}, {"more", more}}, {});
        },
        channel);
  };
  (*step)(start);
}
void JellyfinApi::discover() {
  json(
      "Users/" + part(m_user) + "/Views", {}, "GET", {},
      [this](const QVariantMap &d, const QString &e) {
        if (!e.isEmpty()) {
          emit message(e);
          return;
        }
        m_folders.clear();
        for (const auto &v : d.value("Items").toList()) {
          auto row = v.toMap();
          if (row.value("CollectionType") == "music")
            m_folders.append(QVariantMap{{"id", row.value("Id")},
                                         {"name", row.value("Name")}});
        }
        bool found = folder().isEmpty();
        for (const auto &v : m_folders)
          if (v.toMap().value("id") == folder())
            found = true;
        if (!found)
          m_settings.remove("jellyfin/folder");
        emit changed();
      },
      "folders");
  fetchRows(
      "Items",
      {{"UserId", m_user},
       {"Recursive", "true"},
       {"IncludeItemTypes", "Audio"},
       {"Filters", "IsFavorite"},
       {"Fields", "UserData"}},
      true,
      [this](const QVariantMap &d, const QString &e) {
        if (!e.isEmpty())
          return;
        for (const auto &v : d.value("Items").toList())
          m_stars.insert(item(v.toMap(), "song").value("id").toString());
        emit changed();
      },
      "stars");
  reloadPlaylists();
}
void JellyfinApi::playlistAccess(const QString &id, Reply cb,
                                 const QString &channel) {
  json(
      "Playlists/" + part(id) + "/Users/" + part(m_user), {}, "GET", {},
      [cb](const QVariantMap &d, const QString &e) {
        cb({{"editable", e.isEmpty() && d.value("CanEdit").toBool()}}, {});
      },
      channel);
}
void JellyfinApi::browse(const QVariantMap &req, Reply cb,
                         const QString &channel) {
  if (!connected()) {
    cb({}, "Connect to Jellyfin in Settings.");
    return;
  }
  const auto mode = req.value("mode", "albums").toString();
  QString path = "Items", kind = "song";
  bool all = false;
  Params p{
      {"UserId", m_user},
      {"Recursive", "true"},
      {"Fields", "PrimaryImageAspectRatio,Genres,CanDelete,ChildCount"},
      {"EnableUserData", "true"},
      {"SortBy", "SortName"},
      {"SortOrder", "Ascending"},
      {"StartIndex", QString::number(qMax(0, req.value("offset").toInt()))}};
  auto set = [&p](const QString &k, const QString &v) {
    for (auto it = p.begin(); it != p.end();)
      if (it->first == k)
        it = p.erase(it);
      else
        ++it;
    p.append(QPair<QString, QString>{k, v});
  };
  if (!folder().isEmpty())
    set("ParentId", folder());
  if (mode == "albums") {
    kind = "album";
    set("IncludeItemTypes", "MusicAlbum");
    set("SortBy", "DateCreated,SortName");
    set("SortOrder", "Descending");
  } else if (mode == "artists") {
    kind = "artist";
    path = "Artists";
  } else if (mode == "genres") {
    kind = "genre";
    path = "MusicGenres";
    set("IncludeItemTypes", "Audio");
  } else if (mode == "genre") {
    set("GenreIds", req.value("remoteId").toString());
    set("IncludeItemTypes", "Audio");
  } else if (mode == "favorites") {
    set("Filters", "IsFavorite");
    set("IncludeItemTypes", "Audio");
  } else if (mode == "songs" || mode == "recent") {
    set("IncludeItemTypes", "Audio");
    if (mode == "recent") {
      set("SortBy", "DatePlayed,SortName");
      set("SortOrder", "Descending");
      set("Filters", "IsPlayed");
    }
  } else if (mode == "random") {
    set("SortBy", "Random");
    set("IncludeItemTypes", "Audio");
  } else if (mode == "playlists") {
    kind = "playlist";
    set("IncludeItemTypes", "Playlist");
    set("MediaTypes", "Audio");
    set("ParentId", "");
  } else if (mode == "album") {
    all = true;
    set("ParentId", req.value("remoteId").toString());
    set("IncludeItemTypes", "Audio");
    set("SortBy", "ParentIndexNumber,IndexNumber,SortName");
  } else if (mode == "artist") {
    kind = "album";
    set("ArtistIds", req.value("remoteId").toString());
    set("IncludeItemTypes", "MusicAlbum");
  } else if (mode == "playlist") {
    all = true;
    path = "Playlists/" + part(req.value("remoteId").toString()) + "/Items";
    p = {{"UserId", m_user}, {"Fields", "Genres,CanDelete"}};
  } else if (mode == "search") {
    const auto filter = req.value("filter", "songs").toString();
    kind = filter == "albums"      ? "album"
           : filter == "artists"   ? "artist"
           : filter == "playlists" ? "playlist"
                                   : "song";
    set("SearchTerm", req.value("query").toString());
    if (kind == "artist")
      path = "Artists";
    else
      set("IncludeItemTypes", kind == "album"      ? "MusicAlbum"
                              : kind == "playlist" ? "Playlist"
                                                   : "Audio");
  } else {
    cb({}, "This Jellyfin view is not supported.");
    return;
  }
  fetchRows(
      path, p, all,
      [this, cb, kind, mode, req, channel](const QVariantMap &d,
                                           const QString &e) {
        if (!e.isEmpty()) {
          cb({}, e);
          return;
        }
        QVariantList rows;
        for (const auto &v : d.value("Items").toList()) {
          const auto raw = v.toMap();
          auto row = item(raw, kind);
          if (row.isEmpty())
            continue;
          rows << row;
          if (raw.value("UserData").toMap().value("IsFavorite").toBool())
            m_stars.insert(row.value("id").toString());
          else
            m_stars.remove(row.value("id").toString());
        }
        QVariantMap result{
            {"items", rows},
            {"more", mode != "random" && d.value("more").toBool()}};
        if (mode == "playlist" || mode == "album") {
          const auto remote = req.value("remoteId").toString();
          json(
              "Users/" + part(m_user) + "/Items/" + part(remote), {}, "GET", {},
              [this, cb, result, remote, mode,
               channel](const QVariantMap &meta, const QString &error) mutable {
                if (!error.isEmpty()) {
                  cb({}, error);
                  return;
                }
                result["title"] = meta.value("Name");
                result["artist"] = meta.value(
                    "AlbumArtist",
                    meta.value("AlbumArtists").toStringList().join(", "));
                result["year"] = meta.value("ProductionYear");
                if (mode == "playlist")
                  playlistAccess(
                      remote,
                      [cb, result](const QVariantMap &access,
                                   const QString &) mutable {
                        result["editable"] = access.value("editable");
                        cb(result, {});
                      },
                      channel);
                else
                  cb(result, {});
              },
              channel);
        } else if (kind == "playlist")
          resolvePlaylistAccess(
              rows,
              [cb, result](const QVariantMap &d, const QString &e) mutable {
                result["items"] = d.value("items");
                cb(result, e);
              },
              channel);
        else
          cb(result, {});
      },
      channel);
}
void JellyfinApi::reloadPlaylists() {
  if (!connected())
    return;
  fetchRows(
      "Items",
      {{"UserId", m_user},
       {"Recursive", "true"},
       {"IncludeItemTypes", "Playlist"},
       {"MediaTypes", "Audio"},
       {"Fields", "CanDelete"}},
      true,
      [this](const QVariantMap &d, const QString &e) {
        if (!e.isEmpty()) {
          emit message(e);
          return;
        }
        QVariantList rows;
        for (const auto &v : d.value("Items").toList())
          rows << item(v.toMap(), "playlist");
        resolvePlaylistAccess(
            rows,
            [this](const QVariantMap &result, const QString &) {
              m_playlists = result.value("items").toList();
              emit changed();
            },
            "playlists");
      },
      "playlists");
}
void JellyfinApi::resolvePlaylistAccess(QVariantList rows, Reply cb,
                                        const QString &channel) {
  auto data = std::make_shared<QVariantList>(std::move(rows));
  auto index = std::make_shared<int>(0);
  auto step = std::make_shared<std::function<void()>>();
  std::weak_ptr<std::function<void()>> weak = step;
  *step = [this, data, index, weak, cb, channel] {
    while (*index < data->size() &&
           data->at(*index).toMap().value("editable").toBool())
      ++*index;
    if (*index >= data->size()) {
      cb({{"items", *data}}, {});
      return;
    }
    auto keep = weak.lock();
    playlistAccess(
        data->at(*index).toMap().value("remoteId").toString(),
        [data, index, keep](const QVariantMap &access, const QString &) {
          auto row = data->at(*index).toMap();
          row["editable"] = access.value("editable");
          (*data)[(*index)++] = row;
          (*keep)();
        },
        channel);
  };
  (*step)();
}
void JellyfinApi::createPlaylist(const QString &name, Reply cb) {
  if (name.trimmed().isEmpty() || !connected()) {
    if (cb)
      cb({}, "Connect and enter a playlist name first.");
    return;
  }
  json("Playlists", {}, "POST",
       {{"Name", name.trimmed().left(120)},
        {"UserId", m_user},
        {"MediaType", "Audio"},
        {"IsPublic", false}},
       [this, cb](const QVariantMap &data, const QString &e) {
         if (e.isEmpty())
           reloadPlaylists();
         if (cb)
           cb(data, e);
         else
           emit message(e.isEmpty() ? "Server playlist created" : e);
       });
}
void JellyfinApi::editPlaylist(const QString &id, const Params &params,
                               Reply cb) {
  if (!connected()) {
    cb({}, "Connect to Jellyfin first.");
    return;
  }
  auto done = [this, cb](const QVariantMap &d, const QString &e) {
    if (e.isEmpty())
      reloadPlaylists();
    cb(d, e);
  };
  if (!value(params, "name").isEmpty()) {
    json("Playlists/" + part(id), {}, "POST", {{"Name", value(params, "name")}},
         done);
    return;
  }
  const auto additions = values(params, "songIdToAdd");
  if (!additions.isEmpty()) {
    json("Playlists/" + part(id) + "/Items",
         {{"Ids", additions.join(',')}, {"UserId", m_user}}, "POST", {}, done);
    return;
  }
  const auto entries = values(params, "entryIdToRemove");
  if (!entries.isEmpty()) {
    if (entries.contains(QString())) {
      done({}, "Refresh the playlist before removing songs.");
      return;
    }
    json("Playlists/" + part(id) + "/Items", {{"EntryIds", entries.join(',')}},
         "DELETE", {}, done);
    return;
  }
  const auto removals = values(params, "songIndexToRemove");
  if (!removals.isEmpty()) {
    browse(
        {{"mode", "playlist"}, {"remoteId", id}},
        [this, id, removals, done](const QVariantMap &d, const QString &e) {
          if (!e.isEmpty()) {
            done({}, e);
            return;
          }
          QStringList ids;
          const auto rows = d.value("items").toList();
          for (const auto &s : removals) {
            bool ok = false;
            const int index = s.toInt(&ok);
            if (!ok || index < 0 || index >= rows.size()) {
              done({},
                   "The playlist changed. Refresh it before removing songs.");
              return;
            }
            ids << rows[index].toMap().value("entryId").toString();
          }
          if (ids.contains(QString())) {
            done({}, "Jellyfin did not provide playlist entry identities.");
            return;
          }
          json("Playlists/" + part(id) + "/Items",
               {{"EntryIds", ids.join(',')}}, "DELETE", {}, done);
        },
        "playlist-edit");
    return;
  }
  done({}, "No playlist changes supplied.");
}
void JellyfinApi::removePlaylist(const QString &id, Reply cb) {
  if (!connected()) {
    cb({}, "Connect to Jellyfin first.");
    return;
  }
  json("Items/" + part(id), {}, "DELETE", {},
       [this, cb](const QVariantMap &d, const QString &e) {
         if (e.isEmpty())
           reloadPlaylists();
         cb(d, e);
       });
}
void JellyfinApi::star(const QVariantMap &track, bool starred, Reply cb) {
  if (!owns(track)) {
    cb({}, "Reconnect to this song's Jellyfin server first.");
    return;
  }
  cancel("stars");
  json("Users/" + part(m_user) + "/FavoriteItems/" +
           part(track.value("remoteId").toString()),
       {}, starred ? "POST" : "DELETE", {},
       [this, track, starred, cb](const QVariantMap &d, const QString &e) {
         if (e.isEmpty()) {
           if (starred)
             m_stars.insert(track.value("id").toString());
           else
             m_stars.remove(track.value("id").toString());
           emit changed();
         }
         cb(d, e);
       });
}
void JellyfinApi::reportPlayback(const QVariantMap &track, qint64 position,
                                 bool paused, bool stopped) {
  if (!owns(track) || !scrobbling())
    return;
  const auto id = track.value("remoteId").toString();
  QString endpoint = stopped           ? "Sessions/Playing/Stopped"
                     : m_playing == id ? "Sessions/Playing/Progress"
                                       : "Sessions/Playing";
  if ((stopped && m_playing != id) ||
      (paused && !stopped && m_playing.isEmpty()))
    return;
  QJsonObject body{{"ItemId", id},
                   {"PositionTicks", double(qMax<qint64>(0, position) * 10000)},
                   {"IsPaused", paused},
                   {"CanSeek", true},
                   {"PlayMethod", bitrate() ? "Transcode" : "DirectPlay"}};
  if (stopped)
    m_playing.clear();
  else
    m_playing = id;
  const bool idle = m_reports.isEmpty();
  if (m_reports.size() > 1 && endpoint == "Sessions/Playing/Progress" &&
      m_reports.last().first == endpoint)
    m_reports.last().second = body;
  else
    m_reports.append({endpoint, body});
  if (idle)
    sendPlaybackReport();
}
void JellyfinApi::sendPlaybackReport() {
  if (m_reports.isEmpty())
    return;
  const auto report = m_reports.first();
  json(
      report.first, {}, "POST", report.second,
      [this](const QVariantMap &, const QString &error) {
        if (!error.isEmpty()) {
          m_reports.clear();
          m_playing.clear();
          return;
        }
        if (!m_reports.isEmpty())
          m_reports.removeFirst();
        sendPlaybackReport();
      },
      "session");
}
void JellyfinApi::lyrics(const QVariantMap &track, Reply cb) {
  if (!owns(track)) {
    cb({}, "Reconnect to this song's Jellyfin server first.");
    return;
  }
  json(
      "Audio/" + part(track.value("remoteId").toString()) + "/Lyrics", {},
      "GET", {},
      [cb](const QVariantMap &d, const QString &e) {
        if (!e.isEmpty()) {
          if (d.value("httpStatus").toInt() == 404)
            cb({{"ok", true}, {"lyrics", ""}, {"lines", QVariantList{}}}, {});
          else
            cb({}, e);
          return;
        }
        QStringList text;
        QString lrc;
        const auto offset =
            d.value("Metadata").toMap().value("Offset").toLongLong();
        for (const auto &v : d.value("Lyrics").toList()) {
          const auto line = v.toMap();
          const auto t = line.value("Text").toString();
          text << t;
          if (line.contains("Start") && !line.value("Start").isNull()) {
            const auto ms = qMax<qint64>(
                0, (line.value("Start").toLongLong() - offset) / 10000);
            lrc += QString("[%1:%2.%3]%4\n")
                       .arg(ms / 60000)
                       .arg(ms / 1000 % 60, 2, 10, QChar('0'))
                       .arg(ms % 1000, 3, 10, QChar('0'))
                       .arg(t);
          }
        }
        cb({{"ok", true},
            {"lyrics", text.join('\n')},
            {"lines", Lrc::parse(lrc)},
            {"source", "Jellyfin"}},
           {});
      },
      "lyrics");
}
void JellyfinApi::file(const QUrl &u, const QString &path, qint64 limit,
                       Reply cb, const QString &channel) {
  cancel(channel);
  auto f = std::make_shared<QFile>(path);
  if (!f->open(QIODevice::WriteOnly)) {
    cb({}, "Could not create the server buffer.");
    return;
  }
  f->setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  auto req = request(u);
  if (channel == "prefetch") req.setPriority(QNetworkRequest::LowPriority);
  req.setTransferTimeout(30000);
  auto *r = m_network.get(req);
  m_channels[channel] = r;
  r->setReadBufferSize(256 * 1024);
  const auto generation = m_generation;
  auto failed = std::make_shared<bool>(false);
  auto drain = [r, f, limit, failed] {
    const auto bytes = r->readAll();
    if (f->size() + bytes.size() > limit || f->write(bytes) != bytes.size()) {
      *failed = true;
      r->abort();
    }
  };
  connect(r, &QNetworkReply::readyRead, this, drain);
  auto timer = new QTimer(r);
  timer->setSingleShot(true);
  connect(timer, &QTimer::timeout, r, &QNetworkReply::abort);
  timer->start(180000);
  connect(r, &QNetworkReply::finished, this,
          [this, r, f, drain, failed, generation, path, cb, channel] {
            drain();
            f->close();
            if (m_channels.value(channel) == r)
              m_channels.remove(channel);
            r->deleteLater();
            if (generation != m_generation) {
              f->remove();
              return;
            }
            const auto type =
                r->header(QNetworkRequest::ContentTypeHeader).toString();
            const int status =
                r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (*failed || r->error() != QNetworkReply::NoError ||
                status != 200 || f->size() == 0 || type.contains("json") ||
                type.contains("html") || type.contains("xml")) {
              f->remove();
              cb({}, *failed ? "The server buffer limit was reached or the "
                               "disk is full. Choose a lower bitrate."
                             : networkError(r));
              return;
            }
            cb({{"ok", true}, {"file", path}}, {});
          });
}
void JellyfinApi::download(const QVariantMap &track, const QString &path,
                           Reply cb, const QString &channel) {
  if (!owns(track)) {
    cb({}, "Reconnect to this song's Jellyfin server first.");
    return;
  }
  const auto id = part(track.value("remoteId").toString());
  Params p{{"UserId", m_user}, {"DeviceId", m_device}};
  if (bitrate()) {
    p.append(QPair<QString, QString>{"AudioCodec", "mp3"});
    p.append(QPair<QString, QString>{"AudioBitRate",
                                     QString::number(bitrate() * 1000)});
    p.append(QPair<QString, QString>{"Container", "mp3"});
  } else
    p.append(QPair<QString, QString>{"Static", "true"});
  file(url("Audio/" + id + (bitrate() ? "/stream.mp3" : "/stream"), p), path,
       512LL * 1024 * 1024, cb, channel);
}
void JellyfinApi::cover(const QVariantMap &track, const QString &path, Reply cb,
                        const QString &channel) {
  auto u = artworkUrl(QUrl(track.value("art").toString()));
  if (u.isEmpty()) {
    cb({}, {});
    return;
  }
  const int pixels = channel == "thumb" ? 256 : 1024;
  QUrlQuery query(u);
  query.removeAllQueryItems("MaxWidth");
  query.removeAllQueryItems("MaxHeight");
  query.addQueryItem("MaxWidth", QString::number(pixels));
  query.addQueryItem("MaxHeight", QString::number(pixels));
  u.setQuery(query);
  if (u.isEmpty()) {
    cb({}, {});
    return;
  }
  file(
      u, path + ".source", 4 * 1024 * 1024,
      [path, cb, pixels](const QVariantMap &d, const QString &e) {
        if (!e.isEmpty()) {
          cb({}, e);
          return;
        }
        const auto source = d.value("file").toString();
        QImageReader reader(source);
        const auto size = reader.size();
        QImage image;
        if (size.isValid() && size.width() <= 10000 && size.height() <= 10000) {
          reader.setScaledSize(
              size.scaled(pixels, pixels, Qt::KeepAspectRatio));
          image = reader.read();
        }
        QFile::remove(source);
        QSaveFile f(path);
        if (image.isNull() || !f.open(QIODevice::WriteOnly)) {
          cb({}, "Jellyfin artwork could not be decoded.");
          return;
        }
        f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        if (!image.save(&f, "PNG") || !f.commit()) {
          cb({}, "Jellyfin artwork could not be saved.");
          return;
        }
        cb({{"file", path}}, {});
      },
      channel);
}

bool JellyfinApi::keyringAvailable() const {
  return !QStandardPaths::findExecutable("secret-tool").isEmpty();
}
void JellyfinApi::secret(const QStringList &args, const QByteArray &input,
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

void JellyfinApi::movePlaylistItem(const QString &playlist,
                                   const QString &entry, int index, Reply cb) {
  if (!connected() || playlist.isEmpty() || entry.isEmpty() || index < 0) {
    cb({}, "Refresh the playlist before moving songs.");
    return;
  }
  json("Playlists/" + part(playlist) + "/Items/" + part(entry) + "/Move/" +
           QString::number(index),
       {}, "POST", {}, cb);
}

QUrl JellyfinApi::shareUrl(const QVariantMap &row) const {
  if (!owns(row)) return {};
  QUrl u(address() + "/web/index.html");
  u.setFragment("/details?id=" + QString::fromLatin1(QUrl::toPercentEncoding(row.value("remoteId").toString())));
  return u;
}
