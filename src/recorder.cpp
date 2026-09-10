#include "recorder.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QConicalGradient>
#include <QRandomGenerator>
#include <cmath>

void RecorderSurface::paint(QPainter *p) {
    p->setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);
    p->scale(width()/410.,height()/410.);
    const QPointF center(205,160);
    const auto metal=[&](QRectF rect,double radius){
        p->setPen(Qt::NoPen);p->setBrush(QColor("#747a80"));p->drawRoundedRect(rect.translated(0,.8),radius,radius);
        QLinearGradient bevel(rect.topLeft(),rect.bottomRight());
        bevel.setColorAt(0,QColor("#fafbfc"));bevel.setColorAt(.25,QColor("#ccd0d3"));bevel.setColorAt(.72,QColor("#747b81"));bevel.setColorAt(1,QColor("#e2e5e7"));
        p->setBrush(bevel);p->drawRoundedRect(rect,radius,radius);
        QLinearGradient face(rect.topLeft(),rect.bottomRight());face.setColorAt(0,QColor("#d5d8db"));face.setColorAt(.42,QColor("#c9cccf"));face.setColorAt(1,QColor("#b7bcc1"));
        p->setBrush(face);p->drawRoundedRect(rect.adjusted(1,1,-1,-1.3),qMax(0.,radius-1),qMax(0.,radius-1));
    };
    const auto grain=[&](const QPainterPath &clip,unsigned seed,int count){
        p->save();p->setClipPath(clip);QRandomGenerator random(seed);const auto bounds=clip.boundingRect();
        for(int i=0;i<count;++i){const int v=random.bounded(2)?255:65;p->setPen(QColor(v,v,v,random.bounded(3,12)));p->drawPoint(QPointF(bounds.left()+random.generateDouble()*bounds.width(),bounds.top()+random.generateDouble()*bounds.height()));}p->restore();
    };
    if(m_part==1) {
        QConicalGradient g(center,40);g.setColorAt(0,QColor("#c7ccd0"));g.setColorAt(.22,QColor("#dadee1"));g.setColorAt(.49,QColor("#c2c7cc"));g.setColorAt(.75,QColor("#d4d8dc"));g.setColorAt(1,QColor("#c7ccd0"));
        p->setPen(QPen(QColor("#edf0f2"),.7));p->setBrush(g);p->drawEllipse(center,113,113);
        QPainterPath path;path.addEllipse(center,112,112);grain(path,721,7000);
        p->setPen(QPen(QColor(64,73,80,75),.35));p->drawLine(QPointF(205,49),QPointF(205,123));p->drawLine(QPointF(205,197),QPointF(205,271));
        p->setPen(QPen(QColor("#eef1f3"),.6));p->setBrush(QColor("#252b31"));p->drawEllipse(center,31,31);
        p->setPen(QPen(QColor("#9eabb4"),.45));p->setBrush(Qt::NoBrush);p->drawEllipse(center,30.25,30.25);
        if(!m_artwork.isNull()) {
            p->save();QPainterPath art;art.addEllipse(center,26,26);p->setClipPath(art);p->drawImage(QRectF(179,134,52,52),m_artwork);p->restore();
        }
        for(int i=0;i<3;++i){const double a=i*2*std::acos(-1.)/3;const QPointF at=center+QPointF(28.5*std::sin(a),-28.5*std::cos(a));p->setPen(QPen(QColor("#e0e4e8"),.5));p->setBrush(QColor("#5c636a"));p->drawEllipse(at,1.15,1.15);p->setPen(QPen(QColor("#252c32"),.4));p->drawLine(at+QPointF(-.5,.5),at+QPointF(.5,-.5));}
        return;
    }
    if(m_part==2) {
        p->setPen(QColor("#41484f"));QFont font("sans-serif");font.setPixelSize(17);font.setWeight(QFont::Light);p->setFont(font);p->drawText(QRectF(100,32,80,25),Qt::AlignLeft|Qt::AlignVCenter,"TP-7");
        p->setPen(Qt::NoPen);p->setBrush(QColor("#e97626"));p->drawRoundedRect(QRectF(315,82,6,6),.4,.4);
        p->setBrush(QColor("#515b62"));p->drawEllipse(QPointF(169,44),1.15,1.15);p->setBrush(QColor("#9fa5ab"));p->drawEllipse(QPointF(206,44),1.15,1.15);
        return;
    }
    if(m_part==3) {
        metal(QRectF(65,69,10,186),4);metal(QRectF(65,154,18,17),8);
        p->setPen(QPen(QColor("#edf1f3"),.6));p->setBrush(QColor("#78828b"));p->drawEllipse(QPointF(72,162.5),3.3,3.3);
        p->setPen(QPen(QColor("#323a42"),.55));p->drawLine(QPointF(70.6,163.8),QPointF(73.4,161.1));
        return;
    }
    for(int i=8;i>0;--i){p->setPen(Qt::NoPen);p->setBrush(QColor(0,0,0,4));p->drawRoundedRect(QRectF(85-i/2.,24-i/3.,244+i,352+i),12,12);}
    p->setBrush(QColor("#da7024"));p->drawRoundedRect(QRectF(86.5,22,244,352),11,11);
    metal(QRectF(85,20,244,352),12);
    QPainterPath path;path.addRoundedRect(QRectF(87,22,240,347),10,10);grain(path,7201,13000);
    p->setPen(QPen(QColor("#828b93"),.6));p->setBrush(QColor("#313940"));p->drawEllipse(center,114.5,114.5);
    p->setPen(QPen(QColor("#eff3f5"),.6));p->setBrush(QColor("#0a1117"));p->drawRoundedRect(QRectF(267,30,52,29),4,4);
    metal(QRectF(112,0,8,21),1.5);
    p->setPen(Qt::NoPen);p->setBrush(QColor("#a0a8ae"));p->drawEllipse(QPointF(115,263),9,9);
    p->setBrush(QColor("#20262b"));p->drawRoundedRect(QRectF(85,286,187,86),1.8,1.8);
    p->save();p->translate(298,255.5);p->rotate(46.3);
    p->setPen(QPen(QColor("#eef1f3"),.6));p->setBrush(QColor("#aeb6bc"));p->drawRoundedRect(QRectF(-12.5,-27.5,25,55),12.5,12.5);p->restore();
    // The adapter has a cylindrical silhouette, a diamond knurl and a retaining collar.
    metal(QRectF(279,369,31,6),1.5);metal(QRectF(277,375,35,29),3.5);
    QLinearGradient cylinder(277,0,312,0);cylinder.setColorAt(0,QColor("#7d858c"));cylinder.setColorAt(.25,QColor("#e0e5e8"));cylinder.setColorAt(.7,QColor("#a6b0b7"));cylinder.setColorAt(1,QColor("#626e77"));
    p->setPen(Qt::NoPen);p->setBrush(cylinder);p->drawRoundedRect(QRectF(277.5,375.5,34,28),3,3);
    p->save();p->setClipRect(QRectF(278.5,377,32,24));
    for(double x=251;x<340;x+=4.6){p->setPen(QPen(QColor("#73818c"),.65));p->drawLine(QPointF(x,377),QPointF(x+24,401));p->setPen(QPen(QColor("#e7ecef"),.65));p->drawLine(QPointF(x,377),QPointF(x-24,401));}p->restore();
    p->setPen(QPen(QColor("#ecf0f2"),.65));p->drawLine(QPointF(280,376.5),QPointF(309,376.5));
    p->setPen(QPen(QColor("#67757f"),.65));p->drawLine(QPointF(280,402.2),QPointF(309,402.2));
    for(int x:{294,308}){p->setPen(Qt::NoPen);p->setBrush(QColor("#646c73"));p->drawRoundedRect(QRectF(x,290,1.6,69),.8,.8);p->setPen(QPen(QColor("#e7ecef"),.5));p->drawLine(QPointF(x+1.9,291),QPointF(x+1.9,358));}
    p->setPen(QPen(QColor("#79828a"),.4));p->drawLine(QPointF(283,314),QPointF(319,314));
}
