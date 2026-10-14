#include "vehicle_physics.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
using namespace vehicle;

// Analytic planes and solid boxes exercise real sphere contacts, including the
// rounded edge of a low rock. The old fixture only swept an infinite wall and
// could not expose terrain contacts stopping the car before its wheel rays.
struct Box {V lo,hi;};
struct Scene {
 float slope=0,crossSlope=0,edge=10000;
 std::vector<Box> boxes;
 int rays=0,sweeps=0,maxRays=0,maxSweeps=0,failRay=0,failSweep=0;
 bool malformed=false,missingCorner=false;
};
void check(bool pass,const char*n){std::printf("%s %s\n",pass?"PASS":"FAIL",n);if(!pass)std::exit(1);}
float component(V v,int i){return i==0?v.x:i==1?v.y:v.z;}
V normalAxis(int axis,float sign){return axis==0?V{sign,0,0}:axis==1?V{0,sign,0}:V{0,0,sign};}
bool rayBox(V a,V b,const Box&box,Hit&hit){
 float first=0,last=1;V normal{};
 for(int axis=0;axis<3;axis++){
  float start=component(a,axis),delta=component(b-a,axis),lo=component(box.lo,axis),hi=component(box.hi,axis);
  if(std::fabs(delta)<1e-8f){if(start<lo||start>hi)return false;continue;}
  float enter=(lo-start)/delta,leave=(hi-start)/delta;V n=normalAxis(axis,-1);
  if(enter>leave){std::swap(enter,leave);n=normalAxis(axis,1);}
  if(enter>=first){first=enter;normal=n;}last=std::min(last,leave);if(first>last)return false;
 }
 hit={a+(b-a)*first,normal,first};return first>=0&&first<=1;
}
V nearest(V p,const Box&b){return{clamp(p.x,b.lo.x,b.hi.x),clamp(p.y,b.lo.y,b.hi.y),clamp(p.z,b.lo.z,b.hi.z)};}
bool sphereBox(V a,V b,float radius,const Box&box,Hit&hit){
 V delta=b-a;float distance=length(delta);if(distance<1e-8f)return false;
 float t=0;
 // Distance to a convex box is 1-Lipschitz. Conservative advancement cannot
 // skip the first contact; unlike an expanded AABB this preserves round edges.
 for(int i=0;i<160&&t<=1.f;i++){
  V p=a+delta*t,d=p-nearest(p,box);float n=length(d),gap=n-radius;
  if(gap<=.00001f){V normal=n>.00001f?d*(1/n):unit(delta)*-1;hit={p,normal,t};return true;}
  t+=std::max(gap/distance,.000001f);
 }
 return false;
}
int ray(void*c,V a,V b,Hit&h){
 auto&s=*(Scene*)c;if(++s.rays>4||s.rays==s.failRay)return -1;
 bool found=false;h.fraction=1;
 float den=b.y-a.y-s.slope*(b.z-a.z)-s.crossSlope*(b.x-a.x);
 float t=(s.slope*a.z+s.crossSlope*a.x-a.y)/den;
 if(t>=0&&t<=1&&a.z<s.edge&&!(s.missingCorner&&a.x>0&&a.z>0)){
  h={a+(b-a)*t,unit({-s.crossSlope,1,-s.slope}),t};found=true;
 }
 for(const auto&box:s.boxes){Hit hit;if(rayBox(a,b,box,hit)&&(!found||hit.fraction<h.fraction)){h=hit;found=true;}}
 if(found&&s.malformed)h.normal.y=std::numeric_limits<float>::quiet_NaN();
 return found?1:0;
}
int sweep(void*c,V a,V b,float radius,Hit&h){
 auto&s=*(Scene*)c;if(++s.sweeps>6||s.sweeps==s.failSweep)return -1;
 if(length(b-a)<=.0001f)return -1; // Match the native segment guard.
 bool found=false;h.fraction=1;V normal=unit({-s.crossSlope,1,-s.slope});
 float first=dot(a,normal)-radius,last=dot(b,normal)-radius;
 if(last<first&&last<=0){float t=first<=0?0:first/(first-last);V at=a+(b-a)*t;
  if(at.z<s.edge){h={at,normal,t};found=true;}
 }
 for(const auto&box:s.boxes){Hit hit;if(sphereBox(a,b,radius,box,hit)&&(!found||hit.fraction<h.fraction)){h=hit;found=true;}}
 if(found&&s.malformed)h.fraction=std::numeric_limits<float>::quiet_NaN();
 return found?1:0;
}
bool tick(Scene&scene,State&s,Input input={},float dt=1/60.f){
 scene.rays=scene.sweeps=0;bool okay=step(s,input,dt,{&scene,ray,sweep});
 scene.maxRays=std::max(scene.maxRays,scene.rays);scene.maxSweeps=std::max(scene.maxSweeps,scene.sweeps);return okay;
}
bool clearOfBoxes(const Scene&scene,const State&s){
 for(const auto&box:scene.boxes)for(int row=0;row<3;row++)for(int side=0;side<2;side++){
  V p=point(s,{side?.45f:-.45f,.28f,(row-1)*1.85f});
  if(length(p-nearest(p,box))<.599f)return false;
 }
 return true;
}
void run(Scene&scene,State&s,Input input,int frames){
 for(int i=0;i<frames;i++){
  if(!tick(scene,s,input))check(false,"valid scene tick");
  if(!clearOfBoxes(scene,s))check(false,"accepted pose remains outside solid boxes");
 }
}
State parked(){State s;s.p.y=.64f;return s;}
int main(){
 Scene scene;State s=parked();
 run(scene,s,{1,0,false},240);check(s.p.z>20&&s.speed>8,"acceleration");
 run(scene,s,{0,0,true},120);check(std::fabs(s.speed)<.001f,"brake");
 float z=s.p.z;run(scene,s,{-1,0,false},180);check(s.p.z<z-5&&s.speed<0,"reverse");

 scene={};scene.boxes.push_back({{-100,-10,12},{100,10,13}});s=parked();
 run(scene,s,{1,0,false},600);check(s.p.z<9.6f&&s.p.z>8,"wall sweep stops entire nose");
 z=s.p.z;run(scene,s,{-1,0,false},180);check(s.p.z<z-5,"reverse escapes wall contact");

 scene={};scene.slope=.35f;s=parked();
 run(scene,s,{1,0,false},300);check(s.p.z>12&&s.p.y>4&&s.pitch>.20f,"climbs moderate terrain with floor sphere contacts");

 scene={};scene.boxes.push_back({{-20,-1,6},{20,.50f,16}});s=parked();
 float peak=0;
 for(int i=0;i<480;i++){if(!tick(scene,s,{1,0,false}))check(false,"ledge query tick");peak=std::max(peak,s.p.y);}
 check(s.p.z>20,"crosses half metre ledge without nose trapping");
 check(peak<1.5f,"step response has bounded lift instead of persistent launch");
 check(s.p.y<.75f&&s.grounded,"settles after ledge descent");

 s=parked();s.p.z=3.65f;run(scene,s,{1,0,false},480);
 check(s.p.z>20,"half metre ledge can be climbed from a standing approach");
 scene={};scene.boxes.push_back({{-20,-1,-16},{20,.50f,-6}});s=parked();
 run(scene,s,{-1,0,false},600);check(s.p.z<-20,"reverse traverses low ledge");

 scene={};scene.boxes.push_back({{-.55f,-1,6},{.55f,.50f,6.7f}});s=parked();
 run(scene,s,{1,0,false},480);check(s.p.z>15,"crosses small central rock missed by wheel rays");

 scene={};scene.boxes.push_back({{3,-10,-100},{4,10,100}});s=parked();s.yaw=.4f;
 run(scene,s,{1,0,false},420);check(s.p.z>20&&s.p.x<2.05f,"glancing wall contact slides along wall");

 scene.boxes.push_back({{-100,-10,22},{100,10,23}});s=parked();s.yaw=.4f;
 run(scene,s,{1,0,false},600);check(s.p.z<19.8f&&s.p.x<2.05f,"corner retains both blocking walls");
 V before=s.p;run(scene,s,{-1,0,false},180);check(length(s.p-before)>4,"reverse escapes corner");

 scene={};scene.edge=8;s=parked();run(scene,s,{1,0,false},240);
 check(s.p.z>10&&!s.grounded&&s.p.y<-.5f,"unsupported cliff falls instead of using missing wheel heights");

 scene={};scene.slope=.20f;scene.crossSlope=.10f;scene.missingCorner=true;
 s=parked();s.pitch=std::atan(.20f);s.roll=std::atan(-.10f);run(scene,s,{},120);
 check(s.grounded&&std::fabs(s.p.y-.64f)<.01f,"three actual wheel contacts preserve their support plane");

 scene={};scene.boxes.push_back({{-20,-1,6},{20,1.2f,16}});s=parked();
 run(scene,s,{1,0,false},360);check(s.p.z<3.7f&&s.p.y<.8f,"tall ledge remains an obstacle rather than a climbable wall");

 for(float dt:{1/30.f,1/144.f}){
  scene={};scene.boxes.push_back({{-20,-1,6},{20,.50f,16}});s=parked();
  for(int i=0;i<int(8/dt);i++)if(!tick(scene,s,{1,0,false},dt))check(false,"variable frame terrain tick");
  check(s.p.z>20&&s.p.y<.75f,"low ledge remains traversable at 30 and 144 Hz");
 }

 scene={};s=parked();s.p.y=2;tick(scene,s);
 check(!s.grounded&&s.p.y>1.99f&&s.vertical<0,"airborne car falls without floor teleport");
 run(scene,s,{},120);check(s.grounded&&std::fabs(s.p.y-.64f)<.01f,"gravity settles on terrain");

 scene={};s=parked();scene.failRay=3;before=s.p;
 check(!tick(scene,s,{1,0,false})&&length(s.p-before)<.00001f,"ray failure freezes instead of tunneling");
 scene.failRay=0;scene.failSweep=5;before=s.p;
 check(!tick(scene,s,{1,0,false})&&length(s.p-before)<.00001f,"late sweep failure rolls back complete movement");
 scene.failSweep=0;scene.malformed=true;before=s.p;
 check(!tick(scene,s,{1,0,false})&&length(s.p-before)<.00001f,"malformed collision data fails closed");

 scene={};s=parked();run(scene,s,{1,1,false},240);check(s.yaw>.5f,"D turns right in ER camera coordinates");
 check(scene.maxRays<=4&&scene.maxSweeps<=6,"native query budget remains four rays and six sweeps");
 s=parked();s.pitch=.0001f;run(scene,s,{},60);
 check(std::fabs(s.pitch)<.00001f,"tiny settling rotations avoid rejected native sweep lengths");
 before=s.p;check(!tick(scene,s,{1,0,false},0)&&length(s.p-before)==0,"zero timestep cannot move car");
 check(!tick(scene,s,{1,0,false},.5f)&&length(s.p-before)==0,"long pause cannot teleport car");

 s=parked();V out;check(pushout(s,{0,0,0},out)&&std::fabs(out.x)>1.3f,"parked player collision");
 check(!pushout(s,{3,0,0},out),"outside player unaffected");
 V shift;check(rebase({16,-8,0},shift)&&shift.x==16&&shift.y==-8,"quantized origin shift preserved");
 check(!rebase({3,0,0},shift),"arbitrary warp rejected");
 check(zone(0x3c010200)==zone(0x3c020200),"overworld cell crossing preserves car");
 check(zone(0x10000000)!=zone(0x11000000),"dungeon transitions invalidate car");
 std::puts("VEHICLE CHECKS PASSED");
}
