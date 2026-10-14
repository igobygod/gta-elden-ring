// Native inventory/equip ABI adapted from the local audited equipment.cpp.
// Provenance: The-Grand-Archives/Elden-Ring-CT-TGA @7926205c5a2ed236dd31278c4f5579c964ceec35.
#include "native_melee.h"
#include "native_melee_policy.h"
#include <windows.h>
#include <cstring>
#include <cstdlib>

namespace {
uintptr_t image=0;DWORD gameThread=0;int status=0;
native_melee_policy::EquipmentLease lease;
constexpr uintptr_t kEquipGear=0x249160,kGiveItem=0x561400,kLookupWeapon=0xD563B0;
constexpr uintptr_t kGameDataMan=0x3D61F98,kMapItemMan=0x3D6BAC0,kWorldChrMan=0x3D69FF8;
uint64_t retryAt[6]={};uint32_t retryItem[6]={};
bool readable(const void* ptr,size_t count){
 auto at=(uintptr_t)ptr;if(!at||count>UINTPTR_MAX-at)return false;auto end=at+count;
 while(at<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery((void*)at,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
  if(!(m.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
  auto next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=at)return false;at=next;
 }return true;
}
template<typename T>bool read(uintptr_t at,T& value){if(!readable((void*)at,sizeof(value)))return false;memcpy(&value,(void*)at,sizeof(value));return true;}
bool signature(uintptr_t rva,const char* bytes){size_t i=0;for(auto s=bytes;*s;){while(*s==' ')s++;if(!*s)break;char hex[3]={s[0],s[1],0};uint8_t actual=0;if(!read(image+rva+i,actual)||actual!=(uint8_t)strtoul(hex,nullptr,16))return false;s+=2;i++;}return true;}
bool currentPlayer(uintptr_t expected){
 uintptr_t w=0,vt=0,p=0;if(!expected||!read(image+kWorldChrMan,w)||!read(w,vt)||vt!=image+0x2A4E980||!read(w+0x1E508,p)||p!=expected)return false;
 uint32_t handle=0;return read(p,vt)&&vt==image+0x2A7FBB0&&readable((void*)p,0x6D4)&&read(p+0x6D0,handle)&&handle&&handle!=0xFFFFFFFFu;
}
uintptr_t playerData(){uintptr_t manager=0,p=0;if(!read(image+kGameDataMan,manager)||!read(manager+8,p)||!readable((void*)p,0x5D8))return 0;return p;}
bool activeSlots(uintptr_t p,uint32_t&style,uint32_t&left,uint32_t&right){uint32_t l=0,r=0;return read(p+0x324,style)&&read(p+0x328,l)&&read(p+0x32C,r)&&native_melee_policy::slots(style,l,r,left,right);}
bool equipped(uintptr_t p,unsigned slot,uint32_t&item){return slot<6&&read(p+0x398+4*slot,item);}
struct Entry{uint32_t handle=0,index=0,tail=0;};
bool find(uintptr_t p,uint32_t item,Entry& entry){
 uintptr_t inventory=0,head=0;uint32_t capacity=0,count=0,tail=0;
 if(!read(p+0x5D0,inventory)||!read(inventory+0x10,head)||!read(inventory+0x0C,capacity)||!read(inventory+0x18,count)||!read(inventory+0x1C,tail))return false;
 if(!capacity||capacity>10000||count>capacity||tail>10000||!readable((void*)head,(size_t)capacity*0x18))return false;
 // Allocations can contain holes: scan capacity rather than occupied count.
 for(uint32_t i=0;i<capacity;i++){uint32_t id=0,handle=0,quantity=0;auto at=head+(size_t)i*0x18;
  if(!read(at+4,id)||!read(at,handle)||!read(at+8,quantity))return false;
  if(id==item&&handle!=0xFFFFFFFFu&&quantity>0){entry={handle,i,tail};return true;}
 }return false;
}
bool hasWeaponParam(uint32_t id){alignas(16)uint8_t result[0x40]={};using Lookup=void(__fastcall*)(void*,uint32_t,uint32_t,uint32_t);((Lookup)(image+kLookupWeapon))(result,id,0,0);uintptr_t row=0;memcpy(&row,result+8,8);return readable((void*)row,16);}
bool ensure(uintptr_t p,uint32_t item,Entry& entry,bool allowGrant){
 if(find(p,item,entry))return true;if(!allowGrant){status=-9;return false;}
 uint32_t bit=item==native_melee_policy::fists?1:item==native_melee_policy::sword?2:0;
 if(!bit){status=-6;return false;}if(lease.grant_attempts&bit){status=-5;return false;}
 if(!hasWeaponParam(item)){status=-4;return false;}
 uintptr_t map=0;if(!read(image+kMapItemMan,map)||!readable((void*)map,8)){status=-3;return false;}
 struct Request{uint32_t count,item,quantity;int32_t durability,gem;};static_assert(sizeof(Request)==20,"ItemGive single-item ABI");
 Request request={1,item,1,-1,-1};alignas(16)uint8_t result[128]={};using Give=void(__fastcall*)(void*,Request*,void*,uint32_t);
 lease.grant_attempts|=bit;((Give)(image+kGiveItem))((void*)map,&request,result,0);
 if(!find(p,item,entry)){status=-5;return false;}return true;
}
bool equip(uintptr_t p,unsigned slot,uint32_t target,bool restoring){
 uint32_t before=0;if(!equipped(p,slot,before)){status=-3;return false;}lease.observe(slot,before);
 if(before==target){if(restoring)lease.restored(slot);return true;}
 auto now=GetTickCount64();if(retryItem[slot]==target&&now<retryAt[slot]){status=-10;return false;}
 retryItem[slot]=target;retryAt[slot]=now+1000;
 Entry entry;bool canGrant=!restoring||target==native_melee_policy::fists;if(!ensure(p,target,entry,canGrant))return false;
 using Equip=void(__fastcall*)(void*,uint32_t,uint32_t*,uint32_t,uint32_t,uint32_t,uint32_t);
 ((Equip)(image+kEquipGear))((void*)(p+0x2B0),slot,&entry.handle,entry.index+entry.tail,1,1,0);
 uint32_t actual=0;if(!equipped(p,slot,actual)||actual!=target){status=-7;return false;}
 retryAt[slot]=0;if(restoring)lease.restored(slot);else lease.applied(slot,before,target);return true;
}
int finish(NativeMeleeState*out,uint32_t mode=0,uint32_t hand=0,uint32_t slot=0,uint32_t item=0){if(out)*out={mode,hand,slot,item,status};return status==2?1:0;}
}
extern "C" int native_melee_init(){
 if(image&&status!=-1)return 1;image=(uintptr_t)GetModuleHandleW(nullptr);if(!image){status=-1;return 0;}
 if(!signature(kEquipGear,"40 53 55 56 57 41 56 48 81 EC 90 00 00 00")||
    !signature(kGiveItem,"40 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 70 FF FF FF")||
    !signature(kGiveItem+0x52,"8B 02 83 F8 0A")||
    !signature(kLookupWeapon,"40 57 41 56 41 57 48 83 EC 40 48 C7 44 24 20 FE FF FF FF")||
    !signature(kLookupWeapon+0x22,"8B EA 48 8B F1 33 DB 41 83 CE FF 85 D2")){image=0;status=-1;return 0;}
 int32_t gd=0,mi=0;if(!read(image+0x256560+3,gd)||!read(image+0x5AC7EE+3,mi)||
    image+0x256560+7+gd!=image+kGameDataMan||image+0x5AC7EE+7+mi!=image+kMapItemMan){image=0;status=-1;return 0;}
 status=1;return 1;
}
extern "C" int native_melee_bind_game_thread(){if(!image)return 0;auto id=GetCurrentThreadId();if(gameThread&&id!=gameThread){status=-2;return 0;}gameThread=id;return 1;}
extern "C" int native_melee_select(uintptr_t expected,uint32_t desired,NativeMeleeState*out){
 if(!image){status=-1;return finish(out);}if(!gameThread||gameThread!=GetCurrentThreadId()){status=-2;return finish(out);}
 if(desired>2){status=-6;return finish(out);}if(!currentPlayer(expected)){status=-3;return finish(out);}
 uintptr_t p=playerData();if(!p){status=-3;return finish(out);}if(lease.owner!=p){lease.owner_changed(p);memset(retryAt,0,sizeof(retryAt));memset(retryItem,0,sizeof(retryItem));}
 uint32_t style=0,left=0,right=0;if(!activeSlots(p,style,left,right)){status=-3;return finish(out);}
 if(desired==0){
  // Restore every slot we touched, including slots the user cycled away from.
  // A manual change is never overwritten. Missing prior items are not re-granted.
  for(unsigned slot=0;slot<6;slot++){uint32_t actual=0,target=0;if(!equipped(p,slot,actual)){status=-3;return finish(out);}if(lease.restore_target(slot,actual,target)&&!equip(p,slot,target,true))return finish(out);}
  status=2;return finish(out);
 }
 // The current retargeted asset package has a right-hand weapon anchor only.
 // Reject native two-hand-left sword before modifying either equipment slot.
 if(desired==2&&style==2){status=-8;return finish(out);}
 uint32_t target=desired==2?native_melee_policy::sword:native_melee_policy::fists;
 // Clear the selected offhand too: leaving an old shield would make Unarmed
 // perform shield actions. All equipment changes pass through native EquipGear.
 if(!equip(p,left,native_melee_policy::fists,false)||!equip(p,right,target,false))return finish(out);
 uint32_t actualLeft=0,actualRight=0,checkStyle=0,checkLeft=0,checkRight=0;
 if(!activeSlots(p,checkStyle,checkLeft,checkRight)||checkLeft!=left||checkRight!=right||
    (desired==2&&checkStyle==2)||!equipped(p,left,actualLeft)||!equipped(p,right,actualRight)||
    actualLeft!=native_melee_policy::fists||actualRight!=target){status=-7;return finish(out);}
 status=2;return finish(out,desired,checkStyle==2?1:0,checkStyle==2?left:right,target);
}
extern "C" int native_melee_status(){return status;}
