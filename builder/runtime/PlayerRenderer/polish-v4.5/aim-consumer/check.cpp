#include "../../src/rifle_aim_policy.h"
#include "../../src/projectile_policy.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using rifle_aim::Source;
int main(){
 // A rotated, translated camera catches accidental use of world-axis offsets.
 float view[16]={0,0,-1,0, 0,1,0,0, 1,0,0,0, 7,3,-9,1};
 constexpr uintptr_t owner=0x123456;constexpr uint64_t now=2000;
 shoulder_aim::Offset offset;offset.add(100,-50,16.f/9.f,false);
 ShoulderAimSnapshot snapshot={};assert(shoulder_aim::make(snapshot,owner,now,7,view,.733038286f,16.f/9.f,offset,true,false));
 uint64_t claimed=0;assert(shoulder_aim::claim(snapshot,owner,now+1,view,42,42,claimed));
 assert(!shoulder_aim::claim(snapshot,owner,now+1,view,42,42,claimed));
 float origin[3],direction[3];
 assert(rifle_aim::select(owner,now+1,true,view,&snapshot,origin,direction)==Source::FreeAim);
 // The consumed ray projects exactly to the free reticle at all weapon ranges.
 for(float range:{30.f,70.f,85.f,120.f,250.f}){
  float target[3],delta[3];for(unsigned i=0;i<3;i++){target[i]=origin[i]+direction[i]*range;delta[i]=target[i]-view[12+i];}
  float right=0,up=0,forward=0;for(unsigned i=0;i<3;i++){right+=delta[i]*view[i];up+=delta[i]*view[4+i];forward+=delta[i]*view[8+i];}
  float tangent=std::tan(snapshot.fov*.5f);
  assert(std::abs(right/(forward*tangent*snapshot.aspect)-snapshot.ndc[0])<1e-5f);
  assert(std::abs(up/(forward*tangent)-snapshot.ndc[1])<1e-5f);
 }
 // GL/RPG converge on the same selected point after muzzle parallax; their
 // unchanged solver retains its own speed/gravity and the native cover casts.
 using namespace projectile_policy;V target={origin[0]+direction[0]*20,origin[1]+direction[1]*20,origin[2]+direction[2]*20};V muzzle={7,1.3f,-8.3f};
 for(unsigned variant=0;variant<2;variant++){
  float speed=variant?rpg_speed:launcher_speed,gravity=variant?rpg_gravity:launcher_gravity;V velocity;assert(launch_velocity(muzzle,target,speed,gravity,velocity));
  float time=std::hypot(target.x-muzzle.x,target.z-muzzle.z)/std::hypot(velocity.x,velocity.z);V end=add(muzzle,mul(velocity,time));end.y-=.5f*gravity*time*time;assert(length(sub(end,target))<.0001f);
 }
 auto rejects=[&](ShoulderAimSnapshot s,uintptr_t requestedOwner,uint64_t t,const float*m){assert(rifle_aim::select(requestedOwner,t,true,m,&s,origin,direction)==Source::Unavailable);};
 rejects(snapshot,owner+1,now+1,view);rejects(snapshot,owner,now-1,view);rejects(snapshot,owner,now+101,view);
 for(unsigned failure=0;failure<7;failure++){auto s=snapshot;if(failure==0)s.ready=0;if(failure==1)s.locked=0;if(failure==2)s.free_aim=0;if(failure==3)s.sequence=0;if(failure==4)s.direction[0]=std::numeric_limits<float>::quiet_NaN();if(failure==5)s.origin[1]=std::numeric_limits<float>::infinity();if(failure==6)for(float&v:s.direction)v=0;rejects(s,owner,now+1,view);}
 float moved[16];std::memcpy(moved,view,sizeof(view));moved[12]+=.1f;rejects(snapshot,owner,now+1,moved);
 assert(rifle_aim::select(owner,now+1,true,view,nullptr,origin,direction)==Source::Unavailable);
 // Unlock must immediately ignore even a stale/oppositely aimed Q snapshot.
 assert(rifle_aim::select(owner,now+1000,false,view,&snapshot,origin,direction)==Source::Camera);
 for(unsigned i=0;i<3;i++){assert(origin[i]==view[12+i]);assert(direction[i]==view[8+i]);}
 assert(rifle_aim::select(owner,now,false,view,nullptr,origin,direction)==Source::Camera);
 std::puts("PASS: independent Q ray/reticle agreement, unchanged unlocked ray, GL/RPG convergence, repeated-sequence guard, owner/time/view guards and invalid-ray rejection");
}
