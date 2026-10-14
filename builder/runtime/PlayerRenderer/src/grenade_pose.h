#pragma once
#include "arsenal_animation.h"
namespace grenade_pose {
// Keep the planted throw when stationary, but retain the walking leg cycle
// when native ER movement carries the player through the throw.
inline void blend(arsenal::Rig& rig, arsenal::Pose& gait, arsenal::Pose& action, float moving) {
 moving=std::clamp(moving,0.f,1.f);
 if(moving<=.001f){gait=action;return;}
 if(moving<1.f)rig.blend(gait,action,1.f-moving);
 action.q[45]=donor_motion::multiply(donor_motion::multiply(donor_motion::inverse(gait.q[0]),action.q[0]),action.q[45]);
 for(unsigned j=0;j<rig.bones;j++)if(rig.upper[j]){
  gait.q[j]=action.q[j];if(j!=45)gait.t[j]=action.t[j];
 }
}
}
