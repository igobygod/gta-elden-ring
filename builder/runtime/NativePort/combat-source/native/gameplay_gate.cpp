#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <atomic>
#include "gameplay_gate.h"
// Independently implemented narrow resolver from the current executable's singleton assertion code.
// Research/provenance: LorenzoFerri/RadialMenu commit 9c96d1fecaac5ca26bd32a99b96da07e2770b681,
// src/game/state/gameplay_state.cpp (CSFeMan HUD state +0x78, default=3) and singleton_resolver.cpp.
// No source copied: upstream has no declared license. The actual instance pointer is derived from
// a null-checked global next to an FD4 reflection assertion; the metadata global at 0x3D70398 is NOT used.
namespace {
constexpr uintptr_t kNameGetterRva=0x1EC3250;
struct Span {uintptr_t first=0;size_t size=0;bool has(uintptr_t p,size_t n=1)const{return p>=first&&n<=size&&p-first<=size-n;}};
Span code,data,image;
std::atomic<int> status{0};std::atomic<bool> attempted{false};uintptr_t instance_slot=0;
bool readable(const void* ptr,size_t bytes){
 uintptr_t p=(uintptr_t)ptr,end=p+bytes;if(p<0x10000||end<p)return false;
 while(p<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery((void*)p,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
  if(!(m.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
  uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=p)return false;p=next;
 }return true;
}
uintptr_t relative(const uint8_t* p,unsigned instruction_bytes,unsigned displacement_offset){int32_t d;memcpy(&d,p+displacement_offset,4);return(uintptr_t)p+instruction_bytes+d;}
unsigned argument(const uint8_t* p,unsigned& tag){
 if(p[0]==0x4C&&p[1]==0x8B&&p[2]==0xC8){tag=1;return 3;} // r9 = class name returned by getter
 if(p[0]==0xBA){tag=2;return 5;} // assertion source line
 if(p[0]==0x4C&&p[1]==0x8D&&p[2]==0x05){tag=4;return 7;} // r8 = expression
 if(p[0]==0x48&&p[1]==0x8D&&p[2]==0x0D){tag=8;return 7;} // rcx = source
 tag=0;return 0;
}
uintptr_t guarded_slot(const uint8_t* reflection){
 auto p=(uintptr_t)reflection;if(!code.has(p-16,16))return 0;
 const uint8_t* jump=nullptr;
 if(reflection[-2]==0x75)jump=reflection-2;
 else if(reflection[-6]==0x0F&&reflection[-5]==0x85)jump=reflection-6;
 else return 0;
 const uint8_t* test=jump-3;const uint8_t* load=test-7;
 if((test[0]&0xF8)!=0x48||test[1]!=0x85||(test[2]&0xC0)!=0xC0)return 0;
 unsigned left=((test[0]&4)?8:0)+((test[2]>>3)&7),right=((test[0]&1)?8:0)+(test[2]&7);
 if(left!=right||(load[0]&0xF8)!=0x48||load[1]!=0x8B||(load[2]&0xC7)!=0x05)return 0;
 unsigned loaded=((load[0]&4)?8:0)+((load[2]>>3)&7);if(loaded!=left)return 0;
 uintptr_t slot=relative(load,7,3);return data.has(slot,sizeof(void*))?slot:0;
}
bool sections(){
 uintptr_t base=(uintptr_t)GetModuleHandleW(nullptr);auto dos=(IMAGE_DOS_HEADER*)base;if(!readable(dos,sizeof(*dos))||dos->e_magic!=IMAGE_DOS_SIGNATURE)return false;
 auto nt=(IMAGE_NT_HEADERS64*)(base+dos->e_lfanew);if(!readable(nt,sizeof(*nt))||nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return false;
 image={base,nt->OptionalHeader.SizeOfImage};auto s=IMAGE_FIRST_SECTION(nt);
 if(nt->FileHeader.NumberOfSections>96||!readable(s,nt->FileHeader.NumberOfSections*sizeof(*s)))return false;
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++){Span span{base+s[i].VirtualAddress,s[i].Misc.VirtualSize};if(!image.has(span.first,span.size))return false;
  // The supported executable has TWO .text sections. Select the one containing
  // the independently verified reflection getter, never the last matching name.
  if(memcmp(s[i].Name,".text\0",6)==0&&span.has(base+kNameGetterRva,33))code=span;
  else if(memcmp(s[i].Name,".data\0",6)==0)data=span;
 }return code.size&&data.size&&readable((void*)code.first,code.size);
}
}
extern "C" int gameplay_gate_init(){
 bool expected=false;if(!attempted.compare_exchange_strong(expected,true))return status.load(std::memory_order_acquire)==1?1:0;
 if(!sections()){status.store(GAMEPLAY_GATE_BAD_SECTIONS);return 0;}
 // Verified offline on the exact supported executable: FD4 reflection class-name lookup.
 // Require its entire small body, not just an arbitrary call target encountered by scanning.
 const uint8_t getter_bytes[]={0x48,0x83,0xEC,0x28,0xE8,0x27,0xFF,0xFF,0xFF,0x48,0x85,0xC0,0x74,0x0E,0x48,0x8B,0x10,0x48,0x8B,0xC8,0x48,0x83,0xC4,0x28,0x48,0xFF,0x62,0x08,0x48,0x83,0xC4,0x28,0xC3};
 uintptr_t getter=image.first+kNameGetterRva;
 if(!code.has(getter,sizeof(getter_bytes))||memcmp((void*)getter,getter_bytes,sizeof(getter_bytes))){status.store(GAMEPLAY_GATE_BAD_GETTER);return 0;}
 using NameFn=const char*(__fastcall*)(const void*);
 uintptr_t winner=0;bool ambiguous=false;unsigned candidates=0,readable_names=0;
 // Cache repeated metadata IDs inside this one scan; never hold object pointers across scans/ticks.
 struct NameCache {uintptr_t address;bool wanted;};NameCache cache[2048]={};unsigned cached=0;
 for(size_t offset=24;offset+64<code.size;offset++){
  auto p=(const uint8_t*)(code.first+offset);
  if(p[0]!=0x48||p[1]!=0x8D||p[2]!=0x0D||p[7]!=0xE8||relative(p+7,5,1)!=getter)continue;
  uintptr_t slot=guarded_slot(p);if(!slot)continue;
  const uint8_t* q=p+12;unsigned mask=0;bool valid=true;
  for(unsigned i=0;i<4;i++){unsigned tag=0,n=argument(q,tag);if(!n||(mask&tag)){valid=false;break;}mask|=tag;q+=n;}
  if(!valid||mask!=15||*q!=0xE8)continue;
  uintptr_t metadata=relative(p,7,3);if(!data.has(metadata))continue;
  ++candidates;
  bool found=false,wanted=false;
  for(unsigned i=0;i<cached;i++)if(cache[i].address==metadata){found=true;wanted=cache[i].wanted;break;}
  if(!found){const char* name=((NameFn)getter)((void*)metadata);bool valid_name=name&&readable(name,8);if(valid_name)++readable_names;wanted=valid_name&&memcmp(name,"CSFeMan",8)==0;if(cached<2048)cache[cached++]={metadata,wanted};}
  if(!wanted)continue;
  if(winner&&winner!=slot){ambiguous=true;break;}winner=slot;
 }
 if(!winner||ambiguous){
  int error=ambiguous?GAMEPLAY_GATE_AMBIGUOUS:!candidates?GAMEPLAY_GATE_NO_CANDIDATES:!readable_names?GAMEPLAY_GATE_NAMES_UNAVAILABLE:GAMEPLAY_GATE_NAME_NOT_FOUND;
  status.store(error,std::memory_order_release);return 0;
 }
 instance_slot=winner;status.store(1,std::memory_order_release);return 1;
}
extern "C" int gameplay_gate_status(){return status.load(std::memory_order_acquire);}
extern "C" int gameplay_hud_state(){
 if(status.load(std::memory_order_acquire)!=1||!readable((void*)instance_slot,sizeof(void*)))return -1;
 uintptr_t fe=*(uintptr_t*)instance_slot;if(!readable((void*)fe,0x79))return -1;
 if(!image.has(*(uintptr_t*)fe,sizeof(void*)))return -1;
 int state=*(const uint8_t*)(fe+0x78);return state<=3?state:-1;
}
extern "C" int gameplay_hud_normal(){return gameplay_hud_state()==3?1:0;}
extern "C" uintptr_t gameplay_frontend_instance(){if(gameplay_hud_state()<0)return 0;return *(uintptr_t*)instance_slot;}

