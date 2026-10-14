#pragma once
#include "../../src/arsenal_animation.h"
namespace melee_animation {
constexpr uint32_t kIdle=0,kGaitIdle=90000000,kGaitWalk=90000001,kGaitRun=90000002,kGaitSprint=90000003;
struct Clip {uint32_t raw_id=0,frames=0,flags=0;float duration=0;std::vector<arsenal::V> t;std::vector<arsenal::Q> q;std::vector<arsenal::M> weapon;};
inline float dot(arsenal::V a,arsenal::V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline arsenal::V cross(arsenal::V a,arsenal::V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline arsenal::V unit(arsenal::V a){float s=std::sqrt(dot(a,a));return s>1e-6f?arsenal::V{a.x/s,a.y/s,a.z/s}:arsenal::V{1,0,0};}
inline arsenal::M anchorMix(const arsenal::M&a,const arsenal::M&b,float u){
 arsenal::M m={};for(int j=0;j<16;j++)m.v[j]=a.v[j]+(b.v[j]-a.v[j])*u;
 // Original ER -> GTA axis mapping reflects handedness. Preserve det=-1 instead
 // of converting this reflected frame directly to a quaternion.
 arsenal::V x=unit({m.v[0],m.v[4],m.v[8]}),y={m.v[1],m.v[5],m.v[9]};float p=dot(x,y);y=unit({y.x-p*x.x,y.y-p*x.y,y.z-p*x.z});auto z=cross(x,y);
 float handed=dot(cross({a.v[0],a.v[4],a.v[8]},{a.v[1],a.v[5],a.v[9]}),{a.v[2],a.v[6],a.v[10]})<0?-1.f:1.f;z={z.x*handed,z.y*handed,z.z*handed};
 m.v[0]=x.x;m.v[4]=x.y;m.v[8]=x.z;m.v[1]=y.x;m.v[5]=y.y;m.v[9]=y.z;m.v[2]=z.x;m.v[6]=z.y;m.v[10]=z.z;m.v[12]=m.v[13]=m.v[14]=0;m.v[15]=1;return m;
}
class Packet {public:
 unsigned bones=0;std::vector<Clip> clips;arsenal::M fallbackAttachment={};
 bool load(const char*path,unsigned expectedBones,const char*attachmentPath=nullptr){
  FILE*f=igobygod::open(path,"rb");if(!f)return false;unsigned h[4];bool ok=arsenal::Rig::read(f,h,4)&&h[0]==0x4d4a4e54&&h[1]==1&&h[2]==expectedBones&&h[2]>0&&h[2]<=512&&h[3]>0&&h[3]<=64;
  if(!ok){fclose(f);return false;}std::vector<Clip> next;next.reserve(h[3]);size_t byteBudget=0;
  for(unsigned i=0;i<h[3]&&ok;i++){
   Clip c;ok=arsenal::Rig::read(f,&c.raw_id,1)&&arsenal::Rig::read(f,&c.frames,1)&&arsenal::Rig::read(f,&c.duration,1)&&arsenal::Rig::read(f,&c.flags,1)&&c.frames>=2&&c.frames<=150&&c.duration>0&&c.duration<=30&&std::isfinite(c.duration)&&c.flags<=1;
   for(auto&old:next)if(old.raw_id==c.raw_id)ok=false;
   if(!ok)break;size_t n=size_t(c.frames)*h[2];byteBudget+=n*28+size_t(c.frames)*64;if(byteBudget>64*1024*1024){ok=false;break;}
   ok=arsenal::Rig::readv(f,c.t,n)&&arsenal::Rig::readv(f,c.q,n)&&arsenal::Rig::readv(f,c.weapon,c.frames);
   for(auto v:c.t)ok=ok&&arsenal::Rig::finite(v);for(auto q:c.q)ok=ok&&arsenal::Rig::quaternion(q);for(auto&m:c.weapon)ok=ok&&arsenal::Rig::affine(m);
   if(ok)next.push_back(std::move(c));
  }
  ok=ok&&fgetc(f)==EOF;fclose(f);if(!ok)return false;
  arsenal::M attach={{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}};
  if(attachmentPath){f=igobygod::open(attachmentPath,"rb");if(!f)return false;ok=arsenal::Rig::read(f,&attach,1)&&fgetc(f)==EOF&&arsenal::Rig::affine(attach);fclose(f);if(!ok)return false;}
  bones=h[2];clips=std::move(next);fallbackAttachment=attach;return true;
 }
 const Clip*find(uint32_t raw)const{for(auto&c:clips)if(c.raw_id==raw)return &c;return nullptr;}
 bool samplePhase(const Clip*c,float phase,bool loop,arsenal::Pose&out,arsenal::M&weapon)const{
  if(!c||!std::isfinite(phase)||!bones)return false;phase=std::max(0.f,phase);if(loop)phase-=std::floor(phase);else phase=std::min(phase,1.f);float fr=phase*(c->frames-1);unsigned a=std::min(unsigned(fr),c->frames-1),b=std::min(a+1,c->frames-1);float u=fr-a;out.t.resize(bones);out.q.resize(bones);
  for(unsigned j=0;j<bones;j++){size_t ia=size_t(a)*bones+j,ib=size_t(b)*bones+j;out.t[j]=arsenal::mix(c->t[ia],c->t[ib],u);out.q[j]=arsenal::mix(c->q[ia],c->q[ib],u);}weapon=anchorMix(c->weapon[a],c->weapon[b],u);return true;
 }
 bool sample(const Clip*c,float time,bool loop,arsenal::Pose&out,arsenal::M&weapon)const{return c&&samplePhase(c,time/c->duration,loop,out,weapon);}
 bool sampleNative(const Clip*c,float time,float nativeLength,arsenal::Pose&out,arsenal::M&weapon)const{return c&&std::isfinite(nativeLength)&&nativeLength>0&&samplePhase(c,time/nativeLength,false,out,weapon);}
};
}
