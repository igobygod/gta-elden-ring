#include <cmath>
#include <cstring>
#include <cassert>
#include <cstdio>
#include "../../src/arsenal_wheel_policy.h"
using std::isfinite;bool lock_layout=true,lock_target_layout=true;struct Player{unsigned char*ins;unsigned char*phys;}current;float dist(const float*a,const float*b){float d=0;for(int i=0;i<3;i++)d+=(a[i]-b[i])*(a[i]-b[i]);return std::sqrt(d);}
bool locked_target(float*out){if(!lock_layout||!lock_target_layout||current.ins[0xC9]!=1)return false;float point[3];memcpy(point,current.ins+0xD0,12);for(float v:point)if(!isfinite(v))return false;const float*pos=(const float*)(current.phys+0x70);float range=dist(point,pos);if(!isfinite(range)||range<.2f||range>250.f)return false;memcpy(out,point,12);return true;}
int main(){unsigned char p[1024]={},phys[1024]={};current={p,phys};p[0xC9]=1;float pos[3]={3,2,1},target[3]={13,4,1},out[3]={};memcpy(phys+0x70,pos,12);memcpy(p+0xD0,target,12);assert(locked_target(out)&&out[0]==13);target[0]=300;memcpy(p+0xD0,target,12);assert(!locked_target(out));target[0]=NAN;memcpy(p+0xD0,target,12);assert(!locked_target(out));target[0]=13;memcpy(p+0xD0,target,12);p[0xC9]=0;assert(!locked_target(out));p[0xC9]=1;lock_target_layout=false;assert(!locked_target(out));
arsenal_wheel::LauncherScroll wheel;assert(wheel.update(60,true,6)==0);assert(wheel.update(60,true,6)==1);assert(wheel.update(-120,true,6)==0);assert(wheel.update(120,true,4)==0);assert(wheel.remainder==0);assert(wheel.update(120,false,6)==0);assert(wheel.update(120,true,6)==1);wheel.reset();assert(wheel.remainder==0);assert(wheel.update(32768,true,6)==1);puts("PASS production native-target bounds/signature/lock gates; launcher detents/sector/cancel/overflow");}
