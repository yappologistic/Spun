#pragma once
#include <QQuick3DGeometry>
#include <QVector3D>

class DeckGeometry : public QQuick3DGeometry {
    Q_OBJECT
    Q_PROPERTY(QVector3D dimensions MEMBER m_dimensions NOTIFY shapeChanged)
    Q_PROPERTY(float radius MEMBER m_radius NOTIFY shapeChanged)
public:
    explicit DeckGeometry(QQuick3DObject *parent=nullptr);
signals:
    void shapeChanged();
protected:
    void classBegin() override { QQuick3DGeometry::classBegin(); m_initializing=true; }
    void componentComplete() override { m_initializing=false; rebuild(); QQuick3DGeometry::componentComplete(); }
private:
    bool m_initializing=false;
    void rebuild();
    QVector3D m_dimensions{428,428,26};
    float m_radius=22;
};
