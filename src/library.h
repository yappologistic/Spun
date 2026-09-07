#pragma once
#include <QObject>
#include <QVariantList>
#include <QJsonObject>
#include <QTimer>
#include <QPointer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <functional>
class Cider;

// Browsing is read-only. Playback is sent only by an explicit play action.
class Library : public QObject {
    Q_OBJECT
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
    Q_INVOKABLE void back();
    Q_INVOKABLE void play(int index);
    Q_INVOKABLE void playCollection();
    static QVariantMap item(const QJsonObject &value, const QString &fallbackType = {});
signals:
    void changed();
    void returned();
    void itemsChanging(bool append);
    void itemsChanged();
private:
    void filterItems(bool append=false);
    void continueSearch();
    bool filteringLocally() const;
    void invalidate();
    void setItems(const QVariantList &items, bool append=false);
    void fetch(const QString &path, bool append=false);
    void request(const QString &path, const QJsonObject &body, bool action,
                 std::function<void(QJsonObject)> done);
    void start(const QVariantMap &item);
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
    QVariantList m_allItems, m_items, m_savedItems;
    QStringList m_filterText;
    QVariantMap m_collection;
    QString m_savedNext;
    QSet<QString> m_pages, m_savedPages;
};
