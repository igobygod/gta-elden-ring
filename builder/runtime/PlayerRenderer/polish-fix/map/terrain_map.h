#pragma once
#include <cmath>
#include <cstdint>
namespace er_terrain_map {
constexpr unsigned AtlasSize=4096;
constexpr float OriginX=1792.f,OriginY=5632.f,Extent=3072.f;
struct UV {float u=0,v=0;bool valid=false;};
// Original surface M00 map descriptor, native 0x886838 / 0x877130.
// Block ID is little-endian u32 from PlayerIns+0x6D0; position is +0x6C0.
inline UV uv(uint32_t block,const float*local){
 UV out; if(!local||block>>24!=60)return out;
 for(int i=0;i<3;i++)if(!std::isfinite(local[i]))return out;
 float x=local[0]+float(int((block>>16)&255)-28)*256.f+128.f;
 float y=-local[2]+float(64-int((block>>8)&255))*256.f+128.f;
 out.u=(x-OriginX)/AtlasSize;out.v=(y-OriginY)/AtlasSize;
 out.valid=x>=OriginX&&x<OriginX+Extent&&y>=OriginY&&y<OriginY+Extent;return out;
}
// 80m radar radius = 80/4096 UV radius; rotate sample offsets with camera.
constexpr float uv_radius(float meters){return meters/AtlasSize;}
}
