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
    static QHash<QString, QSharedPointer<QSvgRenderer>> cache;
    auto key = name;
    QFile file(QStringLiteral(":/assets/icons/") + key + QStringLiteral(".svg"));
    if (key.contains('/') || !file.open(QIODevice::ReadOnly)) {
        key = QStringLiteral("disc");
        file.setFileName(QStringLiteral(":/assets/icons/disc.svg"));
        file.open(QIODevice::ReadOnly);
    }
    if (!cache.contains(key)) {
        auto data = file.readAll();
        // A render-only id lets Qt measure the original path's visible bounds.
        data.replace("<path ", "<path id=\"symbol\" ");
        cache.insert(key, QSharedPointer<QSvgRenderer>::create(data));
    }
    m_svg = cache.value(key);
    m_bounds = m_svg->boundsOnElement(QStringLiteral("symbol"));
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
