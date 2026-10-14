#pragma once
#include "shoulder_aim_policy.h"
namespace rifle_aim {
enum class Source {Unavailable,Camera,FreeAim};
// The producer's same-frame claim happens once per rifle tick. Recheck owner,
// time and view when firing; a missing Q ray must never fall back to auto-aim.
inline Source select(uintptr_t owner,uint64_t now,bool locked,const float*view,const ShoulderAimSnapshot*s,float*origin,float*direction){
 if(!view||!origin||!direction)return Source::Unavailable;
 const float*from=view+12;const float*forward=view+8;
 if(locked){
  if(!s||!s->locked||!s->free_aim||!shoulder_aim::fresh(*s,owner,now)||!shoulder_aim::matches(*s,view))return Source::Unavailable;
  from=s->origin;forward=s->direction;
 }
 float length=0;for(unsigned i=0;i<3;i++){if(!std::isfinite(from[i])||!std::isfinite(forward[i]))return Source::Unavailable;length+=forward[i]*forward[i];}
 if(!std::isfinite(length)||length<.000001f)return Source::Unavailable;
 length=std::sqrt(length);for(unsigned i=0;i<3;i++){origin[i]=from[i];direction[i]=forward[i]/length;}
 return locked?Source::FreeAim:Source::Camera;
}
}
