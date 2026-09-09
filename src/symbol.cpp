#include "symbol.h"
#include <QFile>
#include <QHash>
#include <QPainter>

Symbol::Symbol(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    setName(QStringLiteral("play"));
}
void Symbol::setName(const QString &name) {
    if (name == m_name) return;
    // The cache can contain only the finite, bundled symbol set. SVGs are parsed
    // once, independently of theme, scale, hover feedback, and delegate recycling.
    struct CachedSymbol { QSharedPointer<QSvgRenderer> renderer; QRectF bounds; };
    static QHash<QString, CachedSymbol> cache;
    auto key = name;
    auto found = cache.constFind(key);
    if (found == cache.cend()) {
        QFile file(QStringLiteral(":/assets/icons/") + key + QStringLiteral(".svg"));
        if (key.contains('/') || !file.open(QIODevice::ReadOnly)) {
            key = QStringLiteral("disc");
            file.setFileName(QStringLiteral(":/assets/icons/disc.svg"));
            file.open(QIODevice::ReadOnly);
        }
        found = cache.constFind(key);
        if (found == cache.cend()) {
            auto data = file.readAll();
            // A render-only id lets Qt measure the original path's visible bounds.
            data.replace("<path ", "<path id=\"symbol\" ");
            auto renderer = QSharedPointer<QSvgRenderer>::create(data);
            const auto bounds = renderer->boundsOnElement(QStringLiteral("symbol"));
            found = cache.insert(key, {renderer, bounds});
        }
    }
    m_svg = found->renderer;
    m_bounds = found->bounds;
    m_name = name;
    emit nameChanged();
    update();
}
void Symbol::setInk(const QColor &ink) {
    if (ink == m_ink) return;
    m_ink = ink; emit inkChanged(); update();
}
void Symbol::paint(QPainter *painter) {
    if (!m_svg || m_bounds.isEmpty()) return;
    const auto scale = qMin(width(), height()) * .78 / qMax(m_bounds.width(), m_bounds.height());
    const QSizeF size(m_bounds.width() * scale, m_bounds.height() * scale);
    const QRectF target(QPointF((width() - size.width()) / 2, (height() - size.height()) / 2), size);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    m_svg->render(painter, QStringLiteral("symbol"), target);
    painter->setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter->fillRect(boundingRect(), m_ink);
    painter->restore();
}
