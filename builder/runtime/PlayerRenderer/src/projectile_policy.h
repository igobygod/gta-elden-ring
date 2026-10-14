#pragma once
#include <cmath>
#include <algorithm>
namespace projectile_policy {
struct V {float x,y,z;};
inline V add(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V sub(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V mul(V a,float n){return {a.x*n,a.y*n,a.z*n};}
inline float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float length(V a){return std::sqrt(dot(a,a));}
inline V norm(V a){float n=length(a);return n>1e-6f?mul(a,1.f/n):V{0,1,0};}
constexpr float grenade_speed=18.f,grenade_gravity=15.f,grenade_radius=.06f;
constexpr float grenade_restitution=.18f,grenade_friction=.58f;
constexpr float launcher_speed=25.f,launcher_gravity=9.81f;
constexpr float rpg_speed=400.f,rpg_gravity=.1962f;
enum class SweepResult {clear,contact,unavailable};
// Analytic constant-gravity integration and a continuous contact segment. Even
// a400m/s rocket at20fps cannot skip a surface between sampled positions.
template<class Cast> inline SweepResult advance_swept(V&position,V&velocity,float gravity,float dt,Cast&&cast){
 if(!(dt>0)||!std::isfinite(dt))return SweepResult::unavailable;
 V end=add(position,mul(velocity,dt));end.y-=.5f*gravity*dt*dt;V hit={};bool touched=false;
 if(!cast(position,end,hit,touched))return SweepResult::unavailable;
 velocity.y-=gravity*dt;position=touched?hit:end;
 return touched?SweepResult::contact:SweepResult::clear;
}
// Same world units and constant acceleration as the staged native Bullet rows.
// Low arc only; an unreachable target is reported, never snapped to an enemy.
inline bool launch_velocity(V from,V target,float speed,float gravity,V& velocity){
 V d=sub(target,from);double x=std::hypot(double(d.x),double(d.z)),y=d.y,v2=double(speed)*speed,g=gravity;
 if(!(speed>0)||!(gravity>=0)||!std::isfinite(x)||!std::isfinite(y))return false;
 if(g<1e-5){if(length(d)<1e-5)return false;velocity=mul(norm(d),speed);return true;}
 if(x<1e-5){if(y>v2/(2*g))return false;velocity={0,y>=0?speed:-speed,0};return std::abs(y)>1e-5;}
 double disc=v2*v2-g*(g*x*x+2*y*v2);if(disc<0)return false;
 double tanAngle=(g*x*x+2*y*v2)/(x*(v2+std::sqrt(disc)));
 double horizontal=speed/std::sqrt(1+tanAngle*tanAngle);
 velocity={float(d.x/x*horizontal),float(horizontal*tanAngle),float(d.z/x*horizontal)};
 return std::isfinite(velocity.x)&&std::isfinite(velocity.y)&&std::isfinite(velocity.z);
}
// Eight paired rays keep a centered shotgun pattern. The native shootAngle
// field is a heading offset, not cone width; spread is therefore explicit.
inline V pellet(V forward,V right,V up,unsigned index,unsigned shot){
 constexpr float offsets[8][2]={{1,0},{-1,0},{0,1},{0,-1},{.53f,.53f},{-.53f,-.53f},{.53f,-.53f},{-.53f,.53f}};
 float angle=float(shot%17)*.3695991357f,c=std::cos(angle),s=std::sin(angle),x=offsets[index%8][0],y=offsets[index%8][1];
 float radius=.0524077793f;return norm(add(forward,add(mul(right,(x*c-y*s)*radius),mul(up,(x*s+y*c)*radius))));
}
// Inelastic bounce against an observed surface plane. Tangential velocity is
// damped, never reversed by the slope/edge fallback used by the previous build.
inline V bounce(V velocity,V normal){
 normal=norm(normal);float vn=dot(velocity,normal);if(vn>=0)return velocity;
 V tangent=sub(velocity,mul(normal,vn));return sub(mul(tangent,grenade_friction),mul(normal,vn*grenade_restitution));
}
inline bool rest(V velocity,V normal){return normal.y>.55f&&length(velocity)<.9f;}
// Probe two neighbouring contacts around an already-confirmed hit. Their plane
// gives the missing CastRay normal. Failed/edge probes leave the supplied fallback.
template<class Cast> inline V contact_normal(V hit,V incoming,V fallback,Cast&& cast){
 incoming=norm(incoming);V tangent=norm(cross(incoming,std::abs(incoming.y)>.9f?V{1,0,0}:V{0,1,0}));V other=norm(cross(incoming,tangent));
 V p[2];V axes[2]={tangent,other};for(int i=0;i<2;i++){
  V shifted=add(hit,mul(axes[i],.06f));bool touched=false;
  if(!cast(sub(shifted,mul(incoming,.24f)),add(shifted,mul(incoming,.24f)),p[i],touched)||!touched)return norm(fallback);
 }
 V normal=cross(sub(p[0],hit),sub(p[1],hit));if(length(normal)<.0001f)return norm(fallback);
 normal=norm(normal);if(dot(normal,incoming)>0)normal=mul(normal,-1);return normal;
}
}
