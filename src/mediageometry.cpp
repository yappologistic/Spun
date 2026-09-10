#include "mediageometry.h"
#include "disc.h"
#include <QVector3D>
#include <QPainter>
#include <QGuiApplication>
#include <QFontMetrics>
#include <cmath>

namespace {
struct Vertex { float x,y,z,nx,ny,nz,u,v; };
void triangle(QByteArray &data,QVector3D a,QVector3D b,QVector3D c,float radius,bool smoothRadial=false) {
    const auto n=QVector3D::crossProduct(b-a,c-a).normalized();
    for(const auto &p:{a,b,c}){
        auto normal=n;
        if(smoothRadial && std::hypot(p.x(),p.y())>.001f) {
            const float radial=std::hypot(n.x(),n.y());
            const float direction=QVector3D::dotProduct(n,QVector3D(p.x(),p.y(),0))<0?-1:1;
            const auto xy=QVector3D(p.x(),p.y(),0).normalized()*radial*direction;
            normal=QVector3D(xy.x(),xy.y(),n.z()).normalized();
        }
        const Vertex v{p.x(),p.y(),p.z(),normal.x(),normal.y(),normal.z(),.5f+p.x()/(radius*2),.5f-p.y()/(radius*2)};data.append(reinterpret_cast<const char*>(&v),sizeof(v));}
}
void upload(QQuick3DGeometry *geometry,const QByteArray &data,QVector3D low,QVector3D high) {
    geometry->clear();geometry->setStride(sizeof(Vertex));geometry->setVertexData(data);geometry->setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
    geometry->addAttribute(QQuick3DGeometry::Attribute::PositionSemantic,0,QQuick3DGeometry::Attribute::F32Type);
    geometry->addAttribute(QQuick3DGeometry::Attribute::NormalSemantic,3*sizeof(float),QQuick3DGeometry::Attribute::F32Type);
    geometry->addAttribute(QQuick3DGeometry::Attribute::TexCoord0Semantic,6*sizeof(float),QQuick3DGeometry::Attribute::F32Type);
    geometry->setBounds(low,high);geometry->update();
}
}
RecordGeometry::RecordGeometry(QQuick3DObject *parent):QQuick3DGeometry(parent){connect(this,&RecordGeometry::shapeChanged,this,&RecordGeometry::rebuild);rebuild();}
void RecordGeometry::rebuild() {
    const float radius=qBound(1.f,m_radius,300.f),hole=qBound(.1f,m_hole,radius-.1f),depth=qBound(.05f,m_depth,60.f);
    QByteArray data;const int radial=m_grooves?80:1,segments=128;
    constexpr float grooveHeight=.012f;
    data.reserve((radial+3)*segments*6*sizeof(Vertex));
    const auto point=[](float r,int i,float z){const float a=i*2*float(M_PI)/128;return QVector3D(r*std::cos(a),r*std::sin(a),z);};
    const auto quad=[&](QVector3D a,QVector3D b,QVector3D c,QVector3D d){triangle(data,a,b,c,radius,true);triangle(data,a,c,d,radius,true);};
    for(int i=0;i<segments;++i){
        for(int j=0;j<radial;++j){const float a=hole+(radius-hole)*j/radial,b=hole+(radius-hole)*(j+1)/radial;const float za=depth/2+(m_grooves&&j%2?grooveHeight:0),zb=depth/2+(m_grooves&&(j+1)%2?grooveHeight:0);quad(point(a,i,za),point(b,i,zb),point(b,i+1,zb),point(a,i+1,za));}
        quad(point(hole,i,-depth/2),point(hole,i+1,-depth/2),point(radius,i+1,-depth/2),point(radius,i,-depth/2));
        quad(point(radius,i,-depth/2),point(radius,i+1,-depth/2),point(radius,i+1,depth/2),point(radius,i,depth/2));
        quad(point(hole,i,depth/2),point(hole,i+1,depth/2),point(hole,i+1,-depth/2),point(hole,i,-depth/2));
    }
    upload(this,data,{-radius,-radius,-depth/2},{radius,radius,depth/2+grooveHeight});
}
CoverTexture::CoverTexture(QObject *parent):QObject(parent){connect(this,&CoverTexture::artworkChanged,this,&CoverTexture::rebuild);rebuild();}
void CoverTexture::rebuild(){
    QImage image=m_artwork.isNull()?Disc::placeholderArt(512):m_artwork;
    const int side=qMin(image.width(),image.height());image=image.copy((image.width()-side)/2,(image.height()-side)/2,side,side).scaled(512,512,Qt::KeepAspectRatio,Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);
    if(m_paper) {
        const QImage art=image;image=QImage(1024,170,QImage::Format_RGBA8888);image.fill(QColor("#dfd6bf"));
        QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);
        for(int y=0;y<170;y+=2){p.setPen(QColor(70,58,40,(y*17)%7));p.drawLine(0,y,1024,y);}
        p.drawImage(QRect(13,13,144,144),art);p.setPen(QColor("#423e35"));
        QFont font=QGuiApplication::font();font.setPixelSize(39);font.setWeight(QFont::Medium);p.setFont(font);
        p.drawText(QRect(184,19,812,56),Qt::AlignVCenter,QFontMetrics(font).elidedText(m_title,Qt::ElideRight,800));
        font.setPixelSize(29);font.setWeight(QFont::Normal);p.setFont(font);p.setPen(QColor("#726a59"));
        p.drawText(QRect(184,84,812,43),Qt::AlignVCenter,QFontMetrics(font).elidedText(m_artist,Qt::ElideRight,800));
        p.setPen(QPen(QColor("#a4977c"),1));p.drawLine(184,148,995,148);p.end();
    }
    const QByteArray pixels(reinterpret_cast<const char*>(image.constBits()),image.sizeInBytes());
    if(m_texture&&m_texture->size()==image.size()&&m_texture->textureData()==pixels)return;
    // Qt 6.11 reuses raw texture storage on update without regenerating its
    // mipmaps. A new snapshot keeps minified labels and full-size covers in sync.
    auto *next=new QQuick3DTextureData;
    next->setParent(this);
    next->setSize(image.size());next->setFormat(QQuick3DTextureData::RGBA8);
    next->setHasTransparency(false);next->setTextureData(pixels);
    auto *previous=m_texture;m_texture=next;
    emit textureChanged();
    if(previous)previous->deleteLater();
}
WaveGeometry::WaveGeometry(QQuick3DObject *parent):QQuick3DGeometry(parent){connect(this,&WaveGeometry::shapeChanged,this,&WaveGeometry::rebuild);rebuild();}
void WaveGeometry::rebuild(){
    QByteArray data;const float progress=qBound(0.f,m_progress,1.f);const int count=qMax(1,int(256*progress));
    const auto p=[&](float f,float side){const float wave=std::sin(f*(m_linear?45.f:150.f)+m_phase)*m_amplitude;
        if(m_linear)return QVector3D(-123+246*f,-125+wave+side,5);
        const float a=f*2*float(M_PI),r=186+wave+side;return QVector3D(r*std::sin(a),r*std::cos(a),5);
    };
    for(int i=0;i<count;++i){const float a=progress*i/count,b=progress*(i+1)/count;triangle(data,p(a,-1.3f),p(b,-1.3f),p(b,1.3f),200);triangle(data,p(a,-1.3f),p(b,1.3f),p(a,1.3f),200);}
    upload(this,data,{-200,-200,4},{200,200,6});
}

StudioTexture::StudioTexture(QQuick3DObject *parent):QQuick3DTextureData(parent) {
    connect(this,&StudioTexture::tintChanged,this,&StudioTexture::rebuild);rebuild();
}
void StudioTexture::rebuild() {
    QImage image(512,256,QImage::Format_RGBA8888);image.fill(QColor("#303239"));
    QPainter p(&image);
    QLinearGradient gradient(0,0,0,256);gradient.setColorAt(0,QColor("#888c92"));gradient.setColorAt(.5,QColor("#454850"));gradient.setColorAt(1,QColor("#181a1e"));p.fillRect(image.rect(),gradient);
    for(const QRect rect:{QRect(60,38,60,125),QRect(325,62,120,65)}) { QRadialGradient light(rect.center(),rect.width());light.setColorAt(0,QColor("#ffffff"));light.setColorAt(1,QColor(200,206,215,0));p.fillRect(rect,light); }
    QColor tint=m_tint.isValid()?m_tint:QColor(Qt::white);
    QColor bounce=QColor::fromHsvF(qMax(0.,tint.hsvHueF()),qMin(.55,tint.hsvSaturationF()*1.5),1,.35);p.fillRect(image.rect(),bounce);
    p.end();setSize(image.size());setFormat(RGBA8);setHasTransparency(false);setTextureData(QByteArray(reinterpret_cast<const char*>(image.constBits()),image.sizeInBytes()));
}

SurfaceTexture::SurfaceTexture(QQuick3DObject *parent):QQuick3DTextureData(parent) {
    connect(this,&SurfaceTexture::kindChanged,this,&SurfaceTexture::rebuild);rebuild();
}
void SurfaceTexture::rebuild() {
    // Small deterministic material maps, generated once per material rather
    // than every frame. Geometry supplies the silhouette and physical grooves.
    QImage image(256,256,QImage::Format_RGBA8888);
    const auto noise=[](unsigned x,unsigned y){unsigned n=x*374761393u+y*668265263u;n=(n^(n>>13))*1274126177u;return int((n^(n>>16))&255);};
    for(int y=0;y<256;++y)for(int x=0;x<256;++x) {
        QColor color;
        if(m_kind=="disc") {
            const double a=std::atan2(y-127.5,x-127.5);
            const double hue=(a+M_PI)/(2*M_PI);
            color=QColor::fromHsvF(hue,.18,.88+.06*std::sin(a*3));
        } else if(m_kind=="metal-normal") {
            const float slope=(noise(0,(y+1)%256)-noise(0,(y+255)%256))/1024.f;
            const auto n=QVector3D(0,slope,1).normalized();
            color=QColor(qRound((n.x()+1)*127.5),qRound((n.y()+1)*127.5),qRound((n.z()+1)*127.5));
        } else {
            int value=0;
            if(m_kind=="brushed")value=205+noise(0,y)/12+noise(x,y)/64;
            else if(m_kind=="turned")value=175+noise(0,y)/5+noise(x,y)/64;
            else if(m_kind=="vinyl") {const double r=std::hypot(x-127.5,y-127.5);value=210+int(4*std::sin(r*5))+noise(x,y)/64;}
            else value=222+noise(x,y)/32;
            color=QColor(value,value,value);
        }
        image.setPixelColor(x,y,color);
    }
    setSize(image.size());setFormat(RGBA8);setHasTransparency(false);
    setTextureData(QByteArray(reinterpret_cast<const char*>(image.constBits()),image.sizeInBytes()));
}
