#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace car_camera {
struct Frame {float pivot[3]={},wish[3]={},direction[3]={},right[3]={},up[3]={},range=0;};
inline float distance(const float*a,const float*b){float d=0;for(int i=0;i<3;i++){float e=a[i]-b[i];d+=e*e;}return std::sqrt(d);}
inline bool frame(const float*chassis,Frame&out){
 for(int i=0;i<16;i++)if(!std::isfinite(chassis[i]))return false;
 float horizontal=std::hypot(chassis[8],chassis[10]);if(!std::isfinite(horizontal)||horizontal<.001f)return false;
 out={};out.pivot[0]=chassis[12];out.pivot[1]=chassis[13]+.65f;out.pivot[2]=chassis[14];
 out.wish[0]=out.pivot[0]+chassis[8]/horizontal*7.2f;
 out.wish[1]=out.pivot[1]+2.5f;
 out.wish[2]=out.pivot[2]+chassis[10]/horizontal*7.2f;
 out.range=distance(out.pivot,out.wish);
 if(!std::isfinite(out.range)||out.range<.001f)return false;
 for(int i=0;i<3;i++)out.direction[i]=(out.wish[i]-out.pivot[i])/out.range;
 out.right[0]=-chassis[10]/horizontal;out.right[2]=chassis[8]/horizontal;
 out.up[0]=-out.direction[1]*out.right[2];
 out.up[1]=-out.direction[2]*out.right[0]+out.direction[0]*out.right[2];
 out.up[2]=out.direction[1]*out.right[0];
 return true;
}
struct Chase {
 uintptr_t owner=0;unsigned sequence=0;uint64_t time=0;float range=.05f;Frame previous;
 float resolve(uintptr_t nextOwner,unsigned nextSequence,uint64_t now,const Frame&view,float allowed,bool verified){
  bool reset=!owner||owner!=nextOwner||sequence!=nextSequence||now<time||now-time>250;
  float dt=reset?0.f:std::min(.1f,float(now-time)*.001f);
  allowed=std::max(.05f,std::min(view.range,allowed));
  if(!verified){
   // A failed ray never grants extra space. Movement spends the clearance
   // established for the previous camera path, including changes in heading.
   float safe=reset?.05f:std::max(.05f,range-distance(view.pivot,previous.pivot)-distance(view.wish,previous.wish));
   allowed=std::min(allowed,safe);
  }
  if(reset||allowed<range)range=allowed;
  else if(verified)range+=(allowed-range)*(1.f-std::exp(-dt*6.f));
  owner=nextOwner;sequence=nextSequence;time=now;previous=view;return range;
 }
};
}
