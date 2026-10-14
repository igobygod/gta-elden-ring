#pragma once
#include <stdint.h>
#include <cmath>
namespace arsenal_wheel {
// GTA V clockwise wheel categories; gameplay IDs and numeric shortcuts stay stable.
inline unsigned weapon_at_sector(unsigned sector){constexpr unsigned order[]={1,2,4,5,0,3,6,7};return order[sector%8];}
inline unsigned sector_of_weapon(unsigned weapon){constexpr unsigned sectors[]={4,0,1,5,2,3,6,7};return weapon<8?sectors[weapon]:4;}
inline unsigned cursor_update(float&x,float&y,float dx,float dy,unsigned previous){
 if(!std::isfinite(dx)||!std::isfinite(dy))return previous;
 x+=dx;y+=dy;float distance=std::sqrt(x*x+y*y);
 if(distance>100.f){x*=100.f/distance;y*=100.f/distance;}
 if(distance<=15.f)return previous;
 float angle=std::atan2(x,-y);if(angle<0)angle+=6.2831853072f;
 return weapon_at_sector(unsigned(angle/.7853981634f+.5f)%8);
}

template<unsigned Slot>struct VariantScroll {int remainder=0;unsigned variant=0;void reset(){remainder=0;}unsigned update(int delta,bool wheel,unsigned hover){if(!wheel||hover!=Slot){reset();return variant;}if(delta>960)delta=960;if(delta< -960)delta=-960;remainder+=delta;while(remainder>=120){variant^=1;remainder-=120;}while(remainder<=-120){variant^=1;remainder+=120;}return variant;}};
using LauncherScroll=VariantScroll<6>;
using MeleeScroll=VariantScroll<0>;
}
