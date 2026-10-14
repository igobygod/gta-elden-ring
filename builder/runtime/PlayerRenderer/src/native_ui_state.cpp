#include "native_ui_state.h"
#include "../../NativePort/combat-source/native/gameplay_gate.h"
#include <windows.h>
#include <cstring>
#include <atomic>
namespace native_ui {
namespace {std::atomic<int>checked{0};uintptr_t base=0;constexpr uintptr_t slotRva=0x3D72478,vtRva=0x2B48140;
// UI_READ_CACHE_BEGIN: only positive region validation survives within one snapshot.
struct ReadRegion{uintptr_t begin,end;};
thread_local ReadRegion readRegions[32];thread_local unsigned readRegionCount=0;thread_local bool snapshotReading=false;
struct SnapshotReads{SnapshotReads(){readRegionCount=0;snapshotReading=true;}~SnapshotReads(){snapshotReading=false;readRegionCount=0;}};
bool readable(uintptr_t ptr,size_t bytes){
 uintptr_t end=ptr+bytes;if(ptr<0x10000||end<ptr)return false;
 while(ptr<end){uintptr_t next=0;
  if(snapshotReading)for(unsigned i=0;i<readRegionCount;i++)if(ptr>=readRegions[i].begin&&ptr<readRegions[i].end){next=readRegions[i].end;break;}
  if(!next){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery((void*)ptr,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
   if(!(m.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
   next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=ptr)return false;
   if(snapshotReading&&readRegionCount<32)readRegions[readRegionCount++]={(uintptr_t)m.BaseAddress,next};
  }ptr=next;
 }return true;
}
// UI_READ_CACHE_END
template<class T>bool get(uintptr_t p,T&v){if(!readable(p,sizeof(v)))return false;std::memcpy(&v,(void*)p,sizeof(v));return true;}
// Exact executable evidence: LoadingScreen ctor90B6A0 sets identifier6;
// base7427B0 stores identifier+180; reader747643 addresses MenuMan+90+identifier.
// CaptionView8A28C0 obtains current caption through MenuData getter7689C0.
// CaptionInfo text is pointer+8 or small-string+18/capacity+30; alpha signed16+44.
bool textPresent(uintptr_t info,bool& present){uintptr_t text=0;if(!get(info+8,text))return false;if(!text){size_t capacity=0;if(!get(info+0x30,capacity)||capacity>16384)return false;text=info+0x18;if(capacity>=8&&!get(text,text))return false;}uint16_t first=0;if(!get(text,first))return false;present=first!=0;return true;}
void frontend(State& s){uintptr_t fe=gameplay_frontend_instance(),man=0,vt=0;if(!fe||!get(fe+0x18,man)||!get(man,vt)||vt!=base+0x2A9EED8)return;unsigned char load=0;if(!get(man+0x96,load)||load>3)return;s.ui_valid=true;s.loading=(load&2)!=0;unsigned char acquisition=0;if(get(man+0x99,acquisition)&&acquisition<=3){s.item_acquisition_valid=true;s.item_acquisition=(acquisition&2)!=0;}
 unsigned char states[70]={};if(readable(man+0x90,sizeof(states))){std::memcpy(states,(void*)(man+0x90),sizeof(states));for(unsigned i=0;i<70;i++)if(states[i]&2){if(i<64)s.visible_mask0|=uint64_t(1)<<i;else s.visible_mask1|=uint8_t(1)<<(i-64);}}
 unsigned char fade=0;if(get(man+0x94,fade)&&fade<=3){s.menu_fade_valid=true;s.menu_fade=(fade&2)!=0;}
 uintptr_t popup=0,back=0,job=0;if(get(man+0x80,popup)&&popup&&get(popup,vt)&&vt==base+0x2ABA9D8&&get(popup+8,back)&&back==man&&get(popup+0xB0,job)){s.popup_valid=true;if(job){uintptr_t jobVt=0;if(get(job,jobVt)&&jobVt>=base&&jobVt<base+0x5300000&&readable(jobVt,sizeof(uintptr_t)))s.popup_job_active=true;else s.popup_valid=false;}}

 // Exact native rendered boss visibility: FrontEndView+A38 RootScene,
 // BossList row+19A28+n*770, cachedvisible+69. Consumer8C33F0->7368B0.
 uintptr_t front=0;if(get(fe+0x20,front)&&get(front,vt)&&vt==base+0x2AED8E0){bool all=true;uint8_t mask=0;for(unsigned i=0;i<3;i++){unsigned char v=0;if(!get(front+0xA38+0x19A28+size_t(i)*0x770+0x69,v)||v>1){all=false;break;}if(v)mask|=uint8_t(1)<<i;}if(all){s.boss_valid=true;s.boss_visible_mask=mask;}
 // GetItemLog wrapper8CC660 startsatRootScene+7A58, innerfadecache+10,
 // native76B3A0 storesrequestedvisible at+60 after8CD1B7 call.
 uintptr_t log=front+0xA38+0x7A58;unsigned char active=0;if(get(log,vt)&&vt==base+0x2AEC3D0&&get(log+0x70,active)&&active<=1){s.item_log_valid=true;s.item_log=active!=0;}}

 uintptr_t data=0;if(!get(man+8,data)||!get(data,vt)||vt!=base+0x2A9F508)return;bool alt=false,active=false;if(!textPresent(data+0xA8,alt))return;uintptr_t info=data+(alt?0xA8:8);if(!textPresent(info,active))return;int16_t alpha=0;uint16_t style=0;if(!get(info+0x44,alpha)||!get(info+0x42,style)||alpha < -1||alpha>255||style>2)return;s.subtitles_valid=true;s.subtitles=active&&alpha!=0;
}

}
bool init(){int old=checked.load();if(old)return old==1;base=(uintptr_t)GetModuleHandleW(nullptr);if(!base){checked=-1;return false;}const unsigned char sig[]={0x38,0x43,0x28,0x74,0x51,0x48,0x8B,0x4B,0x20,0x48,0x85,0xC9,0x74,0x48,0x0F,0xB6,0x41,0x20,0xA8,0x08,0x75,0x40,0xA8,0x01,0x74,0x04,0xC6,0x43,0x2B,0x01,0xF6,0x41,0x20,0x20,0x74,0x04,0xC6,0x43,0x2B,0x00,0x8B,0x41,0x10,0x89,0x43,0x2C};const unsigned char consume[]={0x4C,0x8B,0xC1,0x48,0x85,0xD2,0x74,0x61,0x0F,0xB6,0x4A,0x20,0xF6,0xC1,0x04,0x74,0x58,0x41,0x80,0xB8,0x81,0x00,0x00,0x00,0x00};bool ok=readable(base+0xA64E61,sizeof(sig))&&!memcmp((void*)(base+0xA64E61),sig,sizeof(sig))&&readable(base+0xA64FA0,sizeof(consume))&&!memcmp((void*)(base+0xA64FA0),consume,sizeof(consume))&&readable(base+slotRva,sizeof(uintptr_t));const unsigned char loadId[]={0xBA,0x06,0x00,0x00,0x00};const unsigned char capGetter[]={0x48,0x8B,0x81,0xB0,0x00,0x00,0x00};ok=ok&&readable(base+0x90B6C8,sizeof(loadId))&&!memcmp((void*)(base+0x90B6C8),loadId,sizeof(loadId))&&readable(base+0x7689C0,sizeof(capGetter))&&!memcmp((void*)(base+0x7689C0),capGetter,sizeof(capGetter));const unsigned char fadeId[]={0xBA,0x04,0x00,0x00,0x00};const unsigned char fadeVisible[]={0x85,0xDB,0x74,0x05,0x80,0xC9,0x03};ok=ok&&readable(base+0x735249,sizeof(fadeId))&&!memcmp((void*)(base+0x735249),fadeId,sizeof(fadeId))&&readable(base+0x735275,sizeof(fadeVisible))&&!memcmp((void*)(base+0x735275),fadeVisible,sizeof(fadeVisible));const unsigned char bossVisibleStore[]={0x88,0x5F,0x69};ok=ok&&readable(base+0x7368CE,sizeof(bossVisibleStore))&&!memcmp((void*)(base+0x7368CE),bossVisibleStore,sizeof(bossVisibleStore));const unsigned char acquisitionId[]={0xBA,0x09,0x00,0x00,0x00};ok=ok&&readable(base+0x8CE6AA,sizeof(acquisitionId))&&!memcmp((void*)(base+0x8CE6AA),acquisitionId,sizeof(acquisitionId));checked=ok?1:-1;return ok;}
State snapshot(){SnapshotReads reads;State s;if(checked.load()==1)frontend(s);int hud=gameplay_hud_state();s.menu_blocking=hud==1||hud==2;if(checked.load()!=1)return s;uintptr_t man=0,vt=0,selected=0;if(!get(base+slotRva,man)||!readable(man,0xC0)||!get(man,vt)||vt!=base+vtRva)return s;unsigned char can=0,gray=0;int32_t text=-1;if(!get(man+0x20,selected)||!get(man+0x2C,text)||!get(man+0x29,can)||!get(man+0x2B,gray)||can>1||gray>1||text<-1||text>10000000)return s;if(selected&&!readable(selected,0x24))return s;s.valid=true;s.prompt_selected=selected&&text>=0;s.can_execute=can!=0;s.grayed=gray!=0;s.text_id=text;return s;}
}
