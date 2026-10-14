#include "../car-proof/car_demo.h"
#include "cheat_console.h"
#include "native_animation_policy.h"
#include "arsenal_wheel_policy.h"
#include "ammo_policy.h"
#include "projectile_policy.h"
#include "rifle_aim_policy.h"
#include "native_shot_observation.h"
#include "native_melee.h"
#include <atomic>
// Standalone adapter derived from Bridge/vendor/er-reference/src/game.cpp.
// Only signatures and layouts already verified there are used. App 1.17.1 only.
// No bridge, compositor, world writes or HP writes. Bounded read-only native observations.
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "rifle.h"
#include "../polish-v4.6/Params/expected_params.h"
#include "input.h"
#include "dummy_matrix.hpp"
#include "gameplay_gate.h"
#include "effects.h"
#include "fx_projection.h"
#include "facing_math.h"


namespace {
uintptr_t base;
DWORD game_thread;
struct Region {uintptr_t first,end;};
thread_local Region regions[32];thread_local unsigned region_count;
struct CombatCost {double main=0,world=0,health=0,peak=0;uint64_t since=0;unsigned ticks=0,health_scans=0,enemies=0,world_scans=0,bullets=0;};
CombatCost combat_cost;uint64_t last_health_scan=0,native_scan_until=0;
double cost_clock(){LARGE_INTEGER t;QueryPerformanceCounter(&t);return double(t.QuadPart);}
double cost_ms(double ticks){static double frequency=[](){LARGE_INTEGER f;QueryPerformanceFrequency(&f);return double(f.QuadPart);}();return ticks*1000./frequency;}
struct CombatTickCost {double start=cost_clock();uint64_t now;CombatTickCost(uint64_t t):now(t){}~CombatTickCost(){double elapsed=cost_ms(cost_clock()-start);combat_cost.main+=elapsed;combat_cost.peak=fmax(combat_cost.peak,elapsed);combat_cost.ticks++;if(!combat_cost.since)combat_cost.since=now;if(now-combat_cost.since>=5000){FILE*f=igobygod::open(igobygod::log_path("combat-performance.log").c_str(),"a");if(f){fprintf(f,"%llu ticks=%u main_ms=%.3f world_ms=%.3f health_ms=%.3f peak_ms=%.3f health_scans=%u enemies=%u world_scans=%u bullets=%u\n",now,combat_cost.ticks,combat_cost.main/combat_cost.ticks,combat_cost.world/combat_cost.ticks,combat_cost.health/combat_cost.ticks,combat_cost.peak,combat_cost.health_scans,combat_cost.enemies,combat_cost.world_scans,combat_cost.bullets);fclose(f);}combat_cost={};combat_cost.since=now;}}};
struct CombatSpan {double start=cost_clock();bool health;CombatSpan(bool h):health(h){}~CombatSpan(){double ms=cost_ms(cost_clock()-start);if(health)combat_cost.health+=ms;else combat_cost.world+=ms;}};
void capture_health(uint64_t now,bool valid,bool force=false);
void launcher_probe(uint64_t now,uint64_t event,unsigned variant,const float* center,float radius,bool impact);

bool readable(const void* p,size_t size) {
 uintptr_t a=(uintptr_t)p,end=a+size;if(a<0x10000 || end<a)return false;
 for(unsigned i=0;i<region_count;i++)if(a>=regions[i].first&&end<=regions[i].end)return true;
 while(a<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery((void*)a,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
  if(!(m.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
  auto next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=a)return false;
  if(region_count<32)regions[region_count++]={(uintptr_t)m.BaseAddress,next};a=next;
 }return true;
}
uint8_t* typed(void* p,uintptr_t vt,size_t length){return readable(p,length)&&*(uintptr_t*)p==base+vt?(uint8_t*)p:nullptr;}
uint8_t* global(uintptr_t offset){auto p=(uint8_t**)(base+offset);return readable(p,8)?*p:nullptr;}
bool sig(uintptr_t offset,const char* pattern){const uint8_t* p=(uint8_t*)(base+offset);size_t n=0;for(auto s=pattern;*s;){while(*s==' ')s++;if(!*s)break;if(*s=='?'){s+=s[1]=='?'?2:1;}else{char h[3]={s[0],s[1],0};if(!readable(p+n,1)||p[n]!=(uint8_t)strtol(h,nullptr,16))return false;s+=2;}n++;}return true;}
struct Player {uint8_t* ins;uint8_t* phys;uint8_t* data;};
Player current={};
uint8_t* camera(){auto cs=global(0x3D669C8);if(!readable(cs,0x10))return nullptr;return typed(*(void**)(cs+8),0x2AA0A40,0x60);}
bool player(Player& p){auto w=typed(global(0x3D69FF8),0x2A4E980,0x1E510);if(!w)return false;
 p.ins=typed(*(void**)(w+0x1E508),0x2A7FBB0,0x6D4);if(!p.ins)return false;
 auto mods=*(uint8_t**)(p.ins+0x190);if(!readable(mods,0x70))return false;
 p.data=typed(*(void**)mods,0x2A380B8,0x140);p.phys=typed(*(void**)(mods+0x68),0x2A3C890,0x2E8);
 if(!p.data||!p.phys)return false;
 auto block=*(uint32_t*)(p.ins+0x6D0);
 return block&&block!=0xFFFFFFFFu&&*(int*)(p.data+0x138)>0&&!(p.ins[0x1C5]&0x80)&&camera();
}
bool unit(float* v){float l=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);if(!isfinite(l)||l<1e-5f)return false;for(int i=0;i<3;i++)v[i]/=l;return true;}
void cross(const float* a,const float* b,float* c){c[0]=a[1]*b[2]-a[2]*b[1];c[1]=a[2]*b[0]-a[0]*b[2];c[2]=a[0]*b[1]-a[1]*b[0];}
float dist(const float* a,const float* b){float v[3]={a[0]-b[0],a[1]-b[1],a[2]-b[2]};return sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);}
bool ray(const float* start,const float* end,float* hit,bool& touched,bool ignore_player=true,uint32_t filter=0x2000058){
 auto h=global(0x3D7A0D0);if(!readable(h,0xA0))return false;void* world=*(void**)(h+0x98);if(!readable(world,8))return false;
 alignas(16)float o[4]={start[0],start[1],start[2],1},delta[4]={end[0]-start[0],end[1]-start[1],end[2]-start[2],0},r[4]={};
 using Cast=bool(__fastcall*)(void*,uint32_t,const float*,const float*,float*,void*);
 touched=((Cast)(base+0xC71E00))(world,filter,o,delta,r,ignore_player?current.ins:nullptr);
 if(touched){for(int i=0;i<3;i++)if(!isfinite(r[i]))return false;memcpy(hit,r,12);}return true;
}
struct alignas(16) Spawn {
 uint64_t owner;int32_t behaviorId,magicId;uint32_t unk10;int32_t bulletId,goodsId,dmyPolyId;
 // -1 inherits owner lock target; custom firearm lockShootLimitAng=0 preserves explicit aim.
 uint64_t target;uint32_t unk28,unk2c;float unk30[4];uint32_t unk40;
 // App1.17.1: Spawn+44 bit0 -> request+194 -> emitter+4C4.
 // RVA3901A3/3901AC stops emission after the initial shot, allowing native
 // cleanup once its children expire. Zero leaves every emitter alive and
 // exhausts the 64+192 pool (Spawn error 4), regardless of HUD ammo.
 uint32_t flags=1;uint8_t pad48[8];
 float right[4],up[4],fwd[4],pos[4];uint8_t rest[0x80];
};static_assert(sizeof(Spawn)==0x110,"SpawnBullet ABI");
struct Task {void** vtbl;uint64_t unk8,pad[6];};
uintptr_t __fastcall no_class(void*){return 0;}void __fastcall no_delete(void*){}
SRWLOCK state_lock=SRWLOCK_INIT;RifleState shared={};ArsenalEvents events={};ArsenalWorld world_shared={};ArsenalImpact impacts[32]={};unsigned impact_cursor=0;
struct Muzzle {uintptr_t owner;uint64_t time;float position[3];uint32_t ready;};Muzzle visual={};
struct View {uintptr_t owner;uint64_t time;float origin[3],direction[3];uint32_t ready;};View final_view={};
struct Weapon {uint32_t clip,interval,reload;int bullet;bool semi;float range;};
const Weapon weapons[8]={{0,0,0,-1,true,0},{12,370,1833,98000200,true,120},{30,92,2000,98000500,false,120},{8,900,2600,98000400,true,70},{30,158,2033,98000000,false,120},{10,1220,2867,98000600,true,250},{1,1200,2867,98000300,true,120},{1,750,2400,98000100,true,30}};
uint32_t selected=4,hover=4,magazines[8]={0,12,30,8,30,10,1,1};uint32_t reserves[8]={0,60,150,40,150,50,5,5};bool switch_reload=false;uint64_t grenade_ready_at=0;bool wheel_was=false;float wheel_x=0,wheel_y=0;
bool lock_layout=false,animation_layout=false,lock_target_layout=false;arsenal_wheel::LauncherScroll launcherScroll;unsigned launcher_variant=0;
arsenal_wheel::MeleeScroll meleeScroll;unsigned melee_variant=0;NativeMeleeState melee_state={};uintptr_t melee_owner=0;bool melee_lease_active=false;
std::atomic<uint32_t> rpg_reload_ms{2733},rpg_fire_ms{733};
const Weapon rpgWeapon={1,800,2733,98000700,true,85};
Weapon equipped_weapon(){Weapon w=selected==6&&launcher_variant?rpgWeapon:weapons[selected];if(selected==6&&launcher_variant)w.reload=rpg_reload_ms.load();return w;}
bool locked_target(float*out){if(!lock_layout||!lock_target_layout||current.ins[0xC9]!=1)return false;float point[3];memcpy(point,current.ins+0xD0,12);for(float v:point)if(!isfinite(v))return false;const float*pos=(const float*)(current.phys+0x70);float range=dist(point,pos);if(!isfinite(range)||range<.2f||range>250.f)return false;memcpy(out,point,12);return true;}
std::atomic<uint32_t> grenade_recovery_ms{2400};uint64_t grenade_release_at=0,grenade_cycle_start=0;
std::atomic<uint32_t> ammo_cheat{0};std::atomic<uintptr_t> ammo_owner{0};std::atomic<uint64_t> ammo_requested_at{0};uintptr_t cheat_owner=0;bool no_reload=false;
uint64_t shot_at=0,reload_at=0,auto_reload_at=0,throw_at=0,pending_at=0;uint32_t ammo=30,sequence=0;bool released=false,reload_released=true,previous_trigger=false;uintptr_t owner=0;
void report(const char* name){FILE* f=igobygod::open(igobygod::log_path("combat.log").c_str(),"a");if(f){fprintf(f,"%llu %s ammo=%u sequence=%u gate=%d\n",GetTickCount64(),name,ammo,sequence,gameplay_gate_status());fclose(f);}}
struct Grenade {bool active,resting;uint64_t owner,born,last_step,support_at;uint32_t serial,block;float position[3],velocity[3],remainder;};
Grenade grenades[8]={};uint32_t grenade_serial=0,physics_block=0;uint64_t physics_owner=0;float previous_player_position[3]={};bool physics_valid=false;
struct LauncherFlight {bool active;uint64_t owner,born,last_step;uint32_t serial,block,variant,sequence;float position[3],velocity[3],remainder;};
LauncherFlight launcher_flights[8]={};uint32_t launcher_serial=0;
uint32_t launch_explosive(const float*from,const float*direction,unsigned variant){
 for(auto&g:launcher_flights)if(!g.active){g={};g.active=true;g.owner=*(uint64_t*)(current.ins+8);g.born=g.last_step=GetTickCount64();g.block=*(uint32_t*)(current.ins+0x6D0);g.serial=++launcher_serial;g.variant=variant;g.sequence=sequence+1;memcpy(g.position,from,12);
  float speed=variant?projectile_policy::rpg_speed:projectile_policy::launcher_speed;for(int i=0;i<3;i++)g.velocity[i]=direction[i]*speed;
  return 0x40000000u|(g.serial&0x3fffffffu);
 }return 0xffffffffu;
}
uint32_t launch_grenade(const float*from,const float*direction){
 for(auto&g:grenades)if(!g.active){g={};g.active=true;g.owner=*(uint64_t*)(current.ins+8);g.born=g.last_step=GetTickCount64();g.block=*(uint32_t*)(current.ins+0x6D0);g.serial=++grenade_serial;memcpy(g.position,from,12);
  // User requested a heavier throw: lower release speed/loft, stronger gravity,
  // and an inelastic surface response below. These are deliberate tuning values.
  float v[3]={direction[0],direction[1]+.12f,direction[2]};if(!unit(v)){g.active=false;return 0xffffffffu;}for(int i=0;i<3;i++)g.velocity[i]=v[i]*projectile_policy::grenade_speed;
  return 0x80000000u|(g.serial&0x7fffffffu);
 }return 0xffffffffu;
}
uint8_t* loaded_param(unsigned index,int id,unsigned size){
 auto rep=global(0x3D85F58);if(!readable(rep,0x88+0x48*index+8))return nullptr;
 auto cap=*(uint8_t**)(rep+0x88+0x48*index);if(!readable(cap,0x88))return nullptr;
 auto fd=*(uint8_t**)(cap+0x80);if(!readable(fd,0x88))return nullptr;
 auto file=*(uint8_t**)(fd+0x80);if(!readable(file,0x40))return nullptr;
 unsigned rows=*(uint16_t*)(file+0x0A);if(!rows||!readable(file+0x40,size_t(rows)*24))return nullptr;
 int lo=0,hi=int(rows)-1;while(lo<=hi){int mid=(lo+hi)/2;auto d=file+0x40+size_t(mid)*24;int rid=*(int*)d;
  if(rid==id){uint64_t offset=*(uint64_t*)(d+8);if(offset>0x10000000||offset+size<offset)return nullptr;auto p=file+offset;return readable(p,size)?p:nullptr;}
  if(rid<id)lo=mid+1;else hi=mid-1;
 }return nullptr;
}
void audit_params(int filter=-1){
 FILE*f=igobygod::open(igobygod::log_path("loaded-params.log").c_str(),"a");if(!f)return;
 for(auto&e:expected_params){if(filter>=0&&e.id!=filter)continue;auto row=loaded_param(e.index,e.id,e.size);uint32_t hash=2166136261u;if(row)for(unsigned i=0;i<e.size;i++)hash=(hash^row[i])*16777619u;
  fprintf(f,"%llu table=%u id=%d size=%u status=%s actual_hash=%08x shipped_hash=%08x repair_hash=%08x\n",GetTickCount64(),e.index,e.id,e.size,!row?"MISSING":hash==e.repair_hash?"REPAIR":hash==e.shipped_hash?"SHIPPED":"OTHER",row?hash:0,e.shipped_hash,e.repair_hash);
 }fclose(f);
}
bool custom_row_ready(unsigned index,int id){for(const auto&e:expected_params)if(e.index==index&&e.id==id){auto row=loaded_param(index,id,e.size);if(!row)return false;uint32_t hash=2166136261u;for(unsigned i=0;i<e.size;i++)hash=(hash^row[i])*16777619u;return hash==e.repair_hash;}return false;}
native_shot_observation::Ledger native_shot_ledger;
struct SeenNativeShot {uint64_t handle,time;};SeenNativeShot native_shots_seen[64]={};unsigned native_seen_cursor=0,native_aim_lines=0;
void observe_native_shot(uint64_t now,uint64_t handle,uint64_t bullet_owner,int id,float age,const float*p,const float*v){
 if(native_aim_lines>=128||!native_shot_observation::firearm(id)||!isfinite(age)||age<0||age>.25f)return;
 for(unsigned i=0;i<3;i++)if(!isfinite(p[i])||!isfinite(v[i]))return;
 // A newly created native bullet can still have zero velocity. Leave it unseen
 // until its initialized moving state can supply usable direction evidence.
 float speed=native_shot_observation::observed_speed(v);if(speed<1)return;
 for(const auto&seen:native_shots_seen)if(seen.time&&seen.handle==handle&&now>=seen.time&&now-seen.time<2000)return;
 native_shots_seen[native_seen_cursor++%64]={handle,now};
 auto match=native_shot_ledger.match(bullet_owner,id,now,age);const auto*r=match.request;
 float angle=r?native_shot_observation::nearest_angle(*r,v):-1.f;
 FILE*f=igobygod::open(igobygod::log_path("native-aim-observations.log").c_str(),"a");if(!f)return;
 native_aim_lines++;fprintf(f,"%llu bullet_handle=%llx owner=%llx bullet_id=%d age=%.5f position=(%.4f,%.4f,%.4f) actual_velocity=(%.4f,%.4f,%.4f) speed=%.4f candidate_requests=%u sequence=%u aim_source=%u expected_rays=%u angle_to_nearest_requested_ray_deg=%.5f association=%s native_velocity_observed=1 damage_verified=0\n",now,handle,bullet_owner,id,age,p[0],p[1],p[2],v[0],v[1],v[2],speed,match.candidates,r?r->sequence:0,r?r->source:0,r?r->count:0,angle,r?"unique_owner_id_birth_window":"unassociated");fclose(f);
}
bool fire(const Muzzle& v,RifleState& s,const ShoulderAimSnapshot& tickAim){
 int required=equipped_weapon().bullet;
 if(!custom_row_ready(10,required)||!custom_row_ready(8,required)||(selected>=6&&!custom_row_ready(10,required+1))||(selected>=6&&!custom_row_ready(8,required+1))){report("weapon parameters missing/mismatched; native spawn skipped");return false;}
 audit_params(equipped_weapon().bullet);
 auto cam=camera();if(!cam)return false;auto m=(const float*)(cam+0x10);float camera_origin[3]={},camera_direction[3]={};
 // CameraStep/shoulder task107 already wrote this frame's camera. Present also
 // publishes final_view, but that snapshot can be a frame behind during rotation.
 // Never replace the current game-thread camera with that asynchronous snapshot.
 // Q's camera still tracks its enemy. Only the independently movable reticle
 // changes the weapon ray, using the camera107 sequence claimed by this tick.
 bool nativeLocked=lock_layout&&current.ins[0xC9]==1;
 auto aimSource=rifle_aim::select((uintptr_t)current.ins,GetTickCount64(),nativeLocked,m,&tickAim,camera_origin,camera_direction);
 if(aimSource==rifle_aim::Source::Unavailable){report("current-frame aim unavailable; shot skipped");return false;}
 float from[3];memcpy(from,v.position,12);
 float chest[3];memcpy(chest,current.phys+0x70,12);chest[1]+=1.25f;
 if(dist(from,chest)>2.5f||dist(camera_origin,chest)>15.f)return false;
 float hit[3]={};bool blocked=false;if(!ray(chest,from,hit,blocked))return false;if(blocked)memcpy(from,chest,12);
 float direction[3]={camera_direction[0],camera_direction[1],camera_direction[2]};if(!unit(direction))return false;
 float target[3];for(int i=0;i<3;i++)target[i]=camera_origin[i]+direction[i]*equipped_weapon().range;
 // Native Q keeps its character lock; projectiles follow the reticle's ray.
 if(!ray(camera_origin,target,hit,blocked))return false;if(blocked)memcpy(target,hit,12);
 for(int i=0;i<3;i++)direction[i]=target[i]-from[i];if(!unit(direction))return false;
 // Native camera cast identifies the crosshair's world hit (map or character).
 // The muzzle has a separate route: cover between barrel and aim point must win.
 bool muzzle_blocked=false;float muzzle_hit[3]={};if(!ray(from,target,muzzle_hit,muzzle_blocked))return false;
 if(muzzle_blocked){memcpy(target,muzzle_hit,12);for(int i=0;i<3;i++)direction[i]=target[i]-from[i];if(!unit(direction))return false;}
 if(selected==6){projectile_policy::V velocity;float speed=launcher_variant?400.f:projectile_policy::launcher_speed,gravity=launcher_variant?.1962f:projectile_policy::launcher_gravity;
  if(projectile_policy::launch_velocity({from[0],from[1],from[2]},{target[0],target[1],target[2]},speed,gravity,velocity)){direction[0]=velocity.x/speed;direction[1]=velocity.y/speed;direction[2]=velocity.z/speed;}
  else{report("launcher target beyond ballistic range; unassisted arc");cheat_console_result("Beyond GL range: firing an unassisted arc");}
 }

 auto mgr=global(0x3D667A8);if(!readable(mgr,8))return false;
 Spawn d={};d.owner=*(uint64_t*)(current.ins+8);d.behaviorId=d.magicId=d.goodsId=d.dmyPolyId=-1;d.bulletId=equipped_weapon().bullet;d.target=~0ull;memcpy(d.fwd,direction,12);memcpy(d.pos,from,12);d.pos[3]=1;
 float up[3]={0,1,0};if(fabsf(direction[1])>.95f){up[0]=1;up[1]=0;}cross(up,direction,d.right);if(!unit(d.right))return false;cross(direction,d.right,d.up);
 uint32_t handle=0xffffffff,error=0;using SpawnFn=uint32_t*(__fastcall*)(void*,uint32_t*,const Spawn*,uint32_t*);capture_health(GetTickCount64(),true,true);
 native_shot_observation::Request observedRequest={};observedRequest.owner=d.owner;observedRequest.begin=GetTickCount64();observedRequest.sequence=sequence+1;observedRequest.source=unsigned(aimSource);observedRequest.bullet=d.bulletId;
 if(selected==7)handle=launch_grenade(from,direction);
 else if(selected==6)handle=launch_explosive(from,direction,launcher_variant);
 else if(selected==3){
  using namespace projectile_policy;V fwd={d.fwd[0],d.fwd[1],d.fwd[2]},right={d.right[0],d.right[1],d.right[2]},up={d.up[0],d.up[1],d.up[2]};
  for(unsigned pelletIndex=0;pelletIndex<8;pelletIndex++){Spawn shot=d;V aim=pellet(fwd,right,up,pelletIndex,sequence+1),side=norm(cross(up,aim)),vertical=cross(aim,side);memcpy(shot.fwd,&aim,12);memcpy(shot.right,&side,12);memcpy(shot.up,&vertical,12);uint32_t h=0xffffffff,e=0;((SpawnFn)(base+0x3A2CB0))(mgr,&h,&shot,&e);if(h!=0xffffffff){handle=h;memcpy(observedRequest.directions[observedRequest.count++],shot.fwd,12);}if(e)error=e;}
 }else{((SpawnFn)(base+0x3A2CB0))(mgr,&handle,&d,&error);if(handle!=0xffffffff){observedRequest.count=1;memcpy(observedRequest.directions[0],d.fwd,12);}}
 observedRequest.end=GetTickCount64();if(observedRequest.count)native_shot_ledger.add(observedRequest);
 auto bullet_row=loaded_param(10,d.bulletId,272);int loaded_attack_id=bullet_row?*(int*)bullet_row:-1;
 FILE*proof=igobygod::open(igobygod::log_path("native-spawn.log").c_str(),"a");if(proof){fprintf(proof,"%llu weapon=%u sequence=%u owner=%llx bullet_id=%d loaded_attack_id=%d native_handle=%08x error=%u simulated_flight=%u from=(%.3f,%.3f,%.3f) target=(%.3f,%.3f,%.3f) direction=(%.6f,%.6f,%.6f) aim_source=%u aim_sequence=%llu aim_ndc=(%.4f,%.4f)\n",GetTickCount64(),selected,sequence+1,d.owner,d.bulletId,loaded_attack_id,handle,error,selected>=6?1:0,from[0],from[1],from[2],target[0],target[1],target[2],direction[0],direction[1],direction[2],unsigned(aimSource),aimSource==rifle_aim::Source::FreeAim?tickAim.sequence:0,aimSource==rifle_aim::Source::FreeAim?tickAim.ndc[0]:0.f,aimSource==rifle_aim::Source::FreeAim?tickAim.ndc[1]:0.f);fclose(proof);}
 if(handle==0xffffffff)return false;native_scan_until=GetTickCount64()+5500;if(selected==6)launcher_probe(GetTickCount64(),handle,launcher_variant,target,250.f,false);
 ArsenalSpawnEvent ev={};ev.time=GetTickCount64();ev.sequence=sequence+1;ev.weapon=selected;ev.native_handle=handle;ev.simulated=selected>=6?1:0;ev.launcher_variant=launcher_variant;ev.bullet_id=d.bulletId;memcpy(ev.from,from,12);memcpy(ev.target,target,12);
 AcquireSRWLockExclusive(&state_lock);if(events.count==16){memmove(events.events,events.events+1,15*sizeof(ArsenalSpawnEvent));events.count=15;}events.events[events.count++]=ev;ReleaseSRWLockExclusive(&state_lock);
 memcpy(s.from,from,12);memcpy(s.to,target,12);if(selected!=7)native_sound_play(selected==1?1:selected==6?2:selected==2?4:selected==3?5:selected==5?6:0);return true;
}
void detonate_grenade(const Grenade&g,uint64_t now){
 native_scan_until=now+5500;
 // Native stationary blast, using only the existing signature-verified Spawn ABI.
 if(g.owner!=*(uint64_t*)(current.ins+8))return;auto mgr=global(0x3D667A8);if(!readable(mgr,8))return;
 Spawn d={};d.owner=g.owner;d.behaviorId=d.magicId=d.goodsId=d.dmyPolyId=-1;d.bulletId=98000101;d.target=~0ull;d.right[0]=d.up[1]=d.fwd[2]=d.pos[3]=1;memcpy(d.pos,g.position,12);
 uint32_t handle=0xffffffff,error=0;using SpawnFn=uint32_t*(__fastcall*)(void*,uint32_t*,const Spawn*,uint32_t*);((SpawnFn)(base+0x3A2CB0))(mgr,&handle,&d,&error);audit_params(98000101);
 FILE*f=igobygod::open(igobygod::log_path("grenade-fuse.log").c_str(),"a");if(f){fprintf(f,"%llu grenade=%u born=%llu fuse_age=%llu owner=%llx blast_id=98000101 native_handle=%08x error=%u pos=(%.3f,%.3f,%.3f)\n",now,g.serial,g.born,now-g.born,g.owner,handle,error,g.position[0],g.position[1],g.position[2]);fclose(f);}
}
void step_grenades(uint64_t now,bool valid,bool advance=true){
 uint64_t current_owner=valid?*(uint64_t*)(current.ins+8):0;uint32_t block=valid?*(uint32_t*)(current.ins+0x6D0):0;const float*p=valid?(float*)(current.phys+0x70):nullptr;
 bool recentered=valid&&physics_valid&&dist(p,previous_player_position)>25.f;
 if(!valid||current_owner!=physics_owner||block!=physics_block||recentered){memset(grenades,0,sizeof(grenades));physics_owner=current_owner;physics_block=block;}
 physics_valid=valid;if(valid)memcpy(previous_player_position,p,12);
 if(!valid)return;
 constexpr float dt=1.f/120.f;
 for(auto&g:grenades)if(g.active){
  if(!advance){if(now>=g.last_step){g.born+=now-g.last_step;g.last_step=now;}else g.active=false;continue;}
  if(g.owner!=current_owner||g.block!=block||now<g.last_step||dist(g.position,p)>200){g.active=false;continue;}
  if(now-g.last_step>250){g.born+=now-g.last_step;g.last_step=now;continue;}
  uint64_t until=now<g.born+4000?now:g.born+4000;g.remainder+=float(until-g.last_step)/1000.f;g.last_step=until;
  unsigned budget=30;
  if(g.resting&&now-g.support_at>=100){float supportEnd[3]={g.position[0],g.position[1]-.16f,g.position[2]},supportHit[3]={};bool support=false;
   if(!ray(g.position,supportEnd,supportHit,support,true,0x5D)||!support)g.resting=false;g.support_at=now;
  }
  if(g.resting)g.remainder=0;
  while(!g.resting&&g.remainder>=dt&&budget--){g.remainder-=dt;g.velocity[1]-=projectile_policy::grenade_gravity*dt;float end[3],hit[3]={};bool touched=false;for(int i=0;i<3;i++){g.velocity[i]*=expf(-.05f*dt);end[i]=g.position[i]+g.velocity[i]*dt;}
   if(!ray(g.position,end,hit,touched)){g.active=false;break;}
   if(!touched){memcpy(g.position,end,12);continue;}
   using namespace projectile_policy;V incoming={g.velocity[0],g.velocity[1],g.velocity[2]},fallback={};
   // Axis contacts supply a bounded fallback at narrow ledges where neighbouring
   // plane probes may miss. Only the normal component is ever reflected.
   for(int axis=0;axis<3;axis++){if(fabsf(end[axis]-g.position[axis])<1e-6f)continue;float probe[3]={g.position[0],g.position[1],g.position[2]},contact[3]={};probe[axis]=end[axis];bool blocked=false;
    if(!ray(g.position,probe,contact,blocked)){g.active=false;break;}if(blocked){float sign=g.velocity[axis]>0?-1.f:1.f;if(axis==0)fallback.x=sign;else if(axis==1)fallback.y=sign;else fallback.z=sign;}
   }
   if(!g.active)break;if(length(fallback)<.01f)fallback=mul(norm(incoming),-1.f);
   auto cast=[&](V a,V b,V&h,bool&t){float start[3]={a.x,a.y,a.z},finish[3]={b.x,b.y,b.z},out[3]={};bool ok=ray(start,finish,out,t);h={out[0],out[1],out[2]};return ok;};
   V contact={hit[0],hit[1],hit[2]},normal=contact_normal(contact,incoming,fallback,cast),velocity=bounce(incoming,normal),position=add(contact,mul(normal,grenade_radius));
   g.velocity[0]=velocity.x;g.velocity[1]=velocity.y;g.velocity[2]=velocity.z;g.position[0]=position.x;g.position[1]=position.y;g.position[2]=position.z;
   if(rest(velocity,normal)){float supportEnd[3]={g.position[0],g.position[1]-.16f,g.position[2]},supportHit[3]={};bool support=false;
    if(ray(g.position,supportEnd,supportHit,support,true,0x5D)&&support){g.resting=true;g.support_at=now;memset(g.velocity,0,sizeof(g.velocity));g.remainder=0;}
   }
  }
  if(g.active&&now>=g.born+4000){g.active=false;detonate_grenade(g,now);}
 }
}
void detonate_launcher(const LauncherFlight&g,uint64_t now){
 // This is the sole explosion spawn for the synthetic flight. There is no native
 // arrow parent and therefore no second, automatic HitBulletID child later.
 if(g.owner!=*(uint64_t*)(current.ins+8))return;int id=g.variant?98000701:98000301;
 if(!custom_row_ready(10,id)||!custom_row_ready(8,id)){report("launcher blast parameters changed; cancelled");return;}
 auto mgr=global(0x3D667A8);if(!readable(mgr,8))return;
 Spawn d={};d.owner=g.owner;d.behaviorId=d.magicId=d.goodsId=d.dmyPolyId=-1;d.bulletId=id;d.target=~0ull;d.right[0]=d.up[1]=d.fwd[2]=d.pos[3]=1;memcpy(d.pos,g.position,12);
 capture_health(now,true,true);uint32_t handle=0xffffffff,error=0;using SpawnFn=uint32_t*(__fastcall*)(void*,uint32_t*,const Spawn*,uint32_t*);
 ((SpawnFn)(base+0x3A2CB0))(mgr,&handle,&d,&error);native_scan_until=now+5500;
 FILE*f=igobygod::open(igobygod::log_path("launcher-contact.log").c_str(),"a");if(f){fprintf(f,"%llu sequence=%u flight=%u variant=%u age_ms=%llu owner=%llx blast_id=%d native_handle=%08x error=%u reason=swept_contact pos=(%.3f,%.3f,%.3f) velocity=(%.3f,%.3f,%.3f)\n",now,g.sequence,g.serial,g.variant,now-g.born,g.owner,id,handle,error,g.position[0],g.position[1],g.position[2],g.velocity[0],g.velocity[1],g.velocity[2]);fclose(f);}
}
void step_launchers(uint64_t now,bool valid,bool advance){
 if(!valid){memset(launcher_flights,0,sizeof(launcher_flights));return;}
 uint64_t current_owner=*(uint64_t*)(current.ins+8);uint32_t block=*(uint32_t*)(current.ins+0x6D0);
 constexpr float dt=1.f/120.f;
 for(auto&g:launcher_flights)if(g.active){
  if(g.owner!=current_owner||g.block!=block||now<g.last_step){g.active=false;continue;}
  if(!advance||now-g.last_step>250){g.born+=now-g.last_step;g.last_step=now;continue;}
  uint64_t expires=g.born+(g.variant?5000:30000),until=now<expires?now:expires;
  g.remainder+=float(until-g.last_step)/1000.f;g.last_step=until;unsigned budget=30;
  while(g.active&&g.remainder>=dt&&budget--){g.remainder-=dt;using namespace projectile_policy;
   V p={g.position[0],g.position[1],g.position[2]},v={g.velocity[0],g.velocity[1],g.velocity[2]};
   auto cast=[&](V a,V b,V&h,bool&t){float start[3]={a.x,a.y,a.z},end[3]={b.x,b.y,b.z},hit[3]={};bool ok=ray(start,end,hit,t,true);h={hit[0],hit[1],hit[2]};return ok;};
   auto result=advance_swept(p,v,g.variant?rpg_gravity:launcher_gravity,dt,cast);
   memcpy(g.position,&p,12);memcpy(g.velocity,&v,12);
   if(result==SweepResult::contact){g.active=false;detonate_launcher(g,now);}
   else if(result==SweepResult::unavailable){g.active=false;report("launcher physics unavailable; flight cancelled without explosion");}
  }
  // Lifespan is cleanup, never a fabricated midair impact. Only swept contact
  // above can create the damaging blast. Hand-grenade timed fuse is separate.
  if(g.active&&now>=expires){g.active=false;report("launcher flight expired without contact");}
 }
}
// Read-only HP observation, never damage application. A decrease near a shot
// is evidence of HP changing; it does not identify the damage source by itself.
struct HealthSample {uint64_t handle;int hp;uint64_t last_hit_by;};HealthSample hp_previous[64]={};
struct LauncherEvidence {uint64_t time,event,enemy,owner;unsigned variant;int baseline,last;float distance;unsigned team,model;};
LauncherEvidence launcher_evidence[24]={};unsigned evidence_cursor=0,evidence_events=0,evidence_lines=0;
void evidence_line(uint64_t now,const char* phase,uint64_t event,unsigned variant,uint64_t enemy,int before,int after,float distance,const char* result,uint64_t last_hit=0,unsigned team=0,unsigned model=0){
 if(evidence_lines++>=512)return;FILE*f=igobygod::open(igobygod::log_path("launcher-damage-test.log").c_str(),"a");if(!f)return;
 fprintf(f,"%llu phase=%s event=%llx variant=%u enemy=%llx hp_before=%d hp_now=%d distance=%.3f result=%s last_hit_by=%llx team=%u model=%u observed_hp_drop=%u actual_custom_weapon_damage_verified=0\n",now,phase,event,variant,enemy,before,after,distance,result,last_hit,team,model,before>after?1:0);fclose(f);
}
void launcher_probe(uint64_t now,uint64_t event,unsigned variant,const float* center,float radius,bool impact){
 if(evidence_events++>=64)return;
 struct Candidate{uint64_t handle;int hp;float distance;unsigned team,model;};Candidate chosen[3]={};unsigned n=0;
 auto w=typed(global(0x3D69FF8),0x2A4E980,0x1F1E8);if(!w)return;uintptr_t begin=(uintptr_t)*(uint8_t**)(w+0x1F1D8),end=(uintptr_t)*(uint8_t**)(w+0x1F1E0);
 if(end<begin||(end-begin)%16)return;size_t count=(end-begin)/16;if(count>64)count=64;if(!count||!readable((void*)begin,count*16))return;
 for(size_t i=0;i<count;i++){auto ins=typed(*(void**)(begin+i*16),0x2A47090,0x580);if(!ins||(ins[0x6C]!=6&&ins[0x6C]!=7))continue;
  auto mods=*(uint8_t**)(ins+0x190);if(!readable(mods,0x70))continue;auto data=typed(*(void**)mods,0x2A380B8,0x140);auto phys=typed(*(void**)(mods+0x68),0x2A3C890,0x7C);if(!data||!phys)continue;
  int hp=*(int*)(data+0x138),maximum=*(int*)(data+0x13C);const float*pos=(float*)(phys+0x70);if(hp<0||maximum<=0||hp>maximum||!isfinite(pos[0])||!isfinite(pos[1])||!isfinite(pos[2]))continue;
  float distance=dist(pos,center);if(distance>radius)continue;Candidate c={*(uint64_t*)(ins+8),hp,distance,ins[0x6C],*(uint32_t*)(ins+0x64)};unsigned limit=impact?3:1,j=0;while(j<n&&chosen[j].distance<=distance)j++;if(j>=limit)continue;if(n<limit)n++;for(unsigned k=n-1;k>j;k--)chosen[k]=chosen[k-1];chosen[j]=c;
 }
 if(!n){evidence_line(now,impact?"blast":"aim",event,variant,0,-1,-1,radius,impact?"no_enemy_center_within_blast_radius":"no_eligible_enemy_near_target");return;}
 for(unsigned i=0;i<n;i++){auto c=chosen[i];int baseline=c.hp;bool prior=false;for(auto&h:hp_previous)if(h.handle==c.handle){baseline=h.hp;prior=true;break;}
  evidence_line(now,impact?"blast":"aim",event,variant,c.handle,baseline,c.hp,c.distance,c.hp==0?(baseline>0?"observed_dead_after_prior_sample":"already_dead_target"):prior?"prior_hp_sample_available":"first_observation_only",0,c.team,c.model);
  if(impact&&c.hp>0){auto&e=launcher_evidence[evidence_cursor++%24];e={now,event,c.handle,*(uint64_t*)(current.ins+8),variant,baseline,c.hp,c.distance,c.team,c.model};}
 }
}
void launcher_results(uint64_t now,const HealthSample*samples,unsigned count){
 for(auto&e:launcher_evidence)if(e.time){if(now<e.time||e.owner!=*(uint64_t*)(current.ins+8)){e={};continue;}bool found=false;
  for(unsigned i=0;i<count;i++)if(samples[i].handle==e.enemy){found=true;int hp=samples[i].hp;if(hp<e.last){evidence_line(now,"after_blast",e.event,e.variant,e.enemy,e.last,hp,e.distance,"observed_hp_drop_not_exclusive_attribution",samples[i].last_hit_by,e.team,e.model);e.last=hp;}break;}
  if(now-e.time>=5000){evidence_line(now,"result",e.event,e.variant,e.enemy,e.baseline,e.last,e.distance,!found?"target_no_longer_observable":e.last<e.baseline?"hp_drop_observed":e.last==0?"dead_target":"no_hp_drop_observed_in_5s",0,e.team,e.model);e={};}
 }
}

void capture_health(uint64_t now,bool valid,bool force){
 CombatSpan timing(true);
 if(!valid){memset(hp_previous,0,sizeof(hp_previous));memset(launcher_evidence,0,sizeof(launcher_evidence));last_health_scan=0;return;}
 bool recent=shot_at&&now>=shot_at&&now-shot_at<5000;
 if(!force&&!recent&&last_health_scan&&now>=last_health_scan&&now-last_health_scan<200)return;
 last_health_scan=now;combat_cost.health_scans++;
 auto w=typed(global(0x3D69FF8),0x2A4E980,0x1F1E8);if(!w)return;
 uintptr_t begin=(uintptr_t)*(uint8_t**)(w+0x1F1D8),end=(uintptr_t)*(uint8_t**)(w+0x1F1E0);
 if(end<begin||(end-begin)%16)return;size_t count=(end-begin)/16;if(count>64)count=64;
 if(!count||!readable((void*)begin,count*16))return;
 HealthSample samples[64]={};unsigned n=0;
 for(size_t i=0;i<count;i++){
  combat_cost.enemies++;auto ins=typed(*(void**)(begin+i*16),0x2A47090,0x580);if(!ins||(ins[0x6C]!=6&&ins[0x6C]!=7))continue;
  auto mods=*(uint8_t**)(ins+0x190);if(!readable(mods,0x70))continue;
  auto data=typed(*(void**)mods,0x2A380B8,0x140);if(!data)continue;
  int hp=*(int*)(data+0x138),maximum=*(int*)(data+0x13C);if(hp<0||maximum<=0||hp>maximum)continue;
  uint64_t handle=*(uint64_t*)(ins+8);uint64_t last_hit_by=*(uint64_t*)(ins+0x180);samples[n++]={handle,hp,last_hit_by};
  if(now>=shot_at&&now-shot_at<5000)for(auto& old:hp_previous)if(old.handle==handle&&old.hp>hp){
   FILE* f=igobygod::open(igobygod::log_path("damage-observations.log").c_str(),"a");
   bool owner_match=last_hit_by==*(uint64_t*)(current.ins+8);bool player_hit=owner_match&&last_hit_by!=old.last_hit_by;
   if(f){fprintf(f,"%llu native enemy handle=%llx hp_before=%d hp_after=%d drop=%d last_shot_age_ms=%llu sequence=%u weapon=%u last_hit_by=%llx actual_enemy_hit_verified=%u actual_custom_weapon_damage_verified=0 attribution=%s\n",now,handle,old.hp,hp,old.hp-hp,now-shot_at,sequence,selected,last_hit_by,player_hit?1:0,player_hit?"native_player_last_hit_transition_with_hp_drop":owner_match?"player_last_hit_owner_matches_but_not_transition":"unverified");fclose(f);}break;
  }
 }
 launcher_results(now,samples,n);memcpy(hp_previous,samples,sizeof(samples));
}
void capture_world(uint64_t now,bool valid){
 CombatSpan timing(false);
 ArsenalWorld frame={};frame.time=now;
 if(valid)for(auto&g:grenades)if(g.active&&frame.projectile_count<16){auto&e=frame.projectiles[frame.projectile_count++];e.handle=(1ull<<63)|g.serial;e.bullet_id=98000100;e.weapon=7;e.age=float(now-g.born)/1000.f;e.radius=.08f;memcpy(e.position,g.position,12);memcpy(e.velocity,g.velocity,12);e.rotation[3]=1;e.simulated=1;}
 if(valid)for(auto&g:launcher_flights)if(g.active&&frame.projectile_count<16){auto&e=frame.projectiles[frame.projectile_count++];e.handle=(1ull<<62)|g.serial;e.bullet_id=g.variant?98000700:98000300;e.weapon=6;e.age=float(now-g.born)/1000.f;e.radius=.02f;memcpy(e.position,g.position,12);memcpy(e.velocity,g.velocity,12);e.rotation[3]=1;e.simulated=1;}

 if(valid){auto mgr=global(0x3D667A8);auto cam=camera();
  if(now<=native_scan_until&&cam&&readable(mgr,0x20)){combat_cost.world_scans++;auto bullet=*(uint8_t**)(mgr+8);unsigned budget=0;
   while(bullet&&budget++<256){
    combat_cost.bullets++;if(!readable(bullet,0xE30))break;auto next=*(uint8_t**)(bullet+0xE18);
    if(*(uint8_t**)(bullet+0xB38)==bullet&&*(uint64_t*)(bullet+0xB40)==*(uint64_t*)(current.ins+8)){
     int id=*(int*)(bullet+0xDD0);float age=*(float*)(bullet+0xB10);const float* p=(float*)(bullet+0x10);const float* v=(float*)(bullet+0x30);
     observe_native_shot(GetTickCount64(),*(uint64_t*)(bullet+8),*(uint64_t*)(bullet+0xB40),id,age,p,v);
     if((id==98000100||id==98000101||id==98000300||id==98000301||id==98000700||id==98000701)&&isfinite(age)&&age>=0&&age<30&&isfinite(p[0])&&isfinite(p[1])&&isfinite(p[2])&&dist(p,(float*)(cam+0x40))<300){
      uint64_t handle=*(uint64_t*)(bullet+8);unsigned weapon=id<98000300?7:6;
      if(id==98000101||id==98000301||id==98000701){bool seen=false;for(auto& e:impacts)if(e.handle==handle&&e.time&&now-e.time<30000)seen=true;
       if(!seen){auto& e=impacts[impact_cursor++%32];e={};e.handle=handle;e.time=now;e.bullet_id=id;e.weapon=weapon;e.radius=id==98000101?1.6f:2.5f;memcpy(e.position,p,12);float groundStart[3]={p[0],p[1]+.08f,p[2]},groundEnd[3]={p[0],p[1]-.25f,p[2]},groundHit[3]={};bool groundTouched=false;e.ground_contact=ray(groundStart,groundEnd,groundHit,groundTouched,true,0x5D)&&groundTouched?1:0;report("native explosion child observed");if(id==98000301||id==98000701)launcher_probe(now,handle,id==98000701?1:0,p,e.radius,true);audit_params(id);auto loaded=loaded_param(10,id,272);int attack_id=loaded?*(int*)loaded:-1;
       FILE*f=igobygod::open(igobygod::log_path("native-impact.log").c_str(),"a");if(f){fprintf(f,"%llu handle=%llx bullet_id=%d loaded_attack_id=%d owner=%llx age=%.3f pos=(%.3f,%.3f,%.3f) collision_child_observed=1 ground_contact=%u damage_verified=0\n",now,handle,id,attack_id,*(uint64_t*)(bullet+0xB40),age,p[0],p[1],p[2],e.ground_contact);fclose(f);}native_sound_play(3);}
      }else if(frame.projectile_count<16&&isfinite(v[0])&&isfinite(v[1])&&isfinite(v[2])){
       native_scan_until=now+5500;auto& e=frame.projectiles[frame.projectile_count++];e.handle=handle;e.bullet_id=id;e.weapon=weapon;e.age=age;e.radius=id==98000100?.08f:.02f;memcpy(e.position,p,12);memcpy(e.velocity,v,12);memcpy(e.rotation,bullet+0x20,16);
       for(float r:e.rotation)if(!isfinite(r)){frame.projectile_count--;break;}
      }
     }
    }
    if(next==bullet)break;bullet=next;
   }
  }
  for(auto& e:impacts)if(e.time&&now>=e.time&&now-e.time<5500)frame.impacts[frame.impact_count++]=e;
 }else{memset(impacts,0,sizeof(impacts));impact_cursor=0;native_scan_until=0;native_shot_ledger={};memset(native_shots_seen,0,sizeof(native_shots_seen));}
 AcquireSRWLockExclusive(&state_lock);world_shared=frame;ReleaseSRWLockExclusive(&state_lock);
}
void __fastcall tick(void*,const void*){
 region_count=0;game_thread=GetCurrentThreadId();native_melee_bind_game_thread();auto now=GetTickCount64();CombatTickCost timing(now);bool world_valid=player(current);bool valid=world_valid;bool lockOn=world_valid&&lock_layout&&current.ins[0xC9]==1;
 // Consume every camera sequence, even on ticks with no shot. An old frame
 // cannot become eligible later just because the trigger was finally pressed.
 ShoulderAimSnapshot tickAim={};auto tickCamera=world_valid?camera():nullptr;
 if(tickCamera&&!shoulder_camera_aim_for_tick((uintptr_t)current.ins,now,(const float*)(tickCamera+0x10),&tickAim))tickAim={};
 static bool audited=false;if(valid&&!audited){audit_params();audited=true;}if(valid&&gameplay_gate_status()==0)gameplay_gate_init();
 DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);const bool hudNormal=gameplay_hud_normal()!=0;valid=valid&&pid==GetCurrentProcessId()&&hudNormal;
 NativeMouseInput input={};native_input_take(&input);valid=valid&&input.ready;
 int action=-1;if(valid){auto mods=*(uint8_t**)(current.ins+0x190);auto ta=*(uint8_t**)(mods+0x18);if(readable(ta,0xC8)&&*(void**)(ta+8)==current.ins){auto idx=*(unsigned*)(ta+0xC4);if(idx<10)action=*(int*)(ta+0x20+idx*16);}}
 // Native roll/jump/interaction must complete before any rifle action.
 bool console=cheat_console_blocking()!=0;int baseAction=animation_layout?native_animation_policy::base(action):action;bool driving=world_valid&&car_controls_owned((uintptr_t)current.ins);bool can=valid&&!console&&!driving&&baseAction>=0&&baseAction<27000;
 // Read-only diagnostics: do not assume animation IDs encode a particular bank.
 uint32_t gateReason=!world_valid?1:pid!=GetCurrentProcessId()?2:!hudNormal?3:!input.ready?4:console?5:baseAction<0?6:baseAction>=27000?7:0;
 static uint64_t diagAt=0;static int oldAction=-2,oldLock=-1,oldReason=-1;static unsigned diagCount=0;
 if(diagCount<256&&now-diagAt>=250&&(oldAction!=action||oldLock!=int(lockOn)||oldReason!=int(gateReason))){diagAt=now;oldAction=action;oldLock=lockOn;oldReason=gateReason;diagCount++;FILE*f=igobygod::open(igobygod::log_path("lockon-gate.log").c_str(),"a");if(f){fprintf(f,"%llu owner=%p lock_layout=%d lock_on=%d raw_action=%d base_action=%d bank_layout=%d gate_reason=%u action_gate_ready=%d\n",now,current.ins,lock_layout,lockOn,action,baseAction,animation_layout,gateReason,can);fclose(f);}}

 if(!world_valid||cheat_owner!=(uintptr_t)current.ins){no_reload=false;grenade_release_at=grenade_cycle_start=grenade_ready_at=0;switch_reload=false;cheat_owner=world_valid?(uintptr_t)current.ins:0;}
 auto cheat=ammo_cheat.exchange(0);auto requestedAt=ammo_requested_at.load();if(cheat&&valid&&ammo_owner.load()==(uintptr_t)current.ins&&now>=requestedAt&&now-requestedAt<250){if(cheat==3){for(unsigned i=0;i<8;i++){magazines[i]=weapons[i].clip;reserves[i]=weapons[i].clip*5;}ammo=magazines[selected];reload_at=auto_reload_at=grenade_ready_at=0;switch_reload=false;cheat_console_result("MAXAMMO: all magazines filled");}else if(cheat==4){no_reload=!no_reload;if(no_reload){for(unsigned i=0;i<8;i++)magazines[i]=weapons[i].clip;ammo=magazines[selected];reload_at=auto_reload_at=0;}cheat_console_result(no_reload?"NORELOAD ON":"NORELOAD OFF");}}else if(cheat)cheat_console_result("Ammunition cheat rejected: gameplay unavailable");
 step_grenades(now,world_valid,world_valid&&hudNormal);
 step_launchers(now,world_valid,world_valid&&hudNormal);
 bool trigger=GetAsyncKeyState(VK_LBUTTON)<0,aim=GetAsyncKeyState(VK_RBUTTON)<0,reload=GetAsyncKeyState('R')<0;
 RifleState s={};Muzzle muzzle={};AcquireSRWLockShared(&state_lock);s=shared;muzzle=visual;ReleaseSRWLockShared(&state_lock);
 if(!valid||owner!=(uintptr_t)current.ins){reload_at=auto_reload_at=0;throw_at=0;pending_at=0;released=false;wheel_was=false;wheel_x=wheel_y=0;owner=valid?(uintptr_t)current.ins:0;}
 bool wheel=can&&GetAsyncKeyState(VK_TAB)<0;
 if(wheel){
  if(!wheel_was){hover=selected;wheel_x=wheel_y=0;launcherScroll.variant=launcher_variant;launcherScroll.reset();meleeScroll.variant=melee_variant;meleeScroll.reset();}
  hover=arsenal_wheel::cursor_update(wheel_x,wheel_y,float(input.dx),float(input.dy),hover);
  for(unsigned i=0;i<8;i++)if(GetAsyncKeyState('1'+i)<0)hover=(i+1)%8;
  launcherScroll.update(input.wheel,true,hover);
  meleeScroll.update(input.wheel,true,hover);
 }
 if(wheel_was&&!wheel&&can){bool changed=ammo_policy::changed(selected,hover,launcher_variant,launcherScroll.variant);magazines[selected]=ammo;selected=hover;launcher_variant=launcherScroll.variant;melee_variant=meleeScroll.variant;ammo=magazines[selected];reload_at=auto_reload_at=0;throw_at=0;pending_at=0;released=false;shot_at=now;s.shot_time=0;if(changed)switch_reload=true;}
 if(!wheel){launcherScroll.reset();meleeScroll.reset();if(!can){launcherScroll.variant=launcher_variant;meleeScroll.variant=melee_variant;}}
 wheel_was=wheel;
 if(!world_valid||melee_owner!=(uintptr_t)current.ins){melee_state={};melee_owner=world_valid?(uintptr_t)current.ins:0;}
 // Reconcile through verified native equipment calls only during a safe native
 // action. A confirmed melee mode persists through its attack/roll animations.
 if(can&&!wheel&&(selected==0||melee_lease_active)){
  unsigned desired=selected==0?(melee_variant?2:1):0;if(desired)melee_lease_active=true;
  int ok=native_melee_select((uintptr_t)current.ins,desired,&melee_state);
  static int previousMeleeStatus=-100;static unsigned previousMeleeMode=~0u;
  if(previousMeleeStatus!=melee_state.status||previousMeleeMode!=melee_state.mode){
   char message[192];snprintf(message,sizeof(message),"MELEE_EQUIP desired=%u mode=%u status=%d item=%u slot=%u",desired,melee_state.mode,melee_state.status,melee_state.item,melee_state.slot);report(message);
   if(!ok&&melee_state.status!=-10)cheat_console_result(melee_state.status==-8?"Sword: switch to right-hand grip first":"Melee unavailable: equipment check failed");
   previousMeleeStatus=melee_state.status;previousMeleeMode=melee_state.mode;
  }
  if(ok&&!desired)melee_lease_active=false;
 }
 const auto& w=equipped_weapon();
 if(!trigger)released=true;
 if(!can||wheel){if(reload_at)switch_reload=true;reload_at=auto_reload_at=0;throw_at=0;pending_at=0;released=!trigger;}
 if(can&&!wheel&&switch_reload){switch_reload=false;if(!no_reload&&ammo<w.clip&&reserves[selected]){if(selected==7){grenade_ready_at=ammo_policy::recovered(now,grenade_cycle_start,grenade_recovery_ms.load())?now:grenade_cycle_start+grenade_recovery_ms.load();}else reload_at=now;}}
 if(can&&!wheel&&!no_reload&&!auto_reload_at&&reload&&reload_released&&ammo<w.clip&&w.clip&&reserves[selected]){if(selected==7)grenade_ready_at=ammo_policy::recovered(now,grenade_cycle_start,grenade_recovery_ms.load())?now:grenade_cycle_start+grenade_recovery_ms.load();else reload_at=now;report("reload");}
 reload_released=valid&&!reload;
 if(can&&!wheel&&!no_reload&&!ammo&&reserves[selected]&&selected==6&&launcher_variant&&!reload_at&&!auto_reload_at){auto_reload_at=shot_at+rpg_fire_ms.load();}
 if(auto_reload_at&&can&&!wheel&&now>=auto_reload_at){reload_at=now;auto_reload_at=0;}
 if(reload_at&&now>=reload_at&&now-reload_at>=w.reload){ammo=ammo_policy::transfer(ammo,w.clip,reserves[selected]);magazines[selected]=ammo;reload_at=0;}
 if(selected==7&&can&&!wheel&&!no_reload&&!ammo&&reserves[7]&&!grenade_ready_at&&!throw_at)grenade_ready_at=ammo_policy::recovered(now,grenade_cycle_start,grenade_recovery_ms.load())?now:grenade_cycle_start+grenade_recovery_ms.load();
 if(selected==7&&can&&!wheel&&grenade_ready_at&&now>=grenade_ready_at&&!throw_at){if(!no_reload)ammo=ammo_policy::transfer(ammo,1,reserves[7]);magazines[7]=ammo;grenade_ready_at=0;}
 s.active=can&&!wheel&&selected!=0;
 if(s.active&&trigger&&!previous_trigger&&released&&!reload_at&&ammo)pending_at=now;
 previous_trigger=trigger;
 if(pending_at&&now-pending_at>350)pending_at=0;
 if(!s.active)pending_at=0;
 s.aiming=s.active&&(aim||trigger||reload_at||auto_reload_at||throw_at||pending_at);s.reload_start=reload_at;
 bool fresh=muzzle.owner==owner&&now>=muzzle.time&&now-muzzle.time<=100;
 if(s.active&&selected==7&&pending_at&&!reload_at&&ammo&&!throw_at&&ammo_policy::recovered(now,grenade_cycle_start,grenade_recovery_ms.load())&&fresh&&muzzle.ready){throw_at=now;pending_at=0;released=false;}
 bool throwing=selected==7&&throw_at&&now>=throw_at&&now-throw_at>=336;
 bool wants_fire=selected==7?throwing:(pending_at||(!w.semi&&trigger&&released));
 if(s.active&&wants_fire&&!reload_at&&ammo&&now-shot_at>=w.interval&&fresh&&muzzle.ready){if(fire(muzzle,s,tickAim)){pending_at=0;shot_at=now;s.shot_time=now;if(!no_reload)ammo--;magazines[selected]=ammo;sequence++;if(selected==7){grenade_release_at=now;grenade_cycle_start=throw_at;grenade_ready_at=grenade_cycle_start+grenade_recovery_ms.load();throw_at=0;}if(w.semi)released=false;if(!no_reload&&!ammo&&reserves[selected]&&selected==6){if(launcher_variant)auto_reload_at=now+rpg_fire_ms.load();else reload_at=now;}s.effect_kind=selected>=6?2:1;report("shot accepted");}else{released=false;pending_at=0;throw_at=0;report("shot rejected");}}
 s.time=now;s.ammo=ammo;s.sequence=sequence;s.weapon=selected;s.wheel_open=wheel;s.wheel_hover=hover;s.capacity=w.clip;s.reserve=reserves[selected];s.throw_start=throw_at;s.prop_hidden=selected==7&&(!ammo||(!throw_at&&grenade_release_at&&now>=grenade_cycle_start&&now-grenade_cycle_start<grenade_recovery_ms.load()))?1:0;s.grenade_release_time=grenade_release_at;s.no_reload=no_reload?1:0;s.lock_on=lockOn?1:0;s.native_action=action;s.gate_reason=gateReason;s.launcher_variant=wheel?launcherScroll.variant:launcher_variant;s.lock_target_valid=world_valid&&locked_target(s.lock_target)?1:0;
 s.melee_variant=wheel?meleeScroll.variant:melee_variant;s.melee_mode=melee_state.mode;s.melee_left_hand=melee_state.left_hand;s.melee_status=melee_state.status;
 // A failed native equip cannot fall through to the previous sword/crossbow.
 bool blockNative=driving||(selected==0?(wheel||melee_state.mode==0):(fresh||wheel));
 native_input_set_context(valid&&blockNative?1:0,wheel?1:0);
 capture_world(now,world_valid);
 capture_health(now,world_valid);
 AcquireSRWLockExclusive(&state_lock);shared=s;ReleaseSRWLockExclusive(&state_lock);region_count=0;
}
void* vtbl[3]={(void*)&no_class,(void*)&no_delete,(void*)&tick};Task task={vtbl,0,{}};
}
extern "C" int rifle_state(RifleState* out){if(!out||!TryAcquireSRWLockShared(&state_lock))return 0;*out=shared;ReleaseSRWLockShared(&state_lock);return GetTickCount64()-out->time<150;}
extern "C" void rifle_muzzle(uintptr_t owner,uint64_t time,const float* p,int ready){if(!p)return;for(int i=0;i<3;i++)if(!isfinite(p[i]))return;AcquireSRWLockExclusive(&state_lock);visual.owner=owner;visual.time=time;visual.ready=ready?1:0;memcpy(visual.position,p,12);ReleaseSRWLockExclusive(&state_lock);}
extern "C" void rifle_start(){
 base=(uintptr_t)GetModuleHandleA(nullptr);
 native_melee_init();
 lock_layout=sig(0x3F42B0,"0F B6 81 C9 00 00 00 C3");
 lock_target_layout=sig(0x3DBEBA,"0F 10 81 D0 00 00 00 0F 29 44 24 40 F3 0F 10 76 08 E8 E0 83 01 00")&&sig(0x3F7AD6,"F3 0F 10 81 D0 00 00 00 F3 0F 58 02 F3 0F 11 81 D0 00 00 00");
 animation_layout=sig(0x1B4090,"48 83 EC 18 C7 04 24 00 00 00 00 44 8B CA 83 FA FF 74 1C B8 83 DE 1B 43 41 B8 01 00 00 00 41 F7 E1 C1 EA 12 69 C2 40 42 0F 00 44 2B C8");
 if(!sig(0xEB3E50,"48 89 5C 24 08 57 48 83 EC 40 48 8D 4C 24 20 49 8B D8 8B FA E8")||!sig(0xC71E00,"40 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 70 FF FF FF 48 81 EC 90 01 00 00")||!sig(0x3A2CB0,"40 53 55 56 57 48 81 EC 98 07 00 00")){report("signature failure");return;}
 uint8_t* imp=nullptr;for(int i=0;i<360&&!imp;i++){region_count=0;imp=typed(global(0x458FE88),0x2C04B00,8);if(!imp)Sleep(500);}if(!imp)return;
 if(!native_input_install()){report("input unavailable");return;}
 static unsigned char pcm[7][441000];const char* samples[7]={"rifle.pcm","pistol.pcm","launcher.pcm","explosion.pcm","smg.pcm","shotgun.pcm","sniper.pcm"};
 for(int i=0;i<7;i++){auto path=igobygod::data_path(std::string("audio/")+samples[i]);FILE* f=igobygod::open(path.c_str(),"rb");if(f){auto n=fread(pcm[i],1,sizeof(pcm[i]),f);fclose(f);native_sound_sample(i,pcm[i],(uint32_t)n);}else report("sound sample missing");}
 native_sound_init();
 HMODULE pinned=nullptr;if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCSTR)&rifle_start,&pinned))return;
 using Register=bool(__fastcall*)(void*,uint32_t,void*);report(((Register)(base+0xEB3E50))(imp,117,&task)?"registered":"registration failed");
}

extern "C" int rifle_events(ArsenalEvents* out){if(!out||!TryAcquireSRWLockShared(&state_lock))return 0;*out=events;ReleaseSRWLockShared(&state_lock);return 1;}

extern "C" int rifle_world(ArsenalWorld* out){if(!out||!TryAcquireSRWLockShared(&state_lock))return 0;*out=world_shared;ReleaseSRWLockShared(&state_lock);return GetTickCount64()-out->time<150;}

extern "C" void rifle_view(uintptr_t owner,uint64_t time,const float* origin,const float* direction,int ready){if(!ready){AcquireSRWLockExclusive(&state_lock);final_view.ready=0;ReleaseSRWLockExclusive(&state_lock);return;}if(!origin||!direction)return;View v={};v.owner=owner;v.time=time;v.ready=ready?1:0;for(int i=0;i<3;i++){if(!isfinite(origin[i])||!isfinite(direction[i]))return;v.origin[i]=origin[i];v.direction[i]=direction[i];}if(!unit(v.direction))return;AcquireSRWLockExclusive(&state_lock);final_view=v;ReleaseSRWLockExclusive(&state_lock);}

extern "C" void rifle_cheat_request(uint32_t kind,uintptr_t owner){if(kind==3||kind==4){ammo_owner.store(owner);ammo_requested_at.store(GetTickCount64());ammo_cheat.store(kind);}}

extern "C" void rifle_grenade_recovery(uint32_t duration_ms){if(duration_ms>=336&&duration_ms<=10000)grenade_recovery_ms.store(duration_ms);}

extern "C" void rifle_rpg_reload_duration(uint32_t duration_ms){if(duration_ms>=100&&duration_ms<=10000)rpg_reload_ms.store(duration_ms);}
extern "C" void rifle_rpg_fire_duration(uint32_t duration_ms){if(duration_ms>=100&&duration_ms<=3000)rpg_fire_ms.store(duration_ms);}
