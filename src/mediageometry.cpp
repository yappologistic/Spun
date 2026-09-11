#include "mediageometry.h"
#include "disc.h"
#include <QVector3D>
#include <QPainter>
#include <QGuiApplication>
#include <QFontMetrics>
#include <cmath>

namespace {
struct Vertex { float x,y,z,nx,ny,nz,u,v; };
void triangle(QByteArray &data,QVector3D a,QVector3D b,QVector3D c,float radius,bool smoothRadial=false,float wallDepth=0) {
    const auto n=QVector3D::crossProduct(b-a,c-a).normalized();
    const bool cylindrical=wallDepth>0&&std::abs(n.z())<.95f;
    const auto longitude=[](QVector3D p){return .5f+std::atan2(p.y(),p.x())/(2*float(M_PI));};
    const float minimum=cylindrical?qMin(longitude(a),qMin(longitude(b),longitude(c))):0;
    const float maximum=cylindrical?qMax(longitude(a),qMax(longitude(b),longitude(c))):0;
    for(const auto &p:{a,b,c}){
        auto normal=n;
        if(smoothRadial && std::hypot(p.x(),p.y())>.001f) {
            const float radial=std::hypot(n.x(),n.y());
            const float direction=QVector3D::dotProduct(n,QVector3D(p.x(),p.y(),0))<0?-1:1;
            const auto xy=QVector3D(p.x(),p.y(),0).normalized()*radial*direction;
            normal=QVector3D(xy.x(),xy.y(),n.z()).normalized();
        }
        float u=.5f+p.x()/(radius*2),v=.5f-p.y()/(radius*2);
        if(!smoothRadial&&std::abs(n.z())<qMax(std::abs(n.x()),std::abs(n.y()))) {
            u=.5f+(std::abs(n.x())>std::abs(n.y())?p.y():p.x())/(radius*2);
            v=.5f-p.z()/(radius*2);
        }
        if(cylindrical) {
            u=longitude(p);
            if(maximum-minimum>.5f&&u<.5f)u+=1;
            v=.5f+p.z()/wallDepth;
        }
        const Vertex vertex{p.x(),p.y(),p.z(),normal.x(),normal.y(),normal.z(),u,v};data.append(reinterpret_cast<const char*>(&vertex),sizeof(vertex));}
}
void upload(QQuick3DGeometry *geometry,const QByteArray &data,QVector3D low,QVector3D high) {
    geometry->clear();geometry->setStride(sizeof(Vertex));geometry->setVertexData(data);geometry->setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
    geometry->addAttribute(QQuick3DGeometry::Attribute::PositionSemantic,0,QQuick3DGeometry::Attribute::F32Type);
    geometry->addAttribute(QQuick3DGeometry::Attribute::NormalSemantic,3*sizeof(float),QQuick3DGeometry::Attribute::F32Type);
    geometry->addAttribute(QQuick3DGeometry::Attribute::TexCoord0Semantic,6*sizeof(float),QQuick3DGeometry::Attribute::F32Type);
    geometry->setBounds(low,high);geometry->update();
}
}
CassettePlateGeometry::CassettePlateGeometry(QQuick3DObject *parent):QQuick3DGeometry(parent) {
    // A stamped carrier with open service slots, not dark rectangles painted on
    // a solid face. Only exposed edges have walls, so adjacent cells cannot flicker.
    constexpr float xs[]={-128,-109,-76,-55,-29,-10,35,60,101,128};
    constexpr float ys[]={-55,-31,-7,15,36,55};
    const auto filled=[](int x,int y) {
        if(x<0||x>=9||y<0||y>=5)return false;
        return !((x==1&&y>=2&&y<=3)||(x==3&&y==1)||(x==6&&y>=1&&y<=3)||(x>=6&&y==0));
    };
    QByteArray data;
    const auto quad=[&](QVector3D a,QVector3D b,QVector3D c,QVector3D d){triangle(data,a,b,c,130);triangle(data,a,c,d,130);};
    for(int y=0;y<5;++y)for(int x=0;x<9;++x)if(filled(x,y)) {
        const float l=xs[x],r=xs[x+1],b=ys[y],t=ys[y+1],z=1.5f;
        quad({l,b,z},{r,b,z},{r,t,z},{l,t,z});
        quad({l,t,-z},{r,t,-z},{r,b,-z},{l,b,-z});
        if(!filled(x-1,y))quad({l,b,-z},{l,b,z},{l,t,z},{l,t,-z});
        if(!filled(x+1,y))quad({r,t,-z},{r,t,z},{r,b,z},{r,b,-z});
        if(!filled(x,y-1))quad({r,b,-z},{r,b,z},{l,b,z},{l,b,-z});
        if(!filled(x,y+1))quad({l,t,-z},{l,t,z},{r,t,z},{r,t,-z});
    }
    upload(this,data,{-128,-55,-1.5f},{128,55,1.5f});
}

CassetteGearGeometry::CassetteGearGeometry(QQuick3DObject *parent):QQuick3DGeometry(parent) {
    QByteArray vertices;
    constexpr int count=24*4;
    const auto point=[](int i,float radius,float z){const float a=i*2*float(M_PI)/count;return QVector3D(radius*std::cos(a),radius*std::sin(a),z);};
    const auto quad=[&](QVector3D a,QVector3D b,QVector3D c,QVector3D d){triangle(vertices,a,b,c,20);triangle(vertices,a,c,d,20);};
    for(int i=0;i<count;++i) {
        const float r=i%4==0||i%4==3?17.5f:20.f,r2=(i+1)%4==0||(i+1)%4==3?17.5f:20.f;
        quad(point(i,3,2),point(i,r,2),point(i+1,r2,2),point(i+1,3,2));
        quad(point(i+1,3,-2),point(i+1,r2,-2),point(i,r,-2),point(i,3,-2));
        quad(point(i,r,-2),point(i+1,r2,-2),point(i+1,r2,2),point(i,r,2));
        quad(point(i,3,2),point(i+1,3,2),point(i+1,3,-2),point(i,3,-2));
    }
    upload(this,vertices,{-20,-20,-2},{20,20,2});
}

RecordGeometry::RecordGeometry(QQuick3DObject *parent):QQuick3DGeometry(parent){connect(this,&RecordGeometry::shapeChanged,this,&RecordGeometry::rebuild);rebuild();}
void RecordGeometry::rebuild() {
    if(m_initializing)return;
    const float radius=qBound(1.f,m_radius,300.f),hole=qBound(.1f,m_hole,radius-.1f),depth=qBound(.05f,m_depth,60.f);
    QByteArray data;const int radial=m_grooves?80:1,segments=qBound(24,int(std::ceil(radius*1.1f)),128);
    constexpr float grooveHeight=.012f;
    data.reserve((radial+5)*segments*6*sizeof(Vertex));
    const float bevel=qMin(.35f,qMin(depth*.2f,(radius-hole)*.2f));
    const float inner=hole+bevel,outer=radius-bevel;
    const auto point=[segments](float r,int i,float z){const float a=i*2*float(M_PI)/segments;return QVector3D(r*std::cos(a),r*std::sin(a),z);};
    const auto quad=[&](QVector3D a,QVector3D b,QVector3D c,QVector3D d){triangle(data,a,b,c,radius,true,depth);triangle(data,a,c,d,radius,true,depth);};
    for(int i=0;i<segments;++i){
        for(int j=0;j<radial;++j){const float a=inner+(outer-inner)*j/radial,b=inner+(outer-inner)*(j+1)/radial;const float za=depth/2+(m_grooves&&j%2?grooveHeight:0),zb=depth/2+(m_grooves&&(j+1)%2?grooveHeight:0);quad(point(a,i,za),point(b,i,zb),point(b,i+1,zb),point(a,i+1,za));}
        quad(point(hole,i,-depth/2),point(hole,i+1,-depth/2),point(radius,i+1,-depth/2),point(radius,i,-depth/2));
        quad(point(radius,i,-depth/2),point(radius,i+1,-depth/2),point(radius,i+1,depth/2-bevel),point(radius,i,depth/2-bevel));
        quad(point(radius,i,depth/2-bevel),point(radius,i+1,depth/2-bevel),point(outer,i+1,depth/2),point(outer,i,depth/2));
        quad(point(inner,i,depth/2),point(inner,i+1,depth/2),point(hole,i+1,depth/2-bevel),point(hole,i,depth/2-bevel));
        quad(point(hole,i,depth/2-bevel),point(hole,i+1,depth/2-bevel),point(hole,i+1,-depth/2),point(hole,i,-depth/2));
    }
    upload(this,data,{-radius,-radius,-depth/2},{radius,radius,depth/2+grooveHeight});
}
TurntableDetailGeometry::TurntableDetailGeometry(QQuick3DObject *parent):QQuick3DGeometry(parent) {
    connect(this,&TurntableDetailGeometry::shapeChanged,this,&TurntableDetailGeometry::rebuild);rebuild();
}
void TurntableDetailGeometry::rebuild() {
    if(m_initializing)return;
    QByteArray data;
    const auto quad=[&](QVector3D a,QVector3D b,QVector3D c,QVector3D d) {triangle(data,a,b,c,300);triangle(data,a,c,d,300);};
    if(m_fascia) {
        // One mesh for the machined diagonal ribs, with closed side walls.
        for(int i=0;i<34;++i) {
            const float x=i*7.f;
            const QVector3D a(x,-17,0),b(x+2.6f,-17,0),c(x+25.6f,17,0),d(x+23,17,0),z(0,0,1.2f);
            quad(a+z,b+z,c+z,d+z);quad(d,c,b,a);
            quad(a,b,b+z,a+z);quad(b,c,c+z,b+z);quad(c,d,d+z,c+z);quad(d,a,a+z,d+z);
        }
        upload(this,data,{0,-17,0},{257,17,1.2f});
    } else {
        // Nine bored holes continue through the aluminum headshell web.
        constexpr int segments=32;
        for(int row=0;row<3;++row)for(int col=0;col<3;++col) {
            const QVector3D center((col-1)*6.f,(row-1)*6.f,0);
            const auto point=[&](int i,bool outer,float z) {
                const float a=i*2*float(M_PI)/segments,c=std::cos(a),s=std::sin(a);
                const float r=outer?3.f/qMax(std::abs(c),std::abs(s)):1.8f;
                return center+QVector3D(r*c,r*s,z);
            };
            for(int i=0;i<segments;++i) {
                quad(point(i,false,1),point(i,true,1),point(i+1,true,1),point(i+1,false,1));
                quad(point(i,false,-1),point(i+1,false,-1),point(i+1,true,-1),point(i,true,-1));
                quad(point(i,false,1),point(i+1,false,1),point(i+1,false,-1),point(i,false,-1));
                if(col==0||col==2||row==0||row==2) {
                    const auto a=point(i,true,0),b=point(i+1,true,0);
                    if((std::abs(a.x())>8.99f&&std::abs(b.x())>8.99f)||(std::abs(a.y())>8.99f&&std::abs(b.y())>8.99f))
                        quad(point(i,true,-1),point(i+1,true,-1),point(i+1,true,1),point(i,true,1));
                }
            }
        }
        upload(this,data,{-9,-9,-1},{9,9,1});
    }
}
ReelGeometry::ReelGeometry(QQuick3DObject *parent):QQuick3DGeometry(parent) {
    constexpr int segments=120;
    constexpr float inner=16.5f,outer=24,edge=31.7f,z=3;
    QByteArray data;
    const auto point=[](float radius,int i,float height) {
        const float angle=i*2*float(M_PI)/segments;
        return QVector3D(radius*std::cos(angle),radius*std::sin(angle),height);
    };
    const auto shaft=[](int i) {
        const float angle=i*2*float(M_PI)/segments;
        return 13.f-4.f*std::pow(qMax(0.f,std::cos(angle*6)),8.f);
    };
    const auto cutout=[](int i){return ((i+segments+6)%40)<12;};
    const auto quad=[&](QVector3D a,QVector3D b,QVector3D c,QVector3D d) {
        triangle(data,a,b,c,32);triangle(data,a,c,d,32);
    };
    // The openings continue through the web and have their own inner walls.
    // Both rotating reels share this mesh, including the six shaft teeth.
    for(int i=0;i<segments;++i) {
        const int j=i+1;
        const float starts[]={shaft(i),inner,outer},ends[]={shaft(j),inner,outer};
        const float outside[]={inner,outer,edge};
        for(int band=0;band<3;++band) {
            if(band==1&&cutout(i))continue;
            quad(point(starts[band],i,z),point(outside[band],i,z),point(outside[band],j,z),point(ends[band],j,z));
            quad(point(starts[band],i,-z),point(ends[band],j,-z),point(outside[band],j,-z),point(outside[band],i,-z));
        }
        quad(point(shaft(i),i,z),point(shaft(j),j,z),point(shaft(j),j,-z),point(shaft(i),i,-z));
        quad(point(edge,i,-z),point(edge,j,-z),point(32,j,-2.6),point(32,i,-2.6));
        quad(point(32,i,-2.6),point(32,j,-2.6),point(32,j,2.6),point(32,i,2.6));
        quad(point(32,i,2.6),point(32,j,2.6),point(edge,j,z),point(edge,i,z));
        if(!cutout(i))continue;
        quad(point(inner,i,-z),point(inner,j,-z),point(inner,j,z),point(inner,i,z));
        quad(point(outer,i,z),point(outer,j,z),point(outer,j,-z),point(outer,i,-z));
        if(!cutout(i-1))quad(point(inner,i,z),point(outer,i,z),point(outer,i,-z),point(inner,i,-z));
        if(!cutout(j))quad(point(inner,j,-z),point(outer,j,-z),point(outer,j,z),point(inner,j,z));
    }
    upload(this,data,{-32,-32,-z},{32,32,z});
}

CoverTexture::CoverTexture(QObject *parent):QObject(parent){connect(this,&CoverTexture::artworkChanged,this,&CoverTexture::rebuild);}
void CoverTexture::rebuild(){
    if(m_initializing)return;
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
    if(m_initializing)return;
    QByteArray data;const float progress=qBound(0.f,m_progress,1.f);const int count=qMax(1,int(256*progress));
    data.reserve(count*6*sizeof(Vertex));
    const auto p=[&](float f,float side){const float wave=std::sin(f*(m_linear?45.f:150.f)+m_phase)*m_amplitude;
        if(m_linear)return QVector3D(-123+246*f,-125+wave+side,5);
        const float a=f*2*float(M_PI),r=186+wave+side;return QVector3D(r*std::sin(a),r*std::cos(a),5);
    };
    auto al=p(0,-1.3f),ar=p(0,1.3f);
    for(int i=0;i<count;++i){
        const float b=progress*(i+1)/count;
        const auto bl=p(b,-1.3f),br=p(b,1.3f);
        triangle(data,al,bl,br,200);triangle(data,al,br,ar,200);
        al=bl;ar=br;
    }
    upload(this,data,{-200,-200,4},{200,200,6});
}

StudioTexture::StudioTexture(QQuick3DObject *parent):QQuick3DTextureData(parent) {
    connect(this,&StudioTexture::tintChanged,this,&StudioTexture::rebuild);rebuild();
}
void StudioTexture::rebuild() {
    if(m_initializing)return;
    // RGBE preserves bright reflection sources in four bytes per pixel. The
    // probe is rebuilt only when the wallpaper accent changes.
    constexpr int width=512,height=256;
    QByteArray pixels(width*height*4,Qt::Uninitialized);
    auto *out=reinterpret_cast<unsigned char*>(pixels.data());
    const QColor tint=m_tint.isValid()?m_tint:QColor(Qt::white);
    const QColor bounce=QColor::fromHsvF(qMax(0.,tint.hsvHueF()),qMin(.55,tint.hsvSaturationF()*1.5),1);
    for(int y=0;y<height;++y) {
      const double v=(y+.5)/height;
      const double ambient=.045+qBound(0.f,m_ambientLift,.5f)+.2*std::pow(1-v,2);
      const double ceiling=.85*std::exp(-2*std::pow(v/.13,4));
      for(int x=0;x<width;++x) {
        const double u=(x+.5)/width;
        const auto strip=[&](double center,double elevation,double breadth,double tallness) {
            const double dx=qMin(std::abs(u-center),1-std::abs(u-center))/breadth;
            const double dy=(v-elevation)/tallness;
            return std::exp(-2*(dx*dx*dx*dx+dy*dy*dy*dy));
        };
        // Broad neutral softboxes retain silver highlights, with a narrow edge
        // reflection and wallpaper-colored bounce. No extra scene lights/passes.
        const double key=2.65*strip(.18,.30,.11,.22);
        const double edge=1.55*strip(.46,.37,.026,.21);
        const double fill=1.05*strip(.73,.39,.17,.15);
        const double base=ambient+key+edge+ceiling;
        const double r=base+fill*(.18+.82*bounce.redF())+ambient*.35*bounce.redF();
        const double g=base+fill*(.18+.82*bounce.greenF())+ambient*.35*bounce.greenF();
        const double b=base+fill*(.18+.82*bounce.blueF())+ambient*.35*bounce.blueF();
        int exponent=0;const double largest=qMax(r,qMax(g,b));
        const double scale=std::frexp(largest,&exponent)*256/largest;
        const int offset=(y*width+x)*4;
        out[offset]=qBound(0,int(r*scale),255);out[offset+1]=qBound(0,int(g*scale),255);
        out[offset+2]=qBound(0,int(b*scale),255);out[offset+3]=exponent+128;
      }
    }
    setSize(QSize(width,height));setFormat(RGBE8);setHasTransparency(false);setTextureData(pixels);
}

SurfaceTexture::SurfaceTexture(QQuick3DObject *parent):QQuick3DTextureData(parent) {
    connect(this,&SurfaceTexture::kindChanged,this,&SurfaceTexture::rebuild);rebuild();
}
void SurfaceTexture::rebuild() {
    if(m_initializing)return;
    // Small deterministic material maps, generated once per material rather
    // than every frame. Geometry supplies the silhouette and physical grooves.
    const int size=(m_kind=="vinyl-normal"||m_kind=="lacquer")?1024:m_kind=="powder"?512:256;
    QByteArray pixels(size*size*4,Qt::Uninitialized);
    auto *out=reinterpret_cast<unsigned char*>(pixels.data());
    // Resolve the material once; per-pixel QString comparisons otherwise
    // dominate these small maps. Bytes stay in the same RGBA8 layout.
    static const QStringList kinds={"vinyl-normal","lacquer","powder","disc","knurled-normal","leather-normal","turned-normal","bead-normal","metal-normal","brushed","turned","satin","vinyl","paper"};
    const int kind=kinds.indexOf(m_kind);
    const auto noise=[](unsigned x,unsigned y){unsigned n=x*374761393u+y*668265263u;n=(n^(n>>13))*1274126177u;return int((n^(n>>16))&255);};
    // Periodic, smoothly interpolated grain avoids blocky patches and seams.
    const auto grain=[&](double x,double y,int cells) {
        x=x/size*cells;y=y/size*cells;
        const int ix=int(std::floor(x)),iy=int(std::floor(y));
        const auto wrap=[&](int n){return (n%cells+cells)%cells;};
        const auto sample=[&](int a,int b){return noise(wrap(a),wrap(b))/255.;};
        double u=x-ix,v=y-iy;u=u*u*(3-2*u);v=v*v*(3-2*v);
        return (1-v)*((1-u)*sample(ix,iy)+u*sample(ix+1,iy))+v*((1-u)*sample(ix,iy+1)+u*sample(ix+1,iy+1));
    };
    const auto encodedNormal=[](double x,double y) {
        const auto n=QVector3D(x,y,1).normalized();
        return QColor(qRound((n.x()+1)*127.5),qRound((n.y()+1)*127.5),qRound((n.z()+1)*127.5));
    };
    for(int y=0;y<size;++y)for(int x=0;x<size;++x) {
        QColor color;
        if(kind==0) {
            const double dx=x-(size-1)*.5,dy=y-(size-1)*.5,r=std::hypot(dx,dy);
            const double slope=.16*std::sin(r*2*M_PI*180/(size*.5));
            const auto n=QVector3D(r>0?slope*dx/r:0,r>0?slope*dy/r:0,1).normalized();
            color=QColor(qRound((n.x()+1)*127.5),qRound((n.y()+1)*127.5),qRound((n.z()+1)*127.5));
        } else if(kind==1) {
            const double r=std::hypot(x-(size-1)*.5,y-(size-1)*.5)/(size*.5);
            const int value=211+qRound(7*std::sin(r*71)+3*std::sin(r*311)+3*grain(x,y,48));
            color=QColor(value,value,value);
        } else if(kind==2) {
            const int value=218+qRound(12*grain(x,y,96)+5*grain(x,y,24));
            color=QColor(value,value,value);
        } else if(kind==3) {
            const double a=std::atan2(y-127.5,x-127.5);
            const double hue=(a+M_PI)/(2*M_PI);
            color=QColor::fromHsvF(hue,.18,.88+.06*std::sin(a*3));
        } else if(kind==4) {
            const double a=x*M_PI/4,b=y*M_PI/8;
            color=encodedNormal(.25*std::sin(a)*std::cos(b),.25*std::cos(a)*std::sin(b));
        } else if(kind==5) {
            const auto height=[&](double a,double b) {
                const double cell=grain(a,b,32);
                return .7*std::pow(std::abs(cell-.5)*2,.65)+.12*grain(a,b,96);
            };
            color=encodedNormal((height(x-1,y)-height(x+1,y))*.7,(height(x,y-1)-height(x,y+1))*.7);
        } else if(kind==6) {
            const double dx=x-(size-1)*.5,dy=y-(size-1)*.5,r=std::hypot(dx,dy);
            const double slope=.075*std::sin(r*2*M_PI/3.2);
            color=encodedNormal(r>0?slope*dx/r:0,r>0?slope*dy/r:0);
        } else if(kind==7) {
            const float sx=(noise((x+1)%256,y)-noise((x+255)%256,y))/2048.f;
            const float sy=(noise(x,(y+1)%256)-noise(x,(y+255)%256))/2048.f;
            const auto n=QVector3D(sx,sy,1).normalized();
            color=QColor(qRound((n.x()+1)*127.5),qRound((n.y()+1)*127.5),qRound((n.z()+1)*127.5));
        } else if(kind==8) {
            const float slope=(noise(0,(y+1)%256)-noise(0,(y+255)%256))/1024.f;
            const auto n=QVector3D(0,slope,1).normalized();
            color=QColor(qRound((n.x()+1)*127.5),qRound((n.y()+1)*127.5),qRound((n.z()+1)*127.5));
        } else {
            int value=0;
            if(kind==9)value=219+noise(0,y)/16+qRound(6*grain(x,y,16));
            else if(kind==10) {const double r=std::hypot(x-127.5,y-127.5);value=220+int(6*std::sin(r*2.8))+noise(x,y)/64;}
            else if(kind==11)value=223+qRound(12*grain(x,y,40)+4*grain(x,y,12));
            else if(kind==12) {const double r=std::hypot(x-127.5,y-127.5);value=210+int(4*std::sin(r*5))+noise(x,y)/64;}
            else if(kind==13)value=228+noise(x,y)/16+noise(x/3,y/2)/32;
            else value=222+noise(x,y)/32;
            color=QColor(value,value,value);
        }
        const int offset=(y*size+x)*4;
        out[offset]=color.red();out[offset+1]=color.green();out[offset+2]=color.blue();out[offset+3]=color.alpha();
    }
    setSize(QSize(size,size));setFormat(RGBA8);setHasTransparency(false);setTextureData(pixels);
}
