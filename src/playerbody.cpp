#include "playerbody.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QRandomGenerator>

void PlayerBody::paint(QPainter *p) {
    p->setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);
    p->scale(width()/440.,height()/440.);
    const bool tape=m_medium=="cassette",cd=m_medium=="cd";
    const auto metal=[&](const QRectF &r,qreal radius){
        // Broad face, rounded bevel and a dark underside share one light source.
        const QColor base=m_surface;
        p->setPen(Qt::NoPen);p->setBrush(base.darker(210));
        p->drawRoundedRect(r,radius,radius);
        QLinearGradient edge(r.topLeft(),r.bottomRight());
        edge.setColorAt(0,base.lighter(165));edge.setColorAt(.26,base.lighter(115));
        edge.setColorAt(.64,base.darker(160));edge.setColorAt(1,base.darker(230));
        p->setBrush(edge);p->drawRoundedRect(r.adjusted(.7,.5,-.7,-1.7),radius,radius);
        const QRectF face=r.adjusted(2,2,-2,-5);
        QLinearGradient g(face.topLeft(),face.bottomRight());
        g.setColorAt(0,base.lighter(130));g.setColorAt(.35,base.lighter(112));
        g.setColorAt(1,base.darker(115));
        p->setBrush(g);p->drawRoundedRect(face,qMax(1.,radius-2),qMax(1.,radius-2));
        p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor(245,249,255,45),.65));
        p->drawRoundedRect(r.adjusted(1,1,-1,-2),radius,radius);
    };
    const auto screw=[&](QPointF at){
        p->setPen(Qt::NoPen);p->setBrush(QColor(0,0,0,135));p->drawEllipse(at+QPointF(0,.8),4.4,4.4);
        QConicalGradient g(at,35);g.setColorAt(0,QColor("#c4c7c6"));g.setColorAt(.25,QColor("#4a4f51"));g.setColorAt(.55,QColor("#b0b7b9"));g.setColorAt(.8,QColor("#303639"));g.setColorAt(1,QColor("#c4c7c6"));
        p->setPen(QPen(QColor(0,0,0,170),.65));p->setBrush(g);p->drawEllipse(at,3.1,3.1);
        p->setPen(QPen(QColor(0,0,0,210),.85));
        p->drawLine(at+QPointF(-1.5,1.5),at+QPointF(1.5,-1.5));
        p->drawLine(at+QPointF(-1.5,-1.5),at+QPointF(1.5,1.5));
    };
    const auto glassEdge=[&](const QPainterPath &path){
        p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor(0,0,0,145),4));p->drawPath(path.translated(0,1.5));
        p->setPen(QPen(QColor(171,197,203,65),2.1));p->drawPath(path);
        p->setPen(QPen(QColor(244,251,255,80),.55));p->drawPath(path.translated(-.35,-.65));
    };
    const auto hinge=[&](QRectF r){
        QLinearGradient chrome(r.topLeft(),r.bottomLeft());
        chrome.setColorAt(0,QColor("#262b2d"));chrome.setColorAt(.22,QColor("#c1c5c3"));
        chrome.setColorAt(.45,QColor("#747b7c"));chrome.setColorAt(.78,QColor("#303637"));chrome.setColorAt(1,QColor("#151a1b"));
        p->setPen(QPen(QColor(0,0,0,170),.6));p->setBrush(chrome);p->drawRoundedRect(r,2,2);
        p->setPen(QPen(QColor(0,0,0,120),.7));
        for(double x=r.left()+4;x<r.right();x+=5)p->drawLine(QPointF(x,r.top()+1),QPointF(x,r.bottom()-1));
    };
    if(m_layer>=2) {
        const QRectF r=m_layer==3 ? QRectF(30,20,380,410) : tape?QRectF(40,106,360,228):QRectF(44,44,352,352);
        p->setPen(Qt::NoPen);p->setBrush(QColor(0,0,0,100));p->drawRoundedRect(r.translated(2,5),5,5);
        metal(r,cd?5:2);
        const QRectF cover=r.adjusted(cd?18:5,5,-5,-5);
        if(!m_artwork.isNull()) {
            const auto side=qMin(m_artwork.width(),m_artwork.height());
            p->drawImage(cover,m_artwork,QRectF((m_artwork.width()-side)/2.,(m_artwork.height()-side)/2.,side,side));
        }
        if(cd){
            p->fillRect(QRectF(r.x()+3,r.y()+5,10,r.height()-10),QColor(180,193,199,70));
            p->setPen(QPen(QColor(240,250,255,95),.6));
            for(int x=0;x<5;++x)p->drawLine(QPointF(r.x()+4+x*2,r.top()+6),QPointF(r.x()+4+x*2,r.bottom()-6));
        }else if(!tape){
            p->setPen(QPen(QColor(0,0,0,65),1));p->drawLine(r.topRight()-QPointF(2,0),r.bottomRight()-QPointF(2,0));
        }
        QLinearGradient gloss(r.topLeft(),r.bottomRight());gloss.setColorAt(0,QColor(255,255,255,cd?40:12));gloss.setColorAt(.48,Qt::transparent);gloss.setColorAt(.5,QColor(255,255,255,cd?16:4));gloss.setColorAt(1,Qt::transparent);
        p->fillRect(r.adjusted(1,1,-1,-1),gloss);return;
    }
    if(m_layer==1) {
        if(m_medium=="vinyl") {
            QLinearGradient acrylic(0,8,0,63);acrylic.setColorAt(0,QColor(222,236,242,18));acrylic.setColorAt(1,QColor(204,225,237,2));
            p->setBrush(acrylic);p->setPen(QPen(QColor(220,235,243,48),.8));p->drawRoundedRect(QRectF(15,9,410,51),7,7);
            hinge(QRectF(69,9,24,7));hinge(QRectF(347,9,24,7));
            // The spindle belongs to the deck and remains stationary above the label.
            QRadialGradient spindle(QPointF(218.5,217),6);spindle.setColorAt(0,QColor("#e6ebed"));spindle.setColorAt(.5,QColor("#a5abad"));spindle.setColorAt(.8,QColor("#50585d"));spindle.setColorAt(1,QColor("#20272b"));
            p->setPen(Qt::NoPen);p->setBrush(QColor(0,0,0,125));p->drawEllipse(QPointF(221,222),6,6);
            p->setBrush(spindle);p->drawEllipse(QPointF(220,220),4.5,5.5);return;
        }
        QPainterPath glass;
        if(tape)glass.addRoundedRect(QRectF(42,112,356,225),12,12);
        else glass.addEllipse(QPointF(220,220),194,194);
        QLinearGradient g(60,70,345,365);g.setColorAt(0,QColor(219,237,244,32));g.setColorAt(.32,QColor(200,220,230,2));g.setColorAt(.33,QColor(243,250,255,22));g.setColorAt(.48,QColor(235,247,255,4));g.setColorAt(1,QColor(8,12,18,30));
        p->fillPath(glass,g);glassEdge(glass);
        p->save();p->setClipPath(glass);
        // Restrained hairline scuffs catch the same overhead light as the bevel.
        p->setPen(QPen(QColor(241,246,247,13),.45));
        for(int i=0;i<11;++i){const double x=65+i*27;const double y=tape?124+(i%4)*4:71+(i%4)*5;p->drawLine(QPointF(x,y),QPointF(x+12+(i%3)*6,y-2));}
        p->restore();
        if(tape){hinge(QRectF(80,107,31,8));hinge(QRectF(329,107,31,8));metal(QRectF(195,334,50,9),3);}
        else{
            hinge(QRectF(159,26,24,8));hinge(QRectF(257,26,24,8));
            QConicalGradient clamp(QPointF(220,220),25);clamp.setColorAt(0,QColor("#b8bfc0"));clamp.setColorAt(.4,QColor("#45494d"));clamp.setColorAt(.7,QColor("#dae0df"));clamp.setColorAt(1,QColor("#b8bfc0"));
            p->setPen(QPen(QColor(0,0,0,180),1));p->setBrush(clamp);p->drawEllipse(QPointF(220,220),21,21);
            p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor(235,242,241,55),.7));
            p->drawEllipse(QPointF(220,220),18,18);p->drawEllipse(QPointF(220,220),15,15);
            p->setBrush(QColor("#303338"));p->setPen(QPen(QColor(0,0,0,150),.8));p->drawEllipse(QPointF(220,220),8,8);
        }
        return;
    }
    const QRectF body=tape?QRectF(19,83,402,283):QRectF(6,6,428,428);
    p->setPen(Qt::NoPen);
    for(int i=6;i>0;--i){p->setBrush(QColor(0,0,0,7));p->drawRoundedRect(body.adjusted(-i,-i,i,i).translated(0,3),cd?220:23,cd?220:23);}
    metal(body,cd?214:22);
    p->save();QPainterPath face;face.addRoundedRect(body.adjusted(3,3,-3,-7),cd?211:20,cd?211:20);p->setClipPath(face);
    QRandomGenerator grain(317);
    for(int i=0;i<2600;++i){
        const double x=body.left()+grain.generateDouble()*body.width(),y=body.top()+grain.generateDouble()*body.height();
        p->setPen(QPen(i%3?QColor(255,255,255,7):QColor(0,0,0,18),.45));
        p->drawLine(QPointF(x,y),QPointF(x+(tape?1.2:3.5),y));
    }
    // A broad reflected light distinguishes the face from its rounded sidewall.
    QRadialGradient light(QPointF(70,30),390);light.setColorAt(0,QColor(237,244,255,16));light.setColorAt(1,Qt::transparent);
    p->fillPath(face,light);p->restore();
    if(tape){
        p->setPen(QPen(QColor(0,0,0,200),2));p->setBrush(QColor("#15171a"));p->drawRoundedRect(QRectF(36,102,368,246),15,15);
        for(int i=0;i<4;++i){p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor(0,0,0,45-i*8),1.5));p->drawRoundedRect(QRectF(33-i,99-i,374+2*i,252+2*i),17+i,17+i);}
        p->setPen(QPen(QColor(226,238,242,32),.7));p->drawRoundedRect(QRectF(34,100,372,250),17,17);
        screw(QPointF(29,96));screw(QPointF(411,96));screw(QPointF(29,354));screw(QPointF(411,354));
        for(int x=0;x<11;++x){p->setPen(QPen(QColor(0,0,0,85),1));p->drawLine(QPointF(176+x*9,358),QPointF(176+x*9,361));}
    }else{
        QRadialGradient recess(QPointF(220,220),197);recess.setColorAt(0,QColor("#16181b"));recess.setColorAt(.95,QColor("#16181b"));recess.setColorAt(.98,QColor("#090b0d"));recess.setColorAt(1,QColor(0,0,0,0));
        p->setPen(Qt::NoPen);p->setBrush(recess);p->drawEllipse(QPointF(220,220),198,198);
        p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor("#656a6b"),2));p->drawEllipse(QPointF(220,220),187,187);
        p->setPen(QPen(QColor("#181b1d"),5));p->drawEllipse(QPointF(220,220),183,183);
        if(!cd){
            for(int r:{185,188,191})for(int i=0;i<120;++i){p->save();p->translate(220,220);p->rotate(i*3+(r%2)*1.5);p->setPen(Qt::NoPen);p->setBrush(QColor(185,192,193,r==188?100:50));p->drawEllipse(QPointF(0,-r),.7,1.05);p->restore();}
            p->setPen(QPen(QColor(221,231,236,42),.8));p->setBrush(Qt::NoBrush);p->drawEllipse(QPointF(220,220),193,193);
            screw(QPointF(20,20));screw(QPointF(420,20));screw(QPointF(20,420));screw(QPointF(420,420));
            metal(QRectF(37,391,34,8),4);metal(QRectF(365,31,25,11),3);
        }else metal(QRectF(195,410,50,9),4);
    }
}
