#pragma once
#include <cmath>
// GTA original Franklin skeleton: root=0, pelvis=1, spineRoot=45.
// Pelvis/spineRoot are siblings with zero translations, allowing the pelvis
// branch to turn without a skinned waist mask or twisting the aiming torso.
namespace donor_motion {
struct Q { float x,y,z,w; };
inline Q multiply(Q a,Q b){return {a.w*b.x+b.w*a.x+a.y*b.z-a.z*b.y,a.w*b.y+b.w*a.y+a.z*b.x-a.x*b.z,a.w*b.z+b.w*a.z+a.x*b.y-a.y*b.x,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
inline Q inverse(Q q){return {-q.x,-q.y,-q.z,q.w};}
inline float wrap(float a){return std::atan2(std::sin(a),std::cos(a));}
// Keep the Q-locked pelvis/body heading while allowing the aiming spine to
// follow the free camera within a comfortable shoulder range.
inline float torsoAimYaw(float bodyYaw,float aimYaw,float limit=1.22f){
 if(!std::isfinite(bodyYaw)||!std::isfinite(aimYaw)||!std::isfinite(limit)||limit<0)return 0;
 // ER camera aimYaw is atan2(-worldX,-worldZ). Renderer body transform
 // maps donor XY to ER (-X,-Z), so donor Z yaw has the opposite sign.
 return std::fmax(-limit,std::fmin(limit,wrap(bodyYaw-aimYaw)));
}
struct Direction { float pelvisYaw; bool reverseLower; };
// ER velocity heading and visual aim heading use atan2(dx,dz). Its world
// mapping is donor(x,y)->ER(-x,-y), therefore donor Z yaw = aim-movement.
inline Direction direction(float dx,float dz,float aimYaw){
 constexpr float pi=3.14159265358979323846f;
 float relative=wrap(aimYaw-std::atan2(dx,dz));
 bool reverse=std::cos(relative)<-1e-5f;
 if(reverse)relative=wrap(relative+(relative>0?-pi:pi));
 return {relative,reverse};
}
inline Q pelvisDirection(Q root,Q pelvis,float donorYaw){
 Q yaw={0,0,std::sin(donorYaw*.5f),std::cos(donorYaw*.5f)};
 return multiply(multiply(multiply(inverse(root),yaw),root),pelvis);
}
inline float lowerPhase(float phase,bool reverse){return reverse ? (phase>0 ? 1-phase : 0):phase;}
constexpr float rifleCycleDistance[4]={0,3.7174620733f,5.8495041478f,7.6405797526f};
constexpr float rifleSourceSpeed[4]={0,1.7157518581f,4.7428401755f,6.9459790675f};
// Distance-normalized increment shared by forward and signed gait clocks.
inline float increment(float dt,float speed,const float weights[4]){
 float moving=weights[1]+weights[2]+weights[3],distance=0;
 for(int i=1;i<4;i++)distance+=weights[i]*rifleCycleDistance[i];
 if(!std::isfinite(dt)||dt>.25f||!std::isfinite(speed)||moving<=.001f||distance<=.01f)return 0;
 return std::fmin(std::fmax(dt,0.f),.1f)*std::fmax(speed,0.f)/(distance/moving);
}
inline float unitPhase(float phase){return phase-std::floor(phase);}
inline float advance(float phase,float dt,float speed,const float weights[4]){
 return unitPhase(phase+increment(dt,speed,weights));
}
// Reversing changes only the clock derivative, never reflects its current phase.
struct SignedPhase {
 float phase=0;
 void reset(float value=0){phase=unitPhase(value);}
 float advance(float dt,float speed,const float weights[4],bool reverse){
  phase=unitPhase(phase+increment(dt,speed,weights)*(reverse?-1.f:1.f));return phase;
 }
};
// +/-8.6 degree dead zone around sideways motion avoids noisy hemisphere flips.
// Yaw goals are still an approximation: genuine strafe clips are preferable.
struct DirectionFilter {
 bool reverse=false,initialized=false;
 float pelvisYaw=0;
 Direction update(float dx,float dz,float aimYaw,bool raised,float dt,bool reset=false){
  constexpr float pi=3.14159265358979323846f;
  bool moving=std::isfinite(dx)&&std::isfinite(dz)&&dx*dx+dz*dz>2.5e-7f;
  float relative=moving?wrap(aimYaw-std::atan2(dx,dz)):0.f;
  if(reset||!initialized){reverse=raised&&moving&&std::cos(relative)<0;pelvisYaw=0;initialized=true;}
  if(!raised)reverse=false;
  else if(moving){float forward=std::cos(relative);if(forward<-.15f)reverse=true;else if(forward>.15f)reverse=false;}
  float goal=pelvisYaw; // Retain planted lower-body heading while stationary.
  if(!raised)goal=0;
  else if(moving)goal=reverse?wrap(relative+(relative>0?-pi:pi)):relative;
  if(reset||dt<=0||dt>.25f)pelvisYaw=goal;
  else pelvisYaw=wrap(pelvisYaw+wrap(goal-pelvisYaw)*(1-std::exp(-dt*16.f)));
  return {pelvisYaw,reverse};
 }
};
}
