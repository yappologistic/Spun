#pragma once
#include <QQuick3DGeometry>
#include <QQuick3DTextureData>
#include <QImage>
#include <QQmlParserStatus>

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
protected:
    void classBegin() override { QQuick3DGeometry::classBegin(); m_initializing=true; }
    void componentComplete() override { m_initializing=false; rebuild(); QQuick3DGeometry::componentComplete(); }
private:
    bool m_initializing=false;
    void rebuild();
    float m_radius=176,m_hole=3,m_depth=2;
    bool m_grooves=false;
};
class ReelGeometry : public QQuick3DGeometry {
    Q_OBJECT
public:
    explicit ReelGeometry(QQuick3DObject *parent=nullptr);
};

class CassetteGearGeometry : public QQuick3DGeometry {
    Q_OBJECT
public:
    explicit CassetteGearGeometry(QQuick3DObject *parent=nullptr);
};

class CassettePlateGeometry : public QQuick3DGeometry {
    Q_OBJECT
public:
    explicit CassettePlateGeometry(QQuick3DObject *parent=nullptr);
};

class TurntableDetailGeometry : public QQuick3DGeometry {
    Q_OBJECT
    Q_PROPERTY(bool fascia MEMBER m_fascia NOTIFY shapeChanged)
public:
    explicit TurntableDetailGeometry(QQuick3DObject *parent=nullptr);
signals:
    void shapeChanged();
protected:
    void classBegin() override { QQuick3DGeometry::classBegin(); m_initializing=true; }
    void componentComplete() override { m_initializing=false; rebuild(); QQuick3DGeometry::componentComplete(); }
private:
    bool m_initializing=false;
    void rebuild();
    bool m_fascia=false;
};

class CoverTexture : public QObject, public QQmlParserStatus {
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QQuick3DTextureData *texture READ texture NOTIFY textureChanged)
    Q_PROPERTY(QImage artwork MEMBER m_artwork NOTIFY artworkChanged)
    Q_PROPERTY(bool paper MEMBER m_paper NOTIFY artworkChanged)
    Q_PROPERTY(QString title MEMBER m_title NOTIFY artworkChanged)
    Q_PROPERTY(QString artist MEMBER m_artist NOTIFY artworkChanged)
public:
    explicit CoverTexture(QObject *parent=nullptr);
    QQuick3DTextureData *texture() { if(!m_texture && !m_initializing)rebuild(); return m_texture; }
    QSize size() { const auto *t=texture(); return t?t->size():QSize(); }
    QByteArray textureData() { const auto *t=texture(); return t?t->textureData():QByteArray(); }
    void classBegin() override { m_initializing=true; }
    void componentComplete() override { m_initializing=false; rebuild(); }
signals:
    void artworkChanged();
    void textureChanged();
private:
    void rebuild();
    bool m_initializing=false;
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
protected:
    void classBegin() override { QQuick3DGeometry::classBegin(); m_initializing=true; }
    void componentComplete() override { m_initializing=false; rebuild(); QQuick3DGeometry::componentComplete(); }
private:
    bool m_initializing=false;
    void rebuild();
    float m_progress=0,m_phase=0,m_amplitude=0;
    bool m_linear=false;
};

class StudioTexture : public QQuick3DTextureData {
    Q_OBJECT
    Q_PROPERTY(QColor tint MEMBER m_tint NOTIFY tintChanged)
    Q_PROPERTY(float ambientLift MEMBER m_ambientLift NOTIFY tintChanged)
public:
    explicit StudioTexture(QQuick3DObject *parent=nullptr);
signals:
    void tintChanged();
protected:
    void classBegin() override { QQuick3DTextureData::classBegin(); m_initializing=true; }
    void componentComplete() override { m_initializing=false; rebuild(); QQuick3DTextureData::componentComplete(); }
private:
    bool m_initializing=false;
    void rebuild();
    QColor m_tint=Qt::white;
    float m_ambientLift=0;
};

class SurfaceTexture : public QQuick3DTextureData {
    Q_OBJECT
    Q_PROPERTY(QString kind MEMBER m_kind NOTIFY kindChanged)
public:
    explicit SurfaceTexture(QQuick3DObject *parent=nullptr);
signals:
    void kindChanged();
protected:
    void classBegin() override { QQuick3DTextureData::classBegin(); m_initializing=true; }
    void componentComplete() override { m_initializing=false; rebuild(); QQuick3DTextureData::componentComplete(); }
private:
    bool m_initializing=false;
    void rebuild();
    QString m_kind="brushed";
};
