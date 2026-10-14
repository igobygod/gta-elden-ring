#include "../../src/locomotion_mode_policy.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <initializer_list>
using namespace locomotion_mode;
static void near(float a,float b){assert(std::fabs(std::atan2(std::sin(a-b),std::cos(a-b)))<1e-5f);}
int main(){
 Heading h;constexpr float dt=.016f;
 auto relaxed=select(true,false,false,true),unlocked=select(true,false,false,false),aiming=select(true,true,false,true);
 assert(!relaxed.raised&&relaxed.relaxed_lock);assert(!unlocked.raised&&!unlocked.relaxed_lock);
 assert(aiming.raised&&!aiming.relaxed_lock);assert(select(true,false,true,true).raised);
 assert(!select(false,true,true,true).raised&&!select(false,true,true,true).relaxed_lock);
 // Entering Q at rest retains the displayed yaw despite a different native
 // enemy-facing yaw and a freely moved reticle. No initial zero-heading snap.
 near(h.update(relaxed,true,true,0,0,dt,2,-2,.7f,false),.7f);assert(h.source==Source::Held);
 near(h.update(relaxed,true,true,0,0,dt,1,-1,.7f,false),.7f);
 // All 8 movement directions face actual travel in the donor's world basis.
 for(unsigned i=0;i<8;i++){float a=i*.78539816339f,dx=.04f*std::sin(a),dz=.04f*std::cos(a);float goal=h.update(relaxed,true,true,dx,dz,dt,2,-2,.7f,false);near(goal,std::atan2(-dx,-dz));assert(h.source==Source::Movement);near(h.update(relaxed,true,true,0,0,dt,-1,1,goal,false),goal);}
 // Release aim while locked: retain the last visible aiming heading at rest,
 // then resume a forward gait oriented along real movement when walking.
 near(h.update(aiming,true,true,0,0,dt,2,-.4f,.7f,false),-.4f);assert(h.source==Source::Aim);
 near(h.update(relaxed,true,true,0,0,dt,2,-2,-.4f,false),-.4f);
 near(h.update(relaxed,true,true,.04f,0,dt,2,-2,-.4f,false),-1.57079632679f);
 // A missing independent aim snapshot must not redirect the visible body to
 // native Q target heading for one frame. The next real sample resumes aim.
 near(h.update(aiming,true,false,0,0,dt,2,-2,.9f,false),.9f);assert(h.source==Source::Held);
 near(h.update(aiming,true,true,0,0,dt,2,-.8f,.9f,false),-.8f);
 // Non-Q keeps original native relaxed heading and camera aim heading.
 near(h.update(unlocked,false,false,.04f,0,dt,1.2f,-2,.7f,false),1.2f);assert(h.source==Source::Native);
 near(h.update(select(true,true,false,false),false,false,0,0,dt,1.2f,-.3f,.7f,false),-.3f);
 // Native jump/roll/death or melee (armed=false) use native orientation.
 near(h.update(select(false,false,false,true),true,true,.04f,0,dt,1.6f,-2,.7f,false),1.6f);
 // Owner/life/teleport/time reset invalidates old held heading and displacement.
 near(h.update(relaxed,true,true,20,0,dt,1.1f,-2,-.7f,true),1.1f);
 for(float invalidDt:{0.f,-.1f,.25f,1.f})near(h.update(relaxed,true,true,.04f,0,invalidDt,2,-2,.7f,false),1.1f);
 near(h.update(relaxed,true,true,3,0,dt,2,-2,.7f,false),1.1f);
 near(h.update(relaxed,true,true,std::numeric_limits<float>::quiet_NaN(),0,dt,2,-2,.7f,false),1.1f);
 near(h.update(relaxed,true,true,.00001f,0,dt,2,-2,.7f,false),1.1f);
 puts("PASS: Q idle entry, eight travel headings, hold/release aim, stale aim, unlocked parity, action/melee bypass and reset/noise guards");
}
