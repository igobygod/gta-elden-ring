#pragma once
#include <cmath>
#include <cstdint>
struct PoseTransition {
 struct Result { bool reset,capture; float alpha; };
 uintptr_t owner=0;int action=0;float clock=0,age=1,duration=.1f;
 Result update(uintptr_t who,int next,float time,float dt,bool discontinuity){
  if(owner!=who||discontinuity){owner=who;action=next;clock=time;age=1;return {true,false,1};}
  bool changed=next!=action||(next!=0&&time+.05f<clock);
  if(changed){duration=next==0?.14f:(next>=202000?.10f:.065f);age=0;action=next;}
  clock=time;float t=std::fmin(1.f,age/duration);float alpha=t*t*(3-2*t);age+=std::fmax(0.f,dt);
  return {false,changed,alpha};
 }
};

// Independent camera aim layered over the native target-lock orientation. Mouse
// deltas only move this view offset; they never change ER's Q target/body lock.
struct MouseAimOffset {
 float yaw=0,pitch=0;
 static constexpr float sensitivity=.002f;
 static constexpr float maxYaw=1.20f,maxPitch=.72f;
 void add(int dx,int dy){
  if(dx < -32768)dx=-32768;if(dx>32768)dx=32768;
  if(dy < -32768)dy=-32768;if(dy>32768)dy=32768;
  yaw=std::fmax(-maxYaw,std::fmin(maxYaw,yaw+float(dx)*sensitivity));
  pitch=std::fmax(-maxPitch,std::fmin(maxPitch,pitch-float(dy)*sensitivity));
 }
 void rotateYaw(float*right,float*forward)const{
  float c=std::cos(yaw),s=std::sin(yaw);
  for(int i=0;i<3;i++){float r=right[i],f=forward[i];right[i]=r*c-f*s;forward[i]=f*c+r*s;}
 }
 void release(float dt){
  if(!std::isfinite(dt)||dt<=0||dt>.25f){yaw=pitch=0;return;}
  float blend=1-std::exp(-dt*18.f);yaw+=(0-yaw)*blend;pitch+=(0-pitch)*blend;
  if(std::fabs(yaw)<.001f)yaw=0;if(std::fabs(pitch)<.001f)pitch=0;
 }
 void reset(){yaw=pitch=0;}
};
