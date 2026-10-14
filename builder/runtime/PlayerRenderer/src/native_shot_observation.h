#pragma once
#include <cmath>
#include <cstdint>
namespace native_shot_observation {
inline bool firearm(int id){return id==98000000||id==98000200||id==98000400||id==98000500||id==98000600;}
struct Request {
 uint64_t owner=0,begin=0,end=0;uint32_t sequence=0,source=0,count=0;int bullet=0;
 float directions[8][3]={};
};
struct Match {const Request* request=nullptr;unsigned candidates=0;};
struct Ledger {
 Request requests[16]={};unsigned cursor=0;
 void add(const Request&r){if(r.owner&&r.begin&&r.end>=r.begin&&r.count&&r.count<=8&&firearm(r.bullet))requests[cursor++%16]=r;}
 Match match(uint64_t owner,int bullet,uint64_t now,float age)const{
  Match result;if(!std::isfinite(age)||age<0||age>.25f)return result;
  // This is a temporal association, not the emitter handle returned by Spawn.
  // Native age and wall time can differ by a frame. Ambiguous shots stay
  // unassociated; never pick whichever request best matches the observed ray.
  double born=double(now)-double(age)*1000.;
  for(const auto&r:requests)if(r.begin&&r.owner==owner&&r.bullet==bullet&&now>=r.begin&&now-r.begin<=350&&born>=double(r.begin)-50.&&born<=double(r.end)+50.){result.request=&r;result.candidates++;}
  if(result.candidates!=1)result.request=nullptr;return result;
 }
};
inline float observed_speed(const float*velocity){
 float squared=0;for(unsigned i=0;i<3;i++){if(!std::isfinite(velocity[i]))return 0;squared+=velocity[i]*velocity[i];}
 return std::isfinite(squared)&&squared>=1?std::sqrt(squared):0;
}
inline float nearest_angle(const Request&r,const float*velocity){
 float speed=observed_speed(velocity);if(speed<1)return -1;float inverseSpeed=1/speed,best=-1;
 for(unsigned n=0;n<r.count&&n<8;n++){float dot=0,length2=0;for(unsigned i=0;i<3;i++){dot+=r.directions[n][i]*velocity[i]*inverseSpeed;length2+=r.directions[n][i]*r.directions[n][i];}
  if(!std::isfinite(dot)||!std::isfinite(length2)||length2<.000001f)continue;dot/=std::sqrt(length2);if(dot>best)best=dot;
 }
 if(!r.count)return -1;best=best>1?1:best<-1?-1:best;return std::acos(best)*57.295779513f;
}
}
