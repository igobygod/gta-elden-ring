#include "player_status.h"
#include "radar_policy.h"
#include "status_region_cache.h"
#include "cheat_console.h"
#include "cheat_policy.h"
#include "../car-proof/vehicle_demo.h"
#include "rifle.h"
#include "../../NativePort/combat-source/native/gameplay_gate.h"
#include "../polish-next/player-status/health_policy.h"
#include <windows.h>
#include <intrin.h>
#include <cstring>
#include <cmath>
#include <atomic>
#include <cstdio>
#include <cstdarg>
namespace {
std::atomic<int> checked{0};uintptr_t base=0;DWORD gameThread=0;
SRWLOCK lock=SRWLOCK_INIT;PlayerStatus shared={};
void report(const char*fmt,...){FILE*f=igobygod::open(igobygod::log_path("health.log").c_str(),"a");if(!f)return;std::fprintf(f,"%llu ",GetTickCount64());va_list a;va_start(a,fmt);std::vfprintf(f,fmt,a);va_end(a);std::fputc('\n',f);std::fclose(f);}
thread_local status_memory::Cache regions;
struct TickRegions {TickRegions(){regions.reset();}~TickRegions(){regions.reset();}};
bool accessible(const void*p,size_t size,bool writing=false){
 return regions.allows(p,size,writing,[](uintptr_t a,status_memory::Region&r){
  MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery((void*)a,&m,sizeof(m)))return false;
  uintptr_t first=(uintptr_t)m.BaseAddress,end=first+m.RegionSize;if(end<first)return false;
  bool active=m.State==MEM_COMMIT&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS));
  unsigned reads=PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
  unsigned writes=PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
  r={first,end,active&&bool(m.Protect&reads),active&&bool(m.Protect&writes)};return true;
 });
}
uint8_t*global(uintptr_t offset){auto p=(uint8_t**)(base+offset);return accessible(p,8)?*p:nullptr;}
uint8_t*typed(void*p,uintptr_t vt,size_t size){return accessible(p,size)&&*(uintptr_t*)p==base+vt?(uint8_t*)p:nullptr;}
bool sig(uintptr_t rva,const unsigned char*b,size_t n){return accessible((void*)(base+rva),n)&&!std::memcmp((void*)(base+rva),b,n);}
void publish(const PlayerStatus&s){AcquireSRWLockExclusive(&lock);shared=s;shared.memory_queries=regions.queries;shared.memory_cache_hits=regions.hits;ReleaseSRWLockExclusive(&lock);}
struct HealthState {uintptr_t owner=0,data=0;uint64_t handle=0;int original=0,mode=0,applied=0;}health;
bool radarIdentity=false;bool invLayout=false;struct InvState {uintptr_t owner=0;uint64_t handle=0;bool active=false,original=false;} inv;
bool inv_bit(uint8_t*p,bool value){if(!accessible(p+0x1C5,1,true))return false;for(int i=0;i<8;i++){auto before=p[0x1C5];auto after=(uint8_t)((before&~0x10)|(value?0x10:0));if((uint8_t)_InterlockedCompareExchange8((volatile CHAR*)(p+0x1C5),(CHAR)after,(CHAR)before)==before)return true;}return false;}
bool change(uint8_t*data,int target,bool dead=false,bool fill=false){
 int hp=*(int*)(data+0x138),maximum=*(int*)(data+0x13C);int next=dead?hp:cheat_policy::health_value(hp,maximum,target,fill);if(next<0||!accessible(data+0x138,8,true)||((uintptr_t)(data+0x138)&7))return false;
 // One atomic pair update on a console command. A concurrent damage
 // change makes this fail rather than overwriting damage. Zero HP stays zero.
 uint64_t before=uint32_t(hp)|(uint64_t(uint32_t(maximum))<<32),after=uint32_t(next)|(uint64_t(uint32_t(target))<<32);
 return (uint64_t)InterlockedCompareExchange64((volatile LONG64*)(data+0x138),(LONG64)after,(LONG64)before)==before;
}
void radar_diagnostic(const PlayerStatus&s){
 static unsigned emitted=0,last[5]={~0u,~0u,~0u,~0u,~0u};unsigned counts[5]={};
 for(unsigned i=0;i<s.hostile_count&&i<32;i++)if(s.hostiles[i].category<5)counts[s.hostiles[i].category]++;
 bool changed=std::memcmp(last,counts,sizeof(counts))!=0;if(!changed||emitted>=16)return;std::memcpy(last,counts,sizeof(counts));emitted++;
 report("RADAR_CLASS record=%u identity_signature=%u markers=%u unknown=%u enemy=%u passive=%u animal=%u ally=%u",emitted,radarIdentity?1:0,s.hostile_count,counts[0],counts[1],counts[2],counts[3],counts[4]);
 // One representative per present category, from already-copied snapshot data.
 for(unsigned category=0;category<5;category++)for(unsigned i=0;i<s.hostile_count&&i<32;i++){const auto&e=s.hostiles[i];if(e.category!=category)continue;report("RADAR_SAMPLE record=%u category=%u team=%u model=%u flags=%u handle=%llx",emitted,category,e.team,e.model_id,e.flags,e.handle);break;}
}
void radar(uint8_t*world,PlayerStatus&s){
 if(!accessible(world+0x1F1D8,16))return;
 uintptr_t begin=(uintptr_t)*(uint8_t**)(world+0x1F1D8),end=(uintptr_t)*(uint8_t**)(world+0x1F1E0);s.hostile_count=0;s.radar_time=s.time;
 if(end<begin||(end-begin)%16)return;size_t count=(end-begin)/16;if(count>64)count=64;if(!count){radar_diagnostic(s);return;}if(!accessible((void*)begin,count*16))return;
 float distances[32]={};
 for(size_t i=0;i<count;i++){
  auto npc=typed(*(void**)(begin+i*16),0x2A47090,0x1C6);if(!npc||(uintptr_t)npc==s.owner)continue;unsigned team=npc[0x6C];
  auto mods=*(uint8_t**)(npc+0x190);if(!accessible(mods,0x70))continue;auto data=typed(*(void**)mods,0x2A380B8,0x140);auto phys=typed(*(void**)(mods+0x68),0x2A3C890,0x7C);if(!data||!phys)continue;
  int hp=*(int*)(data+0x138),max=*(int*)(data+0x13C);if(!status_health::valid(hp,max)||(npc[0x1C5]&0x80))continue;
  float pos[3];std::memcpy(pos,phys+0x70,12);float distance=0;bool finite=true;for(int j=0;j<3;j++){finite=finite&&std::isfinite(pos[j]);float d=pos[j]-s.position[j];distance+=d*d;}if(!finite||!radar_policy::visible(hp,distance))continue;
  uint64_t handle=*(uint64_t*)(npc+8);bool duplicate=false;for(unsigned j=0;j<s.hostile_count;j++)if(s.hostiles[j].handle==handle)duplicate=true;if(duplicate)continue;
  unsigned j=0;while(j<s.hostile_count&&distances[j]<=distance)j++;if(j>=32)continue;if(s.hostile_count<32)s.hostile_count++;for(unsigned k=s.hostile_count-1;k>j;k--){s.hostiles[k]=s.hostiles[k-1];distances[k]=distances[k-1];}
  auto&e=s.hostiles[j];e={};e.handle=handle;e.team=team;bool identity=radarIdentity&&*(uint32_t*)(npc+0x64)==*(uint32_t*)(npc+0x188);e.model_id=identity?*(uint32_t*)(npc+0x64):0;e.category=radar_policy::category(team,e.model_id,identity);e.flags=team==7?1:0;distances[j]=distance;std::memcpy(e.position,pos,12);
 }
 radar_diagnostic(s);
}
PlayerStatus previousRadar={};uintptr_t radarAttemptOwner=0;uint64_t radarAttemptTime=0;float radarAttemptPosition[3]={};
}
extern "C" int player_status_init(){
 int current=checked.load();if(current)return current==1;TickRegions initRegions;base=(uintptr_t)GetModuleHandleW(nullptr);if(!base){checked=-1;return 0;}
 const unsigned char modelGetter[]={0x48,0x8B,0x48,0x08,0x48,0x85,0xC9,0x74,0x04,0x8B,0x41,0x64,0xC3};
 radarIdentity=accessible((void*)(base+0x397B86),sizeof(modelGetter))&&std::memcmp((void*)(base+0x397B86),modelGetter,sizeof(modelGetter))==0;
 // Current supported executable disassembled locally: +190 -> data[0] -> HP/SP.
 const unsigned char hp[]={0x48,0x8B,0x80,0x90,0x01,0,0,0x48,0x8B,0x08,0x8B,0x81,0x38,0x01,0,0};
 const unsigned char sp[]={0x48,0x8B,0x80,0x90,0x01,0,0,0x48,0x8B,0x08,0x8B,0x81,0x54,0x01,0,0};
 const unsigned char maximum[]={0x8B,0x81,0x3C,0x01,0,0,0xC3};
 const unsigned char maxsp[]={0x8B,0x81,0x58,0x01,0,0};
 const unsigned char fp[]={0x8B,0x81,0x48,0x01,0,0};const unsigned char maxfp[]={0x8B,0x81,0x4C,0x01,0,0};
 const unsigned char invSetter[]={0x48,0x85,0xc9,0x74,0x13,0x80,0xa1,0xc5,0x01,0,0,0xef,0x80,0xe2,0x01,0xc0,0xe2,0x04,0x08,0x91,0xc5,0x01,0,0,0xc3};
 const unsigned char invGetter[]={0x41,0xf6,0x81,0xc5,0x01,0,0,0x10,0x75,0x11};invLayout=sig(0x5EE180,invSetter,sizeof(invSetter))&&sig(0x3F3EA6,invGetter,sizeof(invGetter));
 bool ok=sig(0x3297F2,hp,sizeof(hp))&&sig(0x329D22,sp,sizeof(sp))&&sig(0x1D3F030,maximum,sizeof(maximum))&&sig(0x773AC2,maxsp,sizeof(maxsp))&&sig(0x773A76,fp,sizeof(fp))&&sig(0x773A8D,maxfp,sizeof(maxfp));checked=ok?1:-1;report(ok?"verified current executable HP/FP/stamina layout":"layout signature mismatch; status/health disabled");return ok?1:0;
}
extern "C" void player_status_tick(){
 TickRegions tickRegions;
 if(!player_status_init())return;if(!gameThread)gameThread=GetCurrentThreadId();if(gameThread!=GetCurrentThreadId())return;
 CheatCommand cmd={};bool requested=cheat_console_take(&cmd)!=0;if(requested)cheat_console_result("Cheat rejected: player unavailable");
 PlayerStatus s={};s.time=GetTickCount64();auto w=typed(global(0x3D69FF8),0x2A4E980,8);if(!w||!accessible(w+0x1E508,8)){publish(s);return;}
 auto p=typed(*(void**)(w+0x1E508),0x2A7FBB0,0x1C6);if(!p){publish(s);return;}auto mods=*(uint8_t**)(p+0x190);if(!accessible(mods,0x70)){publish(s);return;}
 auto data=typed(*(void**)mods,0x2A380B8,0x160);auto phys=typed(*(void**)(mods+0x68),0x2A3C890,0x7C);if(!data||!phys){publish(s);return;}
 s.owner=(uintptr_t)p;s.hp=*(int*)(data+0x138);s.max_hp=*(int*)(data+0x13C);if(!status_health::valid(s.hp,s.max_hp)){publish(s);return;}
 std::memcpy(s.position,phys+0x70,12);for(float value:s.position)if(!std::isfinite(value)){publish(s);return;}
 if(accessible(p+0x6C0,0x14)){std::memcpy(s.map_local_position,p+0x6C0,12);std::memcpy(&s.map_block_id,p+0x6D0,4);s.map_valid=1;for(float value:s.map_local_position)if(!std::isfinite(value)||std::fabs(value)>65536.f)s.map_valid=0;}
 s.valid=1;s.dead=s.hp<=0||(p[0x1C5]&0x80);s.stamina=*(int*)(data+0x154);s.max_stamina=*(int*)(data+0x158);s.stamina_valid=status_health::valid(s.stamina,s.max_stamina);
 s.fp=*(int*)(data+0x148);s.max_fp=*(int*)(data+0x14C);s.fp_valid=status_health::valid(s.fp,s.max_fp);
 uint64_t handle=*(uint64_t*)(p+8);if(health.owner!=s.owner||health.data!=(uintptr_t)data||health.handle!=handle)health={s.owner,(uintptr_t)data,handle,0,0,0};
 if(health.mode&&s.max_hp!=health.applied){report("native stat recalculation ended mode=%d owner=%p observed_max=%d applied_max=%d",health.mode,p,s.max_hp,health.applied);health.original=0;health.mode=health.applied=0;} // Native stat recalculation wins; never reapply per frame.
 if(inv.owner!=s.owner||inv.handle!=handle){auto old=typed((void*)inv.owner,0x2A7FBB0,0x1C6);if(inv.active&&old&&*(uint64_t*)(old+8)==inv.handle)inv_bit(old,inv.original);inv={s.owner,handle,false,false};}
 // Native death/load transitions own this flag. Never reapply or refill HP each tick.
 if(inv.active&&(s.dead||!(p[0x1C5]&0x10))){if(s.dead)inv_bit(p,inv.original);inv.active=false;}
 bool allowed=false;if(requested&&!s.dead){DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);allowed=foreground==GetCurrentProcessId()&&gameplay_hud_normal()==1;}
 if(requested&&allowed){char result[160]={};
  if(cmd.kind==1){if(!health.mode&&cmd.value)health.original=s.max_hp;int target=cmd.value?cmd.value:(health.original?health.original:s.max_hp);
   if(change(data,target,false,cmd.value!=0)){health.mode=cmd.value?1:0;health.applied=cmd.value?target:0;if(!cmd.value)health.original=0;std::snprintf(result,sizeof(result),cmd.value?"MAXHEALTH %d: health filled":"Normal maximum restored: %d",target);report("console maxhealth owner=%p target=%d",p,target);}else std::snprintf(result,sizeof(result),"MAXHEALTH rejected: health changed concurrently");
  }else if(cmd.kind==2){if(!invLayout)std::snprintf(result,sizeof(result),"INVINCIBLE unavailable: native flag signature mismatch");else {bool enable=!inv.active;if(enable)inv.original=(p[0x1C5]&0x10)!=0;if(inv_bit(p,enable||inv.original)){inv.active=enable;std::snprintf(result,sizeof(result),"INVINCIBLE %s",enable?"ON":"OFF");report("console invincible owner=%p enabled=%d original=%d",p,enable,inv.original);}else std::snprintf(result,sizeof(result),"INVINCIBLE rejected: nonwritable native flag");}
  }else if(cmd.kind==3||cmd.kind==4){rifle_cheat_request(cmd.kind,s.owner);std::snprintf(result,sizeof(result),"Applying ammunition cheat...");}
  if(cmd.kind==5){car_request_spawn(s.owner);std::snprintf(result,sizeof(result),"Checking clear ground for Buffalo...");}
  else if(cmd.kind==6){car_request_remove(s.owner);std::snprintf(result,sizeof(result),"Removing Buffalo...");}
  if(result[0])cheat_console_result(result);
 }
 s.hp=*(int*)(data+0x138);s.max_hp=*(int*)(data+0x13C);s.health_hotkey_enabled=0;s.health_mode=health.mode;s.invincible=inv.active?1:0;
 float radarMove=0;for(int j=0;j<3;j++){float d=s.position[j]-radarAttemptPosition[j];radarMove+=d*d;}
 bool radarDue=radarAttemptOwner!=s.owner||radarMove>9.f||s.time<radarAttemptTime||s.time-radarAttemptTime>=200;
 if(radarDue){radarAttemptOwner=s.owner;radarAttemptTime=s.time;std::memcpy(radarAttemptPosition,s.position,12);
  if(!s.dead&&gameplay_hud_normal()==1){s.radar_scanned=1;radar(w,s);previousRadar=s;}
 }
 if(!s.radar_scanned&&previousRadar.owner==s.owner){s.radar_time=previousRadar.radar_time;s.hostile_count=previousRadar.hostile_count;std::memcpy(s.hostiles,previousRadar.hostiles,sizeof(s.hostiles));}
 publish(s);
}
extern "C" int player_status_snapshot(PlayerStatus*out){if(!out)return 0;AcquireSRWLockShared(&lock);*out=shared;ReleaseSRWLockShared(&lock);auto now=GetTickCount64();if(!out->valid||now<out->time||now-out->time>250){out->valid=0;return 0;}return 1;}
