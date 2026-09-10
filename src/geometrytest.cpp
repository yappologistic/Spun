#include "mediageometry.h"
#include "deckgeometry.h"
#include <QTest>
#include <QPointer>
#include <cmath>
#include <map>
#include <array>

class GeometryTest : public QObject {
    Q_OBJECT
private slots:
    void turntableDetails() {
        for(bool fascia:{false,true}) {
            TurntableDetailGeometry mesh;mesh.setProperty("fascia",fascia);
            const auto data=mesh.vertexData();QVERIFY(!data.isEmpty());QVERIFY(data.size()<300000);
            const auto *v=reinterpret_cast<const float*>(data.constData());
            for(int i=0;i<data.size()/int(sizeof(float));i+=8) {
                for(int k=0;k<8;++k)QVERIFY(std::isfinite(v[i+k]));
                QVERIFY(std::abs(QVector3D(v[i+3],v[i+4],v[i+5]).length()-1)<.001);
                QVERIFY(v[i+2]>=mesh.boundsMin().z()-.001f&&v[i+2]<=mesh.boundsMax().z()+.001f);
                if(!fascia)for(int row=-1;row<=1;++row)for(int col=-1;col<=1;++col)
                    QVERIFY(std::hypot(v[i]-col*6,v[i+1]-row*6)>1.79);
            }
        }
    }
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
    void narrowRingsRetainValidChamfers() {
        for(float thickness:{.05f,.25f,3.5f}) {
            RecordGeometry ring;ring.setProperty("radius",3.2f);ring.setProperty("hole",3.f);ring.setProperty("depth",thickness);
            const auto bytes=ring.vertexData();const auto *v=reinterpret_cast<const float*>(bytes.constData());
            bool hasChamfer=false;
            for(int i=0;i<bytes.size()/int(sizeof(float));i+=24) {
                const QVector3D a(v[i],v[i+1],v[i+2]),b(v[i+8],v[i+9],v[i+10]),c(v[i+16],v[i+17],v[i+18]);
                QVERIFY(QVector3D::crossProduct(b-a,c-a).lengthSquared()>1e-14f);
                for(int k=0;k<24;k+=8) {
                    const float radius=std::hypot(v[i+k],v[i+k+1]);
                    QVERIFY(radius>=2.999f&&radius<=3.201f);
                    QVERIFY(std::abs(v[i+k+2])<=thickness/2+.001f);
                    const QVector3D normal(v[i+k+3],v[i+k+4],v[i+k+5]);
                    QVERIFY(std::abs(normal.length()-1)<.001f);
                    hasChamfer=hasChamfer||(normal.z()>.1f&&normal.z()<.99f);
                }
            }
            QVERIFY(hasChamfer);
        }
    }
    void reelHasOpenShaftAndThreeThroughWindows() {
        ReelGeometry reel;const auto bytes=reel.vertexData();const auto *v=reinterpret_cast<const float*>(bytes.constData());
        const auto covered=[&](QVector2D p) {
            for(int i=0;i<bytes.size()/int(sizeof(float));i+=24) {
                if(v[i+5]<.9f)continue;
                const QVector2D a(v[i],v[i+1]),b(v[i+8],v[i+9]),c(v[i+16],v[i+17]);
                const auto cross=[](QVector2D a,QVector2D b){return a.x()*b.y()-a.y()*b.x();};
                if(cross(b-a,p-a)>=0&&cross(c-b,p-b)>=0&&cross(a-c,p-c)>=0)return true;
            }
            return false;
        };
        QVERIFY(!covered({0,0}));
        for(int i=0;i<3;++i) {
            const float angle=i*2*float(M_PI)/3;
            QVERIFY(!covered({20*std::cos(angle),20*std::sin(angle)}));
            QVERIFY(covered({20*std::cos(angle+float(M_PI)/3),20*std::sin(angle+float(M_PI)/3)}));
        }
        using Point=std::array<int,3>;using Edge=std::array<Point,2>;
        std::map<Edge,int> edges;
        for(int i=0;i<bytes.size()/int(sizeof(float));i+=24) {
            std::array<Point,3> points;
            for(int k=0;k<3;++k) {
                for(int axis=0;axis<3;++axis) {
                    const float value=v[i+k*8+axis];QVERIFY(std::isfinite(value));
                    QVERIFY(value>=reel.boundsMin()[axis]-.001f&&value<=reel.boundsMax()[axis]+.001f);
                    points[k][axis]=qRound(value*1000);
                }
                QVERIFY(std::abs(QVector3D(v[i+k*8+3],v[i+k*8+4],v[i+k*8+5]).length()-1)<.001f);
            }
            for(int k=0;k<3;++k){Edge edge={points[k],points[(k+1)%3]};if(edge[1]<edge[0])std::swap(edge[0],edge[1]);++edges[edge];}
        }
        for(const auto &[edge,count]:edges){Q_UNUSED(edge);QCOMPARE(count,2);}
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
    void texturedHardwareHasNoCollapsedUvTriangles() {
        DeckGeometry deck;deck.setProperty("dimensions",QVector3D(535,398,42));deck.setProperty("radius",3.f);
        TurntableDetailGeometry headshell,fascia;fascia.setProperty("fascia",true);
        for(auto *geometry:QList<QQuick3DGeometry*>{&deck,&headshell,&fascia}) {
            const auto bytes=geometry->vertexData();const auto *v=reinterpret_cast<const float*>(bytes.constData());
            for(int i=0;i<bytes.size()/int(sizeof(float));i+=24) {
                const double du1=v[i+14]-v[i+6],dv1=v[i+15]-v[i+7],du2=v[i+22]-v[i+6],dv2=v[i+23]-v[i+7];
                QVERIFY2(std::abs(du1*dv2-du2*dv1)>1e-12,"Normal mapped triangles must have a finite tangent basis");
            }
        }
    }
    void cylindricalWallsHaveAxialTextureCoordinates() {
        RecordGeometry ring;ring.setProperty("depth",12.f);ring.setProperty("radius",40.f);
        const auto bytes=ring.vertexData();const auto *v=reinterpret_cast<const float*>(bytes.constData());
        int sides=0;
        for(int i=0;i<bytes.size()/int(sizeof(float));i+=24) {
            if(std::abs(v[i+5])>.1f)continue;
            ++sides;
            const float low=qMin(v[i+7],qMin(v[i+15],v[i+23])),high=qMax(v[i+7],qMax(v[i+15],v[i+23]));
            QVERIFY(high-low>.9f);
            const float uLow=qMin(v[i+6],qMin(v[i+14],v[i+22])),uHigh=qMax(v[i+6],qMax(v[i+14],v[i+22]));
            QVERIFY(uHigh-uLow<.1f);
        }
        QVERIFY(sides>40);
    }
    void materialMapsAreBoundedAndRepeatable() {
        QByteArray previous;
        for(const QString kind:{QString("brushed"),QString("molded"),QString("vinyl"),QString("disc"),QString("turned"),QString("metal-normal"),QString("paper"),QString("knurled-normal"),QString("bead-normal"),QString("vinyl-normal"),QString("lacquer"),QString("powder")}){
            SurfaceTexture first,second;first.setProperty("kind",kind);second.setProperty("kind",kind);
            const int size=(kind=="vinyl-normal"||kind=="lacquer")?1024:kind=="powder"?512:256;
            QCOMPARE(first.size(),QSize(size,size));QCOMPARE(first.textureData().size(),size*size*4);
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
    void studioProbeRetainsHdrAndSeamlessEdges() {
        StudioTexture probe;QCOMPARE(probe.format(),QQuick3DTextureData::RGBE8);
        const auto bytes=probe.textureData();const auto *p=reinterpret_cast<const unsigned char*>(bytes.constData());
        double minimum=10,maximum=0;
        const auto component=[&](int pixel,int channel){return std::ldexp(double(p[pixel*4+channel])/256,int(p[pixel*4+3])-128);};
        for(int i=0;i<512*256;++i)for(int channel=0;channel<3;++channel) {
            const double value=component(i,channel);QVERIFY(std::isfinite(value)&&value>=0&&value<4);
            minimum=qMin(minimum,value);maximum=qMax(maximum,value);
        }
        QVERIFY(maximum>2);QVERIFY(minimum<.1);
        for(int y=0;y<256;++y)for(int channel=0;channel<3;++channel)
            QVERIFY(std::abs(component(y*512,channel)-component(y*512+511,channel))<.015);
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
