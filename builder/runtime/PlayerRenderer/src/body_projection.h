#pragma once
#include <cstddef>
#include <vector>

namespace body_projection {
// Project the posed body once per unique model vertex. The renderer gathers
// these values in the original index order so triangle order and attributes
// remain unchanged.
template<class Vertex>
inline void project_unique(const float* model,size_t nv,const float* cam,const float* facing,
                           const float* pos,const float* uvs,const float* bodyLight,
                           float bodyAlpha,float k,float aspect,float nearZ,float farZ,
                           std::vector<Vertex>& projected) {
 projected.resize(nv);
 for(size_t index=0;index<nv;index++){
  size_t src=index*3;float xlocal=-model[src],zlocal=-model[src+1];
  float wx=xlocal*facing[1]+zlocal*facing[0]+pos[0];
  float wy=model[src+2]+pos[1]+.92871094f;
  float wz=-xlocal*facing[0]+zlocal*facing[1]+pos[2];
  float delta[3]={wx-cam[12],wy-cam[13],wz-cam[14]};float x=0,y=0,z=0;
  for(int i=0;i<3;i++){x+=delta[i]*cam[i];y+=delta[i]*cam[i+4];z+=delta[i]*cam[i+8];}
  Vertex value={x*k/aspect,y*k,nearZ*(farZ-z)/(farZ-nearZ),z,uvs[index*2]*.5f,uvs[index*2+1]*.5f};
  value.a=bodyAlpha;value.r=value.g=value.b=bodyLight[index];projected[index]=value;
 }
}
}
