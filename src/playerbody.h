#pragma once
#include <QQuickPaintedItem>
#include <QImage>

// Static physical surfaces remain cached while the scene graph moves the media.
class PlayerBody : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString medium MEMBER m_medium NOTIFY changed)
    Q_PROPERTY(QColor surface MEMBER m_surface NOTIFY changed)
    Q_PROPERTY(QImage artwork MEMBER m_artwork NOTIFY changed)
    Q_PROPERTY(int part MEMBER m_layer NOTIFY changed)
public:
    explicit PlayerBody(QQuickItem *parent=nullptr):QQuickPaintedItem(parent) {
        setAntialiasing(true);connect(this,&PlayerBody::changed,this,[this]{update();});
    }
    void paint(QPainter *painter) override;
signals:
    void changed();
private:
    QString m_medium="vinyl";
    QColor m_surface=QColor("#343438");
    QImage m_artwork;
    int m_layer=0;
};
