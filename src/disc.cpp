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

void ArtworkView::paint(QPainter *painter) {
    if(m_artwork.isNull())return;
    const auto size=QSizeF(m_artwork.size()).scaled(boundingRect().size(),Qt::KeepAspectRatio);
    const QRectF target((width()-size.width())/2,(height()-size.height())/2,size.width(),size.height());
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawImage(target,m_artwork);
}

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

// All media use the identical fallback pixels. Share one immutable image even
// after switching appearances; each painted item retains only an implicit copy.
static const QImage &sharedFallbackArt() {
    static const QImage image = Disc::fallbackArt();
    return image;
}

void Disc::paint(QPainter *p) {
    p->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    const double side = qMin(width(), height());
    p->translate((width() - side) / 2, (height() - side) / 2);
    p->scale(side / 1000, side / 1000);
    if (m_cassette) { paintCassette(p); return; }
    if (m_vinyl) { paintVinyl(p); return; }
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
        m_fallback = sharedFallbackArt();
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

void Disc::paintVinyl(QPainter *p) {
    const QPointF center(500,500);
    QPainterPath record; record.setFillRule(Qt::OddEvenFill);
    record.addEllipse(center,497,497); record.addEllipse(center,16,16);
    p->setClipPath(record);
    if (m_overlay) {
        // Fixed light over a rotating, cached record texture; no per-frame painting.
        QConicalGradient sheen(center,24);
        sheen.setColorAt(0,Qt::transparent); sheen.setColorAt(.12,QColor(190,202,218,18));
        sheen.setColorAt(.22,Qt::transparent); sheen.setColorAt(.5,Qt::transparent);
        sheen.setColorAt(.63,QColor(255,235,205,14)); sheen.setColorAt(.73,Qt::transparent); sheen.setColorAt(1,Qt::transparent);
        p->fillPath(record,sheen); return;
    }
    QRadialGradient body(center,500);
    body.setColorAt(0,QColor("#141519")); body.setColorAt(.46,QColor("#17181c"));
    body.setColorAt(.94,QColor("#101114")); body.setColorAt(1,QColor("#24252a"));
    p->fillPath(record,body);
    p->setBrush(Qt::NoBrush);
    const int inner=m_labelColor.isValid()?475:213;
    for (int r=inner;r<488;r+=4) {
        p->setPen(QPen(QColor(180,185,195,(r%12==1)?26:12),.85));p->drawEllipse(center,r,r);
        p->setPen(QPen(QColor(0,0,0,75),1));p->drawEllipse(center,r+1.5,r+1.5);
    }
    if(!m_labelColor.isValid()) {
        for(int r:{242,310,381,452}) { p->setPen(QPen(QColor(2,3,5,115),3));p->drawEllipse(center,r,r); }
    }
    const double radius=m_labelColor.isValid()?470:198;
    QPainterPath label; label.addEllipse(center,radius,radius);
    p->save();p->setClipPath(label,Qt::IntersectClip);
    if(m_labelColor.isValid())p->fillPath(label,m_labelColor);
    else {
        if(m_art.isNull()&&m_fallback.isNull()) { m_fallback=sharedFallbackArt(); }
        const auto &art=m_art.isNull()?m_fallback:m_art;const double crop=qMin(art.width(),art.height());
        p->drawImage(QRectF(500-radius,500-radius,radius*2,radius*2),art,QRectF((art.width()-crop)/2,(art.height()-crop)/2,crop,crop));
    }
    p->restore();
    p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor(0,0,0,120),3));p->drawEllipse(center,radius+1,radius+1);
    if(!m_labelColor.isValid()) {p->setPen(QPen(QColor(255,255,255,32),1));p->drawEllipse(center,178,178);}
    p->setPen(QPen(QColor(0,0,0,170),3));p->drawEllipse(center,18,18);
    p->setPen(QPen(QColor(255,255,255,40),1));p->drawEllipse(center,21,21);
}

void Disc::paintCassette(QPainter *p) {
    if(m_overlay)return;
    const QRectF shell(3,165,994,670);
    QLinearGradient plastic(0,165,160,835);
    plastic.setColorAt(0,m_shellColor.lighter(150));plastic.setColorAt(.08,m_shellColor.lighter(118));
    plastic.setColorAt(.46,m_shellColor.lighter(115));plastic.setColorAt(.94,m_shellColor.darker(118));
    plastic.setColorAt(.95,m_shellColor.darker(165));plastic.setColorAt(1,m_shellColor.darker(210));
    p->setPen(Qt::NoPen);p->setBrush(plastic);p->drawRoundedRect(shell,38,38);
    p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor(0,0,0,165),5));p->drawRoundedRect(shell.adjusted(3,3,-3,-3),35,35);
    p->setPen(QPen(QColor(255,255,255,32),2));p->drawRoundedRect(shell.adjusted(10,9,-10,-10),30,30);
    p->setPen(QPen(QColor(0,0,0,70),2));p->drawRoundedRect(shell.adjusted(18,17,-18,-18),26,26);
    // Internal posts and the tape guides show through the smoked shell.
    if(!m_labelColor.isValid()) {
        for(int side:{0,1}) {
            p->save();if(side){p->translate(1000,0);p->scale(-1,1);}
            QPainterPath brace;brace.moveTo(52,648);brace.lineTo(98,620);brace.lineTo(155,698);brace.lineTo(108,774);
            p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor(0,0,0,120),12,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p->drawPath(brace);
            p->translate(0,-2);p->setPen(QPen(QColor(171,169,147,35),3,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p->drawPath(brace);p->translate(0,2);
            QRadialGradient recess(QPointF(130,745),54);recess.setColorAt(0,QColor(0,0,0,180));recess.setColorAt(.7,QColor(0,0,0,130));recess.setColorAt(1,QColor(0,0,0,0));
            p->setPen(Qt::NoPen);p->setBrush(recess);p->drawEllipse(QPointF(130,745),54,54);
            QRadialGradient roller(QPointF(124,737),39);roller.setColorAt(0,QColor("#858371"));roller.setColorAt(.65,QColor("#414238"));roller.setColorAt(.86,QColor("#242821"));roller.setColorAt(1,QColor("#aaa38b"));
            p->setBrush(roller);p->drawEllipse(QPointF(130,745),30,30);
            p->setBrush(QColor("#171a17"));p->drawEllipse(QPointF(130,745),9,9);
            p->setBrush(QColor("#949583"));p->drawEllipse(QPointF(130,745),4,4);
            p->restore();
        }
        QPainterPath tape;tape.moveTo(252,567);tape.lineTo(104,729);tape.cubicTo(84,751,103,778,130,778);tape.lineTo(870,778);tape.cubicTo(897,778,916,751,896,729);tape.lineTo(748,567);
        p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor("#17100d"),9));p->drawPath(tape);
        p->setPen(QPen(QColor("#76503a"),3));p->drawPath(tape);
        // Subtle smoke tint belongs to the plastic above the mechanism.
        p->setPen(Qt::NoPen);p->setBrush(QColor(55,53,43,35));p->drawRoundedRect(shell.adjusted(20,20,-20,-20),23,23);
    }
    // Matte grain is baked into the shell once, not regenerated by reel motion.
    QRandomGenerator grain(61);
    for(int i=0;i<3600;++i) {
        p->setPen(QPen(i%2?QColor(255,255,255,9):QColor(0,0,0,18),1));
        p->drawPoint(QPointF(23+grain.bounded(954),185+grain.bounded(630)));
    }
    const QRectF label(54,m_labelColor.isValid()?195:210,892,m_labelColor.isValid()?610:423);
    QPainterPath clip;clip.addRoundedRect(label,24,24);
    p->save();p->setClipPath(clip);
    if(m_labelColor.isValid())p->fillPath(clip,m_labelColor);
    else {
        if(m_art.isNull() && m_fallback.isNull()) {m_fallback=sharedFallbackArt();}
        const auto &art=m_art.isNull()?m_fallback:m_art;
        const double scale=qMax(label.width()/art.width(),label.height()/art.height());
        const QSizeF crop(label.width()/scale,label.height()/scale);
        p->drawImage(label,art,QRectF((art.width()-crop.width())/2,(art.height()-crop.height())/2,crop.width(),crop.height()));
        p->fillPath(clip,QColor(226,215,188,16));
    }
    p->restore();
    p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor(0,0,0,115),3));p->drawPath(clip);
    p->setPen(QPen(QColor(247,232,202,45),1));p->drawRoundedRect(label.adjusted(3,3,-3,-3),22,22);
    if(!m_labelColor.isValid()) {
        // Slight paper fibers and an uneven ink edge keep the label tactile.
        const QRectF paper(58.5,214.6,883,105);
        QLinearGradient stock(0,214,0,320);stock.setColorAt(0,QColor("#ece5cf"));stock.setColorAt(1,QColor("#d9cfb5"));
        p->setBrush(stock);p->setPen(QPen(QColor("#726b56"),1));p->drawRoundedRect(paper,3,3);
        p->save();p->setClipRect(paper.adjusted(2,2,-2,-2));
        QRandomGenerator fibers(108);
        p->setPen(QPen(QColor(86,72,42,20),.7));
        for(int i=0;i<950;++i){const QPointF a(60+fibers.bounded(879),216+fibers.bounded(101));p->drawLine(a,a+QPointF(1+fibers.bounded(4),0));}
        p->restore();
        // A recessed smoked window joins the two reel wells.
        const QRectF window(132,356,736,216);
        p->setPen(QPen(QColor(0,0,0,200),8));p->setBrush(QColor("#141516"));p->drawRoundedRect(window,24,24);
        p->setPen(QPen(QColor(230,235,233,44),2));p->setBrush(Qt::NoBrush);p->drawRoundedRect(window.adjusted(4,5,-4,-4),20,20);
        QLinearGradient glass(0,368,0,560);
        glass.setColorAt(0,QColor(194,205,205,34));glass.setColorAt(.35,QColor(99,107,107,12));glass.setColorAt(1,QColor(0,0,0,60));
        p->setPen(Qt::NoPen);p->setBrush(glass);p->drawRoundedRect(window.adjusted(8,8,-8,-8),16,16);
        p->setPen(QPen(QColor("#684735"),6));p->drawLine(QPointF(371,511),QPointF(629,511));
        p->setPen(QPen(QColor("#aa7c53"),1));p->drawLine(QPointF(371,508),QPointF(629,508));
        for(int i=0;i<9;++i) {
            const double x=420+i*20;p->setPen(QPen(QColor(223,213,186,i%4==0?150:75),2));
            p->drawLine(QPointF(x,440),QPointF(x,i%4==0?468:455));
        }
        // Thin reflection on the clear window, with the mechanism visible below.
        QPainterPath reflection;reflection.moveTo(145,365);reflection.lineTo(260,365);reflection.lineTo(205,559);reflection.lineTo(145,559);reflection.closeSubpath();
        p->setPen(Qt::NoPen);p->setBrush(QColor(228,237,230,9));p->drawPath(reflection);
        // Molded stiffening ribs and the raised tape-head housing.
        for(int i=0;i<18;++i) {
            if(i==8 || i==9)continue;
            const int x=75+i*50;p->setPen(QPen(QColor(0,0,0,95),4));p->drawLine(QPointF(x,672),QPointF(x+29,672));
            p->setPen(QPen(QColor(255,255,255,22),2));p->drawLine(QPointF(x,676),QPointF(x+29,676));
        }
        QPainterPath lip;lip.moveTo(202,818);lip.lineTo(237,707);lip.lineTo(763,707);lip.lineTo(798,818);lip.closeSubpath();
        QLinearGradient molded(0,706,0,822);molded.setColorAt(0,m_shellColor.lighter(130));molded.setColorAt(.08,m_shellColor.darker(125));molded.setColorAt(1,m_shellColor.darker(160));
        p->setPen(QPen(QColor(255,255,255,30),2));p->setBrush(molded);p->drawPath(lip);
        for(int x:{290,710}) {
            p->setPen(QPen(QColor(255,255,255,30),2));p->setBrush(QColor("#101112"));p->drawEllipse(QPointF(x,778),19,19);
            p->setBrush(QColor("#08090a"));p->setPen(Qt::NoPen);p->drawEllipse(QPointF(x,776),14,14);
        }
        p->setBrush(QColor("#0b0c0d"));p->drawRoundedRect(QRectF(425,758,150,43),7,7);
        p->setPen(QPen(QColor("#714b34"),5));p->drawLine(QPointF(427,778),QPointF(573,778));
        p->setPen(QPen(QColor("#afa58a"),2));p->drawLine(QPointF(451,789),QPointF(549,789));
        p->setPen(Qt::NoPen);p->setBrush(QColor("#988872"));p->drawRoundedRect(QRectF(477,781,46,8),1,1);
        for(int x:{367,618}) {p->setBrush(QColor("#0b0c0d"));p->drawRoundedRect(QRectF(x,767,15,31),6,6);}
    }
    // The two molded shell halves meet at a narrow seam. Grip ribs catch light
    // on one edge only, so they read as plastic rather than an outer UI border.
    for(int side:{0,1}) for(int row=0;row<10;++row) {
        const qreal x=side?954:21,y=324+row*29;
        p->setPen(QPen(QColor(0,0,0,110),3));p->drawLine(QPointF(x,y),QPointF(x+24,y));
        p->setPen(QPen(QColor(255,255,255,25),1));p->drawLine(QPointF(x,y+2),QPointF(x+24,y+2));
    }
    // Recessed fasteners, including the center screw above the head opening.
    const QList<QPointF> screws=m_labelColor.isValid()
        ? QList<QPointF>{{32,195},{968,195},{32,802},{968,802}}
        : QList<QPointF>{{32,195},{968,195},{32,802},{968,802},{500,673}};
    for(QPointF point:screws) {
        QRadialGradient well(point,21);well.setColorAt(0,QColor(0,0,0,210));well.setColorAt(.7,QColor(0,0,0,180));well.setColorAt(1,QColor(0,0,0,0));
        p->setPen(Qt::NoPen);p->setBrush(well);p->drawEllipse(point,21,21);
        p->setPen(Qt::NoPen);p->setBrush(QColor(0,0,0,145));p->drawEllipse(point+QPointF(0,2),15,15);
        QRadialGradient steel(point-QPointF(4,5),19);steel.setColorAt(0,QColor("#a5a49b"));steel.setColorAt(.4,QColor("#6e706a"));steel.setColorAt(1,QColor("#272a29"));
        p->setBrush(steel);p->drawEllipse(point,11,11);
        p->setPen(QPen(QColor("#1a1c1b"),3));p->drawLine(point+QPointF(-6,-3),point+QPointF(6,3));
        p->drawLine(point+QPointF(-3,6),point+QPointF(3,-6));
    }
}

class RingNode final : public QSGGeometryNode {
public:
    int capacity = 0, indexCapacity = 0, indexedSteps = -1, indexedDotSteps = -1;
    struct Sample { qreal t, sine, cosine, waveSine, waveCosine, envelope; };
    QList<Sample> samples;
    qreal sampledArc = -1, sampledRadius = -1;
    bool sampledLinear = false;
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
    const qreal radius = m_linear ? 1 : qMin(width(), height())/2-6;
    const qreal arc = m_linear ? qMax(0.,width())*m_progress : 2*M_PI*m_progress;
    const int steps = qMax(1, int(std::ceil(radius*arc/2)));
    const int dotSteps = m_linear ? 0 : 24;
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
    if (node->indexedSteps != steps || node->indexedDotSteps != dotSteps) {
        auto *indices = geometry->indexDataAsUInt();
        for (int i=0; i<steps; ++i) for (int band=0; band<3; ++band) {
            const uint a=i*4+band, b=(i+1)*4+band;
            for (uint index : {a,b,a+1,a+1,b,b+1}) *indices++ = index;
        }
        for (int i=0; i<dotSteps*9; ++i) *indices++ = ringVertices+i;
        node->indexedSteps = steps;
        node->indexedDotSteps = dotSteps;
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
    // Progress changes less often than the animated phase. Reuse the expensive
    // trigonometry between progress updates, retaining the same tessellation.
    if (node->samples.size() != steps+1 || node->sampledArc != arc || node->sampledRadius != radius || node->sampledLinear != m_linear) {
        node->samples.resize(steps+1);
        for (int i=0; i<=steps; ++i) {
            const qreal t=arc*i/steps, wave=t*(m_linear?2*M_PI/24:28);
            node->samples[i]={t,std::sin(t),-std::cos(t),std::sin(wave),std::cos(wave),qBound(0.,qMin(t,arc-t)*radius/14,1.)};
        }
        node->sampledArc=arc;node->sampledRadius=radius;node->sampledLinear=m_linear;
    }
    const qreal phaseSine=std::sin(m_phase),phaseCosine=std::cos(m_phase);
    constexpr qreal offsets[] = {-1.9, -.9, .9, 1.9};
    constexpr bool opacity[] = {false, true, true, false};
    for (const auto &sample : std::as_const(node->samples)) {
        const qreal wave=(sample.waveSine*phaseCosine-sample.waveCosine*phaseSine)*m_amplitude*sample.envelope;
        for (int band=0; band<4; ++band) {
            const qreal r=radius+wave+offsets[band];
            vertex(m_linear?QPointF(sample.t,height()/2+wave+offsets[band]):center+QPointF(sample.sine*r,sample.cosine*r),opacity[band]);
        }
    }
    const QPointF head=center+QPointF(std::sin(arc)*radius,-std::cos(arc)*radius);
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
