#pragma once
#include <QObject>
#include <QImage>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSettings>
#include <QUrl>
#include <QVariantList>
#include <QSet>
#include <memory>
#include <QFutureWatcher>

struct Track {
    QString path, title, artist, album, cover;
    qint64 duration = 0;
    QString albumArtist;
    int year = 0, number = 0, discNumber = 1;
};

struct LoadedArtwork { QImage image; QString file; bool created = false; };
struct ImportedTracks { QList<Track> tracks; QString firstPath; };

class Player : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString trackKey READ trackKey NOTIFY trackChanged)
    Q_PROPERTY(bool cd500Rpm READ cd500Rpm WRITE setCd500Rpm NOTIFY settingsChanged)
    Q_PROPERTY(int vinylSpeed READ vinylSpeed WRITE setVinylSpeed NOTIFY settingsChanged)
    Q_PROPERTY(bool vinylAlbumMode READ vinylAlbumMode WRITE setVinylAlbumMode NOTIFY settingsChanged)
    Q_PROPERTY(bool showPlayerBody READ showPlayerBody WRITE setShowPlayerBody NOTIFY settingsChanged)
    Q_PROPERTY(QString cassetteFinish READ cassetteFinish WRITE setCassetteFinish NOTIFY settingsChanged)
    Q_PROPERTY(bool horizontalSeek READ horizontalSeek WRITE setHorizontalSeek NOTIFY settingsChanged)
    Q_PROPERTY(bool vinylCrackle READ vinylCrackle WRITE setVinylCrackle NOTIFY settingsChanged)
    Q_PROPERTY(bool vinylStatic READ vinylStatic WRITE setVinylStatic NOTIFY settingsChanged)
    Q_PROPERTY(bool vinylSkips READ vinylSkips WRITE setVinylSkips NOTIFY settingsChanged)
    Q_PROPERTY(bool cassetteSounds READ cassetteSounds WRITE setCassetteSounds NOTIFY settingsChanged)
    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString album READ album NOTIFY trackChanged)
    Q_PROPERTY(QString albumKey READ albumKey NOTIFY trackChanged)
    Q_PROPERTY(QVariantMap discDetails READ discDetails NOTIFY discDetailsChanged)
    Q_PROPERTY(QString format READ format NOTIFY trackChanged)
    Q_PROPERTY(bool artworkLoading READ artworkLoading NOTIFY artworkChanged)
    Q_PROPERTY(QImage artwork READ artwork NOTIFY artworkChanged)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(int count READ count NOTIFY queueChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY trackChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY settingsChanged)
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY settingsChanged)
    Q_PROPERTY(bool light READ light WRITE setLight NOTIFY settingsChanged)
    Q_PROPERTY(bool vinyl READ vinyl WRITE setVinyl NOTIFY vinylChanged)
    Q_PROPERTY(QString medium READ medium WRITE setMedium NOTIFY mediumChanged)
    Q_PROPERTY(bool motion READ motion WRITE setMotion NOTIFY settingsChanged)
    Q_PROPERTY(bool ciderAutoStart READ ciderAutoStart WRITE setCiderAutoStart NOTIFY settingsChanged)
    Q_PROPERTY(bool miniOnTop READ miniOnTop WRITE setMiniOnTop NOTIFY miniOnTopChanged)
    Q_PROPERTY(bool miniMode READ miniMode WRITE setMiniMode NOTIFY miniModeChanged)
    Q_PROPERTY(bool backgroundBlur READ backgroundBlur WRITE setBackgroundBlur NOTIFY backgroundBlurChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString importStatus READ importStatus NOTIFY importProgressChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    explicit Player(const QString &settingsPath, QObject *parent = nullptr);
    ~Player() override;
    QString trackKey() const { return currentUrl().toString(); }
    bool cd500Rpm() const { return m_cd500Rpm; }
    void setCd500Rpm(bool value);
    int vinylSpeed() const { return m_vinylSpeed; }
    bool horizontalSeek() const { return m_horizontalSeek; }
    bool vinylCrackle() const { return m_vinylCrackle; }
    bool showPlayerBody() const { return m_showPlayerBody; }
    void setShowPlayerBody(bool value);
    QString cassetteFinish() const { return m_cassetteFinish; }
    void setCassetteFinish(const QString &value);
    bool vinylStatic() const { return m_vinylStatic; }
    bool vinylSkips() const { return m_vinylSkips; }
    void setVinylSpeed(int value);
    void setHorizontalSeek(bool value);
    void setVinylCrackle(bool value);
    void setVinylStatic(bool value);
    void setVinylSkips(bool value);
    bool cassetteSounds() const { return m_cassetteSounds; }
    void setCassetteSounds(bool enabled);
    QString title() const;
    QString artist() const;
    QString album() const;
    QString albumKey() const;
    QVariantMap discDetails() const;
    QString format() const;
    bool artworkLoading() const { return m_artLoading; }
    QImage artwork() const { return m_art; }
    QVariantList queue() const;
    int count() const { return m_tracks.size(); }
    int currentIndex() const { return m_index; }
    bool playing() const { return m_playPending || (m_media && m_media->playbackState() == QMediaPlayer::PlayingState); }
    QMediaPlayer::PlaybackState playbackState() const { return m_playPending ? QMediaPlayer::PlayingState : m_media ? m_media->playbackState() : QMediaPlayer::StoppedState; }
    qint64 position() const { return m_restorePosition >= 0 ? m_restorePosition : m_media ? m_media->position() : 0; }
    qint64 duration() const { return m_media && m_media->duration() > 0 ? m_media->duration() : m_index >= 0 ? m_tracks[m_index].duration : 0; }
    double volume() const { return m_volume; }
    bool shuffle() const { return m_shuffle; }
    int repeatMode() const { return m_repeat; }
    bool light() const { return m_light; }
    bool vinyl() const { return m_medium=="vinyl"; }
    QString medium() const { return m_medium; }
    void setMedium(const QString &value);
    void setVinyl(bool value);
    bool motion() const { return m_motion; }
    bool ciderAutoStart() const { return m_ciderAutoStart; }
    void setCiderAutoStart(bool value);
    Q_INVOKABLE void move(int from, int to);
    bool miniOnTop() const { return m_miniOnTop; }
    bool miniMode() const { return m_miniMode; }
    bool backgroundBlur() const { return m_backgroundBlur; }
    bool busy() const { return m_busy; }
    bool vinylAlbumMode() const { return m_vinylAlbumMode; }
    void setVinylAlbumMode(bool value);
    Q_INVOKABLE bool playAlbumPosition(const QString &path, qint64 position);
    QString importStatus() const { return m_importStatus; }
    QString error() const { return m_error; }
    QUrl currentUrl() const;
    QString artworkFile() const { return m_artFile; }
    QString trackId() const;
    void setVolume(double value);
    void setShuffle(bool value);
    void setRepeatMode(int value);
    void setLight(bool value);
    void setMotion(bool value);
    void setBackgroundBlur(bool value);
    void setMiniMode(bool value);
    void setMiniOnTop(bool value);
    Q_INVOKABLE void addUrls(const QList<QUrl> &urls, bool autoplay = true);
    Q_INVOKABLE void cancelImport();
    Q_INVOKABLE void select(int index, bool autoplay = true);
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void next(bool automatic = false, bool autoplay = true);
    Q_INVOKABLE void previous(bool autoplay = true);
    Q_INVOKABLE void seek(qint64 milliseconds);
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setCover(const QUrl &url);
    Q_INVOKABLE void demo();
    Q_INVOKABLE void dismissError();
    Q_INVOKABLE void save();
    static bool supported(const QString &path);
    static Track readTrack(const QString &path);
signals:
    void vinylChanged();
    void mediumChanged();
    void discDetailsChanged();
    void trackChanged();
    void artworkChanged();
    void queueChanged();
    void playingChanged();
    void positionChanged();
    void seeked(qint64 position);
    void durationChanged();
    void volumeChanged();
    void settingsChanged();
    void backgroundBlurChanged();
    void miniModeChanged();
    void miniOnTopChanged();
    void busyChanged();
    void importProgressChanged();
    void errorChanged();
    void imported();
private:
    void ensureMedia();
    void loadArt();
    void startArtLoad();
    void fail(const QString &message);
    QList<Track> m_tracks;
    QFutureWatcher<ImportedTracks> m_importJob;
    QString m_importStatus;
    bool m_tracksDirty = false;
    int m_index = -1;
    QFutureWatcher<void> m_audioPreparation;
    bool m_preparingAudio = false, m_playPending = false;
    double m_volume = .65;
    std::unique_ptr<QAudioOutput> m_audio;
    std::unique_ptr<QMediaPlayer> m_media;
    QSettings m_settings;
    QImage m_art;
    QFutureWatcher<LoadedArtwork> m_artLoader;
    quint64 m_artGeneration = 0;
    bool m_artLoading = false, m_artJobActive = false;
    QSet<QString> m_shuffleVisited;
    QString m_artFile, m_error;
    bool m_ciderAutoStart = false;
    bool m_cd500Rpm=false;
    int m_vinylSpeed=33;
    bool m_vinylAlbumMode=false;
    bool m_showPlayerBody=true;
    QString m_cassetteFinish="smoke";
    bool m_horizontalSeek=false, m_vinylCrackle=false, m_vinylStatic=false, m_vinylSkips=false;
    bool m_cassetteSounds=true;
    QString m_medium = "cd";
    bool m_shuffle = false, m_light = false, m_motion = true, m_busy = false, m_backgroundBlur = false, m_miniMode = false, m_miniOnTop = false;
    int m_repeat = 0;
    qint64 m_restorePosition = -1;
};
