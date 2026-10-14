#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include "native_lock_marker.h"
#include "lock_marker_policy.h"
#include "rifle.h"
#include "player_status.h"
#include "cheat_console.h"
#include "gameplay_gate.h"
extern "C" int WINAPI MH_Initialize();
extern "C" int WINAPI MH_CreateHook(void*,void*,void**);
extern "C" int WINAPI MH_EnableHook(void*);
extern "C" int WINAPI MH_RemoveHook(void*);
namespace {
uintptr_t base=0;std::atomic<bool> attempted{false};
using Update=void(__fastcall*)(void*,float,const lock_marker::Data*);
Update originalUpdate=nullptr;
// No cached game pointers survive a callback. This cache only amortizes region
// validation during the current native UI update, on its original game thread.
struct Reads {
 struct Span{uintptr_t begin,end;};Span spans[24];unsigned count=0;
 bool readable(uintptr_t p,size_t n){
  uintptr_t end=p+n;if(p<65536||end<p)return false;
  while(p<end){uintptr_t next=0;for(unsigned i=0;i<count;i++)if(p>=spans[i].begin&&p<spans[i].end){next=spans[i].end;break;}
   if(!next){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery((void*)p,&m,sizeof(m))||m.State!=MEM_COMMIT||
      (m.Protect&(PAGE_NOACCESS|PAGE_GUARD))||!(m.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
    next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=p)return false;if(count<24)spans[count++]={(uintptr_t)m.BaseAddress,next};
   }p=next;
  }return true;
 }
 template<class T>bool get(uintptr_t p,T&v){if(!readable(p,sizeof(v)))return false;std::memcpy(&v,(void*)p,sizeof(v));return true;}
 bool typed(uintptr_t p,uintptr_t vt,size_t n){uintptr_t actual=0;return readable(p,n)&&get(p,actual)&&actual==base+vt;}
};
void report(const char*message){FILE*f=igobygod::open(igobygod::log_path("target-marker.log").c_str(),"a");if(f){fprintf(f,"%llu %s\n",GetTickCount64(),message);fclose(f);}}
bool signature(uintptr_t rva,const unsigned char*bytes,size_t size){Reads r;return r.readable(base+rva,size)&&!memcmp((void*)(base+rva),bytes,size);}
template<size_t N>bool signature(uintptr_t rva,const unsigned char(&bytes)[N]){return signature(rva,bytes,N);}
struct Audit {bool candidate=false;int32_t oldX=0,oldY=0;uint64_t sequence=0,age=0;uintptr_t owner=0;float fov=0,aspect=0;uint32_t width=0,height=0;};
const char*correct(void*widget,const lock_marker::Data*data,lock_marker::Data&copy,Audit&a){
 Reads r;lock_marker::Data input={};if(!r.get((uintptr_t)data,input)||input.state!=1)return "not-locked-widget";
 a.candidate=true;a.oldX=input.x;a.oldY=input.y;
 // RootScene+15B28 is constructed specifically with "LockOn" by 8C68A4;
 // update 8C9DF5 passes RootScene->data+3690. Wrapper ctor74B63B sets its vtable.
 uintptr_t fe=gameplay_frontend_instance(),front=0,menu=0,rootData=0;
 if(!fe||!r.get(fe+0x20,front)||!r.typed(front,0x2AED8E0,0xA40)||
    (uintptr_t)widget!=front+0xA38+0x15B28||!r.typed((uintptr_t)widget,0x2A97AF0,0x1A0)||
    !r.get(front+0xA38+0x60,rootData)||(uintptr_t)data!=rootData+0x3690)return "widget-lifetime";
 if(!r.get(fe+0x18,menu)||!r.typed(menu,0x2A9EED8,0xA0))return "menu-lifetime";
 unsigned char loading=0,fade=0;if(!r.get(menu+0x96,loading)||!r.get(menu+0x94,fade)||loading>3||fade>3)return "menu-flags";
 uintptr_t world=0,player=0,cs=0,cam=0;
 if(!r.get(base+0x3D69FF8,world)||!r.typed(world,0x2A4E980,0x1E510)||!r.get(world+0x1E508,player)||
    !r.typed(player,0x2A7FBB0,0x6D4)||!r.get(base+0x3D669C8,cs)||!r.get(cs+8,cam)||!r.typed(cam,0x2AA0A40,0x60))return "world-lifetime";
 a.owner=player;uint64_t now=GetTickCount64();ShoulderAimSnapshot s={};
 if(!shoulder_camera_aim_snapshot(player,now,&s))return "camera-unavailable";
 a.sequence=s.sequence;a.age=now-s.time;
 float view[16],target[3],fov=0,aspect=0;unsigned char nativeLocked=0,playerFlags=0;
 if(!r.get(cam+0x10,view)||!r.get(cam+0x50,fov)||!r.get(cam+0x54,aspect)||
    !r.get(player+0xC9,nativeLocked)||!r.get(player+0x1C5,playerFlags)||!r.get(player+0xD0,target))return "camera-target-read";
 a.fov=fov;a.aspect=aspect;
 RifleState rifle={};PlayerStatus status={};
 if(!rifle_state(&rifle)||!player_status_snapshot(&status)||!status.valid||status.owner!=player||now<status.time||now-status.time>100)return "player-state";
 lock_marker::Gate gate={};gate.normal=gameplay_hud_normal()==1;gate.loading=(loading&2)!=0||(playerFlags&0x80)!=0;
 gate.fade=(fade&2)!=0;gate.modal=rifle.wheel_open||cheat_console_blocking();gate.dead=status.dead||status.hp<=0;
 gate.native_locked=nativeLocked==1;gate.weapon=rifle.weapon;gate.owner=player;gate.now=now;
 if(!lock_marker::eligible(input,gate,s,view,fov,aspect))return "gate-or-final-camera-mismatch";
 float distance=0;for(unsigned i=0;i<3;i++){if(!std::isfinite(target[i])||!std::isfinite(status.position[i]))return "invalid-target";float d=target[i]-status.position[i];distance+=d*d;}
 if(!std::isfinite(distance)||distance<.04f||distance>62500.f)return "target-range";
 // The NORMAL producer777F80 uses the movie's1920x1080 coordinate space:
 // (stageWidth / framebufferWidth) * framebufferWidth, likewise height.
 // Its constants are read here only after the exact producer was verified.
 // The debug widget branch uses a different1280x720 canvas and is not valid.
 float stageWidth=0,stageHeight=0;
 if(!r.get(base+0x32A19B8,stageWidth)||!r.get(base+0x32A19B4,stageHeight)||
    !lock_marker::stage_extent(stageWidth,stageHeight,a.width,a.height))return "stage-unavailable";
 if(!lock_marker::project(input,target,view,fov,aspect,a.width,a.height,copy))return "projection-outside";
 // Re-read owner and widget holder before passing only a stack copy onward.
 uintptr_t currentPlayer=0,currentFront=0;if(!r.get(world+0x1E508,currentPlayer)||currentPlayer!=player||!r.get(fe+0x20,currentFront)||currentFront!=front)return "owner-changed";
 return nullptr;
}
void __fastcall update(void*widget,float delta,const lock_marker::Data*data){
 lock_marker::Data copy={};Audit audit;const char*reason=correct(widget,data,copy,audit);
 // One original call: no writes to the native marker state, view, Q target,
 // HUD masks or reticle. Original consumes the three copied values synchronously.
 originalUpdate(widget,delta,reason?data:&copy);
 static std::atomic<unsigned> lines{0};static std::atomic<uint64_t> last{0};
 uint64_t now=GetTickCount64(),previous=last.load();
 if(audit.candidate&&lines.load()<24&&now>=previous&&now-previous>=1000&&last.compare_exchange_strong(previous,now)){
  lines.fetch_add(1);char message[320];snprintf(message,sizeof(message),"marker %s thread=%lu owner=%p camera_seq=%llu age_ms=%llu fov=%.6f aspect=%.6f native_stage=%ux%u native_xy=%d,%d final_xy=%d,%d",reason?reason:"corrected",GetCurrentThreadId(),(void*)audit.owner,audit.sequence,audit.age,audit.fov,audit.aspect,audit.width,audit.height,audit.oldX,audit.oldY,copy.x,copy.y);report(message);
 }
}
}
extern "C" void native_lock_marker_start(){
 if(attempted.exchange(true))return;base=(uintptr_t)GetModuleHandleW(nullptr);if(!base)return;
 const unsigned char updateSig[]={0x48,0x89,0x5c,0x24,0x10,0x55,0x56,0x57,0x48,0x83,0xec,0x40,0x48,0x8b,0x05,0x3d,0x37,0x39,0x03,0x48,0x33,0xc4,0x48,0x89,0x44,0x24,0x30,0x49,0x8b,0xf0,0x48,0x8b,0xd9,0x45,0x8b,0x40,0x08,0x8b,0x56,0x04,0xe8,0x93,0x89,0xe6,0xff};
 const unsigned char callerSig[]={0x4c,0x8b,0x46,0x60,0x49,0x81,0xc0,0x90,0x36,0x00,0x00,0x48,0x8d,0x8e,0x28,0x5b,0x01,0x00,0x0f,0x28,0xce,0xe8,0xb1,0x18,0x00,0x00};

 const unsigned char stageWidthSig[]={0xf3,0x0f,0x10,0x15,0x76,0x99,0xb2,0x02};
 const unsigned char stageHeightSig[]={0xf3,0x0f,0x10,0x0d,0xe3,0x98,0xb2,0x02};
 const unsigned char stageMapSig[]={0xf3,0x48,0x0f,0x2a,0xc0,0x8b,0x05,0x0b,0xbc,0x3c,0x03,0xf3,0x0f,0x11,0x6d,0x03,0xf3,0x0f,0x5e,0xd0,0xf3,0x0f,0x59,0xd0,0x0f,0x57,0xc0,0xf3,0x48,0x0f,0x2a,0xc0,0x33,0xc0,0xf3,0x0f,0x59,0xd4,0xf3,0x0f,0x5e,0xc8,0xf3,0x0f,0x11,0x55,0xf7,0xf3,0x0f,0x59,0xc8,0xf3,0x0f,0x11,0x55,0x97,0xf3,0x0f,0x59,0xcb,0xf3,0x0f,0x11,0x4d,0xfb};
 const unsigned char markerWriterSig[]={0xc7,0x83,0x10,0x37,0x00,0x00,0x01,0x00,0x00,0x00,0xf3,0x0f,0x2c,0x83,0xd0,0x59,0x00,0x00,0x89,0x83,0x14,0x37,0x00,0x00,0xf3,0x0f,0x2c,0x83,0xd4,0x59,0x00,0x00,0x89,0x83,0x18,0x37,0x00,0x00};
 const unsigned char wrapperSig[]={0x48,0x8d,0x05,0xae,0xc4,0x34,0x02,0x48,0x89,0x03};
 const unsigned char lockSig[]={0x0f,0xb6,0x81,0xc9,0x00,0x00,0x00,0xc3};
 const unsigned char targetSig[]={0x0f,0x10,0x81,0xd0,0x00,0x00,0x00,0x0f,0x29,0x44,0x24,0x40,0xf3,0x0f,0x10,0x76,0x08,0xe8,0xe0,0x83,0x01,0x00};
 const unsigned char targetWriteSig[]={0xf3,0x0f,0x10,0x81,0xd0,0x00,0x00,0x00,0xf3,0x0f,0x58,0x02,0xf3,0x0f,0x11,0x81,0xd0,0x00,0x00,0x00};
 if(!signature(0x8CB6C0,updateSig)||!signature(0x8C9DF5,callerSig)||!signature(0x77803A,stageWidthSig)||!signature(0x7780C9,stageHeightSig)||!signature(0x7780E6,stageMapSig)||!signature(0x7721B9,markerWriterSig)||!signature(0x74B63B,wrapperSig)||
    !signature(0x3F42B0,lockSig)||!signature(0x3DBEBA,targetSig)||!signature(0x3F7AD6,targetWriteSig)){report("signature mismatch: only native marker correction disabled");return;}
 HMODULE pinned=nullptr;if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCSTR)&native_lock_marker_start,&pinned))return;
 int status=MH_Initialize();if(status!=0&&status!=1){report("MinHook unavailable: marker unchanged");return;}
 void*target=(void*)(base+0x8CB6C0);status=MH_CreateHook(target,(void*)&update,(void**)&originalUpdate);
 if(status!=0){report("marker hook creation failed: marker unchanged");return;}
 if(MH_EnableHook(target)!=0){MH_RemoveHook(target);originalUpdate=nullptr;report("marker hook enable failed: marker unchanged");return;}
 report("native LockOn correction installed with normal-producer movie stage; original widget style/state retained");
}
