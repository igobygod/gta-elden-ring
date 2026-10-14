#include <initializer_list>

#include "../../src/directional_motion.h"
#include <cassert>
#include <cstdio>
using namespace donor_motion;
int main(){float w[4]={0,1,0,0};SignedPhase p;p.reset(.2f);
 assert(std::fabs(p.advance(0,2,w,true)-.2f)<1e-6f);
 float step=increment(.01f,2,w);assert(std::fabs(p.advance(.01f,2,w,true)-(.2f-step))<1e-6f);
 assert(std::fabs(p.advance(.01f,2,w,false)-.2f)<1e-6f);
 p.reset(.001f);float back=p.advance(.01f,2,w,true);assert(back>.99f&&back<1);
 DirectionFilter f;
 auto input=[&](float degrees){float r=degrees*3.14159265358979323846f/180;return f.update(-std::sin(r),std::cos(r),0,true,.016f);};
 input(80);for(float a:{89.f,91.f,87.f,93.f,90.f})assert(!input(a).reverseLower);
 assert(input(105).reverseLower);for(float a:{89.f,91.f,87.f,93.f,90.f})assert(input(a).reverseLower);
 assert(!input(75).reverseLower);
 auto still=f.update(0,0,0,true,.016f);assert(still.reverseLower==f.reverse);
 float before=f.pelvisYaw;auto paused=f.update(0,0,0,true,.016f);assert(std::fabs(wrap(paused.pelvisYaw-before))<1e-6f);
 assert(increment(-1,2,w)==0&&increment(.1f,-1,w)==0&&increment(2,2,w)==0);
 assert(std::fabs(wrap(-3.13f-3.13f))<.03f);
 std::puts("PASS reverse phase continuity, signed increments, loop seam, side hysteresis, stationary heading retention, invalid timing");}
