#pragma once
#include <cstdint>
namespace ammo_policy {
inline unsigned transfer(unsigned ammo,unsigned capacity,unsigned&reserve){if(ammo>=capacity)return ammo;unsigned take=capacity-ammo;if(take>reserve)take=reserve;reserve-=take;return ammo+take;}
inline bool changed(unsigned oldWeapon,unsigned newWeapon,unsigned oldVariant,unsigned newVariant){return oldWeapon!=newWeapon||(newWeapon==6&&oldVariant!=newVariant);}
inline bool recovered(uint64_t now,uint64_t cycle,unsigned duration){return !cycle||(now>=cycle&&now-cycle>=duration);}
}
