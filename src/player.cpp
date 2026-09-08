#include "player.h"
#include "artwork.h"
#include <QDir>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QSaveFile>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QtConcurrent>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tvariant.h>
#include <taglib/tpropertymap.h>
#include <algorithm>
#include <utility>

static QString text(const TagLib::String &s) { return QString::fromStdString(s.to8Bit(true)); }

Player::Player(const QString &settingsPath, QObject *parent)
    : QObject(parent), m_settings(settingsPath, QSettings::IniFormat) {
    connect(this, &Player::queueChanged, this, [this] { m_tracksDirty = true; });
    connect(this, &Player::trackChanged, this, &Player::discDetailsChanged);
    connect(this, &Player::queueChanged, this, &Player::discDetailsChanged);
    connect(&m_audioPreparation, &QFutureWatcherBase::finished, this, [this] {
        m_preparingAudio = false;
        if (!m_playPending || m_index < 0) return;
        ensureMedia();
        m_playPending = false;
        m_media->play();
        emit playingChanged();
    });
    m_volume = qBound(0., m_settings.value("volume", 0.65).toDouble(), 1.);
    m_shuffle = m_settings.value("shuffle", false).toBool();
    m_repeat = qBound(0, m_settings.value("repeat", 0).toInt(), 2);
    m_light = m_settings.value("light", false).toBool();
    m_vinyl = m_settings.value("vinyl", false).toBool();
    m_motion = m_settings.value("motion", true).toBool();
    m_ciderAutoStart = m_settings.value("ciderAutoStart", false).toBool();
    m_miniOnTop = m_settings.value("miniOnTop", false).toBool();
    m_miniMode = m_settings.value("miniMode", false).toBool();
    m_backgroundBlur = m_settings.value("backgroundBlur", false).toBool();
    const int size = m_settings.beginReadArray("tracks");
    for (int i = 0; i < size; ++i) {
        m_settings.setArrayIndex(i);
        const auto path = m_settings.value("path").toString();
        if (!QFileInfo::exists(path) || !supported(path)) { m_tracksDirty = true; continue; }
        Track t{path, m_settings.value("title", QFileInfo(path).completeBaseName()).toString(),
                m_settings.value("artist").toString(), m_settings.value("album").toString(),
                m_settings.value("cover").toString(), m_settings.value("duration").toLongLong(), {}, 0, 0, 1};
        if (!m_settings.contains("year")) {
            const auto cover = t.cover;
            t = readTrack(path); t.cover = cover; m_tracksDirty = true;
        } else {
            t.year = m_settings.value("year").toInt(); t.number = m_settings.value("number").toInt();
            t.discNumber = m_settings.value("discNumber", 1).toInt(); t.albumArtist = m_settings.value("albumArtist").toString();
        }
        m_tracks.append(t);
    }
    m_settings.endArray();
    const auto currentPath = m_settings.value("currentPath").toString();
    if (!m_tracks.isEmpty()) {
        const auto savedPosition = m_settings.value("position", 0).toLongLong();
        int selected = 0;
        for (int i = 0; i < count(); ++i) if (m_tracks[i].path == currentPath) selected = i;
        select(selected, false);
        m_restorePosition = qBound<qint64>(0, savedPosition, duration());
    }
}

Player::~Player() {
    save();
    if (m_preparingAudio) m_audioPreparation.waitForFinished();
    // No worker keeps a Player pointer. Finish its bounded cache write before exit.
    if (m_artJobActive) {
        m_artLoader.waitForFinished();
        const auto result = m_artLoader.result();
        if (result.created && result.file != m_artFile) QFile::remove(result.file);
    }
}
void Player::ensureMedia() {
    if (m_media) return;
    m_audio = std::make_unique<QAudioOutput>();
    m_audio->setVolume(m_volume);
    m_media = std::make_unique<QMediaPlayer>();
    m_media->setAudioOutput(m_audio.get());
    connect(m_media.get(), &QMediaPlayer::playbackStateChanged, this, &Player::playingChanged);
    connect(m_media.get(), &QMediaPlayer::positionChanged, this, &Player::positionChanged);
    connect(m_media.get(), &QMediaPlayer::durationChanged, this, &Player::durationChanged);
    connect(m_audio.get(), &QAudioOutput::volumeChanged, this, &Player::volumeChanged);
    connect(m_media.get(), &QMediaPlayer::errorOccurred, this, [this](auto, const QString &message) {
        m_restorePosition = -1;
        fail("Could not play this track. " + message);
    });
    connect(m_media.get(), &QMediaPlayer::mediaStatusChanged, this, [this](auto status) {
        if ((status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia) && m_restorePosition >= 0) {
            const auto pos = m_restorePosition;
            m_restorePosition = -1;
            m_media->setPosition(qBound<qint64>(0, pos, duration()));
        }
        if (status == QMediaPlayer::EndOfMedia) next(true);
    });
    m_media->setSource(currentUrl());
}
QString Player::title() const { return m_index >= 0 ? m_tracks[m_index].title : "Add music"; }
QString Player::artist() const { return m_index >= 0 ? (m_tracks[m_index].artist.isEmpty() ? "Unknown artist" : m_tracks[m_index].artist) : QString(); }
QString Player::album() const { return m_index >= 0 ? m_tracks[m_index].album : QString(); }
QString Player::format() const { return m_index >= 0 ? QFileInfo(m_tracks[m_index].path).suffix().toUpper() : "LOCAL MUSIC PLAYER"; }
QUrl Player::currentUrl() const { return m_index >= 0 ? QUrl::fromLocalFile(m_tracks[m_index].path) : QUrl(); }
QString Player::trackId() const {
    return m_index < 0 ? "/org/mpris/MediaPlayer2/TrackList/NoTrack" : "/org/spun/track/t" +
        QString::fromLatin1(QCryptographicHash::hash(currentUrl().toEncoded(), QCryptographicHash::Sha256).toHex().left(24));
}
QVariantList Player::queue() const {
    QVariantList result;
    for (const auto &t : m_tracks) result.append(QVariantMap{{"path", t.path}, {"title", t.title}, {"artist", t.artist.isEmpty() ? "Unknown artist" : t.artist}, {"duration", t.duration}, {"artwork", queueArtworkUrl(t.path, t.cover)}});
    return result;
}
bool Player::supported(const QString &path) {
    static const QStringList extensions{"mp3", "flac", "ogg", "opus", "m4a", "aac", "wav", "aiff", "aif", "wma"};
    return extensions.contains(QFileInfo(path).suffix().toLower());
}
Track Player::readTrack(const QString &path) {
    Track t;
    t.path = QFileInfo(path).canonicalFilePath();
    t.title = QFileInfo(path).completeBaseName();
    TagLib::FileRef f(QFile::encodeName(path).constData(), true, TagLib::AudioProperties::Fast);
    if (!f.isNull()) {
        if (auto *tag = f.tag()) {
            if (!tag->title().isEmpty()) t.title = text(tag->title());
            t.artist = text(tag->artist());
            t.album = text(tag->album());
            t.year = tag->year(); t.number = tag->track();
            const auto properties = f.file()->properties();
            if (properties.contains("ALBUMARTIST")) t.albumArtist = text(properties["ALBUMARTIST"].toString());
            if (properties.contains("DISCNUMBER")) t.discNumber = qMax(1, text(properties["DISCNUMBER"].toString()).section('/',0,0).toInt());
        }
        if (auto *audio = f.audioProperties()) t.duration = audio->lengthInMilliseconds();
    }
    return t;
}
void Player::addUrls(const QList<QUrl> &urls, bool autoplay) {
    if (m_busy) { fail("Still adding music. Please try again in a moment."); return; }
    m_busy = true;
    dismissError();
    emit busyChanged();
    auto *watcher = new QFutureWatcher<QList<Track>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, autoplay] {
        const auto tracks = watcher->result();
        watcher->deleteLater();
        const int firstNew = count();
        QSet<QString> existingPaths;
        existingPaths.reserve(count() + tracks.size());
        for (const auto &track : m_tracks) existingPaths.insert(track.path);
        for (const auto &t : tracks) {
            if (!t.path.isEmpty() && !existingPaths.contains(t.path)) {
                existingPaths.insert(t.path);
                m_tracks.append(t);
            }
        }
        m_busy = false;
        emit busyChanged();
        emit queueChanged();
        if (count() > firstNew && (m_index < 0 || autoplay)) select(firstNew, autoplay);
        else if (autoplay && !tracks.isEmpty()) {
            for (int i = 0; i < count(); ++i) if (m_tracks[i].path == tracks.first().path) { select(i); break; }
        }
        else if (tracks.isEmpty()) fail("No supported audio found. Try MP3, FLAC, WAV, OGG, Opus or M4A.");
        save();
        emit imported();
    });
    watcher->setFuture(QtConcurrent::run([urls] {
        QStringList paths;
        for (const auto &url : urls) {
            if (!url.isLocalFile()) continue;
            QFileInfo info(url.toLocalFile());
            if (info.isDir()) {
                const auto entries = QDir(info.absoluteFilePath()).entryInfoList(QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
                for (const auto &entry : entries) if (supported(entry.filePath())) paths.append(entry.filePath());
            } else if (info.isFile() && supported(info.filePath())) paths.append(info.absoluteFilePath());
        }
        paths.removeDuplicates();
        QList<Track> result;
        for (const auto &path : paths) result.append(readTrack(path));
        return result;
    }));
}
void Player::select(int index, bool autoplay) {
    if (index < 0 || index >= count()) return;
    if (m_playPending) pause();
    m_restorePosition = -1;
    dismissError();
    m_index = index;
    m_shuffleVisited.insert(m_tracks[index].path);
    if (m_media) m_media->setSource(currentUrl());
    loadArt();
    emit trackChanged(); emit durationChanged(); emit positionChanged();
    if (autoplay) play();
    save();
}
void Player::loadArt() {
    ++m_artGeneration;
    m_art = {};
    m_artLoading = m_index >= 0;
    if (!m_artLoading) {
        if (!m_artFile.isEmpty()) QFile::remove(m_artFile);
        m_artFile.clear();
    }
    emit artworkChanged();
    if (!m_artJobActive && m_artLoading) startArtLoad();
}
void Player::startArtLoad() {
    m_artJobActive = true;
    const auto generation = m_artGeneration;
    const auto track = m_tracks[m_index];
    const auto cache = QFileInfo(m_settings.fileName()).absolutePath();
    m_artLoader.disconnect(this);
    connect(&m_artLoader, &QFutureWatcherBase::finished, this, [this, generation] {
        const auto result = m_artLoader.result();
        m_artJobActive = false;
        if (generation != m_artGeneration) {
            if (result.created && result.file != m_artFile) QFile::remove(result.file);
            if (m_artLoading) startArtLoad();
            return;
        }
        const auto previous = m_artFile;
        m_art = result.image;
        m_artFile = result.file;
        m_artLoading = false;
        if (!previous.isEmpty() && previous != m_artFile) QFile::remove(previous);
        emit artworkChanged();
    });
    m_artLoader.setFuture(QtConcurrent::run([track, cache] {
        LoadedArtwork result;
        result.image = readTrackArtwork(track.path, track.cover, QSize(1200,1200));
        if (result.image.isNull()) return result;
        result.image = result.image.scaled(1200,1200,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        QDir().mkpath(cache);
        const auto hash = QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char *>(result.image.constBits()), result.image.sizeInBytes()), QCryptographicHash::Sha256).toHex().left(20);
        result.file = cache + "/current-art-" + QString::fromLatin1(hash) + ".png";
        if (!QFileInfo::exists(result.file)) {
            QSaveFile file(result.file);
            result.created = file.open(QIODevice::WriteOnly) && result.image.save(&file, "PNG") && file.commit();
            if (!result.created) result.file.clear();
        }
        return result;
    }));
}
void Player::toggle() { playing() ? pause() : play(); }
void Player::play() {
    if (m_index < 0) return;
    dismissError();
    if (m_media) { m_media->play(); return; }
    if (!m_playPending) { m_playPending = true; emit playingChanged(); }
    if (m_preparingAudio) return;
    m_preparingAudio = true;
    // Qt documents these static getters as thread-safe. This initializes the
    // multimedia integration while the GUI keeps rendering and accepting input.
    // https://doc.qt.io/qt-6/qmediadevices.html
    m_audioPreparation.setFuture(QtConcurrent::run([] { (void)QMediaDevices::defaultAudioOutput(); }));
}
void Player::pause() {
    const bool pending = std::exchange(m_playPending, false);
    if (m_media) m_media->pause();
    if (pending) emit playingChanged();
}
void Player::stop() {
    const bool pending = std::exchange(m_playPending, false);
    m_restorePosition = -1;
    if (m_media) m_media->stop();
    if (pending) emit playingChanged();
    emit positionChanged();
}
void Player::next(bool automatic) {
    if (!count()) return;
    if (automatic && m_repeat == 2) { seek(0); play(); return; }
    if (m_shuffle && count() > 1) {
        QList<int> candidates;
        for (int i = 0; i < count(); ++i) if (!m_shuffleVisited.contains(m_tracks[i].path)) candidates.append(i);
        if (candidates.isEmpty()) {
            if (automatic && m_repeat == 0) { stop(); return; }
            m_shuffleVisited.clear();
            m_shuffleVisited.insert(m_tracks[m_index].path);
            for (int i = 0; i < count(); ++i) if (i != m_index) candidates.append(i);
        }
        select(candidates[QRandomGenerator::global()->bounded(candidates.size())]);
        return;
    }
    if (m_index + 1 < count()) select(m_index + 1);
    else if (!automatic || m_repeat == 1) select(0);
    else { stop(); seek(0); }
}
void Player::previous() {
    if (position() > 3000) seek(0);
    else if (count()) select((m_index - 1 + count()) % count());
}
void Player::seek(qint64 milliseconds) {
    const auto pos = qBound<qint64>(0, milliseconds, duration());
    if (!m_media || m_media->mediaStatus() == QMediaPlayer::LoadingMedia) {
        m_restorePosition = pos;
        emit positionChanged();
    } else { m_restorePosition = -1; m_media->setPosition(pos); }
}
void Player::remove(int index) {
    if (index < 0 || index >= count()) return;
    if (count() == 1) { clear(); return; }
    const bool wasPlaying = playing();
    m_tracks.removeAt(index);
    m_tracksDirty = true;
    if (index == m_index) select(qMin(index, count() - 1), wasPlaying);
    else if (index < m_index) { --m_index; emit trackChanged(); }
    emit queueChanged();
    save();
}
void Player::clear() {
    stop();
    if (m_media) m_media->setSource(QUrl());
    m_tracks.clear();
    m_shuffleVisited.clear();
    m_index = -1;
    m_restorePosition = -1;
    loadArt();
    emit trackChanged(); emit queueChanged();
    dismissError(); save();
}
void Player::setCover(const QUrl &url) {
    if (m_index < 0 || !url.isLocalFile()) return;
    QImageReader reader(url.toLocalFile());
    if (!reader.canRead()) { fail("This image could not be opened."); return; }
    m_tracks[m_index].cover = url.toLocalFile();
    loadArt(); emit queueChanged(); save();
}
void Player::demo() { addUrls({QUrl::fromLocalFile(QStringLiteral(SPUN_SOURCE_DIR "/assets/First-Light.flac"))}); }
void Player::setVolume(double value) {
    value = qBound(0., value, 1.);
    if (m_volume == value) return;
    m_volume = value;
    if (m_audio) m_audio->setVolume(value);
    else emit volumeChanged();
}
void Player::setShuffle(bool value) {
    m_shuffle = value; m_shuffleVisited.clear();
    if (m_index >= 0) m_shuffleVisited.insert(m_tracks[m_index].path);
    emit settingsChanged(); save();
}
void Player::setRepeatMode(int value) { m_repeat = qBound(0, value, 2); emit settingsChanged(); save(); }
void Player::setLight(bool value) { m_light = value; emit settingsChanged(); save(); }
void Player::setCiderAutoStart(bool value) { if (m_ciderAutoStart==value) return; m_ciderAutoStart=value; emit settingsChanged(); save(); }
void Player::move(int from, int to) {
    if (from<0 || to<0 || from>=m_tracks.size() || to>=m_tracks.size() || from==to) return;
    m_tracks.move(from,to);
    if (m_index==from) m_index=to;
    else if (from<m_index && to>=m_index) --m_index;
    else if (from>m_index && to<=m_index) ++m_index;
    emit queueChanged(); emit trackChanged(); save();
}
void Player::setMiniOnTop(bool value) { if (m_miniOnTop == value) return; m_miniOnTop = value; emit miniOnTopChanged(); save(); }
void Player::setMiniMode(bool value) { if (m_miniMode == value) return; m_miniMode = value; emit miniModeChanged(); save(); }
void Player::setBackgroundBlur(bool value) { if (m_backgroundBlur == value) return; m_backgroundBlur = value; emit backgroundBlurChanged(); save(); }
void Player::setVinyl(bool value) { if (m_vinyl==value) return; m_vinyl=value; emit vinylChanged(); save(); }
void Player::setMotion(bool value) { m_motion = value; emit settingsChanged(); save(); }
void Player::fail(const QString &message) { m_error = message; emit errorChanged(); }
void Player::dismissError() { m_error.clear(); emit errorChanged(); }
void Player::save() {
    m_settings.setValue("volume", volume());
    m_settings.setValue("shuffle", m_shuffle);
    m_settings.setValue("repeat", m_repeat);
    m_settings.setValue("light", m_light);
    m_settings.setValue("vinyl", m_vinyl);
    m_settings.setValue("motion", m_motion);
    m_settings.setValue("backgroundBlur", m_backgroundBlur);
    m_settings.setValue("miniMode", m_miniMode);
    m_settings.setValue("miniOnTop", m_miniOnTop);
    m_settings.setValue("ciderAutoStart", m_ciderAutoStart);
    m_settings.setValue("currentPath", currentUrl().toLocalFile());
    m_settings.setValue("position", position());
    if (m_tracksDirty) {
        m_settings.beginWriteArray("tracks", count());
        for (int i = 0; i < count(); ++i) {
            m_settings.setArrayIndex(i);
            const auto &t = m_tracks[i];
            m_settings.setValue("path", t.path); m_settings.setValue("title", t.title);
            m_settings.setValue("artist", t.artist); m_settings.setValue("album", t.album);
            m_settings.setValue("year", t.year); m_settings.setValue("number", t.number);
            m_settings.setValue("discNumber", t.discNumber); m_settings.setValue("albumArtist", t.albumArtist);
            m_settings.setValue("duration", t.duration); m_settings.setValue("cover", t.cover);
        }
        m_settings.endArray();
        m_tracksDirty = false;
    }
    m_settings.sync();
}

QVariantMap Player::discDetails() const {
    if (m_index < 0) return {};
    const auto &current = m_tracks[m_index];
    const auto currentFolder = QFileInfo(current.path).absolutePath();
    QList<int> indices;
    for (int i=0; i<m_tracks.size(); ++i) {
        const auto &t=m_tracks[i];
        if (i==m_index) { indices.append(i); continue; }
        if (current.album.isEmpty() || t.album!=current.album) continue;
        const bool sameArtist=(!current.albumArtist.isEmpty() ? current.albumArtist==t.albumArtist : current.artist==t.artist);
        if (sameArtist || QFileInfo(t.path).absolutePath()==currentFolder) indices.append(i);
    }
    std::stable_sort(indices.begin(), indices.end(), [&](int a,int b) {
        const auto &left=m_tracks[a], &right=m_tracks[b];
        if (left.discNumber!=right.discNumber) return left.discNumber<right.discNumber;
        const int ln=left.number ? left.number : a+1;
        const int rn=right.number ? right.number : b+1;
        return ln<rn;
    });
    QVariantList tracks;
    for (int i:indices) {
        const auto &t=m_tracks[i];
        tracks.append(QVariantMap{{"title",t.title},{"artist",t.artist},{"number",t.number},{"disc",t.discNumber},
            {"duration",t.duration},{"current",i==m_index}});
    }
    return {{"title",current.album.isEmpty() ? "Unknown album" : current.album},
        {"artist",current.albumArtist.isEmpty() ? current.artist : current.albumArtist},
        {"year",current.year>0 ? QString::number(current.year) : QString()}, {"tracks",tracks}, {"scope","loaded"}};
}

QString Player::albumKey() const {
    if(m_index<0)return {};
    const auto &t=m_tracks[m_index];
    return t.album.isEmpty() ? t.path : t.album+"|"+(t.albumArtist.isEmpty() ? QFileInfo(t.path).absolutePath() : t.albumArtist);
}
