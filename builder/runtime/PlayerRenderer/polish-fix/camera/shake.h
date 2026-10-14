#pragma once
#include <cmath>
#include <cstdint>
namespace camera_shake {
struct Impulse{uintptr_t owner=0;uint64_t time=0;float strength=0,duration=0;bool blast=false;};
struct Motion{float pitch=0,roll=0;};
struct Queue{
 Impulse impulses[8];unsigned next=0;
 void clear(){for(auto&i:impulses)i={};next=0;}
 bool add(uintptr_t owner,uint64_t time,float strength,float duration,bool blast){if(!owner||!std::isfinite(strength)||!std::isfinite(duration)||strength<=0||duration<=0)return false;impulses[next++%8]={owner,time,std::fmin(strength,.03f),std::fmin(duration,1.f),blast};return true;}
 Motion sample(uintptr_t owner,uint64_t now){Motion out;for(auto&i:impulses){if(!i.owner)continue;if(i.owner!=owner||now<i.time||float(now-i.time)*.001f>=i.duration){i={};continue;}float age=float(now-i.time)*.001f,u=age/i.duration,e=std::exp(-u*5.f);out.pitch+=i.strength*e*(i.blast?std::sin(age*110.f):1.f);out.roll+=i.blast?i.strength*e*std::sin(age*137.f):0;}out.pitch=std::fmax(-.035f,std::fmin(.035f,out.pitch));out.roll=std::fmax(-.025f,std::fmin(.025f,out.roll));return out;}
};
}
