#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace scene_integration {
// Keep lighting separate from geometry; no changes to native movement/hitboxes.
template<class Vertex>
inline void body_normals(std::vector<Vertex>& projected,const float* normals,
                         const float* facing,const float* cam){
 for(size_t v=0;v<projected.size();v++){
  const float* n=normals+v*3;float d=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
  float world[3]={0,1,0};
  if(std::isfinite(d)&&d>1e-9f){world[0]=(-n[0]*facing[1]-n[1]*facing[0])/d;world[1]=n[2]/d;world[2]=(n[0]*facing[0]-n[1]*facing[1])/d;}
  auto& p=projected[v];p.r=p.g=p.b=0;
  for(int a=0;a<3;a++){p.r+=world[a]*cam[a];p.g+=world[a]*cam[4+a];p.b+=world[a]*cam[8+a];}
  p.nu=world[1];p.softness=-20; // PS body material; weapons/FX retain their layout.
 }
}
struct Foot {float local[3]={},height=0,opacity=0;bool valid=false;};
class FootContacts {
 std::vector<uint32_t> sole[2];size_t vertices=0;
public:
 template<class Point> void prepare(const std::vector<Point>& rest){
  if(vertices==rest.size())return;vertices=rest.size();sole[0].clear();sole[1].clear();
  float bottom=1e9f;for(const auto& p:rest)bottom=std::min(bottom,p.z);
  for(uint32_t i=0;i<rest.size();i++)if(rest[i].z<bottom+.075f)sole[rest[i].x>0?1:0].push_back(i);
 }
 Foot sample(unsigned side,const float* pose,size_t nv,float playerY,float floorY)const{
  Foot f;if(side>1||sole[side].empty()||vertices!=nv)return f;
  float low=1e9f;for(uint32_t i:sole[side]){for(int a=0;a<3;a++)f.local[a]+=pose[i*3+a];low=std::min(low,pose[i*3+2]);}
  for(float& a:f.local)a/=float(sole[side].size());
  f.height=low+playerY+.92871094f-floorY;
  // No planted shadow from a lifted foot, airborne body or another stair tread.
  f.valid=std::isfinite(f.height)&&f.height>-.09f&&f.height<.22f;
  f.opacity=f.valid?.48f*std::exp(-std::max(f.height,0.f)*15.f):0;return f;
 }
};
}
