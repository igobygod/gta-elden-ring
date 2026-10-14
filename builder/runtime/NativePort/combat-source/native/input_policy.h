#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
// Canonical DirectInput formats verified in this executable and declared by Microsoft's dinput.h.
namespace input_policy {
struct Policy {bool owns_world;bool wheel;bool aim=false;bool console=false;bool free_aim=false;bool driving=false;};
inline void mouse(void* data,size_t size,Policy p) {
 if(!p.owns_world||!data||(size!=16&&size!=20))return;
 if(p.console){memset(data,0,size);return;}
 auto bytes=(uint8_t*)data;if(p.driving){memset(bytes+8,0,size-8);return;}bytes[12]=p.aim&&!p.wheel?0x80:0;bytes[13]=0;
 if(p.wheel)memset(bytes,0,12); // only camera axes/wheel; extra mouse buttons pass through
 else if(p.free_aim)memset(bytes,0,8); // Q consumes XY for its independent reticle, never native target switching
}
inline void keyboard(void* data,size_t size,Policy p) {
 if(!p.owns_world||!data||size!=256)return;
 if(p.console){memset(data,0,size);return;}
 auto keys=(uint8_t*)data;if(p.driving){uint8_t escape=keys[1],console=keys[0x29];memset(keys,0,256);keys[1]=escape;keys[0x29]=console;return;}keys[0x0F]=0;keys[0x13]=0; // TAB and R
 if(p.wheel)for(unsigned i=0x02;i<=0x09;i++)keys[i]=0; // 1–8 selection shortcuts
}
inline uint32_t event(bool is_mouse,uint32_t offset,uint32_t value,Policy p) {
 if(!p.owns_world)return value;
 if(p.console)return 0;
 if(p.driving)return is_mouse?(offset<8?value:0):(offset==1||offset==0x29?value:0);
 if(is_mouse)return offset==12||offset==13||(p.wheel&&(offset==0||offset==4||offset==8))||(p.free_aim&&(offset==0||offset==4))?0:value;
 return offset==0x0F||offset==0x13||(p.wheel&&offset>=0x02&&offset<=0x09)?0:value;
}
}
