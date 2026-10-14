#pragma once
#include <cstdint>
#include <cmath>
namespace camera_policy {
inline bool heartbeat(uint64_t now,uint64_t last,bool established){return now>=last&&now-last<=(established?1500u:250u);}
inline bool collision(bool reset,uint64_t now,uint64_t last,float shift){return reset||now<last||now-last>=100||shift>.04f;}
inline float zoom(float target,int notches){return std::fmax(6.f,std::fmin(24.f,target-notches*1.5f));}
}
