#pragma once
#include <cmath>
#include <algorithm>
namespace vehicle {
struct V {float x=0,y=0,z=0; V operator+(V b)const{return{x+b.x,y+b.y,z+b.z};}V operator-(V b)const{return{x-b.x,y-b.y,z-b.z};}V operator*(float s)const{return{x*s,y*s,z*s};}};
inline float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline float length(V a){return std::sqrt(dot(a,a));}
inline V unit(V a){float n=length(a);return n>.00001f?a*(1/n):V{};}
inline V cross(V a,V b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float clamp(float x,float lo,float hi){return std::max(lo,std::min(hi,x));}
inline bool finite(V a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
inline unsigned zone(unsigned block){unsigned area=block>>24;return area==60||area==61?area<<24:block;}
inline V stable(V p,unsigned block){unsigned area=block>>24;if(area==60||area==61){p.x+=256*((block>>16)&255);p.z+=256*((block>>8)&255);}return p;}
inline bool rebase(V delta,V&shift){shift={std::round(delta.x/8)*8,std::round(delta.y/8)*8,std::round(delta.z/8)*8};return length(delta-shift)<.20f;}
struct Hit{V point,normal;float fraction=1;};
struct Query {void*ctx=nullptr;int(*ray)(void*,V,V,Hit&)=nullptr;int(*sweep)(void*,V,V,float,Hit&)=nullptr;};
struct Input {float throttle=0,steer=0;bool brake=false;};
struct Contact {V normal;float remaining=0,rise=0;};
struct State {V p;float yaw=0,speed=0,steer=0,pitch=0,roll=0,vertical=0,wheel=0;bool grounded=true;Contact contacts[2];};
inline void matrix(const State&s,float*m){
 V f={std::sin(s.yaw)*std::cos(s.pitch),std::sin(s.pitch),std::cos(s.yaw)*std::cos(s.pitch)};
 V right=unit(cross(f,{0,1,0})),up=unit(cross(right,f));
 V r=right*std::cos(s.roll)+up*std::sin(s.roll);up=unit(cross(r,f));
 for(int i=0;i<16;i++)m[i]=0;
 m[0]=r.x;m[1]=r.y;m[2]=r.z;m[4]=up.x;m[5]=up.y;m[6]=up.z;m[8]=-f.x;m[9]=-f.y;m[10]=-f.z;m[12]=s.p.x;m[13]=s.p.y;m[14]=s.p.z;m[15]=1;
}
inline V point(const State&s,V p){float m[16];matrix(s,m);return{m[0]*p.x+m[4]*p.y+m[8]*p.z+s.p.x,m[1]*p.x+m[5]*p.y+m[9]*p.z+s.p.y,m[2]*p.x+m[6]*p.y+m[10]*p.z+s.p.z};}
// Every accepted candidate, including a slide/climb, uses the same six hull
// sweeps. A contact changes the NEXT candidate: there is never an unchecked
// second move after exhausting the native six-sweep budget.
inline bool validHit(const Hit&h){
 float n=dot(h.normal,h.normal);
 return finite(h.point)&&finite(h.normal)&&std::isfinite(h.fraction)&&h.fraction>=0&&h.fraction<=1&&n>.64f&&n<1.44f;
}
inline void remember(State&s,Contact contact){
 for(auto&c:s.contacts)if(c.remaining>0&&dot(c.normal,contact.normal)>.90f){c=contact;return;}
 s.contacts[s.contacts[0].remaining<s.contacts[1].remaining?0:1]=contact;
}
inline bool step(State&s,Input in,float dt,Query q){
 auto stop=[&](){s.speed=s.vertical=0;return false;};
 if(!finite(s.p)||!std::isfinite(s.yaw)||!std::isfinite(s.pitch)||!std::isfinite(s.roll)||
    !std::isfinite(s.speed)||!std::isfinite(s.vertical)||!std::isfinite(s.steer)||
    !std::isfinite(in.throttle)||!std::isfinite(in.steer)||!std::isfinite(dt)||dt<=0||dt>.15f||!q.ray||!q.sweep)return stop();
 dt=std::min(dt,.05f);State next=s;float throttle=clamp(in.throttle,-1,1);
 for(auto&c:next.contacts){c.remaining=std::max(0.f,c.remaining-dt);if(c.normal.y>=.50f&&c.rise<=.01f)c.remaining=0;}
 float brake=in.brake?16.f:(throttle*s.speed<-.05f?12.f:0.f);
 float accel=throttle*5.5f-s.speed*.12f-s.speed*std::fabs(s.speed)*.009f;
 if(brake>0)next.speed=std::copysign(std::max(0.f,std::fabs(s.speed)-brake*dt),s.speed);
 else next.speed=clamp(s.speed+accel*dt,-5.f,16.f);
 if(std::fabs(throttle)<.01f&&!in.brake){float v=std::fabs(next.speed);next.speed=std::copysign(std::max(0.f,v-.65f*dt),next.speed);}
 float target=clamp(in.steer,-1,1)*(.52f/(1.f+std::fabs(next.speed)*.065f));next.steer+=(target-next.steer)*(1-std::exp(-dt*10));
 next.yaw+=next.speed*std::tan(next.steer)/3.04f*dt;
 V forward={std::sin(next.yaw),0,std::cos(next.yaw)};next.p=next.p+forward*(next.speed*dt);

 // Fit only actual support points. Missing wheels must not invent old-height
 // ground, and three contacts must recover the same plane as four contacts.
 V support[4];unsigned supports=0;V mean{};
 const V wheel[4]={{-.785f,0,-1.596f},{.785f,0,-1.596f},{-.785f,0,1.443f},{.785f,0,1.443f}};
 for(int i=0;i<4;i++){
  V at=point(next,wheel[i]);Hit h;
  int code=q.ray(q.ctx,{at.x,s.p.y+1.1f,at.z},{at.x,s.p.y-2.5f,at.z},h);
  if(code<0||(code>0&&!validHit(h)))return stop();
  if(code>0&&h.normal.y>=.72f){support[supports++]=h.point;mean=mean+h.point;}
 }
 float ground=0,pitch=s.pitch,roll=s.roll;bool plane=false;
 if(supports>=3){
  mean=mean*(1.f/supports);float xx=0,xz=0,zz=0,xy=0,zy=0;
  for(unsigned i=0;i<supports;i++){V d=support[i]-mean;xx+=d.x*d.x;xz+=d.x*d.z;zz+=d.z*d.z;xy+=d.x*d.y;zy+=d.z*d.y;}
  float determinant=xx*zz-xz*xz;
  if(determinant>.0001f){
   float dx=(xy*zz-zy*xz)/determinant,dz=(zy*xx-xy*xz)/determinant;
   plane=dx*dx+dz*dz<=.93f; // approximately 44 degrees, including cross slope.
   ground=mean.y+dx*(next.p.x-mean.x)+dz*(next.p.z-mean.z)+.64f;
   pitch=std::atan(dx*forward.x+dz*forward.z);
   roll=std::atan(dx*-forward.z+dz*forward.x);
  }
 }
 next.vertical-=9.81f*dt;float fallingY=s.p.y+next.vertical*dt;
 float followDown=s.grounded?.10f+std::fabs(next.speed)*dt*.8f:.06f;
 next.grounded=plane&&ground>=fallingY-followDown&&ground-s.p.y<=.55f;
 if(next.grounded){
  next.vertical=0;next.p.y=ground;
  next.pitch+=(pitch-next.pitch)*(1-std::exp(-dt*10));next.roll+=(roll-next.roll)*(1-std::exp(-dt*10));
 }else next.p.y=fallingY;

 // Preserve movement away from an obstruction (particularly reverse). Upward
 // rock-edge normals can lift the chassis until wheel support catches up;
 // vertical walls only remove the motion into the wall. Two planes retain
 // inside corners. All resulting movement still needs the sweeps below.
 V motion=next.p-s.p;
 for(const auto&c:next.contacts)if(c.remaining>0&&c.normal.y>=.50f&&
     motion.x*c.normal.x+motion.z*c.normal.z<-.000001f)motion.y=std::max(0.f,motion.y);
 // Pitch/roll settling also moves the hull. Include those endpoint deltas
 // in contact projection so the rear cannot catch on a ledge while levelling.
 V rotation[6];State rotated=next;rotated.p=s.p;
 for(int row=0;row<3;row++)for(int side=0;side<2;side++){
  V local={side?.45f:-.45f,.28f,(row-1)*1.85f};
  rotation[row*2+side]=point(rotated,local)-point(s,local);
 }
 for(int pass=0;pass<3;pass++)for(const auto&c:next.contacts)if(c.remaining>0){
  float into=0;for(V delta:rotation)into=std::min(into,dot(motion+delta,c.normal));
  if(into<0)motion=motion-c.normal*into;
 }
 next.p=s.p+motion;
 float horizontal=std::sqrt(motion.x*motion.x+motion.z*motion.z);
 if(horizontal<std::fabs(next.speed)*dt*.05f)next.speed=0;

 float allowed=1.f;bool terrainContact=false;Contact contacts[6];unsigned contactCount=0;
 for(int row=0;row<3;row++)for(int side=0;side<2;side++){
  V local={side?.45f:-.45f,.28f,(row-1)*1.85f};V a=point(s,local),b=point(next,local);float distance=length(b-a);
  // The native segment guard requires distance > .0001 m.
  if(distance<=.00012f)continue;
  Hit h;int code=q.sweep(q.ctx,a,b,.60f,h);
  if(code<0||(code>0&&!validHit(h)))return stop();
  if(code>0&&dot(b-a,h.normal)<-.000001f){
   allowed=std::min(allowed,std::max(0.f,h.fraction-.012f/distance));
   V n=unit(h.normal);
   // A sphere's returned position is its centre at impact; recover the
   // contacted surface height. A high/steep face must never become a ramp.
   float surfaceY=h.point.y-.60f*n.y;
   bool climb=n.y>=.50f&&surfaceY<=s.p.y-.64f+.60f;
   if(!climb&&n.y<.72f)n=unit({n.x,0,n.z});
   terrainContact|=climb;contacts[contactCount++]={n,.18f,surfaceY+.64f-s.p.y};
  }
 }
 if(allowed<1){
  next.p=s.p+(next.p-s.p)*allowed;next.yaw=s.yaw+(next.yaw-s.yaw)*allowed;
  next.pitch=s.pitch+(next.pitch-s.pitch)*allowed;next.roll=s.roll+(next.roll-s.roll)*allowed;
  // Small terrain contacts retain momentum for the checked climb next tick.
  // A direct wall impact stops; glancing motion is recovered by its plane.
  if(!terrainContact){
   V tangent={motion.x,0,motion.z};
   for(int pass=0;pass<3;pass++)for(unsigned i=0;i<contactCount;i++){
    V n=unit({contacts[i].normal.x,0,contacts[i].normal.z});float into=dot(tangent,n);
    if(into<0)tangent=tangent-n*into;
   }
   float retained=horizontal>.00001f?length(tangent)/horizontal:0;
   next.speed*=std::max(allowed,clamp(retained,0,1));
  }
  next.vertical=0;
 }
 for(unsigned i=0;i<contactCount;i++)remember(next,contacts[i]);
 // Relative headroom survives the game's quantized world-origin rebases.
 for(auto&c:next.contacts)c.rise-=next.p.y-s.p.y;
 next.wheel+=length(next.p-s.p)*std::copysign(1.f,s.speed+next.speed)/.393f;
 if(!finite(next.p)||!std::isfinite(next.yaw)||std::fabs(next.p.y-s.p.y)>2)return stop();
 s=next;return true;
}
// On-foot player capsule against the parked car footprint. Return the nearest
// exterior point; caller checks its world clearance before moving the proxy.
inline bool pushout(const State&s,V p,V&out){
 if(p.y>s.p.y+1.25f||p.y+1.7f<s.p.y-.55f)return false;
 State flat=s;flat.pitch=flat.roll=0;float m[16];matrix(flat,m);V d=p-s.p;float x=d.x*m[0]+d.z*m[2],z=d.x*m[8]+d.z*m[10];
 if(std::fabs(x)>=1.36f||std::fabs(z)>=2.94f)return false;
 if(1.36f-std::fabs(x)<2.94f-std::fabs(z))x=std::copysign(1.38f,x);else z=std::copysign(2.96f,z);
 out=point(flat,{x,p.y-s.p.y,z});return true;
}
}
