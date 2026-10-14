#include "terrain_map.h"
#include <cassert>
int main(){float p[3]={22.72f,3.85f,-23.03f};auto v=er_terrain_map::uv((60u<<24)|(40u<<16)|(35u<<8),p);assert(v.valid);assert(std::fabs(v.u-1430.72f/4096)<1e-6f);assert(std::fabs(v.v-1943.03f/4096)<1e-6f);assert(!er_terrain_map::uv(0,p).valid);p[0]=NAN;assert(!er_terrain_map::uv(60u<<24,p).valid);}
