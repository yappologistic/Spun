#pragma once
#include <QObject>
#include <QVariantList>
#include <QJsonObject>
#include <QTimer>
#include <QPointer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QElapsedTimer>
#include <functional>
class Cider;

// Browsing is read-only. Playback is sent only by an explicit play action.
class Library : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool preparingTail READ preparingTail NOTIFY changed)
    Q_PROPERTY(bool tailCollection READ tailCollection NOTIFY changed)
    Q_PROPERTY(bool canUndoSavedQueue READ canUndoSavedQueue NOTIFY savedUndoChanged)
    Q_PROPERTY(bool radioBusy READ radioBusy NOTIFY radioChanged)
    Q_PROPERTY(bool radioAvailable READ radioAvailable NOTIFY radioChanged)
    Q_PROPERTY(QString radioError READ radioError NOTIFY radioChanged)
    Q_PROPERTY(QString releaseNotice READ releaseNotice NOTIFY changed)
    Q_PROPERTY(bool hasArtistPins READ hasArtistPins NOTIFY pinsChanged)
    Q_PROPERTY(QString discography READ discography WRITE setDiscography NOTIFY changed)
    Q_PROPERTY(QString artistView READ artistView WRITE setArtistView NOTIFY changed)
    Q_PROPERTY(QStringList recentSearches READ recentSearches NOTIFY recentSearchesChanged)
    Q_PROPERTY(bool newestFirst READ newestFirst WRITE setNewestFirst NOTIFY changed)
    Q_PROPERTY(QVariantList pins READ pins NOTIFY pinsChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
    Q_PROPERTY(QString section READ section WRITE setSection NOTIFY changed)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY changed)
    Q_PROPERTY(QString collectionQuery READ collectionQuery WRITE setCollectionQuery NOTIFY changed)
    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY changed)
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QVariantMap collection READ collection NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool needsConnection READ needsConnection NOTIFY changed)
    Q_PROPERTY(bool starting READ starting NOTIFY changed)
    Q_PROPERTY(QString playError READ playError NOTIFY changed)
public:
    explicit Library(Cider *cider, QObject *parent=nullptr);
    ~Library() override;
    bool radioBusy() const { return m_radioBusy || m_radioStarting; }
    bool radioAvailable() const { return !m_radioStation.isEmpty(); }
    QString radioError() const { return m_radioError; }
    QString releaseNotice() const { return m_releaseNotice; }
    bool hasArtistPins() const;
    Q_INVOKABLE void prepareRadio(const QVariantMap &song, bool current=false);
    Q_INVOKABLE void playRadio();
    QString discography() const { return m_discography; }
    void setDiscography(const QString &value);
    QString artistView() const { return m_artistView; }
    void setArtistView(const QString &value);
    QStringList recentSearches() const { return m_recentSearches; }
    Q_INVOKABLE void rememberSearch();
    Q_INVOKABLE void removeRecentSearch(const QString &query);
    Q_INVOKABLE void shuffleCollection(const QVariantMap &item);
    bool newestFirst() const { return m_newestFirst; }
    void setNewestFirst(bool value);
    Q_INVOKABLE bool saveQueue(const QString &name);
    Q_INVOKABLE void deleteSavedQueue(const QString &id);
    Q_INVOKABLE bool renameSavedQueue(const QVariantMap &queue, const QString &name);
    Q_INVOKABLE bool editSavedTrack(int from, int to, bool remove);
    Q_INVOKABLE QVariantMap savedQueueChoices() const;
    Q_INVOKABLE bool openQuickTarget(const QString &kind, const QString &key);
    bool canUndoSavedQueue() const { return m_savedUndoTimer.isActive() && !m_savedUndoBefore.isEmpty(); }
    Q_INVOKABLE bool undoSavedQueue();
    Q_INVOKABLE QVariantMap appendSavedQueue(const QVariantMap &queue, const QVariantList &tracks, bool skipDuplicates);
    Q_INVOKABLE QVariantList savedTracks(const QString &id);
    Q_INVOKABLE void copyLink(const QVariantMap &item);
    static QString songLink(const QString &url);
    QVariantList pins() const { return m_pins; }
    Q_INVOKABLE bool isPinned(const QVariantMap &item) const;
    Q_INVOKABLE void togglePin(const QVariantMap &item);
    Q_INVOKABLE void playPin(int index);
    Q_INVOKABLE void openPin(int index);
    bool active() const { return m_active; }
    QString section() const { return m_section; }
    QString query() const { return m_query; }
    QString collectionQuery() const { return m_collectionQuery; }
    void setCollectionQuery(const QString &value);
    QString kind() const { return m_kind; }
    QVariantList items() const { return m_items; }
    QVariantMap collection() const { return m_collection; }
    bool busy() const { return m_busy; }
    bool hasMore() const { return !m_next.isEmpty(); }
    QString error() const { return m_error; }
    bool needsConnection() const { return m_needsConnection; }
    bool starting() const { return m_starting; }
    QString playError() const { return m_playError; }
    void setActive(bool);
    void setSection(const QString &);
    void setQuery(const QString &);
    void setKind(const QString &);
    Q_INVOKABLE bool openLink(const QString &text);
    Q_INVOKABLE bool openClipboardLink();
    static QString linkPath(const QString &text);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void more();
    Q_INVOKABLE void open(int index);
    Q_INVOKABLE void showArtist(const QString &name);
    Q_INVOKABLE void back();
    bool preparingTail() const { return m_tailIndex>=0; }
    bool tailCollection() const;
    Q_INVOKABLE void queueFromHere(const QVariantMap &selected);
    Q_INVOKABLE void cancelQueueFromHere();
    Q_INVOKABLE void play(int index);
    Q_INVOKABLE void playCollection();
    static QVariantMap item(const QJsonObject &value, const QString &fallbackType = {});
signals:
    void tailReady(const QVariantList &tracks);
    void feedback(const QString &message, bool error);
    void savedUndoChanged();
    void savedEditCommitted();
    void radioChanged();
    void recentSearchesChanged();
    void pinsChanged();
    void changed();
    void returned();
    void navigating();
    void navigationReset();
    void itemsChanging(bool append);
    void itemsChanged();
private:
    int m_tailIndex=-1;
    QVariantMap m_tailAnchor;
    void continueTail();
    void radioRequest(const QString &endpoint, const QJsonObject &body, std::function<void(QJsonObject)> done);
    void resolveRadio(const QVariantMap &song);
    QPointer<QNetworkReply> m_radioReply;
    int m_radioGeneration=0;
    bool m_radioBusy=false, m_radioCurrent=false, m_radioStarting=false;
    int m_radioCacheToken=-1, m_radioResolvedToken=-1;
    QString m_radioError, m_radioTrack;
    QVariantMap m_radioStation;
    QHash<QString,QVariantMap> m_radioCache;
    QElapsedTimer m_radioCacheClock;
    void fetchRecommendations(const QString &path,bool append,const QString &reason={});
    QVariantList m_recommendationPages;
    void fetchReleases();
    void fetchReleaseBatch();
    void finishReleases();
    QStringList m_releaseRequests;
    QVariantList m_releaseRows, m_releaseCache;
    QElapsedTimer m_releaseClock;
    QString m_releaseNotice;
    bool m_releaseFailed=false;
    int m_releaseTokenGeneration=-1;
    static QVariantMap validatedPin(const QVariantMap &item);
    QString m_sessionsDir, m_sortSettings;
    QVariantMap m_savedUndoBefore, m_savedUndoAfter;
    QTimer m_savedUndoTimer;
    void clearSavedUndo();
    QVariantList readSessions(bool *valid=nullptr) const;
    bool writeSessions(const QVariantList &rows);
    bool updateSavedQueue(const QVariantMap &expected, const QString &name, const QVariantList *tracks);
    QString m_pinsPath;
    QVariantList m_pins;
    bool m_preservePosition=false;
    void filterItems(bool append=false);
    void continueSearch();
    bool filteringLocally() const;
    void invalidate();
    void setItems(const QVariantList &items, bool append=false);
    void fetch(const QString &path, bool append=false);
    void request(const QString &path, const QJsonObject &body, bool action,
                 std::function<void(QJsonObject)> done);
    void start(const QVariantMap &item, bool shuffle=false);
    void persistSearches();
    QStringList m_recentSearches;
    QString collectionPath() const;
    QString m_artistView="albums",m_discography="all";
    QString listPath() const;
    QString resultType() const;
    Cider *m_cider;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply, m_actionReply;
    QTimer m_debounce;
    int m_generation=0;
    bool m_active=false, m_busy=false, m_loaded=false, m_starting=false, m_needsConnection=false;
    QString m_section="search", m_kind="songs", m_query, m_storefront, m_next, m_error, m_playError;
    QString m_collectionQuery;
    struct Page {
        QVariantMap collection;
        QVariantList items, allItems;
        QString next, collectionQuery, artistView, discography;
        QVariantList recommendationPages;
        QSet<QString> pages;
    };
    QList<Page> m_history;
    void pushPage();
    bool m_findArtist=false, m_newestFirst=false;
    QVariantList m_allItems, m_items;
    QStringList m_filterText;
    QVariantMap m_collection;
    QSet<QString> m_pages;
};
