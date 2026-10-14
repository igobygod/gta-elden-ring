#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../../src/shoulder_aim_policy.h"
#include "../../../NativePort/combat-source/native/input_policy.h"
static bool eq(float a,float b){return std::fabs(a-b)<.00001f;}
int main(){
 shoulder_aim::Offset o;o.add(100,-50,16.f/9,false);assert(eq(o.x,.3f)&&eq(o.y,.15f));
 // RMB/scope transitions keep the mouse-selected point; only lens limits clamp.
 float before[2]={o.x,o.y};o.bound(16.f/9,true);assert(eq(o.x,before[0])&&eq(o.y,before[1]));o.bound(16.f/9,false);assert(eq(o.x,before[0])&&eq(o.y,before[1]));
 for(int i=0;i<10000;i++)o.add(1000,1000,16.f/9,false);assert(eq(o.x,.78f*16.f/9)&&eq(o.y,-.70f));
 o.bound(16.f/9,true);assert(std::hypot(o.x,o.y)<=.70001f);
 o.reset();assert(o.x==0&&o.y==0);o.add(2147483647,-2147483647,1,true);assert(std::isfinite(o.x)&&std::isfinite(o.y)&&std::hypot(o.x,o.y)<=.70001f);
 o.bound(NAN,true);assert(o.x==0&&o.y==0);
 unsigned checks=0;
 for(float yaw:{0.f,.4f,-1.9f,3.14f})for(float fov:{.10471976f,.2443461f,.7330383f,.9599311f})for(float aspect:{1.f,16.f/9,2.4f})for(bool scoped:{false,true}){
  float view[16]={std::cos(yaw),0,-std::sin(yaw),0,0,1,0,0,std::sin(yaw),0,std::cos(yaw),0,5,2,-3,1},original[16];memcpy(original,view,sizeof(view));
  o.reset();o.add(111,-77,aspect,scoped);ShoulderAimSnapshot s={};assert(shoulder_aim::make(s,123,1000,1,view,fov,aspect,o,true,scoped));
  assert(!memcmp(view,original,sizeof(view))&&!memcmp(s.view,original,sizeof(view)));assert(s.ready&&s.locked&&s.free_aim&&s.scoped==scoped);
  float x=0,y=0,z=0,length=0;for(int i=0;i<3;i++){assert(s.origin[i]==view[12+i]);x+=s.direction[i]*view[i];y+=s.direction[i]*view[4+i];z+=s.direction[i]*view[8+i];length+=s.direction[i]*s.direction[i];}
  assert(eq(length,1));assert(eq(x/z/std::tan(fov*.5f)/aspect,s.ndc[0]));assert(eq(y/z/std::tan(fov*.5f),s.ndc[1]));
  if(scoped)assert(std::hypot(s.ndc[0]*aspect,s.ndc[1])<=.70001f);else assert(std::fabs(s.ndc[0])<=.78001f&&std::fabs(s.ndc[1])<=.70001f);
  uint64_t previous=0;assert(shoulder_aim::claim(s,123,1000,view,7,7,previous));assert(!shoulder_aim::claim(s,123,1000,view,7,7,previous));
  s.sequence=2;assert(!shoulder_aim::claim(s,123,1000,view,7,8,previous));assert(shoulder_aim::claim(s,123,1000,view,7,7,previous));
  s.sequence=1;assert(!shoulder_aim::claim(s,123,1000,view,7,7,previous));
  assert(!shoulder_aim::fresh(s,124,1000)&&!shoulder_aim::fresh(s,123,999)&&!shoulder_aim::fresh(s,123,1101));
  assert(shoulder_aim::fresh(s,123,1100));s.ready=0;assert(!shoulder_aim::fresh(s,123,1000));s.ready=1;
  view[12]+=.01f;assert(!shoulder_aim::matches(s,view));view[12]-=.01f;s.view[0]=NAN;assert(!shoulder_aim::matches(s,view));s.view[0]=view[0];view[0]=NAN;assert(!shoulder_aim::matches(s,view));
  assert(!shoulder_aim::make(s,123,1000,3,view,fov,aspect,o,true,scoped));checks++;
 }
 float view[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,2,-3,1};ShoulderAimSnapshot s={};o.add(300,-100,16.f/9,false);
 assert(shoulder_aim::make(s,123,1000,3,view,.7f,16.f/9,o,false,false));assert(s.ndc[0]==0&&s.ndc[1]==0&&s.direction[0]==0&&s.direction[1]==0&&s.direction[2]==1);
 // DirectInput state & buffered paths consume Q XY only. Zoom wheel survives.
 for(unsigned size:{16u,20u}){int32_t mouse[5]={15,-22,120,-1,-1};input_policy::mouse(mouse,size,{true,false,false,false,true});assert(mouse[0]==0&&mouse[1]==0&&mouse[2]==120);
  int32_t vanilla[5]={15,-22,120,-1,-1};input_policy::mouse(vanilla,size,{false,false,false,false,false});assert(vanilla[0]==15&&vanilla[1]==-22&&vanilla[2]==120);
  int32_t wheel[5]={15,-22,120,-1,-1};input_policy::mouse(wheel,size,{true,true,false,false,true});assert(wheel[0]==0&&wheel[1]==0&&wheel[2]==0);
 }
 for(unsigned axis:{0u,4u}){assert(input_policy::event(true,axis,55,{true,false,false,false,true})==0);assert(input_policy::event(true,axis,55,{true,false,false,false,false})==55);}
 assert(input_policy::event(true,8,120,{true,false,false,false,true})==120);
 assert(input_policy::event(true,8,120,{true,false,false,true,true})==0);
 puts("PASS Q aim: supplied matrix/origin preserved by aim policy; scope/RMB offset preservation; bounded mouse extremes; projection agrees with reticle across96 cases; owner/time/thread/matrix/single-frame claim guards; Q XY input isolation and wheel/normal input preservation");
 assert(checks==96);
}
