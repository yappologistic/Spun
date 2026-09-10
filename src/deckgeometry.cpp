#include "deckgeometry.h"
#include <QVector2D>
#include <cmath>
#include <array>

DeckGeometry::DeckGeometry(QQuick3DObject *parent):QQuick3DGeometry(parent) {
    connect(this,&DeckGeometry::shapeChanged,this,&DeckGeometry::rebuild);rebuild();
}
void DeckGeometry::rebuild() {
    struct Vertex {float x,y,z,nx,ny,nz,u,v;};
    QByteArray vertices;
    const float w=qMax(.01f,m_dimensions.x()/2),h=qMax(.01f,m_dimensions.y()/2),d=qMax(.01f,m_dimensions.z()/2);
    const float r=qBound(.001f,m_radius,qMin(w,h));
    const float bevel=qMin(qMin(3.f,d/2),r*.75f);
    constexpr int perimeter=68;
    const auto outward=[](int i){
        const int corner=(i/17)%4;
        const float angle=(corner*90+(i%17)*90.f/16)*float(M_PI)/180;
        return QVector3D(std::cos(angle),std::sin(angle),0);
    };
    const auto point=[&](int i,float inset,float z){
        const int corner=(i/17)%4;
        const auto n=outward(i);
        return QVector3D((corner==0||corner==3?w-r:-w+r)+(r-inset)*n.x(),
                         (corner<2?h-r:-h+r)+(r-inset)*n.y(),z);
    };
    const auto add=[&](QVector3D a,QVector3D b,QVector3D c,QVector3D na,QVector3D nb,QVector3D nc,QVector2D ta=QVector2D(-1,-1),QVector2D tb={},QVector2D tc={}){
        if(QVector3D::crossProduct(b-a,c-a).lengthSquared()<1e-12f)return;
        const std::array<QVector3D,3> points={a,b,c},normals={na,nb,nc};
        const std::array<QVector2D,3> coordinates={ta,tb,tc};
        for(int i=0;i<3;++i){
            const auto v=points[i],n=normals[i];
            const auto uv=ta.x()<0?QVector2D(.5f+v.x()/(2*w),.5f-v.y()/(2*h)):coordinates[i];
            const Vertex out{v.x(),v.y(),v.z(),n.x(),n.y(),n.z(),uv.x(),uv.y()};vertices.append(reinterpret_cast<const char*>(&out),sizeof(out));
        }
    };
    // Quarter-circle edge profiles give highlights an actual rounded surface.
    struct Edge {float inset,z,radial,vertical;};
    std::array<Edge,8> rings;
    for(int k=0;k<4;++k){
        const float angle=k*float(M_PI)/6;
        rings[k]={bevel*(1-std::sin(angle)),-d+bevel*(1-std::cos(angle)),std::sin(angle),-std::cos(angle)};
        rings[7-k]={rings[k].inset,-rings[k].z,rings[k].radial,-rings[k].vertical};
    }
    for(int i=0;i<perimeter;++i){
        const int j=(i+1)%perimeter;
        add({0,0,d},point(i,bevel,d),point(j,bevel,d),{0,0,1},{0,0,1},{0,0,1});
        add({0,0,-d},point(j,bevel,-d),point(i,bevel,-d),{0,0,-1},{0,0,-1},{0,0,-1});
        for(int k=0;k<7;++k){
            const auto lo=rings[k],hi=rings[k+1];
            const auto a=point(i,lo.inset,lo.z),b=point(j,lo.inset,lo.z),c=point(j,hi.inset,hi.z),e=point(i,hi.inset,hi.z);
            const auto na=outward(i)*lo.radial+QVector3D(0,0,lo.vertical),nb=outward(j)*lo.radial+QVector3D(0,0,lo.vertical);
            const auto nc=outward(j)*hi.radial+QVector3D(0,0,hi.vertical),ne=outward(i)*hi.radial+QVector3D(0,0,hi.vertical);
            // Side and bevel textures need height in V. A planar top UV collapses
            // vertical faces and makes the normal-map tangent basis undefined.
            const float u0=float(i)/perimeter,u1=float(i+1)/perimeter;
            const QVector2D ta(u0,.5f+a.z()/(2*d)),tb(u1,.5f+b.z()/(2*d)),tc(u1,.5f+c.z()/(2*d)),te(u0,.5f+e.z()/(2*d));
            add(a,b,c,na,nb,nc,ta,tb,tc);add(a,c,e,na,nc,ne,ta,tc,te);
        }
    }
    clear();setStride(sizeof(Vertex));setVertexData(vertices);setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
    addAttribute(QQuick3DGeometry::Attribute::PositionSemantic,0,QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::NormalSemantic,3*sizeof(float),QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::TexCoord0Semantic,6*sizeof(float),QQuick3DGeometry::Attribute::F32Type);
    setBounds(QVector3D(-w,-h,-d),QVector3D(w,h,d));update();
}
