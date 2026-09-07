#include "library.h"
#include "cider.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <QUrlQuery>

Library::Library(Cider *cider, QObject *parent) : QObject(parent), m_cider(cider) {
    m_network.setProxy(QNetworkProxy::NoProxy);
    m_debounce.setSingleShot(true); m_debounce.setInterval(350);
    connect(&m_debounce, &QTimer::timeout, this, &Library::reload);
}
Library::~Library() {
    for (auto reply : {m_reply, m_actionReply}) if (reply) { reply->disconnect(this); reply->abort(); }
}
void Library::setItems(const QVariantList &items, bool append) {
    emit itemsChanging(append); m_items=items; emit itemsChanged();
}
void Library::invalidate() {
    ++m_generation; m_debounce.stop();
    if (m_reply) m_reply->abort();
    m_busy=false; m_loaded=false; m_error.clear(); m_playError.clear(); m_needsConnection=false;
}
void Library::setActive(bool value) {
    if (m_active==value) return;
    m_active=value;
    if (!value) {
        ++m_generation; m_debounce.stop();
        if (m_busy) m_loaded=false;
        if (m_reply) m_reply->abort();
        m_busy=false;
    } else if (!m_loaded || (m_section=="recent" && m_collection.isEmpty())) reload();
    emit changed();
}
void Library::setSection(const QString &value) {
    if (value==m_section || !QStringList{"search","songs","albums","playlists","recent"}.contains(value)) return;
    invalidate(); m_section=value; m_query.clear(); m_collectionQuery.clear(); m_allItems.clear(); m_filterText.clear(); m_collection.clear(); setItems({}); m_next.clear();
    m_savedItems.clear(); m_savedPages.clear();
    emit changed(); if (m_active) reload();
}
void Library::setQuery(const QString &value) {
    const auto query=value.left(2048);
    if (query==m_query || !m_collection.isEmpty()) return;
    if (m_section=="recent") {
        m_query=query; filterItems(); emit changed(); continueSearch(); return;
    }
    invalidate(); m_query=query; setItems({}); m_next.clear(); emit changed();
    if (m_active) { m_busy=true; m_debounce.start(); emit changed(); }
}
bool Library::filteringLocally() const { return !m_collection.isEmpty() || m_section=="recent"; }
void Library::setCollectionQuery(const QString &value) {
    const auto query=value.left(200);
    if (m_collection.isEmpty() || m_collectionQuery==query) return;
    m_collectionQuery=query; filterItems(); emit changed(); continueSearch();
}
void Library::filterItems(bool append) {
    const auto fold=[](QString value) {
        static const QRegularExpression marks("[\\p{M}]");
        value=value.normalized(QString::NormalizationForm_D).toCaseFolded();
        value.remove(marks); return value;
    };
    static const QRegularExpression spaces("\\s+");
    const auto terms=fold(m_collection.isEmpty()?m_query:m_collectionQuery).split(spaces,Qt::SkipEmptyParts);
    if (terms.isEmpty()) { setItems(m_allItems,append); return; }
    // Normalize each loaded row once, on first search. Pagination only extends
    // the index; replacing or leaving the collection clears it.
    m_filterText.reserve(m_allItems.size());
    while (m_filterText.size()<m_allItems.size()) {
        const auto row=m_allItems[m_filterText.size()].toMap();
        m_filterText.append(fold(row["title"].toString()+" "+row["artist"].toString()));
    }
    QVariantList rows;
    for (qsizetype i=0;i<m_allItems.size();++i) {
        const auto &text=m_filterText[i];
        bool matches=true;
        for (const auto &term:terms) if (!text.contains(term)) { matches=false; break; }
        if (matches) rows.append(m_allItems[i]);
    }
    setItems(rows,append);
}
void Library::continueSearch() {
    const auto query=m_collection.isEmpty()?m_query:m_collectionQuery;
    if (!filteringLocally() || !m_active || m_busy || query.trimmed().isEmpty() || m_next.isEmpty() || !m_error.isEmpty()) return;
    const int generation=m_generation;
    QTimer::singleShot(0,this,[this,generation] {
        const auto query=m_collection.isEmpty()?m_query:m_collectionQuery;
        if (generation==m_generation && filteringLocally() && m_active && !m_busy && !query.trimmed().isEmpty() && !m_next.isEmpty() && m_error.isEmpty()) more();
    });
}
void Library::setKind(const QString &value) {
    if (value==m_kind || !QStringList{"songs","albums","playlists"}.contains(value)) return;
    invalidate(); m_kind=value; setItems({}); m_next.clear(); emit changed(); if (m_active) reload();
}
// Resolve only explicit Apple Music catalog URLs. Never follow a pasted URL or
// forward it (or the Cider token) to an arbitrary origin.
QString Library::linkPath(const QString &text) {
    if (text.size()>2048) return {};
    const QUrl url(text.trimmed(),QUrl::StrictMode);
    if (!url.isValid() || url.scheme()!="https" || url.host()!="music.apple.com" ||
        !url.userInfo().isEmpty() || (url.port()!=-1 && url.port()!=443)) return {};
    const auto parts=url.path().split('/',Qt::SkipEmptyParts);
    if (parts.contains(".") || parts.contains("..") || parts.size()<3 || parts.size()>4 || !QRegularExpression("^[a-z]{2}$").match(parts[0]).hasMatch()) return {};
    QString type=parts[1], id=parts.last();
    if (!QStringList{"song","album","playlist"}.contains(type)) return {};
    const QUrlQuery query(url);
    if (type=="album" && query.hasQueryItem("i")) { type="song"; id=query.queryItemValue("i"); }
    const auto pattern=type=="playlist" ? "^pl\\.[A-Za-z0-9.-]+$" : "^[0-9]+$";
    if (!QRegularExpression(pattern).match(id).hasMatch()) return {};
    return "/v1/catalog/"+parts[0]+"/"+type+"s/"+id;
}
bool Library::openLink(const QString &text) {
    if (linkPath(text).isEmpty()) return false;
    if (!m_collection.isEmpty()) back();
    setSection("search");
    setQuery(text.trimmed());
    if (m_active) reload();
    return true;
}
bool Library::openClipboardLink() { return openLink(QGuiApplication::clipboard()->text()); }
QString Library::resultType() const {
    if (!m_collection.isEmpty()) return m_collection["type"].toString().startsWith("library-") ? "library-songs" : "songs";
    if (m_section=="recent") return "songs";
    return m_section=="search" ? m_kind : "library-"+m_section;
}
QString Library::listPath() const {
    if (!m_collection.isEmpty()) return m_collection["path"].toString()+"/tracks?limit=50";
    if (m_section=="recent") return "/v1/me/recent/played/tracks?types=songs,library-songs&limit=30";
    if (m_section=="search" && !linkPath(m_query).isEmpty()) return linkPath(m_query);
    if (m_query.trimmed().isEmpty()) return m_section=="search" ? QString() : "/v1/me/library/"+m_section+"?limit=50";
    QUrl url(m_section=="search" ? "/v1/catalog/"+m_storefront+"/search" : "/v1/me/library/search");
    QUrlQuery query; query.addQueryItem("term",m_query.trimmed().left(200)); query.addQueryItem("types",resultType()); query.addQueryItem("limit","25");
    url.setQuery(query); return url.toString(QUrl::FullyEncoded);
}
void Library::request(const QString &path, const QJsonObject &body, bool action, std::function<void(QJsonObject)> done) {
    auto url=m_cider->m_rpcBase; url.setPath(path); url.setQuery(QString());
    QNetworkRequest req(url); req.setTransferTimeout(10000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    req.setRawHeader("apptoken",m_cider->m_apiToken.toUtf8());
    auto *reply=m_network.post(req,QJsonDocument(body).toJson(QJsonDocument::Compact));
    if (action) m_actionReply=reply; else m_reply=reply;
    const int generation=m_generation;
    connect(reply,&QNetworkReply::readyRead,reply,[reply] { if (reply->bytesAvailable()>4*1024*1024) reply->abort(); });
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,action,done] {
        reply->deleteLater();
        if (!action && generation!=m_generation) return;
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto json=QJsonDocument::fromJson(reply->readAll());
        auto object=json.object();
        const bool upstreamError=object.contains("errors") || object.value("data").toObject().contains("errors");
        if (reply->error()!=QNetworkReply::NoError || status<200 || status>=300 || !json.isObject() || upstreamError) {
            QString message;
            if (status==401 || status==403) { message="Check Spun’s access token in Cider."; m_needsConnection=true; }
            else if (status==422) message="No playable tracks in this collection.";
            else if (status==404) message="This item is no longer available.";
            else if (reply->error()==QNetworkReply::ConnectionRefusedError) message="Open Cider to browse your music.";
            else message=action ? "Cider couldn’t start playback. Try again." : "Couldn’t load music. Try again.";
            if (action) { m_starting=false; m_playError=message; }
            else { m_busy=false; m_error=message; }
            emit changed(); return;
        }
        done(object);
    });
}
void Library::reload() {
    if (!m_active) return;
    invalidate(); m_allItems.clear(); m_filterText.clear(); setItems({}); m_next.clear(); m_pages.clear();
    if (m_cider->m_apiToken.isEmpty()) {
        m_needsConnection=true; m_error="Connect Spun to Cider to browse music."; emit changed(); return;
    }
    if (m_section=="search" && m_collection.isEmpty() && m_query.trimmed().contains("://") && linkPath(m_query).isEmpty()) {
        m_error="Use an Apple Music song, album or playlist link."; emit changed(); return;
    }
    m_busy=true; emit changed();
    if (m_section=="search" && !m_query.trimmed().isEmpty() && linkPath(m_query).isEmpty() && m_storefront.isEmpty() && m_collection.isEmpty()) {
        request("/api/v1/amapi/run-v3",{{"path","/v1/me/storefront"}},false,[this](QJsonObject response) {
            const auto data=response.value("data").toObject().value("data").toArray();
            const auto storefront=data.isEmpty() ? QString() : data.first().toObject().value("id").toString();
            if (!QRegularExpression("^[a-z]{2}$").match(storefront).hasMatch()) {
                m_busy=false; m_error="Sign in to Apple Music in Cider."; emit changed(); return;
            }
            m_storefront=storefront; fetch(listPath());
        });
    } else fetch(listPath());
}
QVariantMap Library::item(const QJsonObject &value, const QString &fallbackType) {
    static const QRegularExpression validId("^[A-Za-z0-9._-]+$");
    const auto attrs=value.value("attributes").toObject();
    const auto type=value.value("type").toString(fallbackType), id=value.value("id").toString();
    if (!validId.match(id).hasMatch() ||
        !QStringList{"songs","albums","playlists","library-songs","library-albums","library-playlists"}.contains(type)) return {};
    QString artwork=attrs.value("artwork").toObject().value("url").toString();
    artwork.replace("{w}","160").replace("{h}","160").replace("{f}","jpg");
    const QUrl artUrl(artwork);
    if (artUrl.scheme()!="https") artwork.clear();
    const auto playParams=attrs.value("playParams").toObject();
    return {{"id",id},{"type",type},{"title",attrs.value("name").toString()},
        {"artist",attrs.value("artistName").toString(attrs.value("curatorName").toString())},
        {"artwork",artwork},{"year",attrs.value("releaseDate").toString().left(4)},
        {"duration",attrs.value("durationInMillis").toInteger()},
        {"trackCount",attrs.value("trackCount").toInt(-1)},
        {"playable",!playParams.isEmpty()}, {"path",value.value("href").toString()}};
}
void Library::fetch(const QString &path, bool append) {
    if (path.isEmpty()) { m_busy=false; m_loaded=true; emit changed(); return; }
    const QUrl target(path);
    const QString linked=m_section=="search" && m_collection.isEmpty() ? linkPath(m_query) : QString();
    const QString base=m_section=="recent" && m_collection.isEmpty() ? "/v1/me/recent/played/tracks" : !linked.isEmpty() ? linked : m_collection.isEmpty() ? (m_section=="search" ? "/v1/catalog/"+m_storefront+"/search" : "/v1/me/library/"+(m_query.trimmed().isEmpty() ? m_section : "search")) : m_collection["path"].toString()+"/tracks";
    if (!target.isRelative() || target.hasFragment() || target.path()!=base || m_pages.contains(path)) {
        m_busy=false; m_error="Couldn’t load the next page."; m_next.clear(); emit changed(); return;
    }
    m_busy=true; m_error.clear(); emit changed();
    request("/api/v1/amapi/run-v3",{{"path",path}},false,[this,path,append,linked](QJsonObject response) {
        auto data=response.value("data").toObject();
        const bool searchResponse=data.value("results").isObject();
        if (searchResponse) data=data.value("results").toObject().value(resultType()).toObject();
        // Search may legitimately omit a category with no matches.
        if ((!searchResponse || !data.isEmpty()) && !data.value("data").isArray()) {
            m_busy=false; m_error="Cider returned an unreadable music list."; emit changed(); return;
        }
        QVariantList rows=append ? (filteringLocally()?m_allItems:m_items) : QVariantList{};
        for (const auto &value : data.value("data").toArray()) {
            auto row=item(value.toObject(),resultType()); if (row.isEmpty()) continue;
            const auto type=row["type"].toString();
            const QString root=type.startsWith("library-") ? "/v1/me/library/"+type.mid(8) : "/v1/catalog/"+(!linked.isEmpty()?linked.split('/').value(3):!m_collection.isEmpty()?m_collection["path"].toString().split('/').value(3):m_storefront)+"/"+type;
            row["path"]=root+"/"+row["id"].toString();
            if (m_section=="recent" && m_collection.isEmpty()) {
                static const QRegularExpression catalogSong("^/v1/catalog/[a-z]{2}/songs/[A-Za-z0-9._-]+$");
                const auto href=value.toObject().value("href").toString();
                // History can mix catalog and library songs from a different storefront.
                const auto expected=type=="library-songs" ? "/v1/me/library/songs/"+row["id"].toString() : QString();
                row["path"]=type=="library-songs" ? expected :
                    catalogSong.match(href).hasMatch() ? href : QString();
            }
            rows.append(row);
        }
        if (filteringLocally()) { if (!append) m_filterText.clear(); m_allItems=rows; filterItems(append); }
        else setItems(rows,append);
        m_next=linked.isEmpty() ? data.value("next").toString() : QString(); m_pages.insert(path);
        m_busy=false; m_loaded=true; emit changed(); continueSearch();
    });
}
void Library::more() { if (m_active && !m_busy && !m_next.isEmpty()) fetch(m_next,true); }
void Library::open(int index) {
    if (index<0 || index>=m_items.size() || m_busy || !m_collection.isEmpty()) return;
    const auto selected=m_items[index].toMap();
    if (selected["type"].toString().endsWith("songs")) { play(index); return; }
    m_savedItems=m_items; m_savedNext=m_next; m_savedPages=m_pages;
    m_collectionQuery.clear(); m_collection=selected; reload();
}
void Library::back() {
    if (m_collection.isEmpty()) return;
    invalidate(); m_collectionQuery.clear(); m_allItems.clear(); m_filterText.clear(); m_collection.clear(); setItems(m_savedItems); m_next=m_savedNext; m_pages=m_savedPages;
    m_loaded=true; emit changed(); emit returned();
}
void Library::play(int index) { if (m_active && index>=0 && index<m_items.size()) start(m_items[index].toMap()); }
void Library::playCollection() { if (m_active && !m_collection.isEmpty()) start(m_collection); }
void Library::start(const QVariantMap &selected) {
    if (m_starting || !selected["playable"].toBool()) return;
    const bool song=selected["type"].toString().endsWith("songs");
    m_starting=true; m_playError.clear(); emit changed();
    request(song ? "/api/v2/playback/play-item" : "/api/v2/playback/play-collection",
        {{"type",selected["type"].toString()},{"id",selected["id"].toString()}},true,[this](QJsonObject) {
            m_starting=false; emit changed();
            QTimer::singleShot(600,m_cider,[cider=m_cider] { cider->refresh(); if (cider->queueVisible()) cider->refreshQueue(); });
        });
}
