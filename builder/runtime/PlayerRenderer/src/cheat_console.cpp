#include "cheat_console.h"
#include "cheat_policy.h"
#include "../../NativePort/combat-source/native/input.h"
#include <windows.h>
#include <cstring>
#include <cstdio>
namespace {
SRWLOCK lock=SRWLOCK_INIT;CheatConsoleSnapshot state={};CheatCommand pending={};bool keys[256]={};bool drain=false;uint64_t lastTick=0;
bool gameplay_key_down(const bool*down){for(int k=1;k<256;k++){if(k==VK_CAPITAL||k==VK_NUMLOCK||k==VK_SCROLL)continue;if(down[k])return true;}return false;}
}
extern "C" void cheat_console_tick(int allowed){
 DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);bool focused=pid==GetCurrentProcessId();allowed=allowed&&focused;auto now=GetTickCount64();
 AcquireSRWLockExclusive(&lock);bool stale=lastTick&&now-lastTick>250;lastTick=now;
 bool tilde=GetAsyncKeyState(VK_OEM_3)<0;bool toggle=tilde&&!keys[VK_OEM_3];bool opening=allowed&&toggle&&!state.active&&!stale;
 bool down[256]={},edge[256]={};down[VK_OEM_3]=tilde;
 // Idle gameplay samples only the opening key. Capture held keys on open so
 // a movement key already down cannot become a typed character.
 if(state.active||drain||opening||stale)for(int k=1;k<256;k++){down[k]=k==VK_OEM_3?tilde:GetAsyncKeyState(k)<0;edge[k]=down[k]&&!keys[k];}
 if(opening||stale){std::memcpy(keys,down,sizeof(keys));std::memset(edge,0,sizeof(edge));}
 if(stale){state.active=0;pending={};drain=true;toggle=false;}
 if(!allowed){if(state.active)drain=true;state.active=0;pending={};}
 else if(toggle){state.active=!state.active;drain=!state.active;if(state.active){state.input[0]=0;state.result_time=now;std::snprintf(state.result,sizeof(state.result),"MAXHEALTH, INVINCIBLE, MAXAMMO, NORELOAD, BUFFALO, CARCLEAR");}}
 else if(state.active){
  if(edge[VK_ESCAPE]){state.active=0;drain=true;state.input[0]=0;}
  else if(edge[VK_RETURN]){state.result_time=now;CheatCommand cmd={};if(cheat_policy::parse(state.input,cmd)&&!pending.kind){pending=cmd;std::snprintf(state.result,sizeof(state.result),"Applying cheat...");}else std::snprintf(state.result,sizeof(state.result),"Unknown command or invalid value");state.active=0;drain=true;}
  else {size_t n=std::strlen(state.input);if(edge[VK_BACK]&&n)state.input[--n]=0;for(int k='A';k<='Z';k++)if(edge[k]&&n+1<sizeof(state.input)){state.input[n++]=(char)k;state.input[n]=0;}for(int k='0';k<='9';k++)if(edge[k]&&n+1<sizeof(state.input)){state.input[n++]=(char)k;state.input[n]=0;}if(edge[VK_SPACE]&&n+1<sizeof(state.input)){state.input[n++]=' ';state.input[n]=0;}}
 }
 if(allowed&&drain&&!gameplay_key_down(down))drain=false;state.time=now;state.blocking=allowed&&(state.active||drain);state.pending=pending.kind?1:0;std::memcpy(keys,down,sizeof(keys));bool block=state.blocking;ReleaseSRWLockExclusive(&lock);native_input_set_console(block?1:0);
}
extern "C" int cheat_console_snapshot(CheatConsoleSnapshot*out){if(!out)return 0;AcquireSRWLockShared(&lock);*out=state;ReleaseSRWLockShared(&lock);return GetTickCount64()-out->time<250;}
extern "C" int cheat_console_active(){CheatConsoleSnapshot s={};return cheat_console_snapshot(&s)&&s.active;}
extern "C" int cheat_console_blocking(){CheatConsoleSnapshot s={};return cheat_console_snapshot(&s)&&s.blocking;}
extern "C" int cheat_console_take(CheatCommand*out){if(!out)return 0;AcquireSRWLockExclusive(&lock);*out=pending;pending={};state.pending=0;ReleaseSRWLockExclusive(&lock);return out->kind!=0;}
extern "C" void cheat_console_result(const char*result){AcquireSRWLockExclusive(&lock);std::snprintf(state.result,sizeof(state.result),"%s",result?result:"");state.result_time=GetTickCount64();ReleaseSRWLockExclusive(&lock);}
