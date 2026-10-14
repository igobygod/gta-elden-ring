#pragma once
#include <stdint.h>
#include <cmath>
#include <cstring>
namespace car_demo_policy {
inline bool finite3(const float* p) { return std::isfinite(p[0])&&std::isfinite(p[1])&&std::isfinite(p[2]); }
inline bool ground_height(const float* heights,float& highest,float& spread) {
    highest=-1e30f;float lowest=1e30f;
    for(int i=0;i<5;i++){if(!std::isfinite(heights[i]))return false;highest=std::fmax(highest,heights[i]);lowest=std::fmin(lowest,heights[i]);}
    spread=highest-lowest;return spread<=.85f;
}
inline bool lifecycle(uintptr_t owner,uintptr_t current,bool valid,bool dead,uint32_t block,uint32_t currentBlock,const float*before,const float*now) {
    if(!valid||dead||!owner||owner!=current||block!=currentBlock||!finite3(before)||!finite3(now))return false;
    float d=0;for(int i=0;i<3;i++){float e=now[i]-before[i];d+=e*e;}return d<=64.f;
}
inline bool fresh(uint64_t now,uint64_t at){return at&&now>=at&&now-at<=250;}
inline bool world_ready(bool uiValid,bool fadeValid,bool loading,bool fade){return uiValid&&fadeValid&&!loading&&!fade;}
inline void transform(float* m,const float*forward,const float*origin) {
    // Proper rotation: local -Z points forward, +Y stays up, determinant +1.
    float z[3]={-forward[0],0,-forward[2]},x[3]={z[2],0,-z[0]};
    std::memset(m,0,16*sizeof(float));m[0]=x[0];m[2]=x[2];m[5]=1;m[8]=z[0];m[10]=z[2];m[15]=1;
    std::memcpy(m+12,origin,3*sizeof(float));
}
inline void point(const float*m,float x,float y,float z,float*out) {
    for(int i=0;i<3;i++)out[i]=m[i]*x+m[4+i]*y+m[8+i]*z+m[12+i];
}
}
