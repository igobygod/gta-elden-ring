#include "../../src/projectile_policy.h"
#include <cassert>
#include <cstdio>
using namespace projectile_policy;
bool floorCast(V a,V b,V&out,bool&hit){hit=a.y>=0&&b.y<=0;if(hit){float t=a.y/(a.y-b.y);out=add(a,mul(sub(b,a),t));}return true;}
int main(){
 for(V target: {V{0,0,10},V{10,3,30},V{-20,-5,0},V{0,15,10},V{35,0,35}}){V v;assert(launch_velocity({0,1.3f,0},target,25,9.81f,v));float horizontal=std::hypot(v.x,v.z),time=std::hypot(target.x,target.z)/horizontal;V landing=add({0,1.3f,0},mul(v,time));landing.y-=.5f*9.81f*time*time;assert(length(sub(landing,target))<.001f);assert(std::abs(length(v)-25)<.001f);}
 V v;assert(!launch_velocity({0,0,0},{0,0,100},25,9.81f,v));assert(!launch_velocity({0,0,0},{0,40,0},25,9.81f,v));
 assert(launch_velocity({0,1,0},{0,0,85},400,.1962f,v));float t=85/v.z;assert(std::abs(1+v.y*t-.5f*.1962f*t*t)<.0001f);
 for(unsigned shot=0;shot<17;shot++){V sum={};for(unsigned i=0;i<8;i++){V a=pellet({0,0,1},{1,0,0},{0,1,0},i,shot);sum=add(sum,a);assert(a.z>=std::cos(3.001f*3.14159265f/180.f));}assert(std::abs(sum.x)<1e-6f&&std::abs(sum.y)<1e-6f);}
 V n=contact_normal({0,0,0},{2,-4,3},{0,1,0},floorCast);assert(n.y>.999f);
 V vertical;assert(launch_velocity({0,0,0},{0,2,0},25,9.81f,vertical)&&vertical.y==25);assert(launch_velocity({0,0,0},{0,-2,0},25,9.81f,vertical)&&vertical.y==-25);
 V wall=bounce({-5,2,7},{1,0,0});assert(wall.x>0&&wall.y>0&&wall.z>0);
 auto slopeCast=[](V a,V b,V&out,bool&hit){float da=a.y-.4f*a.x,db=b.y-.4f*b.x;hit=da>=0&&db<=0;if(hit)out=add(a,mul(sub(b,a),da/(da-db)));return true;};V slope=contact_normal({0,0,0},{3,-6,2},{0,1,0},slopeCast);assert(dot(slope,norm({-.4f,1,0}))>.999f);
 V reflected=bounce({8,-4,3},n);assert(reflected.x>0&&reflected.z>0&&reflected.y>0);assert(length(reflected)<length(V{8,-4,3}));
 V p={0,1.3f,0};v={0,3,17};int bounces=0;bool resting=false;constexpr float dt=1.f/120;
 for(int i=0;i<480;i++){if(resting)break;v.y-=grenade_gravity*dt;V end=add(p,mul(v,dt));bool hit;V contact;floorCast(p,end,contact,hit);if(hit){v=bounce(v,{0,1,0});p=add(contact,{0,grenade_radius,0});bounces++;resting=rest(v,{0,1,0});}else p=end;}
 assert(resting);assert(bounces<=7);assert(p.z<25);assert(p.y==grenade_radius);
 //400m/s traverses20m at20fps. A0.04m obstacle must still detonate exactly
 // once, at the observed front surface, with no late/expiry explosion.
 for(float frame:{.05f,.1f,.016f}){V p={0,1,0},v={0,0,400};bool active=true;unsigned contacts=0;float impact=0;
  auto thinWall=[](V a,V b,V&hit,bool&t){t=a.z<3.7f&&b.z>=3.7f;if(t)hit=add(a,mul(sub(b,a),(3.7f-a.z)/(b.z-a.z)));return true;};
  for(int i=0;i<20;i++)if(active){auto r=advance_swept(p,v,rpg_gravity,frame,thinWall);if(r==SweepResult::contact){active=false;contacts++;impact=p.z;}}
  assert(!active&&contacts==1&&std::abs(impact-3.7f)<.0001f);
 }
 // GL constant-acceleration sweep agrees with the low-arc solver, and absence
 // of a collision never turns a mere lifespan timeout into an air explosion.
 V target={13,2,24};p={0,1,0};assert(launch_velocity(p,target,25,9.81f,v));t=std::hypot(target.x,target.z)/std::hypot(v.x,v.z);
 auto clear=[](V,V,V&,bool&hit){hit=false;return true;};assert(advance_swept(p,v,9.81f,t,clear)==SweepResult::clear);assert(length(sub(p,target))<.0001f);
 auto missing=[](V,V,V&,bool&){return false;};assert(advance_swept(p,v,9.81f,.05f,missing)==SweepResult::unavailable);
 std::printf("PASS: ballistic/vertical/unreachable targets, RPG arc,136 centered pellets, slope normals, grenade rests after%d contacts, continuous launcher contact at10/20/60fps, one contact only, missing-world failure\n",bounces);
}
