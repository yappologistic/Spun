#include "disc.h"
#include <QPainter>
#include <QPainterPath>
#include <QConicalGradient>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <cmath>
#include <array>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>

Disc::Disc(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
}
void Disc::setArtwork(const QImage &image) { if (m_art.cacheKey() == image.cacheKey()) return; m_art = image; update(); emit artworkChanged(); }
void Disc::setOverlay(bool value) { if (m_overlay == value) return; m_overlay = value; update(); emit overlayChanged(); }

QImage Disc::fallbackArt(int size) {
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 1000.0, size / 1000.0);
    QLinearGradient sky(0, 0, 700, 1000);
    sky.setColorAt(0, QColor("#1b2738")); sky.setColorAt(.36, QColor("#63637a"));
    sky.setColorAt(.59, QColor("#c99b92")); sky.setColorAt(.82, QColor("#eec5a2")); sky.setColorAt(1, QColor("#ad8b80"));
    p.fillRect(QRectF(0, 0, 1000, 1000), sky);
    QRadialGradient sun(685, 335, 290);
    sun.setColorAt(0, QColor(255, 218, 171, 190)); sun.setColorAt(.42, QColor(255, 188, 154, 65)); sun.setColorAt(1, Qt::transparent);
    p.fillRect(QRectF(0, 0, 1000, 1000), sun);
    p.setPen(Qt::NoPen); p.setBrush(QColor("#f2c7a4")); p.drawEllipse(QPointF(690, 327), 83, 83);
    for (int i = 0; i < 11; ++i) {
        QPainterPath hill;
        const double base = 520 + i * 45;
        hill.moveTo(-100, base);
        hill.cubicTo(160, base - 145 + i * 8, 345, base + 140, 630, base - 70);
        hill.cubicTo(800, base - 170, 940, base - 65, 1100, base - 120);
        hill.lineTo(1100, 1100); hill.lineTo(-100, 1100); hill.closeSubpath();
        p.setBrush(QColor::fromRgbF(.31 - i * .021, .32 - i * .021, .39 - i * .023)); p.drawPath(hill);
        p.setPen(QPen(QColor(249, 206, 181, 40), 1.3)); p.setBrush(Qt::NoBrush); p.drawPath(hill); p.setPen(Qt::NoPen);
    }
    // Deterministic film grain, generated once rather than on every animation frame.
    QRandomGenerator noise(42);
    std::array<QPen, 19> grainPens;
    for (int i = 0; i < int(grainPens.size()); ++i)
        grainPens[i] = QPen(QColor(255, 244, 221, i + 4));
    for (int i = 0; i < 46000; ++i) {
        p.setPen(grainPens[noise.bounded(4, 23) - 4]);
        p.drawPoint(QPointF(noise.bounded(1000), noise.bounded(1000)));
    }
    return image;
}

void Disc::paint(QPainter *p) {
    p->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    const double side = qMin(width(), height());
    p->translate((width() - side) / 2, (height() - side) / 2);
    p->scale(side / 1000, side / 1000);
    const QPointF center(500, 500);
    QPainterPath annulus;
    annulus.setFillRule(Qt::OddEvenFill);
    annulus.addEllipse(center, 497, 497);
    annulus.addEllipse(center, 55, 55);
    p->setClipPath(annulus);
    if (m_overlay) {
        QConicalGradient light(center, 28);
        light.setColorAt(0, QColor(255, 255, 255, 0));
        light.setColorAt(.10, QColor(242, 223, 255, 28));
        light.setColorAt(.15, QColor(255, 255, 255, 65));
        light.setColorAt(.22, QColor(255, 255, 255, 0));
        light.setColorAt(.52, QColor(255, 255, 255, 0));
        light.setColorAt(.64, QColor(195, 226, 255, 39));
        light.setColorAt(.73, QColor(255, 226, 204, 0));
        light.setColorAt(1, QColor(255, 255, 255, 0));
        p->fillPath(annulus, light);
        return;
    }
    QConicalGradient metal(center, 35);
    metal.setColorAt(0, QColor("#e9e6df")); metal.setColorAt(.12, QColor("#807a94"));
    metal.setColorAt(.22, QColor("#c3d8cc")); metal.setColorAt(.30, QColor("#f8d7c4"));
    metal.setColorAt(.41, QColor("#6b7488")); metal.setColorAt(.50, QColor("#e6e9eb"));
    metal.setColorAt(.61, QColor("#a8a0af")); metal.setColorAt(.74, QColor("#d6dab4"));
    metal.setColorAt(.84, QColor("#b9c9e4")); metal.setColorAt(.94, QColor("#f3d6dc")); metal.setColorAt(1, QColor("#e9e6df"));
    p->fillPath(annulus, metal);
    p->save();
    QPainterPath label;
    label.setFillRule(Qt::OddEvenFill);
    label.addEllipse(center, 470, 470); label.addEllipse(center, 122, 122);
    p->setClipPath(label, Qt::IntersectClip);
    if (m_labelColor.isValid()) {
        p->fillPath(label, m_labelColor);
    } else {
    if (m_art.isNull() && m_fallback.isNull()) {
        static const QImage sharedFallback = fallbackArt();
        m_fallback = sharedFallback;
    }
    const QImage &art = m_art.isNull() ? m_fallback : m_art;
    const double crop = qMin(art.width(), art.height());
    p->drawImage(QRectF(30, 30, 940, 940), art, QRectF((art.width() - crop) / 2, (art.height() - crop) / 2, crop, crop));
    }
    p->restore();
    p->setBrush(Qt::NoBrush);
    for (int radius : {478, 483, 488}) {
        p->setPen(QPen(QColor(255, 255, 255, 45), .9)); p->drawEllipse(center, radius, radius);
    }
    p->setPen(QPen(QColor(10, 12, 18, 100), 3)); p->drawEllipse(center, 122, 122);
    p->setPen(QPen(QColor(255, 255, 255, 110), 2)); p->drawEllipse(center, 115, 115);
    p->setBrush(QColor(20, 22, 29, 55)); p->setPen(Qt::NoPen); p->drawEllipse(center, 95, 95);
    p->setBrush(Qt::NoBrush); p->setPen(QPen(QColor(255, 255, 255, 110), 2)); p->drawEllipse(center, 91, 91);
    p->setPen(QPen(QColor(0, 0, 0, 150), 4)); p->drawEllipse(center, 56, 56);
    p->setPen(QPen(QColor(255, 255, 255, 150), 1.3)); p->drawEllipse(center, 59, 59);
}

class RingNode final : public QSGGeometryNode {
public:
    int capacity = 0, indexCapacity = 0, indexedSteps = -1;
};

// Small vertex buffers animate the rim without repainting or uploading a full CD-sized texture.
QSGNode *ProgressRing::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    auto *node = static_cast<RingNode *>(oldNode);
    if (!node) {
        node = new RingNode;
        node->setGeometry(new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0, 0, QSGGeometry::UnsignedIntType));
        node->geometry()->setIndexDataPattern(QSGGeometry::StaticPattern);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new QSGVertexColorMaterial);
        node->setFlag(QSGNode::OwnsMaterial);
    }
    const qreal radius = qMin(width(), height())/2-6;
    const qreal arc = 2*M_PI*m_progress;
    const int steps = qMax(1, int(std::ceil(radius*arc/2)));
    constexpr int dotSteps = 24;
    auto *geometry = node->geometry();
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    const int ringVertices = (steps+1)*4;
    const int count = ringVertices + dotSteps*9;
    const int indexCount = steps*18 + dotSteps*9;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    // Qt 6.10 can change the active vertex count without reallocating storage.
    if (count > node->capacity || indexCount > node->indexCapacity) {
        node->capacity = qMax(node->capacity, ((count + 255) / 256) * 256);
        node->indexCapacity = qMax(node->indexCapacity, ((indexCount + 255) / 256) * 256);
        geometry->allocate(node->capacity, node->indexCapacity);
        node->indexedSteps = -1;
    }
    geometry->setVertexCount(count);
    geometry->setIndexCount(indexCount);
#else
    geometry->allocate(count, indexCount);
    node->indexedSteps = -1;
#endif
    // Adjacent triangles share the exact same edge vertices. Their topology
    // changes only when the segment count changes, not as the wave animates.
    if (node->indexedSteps != steps) {
        auto *indices = geometry->indexDataAsUInt();
        for (int i=0; i<steps; ++i) for (int band=0; band<3; ++band) {
            const uint a=i*4+band, b=(i+1)*4+band;
            for (uint index : {a,b,a+1,a+1,b,b+1}) *indices++ = index;
        }
        for (int i=0; i<dotSteps*9; ++i) *indices++ = ringVertices+i;
        node->indexedSteps = steps;
        geometry->markIndexDataDirty();
    }
    auto *vertices = geometry->vertexDataAsColoredPoint2D();
    const QPointF center(width()/2, height()/2);
    const qreal alpha = m_accent.alphaF();
    const int red = qRound(m_accent.red()*alpha), green = qRound(m_accent.green()*alpha);
    const int blue = qRound(m_accent.blue()*alpha), opaque = qRound(255*alpha);
    auto vertex = [&](QPointF point, bool visible) {
        (vertices++)->set(point.x(), point.y(), visible ? red : 0, visible ? green : 0,
                         visible ? blue : 0, visible ? opaque : 0);
    };
    auto point = [&](qreal t, qreal offset) {
        const qreal envelope = qBound(0., qMin(t,arc-t)*radius/14, 1.);
        const qreal r = radius + std::sin(t*28-m_phase)*m_amplitude*envelope + offset;
        return center + QPointF(std::sin(t)*r, -std::cos(t)*r);
    };
    constexpr qreal offsets[] = {-1.9, -.9, .9, 1.9};
    constexpr bool opacity[] = {false, true, true, false};
    auto edge = [&](qreal t) {
        const qreal envelope = qBound(0., qMin(t,arc-t)*radius/14, 1.);
        const qreal r = radius + std::sin(t*28-m_phase)*m_amplitude*envelope;
        const qreal sine = std::sin(t), cosine = -std::cos(t);
        std::array<QPointF,4> points;
        for (int j=0; j<4; ++j) points[j] = center + QPointF(sine*(r+offsets[j]), cosine*(r+offsets[j]));
        return points;
    };
    for (int i=0; i<=steps; ++i) {
        const auto points = edge(arc*i/steps);
        for (int band=0; band<4; ++band) vertex(points[band],opacity[band]);
    }
    const QPointF head=point(arc,0);
    for (int i=0; i<dotSteps; ++i) {
        const qreal a=2*M_PI*i/dotSteps, b=2*M_PI*(i+1)/dotSteps;
        const QPointF av(std::cos(a),std::sin(a)), bv(std::cos(b),std::sin(b));
        vertex(head,1); vertex(head+av*2.7,1); vertex(head+bv*2.7,1);
        vertex(head+av*2.7,1); vertex(head+av*3.5,0); vertex(head+bv*2.7,1);
        vertex(head+bv*2.7,1); vertex(head+av*3.5,0); vertex(head+bv*3.5,0);
    }
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}

DiscPresentation::DiscPresentation(QObject *parent):QObject(parent) {
    m_wait.setSingleShot(true); m_wait.setInterval(1500);
    connect(&m_wait,&QTimer::timeout,this,[this]{const auto key=m_pendingKey;present({},key,m_pendingAnimate,false);});
}
void DiscPresentation::releaseOutgoing() {
    if (m_outgoing.isNull()) return;
    m_outgoing = {};
    emit changed();
}
void DiscPresentation::present(const QImage &art,const QString &key,bool animate,bool waitForArt) {
    if(art.isNull() && waitForArt && !m_key.isEmpty() && !key.isEmpty()){
        m_pendingAnimate=animate;
        if(m_pendingKey!=key || !m_wait.isActive()){m_pendingKey=key;m_wait.start();}
        return;
    }
    m_wait.stop(); m_pendingKey.clear();
    const bool swap=!key.isEmpty() && !m_key.isEmpty() && key!=m_key && animate;
    if(swap)m_outgoing=m_art;
    if (!swap && m_key == key && m_art.cacheKey() == art.cacheKey()) return;
    m_art=art; m_key=key; emit changed();
    if(swap)emit swapRequested();
}
