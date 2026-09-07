#include "artwork.h"
#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QCache>
#include <QCryptographicHash>
#include <QMutex>
#include <taglib/fileref.h>
#include <taglib/tvariant.h>

// Only thumbnail pixels are retained, capped at 1 MiB per provider. Full disc
// artwork does not enter this cache. Copies of a hit share the same image data.
class ArtworkCache {
public:
    QImage find(const QByteArray &key) {
        QMutexLocker lock(&mutex);
        const auto *image = images.object(key);
        return image ? *image : QImage();
    }
    void insert(const QByteArray &key, const QImage &image) {
        QMutexLocker lock(&mutex);
        images.insert(key,new QImage(image),qMax(1,int((image.sizeInBytes()+1023)/1024)));
    }
private:
    QMutex mutex;
    QCache<QByteArray,QImage> images{1024};
};

static QImage decode(QImageReader &reader, const QSize &limit, ArtworkCache *cache=nullptr, QByteArray key={}) {
    if (cache) {
        key += ':' + QByteArray::number(limit.width()) + 'x' + QByteArray::number(limit.height());
        auto hit = cache->find(key);
        if (!hit.isNull()) return hit;
    }
    reader.setAutoTransform(true);
    const auto original = reader.size();
    if (original.isValid() && limit.isValid()) reader.setScaledSize(original.scaled(limit, Qt::KeepAspectRatio));
    auto image = reader.read();
    if (cache && !image.isNull()) cache->insert(key,image);
    return image;
}
static QImage decodeFile(const QString &path, const QSize &limit, ArtworkCache *cache) {
    QByteArray key;
    if (cache) {
        const QFileInfo info(path);
        if (!info.isFile()) return {};
        key = "file:" + info.absoluteFilePath().toUtf8() + '\0' + QByteArray::number(info.size())
            + ':' + QByteArray::number(info.lastModified().toMSecsSinceEpoch());
    }
    QImageReader reader(path);
    return decode(reader,limit,cache,key);
}
static QImage readArtwork(const QString &path, const QString &cover, const QSize &limit, ArtworkCache *cache) {
    if (!cover.isEmpty()) {
        auto image = decodeFile(cover,limit,cache);
        if (!image.isNull()) return image;
    }
    TagLib::FileRef file(QFile::encodeName(path).constData(), false);
    if (!file.isNull()) {
        const auto pictures = file.complexProperties("PICTURE");
        for (bool frontOnly : {true, false}) for (const auto &picture : pictures) {
            if (!picture.contains("data")) continue;
            const bool front = picture.contains("pictureType") && picture["pictureType"].toString() == "Front Cover";
            if (front != frontOnly) continue;
            const auto bytes = picture["data"].toByteVector();
            // The TagLib byte vector stays alive throughout this synchronous
            // decode, so the buffer need not copy the embedded image bytes.
            QByteArray data = QByteArray::fromRawData(bytes.data(), int(bytes.size()));
            QBuffer buffer(&data); buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer);
            const auto key = cache ? QByteArray("embedded:") + QCryptographicHash::hash(data,QCryptographicHash::Sha256) : QByteArray();
            auto image = decode(reader,limit,cache,key);
            if (!image.isNull()) return image;
        }
    }
    const auto info = QFileInfo(path);
    const auto dir = info.absoluteDir();
    for (const auto &name : QStringList{info.completeBaseName()+".jpg", "cover.jpg", "cover.png", "folder.jpg", "folder.png", "front.jpg"}) {
        auto image = decodeFile(dir.filePath(name),limit,cache);
        if (!image.isNull()) return image;
    }
    return {};
}
QImage readTrackArtwork(const QString &path, const QString &cover, const QSize &limit) {
    return readArtwork(path,cover,limit,nullptr);
}
QString queueArtworkUrl(const QString &path, const QString &cover) {
    const auto data = QJsonDocument(QJsonObject{{"path",path}, {"cover",cover},
        {"modified",QFileInfo(path).lastModified().toMSecsSinceEpoch()},
        {"coverModified",QFileInfo(cover).lastModified().toMSecsSinceEpoch()}}).toJson(QJsonDocument::Compact);
    return "image://queueart/" + QString::fromLatin1(data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}
QueueArtworkProvider::QueueArtworkProvider() : QQuickImageProvider(Image,ForceAsynchronousImageLoading), m_cache(std::make_unique<ArtworkCache>()) {}
QueueArtworkProvider::~QueueArtworkProvider() = default;
QImage QueueArtworkProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    const auto track = QJsonDocument::fromJson(QByteArray::fromBase64(id.toLatin1(), QByteArray::Base64UrlEncoding)).object();
    const auto limit = requestedSize.isValid() ? requestedSize.boundedTo(QSize(128,128)) : QSize(96,96);
    auto image = readArtwork(track.value("path").toString(), track.value("cover").toString(), limit,m_cache.get());
    if (image.isNull()) {
        // The themed disc placeholder beneath the image remains visible.
        image=QImage(1,1,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
    }
    if (size) *size=image.size();
    return image;
}
