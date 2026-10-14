#pragma once
#include "../src/arsenal_animation.h"
namespace car_driver {
struct Animation {
 std::vector<arsenal::Clip> clips;
 bool load(const char*path,unsigned bones){FILE*f=igobygod::open(path,"rb");if(!f)return false;unsigned h[4];bool ok=arsenal::Rig::read(f,h,4)&&h[0]==0x44524956&&h[1]==1&&h[2]==bones&&h[3]==4;std::vector<arsenal::Clip> next;
  for(unsigned i=0;i<4&&ok;i++){arsenal::Clip c={};ok=arsenal::Rig::read(f,&c.frames,1)&&arsenal::Rig::read(f,&c.duration,1)&&c.frames>1&&c.frames<480&&std::isfinite(c.duration)&&c.duration>0&&c.duration<10;if(!ok)break;ok=arsenal::Rig::readv(f,c.t,size_t(c.frames)*bones)&&arsenal::Rig::readv(f,c.q,size_t(c.frames)*bones);for(auto t:c.t)ok=ok&&arsenal::Rig::finite(t);for(auto q:c.q)ok=ok&&arsenal::Rig::quaternion(q);next.push_back(std::move(c));}
  ok=ok&&fgetc(f)==EOF;fclose(f);if(ok)clips=std::move(next);return ok;
 }
 template<class Snapshot> bool pose(arsenal::Rig&rig,const Snapshot&s,float seconds,std::vector<float>&model,const float*pos){if(clips.size()!=4)return false;arsenal::Pose pose,steering;
  unsigned c=s.driver_phase==1?2:s.driver_phase==3?3:0;float duration=s.driver_phase==1?1.f:1.1f;float phase=std::max(0.f,std::min(1.f,s.phase_seconds/duration));
  rig.sample(&clips[c],c?phase*clips[c].duration:seconds,true,pose);
  if(c){rig.sample(&clips[c],phase*clips[c].duration,false,pose);}else{float st=std::max(0.f,std::min(1.f,.5f+s.steer/.9f));rig.sample(&clips[1],st*clips[1].duration,false,steering);rig.blend(pose,steering,std::min(1.f,std::fabs(s.steer)*4));}
  rig.matrices(pose);rig.skin(model);
  float t=phase*phase*(3-2*phase);float anchor[3];for(int i=0;i<3;i++)anchor[i]=s.chassis[12+i]-.4228f*s.chassis[i]-.133352f*s.chassis[4+i]-.122932f*s.chassis[8+i];
  // GTA locomotion extraction does not include the engine's door/seat root task.
  // Blend the root between verified entry/exit feet and the actual seat bone.
  if(c==2)for(int i=0;i<3;i++)anchor[i]=s.entry[i]+(i==1?.92871094f:0)+(anchor[i]-s.entry[i]-(i==1?.92871094f:0))*t;
  if(c==3)for(int i=0;i<3;i++)anchor[i]+=(s.exit[i]+(i==1?.92871094f:0)-anchor[i])*t;
  for(size_t v=0;v<model.size();v+=3){float x=model[v],y=model[v+2],z=-model[v+1];float w[3];for(int i=0;i<3;i++)w[i]=anchor[i]+s.chassis[i]*x+s.chassis[4+i]*y+s.chassis[8+i]*z;model[v]=pos[0]-w[0];model[v+1]=pos[2]-w[2];model[v+2]=w[1]-pos[1]-.92871094f;}
  return true;
 }
};
}
