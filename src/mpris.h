#pragma once
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QVariantMap>
#include "player.h"
#include "cider.h"
#include <QPointer>

class RootAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit CONSTANT)
    Q_PROPERTY(bool CanRaise READ canRaise CONSTANT)
    Q_PROPERTY(bool HasTrackList READ hasTrackList CONSTANT)
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ schemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ mimeTypes CONSTANT)
public:
    explicit RootAdaptor(Player *p) : QDBusAbstractAdaptor(p) {}
    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return "Spun"; }
    QString desktopEntry() const { return "spun"; }
    QStringList schemes() const { return {"file"}; }
    QStringList mimeTypes() const { return {"audio/mpeg", "audio/flac", "audio/ogg", "audio/x-wav", "audio/mp4"}; }
public slots:
    void Quit();
    void Raise();
    void OpenFiles(const QStringList &urls);
};

class PlayerAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ status)
    Q_PROPERTY(QString LoopStatus READ loop WRITE setLoop)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ rate CONSTANT)
    Q_PROPERTY(double MaximumRate READ rate CONSTANT)
    Q_PROPERTY(bool CanGoNext READ canNext)
    Q_PROPERTY(bool CanGoPrevious READ canPrevious)
    Q_PROPERTY(bool CanPlay READ canControl)
    Q_PROPERTY(bool CanPause READ canControl)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ always CONSTANT)
public:
    explicit PlayerAdaptor(Player *p, Cider *cider = nullptr, Player *youtube = nullptr, Player *jellyfin = nullptr, Player *subsonic = nullptr);
    void setRemote(bool remote);
    void setSourceWindow(QObject *window);
    QString trackId() const;
    qint64 duration() const;
    QString status() const;
    QString loop() const;
    void setLoop(const QString &value);
    double rate() const { return 1.0; }
    void setRate(double value) { if (value == 0.) Pause(); }
    bool shuffle() const { return m_remote ? m_cider->shuffle() : m_player->shuffle(); }
    void setShuffle(bool value) { if (m_remote) m_cider->setShuffle(value); else m_player->setShuffle(value); }
    QVariantMap metadata() const;
    double volume() const { return m_remote ? m_cider->volume() : m_player->volume(); }
    void setVolume(double v);
    qlonglong position() const { return (m_remote ? m_cider->position() : m_player->position()) * 1000; }
    bool canControl() const { return m_remote ? m_cider->count() > 0 : m_player->count() > 0; }
    bool canSeek() const { return canControl() && duration() > 0 && (!m_remote || m_cider->canSeek()); }
    bool canNext() const { return m_remote ? canControl() && m_cider->canNext() : m_player->count() > 1; }
    bool canPrevious() const { return m_remote ? canControl() && m_cider->canPrevious() : m_player->count() > 0; }
    bool always() const { return true; }
public slots:
    void Next();
    void Previous();
    void Pause() { if (canControl()) { if (m_remote) m_cider->pause(); else m_player->pause(); } }
    void PlayPause() { if (canControl()) { if (m_remote) m_cider->toggle(); else m_player->toggle(); } }
    void Stop() { if (m_remote) { if (canControl()) m_cider->stop(); } else m_player->stop(); }
    void Play() { if (canControl()) { if (m_remote) m_cider->play(); else m_player->play(); } }
    void Seek(qlonglong offset);
    void SetPosition(const QDBusObjectPath &trackId, qlonglong position);
    void OpenUri(const QString &uri) { m_local->addUrls({QUrl(uri)}); }
signals:
    void Seeked(qlonglong position);
private slots:
    void syncSource();
private:
    void changed();
    void seekTo(qint64 position);
    Player *m_player;
    Player *m_local;
    Player *m_youtube;
    Player *m_jellyfin;
    Player *m_subsonic;
    Cider *m_cider;
    QPointer<QObject> m_sourceWindow;
    bool m_remote = false, m_changePending = false;
    QVariantMap m_lastProperties;
};

PlayerAdaptor *registerMpris(Player *player, Cider *cider, Player *youtube = nullptr, Player *jellyfin = nullptr, Player *subsonic = nullptr);
