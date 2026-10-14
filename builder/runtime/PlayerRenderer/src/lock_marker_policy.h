#pragma once
#include <cmath>
#include <cstdint>
#include "shoulder_aim_policy.h"
namespace lock_marker {
struct Data { int32_t state,x,y; };
static_assert(sizeof(Data)==12,"Native LockOn update reads three signed 32-bit fields");
struct Gate {
 bool normal=false,loading=true,fade=true,modal=true,dead=true,native_locked=false;
 uint32_t weapon=0;uintptr_t owner=0;uint64_t now=0;
};
inline bool eligible(const Data&d,const Gate&g,const ShoulderAimSnapshot&s,const float*view,float fov,float aspect){
 return d.state==1&&g.normal&&!g.loading&&!g.fade&&!g.modal&&!g.dead&&g.native_locked&&g.weapon>0&&g.weapon<8&&
  shoulder_aim::fresh(s,g.owner,g.now)&&s.locked&&s.free_aim&&shoulder_aim::matches(s,view)&&
  std::isfinite(fov)&&std::isfinite(s.fov)&&std::fabs(fov-s.fov)<.00001f&&
  std::isfinite(aspect)&&std::isfinite(s.aspect)&&std::fabs(aspect-s.aspect)<.00001f;
}
// Values come from the signature-verified normal marker producer's stage
// constants, not physical framebuffer dimensions or its unrelated debug branch.
inline bool stage_extent(float width,float height,uint32_t&outWidth,uint32_t&outHeight){
 if(!std::isfinite(width)||!std::isfinite(height)||width<64||height<64||width>16384||height>16384||
    width!=std::floor(width)||height!=std::floor(height))return false;
 outWidth=uint32_t(width);outHeight=uint32_t(height);return true;
}
// Matrix columns are world-space right/up/forward/origin. Output remains in
// native movie coordinates; Scaleform applies its existing viewport transform.
inline bool project(const Data&original,const float*target,const float*view,float fov,float aspect,uint32_t width,uint32_t height,Data&out){
 if(!target||!view||width<64||height<64||width>16384||height>16384||
    !std::isfinite(fov)||fov<=.1f||fov>=3.f||!std::isfinite(aspect)||aspect<.5f||aspect>4.f)return false;
 for(unsigned i=0;i<16;i++)if(!std::isfinite(view[i]))return false;
 float camera[3]={};
 for(unsigned i=0;i<3;i++){
  if(!std::isfinite(target[i]))return false;
  float delta=target[i]-view[12+i];
  camera[0]+=delta*view[i];camera[1]+=delta*view[4+i];camera[2]+=delta*view[8+i];
 }
 if(!std::isfinite(camera[2])||camera[2]<=.05f)return false;
 float tanHalf=std::tan(fov*.5f),x=camera[0]/(camera[2]*tanHalf*aspect),y=camera[1]/(camera[2]*tanHalf);
 // Preserve the game's off-screen marker behavior rather than invent clamping.
 if(!std::isfinite(x)||!std::isfinite(y)||std::fabs(x)>1.f||std::fabs(y)>1.f)return false;
 out=original;out.x=int32_t((.5f+.5f*x)*float(width));out.y=int32_t((.5f-.5f*y)*float(height));return true;
}
}
