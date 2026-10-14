#include "../../NativePort/combat-source/native/input_policy.h"
#include <array>
#include <cassert>
#include <cstdio>
int main(){using namespace input_policy;Policy p{true,false,false,false,false,true};
 for(unsigned size:{16,20}){std::array<unsigned char,20> data;data.fill(0x80);mouse(data.data(),size,p);for(unsigned i=0;i<size;i++)assert(data[i]==(i<8?0x80:0));}
 assert(event(true,8,120,p)==0&&event(true,12,0x80,p)==0&&event(true,0,30,p)==30);
 std::array<unsigned char,256> keys;keys.fill(0x80);keyboard(keys.data(),keys.size(),p);for(unsigned i=0;i<256;i++)assert(keys[i]==((i==1||i==0x29)?0x80:0));
 p.owns_world=false;keys.fill(0x80);keyboard(keys.data(),keys.size(),p);for(auto v:keys)assert(v==0x80);
 puts("PASS driver input: both mouse formats, buffered wheel/buttons, native movement blocking, exit to on-foot ownership");}
