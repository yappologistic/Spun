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
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QUuid>
#include <QSettings>
#include <algorithm>

Library::Library(Cider *cider, QObject *parent) : QObject(parent), m_cider(cider) {
    if (!cider->m_connectionPath.isEmpty()) {
        m_sortSettings=QFileInfo(cider->m_connectionPath).dir().filePath("settings.ini");
        for (const auto &term:QSettings(m_sortSettings,QSettings::IniFormat).value("recentSearches").toStringList().mid(0,8))
            if (!term.trimmed().isEmpty() && term.size()<=200 && !m_recentSearches.contains(term)) m_recentSearches.append(term);
        m_newestFirst=QSettings(m_sortSettings,QSettings::IniFormat).value("libraryNewestFirst",false).toBool();
        m_sessionsDir=QFileInfo(cider->m_connectionPath).dir().filePath("saved-queues");
        m_pinsPath=QFileInfo(cider->m_connectionPath).dir().filePath("cider-pins.json");
        QFile file(m_pinsPath);
        if (file.open(QIODevice::ReadOnly) && file.size()<=1024*1024) {
            for (const auto &entry:QJsonDocument::fromJson(file.readAll()).array()) {
                const auto pin=validatedPin(entry.toObject().toVariantMap());
                if (!pin.isEmpty() && !isPinned(pin) && m_pins.size()<200) m_pins.append(pin);
            }
        }
    }
    connect(cider,&Cider::connectionRestored,this,[this] { if(m_active && (m_needsConnection || !m_error.isEmpty())) reload(); });
    m_savedUndoTimer.setSingleShot(true);m_savedUndoTimer.setInterval(8000);
    connect(&m_savedUndoTimer,&QTimer::timeout,this,&Library::clearSavedUndo);
    connect(this,&Library::changed,this,[this] { if(preparingTail()) QTimer::singleShot(0,this,&Library::continueTail); });
    m_network.setProxy(QNetworkProxy::NoProxy);
    m_debounce.setSingleShot(true); m_debounce.setInterval(350);
    connect(&m_debounce, &QTimer::timeout, this, &Library::reload);
}
Library::~Library() {
    clearSavedUndo();
    for (auto reply : {m_reply, m_actionReply, m_radioReply}) if (reply) { reply->disconnect(this); reply->abort(); }
}
void Library::setItems(const QVariantList &items, bool append) {
    emit itemsChanging(append || m_preservePosition); m_items=items; emit itemsChanged();
}
void Library::invalidate() {
    cancelQueueFromHere();
    ++m_generation; m_debounce.stop(); m_preservePosition=false;
    if (m_reply) m_reply->abort();
    m_busy=false; m_loaded=false; m_error.clear(); m_playError.clear(); m_needsConnection=false;
}
void Library::setActive(bool value) {
    if (m_active==value) return;
    m_active=value; m_cider->setLibraryVisible(value);
    if (!value) {
        cancelQueueFromHere();
        ++m_generation; m_debounce.stop();
        if (m_busy) m_loaded=false;
        if (m_reply) m_reply->abort();
        m_busy=false;
    } else if (!m_loaded || (m_section=="recent" && m_collection.isEmpty())) reload();
    emit changed();
}
void Library::setSection(const QString &value) {
    if (value==m_section || !QStringList{"search","songs","albums","playlists","recent","sessions","releases","for-you"}.contains(value)) return;
    invalidate(); m_findArtist=false; m_section=value; m_query.clear(); m_collectionQuery.clear(); m_allItems.clear(); m_filterText.clear(); m_collection.clear(); setItems({}); m_next.clear();
    m_history.clear(); emit navigationReset();
    emit changed(); if (m_active) {
        if(value=="releases" && m_releaseClock.isValid() && m_releaseClock.elapsed()<3600000 && m_releaseTokenGeneration==m_cider->m_tokenGeneration) {
            m_allItems=m_releaseCache; m_filterText.clear(); filterItems(); m_loaded=true; m_releaseNotice.clear(); emit changed();
        } else reload();
    }
}
void Library::setQuery(const QString &value) {
    const auto query=value.left(2048);
    if (query==m_query || !m_collection.isEmpty()) return;
    if (m_section=="recent" || m_section=="sessions" || m_section=="releases" || m_section=="for-you") {
        m_query=query; filterItems(); emit changed(); continueSearch(); return;
    }
    invalidate(); m_findArtist=false; m_query=query; setItems({}); m_next.clear(); emit changed();
    if (m_active) { m_busy=true; m_debounce.start(); emit changed(); }
}
bool Library::filteringLocally() const { return !m_collection.isEmpty() || m_section=="recent" || m_section=="sessions" || m_section=="releases" || m_section=="for-you"; }
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
        m_filterText.append(fold(row["title"].toString()+" "+row["artist"].toString()+" "+row["recommendation"].toString()));
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
    if (value==m_kind || !QStringList{"songs","albums","playlists","artists","stations"}.contains(value)) return;
    invalidate(); m_findArtist=false; m_kind=value; m_history.clear(); emit navigationReset(); setItems({}); m_next.clear(); emit changed(); if (m_active) reload();
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
    if (!QStringList{"song","album","playlist","station"}.contains(type)) return {};
    const QUrlQuery query(url);
    if (type=="album" && query.hasQueryItem("i")) { type="song"; id=query.queryItemValue("i"); }
    const auto pattern=type=="station" ? "^ra\\.[A-Za-z0-9.-]+$" : type=="playlist" ? "^pl\\.[A-Za-z0-9.-]+$" : "^[0-9]+$";
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
    if (m_collection.value("type")=="artists") return m_artistView=="similar" ? "artists" : m_artistView;
    if (!m_collection.isEmpty()) return m_collection["type"].toString().startsWith("library-") ? "library-songs" : "songs";
    if (m_section=="recent") return "songs";
    return m_section=="search" ? m_kind : "library-"+m_section;
}
QString Library::listPath() const {
    if (!m_collection.isEmpty()) return collectionPath()+(m_collection.value("type")=="artists" && m_artistView=="songs"?"?limit=25":"?limit=50");
    if (m_section=="recent") return "/v1/me/recent/played/tracks?types=songs,library-songs&limit=30";
    if (m_section=="search" && !linkPath(m_query).isEmpty()) return linkPath(m_query);
    if (m_query.trimmed().isEmpty()) return m_section=="search" ? QString() : "/v1/me/library/"+m_section+"?limit=50"+(m_newestFirst && (m_section=="songs" || m_section=="albums")?"&sort=-dateAdded":"");
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
    const bool trackRelationship=!action && path=="/api/v1/amapi/run-v3" && QRegularExpression("^/v1/me/library/(albums|playlists)/[A-Za-z0-9._-]+/tracks$").match(QUrl(body.value("path").toString()).path()).hasMatch();
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,action,done,trackRelationship] {
        reply->deleteLater();
        if (!action && generation!=m_generation) return;
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // A denied library capability does not invalidate working playback/queue access.
        // Unauthorized tokens and transport outages still update the shared connection.
        if(status!=403 || action)m_cider->observeConnection(status,reply->error());
        const auto json=QJsonDocument::fromJson(reply->readAll());
        auto object=json.object();
        const auto errors=object.value("data").toObject().value("errors").toArray();
        if(trackRelationship && status==200 && !errors.isEmpty() && std::all_of(errors.begin(),errors.end(),[](const QJsonValue &error){return error.toObject().value("code")=="40403";}))
            object["data"]=QJsonObject{{"data",QJsonArray{}}};
        const bool upstreamError=object.contains("errors") || object.value("data").toObject().contains("errors");
        if (reply->error()!=QNetworkReply::NoError || status<200 || status>=300 || !json.isObject() || upstreamError) {
            QString message;
            if (status==401 || status==403) { message="Reconnect to approve Spun’s access in Cider."; m_needsConnection=true; }
            else if(status==404 && m_section=="for-you" && m_collection.isEmpty()) message="For You isn’t available through this Cider connection.";
            else if (status==422) message="No playable tracks in this collection.";
            else if(status==404 && m_collection.value("type")=="artists" && m_artistView=="albums" && m_discography!="all") message="This release category isn’t available through Cider.";
            else if (status==404) message=m_collection.value("type")=="artists" && m_artistView=="songs" ? "Top songs aren’t available through this Cider connection." : "This item is no longer available.";
            else if (!status) message=m_cider->connectionMessage();
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
    invalidate(); m_preservePosition=!m_items.isEmpty(); m_pages.clear();
    if(m_section=="releases" && m_collection.isEmpty()) { fetchReleases(); return; }
    if(m_section=="for-you" && m_collection.isEmpty()) { m_recommendationPages.clear();m_next.clear();fetchRecommendations("/v1/me/recommendations?limit=10",false);return; }
    if(m_section=="sessions") {
        bool valid=true;
        const auto sessions=readSessions(&valid);
        if(m_collection.isEmpty())m_allItems=sessions;
        else {
            const auto id=m_collection.value("id").toString();QVariantMap current;
            for(const auto &entry:sessions)if(entry.toMap().value("id")==id)current=entry.toMap();
            m_allItems.clear();
            if(current.isEmpty())m_error="This saved queue is no longer available.";
            else { m_collection=current;m_allItems=savedTracks(id);
                for(int i=0;i<m_allItems.size();++i) { auto row=m_allItems[i].toMap();row["savedIndex"]=i;row["collectionIndex"]=i;m_allItems[i]=row; }
            }
        }
        if(!valid)m_error="Couldn’t read the saved queue list.";
        m_filterText.clear();filterItems();m_next.clear();m_loaded=true;m_preservePosition=false;emit changed();return;
    }
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
        !QStringList{"songs","albums","playlists","library-songs","library-albums","library-playlists","artists","stations"}.contains(type)) return {};
    QString artwork=attrs.value("artwork").toObject().value("url").toString();
    artwork.replace("{w}","160").replace("{h}","160").replace("{f}","jpg");
    const QUrl artUrl(artwork);
    if (artUrl.scheme()!="https") artwork.clear();
    const auto playParams=attrs.value("playParams").toObject();
    return {{"id",id},{"type",type},{"title",attrs.value("name").toString()},
        {"artist",type=="artists"?attrs.value("genreNames").toVariant().toStringList().join(" · "):attrs.value("artistName").toString(attrs.value("curatorName").toString())},
        {"artwork",artwork},{"year",attrs.value("releaseDate").toString().left(4)},
        {"duration",attrs.value("durationInMillis").toInteger()},{"releaseDate",attrs.value("releaseDate").toString()},
        {"trackCount",attrs.value("trackCount").toInt(-1)},
        {"catalogId",playParams.value("catalogId").toString()},{"url",attrs.value("url").toString()},
        {"playable",!playParams.isEmpty() && !(type.endsWith("albums") && attrs.value("trackCount").toInt(-1)==0)}, {"path",value.value("href").toString()}};
}
void Library::fetch(const QString &path, bool append) {
    if (path.isEmpty()) { m_busy=false; m_loaded=true; m_next.clear(); setItems({}); m_preservePosition=false; emit changed(); return; }
    const QUrl target(path);
    const QString linked=m_section=="search" && m_collection.isEmpty() ? linkPath(m_query) : QString();
    const QString base=m_section=="recent" && m_collection.isEmpty() ? "/v1/me/recent/played/tracks" : !linked.isEmpty() ? linked : m_collection.isEmpty() ? (m_section=="search" ? "/v1/catalog/"+m_storefront+"/search" : "/v1/me/library/"+(m_query.trimmed().isEmpty() ? m_section : "search")) : collectionPath();
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
            if(tailCollection())row["collectionIndex"]=rows.size();
            rows.append(row);
        }
        if (filteringLocally()) { if (!append) m_filterText.clear(); m_allItems=rows; filterItems(append); }
        else setItems(rows,append);
        m_next=linked.isEmpty() ? data.value("next").toString() : QString(); m_pages.insert(path);
        m_preservePosition=false; m_busy=false; m_loaded=true; emit changed(); continueSearch();
        if(m_findArtist && m_collection.isEmpty()) {
            m_findArtist=false; int match=-1, count=0;
            for(int i=0;i<m_items.size();++i) if(m_items[i].toMap().value("title").toString().compare(m_query.trimmed(),Qt::CaseInsensitive)==0) { match=i; ++count; }
            if(count==1)open(match);
        }
    });
}
void Library::more() {
    if(!m_active || m_busy || m_next.isEmpty())return;
    if(m_section=="for-you" && m_collection.isEmpty()) {
        if(!m_recommendationPages.isEmpty()) {const auto page=m_recommendationPages.first().toMap();fetchRecommendations(page.value("path").toString(),true,page.value("reason").toString());}
        return;
    }
    QUrl next(m_next);
    if(m_newestFirst && m_collection.isEmpty() && m_query.trimmed().isEmpty() && (m_section=="songs" || m_section=="albums")) {
        QUrlQuery query(next);query.removeAllQueryItems("sort");query.addQueryItem("sort","-dateAdded");next.setQuery(query);
    }
    fetch(next.toString(QUrl::FullyEncoded),true);
}
void Library::pushPage() {
    if(m_history.size()>=16)m_history.removeFirst();
    m_history.append({m_collection,m_items,m_allItems,m_next,m_collectionQuery,m_artistView,m_discography,m_recommendationPages,m_pages}); emit navigating();
}
void Library::open(int index) {
    if(index<0 || index>=m_items.size() || m_busy)return;
    const auto selected=m_items[index].toMap();
    if(selected.value("type").toString().endsWith("songs") || selected.value("type")=="stations") {play(index);return;}
    if(!m_collection.isEmpty() && m_collection.value("type")!="artists")return;
    rememberSearch();
    pushPage(); m_collectionQuery.clear(); m_collection=selected; if(selected.value("type")=="artists") {m_artistView="albums";m_discography="all";} setItems({}); reload();
}
void Library::back() {
    if(m_history.isEmpty())return;
    invalidate(); const auto page=m_history.takeLast();
    m_collection=page.collection; m_artistView=page.artistView; m_discography=page.discography;m_recommendationPages=page.recommendationPages; m_collectionQuery=page.collectionQuery; m_allItems=page.allItems; m_filterText.clear();
    setItems(page.items); m_next=page.next; m_pages=page.pages; m_loaded=true;
    if(m_section=="sessions" && m_collection.isEmpty())reload();
    emit changed(); emit returned();
}
void Library::showArtist(const QString &name) {
    if(name.trimmed().isEmpty())return;
    invalidate(); m_history.clear();emit navigationReset();
    m_collection.clear();m_collectionQuery.clear();m_allItems.clear();m_filterText.clear();m_next.clear();setItems({});
    m_section="search";m_kind="artists";m_query=name.trimmed().left(200);m_findArtist=true;emit changed();reload();
}
bool Library::tailCollection() const {
    const auto type=m_collection.value("type").toString();
    return type.endsWith("albums") || type.endsWith("playlists") || type=="saved-queues";
}
void Library::cancelQueueFromHere() {
    if(!preparingTail())return;
    m_tailIndex=-1;m_tailAnchor.clear();emit changed();
}
void Library::queueFromHere(const QVariantMap &selected) {
    if(!m_active || preparingTail() || m_starting || m_cider->controlBusy() || !tailCollection() || !selected.contains("collectionIndex"))return;
    const int index=selected.value("collectionIndex").toInt();
    if(index<0 || index>=m_allItems.size() || m_allItems[index].toMap()!=selected)return;
    m_tailIndex=index;m_tailAnchor=selected;emit feedback("Loading remaining tracks…",false);emit changed();continueTail();
}
void Library::continueTail() {
    if(!preparingTail() || m_busy)return;
    if(!m_error.isEmpty() || m_tailIndex>=m_allItems.size() || m_allItems[m_tailIndex].toMap()!=m_tailAnchor) {
        cancelQueueFromHere();emit feedback("Couldn’t read the remaining tracks. Nothing was queued.",true);return;
    }
    if(m_allItems.size()-m_tailIndex>5000) { cancelQueueFromHere();emit feedback("Select a smaller range; the limit is 5,000 tracks.",true);return; }
    if(!m_next.isEmpty()) { more();return; }
    const auto tracks=m_allItems.mid(m_tailIndex);cancelQueueFromHere();
    for(const auto &value:tracks)if(!value.toMap().value("playable").toBool()) {
        emit feedback("This range includes unavailable songs. Select available tracks instead.",true);return;
    }
    emit tailReady(tracks);
}

void Library::play(int index) { if (m_active && index>=0 && index<m_items.size()) start(m_items[index].toMap()); }
void Library::playCollection() { if (m_active && !m_collection.isEmpty()) start(m_collection); }
void Library::shuffleCollection(const QVariantMap &selected) {
    if (!m_active || validatedPin(selected).isEmpty() || selected.value("type")=="stations") return;
    start(selected,true);
}
void Library::start(const QVariantMap &selected, bool shuffle) {
    if (m_starting || m_radioStarting || m_cider->controlBusy() || selected.value("type")=="artists" || !selected["playable"].toBool()) return;
    if(selected.value("type")=="stations") {startStation(selected,false);return;}
    const bool song=selected["type"].toString().endsWith("songs");
    rememberSearch();
    m_starting=true; m_playError.clear(); emit changed();
    QJsonObject body{{"type",selected["type"].toString()},{"id",selected["id"].toString()}};
    if(!song)body["shuffle"]=shuffle;
    request(song ? "/api/v2/playback/play-item" : "/api/v2/playback/play-collection",body,true,[this](QJsonObject) {
            m_starting=false; emit changed();
            QTimer::singleShot(600,m_cider,[cider=m_cider] { cider->refresh(); if (cider->queueVisible()) cider->refreshQueue(); });
        });
}

QVariantMap Library::validatedPin(const QVariantMap &row) {
    const auto type=row.value("type").toString(), id=row.value("id").toString(), path=row.value("path").toString();
    static const QRegularExpression identifier("^[A-Za-z0-9._-]{1,200}$");
    if (!QStringList{"albums","playlists","library-albums","library-playlists","stations","artists"}.contains(type) || !identifier.match(id).hasMatch()) return {};
    const QString prefix=type.startsWith("library-")?"/v1/me/library/"+type.mid(8):"/v1/catalog/"+path.split('/').value(3)+"/"+type;
    if (path!=prefix+"/"+id || (!type.startsWith("library-") && !QRegularExpression("^[a-z]{2}$").match(path.split('/').value(3)).hasMatch())) return {};
    QVariantMap result;
    for (const auto &key:QStringList{"id","type","path","title","artist","year","trackCount","playable"}) result[key]=row.value(key);
    result["title"]=row.value("title").toString().left(300); result["artist"]=row.value("artist").toString().left(300);
    const QUrl art(row.value("artwork").toString());
    result["artwork"]=art.scheme()=="https" && art.userInfo().isEmpty()?art.toString().left(4096):QString();
    return result;
}
bool Library::isPinned(const QVariantMap &row) const {
    for (const auto &entry:m_pins) if(entry.toMap().value("path")==row.value("path")) return true;
    return false;
}
void Library::togglePin(const QVariantMap &row) {
    const auto pin=validatedPin(row); if(pin.isEmpty())return;
    auto next=m_pins; bool removed=false;
    for (qsizetype i=0;i<next.size();++i) if(next[i].toMap().value("path")==pin.value("path")) { next.removeAt(i); removed=true; break; }
    if (!removed) {
        if(next.size()>=200) { m_playError="Your pins are full. Unpin an item first."; emit changed(); return; }
        next.append(pin);
    }
    if (!m_pinsPath.isEmpty()) {
        QDir().mkpath(QFileInfo(m_pinsPath).absolutePath()); QSaveFile file(m_pinsPath);
        const auto bytes=QJsonDocument(QJsonArray::fromVariantList(next)).toJson(QJsonDocument::Compact);
        if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner) || file.write(bytes)!=bytes.size() || !file.commit()) {
            m_playError="Couldn’t save your pins. Try again."; emit changed(); return;
        }
    }
    m_pins=next; m_releaseClock.invalidate(); emit pinsChanged();
}
void Library::playPin(int index) {
    if(!m_active || index<0 || index>=m_pins.size())return;
    if(m_pins[index].toMap().value("type")=="artists")openPin(index);else start(m_pins[index].toMap());
}
void Library::openPin(int index) {
    if (!m_active || m_busy || !m_collection.isEmpty() || index<0 || index>=m_pins.size())return;
    if(m_pins[index].toMap().value("type")=="stations") { playPin(index); return; }
    pushPage();
    m_collectionQuery.clear(); m_collection=m_pins[index].toMap();
    if(m_collection.value("type")=="artists")m_artistView="songs";
    setItems({}); reload();
}

void Library::setNewestFirst(bool value) {
    if(m_newestFirst==value)return;
    m_newestFirst=value;
    if(!m_sortSettings.isEmpty()) { QSettings settings(m_sortSettings,QSettings::IniFormat);settings.setValue("libraryNewestFirst",value); }
    if(m_collection.isEmpty() && (m_section=="songs" || m_section=="albums")) { invalidate();setItems({});m_next.clear();emit changed();reload(); }
    else emit changed();
}
QString Library::songLink(const QString &url) {
    const auto path=linkPath(url);
    if(!path.contains("/songs/"))return {};
    return "https://music.apple.com/"+path.split('/').value(3)+"/song/"+path.section('/',-1);
}
void Library::copyLink(const QVariantMap &row) {
    QString link=songLink(row.value("url").toString());
    const auto type=row.value("type").toString();
    const auto id=type=="songs"?row.value("id").toString():row.value("catalogId").toString();
    if(link.isEmpty() && type.endsWith("songs") && QRegularExpression("^[0-9]+$").match(id).hasMatch()) {
        const auto path=row.value("path").toString();
        const auto store=path.startsWith("/v1/catalog/")?path.split('/').value(3):m_storefront;
        if(QRegularExpression("^[a-z]{2}$").match(store).hasMatch())link="https://music.apple.com/"+store+"/song/"+id;
        else if(!m_busy) {
            m_busy=true;emit changed();
            request("/api/v1/amapi/run-v3",{{"path","/v1/me/storefront"}},false,[this,row](QJsonObject response) {
                m_busy=false;const auto rows=response.value("data").toObject().value("data").toArray();
                const auto store=rows.isEmpty()?QString():rows.first().toObject().value("id").toString();
                if(!QRegularExpression("^[a-z]{2}$").match(store).hasMatch()) { emit m_cider->apiFeedback("No shareable Apple Music link for this song.",true);emit changed();return; }
                m_storefront=store;copyLink(row);emit changed();
            });return;
        } else { emit m_cider->apiFeedback("Wait for the music list to finish loading, then copy again.",true);return; }
    }
    if(link.isEmpty()) { emit m_cider->apiFeedback("No shareable Apple Music link for this song.",true);return; }
    QGuiApplication::clipboard()->setText(link);emit m_cider->apiFeedback("Song link copied",false);
}

static bool sessionId(const QString &id) {
    static const QRegularExpression valid("^[a-f0-9]{32}$");return valid.match(id).hasMatch();
}
static QVariantMap savedSong(const QVariantMap &row) {
    const auto type=row.value("type").toString(),id=row.value("id").toString();
    static const QRegularExpression valid("^[A-Za-z0-9._-]{1,200}$");
    if(!QStringList{"songs","library-songs"}.contains(type) || !valid.match(id).hasMatch() || !row.value("playable").toBool())return {};
    QVariantMap song{{"id",id},{"type",type},{"playable",true}};
    for(const auto &key:QStringList{"title","artist","catalogId"})song[key]=row.value(key).toString().left(300);
    song["duration"]=qBound<qint64>(qint64(0),row.value("duration").toLongLong(),qint64(86400000));
    const QUrl art(row.value("artwork").toString());song["artwork"]=art.scheme()=="https"&&art.userInfo().isEmpty()?art.toString().left(4096):QString();
    song["url"]=Library::songLink(row.value("url").toString());return song;
}
QVariantList Library::readSessions(bool *valid) const {
    if(valid)*valid=true;
    QVariantList rows;QFile file(m_sessionsDir+"/index.json");
    if(m_sessionsDir.isEmpty() || !file.exists())return rows;
    const auto fail=[valid] { if(valid)*valid=false;return QVariantList{}; };
    if(!file.open(QIODevice::ReadOnly) || file.size()>128*1024)return fail();
    const auto doc=QJsonDocument::fromJson(file.readAll());
    if(!doc.isArray() || doc.array().size()>30)return fail();
    QSet<QString> ids,dataIds;
    for(const auto &value:doc.array()) {
        const auto row=value.toObject();const auto id=row.value("id").toString();
        const int count=row.value("trackCount").toInt();
        const auto dataId=row.value("dataId").toString(id);
        if(!sessionId(id) || !sessionId(dataId) || ids.contains(id) || dataIds.contains(dataId) || !row.value("trackCount").isDouble() || row.value("trackCount").toDouble()!=count || count<0 || count>5000)return fail();
        ids.insert(id);dataIds.insert(dataId);
        rows.append(QVariantMap{{"id",id},{"type","saved-queues"},{"title",row.value("title").toString().left(80)},
            {"artist",QString()},{"dataId",dataId},{"trackCount",count},{"playable",count>0},{"artwork",QString()}});
    }return rows;
}
bool Library::writeSessions(const QVariantList &rows) {
    if(m_sessionsDir.isEmpty() || !QDir().mkpath(m_sessionsDir))return false;
    QSaveFile file(m_sessionsDir+"/index.json");const auto bytes=QJsonDocument(QJsonArray::fromVariantList(rows)).toJson(QJsonDocument::Compact);
    return file.open(QIODevice::WriteOnly)&&file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner)&&file.write(bytes)==bytes.size()&&file.commit();
}
bool Library::saveQueue(const QString &name) {
    if(m_sessionsDir.isEmpty() || name.trimmed().isEmpty() || !m_cider->queueReady() || m_cider->queueBusy() || !m_cider->queueError().isEmpty())return false;
    bool valid=true;auto sessions=readSessions(&valid);
    if(!valid) { emit m_cider->apiFeedback("Couldn’t read saved queues. Your existing files were left intact.",true);return false; }
    if(sessions.size()>=30) { emit m_cider->apiFeedback("Saved queues are full. Delete one first.",true);return false; }
    QVariantList tracks;const auto queue=m_cider->queue();
    for(int i=qMax(0,m_cider->currentIndex());i<queue.size();++i) {
        const auto song=savedSong(queue[i].toMap());
        if(song.isEmpty() || tracks.size()>=5000) { emit m_cider->apiFeedback("This queue is too large or includes tracks Cider can’t replay.",true);return false; }tracks.append(song);
    }
    if(tracks.isEmpty()) { emit m_cider->apiFeedback("There are no tracks to save.",true);return false; }
    const auto id=QUuid::createUuid().toString(QUuid::Id128);
    QDir().mkpath(m_sessionsDir);QSaveFile file(m_sessionsDir+"/"+id+".json");
    const auto bytes=QJsonDocument(QJsonArray::fromVariantList(tracks)).toJson(QJsonDocument::Compact);
    bool ok=file.open(QIODevice::WriteOnly)&&file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner)&&file.write(bytes)==bytes.size()&&file.commit();
    if(ok) {
        sessions.prepend(QVariantMap{{"id",id},{"title",name.trimmed().left(80)},{"trackCount",tracks.size()}});ok=writeSessions(sessions);
        if(!ok)QFile::remove(m_sessionsDir+"/"+id+".json");
    }
    emit m_cider->apiFeedback(ok?"Queue saved":"Couldn’t save the queue. Try again.",!ok);
    if(ok && m_section=="sessions" && m_collection.isEmpty())reload();
    return ok;
}
QVariantList Library::savedTracks(const QString &id) {
    if(!sessionId(id) || m_sessionsDir.isEmpty())return {};
    bool valid=true;const auto sessions=readSessions(&valid);QVariantMap info;
    for(const auto &entry:sessions)if(entry.toMap().value("id")==id)info=entry.toMap();
    if(!valid || info.isEmpty()) { m_error="Couldn’t read this saved queue.";emit changed();return {}; }
    QFile file(m_sessionsDir+"/"+info.value("dataId").toString()+".json");
    if(!file.open(QIODevice::ReadOnly) || file.size()>24*1024*1024) { m_error="Couldn’t read this saved queue.";emit changed();return {}; }
    const auto doc=QJsonDocument::fromJson(file.readAll());
    if(!doc.isArray() || doc.array().size()!=info.value("trackCount").toInt() || doc.array().size()>5000) { m_error="This saved queue is unreadable.";emit changed();return {}; }
    QVariantList result;result.reserve(doc.array().size());
    for(const auto &value:doc.array()) {
        const auto song=savedSong(value.toObject().toVariantMap());
        if(song.isEmpty()) { m_error="This saved queue contains an unavailable track.";emit changed();return {}; }result.append(song);
    }return result;
}
QVariantMap Library::savedQueueChoices() const {
    bool valid=true;const auto rows=readSessions(&valid);
    return {{"rows",rows},{"error",valid?QString():QString("Couldn’t read saved queues. Try again.")}};
}
QVariantMap Library::appendSavedQueue(const QVariantMap &expected, const QVariantList &selection, bool skipDuplicates) {
    auto fail=[](const QString &message) { return QVariantMap{{"ok",false},{"error",message}}; };
    if(selection.isEmpty() || selection.size()>5000)return fail("Choose between 1 and 5,000 songs.");
    bool valid=true;const auto sessions=readSessions(&valid);QVariantMap current;
    for(const auto &entry:sessions)if(entry.toMap().value("id")==expected.value("id"))current=entry.toMap();
    if(!valid || current.isEmpty())return fail("This saved queue is unavailable. Refresh the list.");
    if(current.value("dataId")!=expected.value("dataId",expected.value("id")) || current.value("title")!=expected.value("title") || current.value("trackCount")!=expected.value("trackCount"))return fail("This saved queue changed. Refresh the list and choose it again.");
    // Read every selected item before writing anything; never silently drop an
    // unavailable song from a requested append.
    QVariantList incoming;
    for(const auto &value:selection) { const auto row=savedSong(value.toMap());if(row.isEmpty())return fail("This selection includes a song that can’t be saved.");incoming.append(row); }
    const auto priorError=m_error;
    auto tracks=savedTracks(current.value("id").toString());
    if(tracks.size()!=current.value("trackCount").toInt() || (tracks.isEmpty() && m_error!=priorError))return fail("Couldn’t read this saved queue. Your files were left intact.");
    // A valid empty queue still has a readable track file. savedTracks reports
    // errors through m_error, so check its storage explicitly for that case.
    if(tracks.isEmpty()) {
        QFile file(m_sessionsDir+"/"+current.value("dataId").toString()+".json");
        if(!file.open(QIODevice::ReadOnly) || file.size()>24*1024*1024)return fail("Couldn’t read this saved queue. Your files were left intact.");
        const auto doc=QJsonDocument::fromJson(file.readAll());
        if(!doc.isArray() || !doc.array().isEmpty())return fail("Couldn’t read this saved queue. Your files were left intact.");
    }
    auto key=[](const QVariantMap &row) { const auto id=row.value("catalogId").toString();return id.isEmpty()?row.value("type").toString()+":"+row.value("id").toString():"songs:"+id; };
    QSet<QString> seen;
    if(skipDuplicates)for(const auto &row:tracks)seen.insert(key(row.toMap()));
    int added=0,skipped=0;
    for(const auto &row:incoming) {
        const auto identity=key(row.toMap());
        if(skipDuplicates && seen.contains(identity)) { ++skipped;continue; }
        seen.insert(identity);tracks.append(row);++added;
    }
    if(tracks.size()>5000)return fail("A saved queue can hold up to 5,000 songs.");
    if(added && !updateSavedQueue(current,current.value("title").toString(),&tracks))return fail("Couldn’t save the changes. Refresh the list before trying again.");
    emit m_cider->apiFeedback(added?QString("Added %1 %2%3").arg(added).arg(added==1?"song":"songs").arg(skipped?QString(" · %1 duplicates skipped").arg(skipped):QString()):QString("All selected songs are already saved"),false);
    if(added)emit savedEditCommitted();
    return {{"ok",true},{"added",added},{"skipped",skipped}};
}
bool Library::renameSavedQueue(const QVariantMap &queue, const QString &name) {
    const bool ok=updateSavedQueue(queue,name,nullptr);if(ok)emit savedEditCommitted();return ok;
}
bool Library::editSavedTrack(int from, int to, bool remove) {
    if(m_section!="sessions" || m_collection.value("type")!="saved-queues")return false;
    const auto expected=m_collection;auto tracks=savedTracks(expected.value("id").toString());
    if(from<0 || from>=tracks.size() || (!remove && (to<0 || to>=tracks.size() || to==from || !m_collectionQuery.trimmed().isEmpty())))return false;
    if(remove)tracks.removeAt(from);else tracks.move(from,to);
    const bool ok=updateSavedQueue(expected,expected.value("title").toString(),&tracks);if(ok)emit savedEditCommitted();return ok;
}
bool Library::updateSavedQueue(const QVariantMap &expected, const QString &name, const QVariantList *tracks) {
    const auto id=expected.value("id").toString();const auto title=name.trimmed().left(80);
    if(!sessionId(id) || title.isEmpty())return false;
    bool valid=true;auto sessions=readSessions(&valid);int at=-1;
    for(int i=0;i<sessions.size();++i)if(sessions[i].toMap().value("id")==id)at=i;
    if(!valid || at<0) { emit m_cider->apiFeedback("Couldn’t read this saved queue. Your files were left intact.",true);return false; }
    auto row=sessions[at].toMap();const auto oldData=row.value("dataId").toString();
    if(oldData!=expected.value("dataId",id).toString() || row.value("title")!=expected.value("title") || row.value("trackCount")!=expected.value("trackCount")) {
        emit m_cider->apiFeedback("This saved queue changed. Reopen it before editing.",true);return false;
    }
    QString newData;
    if(tracks) {
        // Publish a new immutable track file, then atomically switch the index.
        // Failed writes leave the previous queue and its metadata together.
        newData=QUuid::createUuid().toString(QUuid::Id128);
        QSaveFile file(m_sessionsDir+"/"+newData+".json");
        const auto bytes=QJsonDocument(QJsonArray::fromVariantList(*tracks)).toJson(QJsonDocument::Compact);
        if(!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner) || file.write(bytes)!=bytes.size() || !file.commit()) {
            emit m_cider->apiFeedback("Couldn’t save the edit. Your saved queue is unchanged.",true);return false;
        }
        row["dataId"]=newData;row["trackCount"]=tracks->size();row["playable"]=!tracks->isEmpty();
    }
    const auto before=sessions[at].toMap();
    row["title"]=title;sessions[at]=row;
    if(!writeSessions(sessions)) {
        if(!newData.isEmpty())QFile::remove(m_sessionsDir+"/"+newData+".json");
        emit m_cider->apiFeedback("Couldn’t save the edit. Your saved queue is unchanged.",true);return false;
    }
    clearSavedUndo();m_savedUndoBefore=before;m_savedUndoAfter=row;
    m_savedUndoTimer.start();emit savedUndoChanged();
    if(m_section=="sessions")reload();
    emit m_cider->apiFeedback(tracks?"Saved queue updated":"Saved queue renamed",false);return true;
}
void Library::clearSavedUndo() {
    const auto oldData=m_savedUndoBefore.value("dataId").toString();
    if(!oldData.isEmpty() && oldData!=m_savedUndoAfter.value("dataId").toString()) {
        bool valid=true;const auto rows=readSessions(&valid);bool referenced=false;
        for(const auto &row:rows)if(row.toMap().value("dataId")==oldData)referenced=true;
        if(valid&&!referenced)QFile::remove(m_sessionsDir+"/"+oldData+".json");
    }
    m_savedUndoTimer.stop();m_savedUndoBefore.clear();m_savedUndoAfter.clear();emit savedUndoChanged();
}
bool Library::undoSavedQueue() {
    if(!canUndoSavedQueue())return false;
    bool valid=true;auto rows=readSessions(&valid);int at=-1;
    for(int i=0;i<rows.size();++i)if(rows[i].toMap().value("id")==m_savedUndoAfter.value("id"))at=i;
    if(!valid || at<0 || rows[at].toMap()!=m_savedUndoAfter) {
        clearSavedUndo();emit m_cider->apiFeedback("This saved queue changed. Undo is no longer available.",true);return false;
    }
    // Restore the previous immutable file by switching the index. No second
    // track list is held in RAM and no Cider playback action is issued.
    QFile file(m_sessionsDir+"/"+m_savedUndoBefore.value("dataId").toString()+".json");
    if(!file.open(QIODevice::ReadOnly)||file.size()>24*1024*1024) {
        clearSavedUndo();emit m_cider->apiFeedback("The previous saved queue is unavailable.",true);return false;
    }
    const auto doc=QJsonDocument::fromJson(file.readAll());
    bool readable=doc.isArray()&&doc.array().size()==m_savedUndoBefore.value("trackCount").toInt();
    if(readable)for(const auto &v:doc.array())if(savedSong(v.toObject().toVariantMap()).isEmpty()){readable=false;break;}
    if(!readable){clearSavedUndo();emit m_cider->apiFeedback("The previous saved queue is unreadable.",true);return false;}
    rows[at]=m_savedUndoBefore;
    if(!writeSessions(rows)) { emit m_cider->apiFeedback("Couldn’t undo the edit. Try again.",true);return false; }
    const auto replaced=m_savedUndoAfter.value("dataId").toString();const auto restored=m_savedUndoBefore.value("dataId").toString();
    clearSavedUndo();if(replaced!=restored)QFile::remove(m_sessionsDir+"/"+replaced+".json");
    if(m_section=="sessions")reload();
    emit m_cider->apiFeedback("Saved queue edit undone",false);return true;
}
bool Library::openQuickTarget(const QString &kind, const QString &key) {
    QVariantMap selected;
    if(kind=="saved") { for(const auto &v:readSessions())if(v.toMap().value("id")==key)selected=v.toMap(); }
    else if(kind=="pin") { for(const auto &v:m_pins)if(v.toMap().value("path")==key)selected=v.toMap(); }
    if(selected.isEmpty()) { emit m_cider->apiFeedback("This item is no longer available. Open Quick jump again.",true);return false; }
    invalidate();m_history.clear();m_collection.clear();m_collectionQuery.clear();m_query.clear();m_allItems.clear();m_filterText.clear();m_next.clear();m_findArtist=false;
    m_section=kind=="saved"?"sessions":"search";m_kind="songs";
    if(kind=="saved")m_allItems=readSessions();
    setItems(m_allItems);m_loaded=true;emit navigationReset();pushPage();
    if(selected.value("type")=="stations") { m_kind="stations";m_query=selected.value("title").toString(); }
    else { m_collection=selected;if(selected.value("type")=="artists")m_artistView="songs"; }
    setItems({});m_loaded=false;emit changed();reload();return true;
}
void Library::deleteSavedQueue(const QString &id) {
    if(!sessionId(id))return;
    bool valid=true;auto sessions=readSessions(&valid);
    if(!valid) { emit m_cider->apiFeedback("Couldn’t read saved queues. Your existing files were left intact.",true);return; }
    bool found=false;QString dataId;
    for(int i=0;i<sessions.size();++i)if(sessions[i].toMap().value("id")==id) { dataId=sessions[i].toMap().value("dataId").toString();sessions.removeAt(i);found=true;break; }
    if(!found)return;
    if(!writeSessions(sessions)) { emit m_cider->apiFeedback("Couldn’t delete this saved queue.",true);return; }
    if(m_savedUndoAfter.value("id")==id)clearSavedUndo();
    QFile::remove(m_sessionsDir+"/"+dataId+".json");
    if(m_collection.value("id")==id)back();
    if(m_section=="sessions")reload();
    emit m_cider->apiFeedback("Saved queue deleted",false);
}

void Library::persistSearches() {
    if (!m_sortSettings.isEmpty()) { QSettings settings(m_sortSettings,QSettings::IniFormat); settings.setValue("recentSearches",m_recentSearches); }
    emit recentSearchesChanged();
}
void Library::rememberSearch() {
    // Store intentional searches (Enter or a result action), never every keystroke.
    const auto term=m_query.trimmed().left(200);
    if (m_section!="search" || !m_collection.isEmpty() || term.isEmpty() || term.contains("://") || m_findArtist) return;
    auto next=m_recentSearches;
    for (qsizetype i=next.size();i>0;--i) if(next[i-1].compare(term,Qt::CaseInsensitive)==0) next.removeAt(i-1);
    next.prepend(term); next=next.mid(0,8);
    if(next==m_recentSearches)return;
    m_recentSearches=next;persistSearches();
}
void Library::removeRecentSearch(const QString &query) {
    if(m_recentSearches.removeAll(query))persistSearches();
}

QString Library::collectionPath() const {
    return m_collection.value("path").toString()+(m_collection.value("type")=="artists"?
        (m_artistView=="songs"?"/view/top-songs":m_artistView=="similar"?"/view/similar-artists":m_discography=="all"?"/albums":"/view/"+m_discography):"/tracks");
}
void Library::setDiscography(const QString &value) {
    if(m_collection.value("type")!="artists" || m_artistView!="albums" || m_discography==value || !QStringList{"all","full-albums","singles","live-albums"}.contains(value))return;
    invalidate();m_discography=value;m_allItems.clear();m_filterText.clear();m_next.clear();setItems({});
    emit changed();if(m_active)reload();
}
void Library::setArtistView(const QString &value) {
    if(m_collection.value("type")!="artists" || m_artistView==value || (value!="albums" && value!="songs" && value!="similar"))return;
    invalidate();m_artistView=value;m_collectionQuery.clear();m_allItems.clear();m_filterText.clear();m_next.clear();setItems({});
    emit changed();if(m_active)reload();
}


bool Library::hasArtistPins() const {
    for(const auto &pin:m_pins) if(pin.toMap().value("type")=="artists") return true;
    return false;
}
void Library::fetchRecommendations(const QString &path,bool append,const QString &reason) {
    const QUrl url(path);
    static const QRegularExpression safePath("^/v1/me/recommendations(?:/[A-Za-z0-9._~-]+(?:/contents)?)?$");
    if(!url.isRelative() || !url.host().isEmpty() || url.hasFragment() || !safePath.match(url.path()).hasMatch() || path.size()>2048 || m_pages.contains(path)) {
        m_busy=false;m_error="Couldn’t load more recommendations.";emit changed();return;
    }
    if(m_cider->m_apiToken.isEmpty()) {m_needsConnection=true;m_busy=false;m_error="Connect Spun to Cider to see For You.";emit changed();return;}
    m_busy=true;m_error.clear();emit changed();
    request("/api/v1/amapi/run-v3",{{"path",path}},false,[this,path,append,reason](QJsonObject response) {
        const auto data=response.value("data").toObject();
        if(!data.value("data").isArray()) {m_busy=false;m_error="Cider returned unreadable recommendations.";emit changed();return;}
        auto pending=append?m_recommendationPages:QVariantList{};
        if(append && !pending.isEmpty() && pending.first().toMap().value("path")==path)pending.removeFirst();
        QVariantList rows=append?m_allItems:QVariantList{};QSet<QString> identities;
        for(const auto &row:rows)identities.insert(row.toMap().value("type").toString()+":"+row.toMap().value("id").toString());
        bool truncated=false;
        const auto enqueue=[&](const QString &next,const QString &label) {
            if(next.isEmpty())return;
            if(pending.size()+m_pages.size()>=100) {truncated=true;return;}
            // URL validation runs again before any request; credentials stay on localhost.
            if(next==path || m_pages.contains(next))return;
            for(const auto &value:pending)if(value.toMap().value("path")==next)return;
            pending.append(QVariantMap{{"path",next},{"reason",label.left(200)}});
        };
        std::function<void(const QJsonArray &,const QString &,int)> collect;
        collect=[&](const QJsonArray &items,const QString &label,int depth) {
            if(depth>4) {truncated=true;return;}
            for(const auto &value:items) {
                const auto resource=value.toObject();
                if(resource.value("type")=="personal-recommendation") {
                    const auto attributes=resource.value("attributes").toObject();
                    const auto title=attributes.value("title").toObject().value("stringForDisplay").toString(label).left(200);
                    const auto contents=resource.value("relationships").toObject().value("contents").toObject();
                    collect(contents.value("data").toArray(),title,depth+1);enqueue(contents.value("next").toString(),title);continue;
                }
                const auto type=resource.value("type").toString();
                if(!QStringList{"albums","playlists","stations"}.contains(type))continue;
                auto row=item(resource);if(row.isEmpty())continue;
                const auto href=resource.value("href").toString();
                static const QRegularExpression catalog("^/v1/catalog/[a-z]{2}/(?:albums|playlists|stations)/[A-Za-z0-9._-]+$");
                if(!catalog.match(href).hasMatch() || !href.endsWith("/"+type+"/"+row.value("id").toString()))continue;
                row["path"]=href;row["recommendation"]=label;
                const auto key=type+":"+row.value("id").toString();if(identities.contains(key))continue;
                if(rows.size()>=1000) {truncated=true;return;}
                identities.insert(key);rows.append(row);
            }
        };
        collect(data.value("data").toArray(),reason,0);enqueue(data.value("next").toString(),reason);
        m_pages.insert(path);m_recommendationPages=pending;
        if(truncated) {m_recommendationPages.clear();m_releaseNotice="Showing the first available recommendations.";}else m_releaseNotice.clear();
        m_next=m_recommendationPages.isEmpty()?QString():m_recommendationPages.first().toMap().value("path").toString();
        m_allItems=rows;if(!append)m_filterText.clear();filterItems(append);
        m_busy=false;m_loaded=true;m_preservePosition=false;emit changed();continueSearch();
        if(rows.isEmpty() && !m_next.isEmpty())QTimer::singleShot(0,this,[this] {if(m_active && m_section=="for-you" && m_collection.isEmpty() && !m_busy && m_error.isEmpty())more();});
    });
}

void Library::fetchReleases() {
    m_releaseRequests.clear(); m_releaseRows.clear(); m_releaseNotice.clear(); m_releaseFailed=false; m_next.clear();
    if(!hasArtistPins()) { m_allItems.clear();m_filterText.clear();filterItems();m_loaded=true;emit changed();return; }
    if(m_cider->m_apiToken.isEmpty()) { m_needsConnection=true;m_error="Connect Spun to Cider to see releases.";emit changed();return; }
    // Group by storefront; batch 25 artists rather than issuing a request per pin.
    QMap<QString,QStringList> groups;
    for(const auto &entry:m_pins) {
        const auto pin=validatedPin(entry.toMap());
        if(pin.value("type")=="artists")groups[pin.value("path").toString().split('/').value(3)].append(pin.value("id").toString());
    }
    for(auto it=groups.cbegin();it!=groups.cend();++it) for(int i=0;i<it.value().size();i+=25)
        m_releaseRequests.append("/v1/catalog/"+it.key()+"/artists?ids="+it.value().mid(i,25).join(',')+"&views=latest-release");
    m_busy=true;emit changed();fetchReleaseBatch();
}
void Library::fetchReleaseBatch() {
    if(m_releaseRequests.isEmpty()) { finishReleases();return; }
    const auto path=m_releaseRequests.takeFirst();
    auto url=m_cider->m_rpcBase;url.setPath("/api/v1/amapi/run-v3");url.setQuery(QString());
    QNetworkRequest req(url);req.setTransferTimeout(10000);req.setRawHeader("apptoken",m_cider->m_apiToken.toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto *reply=m_network.post(req,QJsonDocument(QJsonObject{{"path",path}}).toJson(QJsonDocument::Compact));m_reply=reply;
    const int generation=m_generation,token=m_cider->m_tokenGeneration;
    connect(reply,&QNetworkReply::readyRead,reply,[reply] { if(reply->bytesAvailable()>4*1024*1024)reply->abort(); });
    connect(reply,&QNetworkReply::finished,this,[this,reply,path,generation,token] {
        reply->deleteLater();if(generation!=m_generation)return;
        if(token!=m_cider->m_tokenGeneration) { m_busy=false;m_error="Cider access changed. Refresh releases.";emit changed();return; }
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        m_cider->observeConnection(status,reply->error());
        const auto data=QJsonDocument::fromJson(reply->readAll()).object().value("data").toObject();
        if(status!=200 || reply->error()!=QNetworkReply::NoError || !data.value("data").isArray()) {
            m_releaseFailed=true;
            if(status==401 || status==403) { m_needsConnection=true;m_releaseRequests.clear(); }
        } else for(const auto &artist:data.value("data").toArray()) {
            const auto releases=artist.toObject().value("views").toObject().value("latest-release").toObject().value("data");
            if(!releases.isArray()) { m_releaseFailed=true;continue; }
            for(const auto &release:releases.toArray()) {
                auto row=item(release.toObject());if(row.value("type")!="albums" || !row.value("playable").toBool())continue;
                row["path"]="/v1/catalog/"+path.split('/').value(3)+"/albums/"+row.value("id").toString();
                bool duplicate=false;for(const auto &existing:m_releaseRows)if(existing.toMap().value("id")==row.value("id")) { duplicate=true;break; }
                if(!duplicate && m_releaseRows.size()<200)m_releaseRows.append(row);
            }
        }
        fetchReleaseBatch();
    });
}
void Library::finishReleases() {
    std::stable_sort(m_releaseRows.begin(),m_releaseRows.end(),[](const QVariant &a,const QVariant &b) {
        return a.toMap().value("releaseDate").toString()>b.toMap().value("releaseDate").toString();
    });
    m_busy=false;m_loaded=true;
    if(m_releaseFailed) {
        m_error=m_needsConnection?"Reconnect to load releases.":"Some releases couldn’t load. Try again.";
        // Preserve the last good list on a complete outage; don't cache partial reads.
        if(m_releaseRows.isEmpty() && !m_allItems.isEmpty())m_releaseNotice="Showing previous results";
        else m_allItems=m_releaseRows;
    } else { m_allItems=m_releaseRows;m_releaseCache=m_releaseRows;m_releaseClock.restart();m_releaseTokenGeneration=m_cider->m_tokenGeneration; }
    m_filterText.clear();filterItems();m_preservePosition=false;emit changed();
}

void Library::prepareRadio(const QVariantMap &song, bool current) {
    ++m_radioGeneration;if(m_radioReply)m_radioReply->abort();
    m_radioResolvedToken=m_cider->m_tokenGeneration;
    if(m_radioCacheToken!=m_cider->m_tokenGeneration) { m_radioCache.clear();m_radioCacheToken=m_cider->m_tokenGeneration; }
    m_radioCurrent=current;m_radioTrack=m_cider->m_track;m_radioStation.clear();m_radioError.clear();m_radioBusy=false;
    if(m_radioCacheClock.isValid() && m_radioCacheClock.elapsed()>1800000)m_radioCache.clear();
    if(!m_radioCacheClock.isValid() || m_radioCacheClock.elapsed()>1800000)m_radioCacheClock.restart();
    if(m_cider->m_apiToken.isEmpty()) { m_radioError="Connect to Cider to find a station.";emit radioChanged();return; }
    m_radioBusy=true;emit radioChanged();
    if(current)radioRequest("/api/v2/playback/now-playing",{},[this](QJsonObject json) {
        const auto data=json.value("data").toObject();const auto attrs=data.value("attributes").isObject()?data.value("attributes").toObject():data;
        resolveRadio({{"id",data.value("id").toString()},{"type","songs"},{"url",attrs.value("url").toString()},{"catalogId",attrs.value("playParams").toObject().value("catalogId").toString()}});
    });
    else resolveRadio(song);
}
void Library::radioRequest(const QString &endpoint,const QJsonObject &body,std::function<void(QJsonObject)> done) {
    auto url=m_cider->m_rpcBase;url.setPath(endpoint);url.setQuery(QString());
    QNetworkRequest req(url);req.setTransferTimeout(8000);req.setRawHeader("apptoken",m_cider->m_apiToken.toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto *reply=body.isEmpty()?m_network.get(req):m_network.post(req,QJsonDocument(body).toJson(QJsonDocument::Compact));m_radioReply=reply;
    const int generation=m_radioGeneration,token=m_cider->m_tokenGeneration;
    connect(reply,&QNetworkReply::readyRead,reply,[reply] { if(reply->bytesAvailable()>1024*1024)reply->abort(); });
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,token,done] {
        reply->deleteLater();if(generation!=m_radioGeneration)return;
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto json=QJsonDocument::fromJson(reply->readAll());
        if(token!=m_cider->m_tokenGeneration || (m_radioCurrent && m_radioTrack!=m_cider->m_track)) { m_radioBusy=false;m_radioError="Song or connection changed. Reopen the menu.";emit radioChanged();return; }
        if(status!=403)m_cider->observeConnection(status,reply->error());
        if(reply->error()!=QNetworkReply::NoError || status!=200 || !json.isObject()) {
            m_radioBusy=false;m_radioError=status==401 || status==403?"Allow library access for Spun in Cider to find radio.":"Couldn’t check radio. Reopen the menu to retry.";emit radioChanged();return;
        }
        done(json.object());
    });
}
void Library::resolveRadio(const QVariantMap &song) {
    auto path=linkPath(song.value("url").toString());
    const auto id=song.value("catalogId").toString().isEmpty()?(song.value("type")=="songs"?song.value("id").toString():QString()):song.value("catalogId").toString();
    if(!path.contains("/songs/"))path.clear();
    if(path.isEmpty() && QRegularExpression("^[0-9]+$").match(id).hasMatch()) {
        const auto candidate=song.value("path").toString().split('/').value(3);
        const auto store=QRegularExpression("^[a-z]{2}$").match(candidate).hasMatch()?candidate:m_storefront;
        if(store.isEmpty()) {
            radioRequest("/api/v1/amapi/run-v3",{{"path","/v1/me/storefront"}},[this,song](QJsonObject json) {
                const auto rows=json.value("data").toObject().value("data").toArray();
                const auto store=rows.isEmpty()?QString():rows.first().toObject().value("id").toString();
                if(!QRegularExpression("^[a-z]{2}$").match(store).hasMatch()) { m_radioBusy=false;m_radioError="Cider couldn’t identify your music region.";emit radioChanged();return; }
                m_storefront=store;resolveRadio(song);
            });return;
        }
        path="/v1/catalog/"+store+"/songs/"+id;
    }
    if(path.isEmpty()) { m_radioBusy=false;emit radioChanged();return; }
    if(m_radioCache.contains(path)) { m_radioStation=m_radioCache.value(path);m_radioBusy=false;emit radioChanged();return; }
    radioRequest("/api/v1/amapi/run-v3",{{"path",path+"/station"}},[this,path](QJsonObject json) {
        const auto data=json.value("data").toObject();const auto errors=data.value("errors").toArray();
        bool absent=!errors.isEmpty();for(const auto &error:errors)if(error.toObject().value("code").toString()!="40403")absent=false;
        if(!data.value("data").isArray() && !absent) m_radioError="Couldn’t check radio. Reopen the menu to retry.";
        else {
            for(const auto &value:data.value("data").toArray()) { auto row=item(value.toObject());if(row.value("type")=="stations" && row.value("playable").toBool()) {
                if(row.value("path").toString().isEmpty())row["path"]="/v1/catalog/"+path.split('/').value(3)+"/stations/"+row.value("id").toString();
                if(!validatedPin(row).isEmpty()) {m_radioStation=row;break;}
            } }
            if(m_radioCache.size()>=64)m_radioCache.clear();
            m_radioCache.insert(path,m_radioStation);
        }
        m_radioBusy=false;emit radioChanged();
    });
}
void Library::playRadio() {
    if(radioBusy() || m_starting || m_radioStation.isEmpty() || m_cider->controlBusy())return;
    if(m_radioResolvedToken!=m_cider->m_tokenGeneration) { m_radioStation.clear();emit radioChanged();emit m_cider->apiFeedback("Cider access changed. Choose radio again.",true);return; }
    if(m_radioCurrent && m_radioTrack!=m_cider->m_track) { m_radioStation.clear();emit radioChanged();emit m_cider->apiFeedback("Song changed. Choose radio again.",true);return; }
    startStation(m_radioStation,true);
}
void Library::finishStation(bool radio,const QString &error) {
    if(radio) {m_radioStarting=false;m_radioError=error;emit radioChanged();}
    else {m_starting=false;m_playError=error;emit changed();}
    emit m_cider->apiFeedback(error.isEmpty()?"Radio started":error,!error.isEmpty());
    if(error.isEmpty()) {m_cider->refresh();if(m_cider->queueVisible() || m_cider->libraryVisible())m_cider->refreshQueue();}
}
void Library::startStation(const QVariantMap &station,bool radio) {
    if(validatedPin(station).isEmpty() || station.value("type")!="stations") {finishStation(radio,"This station has no valid playback link.");return;}
    if(radio) {m_radioStarting=true;m_radioError.clear();emit radioChanged();}
    else {m_starting=true;m_playError.clear();emit changed();}
    m_stationClock.start();const int token=m_cider->m_tokenGeneration;
    const QPointer<Library> guard(this);
    // Cider's awaited collection RPC may never settle for stations. The regular
    // resource route also handles streamed radio; its receipt still needs readback.
    m_cider->apiRequest("POST","/playback/play-href",{{"href",station.value("path").toString()}},[this,guard,station,radio,token](bool ok,QJsonObject) {
        if(!guard)return;
        if(!ok) {finishStation(radio,"Cider couldn’t accept the station. Check its connection and playback access.");return;}
        verifyStation(station,radio,token);
    },false);
}
void Library::verifyStation(const QVariantMap &station,bool radio,int token) {
    if(token!=m_cider->m_tokenGeneration) {finishStation(radio,"Cider access changed while starting radio. Check playback before trying again.");return;}
    const QPointer<Library> guard(this);
    m_cider->apiRequest("GET","/queue/position",{},[this,guard,station,radio,token](bool ok,QJsonObject json) {
        if(!guard)return;
        const int index=json.value("data").toObject().value("position").toInt(-1);
        if(!ok) {finishStation(radio,"Couldn’t verify radio playback. Check Cider before trying again.");return;}
        m_cider->apiRequest("GET",QString("/queue?offset=%1&limit=1").arg(qMax(0,index)),{},[this,guard,station,radio,token](bool read,QJsonObject queue) {
            if(!guard)return;
            const auto rows=queue.value("data").toObject().value("items").toArray();
            const auto context=rows.isEmpty()?QJsonObject():rows.first().toObject().value("containerContext").toObject();
            const bool matches=read && context.value("id").toString()==station.value("id").toString();
            m_cider->apiRequest("GET","/playback",{},[this,guard,station,radio,token,matches](bool playing,QJsonObject state) {
                if(!guard)return;
                if(token!=m_cider->m_tokenGeneration) {finishStation(radio,"Cider access changed while starting radio. Check playback before trying again.");return;}
                const auto data=state.value("data").toObject();
                const auto params=data.value("nowPlaying").toObject().value("playParams").toObject();
                const bool stream=params.value("kind")=="station" && params.value("id").toString()==station.value("id").toString();
                if((matches || stream) && playing && data.value("state")=="playing") {finishStation(radio);return;}
                if(!playing || m_stationClock.elapsed()>=10000) {finishStation(radio,"Cider hasn’t started this station. Check playback before trying again.");return;}
                QTimer::singleShot(350,this,[this,station,radio,token] {verifyStation(station,radio,token);});
            },false);
        },false);
    },false);
}
