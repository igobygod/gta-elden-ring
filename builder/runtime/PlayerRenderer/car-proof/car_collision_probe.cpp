#include "car_collision_probe.h"
#include "car_probe_signatures.h"
#include <windows.h>
#include <cmath>
#include <cstring>

namespace {
bool read(const void*p,size_t length){
    if(!p||!length)return false;uintptr_t at=(uintptr_t)p,end=at+length;if(end<at)return false;
    while(at<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery((void*)at,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        DWORD types=PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
        if(!(m.Protect&types))return false;uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=at)return false;at=next;}
    return true;
}
bool finite(const float*p){return p&&std::isfinite(p[0])&&std::isfinite(p[1])&&std::isfinite(p[2])&&std::fabs(p[0])<100000&&std::fabs(p[1])<100000&&std::fabs(p[2])<100000;}
bool segment(const float*a,const float*b){if(!finite(a)||!finite(b))return false;float n=0;for(int i=0;i<3;i++){float d=b[i]-a[i];n+=d*d;}return n>.00000001f&&n<40000.f;}
bool identities(CarNativeProbe*p){
    if(!p||!p->verified||!p->owner||!read((void*)p->owner,0x1C6)||*(uintptr_t*)p->owner!=p->base+0x2A7FBB0||(*(unsigned char*)(p->owner+0x1C5)&0x80))return false;
    auto slot=(void**)(p->base+0x3D7A0D0);if(!read(slot,8)||!read(*slot,0xA0))return false;
    auto world=*(unsigned char**)((unsigned char*)*slot+0x98);
    if(!read(world,16)||*(uintptr_t*)world!=p->base+0x2B96978)return false;
    auto havok=*(void**)(world+8);if(!read(havok,8)||*(uintptr_t*)havok!=p->base+0x2EF0DD8)return false;p->world=(uintptr_t)world;return true;
}
double clock(){LARGE_INTEGER value,frequency;QueryPerformanceCounter(&value);QueryPerformanceFrequency(&frequency);return double(value.QuadPart)/double(frequency.QuadPart);}
bool output(const float*from,const float*to,const float*position,const float*normal,float fraction,CarProbeHit*out){
    if(!finite(position)||!finite(normal)||!std::isfinite(fraction)||fraction<-.0001f||fraction>1.0001f)return false;
    float n=0,error=0;for(int i=0;i<3;i++){n+=normal[i]*normal[i];float e=from[i]+fraction*(to[i]-from[i])-position[i];error+=e*e;}
    if(n<.64f||n>1.44f||error>.0025f)return false;
    float length=std::sqrt(n);std::memcpy(out->position,position,12);for(int i=0;i<3;i++)out->normal[i]=normal[i]/length;out->fraction=std::fmax(0.f,std::fmin(1.f,fraction));return true;
}
int guard(CarNativeProbe*p,bool sphere){
    if(!p||!p->verified)return CAR_PROBE_GUARD_FAILED;if(GetCurrentThreadId()!=p->game_thread)return CAR_PROBE_WRONG_THREAD;
    if(!(p->enabled&(sphere?2u:1u)))return CAR_PROBE_DISABLED;
    if(p->elapsed_ms>=4.f||(sphere?p->sphere_count>=p->sphere_limit:p->ray_count>=p->ray_limit))return CAR_PROBE_BUDGET;
    return identities(p)?CAR_PROBE_HIT:CAR_PROBE_GUARD_FAILED;
}
}
extern "C" int car_native_probe_init(CarNativeProbe*p,uintptr_t base,uint32_t thread,int approved){
    if(!p)return CAR_PROBE_INVALID;*p={};p->base=base;p->game_thread=thread;
    if(!approved||!base||!thread)return CAR_PROBE_GUARD_FAILED;
    for(const auto&site:car_probe_signatures)if(!read((void*)(base+site.rva),site.size)||std::memcmp((void*)(base+site.rva),site.bytes,site.size))return CAR_PROBE_GUARD_FAILED;
    p->verified=1;return CAR_PROBE_HIT;
}
extern "C" void car_native_probe_arm(CarNativeProbe*p,int ray,int sphere){if(p)p->enabled=(ray?1u:0u)|(sphere?2u:0u);}
extern "C" int car_native_probe_begin_frame(CarNativeProbe*p,uintptr_t owner,int valid,uint32_t rays,uint32_t spheres){
    if(!p)return CAR_PROBE_INVALID;p->world=0;p->owner=0;p->ray_count=p->sphere_count=0;p->elapsed_ms=0;
    if(!valid||!p->verified)return CAR_PROBE_GUARD_FAILED;if(GetCurrentThreadId()!=p->game_thread)return CAR_PROBE_WRONG_THREAD;
    p->owner=owner;p->frame=GetTickCount64();p->ray_limit=rays<4?rays:4;p->sphere_limit=spheres<6?spheres:6;return identities(p)?CAR_PROBE_HIT:CAR_PROBE_GUARD_FAILED;
}
extern "C" int car_native_probe_ray(CarNativeProbe*p,const float*from,const float*to,CarProbeHit*out){
    if(out)*out={};if(!out||!segment(from,to))return CAR_PROBE_INVALID;int ready=guard(p,false);if(ready<0)return ready;
    alignas(16)float a[4]={from[0],from[1],from[2],1},d[4]={to[0]-from[0],to[1]-from[1],to[2]-from[2],0},position[4]={},normal[4]={};float fraction=0;
    using Ray=bool(__fastcall*)(void*,uint32_t,const float*,const float*,float*,float*,float*);
    double start=clock();bool hit=((Ray)(p->base+0xC71AA0))((void*)p->world,0x5D,a,d,position,normal,&fraction);++p->ray_count;
    out->elapsed_ms=float((clock()-start)*1000);p->elapsed_ms+=out->elapsed_ms;
    if(!hit)return CAR_PROBE_MISS;return output(from,to,position,normal,fraction,out)?CAR_PROBE_HIT:CAR_PROBE_INVALID;
}
extern "C" int car_native_probe_sphere(CarNativeProbe*p,const float*from,const float*to,float radius,CarProbeHit*out){
    if(out)*out={};if(!out||!segment(from,to)||!std::isfinite(radius)||radius<.01f||radius>1.f)return CAR_PROBE_INVALID;int ready=guard(p,true);if(ready<0)return ready;
    alignas(16)float a[4]={from[0],from[1],from[2],1},d[4]={to[0]-from[0],to[1]-from[1],to[2]-from[2],0},position[4]={},normal[4]={};float fraction=0;
    using Sphere=bool(__fastcall*)(void*,uint32_t,const float*,const float*,float,float*,float*,float*);
    double start=clock();bool hit=((Sphere)(p->base+0xC72980))((void*)p->world,0x63,a,d,radius,position,normal,&fraction);++p->sphere_count;
    out->elapsed_ms=float((clock()-start)*1000);p->elapsed_ms+=out->elapsed_ms;
    if(!hit)return CAR_PROBE_MISS;return output(from,to,position,normal,fraction,out)?CAR_PROBE_HIT:CAR_PROBE_INVALID;
}
