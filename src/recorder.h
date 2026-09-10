#pragma once
#include <QQuickPaintedItem>
#include <QImage>

// Static machined surfaces; transport motion stays on the scene graph transform.
class RecorderSurface : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(int part MEMBER m_part NOTIFY changed)
    Q_PROPERTY(QImage artwork MEMBER m_artwork NOTIFY changed)
public:
    explicit RecorderSurface(QQuickItem *parent=nullptr):QQuickPaintedItem(parent) {
        setAntialiasing(true); connect(this,&RecorderSurface::changed,this,[this]{update();});
    }
    void paint(QPainter *painter) override;
signals:
    void changed();
private:
    int m_part=0;
    QImage m_artwork;
};
