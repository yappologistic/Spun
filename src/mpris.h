#pragma once
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QVariantMap>
#include "player.h"

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
    Q_PROPERTY(bool CanGoNext READ canControl)
    Q_PROPERTY(bool CanGoPrevious READ canControl)
    Q_PROPERTY(bool CanPlay READ canControl)
    Q_PROPERTY(bool CanPause READ canControl)
    Q_PROPERTY(bool CanSeek READ canControl)
    Q_PROPERTY(bool CanControl READ always CONSTANT)
public:
    explicit PlayerAdaptor(Player *p);
    QString status() const;
    QString loop() const;
    void setLoop(const QString &value);
    double rate() const { return 1.0; }
    void setRate(double) {}
    bool shuffle() const { return m_player->shuffle(); }
    void setShuffle(bool value) { m_player->setShuffle(value); }
    QVariantMap metadata() const;
    double volume() const { return m_player->volume(); }
    void setVolume(double v) { m_player->setVolume(v); }
    qlonglong position() const { return m_player->position() * 1000; }
    bool canControl() const { return m_player->count() > 0; }
    bool always() const { return true; }
public slots:
    void Next() { m_player->next(); }
    void Previous() { m_player->previous(); }
    void Pause() { m_player->pause(); }
    void PlayPause() { m_player->toggle(); }
    void Stop() { m_player->stop(); }
    void Play() { m_player->play(); }
    void Seek(qlonglong offset);
    void SetPosition(const QDBusObjectPath &trackId, qlonglong position);
    void OpenUri(const QString &uri) { m_player->addUrls({QUrl(uri)}); }
signals:
    void Seeked(qlonglong position);
private:
    void changed();
    Player *m_player;
};

bool registerMpris(Player *player);
