#pragma once
#include <QImage>
#include <QQuickImageProvider>
#include <memory>

class ArtworkCache;

QImage readTrackArtwork(const QString &path, const QString &cover, const QSize &limit);
QString queueArtworkUrl(const QString &path, const QString &cover);

class QueueArtworkProvider : public QQuickImageProvider {
public:
    QueueArtworkProvider();
    ~QueueArtworkProvider() override;
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
private:
    std::unique_ptr<ArtworkCache> m_cache;
};
