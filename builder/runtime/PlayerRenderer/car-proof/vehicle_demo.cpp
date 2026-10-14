#include "car_demo.h"
#include "../car-drive/vehicle_physics.h"
#include "../car-drive/lifecycle_policy.h"
#include "car_demo_policy.h"
#include "car_collision_probe.h"
#include "../src/player_status.h"
#include "../src/cheat_console.h"
#include "../src/native_ui_state.h"
#include "../../NativePort/combat-source/native/gameplay_gate.h"
#include <windows.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
std::atomic<uintptr_t> requestedOwner{0};
std::atomic<bool> requestedClear{false};
std::atomic<unsigned> probes{0};
uintptr_t base=0;DWORD gameThread=0;int checked=0;
SRWLOCK snapshotLock=SRWLOCK_INIT;
CarSnapshot shared={};
struct SpawnCheck {
    bool active=false;uintptr_t owner=0;uint32_t block=0,groundIndex=0,obstacleIndex=0;
    uint64_t start=0;float player[3]={},origin[3]={},forward[3]={},matrix[16]={},heights[5]={};
} pending;
uintptr_t anchorOwner=0;uint32_t anchorBlock=0;float previousPlayer[3]={};
unsigned totalLegacyRays=0,sequence=0;
CarNativeProbe nativeProbe={};bool probeInitialized=false,rayProbeFailed=false,sphereProbeFailed=false;
unsigned rayProbeIndex=0,sphereProbeIndex=0;
constexpr float wheelPivots[4][3]={{-.784932f,-.233138f,-1.596394f},{.784932f,-.233138f,-1.596394f},{-.784932f,-.233138f,1.443266f},{.784932f,-.233138f,1.443266f}};
bool read(const void*p,size_t length) {
    if(!p||!length)return false;uintptr_t at=(uintptr_t)p,end=at+length;if(end<at)return false;
    while(at<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery((void*)at,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
        DWORD types=PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
        if(!(m.Protect&types))return false;uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=at)return false;at=next;}
    return true;
}
bool sig(uintptr_t r,const unsigned char*p,size_t n){return read((void*)(base+r),n)&&!std::memcmp((void*)(base+r),p,n);}
void log(const char*text){FILE*f=igobygod::open(igobygod::log_path("car-demo.log").c_str(),"a");if(f){std::fprintf(f,"%llu %s\n",GetTickCount64(),text);std::fclose(f);}}
bool initialize(){
    if(checked)return checked==1;base=(uintptr_t)GetModuleHandleW(nullptr);
    const unsigned char oldRay[]={0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xac,0x24,0x70,0xff,0xff,0xff,0x48,0x81,0xec,0x90,0x01,0,0};
    // Caller must already have passed the DLL loader's exact executable SHA256.
    checked=base&&sig(0xC71E00,oldRay,sizeof(oldRay))?1:-1;
    log(checked==1?"CAR old native ray signature accepted; driving controller enabled":"CAR native ray signature mismatch; disabled");return checked==1;
}
void* world(){
    auto slot=(void**)(base+0x3D7A0D0);if(!read(slot,8))return nullptr;auto manager=(unsigned char*)*slot;
    if(!read(manager,0xA0))return nullptr;auto w=*(unsigned char**)(manager+0x98);
    if(!read(w,16)||*(uintptr_t*)w!=base+0x2B96978)return nullptr;auto havok=*(void**)(w+8);
    if(!read(havok,8)||*(uintptr_t*)havok!=base+0x2EF0DD8)return nullptr;return w;
}
bool cameraForward(float* out){
    auto slot=(unsigned char**)(base+0x3D669C8);if(!read(slot,8)||!read(*slot,16))return false;auto cam=*(unsigned char**)(*slot+8);
    if(!read(cam,0x50)||*(uintptr_t*)cam!=base+0x2AA0A40)return false;float m[16];std::memcpy(m,cam+0x10,64);
    out[0]=m[8];out[1]=0;out[2]=m[10];float n=std::sqrt(out[0]*out[0]+out[2]*out[2]);if(!std::isfinite(n)||n<.01f)return false;out[0]/=n;out[2]/=n;return true;
}
bool legacyRay(void*w,uintptr_t owner,const float*from,const float*to,float*point,bool&hit){
    if(!w||!car_demo_policy::finite3(from)||!car_demo_policy::finite3(to))return false;
    alignas(16)float a[4]={from[0],from[1],from[2],1},d[4]={to[0]-from[0],to[1]-from[1],to[2]-from[2],0},p[4]={};
    using Ray=bool(__fastcall*)(void*,uint32_t,const float*,const float*,float*,void*);
    hit=((Ray)(base+0xC71E00))(w,0x5D,a,d,p,(void*)owner);++totalLegacyRays;
    if(hit){if(!car_demo_policy::finite3(p))return false;std::memcpy(point,p,12);}return true;
}
void publish(const CarSnapshot& s){AcquireSRWLockExclusive(&snapshotLock);shared=s;ReleaseSRWLockExclusive(&snapshotLock);}
CarSnapshot copy(){CarSnapshot s;AcquireSRWLockShared(&snapshotLock);s=shared;ReleaseSRWLockShared(&snapshotLock);return s;}
void releaseDriver();
extern bool carBodyReady;
void clear(uint64_t now){releaseDriver();carBodyReady=false;pending={};anchorOwner=0;CarSnapshot s={};s.time=now;s.drive_blocked=1;publish(s);}
void reject(uint64_t now,const char*why){pending={};auto s=copy();s.time=now;s.status=CAR_DEMO_SPAWN_REJECTED;s.drive_blocked=1;s.legacy_rays=totalLegacyRays;publish(s);log(why);cheat_console_result(why);}
void begin(const PlayerStatus&s,uint64_t now){
    clear(now);
    SpawnCheck next={};next.owner=s.owner;next.block=s.map_block_id;next.start=now;std::memcpy(next.player,s.position,12);
    if(!cameraForward(next.forward)){reject(now,"BUFFALO rejected: camera unavailable");return;}
    // Four metres ahead and 1.8 m to camera right; original ~5 m length fits.
    float right[3]={next.forward[2],0,-next.forward[0]};
    for(int i=0;i<3;i++)next.origin[i]=s.position[i]+4.f*next.forward[i]+1.8f*right[i];
    car_demo_policy::transform(next.matrix,next.forward,next.origin);next.active=true;pending=next;
    auto view=copy();view.time=now;view.status=CAR_DEMO_CHECKING_SPAWN;view.drive_blocked=1;publish(view);
}
void groundPoint(unsigned index,float* from,float* to){
    float local[3]={};if(index>0){local[0]=wheelPivots[index-1][0];local[2]=wheelPivots[index-1][2];}
    car_demo_policy::point(pending.matrix,local[0],0,local[2],from);std::memcpy(to,from,12);
    from[1]=pending.player[1]+2.f;to[1]=pending.player[1]-4.f;
}
void obstacleSegment(unsigned index,float*from,float*to){
    // Four perimeter edges and two diagonals at two heights: bounded footprint
    // clearance proof, not a solid collider or an exact hull overlap test.
    const float corners[4][2]={{-1.02f,-2.51f},{1.02f,-2.51f},{1.02f,2.58f},{-1.02f,2.58f}};
    unsigned edge=index%6,level=index/6;unsigned a=edge<4?edge:edge-4,b=edge<4?(edge+1)%4:edge-2;
    float y=-.62474f+(level?.95f:.30f);
    car_demo_policy::point(pending.matrix,corners[a][0],y,corners[a][1],from);
    car_demo_policy::point(pending.matrix,corners[b][0],y,corners[b][1],to);
}
void finish(uint64_t now){
    CarSnapshot s={};s.time=now;s.owner=pending.owner;s.visible=1;s.driving=0;s.drive_blocked=1;s.status=CAR_DEMO_PARKED_DIAGNOSTIC;s.spawn_sequence=++sequence;s.legacy_rays=totalLegacyRays;
    float highest=0;car_demo_policy::ground_height(pending.heights,highest,s.ground_spread_m);std::memcpy(s.chassis,pending.matrix,sizeof(s.chassis));
    for(int wheel=0;wheel<4;wheel++){std::memcpy(s.wheels[wheel],s.chassis,sizeof(s.chassis));car_demo_policy::point(s.chassis,wheelPivots[wheel][0],wheelPivots[wheel][1],wheelPivots[wheel][2],s.wheels[wheel]+12);}
    anchorOwner=s.owner;anchorBlock=pending.block;std::memcpy(previousPlayer,pending.player,12);pending={};publish(s);
    rayProbeIndex=sphereProbeIndex=0;rayProbeFailed=sphereProbeFailed=false;
    log("CAR spawned; F at driver door to enter");cheat_console_result("BUFFALO spawned: F beside driver door to enter");
}
void diagnose(const PlayerStatus&player,CarSnapshot&s){
    unsigned mode=probes.load();if(!mode||!s.visible)return;
    if(!probeInitialized){probeInitialized=true;int accepted=car_native_probe_init(&nativeProbe,base,gameThread,1);log(accepted==CAR_PROBE_HIT?"CAR richer native query signatures accepted (static only)":"CAR richer query signature rejection; diagnostics disabled");}
    car_native_probe_arm(&nativeProbe,mode&1,mode&2);
    if(car_native_probe_begin_frame(&nativeProbe,player.owner,1,2,1)<0)return;
    if((mode&1)&&rayProbeIndex<4){
        auto w=world();if(!w)return;
        for(unsigned count=0;count<2&&rayProbeIndex<4;count++){
            unsigned wheel=rayProbeIndex++;float a[3],b[3],legacy[3]={};bool oldHit=false;
            std::memcpy(a,s.wheels[wheel]+12,12);std::memcpy(b,a,12);a[1]+=.5f;b[1]-=1.f;
            bool oldOkay=legacyRay(w,player.owner,a,b,legacy,oldHit);CarProbeHit hit={};unsigned before=nativeProbe.ray_count;int code=car_native_probe_ray(&nativeProbe,a,b,&hit);
            float error=0;for(int i=0;i<3;i++){float d=hit.position[i]-legacy[i];error+=d*d;}
            bool pass=oldOkay&&oldHit&&code==CAR_PROBE_HIT&&error<.000625f&&hit.normal[1]>.8f;rayProbeFailed|=!pass;
            s.probe_rays+=nativeProbe.ray_count-before;s.last_probe_ms=hit.elapsed_ms;
            char text[320];std::snprintf(text,sizeof(text),"CAR RICH_RAY sample=%u old_hit=%u code=%d agree=%u normal=(%.5f,%.5f,%.5f) fraction=%.6f position_error_m=%.6f cost_ms=%.4f",wheel,oldHit?1:0,code,pass?1:0,hit.normal[0],hit.normal[1],hit.normal[2],hit.fraction,std::sqrt(error),hit.elapsed_ms);log(text);
        }
        if(rayProbeIndex==4){s.normal_probe_verified=rayProbeFailed?0:1;log(rayProbeFailed?"CAR rich-ray comparison incomplete/failed; driving blocked":"CAR four parked ground ray comparisons passed; slopes/walls/overlap still unverified");}
        return; // <=4 total old+rich rays; sphere diagnostics begin next tick.
    }
    if((mode&2)&&sphereProbeIndex<2){
        float a[3],b[3];std::memcpy(a,s.chassis+12,12);std::memcpy(b,a,12);a[1]+=.5f;
        unsigned sample=sphereProbeIndex++;
        if(sample==0){std::memcpy(b,a,12);for(int i=0;i<3;i++)b[i]-=s.chassis[8+i]*3.f;}
        else {std::memcpy(b,a,12);b[1]-=2.f;}
        CarProbeHit hit={};int code=car_native_probe_sphere(&nativeProbe,a,b,.25f,&hit);
        bool pass=sample==0?code>=0:code==CAR_PROBE_HIT&&hit.normal[1]>.8f;sphereProbeFailed|=!pass;
        s.probe_sweeps+=nativeProbe.sphere_count;s.last_probe_ms=hit.elapsed_ms;
        char text[320];std::snprintf(text,sizeof(text),"CAR SPHERE sample=%u code=%d valid=%u traveled_center=(%.5f,%.5f,%.5f) normal=(%.5f,%.5f,%.5f) fraction=%.6f radius=.25 cost_ms=%.4f",sample,code,pass?1:0,hit.position[0],hit.position[1],hit.position[2],hit.normal[0],hit.normal[1],hit.normal[2],hit.fraction,hit.elapsed_ms);log(text);
        if(sphereProbeIndex==2){s.sphere_probe_verified=sphereProbeFailed?0:1;log(sphereProbeFailed?"CAR sphere diagnostic incomplete/failed; driving blocked":"CAR bounded sphere diagnostics passed; initial overlap/corners/high speed still unverified");}
    }
}
#include "../car-drive/controller.inc"
}
extern "C" int car_controls_owned(uintptr_t owner){auto s=copy();return s.driving&&(!owner||owner==s.owner)&&car_demo_policy::fresh(GetTickCount64(),s.time);}
extern "C" void car_request_spawn(uintptr_t owner){if(owner)requestedOwner.store(owner);}
extern "C" void car_request_despawn(){requestedClear=true;requestedOwner=0;}
extern "C" void car_request_remove(uintptr_t owner){if(owner)car_request_despawn();}
extern "C" void car_probe_enable(int ray,int sphere){probes=(ray?1u:0u)|(sphere?2u:0u);}
extern "C" int car_snapshot(CarSnapshot*out){if(!out)return 0;*out=copy();if(!car_demo_policy::fresh(GetTickCount64(),out->time)){out->visible=out->driving=0;return 0;}return out->visible?1:0;}
extern "C" void car_tick(){
    auto now=GetTickCount64();if(!gameThread)gameThread=GetCurrentThreadId();if(GetCurrentThreadId()!=gameThread)return;
    if(requestedClear.exchange(false)){clear(now);cheat_console_result("CARCLEAR: Buffalo removed");return;}
    if(!pending.active&&!anchorOwner&&!requestedOwner.load()&&!probes.load())return;
    PlayerStatus player={};bool valid=player_status_snapshot(&player)&&player.valid&&player.map_valid&&!player.dead&&player.hp>0;
    static uint64_t unavailableSince=0;
    auto ui=native_ui::snapshot();
    if(player.dead||(player.valid&&player.hp<=0)||(ui.ui_valid&&ui.loading)){requestedOwner=0;clear(now);unavailableSince=0;log("CAR cleared on native death/loading");return;}
    if(!valid||!ui.ui_valid||!ui.menu_fade_valid){
        if(!unavailableSince)unavailableSince=now;
        if(car_lifecycle::elapsed(now,unavailableSince)>500){requestedOwner=0;clear(now);return;}
        if(drivePhase==1||drivePhase==3)phaseAt+=std::min(car_lifecycle::elapsed(now,carLast),car_lifecycle::elapsed(now,phaseAt));carLast=now;auto held=copy();held.time=now;publish(held);return;
    }
    unavailableSince=0;
    if(anchorOwner&&(anchorOwner!=player.owner||vehicle::zone(anchorBlock)!=vehicle::zone(player.map_block_id))){clear(now);log("CAR cleared on owner/map transition");}
    bool paused=ui.menu_fade||ui.menu_blocking||gameplay_hud_normal()!=1||cheat_console_blocking();
    if(paused){if(pending.active)pending.start=now;auto s=copy();driveTick(player,s,now,true);s.time=now;publish(s);return;}
    if(!initialize()){auto s=copy();s.time=now;s.status=CAR_DEMO_NATIVE_GUARD_FAILED;s.visible=0;s.drive_blocked=1;publish(s);requestedOwner=0;return;}
    auto request=requestedOwner.exchange(0);if(request){if(request==player.owner)begin(player,now);else reject(now,"BUFFALO rejected: owner changed");}
    bool wasSpawning=pending.active;
    if(pending.active){
        if(!car_demo_policy::lifecycle(pending.owner,player.owner,valid,player.dead,pending.block,player.map_block_id,pending.player,player.position)||now<pending.start||now-pending.start>1000){reject(now,"BUFFALO rejected: world changed during spawn check");return;}
        auto w=world();if(!w){reject(now,"BUFFALO rejected: terrain unavailable");return;}
        unsigned budget=4;
        while(budget&&pending.groundIndex<5){float a[3],b[3],hit[3]={};bool touched=false;groundPoint(pending.groundIndex,a,b);
            if(!legacyRay(w,player.owner,a,b,hit,touched)||!touched){reject(now,"BUFFALO rejected: no ground under footprint");return;}pending.heights[pending.groundIndex++]=hit[1];--budget;}
        if(pending.groundIndex==5&&pending.obstacleIndex==0){float height=0,spread=0;if(!car_demo_policy::ground_height(pending.heights,height,spread)){reject(now,"BUFFALO rejected: choose flatter ground");return;}
            vehicle::State spawn;spawn.p={pending.origin[0],(pending.heights[1]+pending.heights[2]+pending.heights[3]+pending.heights[4])*.25f+.64f,pending.origin[2]};spawn.yaw=atan2f(pending.forward[0],pending.forward[2]);spawn.pitch=atan2f((pending.heights[1]+pending.heights[2]-pending.heights[3]-pending.heights[4])*.5f,3.04f);spawn.roll=atan2f((pending.heights[2]+pending.heights[4]-pending.heights[1]-pending.heights[3])*.5f,1.57f);vehicle::matrix(spawn,pending.matrix);}
        while(budget&&pending.groundIndex==5&&pending.obstacleIndex<12){float a[3],b[3],hit[3]={};bool touched=false;obstacleSegment(pending.obstacleIndex,a,b);
            if(!legacyRay(w,player.owner,a,b,hit,touched)||touched){reject(now,"BUFFALO rejected: footprint crosses terrain or a wall");return;}++pending.obstacleIndex;--budget;}
        if(pending.groundIndex==5&&pending.obstacleIndex==12)finish(now);
    }
    auto s=copy();if(!wasSpawning)driveTick(player,s,now,false);s.time=now;s.legacy_rays=totalLegacyRays;publish(s);auto currentPhys=physics(player.owner);if(currentPhys)memcpy(previousPlayer,currentPhys+0x70,12);else memcpy(previousPlayer,player.position,12);
}
