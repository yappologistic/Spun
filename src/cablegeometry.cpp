#include "cablegeometry.h"
#include <array>
#include <cmath>
CableGeometry::CableGeometry(QQuick3DObject *p):QQuick3DGeometry(p){connect(this,&CableGeometry::shapeChanged,this,&CableGeometry::rebuild);rebuild();}
void CableGeometry::rebuild(){
    struct V{float x,y,z,nx,ny,nz,u,v;};QList<V> vertices;QList<quint32> indices;
    const QVector3D a=m_start,b=a+QVector3D(-32,119,-14),d=m_end,c=d+QVector3D(32,119,-14);
    QVector3D lo(1e5,1e5,1e5),hi(-1e5,-1e5,-1e5);
    for(int i=0;i<=96;++i){const float t=i/96.f,s=1-t;const auto p=s*s*s*a+3*s*s*t*b+3*s*t*t*c+t*t*t*d;
        const auto tangent=(3*s*s*(b-a)+6*s*t*(c-b)+3*t*t*(d-c)).normalized();
        const auto n=QVector3D::crossProduct(tangent,QVector3D(0,0,1)).normalized(),bn=QVector3D::crossProduct(tangent,n).normalized();
        for(int j=0;j<=12;++j){float angle=j*2*M_PI/12;auto normal=n*std::cos(angle)+bn*std::sin(angle);auto v=p+normal*1.65;
            vertices.append({v.x(),v.y(),v.z(),normal.x(),normal.y(),normal.z(),float(j)/12,t*12});
            for(int axis=0;axis<3;++axis){lo[axis]=std::min(lo[axis],v[axis]);hi[axis]=std::max(hi[axis],v[axis]);}
            if(i<96&&j<12){quint32 k=i*13+j;indices.append({k,k+1,k+13,k+1,k+14,k+13});}
        }
    }
    clear();setStride(sizeof(V));setPrimitiveType(PrimitiveType::Triangles);addAttribute(Attribute::PositionSemantic,0,Attribute::F32Type);addAttribute(Attribute::NormalSemantic,12,Attribute::F32Type);addAttribute(Attribute::TexCoord0Semantic,24,Attribute::F32Type);addAttribute(Attribute::IndexSemantic,0,Attribute::U32Type);
    setVertexData(QByteArray(reinterpret_cast<const char*>(vertices.constData()),vertices.size()*sizeof(V)));setIndexData(QByteArray(reinterpret_cast<const char*>(indices.constData()),indices.size()*4));setBounds(lo,hi);update();
}
