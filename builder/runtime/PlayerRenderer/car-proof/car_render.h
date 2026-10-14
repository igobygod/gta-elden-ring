#pragma once
#include "car_asset.h"
#include <cmath>
#include <vector>
namespace car_render {
// Every transform consumes converted GTA metres (x,z,-y); the car never uses
// Franklin's pelvis offset. Matrices are column-major with world translation.
template<class Vertex,class Snapshot>
bool emit(const carasset::Asset&a,const Snapshot&s,Vertex*out,size_t&count,size_t capacity,
          const float*cam,float k,float aspect,float nearZ,float farZ){
 if(!s.visible||a.meshes.size()!=5)return true;
 float range2=0;for(int j=0;j<3;j++){float d=s.chassis[12+j]-cam[12+j];range2+=d*d;}
 if(!std::isfinite(range2)||range2>10000)return true;
 size_t required=0;for(const auto&m:a.meshes)required+=m.indices.size();if(required>capacity-count)return false;
 static std::vector<Vertex> projected;
 for(size_t mi=0;mi<a.meshes.size();mi++){
  const auto&m=a.meshes[mi];const float*t=mi?s.wheels[mi-1]:s.chassis;
  for(int j=0;j<16;j++)if(!std::isfinite(t[j]))return false;
  projected.resize(m.vertices.size());
  for(size_t vi=0;vi<m.vertices.size();vi++){
   const auto&v=m.vertices[vi];const float p[3]={v.p[0],v.p[2],-v.p[1]},n[3]={v.n[0],v.n[2],-v.n[1]};float world[3],normal[3];
   for(int j=0;j<3;j++){world[j]=t[j]*p[0]+t[4+j]*p[1]+t[8+j]*p[2]+t[12+j];normal[j]=t[j]*n[0]+t[4+j]*n[1]+t[8+j]*n[2];}
   float x=0,y=0,z=0;for(int j=0;j<3;j++){float d=world[j]-cam[12+j];x+=d*cam[j];y+=d*cam[4+j];z+=d*cam[8+j];}
   Vertex o={x*k/aspect,y*k,nearZ*(farZ-z)/(farZ-nearZ),z,v.uv[0],v.uv[1]};
   o.r=normal[0];o.g=normal[1];o.b=normal[2];o.a=1;projected[vi]=o;
  }
  for(const auto&r:m.ranges){if(a.materials[r.material].kind==4)continue;
   for(uint32_t j=0;j<r.count;j++){Vertex v=projected[m.indices[r.first+j]];v.frameBlend=100.f+float(r.material);out[count++]=v;}
  }
 }
 return true;
}
}
