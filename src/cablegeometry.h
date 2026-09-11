#pragma once
#include <QQuick3DGeometry>
#include <QVector3D>
class CableGeometry : public QQuick3DGeometry {
    Q_OBJECT
    Q_PROPERTY(QVector3D start MEMBER m_start NOTIFY shapeChanged)
    Q_PROPERTY(QVector3D end MEMBER m_end NOTIFY shapeChanged)
public:
    explicit CableGeometry(QQuick3DObject *parent=nullptr);
signals:
    void shapeChanged();
private:
    void rebuild();
    QVector3D m_start{-79,176,-8},m_end{216,176,-8};
};
