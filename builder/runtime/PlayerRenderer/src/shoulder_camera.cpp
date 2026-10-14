// Supported executable is fingerprinted in lib.rs. Camera/task/raycast layouts
// match the locally verified ER reference. ER still supplies orbit input/rotation.
#include <windows.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <atomic>
#include "rifle.h"
#include "player_status.h"
#include "../car-proof/vehicle_demo.h"
#include "../car-drive/camera_policy.h"
#include "cheat_console.h"
#include "shoulder_camera.h"
#include "gameplay_gate.h"
#include "native_animation_policy.h"
#include "shoulder_aim_policy.h"
#include "lock_camera_framing.h"
#include "ground_contact_policy.h"
#include "camera_overlay_policy.h"
#include "../polish-fix/camera/shake.h"
#include "../polish-next/camera/policy.h"
namespace {
uintptr_t base=0;
SRWLOCK lock=SRWLOCK_INIT;
uintptr_t renderOwner=0;uint64_t renderTime=0;float scopeShared=0;camera_shake::Queue shakes;float groundShared[3]={},groundHeight=0;uintptr_t groundOwner=0;uint64_t groundTime=0;
struct Span{uintptr_t a,b;DWORD protection;};Span spans[64];unsigned spanCount=0;bool caching=false;unsigned queries=0,rays=0;
bool readableProtection(DWORD p){return p&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY);}
bool writableProtection(DWORD p){return p&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY);}
bool accessible(const void*p,size_t n,bool writing){uintptr_t a=(uintptr_t)p,end=a+n;if(a<65536||end<a)return false;while(a<end){if(caching)for(unsigned i=0;i<spanCount;i++)if(a>=spans[i].a&&end<=spans[i].b&&(!writing||writableProtection(spans[i].protection)))return true;MEMORY_BASIC_INFORMATION m={};queries++;if(!VirtualQuery((void*)a,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!readableProtection(m.Protect)||(writing&&!writableProtection(m.Protect)))return false;auto next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=a)return false;if(caching&&spanCount<64)spans[spanCount++]={(uintptr_t)m.BaseAddress,next,m.Protect};a=next;}return true;}
bool read(const void*p,size_t n){return accessible(p,n,false);}bool writeable(const void*p,size_t n){return accessible(p,n,true);}
std::atomic<int> inputBlocked{0},scrollDelta{0},scrollReady{0};float zoomTarget=14,zoomFov=14,zoomShared=14;
std::atomic<int> manualAimReady{0},mouseDx{0},mouseDy{0};std::atomic<uint64_t> aimInputTime{0};
shoulder_aim::Offset manualAim;ShoulderAimSnapshot aimShared={};uint64_t aimSequence=0,aimClaimed=0;DWORD aimThread=0;
void invalidateAim(){AcquireSRWLockExclusive(&lock);aimShared.ready=0;ReleaseSRWLockExclusive(&lock);}
void publishAim(uintptr_t owner,uint64_t now,const float*view,float fov,float aspect,bool locked,bool scoped){ShoulderAimSnapshot next={};shoulder_aim::make(next,owner,now,++aimSequence,view,fov,aspect,manualAim,locked,scoped);AcquireSRWLockExclusive(&lock);aimShared=next;aimThread=GetCurrentThreadId();ReleaseSRWLockExclusive(&lock);}
LARGE_INTEGER frequency={};double stamp(){LARGE_INTEGER t;QueryPerformanceCounter(&t);return double(t.QuadPart)/double(frequency.QuadPart);}
struct Timing{double start,statusEnd,validationEnd,collisionEnd,groundMs=0;uint64_t now;Timing(){if(!frequency.QuadPart)QueryPerformanceFrequency(&frequency);start=statusEnd=validationEnd=collisionEnd=stamp();now=GetTickCount64();spanCount=queries=rays=0;caching=true;}~Timing(){caching=false;static double total=0,status=0,validation=0,collision=0,ground=0,maximum=0;static unsigned count=0,q=0,r=0;static uint64_t last=0;double elapsed=(stamp()-start)*1000;total+=elapsed;status+=(statusEnd-start)*1000;validation+=(validationEnd-statusEnd)*1000;collision+=(collisionEnd-validationEnd)*1000;ground+=groundMs;maximum=fmax(maximum,elapsed);count++;q+=queries;r+=rays;if(!last)last=now;if(now-last>=5000){char b[240];snprintf(b,sizeof(b),"PERF camera ticks=%u avg_ms=%.3f max_ms=%.3f status_ms=%.3f validate_ms=%.3f collision_ms=%.3f ground_ms=%.3f vquery=%u rays=%u",count,total/count,maximum,status/count,validation/count,collision/count,ground/count,q,r);reportTiming(b);last=now;total=status=validation=collision=ground=maximum=0;count=q=r=0;}}static void reportTiming(const char*);};
uint8_t* typed(void*p,uintptr_t vt,size_t n){return read(p,n)&&*(uintptr_t*)p==base+vt?(uint8_t*)p:nullptr;}
uint8_t* global(uintptr_t r){auto p=(uint8_t**)(base+r);return read(p,8)?*p:nullptr;}
void report(const char*s){FILE*f=igobygod::open(igobygod::log_path("camera.log").c_str(),"a");if(f){fprintf(f,"%llu %s\n",GetTickCount64(),s);fclose(f);}}
void Timing::reportTiming(const char*s){report(s);}
bool sig(uintptr_t r,const unsigned char*p,size_t n){return read((void*)(base+r),n)&&!memcmp((void*)(base+r),p,n);}
bool normalize(float*v){float l=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);if(!isfinite(l)||l<.001f)return false;for(int i=0;i<3;i++)v[i]/=l;return true;}
void*rayWorld(){auto h=global(0x3D7A0D0);if(!read(h,0xA0))return nullptr;auto world=*(void**)(h+0x98);return read(world,8)?world:nullptr;}
bool trace(void*world,uint8_t*player,const float*a,const float*b,float*out,bool&hit){if(!world)return false;rays++;alignas(16)float o[4]={a[0],a[1],a[2],1},d[4]={b[0]-a[0],b[1]-a[1],b[2]-a[2],0},r[4]={};using Ray=bool(__fastcall*)(void*,uint32_t,const float*,const float*,float*,void*);hit=((Ray)(base+0xC71E00))(world,0x2000058,o,d,r,player);if(hit){for(int i=0;i<3;i++){if(!isfinite(r[i]))return false;out[i]=r[i];}}return true;}
ground_contact::Cache groundProbe;
void sampleGround(uint8_t*player,const float*pos,uint64_t now,bool force=false,void*world=nullptr){
 uintptr_t owner=(uintptr_t)player;
 if(groundProbe.due(owner,now,force)){
  float a[3]={pos[0],pos[1]+.2f,pos[2]},b[3]={pos[0],pos[1]-4.f,pos[2]},point[3]={};bool hit=false;
  bool okay=trace(world?world:rayWorld(),player,a,b,point,hit)&&hit;
  groundProbe.store(owner,now,okay,point,okay?fmaxf(0.f,pos[1]-point[1]):0.f);
 }
 // Republish the cached result between 80 ms probes, retaining its real age.
 AcquireSRWLockExclusive(&lock);groundOwner=groundProbe.valid(owner,now)?owner:0;groundTime=groundProbe.time;
 if(groundOwner){memcpy(groundShared,groundProbe.point,12);groundHeight=groundProbe.height;}ReleaseSRWLockExclusive(&lock);
}
camera_overlay::HeldView overlayView;
car_camera::Chase carChase;
uintptr_t cameraOwner=0;uint64_t lastTime=0;float aimBlend=0,scopeBlend=0,distanceSmoothed=3.6f;float lastPlayer[3]={};
void inactive(uint64_t now,bool preserveGround=false){invalidateAim();overlayView.reset();manualAimReady=0;mouseDx=mouseDy=0;manualAim.reset();scrollReady=0;scrollDelta=0;cameraOwner=0;lastTime=now;aimBlend=scopeBlend=0;if(!preserveGround)groundProbe.reset();AcquireSRWLockExclusive(&lock);scopeShared=0;if(!preserveGround)groundOwner=0;shakes.clear();ReleaseSRWLockExclusive(&lock);float zero[3]={},forward[3]={0,0,1};rifle_view(0,now,zero,forward,0);}
void __fastcall tick(void*,const void*){
 Timing timing;player_status_tick();car_tick();timing.statusEnd=timing.validationEnd=timing.collisionEnd=stamp();
 auto now=GetTickCount64();uintptr_t renderer=0;uint64_t heartbeat=0;AcquireSRWLockShared(&lock);renderer=renderOwner;heartbeat=renderTime;ReleaseSRWLockShared(&lock);
 auto w=typed(global(0x3D69FF8),0x2A4E980,0x1E510);auto cs=global(0x3D669C8);
 if(!w||!read(cs,16)){inactive(now);return;}auto p=typed(*(void**)(w+0x1E508),0x2A7FBB0,0x6D4);auto cam=typed(*(void**)(cs+8),0x2AA0A40,0x60);
 if(!p||!cam||!writeable(cam+0x10,0x44)||renderer!=(uintptr_t)p||now<heartbeat||!camera_policy::heartbeat(now,heartbeat,cameraOwner==(uintptr_t)p)){inactive(now);return;}
 auto mods=*(uint8_t**)(p+0x190);if(!read(mods,0x70)){inactive(now);return;}auto hp=typed(*(void**)mods,0x2A380B8,0x140);auto phys=typed(*(void**)(mods+0x68),0x2A3C890,0x2E8);
 if(!hp||!phys||*(int*)(hp+0x138)<=0||(p[0x1C5]&0x80)||gameplay_hud_normal()!=1){inactive(now);return;}
 CarSnapshot car={};if(car_snapshot(&car)&&car.driving&&car.owner==(uintptr_t)p){
  inactive(now);car_camera::Frame view;if(!car_camera::frame(car.chassis,view)){carChase={};return;}
  timing.validationEnd=timing.collisionEnd=stamp();void*world=rayWorld();bool verified=true;float allowed=view.range;
  // Fixed world-up chase: terrain pitch/roll never rotates the horizon.
  // Three bounded parallel rays cover the centre and lateral camera clearance.
  const float offsets[3]={0,-.25f,.25f};
  for(float offset:offsets){float a[3],b[3],hitPoint[3]={};for(int i=0;i<3;i++){a[i]=view.pivot[i]+view.right[i]*offset;b[i]=view.wish[i]+view.right[i]*offset;}bool hit=false;
   if(!trace(world,p,a,b,hitPoint,hit)){verified=false;break;}
   if(hit){float d=0;for(int i=0;i<3;i++)d+=(hitPoint[i]-a[i])*view.direction[i];allowed=fminf(allowed,fmaxf(.05f,d-.3f));}
  }
  float range=carChase.resolve(car.owner,car.spawn_sequence,now,view,allowed,verified),m[16]={};
  for(int i=0;i<3;i++){m[i]=view.right[i];m[4+i]=view.up[i];m[8+i]=-view.direction[i];m[12+i]=view.pivot[i]+view.direction[i]*range;}m[15]=1;
  timing.collisionEnd=stamp();
  memcpy(cam+0x10,m,sizeof(m));*(float*)(cam+0x50)=60.f*.01745329251994f;return;
 }
 carChase={};
 RifleState s={};bool fresh=rifle_state(&s)!=0;DWORD foregroundPid=0;GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);bool focused=foregroundPid==GetCurrentProcessId();if(!fresh||!focused){inactive(now);return;}
 // The cheat console and wheel capture controls only. Calling inactive here
 // used to discard the shoulder/zoom/lock offsets and expose ER's centred cam.
 bool modal=inputBlocked.load()||cheat_console_blocking()||s.wheel_open;
 if(modal){
  invalidateAim();manualAimReady=0;mouseDx=mouseDy=0;scrollReady=0;scrollDelta=0;
  float held[16],angle=0;auto pos=(const float*)(phys+0x70);
  if(cameraOwner==(uintptr_t)p&&overlayView.restore((uintptr_t)p,pos,held,angle)){
   memcpy(cam+0x10,held,sizeof(held));*(float*)(cam+0x50)=angle;lastTime=now;memcpy(lastPlayer,pos,12);
   rifle_view(0,now,nullptr,nullptr,0);return;
  }
  inactive(now);return;
 }
 if(!s.weapon){
  float meleePos[3];memcpy(meleePos,phys+0x70,12);for(float x:meleePos)if(!isfinite(x)){inactive(now);return;}
  // Melee retains ER's camera, but the visible replacement still needs ground
  // contact. Keep the previous publication until the bounded probe completes.
  inactive(now,true);double groundStart=stamp();sampleGround(p,meleePos,now);timing.groundMs=(stamp()-groundStart)*1000;return;
 }
 if(!native_animation_policy::camera_action(s.native_action)&&!native_animation_policy::rpg_weapon_action(s.native_action,s.weapon)){
  invalidateAim();manualAimReady=0;mouseDx=mouseDy=0;manualAim.reset();scrollReady=0;scrollDelta=0;aimBlend=scopeBlend=0;lastTime=now;
  AcquireSRWLockExclusive(&lock);scopeShared=0;groundOwner=0;shakes.clear();ReleaseSRWLockExclusive(&lock);
  rifle_view(0,now,nullptr,nullptr,0);return;
 }
 // Weapon controls remain gated by s.active in rifle.cpp; the camera must
 // continue through native jump/roll actions so RMB free-aim stays responsive.
 bool mouseAim=GetAsyncKeyState(VK_RBUTTON)<0;bool aim=mouseAim;bool scoped=mouseAim&&s.weapon==5;
 bool locked=p[0xC9]!=0; // Locally verified player lock-on getter at RVA3F42B0.
 bool manualAimActive=locked;
 manualAimReady=manualAimActive?1:0;aimInputTime=now;
 if(!manualAimActive){mouseDx=mouseDy=0;manualAim.reset();}
 float m[16];memcpy(m,cam+0x10,64);for(float x:m)if(!isfinite(x)){inactive(now);return;}
 float aspect=*(float*)(cam+0x54);if(!isfinite(aspect)||aspect<.5f||aspect>4.f){inactive(now);return;}
 float pos[3];memcpy(pos,phys+0x70,12);for(float x:pos)if(!isfinite(x)){inactive(now);return;}
 float dt=lastTime&&now>=lastTime?float(now-lastTime)*.001f:0;float displacement=0;for(int i=0;i<3;i++)displacement+=(pos[i]-lastPlayer[i])*(pos[i]-lastPlayer[i]);static bool previousLocked=false;bool reset=cameraOwner!=(uintptr_t)p||now<lastTime||displacement>9||previousLocked!=locked;if(!reset&&dt<=0)return;dt=fminf(.1f,fmaxf(0.f,dt));scrollReady=scoped?1:0;if(scoped){int wheel=scrollDelta.exchange(0);zoomTarget=camera_policy::zoom(zoomTarget,wheel);}else scrollDelta=0;zoomFov+=(zoomTarget-zoomFov)*(1-expf(-dt*14.f));
 if(reset){manualAim.reset();mouseDx=mouseDy=0;aimBlend=aim?1.f:0.f;scopeBlend=scoped?1.f:0.f;distanceSmoothed=aim?1.65f:3.6f;}else {float a=1-expf(-dt*12.f);aimBlend+=((aim?1.f:0.f)-aimBlend)*a;scopeBlend+=((scoped?1.f:0.f)-scopeBlend)*a;}
 previousLocked=locked;lastTime=now;cameraOwner=(uintptr_t)p;memcpy(lastPlayer,pos,12);
 float right[3]={m[0],m[1],m[2]},up[3]={m[4],m[5],m[6]},forward[3]={m[8],m[9],m[10]};if(!normalize(right)||!normalize(up)||!normalize(forward)){inactive(now);return;}
 // Q's tracked orientation belongs to ER. Mouse deltas alter only the aim
 // ray/reticle; player framing below is independent of those mouse deltas.
 if(manualAimActive){int dx=mouseDx.exchange(0),dy=mouseDy.exchange(0);manualAim.add(dx,dy,aspect,scoped);}
 timing.validationEnd=timing.collisionEnd=stamp();void*world=rayWorld();if(!world){inactive(now);return;}
 // Q keeps ER's tracked orientation and vertical composition, but removes the
 // native lateral offset so Franklin stays horizontally centred. All weapons
 // consume the final post-collision origin published below, including scope.
 float pivot[3]={pos[0],pos[1]+1.48f,pos[2]};float back=3.6f+(1.65f-3.6f)*aimBlend;float shoulder=.62f+(.70f-.62f)*aimBlend;
 float wish[3],delta[3],length=0;
 if(locked){if(!lock_camera_framing::centered_origin(m,pivot,wish)){inactive(now);return;}}
 else for(int i=0;i<3;i++)wish[i]=pivot[i]-forward[i]*back+right[i]*shoulder;
 for(int i=0;i<3;i++){delta[i]=wish[i]-pivot[i];length+=delta[i]*delta[i];}length=sqrtf(length);if(!normalize(delta)){inactive(now);return;}
 // Five swept rays conservatively protect the near plane at doors/corners.
 static uint64_t collisionTime=0;static float collisionPivot[3]={},collisionWish[3]={},cachedAllowed=0;float shift=0;for(int i=0;i<3;i++){shift+=fabsf(pivot[i]-collisionPivot[i])+fabsf(wish[i]-collisionWish[i]);}
 bool resample=camera_policy::collision(reset,now,collisionTime,shift);float allowed=resample?length:fmaxf(.05f,fminf(length,cachedAllowed-shift));const float offsets[5][2]={{0,0},{.12f,0},{-.12f,0},{0,.12f},{0,-.12f}};
 if(resample)for(auto&offset:offsets){float a[3],b[3],hitPoint[3]={};for(int i=0;i<3;i++){a[i]=pivot[i]+right[i]*offset[0]+up[i]*offset[1];b[i]=wish[i]+right[i]*offset[0]+up[i]*offset[1];}bool hit=false;if(!trace(world,p,a,b,hitPoint,hit)){inactive(now);return;}if(hit){float d=0;for(int i=0;i<3;i++)d+=(hitPoint[i]-a[i])*delta[i];allowed=fminf(allowed,fmaxf(.05f,d-.18f));}}
 if(resample){collisionTime=now;cachedAllowed=allowed;memcpy(collisionPivot,pivot,12);memcpy(collisionWish,wish,12);}timing.collisionEnd=stamp();
 if(reset||allowed<distanceSmoothed)distanceSmoothed=allowed;else distanceSmoothed+=(allowed-distanceSmoothed)*(1-expf(-dt*9.f));
 for(int i=0;i<3;i++)m[12+i]=pivot[i]+delta[i]*distanceSmoothed;
 timing.collisionEnd=stamp();
 static uint64_t lastShot=0;if(reset)lastShot=s.shot_time;
 AcquireSRWLockExclusive(&lock);
 if(locked||reset)shakes.clear();
 if(!locked&&s.shot_time&&s.shot_time!=lastShot&&now>=s.shot_time&&now-s.shot_time<150){float strength=s.weapon==3?.022f:s.weapon==5?.026f:.010f;shakes.add((uintptr_t)p,now,strength,.22f,false);}lastShot=s.shot_time;
 auto shake=shakes.sample((uintptr_t)p,now);ReleaseSRWLockExclusive(&lock);
 if(!locked){float c=cosf(shake.pitch),sn=sinf(shake.pitch);for(int i=0;i<3;i++){float u=up[i],f=forward[i];up[i]=u*c-f*sn;forward[i]=f*c+u*sn;}c=cosf(shake.roll);sn=sinf(shake.roll);for(int i=0;i<3;i++){float r=right[i],u=up[i];right[i]=r*c+u*sn;up[i]=u*c-r*sn;m[i]=right[i];m[4+i]=up[i];m[8+i]=forward[i];}}
 float fov=(55.f+(42.f-55.f)*aimBlend)*(1-scopeBlend)+zoomFov*scopeBlend;
 memcpy(cam+0x10,m,64);*(float*)(cam+0x50)=fov*.01745329251994f;
 overlayView.save((uintptr_t)p,m,pos,*(float*)(cam+0x50));
 publishAim((uintptr_t)p,now,m,*(float*)(cam+0x50),aspect,locked,scoped);
 rifle_view((uintptr_t)p,now,m+12,forward,1);AcquireSRWLockExclusive(&lock);scopeShared=scopeBlend;zoomShared=zoomFov;ReleaseSRWLockExclusive(&lock);
 double groundStart=stamp();sampleGround(p,pos,now,reset,world);
 timing.groundMs=(stamp()-groundStart)*1000;
 static uint64_t lastLog=0;if(now-lastLog>3000){lastLog=now;char b[256];snprintf(b,sizeof(b),"view owner=%p locked=%u aim=%.2f scope=%.2f fov=%.2f distance=%.2f origin=%.3f,%.3f,%.3f",p,unsigned(locked),aimBlend,scopeBlend,fov,distanceSmoothed,m[12],m[13],m[14]);report(b);}
}
uintptr_t __fastcall klass(void*){return 0;}void __fastcall destroy(void*){}
void* vtable[3]={(void*)&klass,(void*)&destroy,(void*)&tick};struct Task{void** vtable;uint64_t unk,pad[6];};Task task={vtable,0,{}};
}
extern "C" void shoulder_camera_heartbeat(uintptr_t owner,uint64_t now){AcquireSRWLockExclusive(&lock);renderOwner=owner;renderTime=now;ReleaseSRWLockExclusive(&lock);}
extern "C" void shoulder_camera_mouse_motion(int dx,int dy){
 if(!shoulder_camera_mouse_ready())return;
 if(dx < -32768)dx=-32768;if(dx>32768)dx=32768;
 if(dy < -32768)dy=-32768;if(dy>32768)dy=32768;
 auto accumulate=[](std::atomic<int>&value,int delta){int old=value.load(std::memory_order_relaxed);for(;;){int next=old+delta;if(next>32768)next=32768;if(next< -32768)next=-32768;if(value.compare_exchange_weak(old,next,std::memory_order_relaxed))break;}};
 accumulate(mouseDx,dx);accumulate(mouseDy,dy);
}
extern "C" float shoulder_camera_scope(){AcquireSRWLockShared(&lock);float result=scopeShared;ReleaseSRWLockShared(&lock);return result;}
extern "C" void shoulder_camera_start(){base=(uintptr_t)GetModuleHandleA(nullptr);const unsigned char reg[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x40,0x48,0x8d,0x4c,0x24,0x20,0x49,0x8b,0xd8,0x8b,0xfa,0xe8};const unsigned char ray[]={0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xac,0x24,0x70,0xff,0xff,0xff,0x48,0x81,0xec,0x90,0x01,0x00,0x00};if(!sig(0xEB3E50,reg,sizeof(reg))||!sig(0xC71E00,ray,sizeof(ray))){report("signature mismatch; camera disabled");return;}uint8_t*imp=nullptr;for(int i=0;i<360&&!imp;i++){imp=typed(global(0x458FE88),0x2C04B00,8);if(!imp)Sleep(500);}if(!imp)return;HMODULE pinned=nullptr;if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCSTR)&shoulder_camera_start,&pinned))return;using Register=bool(__fastcall*)(void*,uint32_t,void*);report(((Register)(base+0xEB3E50))(imp,107,&task)?"camera registered after CameraStep":"camera registration failed");}

extern "C" int shoulder_camera_ground(uintptr_t owner,float* point,float*height){if(!point||!height)return 0;AcquireSRWLockShared(&lock);bool valid=owner==groundOwner&&GetTickCount64()-groundTime<200;if(valid){memcpy(point,groundShared,12);*height=groundHeight;}ReleaseSRWLockShared(&lock);return valid?1:0;}

extern "C" void shoulder_camera_input_blocked(int blocked){inputBlocked=blocked!=0;if(blocked){invalidateAim();manualAimReady=0;mouseDx=mouseDy=0;scrollReady=0;scrollDelta=0;}}
extern "C" int shoulder_camera_scroll_ready(){DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);RifleState state={};return pid==GetCurrentProcessId()&&GetAsyncKeyState(VK_RBUTTON)<0&&rifle_state(&state)&&state.active&&state.weapon==5&&!state.wheel_open&&!cheat_console_blocking()&&gameplay_hud_normal()==1&&scrollReady.load()&&!inputBlocked.load();}
extern "C" int shoulder_camera_scroll(int delta){if(!delta||!shoulder_camera_scroll_ready())return 0;int step=delta/120;if(!step)step=delta>0?1:-1;step=step>8?8:step< -8?-8:step;scrollDelta.fetch_add(step);return 1;}
extern "C" float shoulder_camera_zoom(){AcquireSRWLockShared(&lock);float result=42.f/zoomShared;ReleaseSRWLockShared(&lock);return result;}

extern "C" void shoulder_camera_blast(uintptr_t owner,float strength,float duration){AcquireSRWLockExclusive(&lock);shakes.add(owner,GetTickCount64(),strength,duration,true);ReleaseSRWLockExclusive(&lock);}

extern "C" int shoulder_camera_mouse_ready(){auto now=GetTickCount64(),at=aimInputTime.load(std::memory_order_relaxed);return manualAimReady.load(std::memory_order_relaxed)&&!inputBlocked.load()&&now>=at&&now-at<=150;}
extern "C" int shoulder_camera_aim_for_tick(uintptr_t owner,uint64_t now,const float*view,ShoulderAimSnapshot*out){
 if(!out)return 0;*out={};AcquireSRWLockExclusive(&lock);bool valid=shoulder_aim::claim(aimShared,owner,now,view,aimThread,GetCurrentThreadId(),aimClaimed);if(valid)*out=aimShared;ReleaseSRWLockExclusive(&lock);return valid?1:0;
}
extern "C" int shoulder_camera_aim_snapshot(uintptr_t owner,uint64_t now,ShoulderAimSnapshot*out){
 if(!out)return 0;*out={};AcquireSRWLockShared(&lock);bool valid=shoulder_aim::fresh(aimShared,owner,now);if(valid)*out=aimShared;ReleaseSRWLockShared(&lock);return valid?1:0;
}
