#pragma once
#include <cmath>
#include <cstdint>
namespace muzzle_visual {
struct Spec { float uv[4]={}; float width=0,length=0,pivotAlong=.5f; bool sideview=false,valid=false;float centerAlong=0,lifetime=0;};
inline uint32_t hash(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
inline float random(uint32_t seed){return float(hash(seed)&0xffffff)/16777216.f;}
inline float mix(float a,float b,float r){return a+(b-a)*r;}
// Atlas4096 originalflare row6col0 (1x2); originalcore row6col4 (4x4).
// Return one source sprite component. Launcher source FlashFx is EMPTY: no flash.
// Original SMG/core frame is selected atspawn; no texture-animation behaviour exists.
inline Spec evaluate(unsigned weapon,float age,uint32_t shotSeed){
 Spec s;if(weapon<1||weapon>5||!std::isfinite(age)||age<0)return s;
 float r=random(shotSeed),scale=weapon==1||weapon==3?.8f:weapon==5?.5f:1.f;
 s.lifetime=weapon==2?mix(.07f,.08f,r):mix(.05f,.09f,r);if(age>=s.lifetime)return s;
 s.valid=true;
 // Original effectglobal zoomScalar ispercentage, before weaponFlashFxScale.
 float zoom=weapon==1?mix(.45f,.4911628f,r):weapon==2?mix(.50f,.60f,r):weapon==3?mix(.63f,.73f,r):weapon==4?mix(.16f,.25f,r):mix(.30f,.40f,r);scale*=zoom;
 if(weapon==2||weapon==3){
  unsigned frame=hash(shotSeed+37u)&15u;s.uv[0]=(4.f+(frame%4)*.25f)/8.f;s.uv[1]=(6.f+(frame/4)*.25f)/8.f;s.uv[2]=s.uv[3]=.25f/8.f;
  float u=age/s.lifetime;
  if(weapon==2){float lo=mix(.204782f,.470507f,fminf(1.f,u/.070853f)),hi=mix(.180542f,.471238f,fminf(1.f,u/.202899f));s.length=s.width=2.f*mix(lo,hi,r)*scale;s.centerAlong=.005349f*scale;}
  else{s.length=s.width=2.f*mix(mix(.08f,.3f,u),mix(.1f,.5f,u),r)*scale;s.centerAlong=.005581f*scale;}
 }else{
  s.uv[0]=0;s.uv[1]=6.5f/8.f;s.uv[2]=1.f/8.f;s.uv[3]=.5f/8.f;s.sideview=true;
  // TblrScalar is authored asymmetry. Exactsource values retained rather than
  // recentering the directionaltexture atits geometricmiddle.
  if(weapon==1){float l=.3f,rr=1.149768f;s.length=mix(1.4f,3.999999f,r)*(l+rr)*scale;s.width=mix(.6f,2.2f,r)*1.071628f*scale;s.pivotAlong=l/(l+rr);s.centerAlong=.038837f*scale;}
  else {float l=.45f,rr=1.964651f;s.length=mix(1.035484f,1.990965f,r)*(l+rr)*scale;s.width=mix(.6f,1.05462f,r)*1.205582f*scale;s.pivotAlong=l/(l+rr);s.centerAlong=.25f*scale;}
 }
 return s;
}
// For sideview: axis=normalizedgunWorld(muzzle+X)-gunWorld(muzzle),
// side=normalize(cross(axis,viewToCamera)); center=muzzle+axis*centerAlong.
// corners = center + axis*((u-pivotAlong)*length) + side*((v-.5)*width).
// For core: cameraRight/cameraUp, centered atmuzzle+axis*centerAlong.
// Source domains/damping, randomrotations, smoke, lights, alternate-star emitters
// are not reproduced here. Sizecurve phase uses normalizedparticleage; fullRAGE
// interpolation semantics remain an approximation. This fixes socket/sheet mapping.
}
