#include "native_locomotion.h"
#include "native_locomotion_policy.h"
#include "rifle.h"
#include "player_status.h"
#include "cheat_console.h"
#include "gameplay_gate.h"
#include <windows.h>
#include <intrin.h>
#include <atomic>
#include <cstring>

extern "C" {
 int WINAPI MH_Initialize();
 int WINAPI MH_CreateHook(void*,void*,void**);
 int WINAPI MH_EnableHook(void*);
 int WINAPI MH_RemoveHook(void*);
}
namespace {
using Getter=uint8_t(__fastcall*)(void*);
Getter original=nullptr;
uintptr_t image=0;
std::atomic<int> installed{0};
std::atomic<uint64_t> overrides[4]{},lastTime{0};
std::atomic<uintptr_t> lastOwner{0};
std::atomic<int32_t> lastRaw{-1};
// Regions are cached only for this single observation, never across lifetimes.
struct Reader {
 struct Region{uintptr_t begin,end;};Region cache[12]{};unsigned used=0;
 bool readable(uintptr_t at,size_t count){
  if(at<0x10000||count>UINTPTR_MAX-at)return false;uintptr_t end=at+count;
  for(unsigned i=0;i<used;i++)if(at>=cache[i].begin&&end<=cache[i].end)return true;
  while(at<end){MEMORY_BASIC_INFORMATION m={};
   if(!VirtualQuery((void*)at,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))||
      !(m.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
   uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=at)return false;
   if(used<12)cache[used++]={(uintptr_t)m.BaseAddress,next};at=next;
  }return true;
 }
 template<class T>bool read(uintptr_t at,T&out){if(!readable(at,sizeof(out)))return false;std::memcpy(&out,(void*)at,sizeof(out));return true;}
 bool typed(uintptr_t at,uintptr_t vt,size_t bytes){uintptr_t actual=0;return readable(at,bytes)&&read(at,actual)&&actual==image+vt;}
};
bool context(uintptr_t expected,native_locomotion_policy::State&out){
 using namespace native_locomotion_policy;
 if(!installed.load(std::memory_order_relaxed)||!expected)return false;
 RifleState gun={};if(!rifle_state(&gun)||!gun.active||gun.weapon<1||gun.weapon>7||gun.wheel_open)return false;
 auto now=GetTickCount64();if(!fresh(now,gun.time)||!locomotion(gun.native_action))return false;
 if(cheat_console_blocking()||gameplay_hud_normal()!=1)return false;
 PlayerStatus player={};if(!player_status_snapshot(&player)||player.owner!=expected||!player.valid||player.dead||player.hp<=0||!fresh(now,player.time))return false;
 Reader r;uintptr_t world=0,owner=0,mods=0,ta=0,taOwner=0,data=0;uint32_t block=0,index=0;uint8_t lifecycle=0,locked=0;int32_t raw=-1,hp=0;
 // Exact same local-player and TimeAct ownership paths used by rifle/renderer.
 // Check current native action too: a previous-frame snapshot cannot admit a
 // newly started roll, jump, hit reaction, death or melee action.
 if(!r.read(image+0x3D69FF8,world)||!r.typed(world,0x2A4E980,8)||
    !r.read(world+0x1E508,owner)||owner!=expected||!r.typed(owner,0x2A7FBB0,0x1C6)||
    !r.read(owner+0x1C5,lifecycle)||(lifecycle&0x80)||!r.read(owner+0xC9,locked)||locked!=1||
    !r.read(owner+0x6D0,block)||!block||block==0xFFFFFFFFu||!r.read(owner+0x190,mods)||
    !r.read(mods,data)||!r.typed(data,0x2A380B8,0x140)||!r.read(data+0x138,hp)||hp<=0||
    !r.read(mods+0x18,ta)||!r.read(ta+8,taOwner)||taOwner!=owner||
    !r.read(ta+0xC4,index)||index>=10||!r.read(ta+0x20+index*16,raw))return false;
 out.owner=owner;out.expected=expected;out.now=now;out.weapon_time=gun.time;out.player_time=player.time;
 out.weapon=gun.weapon;out.raw=raw;out.installed=out.alive=out.normal=out.active=out.locked=true;
 return eligible(out);
}
__declspec(noinline) uint8_t __fastcall hooked(void*owner){
 uintptr_t caller=(uintptr_t)_ReturnAddress();
 uint8_t value=original(owner);
 int site=caller>=image?native_locomotion_policy::site(caller-image):-1;
 if(value!=1||site<0)return value;
 native_locomotion_policy::State state;
 if(!context((uintptr_t)owner,state))return value;
 overrides[site].fetch_add(1,std::memory_order_relaxed);
 lastOwner.store((uintptr_t)owner,std::memory_order_relaxed);lastRaw.store(state.raw,std::memory_order_relaxed);
 lastTime.store(state.now,std::memory_order_release);
 return 0;
}
bool signature(uintptr_t rva,const unsigned char*bytes,size_t size){Reader r;return r.readable(image+rva,size)&&!std::memcmp((void*)(image+rva),bytes,size);}
template<size_t N>bool signature(uintptr_t rva,const unsigned char(&bytes)[N]){return signature(rva,bytes,N);}
}
extern "C" int native_locomotion_init(){
 if(installed.load())return 1;
 image=(uintptr_t)GetModuleHandleW(nullptr);if(!image)return 0;
 const unsigned char getter[]={0x0F,0xB6,0x81,0xC9,0,0,0,0xC3};
 const unsigned char facing[]={0x48,0x8B,0x8E,0xA8,0,0,0,0xE8,0x4D,0xAB,1,0,0x84,0xC0};
 const unsigned char relative[]={0x48,0x8B,0x89,0xA8,0,0,0,0xE8,0x27,0xA2,1,0,0x84,0xC0};
 const unsigned char input[]={0xE8,0xE0,0x83,1,0,0x44,0x0F,0xB6,0xC0,0x4C,0x8D,0x4C,0x24};
 const unsigned char graph[]={0x48,0x8B,0xCB,0xE8,0x67,0xDB,1,0,0x48,0x8B,0xC8,0xE8,0xBF,0x4B,0xFD,0xFF,0x0F,0xB6,0xD0,0x48,0x8D,0x4C,0x24,0x20,0xE8,0x32,0xEE,0x7F,0};
 const unsigned char handle[]={0x49,0x8D,0x9F,0xD0,0x15,0,0};
 const unsigned char variableTable[]={0x48,0x8D,0x35,0xE0,0x17,0x70,3,0xBD,0x29};
 if(!signature(0x3F42B0,getter)||!signature(0x3D9757,facing)||!signature(0x3DA07D,relative)||
    !signature(0x3DBECB,input)||!signature(0x41F6E1,graph)||!signature(0x419AE0,handle)||!signature(0x419BF9,variableTable))return 0;
 Reader r;uintptr_t variable=0;const wchar_t name[]=L"IsLockon";
 if(!r.read(image+0x3B1B3E0,variable)||variable!=image+0x2A35DF0||!r.readable(variable,sizeof(name))||std::memcmp((void*)variable,name,sizeof(name)))return 0;
 HMODULE pinned=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&native_locomotion_init,&pinned))return 0;
 int status=MH_Initialize();if(status!=0&&status!=1)return 0;
 void*target=(void*)(image+0x3F42B0);
 if(MH_CreateHook(target,(void*)&hooked,(void**)&original)!=0)return 0;
 if(MH_EnableHook(target)!=0){MH_RemoveHook(target);original=nullptr;return 0;}
 installed.store(1,std::memory_order_release);return 1;
}
extern "C" int native_locomotion_active(uintptr_t owner){
 auto now=GetTickCount64();auto time=lastTime.load(std::memory_order_acquire);
 if(lastOwner.load(std::memory_order_relaxed)!=owner||!native_locomotion_policy::fresh(now,time))return 0;
 native_locomotion_policy::State state;return context(owner,state)?1:0;
}
extern "C" void native_locomotion_status(NativeLocomotionStatus*out){
 if(!out)return;out->installed=installed.load();out->time=lastTime.load(std::memory_order_acquire);
 out->owner=lastOwner.load();out->raw=lastRaw.load();for(unsigned i=0;i<4;i++)out->overrides[i]=overrides[i].load();
}
