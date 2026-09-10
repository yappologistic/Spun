#include "mediageometry.h"
#include "deckgeometry.h"
#include <QTest>
#include <QPointer>
#include <cmath>

class GeometryTest : public QObject {
    Q_OBJECT
private slots:
    void recordTopology_data() {
        QTest::addColumn<float>("hole");QTest::addColumn<bool>("grooves");
        QTest::newRow("CD")<<15.f<<false;QTest::newRow("vinyl")<<3.5f<<true;QTest::newRow("tape-pack")<<31.f<<false;
    }
    void recordTopology() {
        QFETCH(float,hole);QFETCH(bool,grooves);
        RecordGeometry record;record.setProperty("hole",hole);record.setProperty("grooves",grooves);
        const auto data=record.vertexData();QVERIFY(!data.isEmpty());QCOMPARE(data.size()%(3*record.stride()),0);
        const float *values=reinterpret_cast<const float*>(data.constData());const int stride=record.stride()/sizeof(float);
        float minZ=100,maxZ=-100;
        for(int i=0;i<data.size()/int(sizeof(float));i+=stride){
            for(int j=0;j<stride;++j)QVERIFY(std::isfinite(values[i+j]));
            const float x=values[i],y=values[i+1],z=values[i+2];
            QVERIFY(std::hypot(x,y)>=hole-.01);QVERIFY(std::hypot(x,y)<=176.01);
            const auto n=QVector3D(values[i+3],values[i+4],values[i+5]);QVERIFY(std::abs(n.length()-1)<.001);
            QVERIFY(values[i+6]>=0&&values[i+6]<=1);QVERIFY(values[i+7]>=0&&values[i+7]<=1);
            minZ=qMin(minZ,z);maxZ=qMax(maxZ,z);
        }
        QVERIFY(minZ<0&&maxZ>0);QVERIFY(record.boundsMin().z()<=minZ);QVERIFY(record.boundsMax().z()>=maxZ);
    }
    void cartridgeProportions() {
        DeckGeometry body;body.setProperty("dimensions",QVector3D(12,21,11));body.setProperty("radius",3.f);
        QCOMPARE(body.boundsMax()-body.boundsMin(),QVector3D(12,21,11));
        QVERIFY(body.vertexData().size()>0);
    }
    void thinMachinedPartsStayInsideBounds() {
        for(const auto size:{QVector3D(4,.7,.3),QVector3D(428,428,28),QVector3D(12,21,11)}){
            DeckGeometry body;body.setProperty("dimensions",size);body.setProperty("radius",2.f);
            const auto bytes=body.vertexData();const auto *v=reinterpret_cast<const float*>(bytes.constData());
            for(int i=0;i<bytes.size()/int(sizeof(float));i+=8){
                for(int axis=0;axis<3;++axis)QVERIFY(std::abs(v[i+axis])<=size[axis]/2+.001f);
                QVERIFY(std::abs(QVector3D(v[i+3],v[i+4],v[i+5]).length()-1)<.001f);
            }
        }
    }
    void materialMapsAreBoundedAndRepeatable() {
        QByteArray previous;
        for(const QString kind:{QString("brushed"),QString("molded"),QString("vinyl"),QString("disc"),QString("turned"),QString("metal-normal")}){
            SurfaceTexture first,second;first.setProperty("kind",kind);second.setProperty("kind",kind);
            QCOMPARE(first.size(),QSize(256,256));QCOMPARE(first.textureData().size(),256*256*4);
            QCOMPARE(first.textureData(),second.textureData());QVERIFY(first.textureData()!=previous);
            previous=first.textureData();
        }
    }
    void coversUseBoundedIndependentPixels() {
        CoverTexture cover;QImage input(1200,600,QImage::Format_RGBA8888);input.fill(Qt::red);cover.setProperty("artwork",input);
        QCOMPARE(cover.size(),QSize(512,512));QCOMPARE(cover.textureData().size(),512*512*4);
        input.fill(Qt::blue);QCOMPARE(static_cast<unsigned char>(cover.textureData()[0]),255);
        cover.setProperty("artwork",input);QCOMPARE(static_cast<unsigned char>(cover.textureData()[2]),255);
        cover.setProperty("artwork",QImage());QCOMPARE(cover.textureData().size(),512*512*4);
    }
    void coverSnapshotsReplaceAndRetireTextures() {
        CoverTexture cover;QImage image(64,64,QImage::Format_RGBA8888);image.fill(Qt::red);
        cover.setProperty("artwork",image);
        QPointer<QQuick3DTextureData> previous=cover.texture();
        const auto pixels=previous->textureData();
        image.fill(Qt::blue);cover.setProperty("artwork",image);
        QVERIFY(cover.texture()!=previous);QCOMPARE(previous->textureData(),pixels);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QVERIFY(previous.isNull());
        const auto *unchanged=cover.texture();
        cover.setProperty("artwork",image.copy());QCOMPARE(cover.texture(),unchanged);
        for(int i=0;i<30;++i){image.fill(i%2?Qt::red:Qt::blue);cover.setProperty("artwork",image);}
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QCOMPARE(cover.findChildren<QQuick3DTextureData*>().size(),1);
        QCOMPARE(cover.textureData().size(),512*512*4);
    }
    void paperLabelsAndWallpaperProbe() {
        CoverTexture label;label.setProperty("paper",true);label.setProperty("title",QString("Test album"));
        QCOMPARE(label.size(),QSize(1024,170));QCOMPARE(label.textureData().size(),1024*170*4);
        StudioTexture probe;const auto neutral=probe.textureData();probe.setProperty("tint",QColor("#80b8e8"));
        QCOMPARE(probe.size(),QSize(512,256));QCOMPARE(probe.textureData().size(),512*256*4);QVERIFY(probe.textureData()!=neutral);
        const auto blue=probe.textureData();probe.setProperty("tint",QColor("#e8ac80"));QVERIFY(probe.textureData()!=blue);
    }
    void waveIsFiniteAtEndpoints() {
        for(bool linear:{false,true})for(float progress:{0.f,.25f,1.f}){
            WaveGeometry wave;wave.setProperty("linear",linear);wave.setProperty("progress",progress);wave.setProperty("amplitude",2.4f);
            const auto bytes=wave.vertexData();const auto *values=reinterpret_cast<const float*>(bytes.constData());for(int i=0;i<bytes.size()/int(sizeof(float));++i)QVERIFY(std::isfinite(values[i]));
        }
    }
};
QTEST_MAIN(GeometryTest)
#include "geometrytest.moc"
