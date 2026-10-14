#pragma once
#include <cmath>
#include <cstdint>
namespace ground_contact {
struct Cache {
 uintptr_t owner=0;uint64_t time=0;bool hit=false;float point[3]={},height=0;
 void reset(){*this={};}
 bool due(uintptr_t current,uint64_t now,bool force=false)const{return force||!owner||owner!=current||now<time||now-time>=80;}
 void store(uintptr_t current,uint64_t now,bool success,const float*position,float distance){
  owner=current;time=now;hit=success&&position&&std::isfinite(distance);height=distance;
  if(hit)for(unsigned i=0;i<3;i++){point[i]=position[i];if(!std::isfinite(point[i]))hit=false;}
 }
 bool valid(uintptr_t current,uint64_t now)const{return current&&current==owner&&hit&&now>=time&&now-time<200;}
};
}
