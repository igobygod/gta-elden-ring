#pragma once
#include "../../src/arsenal_animation.h"
namespace donor_strafe {
inline float canonicalBearing(float vx,float vz,float visualYaw){
 float c=std::cos(visualYaw),s=std::sin(visualYaw);
 return std::atan2(-vx*c+vz*s,-vx*s-vz*c);
}
struct Clip {arsenal::Clip pose;unsigned family=0,gait=0,sector=0;float cycles=1,distance=1,offset=0;};
class DirectionalRig {
 std::vector<Clip> clips;float phase=0,bearing=0;bool headingReady=false;
 const Clip* find(unsigned weapon,unsigned gait,unsigned sector)const{
  if(weapon==7)for(const auto&c:clips)if(c.family==7&&c.gait==gait&&c.sector==sector)return &c;
  for(const auto&c:clips)if(c.family==0&&c.gait==gait&&c.sector==sector)return &c;return nullptr;
 }
public:
 bool load(const char*path,unsigned bones){
  FILE*f=igobygod::open(path,"rb");if(!f)return false;unsigned h[4];bool ok=arsenal::Rig::read(f,h,4)&&h[0]==0x41444952&&h[1]==1&&h[2]==bones&&h[3]>=16&&h[3]<=64;
  std::vector<Clip> next;if(ok)next.resize(h[3]);
  for(auto&c:next){unsigned n[4];float v[4];ok=ok&&arsenal::Rig::read(f,n,4)&&arsenal::Rig::read(f,v,4);if(!ok)break;
   c.family=n[0];c.gait=n[1];c.sector=n[2];c.pose.frames=n[3];c.pose.duration=v[0];c.cycles=v[1];c.distance=v[2];c.offset=v[3];
   ok=(c.family==0||c.family==7)&&c.gait<2&&c.sector<8&&c.pose.frames>=2&&c.pose.frames<=241&&std::isfinite(v[0])&&v[0]>0&&v[0]<20&&std::isfinite(v[1])&&v[1]>=1&&v[1]<=4&&std::isfinite(v[2])&&v[2]>.1f&&v[2]<30&&std::isfinite(v[3]);if(!ok)break;
   ok=arsenal::Rig::readv(f,c.pose.t,size_t(bones)*c.pose.frames)&&arsenal::Rig::readv(f,c.pose.q,size_t(bones)*c.pose.frames);for(auto t:c.pose.t)ok=ok&&arsenal::Rig::finite(t);for(auto q:c.pose.q)ok=ok&&arsenal::Rig::quaternion(q);if(!ok)break;
  }
  ok=ok&&fgetc(f)==EOF;fclose(f);if(!ok)return false;clips=std::move(next);
  for(unsigned g=0;g<2;g++)for(unsigned s=0;s<8;s++)if(!find(0,g,s)){clips.clear();return false;}return true;
 }
 // vx/vz are observed native position differences, only their heading is used.
 // Invert the exact renderer worldPoint XY->XZ transform.
 // Donor +Y points toward camera forward when visualYaw=aimYaw=-camera forward.
 bool sample(const arsenal::Rig&rig,unsigned weapon,float vx,float vz,float aimYaw,float speed,float dt,bool reset,arsenal::Pose&out){
  if(clips.empty()||!std::isfinite(speed)||!std::isfinite(dt))return false;
  float target=canonicalBearing(vx,vz,aimYaw);bool moving=vx*vx+vz*vz>2.5e-7f;
  if(reset||!headingReady){phase=0;bearing=moving?target:0;headingReady=true;}
  else if(moving&&dt>0&&dt<=.25f)bearing=donor_motion::wrap(bearing+donor_motion::wrap(target-bearing)*(1-std::exp(-dt*14.f)));
  constexpr float turn=6.28318530718f;float angle=bearing;if(angle<0)angle+=turn;float sector=angle/(turn/8);unsigned a=unsigned(sector)%8,b=(a+1)%8;float u=sector-std::floor(sector);
  const Clip* use[4]={find(weapon,0,a),find(weapon,0,b),find(weapon,1,a),find(weapon,1,b)};for(auto c:use)if(!c)return false;
  float walkSpeed=(1-u)*use[0]->distance/use[0]->pose.duration+u*use[1]->distance/use[1]->pose.duration;
  float runSpeed=(1-u)*use[2]->distance/use[2]->pose.duration+u*use[3]->distance/use[3]->pose.duration;
  float run=std::fmin(1.f,std::fmax(0.f,(speed-walkSpeed)/std::fmax(.1f,runSpeed-walkSpeed)));
  float weights[4]={(1-run)*(1-u),(1-run)*u,run*(1-u),run*u},cycleDistance=0;
  for(unsigned j=0;j<4;j++)cycleDistance+=weights[j]*use[j]->distance/use[j]->cycles;
  if(moving&&dt>0&&dt<=.25f)phase=std::fmod(phase+std::fmin(dt,.1f)*std::fmax(speed,0.f)/std::fmax(.1f,cycleDistance),12.f);
  float total=0;arsenal::Pose part;
  for(unsigned j=0;j<4;j++){if(weights[j]<1e-5f)continue;const auto&c=*use[j];rig.sample(&c.pose,(phase+c.offset)*c.pose.duration/c.cycles,true,part);if(total==0)out=part;else rig.blend(out,part,weights[j]/(total+weights[j]));total+=weights[j];}
  return total>0;
 }
};
}
