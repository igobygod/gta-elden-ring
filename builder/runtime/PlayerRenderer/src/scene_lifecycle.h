#pragma once
#include <cstdint>
#include <algorithm>
// Rendering follows the native loading/fade/menu state. A dead snapshot left
// behind by a loading screen cannot begin a new death presentation.
struct SceneLifecycle {
 uintptr_t owner=0; uint64_t readySince=0,diedAt=0,last=0; uint32_t epoch=0;
 bool aliveSeen=false,wasDead=false,visible=false,deathStarted=false;
 void clear(){if(owner||readySince||visible)++epoch;owner=0;readySince=diedAt=0;aliveSeen=wasDead=visible=deathStarted=false;}
 bool update(uint64_t now,uintptr_t who,bool worldValid,bool loading,bool fade,bool menu,bool dead){
  deathStarted=false;
  if(now<last){clear();}last=now;
  if(!worldValid||!who||loading||fade){clear();return false;}
  if(owner!=who){clear();owner=who;}
  if(menu){visible=false;readySince=0;return false;}
  if(!readySince){readySince=now;}
  visible=now>=readySince&&now-readySince>=150;
  if(!dead){aliveSeen=true;wasDead=false;diedAt=0;}
  else if(!wasDead){wasDead=true;if(aliveSeen&&visible){diedAt=now;deathStarted=true;}}
  return visible;
 }
 float wasted(uint64_t now)const{
  if(!visible||!wasDead||!diedAt||now<diedAt)return 0;
  uint64_t age=now-diedAt;if(age<1800||age>=6500)return 0;
  return std::min(1.f,float(age-1800)/450.f)*std::min(1.f,float(6500-age)/650.f);
 }
};
