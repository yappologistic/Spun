#pragma once
#include <QQuick3DGeometry>
#include <QQuick3DTextureData>
#include <QImage>

class RecordGeometry : public QQuick3DGeometry {
    Q_OBJECT
    Q_PROPERTY(float radius MEMBER m_radius NOTIFY shapeChanged)
    Q_PROPERTY(float hole MEMBER m_hole NOTIFY shapeChanged)
    Q_PROPERTY(float depth MEMBER m_depth NOTIFY shapeChanged)
    Q_PROPERTY(bool grooves MEMBER m_grooves NOTIFY shapeChanged)
public:
    explicit RecordGeometry(QQuick3DObject *parent=nullptr);
signals:
    void shapeChanged();
private:
    void rebuild();
    float m_radius=176,m_hole=3,m_depth=2;
    bool m_grooves=false;
};
class CoverTexture : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQuick3DTextureData *texture READ texture NOTIFY textureChanged)
    Q_PROPERTY(QImage artwork MEMBER m_artwork NOTIFY artworkChanged)
    Q_PROPERTY(bool paper MEMBER m_paper NOTIFY artworkChanged)
    Q_PROPERTY(QString title MEMBER m_title NOTIFY artworkChanged)
    Q_PROPERTY(QString artist MEMBER m_artist NOTIFY artworkChanged)
public:
    explicit CoverTexture(QObject *parent=nullptr);
    QQuick3DTextureData *texture() const { return m_texture; }
    QSize size() const { return m_texture->size(); }
    QByteArray textureData() const { return m_texture->textureData(); }
signals:
    void artworkChanged();
    void textureChanged();
private:
    void rebuild();
    QQuick3DTextureData *m_texture=nullptr;
    QImage m_artwork;
    bool m_paper=false;
    QString m_title,m_artist;
};
class WaveGeometry : public QQuick3DGeometry {
    Q_OBJECT
    Q_PROPERTY(float progress MEMBER m_progress NOTIFY shapeChanged)
    Q_PROPERTY(float phase MEMBER m_phase NOTIFY shapeChanged)
    Q_PROPERTY(float amplitude MEMBER m_amplitude NOTIFY shapeChanged)
    Q_PROPERTY(bool linear MEMBER m_linear NOTIFY shapeChanged)
public:
    explicit WaveGeometry(QQuick3DObject *parent=nullptr);
signals:
    void shapeChanged();
private:
    void rebuild();
    float m_progress=0,m_phase=0,m_amplitude=0;
    bool m_linear=false;
};

class StudioTexture : public QQuick3DTextureData {
    Q_OBJECT
    Q_PROPERTY(QColor tint MEMBER m_tint NOTIFY tintChanged)
public:
    explicit StudioTexture(QQuick3DObject *parent=nullptr);
signals:
    void tintChanged();
private:
    void rebuild();
    QColor m_tint=Qt::white;
};

class SurfaceTexture : public QQuick3DTextureData {
    Q_OBJECT
    Q_PROPERTY(QString kind MEMBER m_kind NOTIFY kindChanged)
public:
    explicit SurfaceTexture(QQuick3DObject *parent=nullptr);
signals:
    void kindChanged();
private:
    void rebuild();
    QString m_kind="brushed";
};
