#pragma once
#include "vehicle_physics.h"
#include <cstdint>
namespace car_lifecycle {
inline uint64_t elapsed(uint64_t now,uint64_t start){return now>=start?now-start:0;}
enum class Motion {stable,correction,rebase,park};
// A pinned native capsule may still receive contact resolution. That is not a
// teleport. Origin shifts must have the 8 m quantum AND map-offset evidence.
inline Motion motion(vehicle::V delta,vehicle::V offsetDelta,bool hasOffset,vehicle::V&shift){
 shift={};if(!vehicle::finite(delta))return Motion::park;
 float distance=vehicle::length(delta);if(distance<.2f)return Motion::stable;
 if(distance<=2.5f)return Motion::correction;
 vehicle::V q={std::round(delta.x/8)*8,std::round(delta.y/8)*8,std::round(delta.z/8)*8};
 if(vehicle::length(q)>=4&&vehicle::length(delta-q)<=1.5f&&
    (!hasOffset||(vehicle::finite(offsetDelta)&&vehicle::length(offsetDelta-q)<=2.f))){shift=q;return Motion::rebase;}
 return Motion::park;
}
}
