#pragma once
#include <math.h>
// Camera-space projection, normalized to viewport dimensions. ER camera FOV is
// vertical radians. No game pointers or writes are used by this calculation.
inline bool fx_project(const float* m,float fov,float aspect,const float* p,float* out){
 if(!isfinite(fov)||fov<.1f||fov>2.9f||!isfinite(aspect)||aspect<.5f||aspect>4.f)return false;
 float v[3]={p[0]-m[12],p[1]-m[13],p[2]-m[14]};
 float x=0,y=0,z=0;for(int i=0;i<3;i++){x+=v[i]*m[i];y+=v[i]*m[4+i];z+=v[i]*m[8+i];}
 if(!isfinite(x)||!isfinite(y)||!isfinite(z)||z<.15f||z>200.f)return false;
 float k=1.f/tanf(fov*.5f);out[0]=.5f+x*k/(2.f*aspect*z);out[1]=.5f-y*k/(2.f*z);out[2]=k/(2.f*z);
 return isfinite(out[0])&&isfinite(out[1])&&fabsf(out[0]-.5f)<2.f&&fabsf(out[1]-.5f)<2.f;
}
