#include "muzzle_visual.h"
#include <cassert>
int main(){for(unsigned w=0;w<8;w++)for(unsigned seed=0;seed<32;seed++)for(int t=0;t<150;t++){auto s=muzzle_visual::evaluate(w,t*.001f,seed);if(s.valid){assert(w>=1&&w<=5);assert(s.width>0&&s.length>0&&std::isfinite(s.width+s.length+s.centerAlong));assert(s.uv[0]>=0&&s.uv[1]>=0&&s.uv[0]+s.uv[2]<=1&&s.uv[1]+s.uv[3]<=1);assert(s.pivotAlong>=0&&s.pivotAlong<=1);assert(t*.001f<s.lifetime);}}}
