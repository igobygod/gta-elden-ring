#pragma once
#include <math.h>
// CSChrPhysicsModule orientation is XYZW (fromsoftware-rs rotation.rs).
// The already-working Bridge adapter uses yaw=atan2(world_x,world_z), then
// {0,sin(yaw/2),0,cos(yaw/2)} at physics+0x50/+0x60. Thus PHYSICS forward is
// quaternion-rotated +Z. The exported HKX's Master-local basis is different.
namespace facing_math {
inline bool normalized(const float* q,float* out){
 float n=0;for(unsigned i=0;i<4;i++){if(!isfinite(q[i]))return false;n+=q[i]*q[i];}
 if(n<0.25f||n>4.f)return false;n=1.f/sqrtf(n);
 for(unsigned i=0;i<4;i++)out[i]=q[i]*n;return true;
}
inline bool forward_xz(const float* q,float* out){
 float n[4];if(!normalized(q,n))return false;
 out[0]=2.f*(n[0]*n[2]+n[3]*n[1]);
 out[1]=1.f-2.f*(n[0]*n[0]+n[1]*n[1]);
 float l=out[0]*out[0]+out[1]*out[1];if(l<1.e-6f)return false;
 l=1.f/sqrtf(l);out[0]*=l;out[1]*=l;return true;
}
// Apply world-Y rotation only; retain the existing quaternion's pitch/roll.
// Reject a nearly vertical camera instead of selecting an unstable yaw.
inline bool align_yaw(const float* q,float camera_x,float camera_z,float* out,float* before_dot=nullptr){
 if(!isfinite(camera_x)||!isfinite(camera_z))return false;
 float horizontal=camera_x*camera_x+camera_z*camera_z;
 if(horizontal<1.e-4f||!isfinite(horizontal))return false;
 float n[4],old[2];if(!normalized(q,n)||!forward_xz(n,old))return false;
 horizontal=1.f/sqrtf(horizontal);camera_x*=horizontal;camera_z*=horizontal;
 float dot=old[0]*camera_x+old[1]*camera_z;
 float delta=atan2f(camera_x*old[1]-camera_z*old[0],dot);
 float s=sinf(delta*.5f),c=cosf(delta*.5f);
 out[0]=c*n[0]+s*n[2];out[1]=c*n[1]+s*n[3];
 out[2]=c*n[2]-s*n[0];out[3]=c*n[3]-s*n[1];
 if(before_dot)*before_dot=dot;return true;
}
}
