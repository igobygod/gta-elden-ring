#pragma once
#include <cmath>
#include <cstring>
#include "shoulder_camera.h"
namespace shoulder_aim {
inline float clamp(float n,float lo,float hi){return std::fmax(lo,std::fmin(hi,n));}
struct Offset {
 // Height-relative NDC units: horizontal and vertical mouse travel have the
 // same pixel scale regardless of aspect. Positive Y means screen-up.
 float x=0,y=0;static constexpr float sensitivity=.003f;
 void reset(){x=y=0;}
 void bound(float aspect,bool scope){
  if(!std::isfinite(aspect)||aspect<.5f||aspect>4.f){reset();return;}
  if(scope){float r=std::hypot(x,y);if(r>.70f){x*=.70f/r;y*=.70f/r;}}
  else{x=clamp(x,-.78f*aspect,.78f*aspect);y=clamp(y,-.70f,.70f);}
 }
 void add(int dx,int dy,float aspect,bool scope){
  // Saturated hardware accumulation cannot wrap or launch the reticle offscreen.
  x+=clamp(float(dx),-32768.f,32768.f)*sensitivity;
  y-=clamp(float(dy),-32768.f,32768.f)*sensitivity;bound(aspect,scope);
 }
 void ndc(float aspect,float*out)const{out[0]=x/aspect;out[1]=y;}
};
inline bool make(ShoulderAimSnapshot&s,uintptr_t owner,uint64_t now,uint64_t sequence,const float*view,float fov,float aspect,const Offset&o,bool locked,bool scope){
 s={};if(!owner||!view||!sequence||!std::isfinite(fov)||fov<=.1f||fov>=3.f||!std::isfinite(aspect)||aspect<.5f||aspect>4.f)return false;
 for(unsigned i=0;i<16;i++)if(!std::isfinite(view[i]))return false;
 s.owner=owner;s.time=now;s.sequence=sequence;s.locked=locked;s.free_aim=locked;s.scoped=scope;s.fov=fov;s.aspect=aspect;
 if(locked)o.ndc(aspect,s.ndc);if(!std::isfinite(s.ndc[0])||!std::isfinite(s.ndc[1]))return false;
 float t=std::tan(fov*.5f),length=0;
 for(unsigned i=0;i<3;i++){s.origin[i]=view[12+i];s.direction[i]=view[8+i]+view[i]*(s.ndc[0]*aspect*t)+view[4+i]*(s.ndc[1]*t);length+=s.direction[i]*s.direction[i];}
 if(!std::isfinite(length)||length<.000001f)return false;length=std::sqrt(length);for(float&v:s.direction)v/=length;
 std::memcpy(s.view,view,sizeof(s.view));s.ready=1;return true;
}
inline bool fresh(const ShoulderAimSnapshot&s,uintptr_t owner,uint64_t now){return s.ready&&owner&&s.owner==owner&&s.sequence&&now>=s.time&&now-s.time<=100;}
inline bool matches(const ShoulderAimSnapshot&s,const float*view){if(!view)return false;for(unsigned i=0;i<16;i++)if(!std::isfinite(view[i])||!std::isfinite(s.view[i])||std::fabs(view[i]-s.view[i])>.00001f)return false;return true;}
inline bool claim(const ShoulderAimSnapshot&s,uintptr_t owner,uint64_t now,const float*view,uint32_t producer,uint32_t caller,uint64_t&previous){
 if(!producer||caller!=producer||!s.sequence||s.sequence<=previous)return false;
 previous=s.sequence;return fresh(s,owner,now)&&matches(s,view);
}
}
