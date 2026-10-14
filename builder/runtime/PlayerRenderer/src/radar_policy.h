#pragma once
#include <cmath>
#include <cstdint>
namespace radar_policy {
enum Category:unsigned {Unknown=0,Enemy=1,Passive=2,Animal=3,Ally=4};
// Exact wildlife IDs named in the installed Soulstruct Elden Ring constants.
// Miriel6091 is deliberately not classified as wildlife.
inline bool animal(unsigned id){switch(id){case 6000:case 6001:case 6010:case 6030:case 6031:case 6040:case 6050:case 6060:case 6070:case 6071:case 6072:case 6080:case 6081:case 6082:case 6090:case 6100:return true;default:return false;}}
inline unsigned category(unsigned team,unsigned model,bool identity){if(identity&&animal(model))return Animal;if(team==7)return Enemy;if(team==6)return Passive;if(team==47||team==2)return Ally;return Unknown;}
inline bool visible(int hp,float squared){return hp>0&&std::isfinite(squared)&&squared>=0&&squared<=6400;}
}
