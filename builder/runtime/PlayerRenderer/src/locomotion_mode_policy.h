#pragma once
#include <cmath>
namespace locomotion_mode {
struct Mode {bool raised=false,relaxed_lock=false;};
inline Mode select(bool armed,bool aiming,bool native_rpg_action,bool locked){
 bool raised=armed&&(aiming||native_rpg_action);
 return {raised,armed&&locked&&!raised};
}
// Render heading only. Native orientation, input, movement speed and target
// state remain owned by Elden Ring. Donor +Y maps to world(-X,-Z).
enum class Source {Native,Aim,Movement,Held};
inline const char* name(Source s){return s==Source::Native?"native":s==Source::Aim?"aim":s==Source::Movement?"movement":"held";}
struct Heading {
 float held=0;bool relaxed=false;Source source=Source::Native;
 float update(Mode mode,bool locked,bool aim_fresh,float dx,float dz,float dt,
              float body_yaw,float aim_yaw,float displayed_yaw,bool reset){
  float previous=std::isfinite(displayed_yaw)?displayed_yaw:0.f;
  float body=std::isfinite(body_yaw)?body_yaw:previous;
  float aim=std::isfinite(aim_yaw)?aim_yaw:previous;
  if(reset){held=body;relaxed=false;previous=body;}
  if(!mode.relaxed_lock){relaxed=false;source=mode.raised?(locked&&!aim_fresh?Source::Held:Source::Aim):Source::Native;return mode.raised?(locked&&!aim_fresh?previous:aim):body;}
  // Entering Q while still must retain what was actually on screen, rather
  // than snapping toward the native locked target or an uninitialized yaw.
  if(!relaxed){held=previous;relaxed=true;}
  source=reset?Source::Native:Source::Held;
  if(!reset&&std::isfinite(dx)&&std::isfinite(dz)&&std::isfinite(dt)&&dt>.0001f&&dt<.25f){
   float distance2=dx*dx+dz*dz,threshold=.10f*dt;
   if(std::isfinite(distance2)&&distance2<9.f&&distance2>std::fmax(2.5e-7f,threshold*threshold)){held=std::atan2(-dx,-dz);source=Source::Movement;}
  }
  return held;
 }
};
}
