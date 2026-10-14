#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
// Smooth, area-weighted geometric normals of the displayed pose. Does not alter it.
// GTA body coordinates are Z-up; use sky/ground hemispheres without inventing a sun position.
namespace body_lighting {
inline void shade(const float* p,size_t nv,const uint32_t* ix,size_t ni,std::vector<float>&normal,std::vector<float>&light){
 normal.assign(nv*3,0.f);light.assign(nv,.75f);
 for(size_t i=0;i+2<ni;i+=3){uint32_t a=ix[i],b=ix[i+1],c=ix[i+2];if(a>=nv||b>=nv||c>=nv)continue;
  float u[3],v[3];for(int k=0;k<3;k++){u[k]=p[b*3+k]-p[a*3+k];v[k]=p[c*3+k]-p[a*3+k];}
  float n[3]={u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]};
  if(!std::isfinite(n[0]+n[1]+n[2]))continue;
  for(uint32_t j:{a,b,c})for(int k=0;k<3;k++)normal[j*3+k]+=n[k];
 }
 for(size_t j=0;j<nv;j++){const float*n=&normal[j*3];float len=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
  if(len>1e-9f&&std::isfinite(len)){float up=std::clamp(n[2]/len,-1.f,1.f);light[j]=.72f+.20f*up;}
 }
}
}
