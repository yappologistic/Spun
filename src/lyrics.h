#pragma once
#include <QObject>
#include <QVariantList>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QNetworkReply>
#include <QJsonObject>
#include <functional>
class Player;
class Cider;
class LyricTimeline {
public:
    void reset(const QVariantList &lines);
    int indexAt(qint64 position) const;
private:
    struct Change { qint64 time; int index; };
    QList<Change> m_changes;
};
class Lyrics : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
    Q_PROPERTY(bool remote READ remote WRITE setRemote NOTIFY changed)
    Q_PROPERTY(QVariantList lines READ lines NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool timed READ timed NOTIFY changed)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
public:
    Lyrics(Player *player, Cider *cider, QObject *parent=nullptr);
    bool active() const { return m_active; }
    bool remote() const { return m_remote; }
    QVariantList lines() const { return m_lines; }
    bool loading() const { return m_loading; }
    QString message() const { return m_message; }
    bool timed() const { return m_timed; }
    int currentIndex() const { return m_index; }
    void setActive(bool value);
    void setRemote(bool value);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool seekToLine(int index);
    static QVariantList parse(const QString &text);
    static int indexAt(const QVariantList &lines, qint64 position);
signals:
    void changed();
    void currentIndexChanged();
private:
    void trackChanged();
    void updateIndex();
    void finish(const QVariantList &lines, const QString &message={});
    void cancel();
    void request(const QString &path, const QJsonObject &body, int generation, std::function<void(QJsonObject)> done);
    QString key() const;
    Player *m_player;
    Cider *m_cider;
    bool m_active=false, m_remote=false, m_loading=false, m_timed=false;
    int m_generation=0, m_index=-1;
    QString m_message, m_key, m_cacheKey;
    QVariantList m_lines, m_cache;
    LyricTimeline m_timeline;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
};
