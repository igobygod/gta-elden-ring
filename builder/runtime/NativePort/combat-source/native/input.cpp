#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <stdint.h>
#include <atomic>
#include "input.h"
#include "input_policy.h"
extern "C" int car_controls_owned(uintptr_t);
// Microsoft's documented DirectInput ABI; no Elden Ring address or memory write.
// The executable imports DirectInput8Create and contains canonical relative mouse2/keyboard formats.
extern "C" {
 void shoulder_camera_mouse_motion(int dx,int dy);
 int shoulder_camera_mouse_ready();
 int shoulder_camera_scroll(int delta);
 int shoulder_camera_scroll_ready();
 int WINAPI MH_Initialize();
 int WINAPI MH_CreateHook(void*,void*,void**);
 int WINAPI MH_EnableHook(void*);
 int WINAPI MH_DisableHook(void*);
 int WINAPI MH_RemoveHook(void*);
}
namespace {
std::atomic<bool> ready{false},owns_world{false},wheel_open{false},animation_aim{false};
std::atomic<uint64_t> context_time{0},console_time{0};
std::atomic<bool> console_block{false};
std::atomic<int32_t> dx{0},dy{0},scroll{0};
std::atomic<uint32_t> buttons{0},state_calls{0},buffer_calls{0};
std::atomic<bool> keyboard_seen{false};
std::atomic<unsigned> capture_path{0}; // select a single read API, so state+buffered don't double-count
DWORD process_id;
struct Device {void* ptr;DWORD kind;};Device devices[32]={};SRWLOCK device_lock=SRWLOCK_INIT;
using StateFn=HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*,DWORD,void*);
using DataFn=HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*,DWORD,DIDEVICEOBJECTDATA*,DWORD*,DWORD);
StateFn original_state[2]={};DataFn original_data[2]={};void* targets[4]={};unsigned target_count=0;
DWORD device_kind(IDirectInputDevice8W* self){
 AcquireSRWLockShared(&device_lock);for(auto& d:devices)if(d.ptr==self){auto kind=d.kind;ReleaseSRWLockShared(&device_lock);return kind;}ReleaseSRWLockShared(&device_lock);
 DIDEVCAPS caps={};caps.dwSize=sizeof(caps);if(FAILED(self->GetCapabilities(&caps)))return 0;
 DWORD kind=GET_DIDEVICE_TYPE(caps.dwDevType);if(kind!=DI8DEVTYPE_MOUSE&&kind!=DI8DEVTYPE_KEYBOARD)return 0;
 AcquireSRWLockExclusive(&device_lock);for(auto& d:devices)if(!d.ptr||d.ptr==self){d={self,kind};break;}ReleaseSRWLockExclusive(&device_lock);return kind;
}
input_policy::Policy policy(){
 auto consoleNow=GetTickCount64();DWORD consolePid=0;GetWindowThreadProcessId(GetForegroundWindow(),&consolePid);
 auto consoleAt=console_time.load();if(console_block.load()&&consolePid==process_id&&consoleNow>=consoleAt&&consoleNow-consoleAt<250)return {true,false,false,true};
 if(!ready.load(std::memory_order_acquire)||!owns_world.load(std::memory_order_relaxed))return {false,false};
 auto now=GetTickCount64(),at=context_time.load(std::memory_order_relaxed);
 // Restore vanilla input if the caller stops updating (pause/loading/fault).
 if(now<at||now-at>150)return {false,false};
 DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
 if(pid!=process_id)return {false,false};
 return {true,wheel_open.load(std::memory_order_relaxed)||GetAsyncKeyState(VK_TAB)<0,animation_aim.load(std::memory_order_relaxed),false,shoulder_camera_mouse_ready()!=0,car_controls_owned(0)!=0};
}
void add_motion(int32_t x,int32_t y,int32_t z){
 // Clamp individual hardware samples; never let an overflow/absolute-mode packet jump the wheel.
 if(x>=-32768&&x<=32768)dx.fetch_add(x,std::memory_order_relaxed);
 if(y>=-32768&&y<=32768)dy.fetch_add(y,std::memory_order_relaxed);
 if(z>=-32768&&z<=32768)scroll.fetch_add(z,std::memory_order_relaxed);
}
void camera_motion(int32_t x,int32_t y,input_policy::Policy p){
 if(!p.owns_world||!p.free_aim||p.wheel||p.console)return;
 if(x< -32768||x>32768||y< -32768||y>32768)return;
 if(x||y)shoulder_camera_mouse_motion(x,y);
}
void read_state(DWORD kind,DWORD size,void* data,input_policy::Policy p){
 if(kind==DI8DEVTYPE_MOUSE&&(size==sizeof(DIMOUSESTATE)||size==sizeof(DIMOUSESTATE2))){
  state_calls.fetch_add(1,std::memory_order_relaxed);
  unsigned no_path=0;capture_path.compare_exchange_strong(no_path,1);
  auto* m=(DIMOUSESTATE*)data;
  if(capture_path.load(std::memory_order_relaxed)==1){
   buttons.store((m->rgbButtons[0]&0x80?1u:0u)|(m->rgbButtons[1]&0x80?2u:0u),std::memory_order_relaxed);
   camera_motion(m->lX,m->lY,p);
   if(p.owns_world&&!p.wheel&&!p.console&&m->lZ&&shoulder_camera_scroll(m->lZ))m->lZ=0;
   if(p.owns_world&&p.wheel)add_motion(m->lX,m->lY,m->lZ);
  }
  input_policy::mouse(data,size,p);
 }else if(kind==DI8DEVTYPE_KEYBOARD&&size==256){keyboard_seen.store(true,std::memory_order_relaxed);input_policy::keyboard(data,size,p);}
}
template<unsigned I>HRESULT STDMETHODCALLTYPE state_hook(IDirectInputDevice8W* self,DWORD size,void* data){
 HRESULT result=original_state[I](self,size,data);
 if(SUCCEEDED(result)&&data)read_state(device_kind(self),size,data,policy());return result;
}
template<unsigned I>HRESULT STDMETHODCALLTYPE data_hook(IDirectInputDevice8W* self,DWORD stride,DIDEVICEOBJECTDATA* data,DWORD* count,DWORD flags){
 DWORD capacity=count?*count:0;HRESULT result=original_data[I](self,stride,data,count,flags);
 if(FAILED(result)||!data||!count||(stride!=sizeof(DIDEVICEOBJECTDATA)&&stride!=16)||*count>capacity||*count>65536)return result;
 DWORD kind=device_kind(self);if(kind!=DI8DEVTYPE_MOUSE&&kind!=DI8DEVTYPE_KEYBOARD)return result;
 auto p=policy();bool mouse=kind==DI8DEVTYPE_MOUSE;
 if(!mouse)keyboard_seen.store(true,std::memory_order_relaxed);
 if(mouse){buffer_calls.fetch_add(1,std::memory_order_relaxed);unsigned no_path=0;if(!(flags&DIGDD_PEEK))capture_path.compare_exchange_strong(no_path,2);}
 for(DWORD i=0;i<*count;i++){
  auto event=(DIDEVICEOBJECTDATA*)((uint8_t*)data+(size_t)i*stride);
  // Peek packets are filtered for the game but never accumulated a second time.
  if(mouse&&!(flags&DIGDD_PEEK)&&capture_path.load(std::memory_order_relaxed)==2){
   if(event->dwOfs==0)camera_motion((int32_t)event->dwData,0,p);else if(event->dwOfs==4)camera_motion(0,(int32_t)event->dwData,p);
   if(event->dwOfs==12||event->dwOfs==13){uint32_t bit=event->dwOfs==12?1:2;if(event->dwData&0x80)buttons.fetch_or(bit);else buttons.fetch_and(~bit);}
   if(p.owns_world&&p.wheel){if(event->dwOfs==0)add_motion((int32_t)event->dwData,0,0);else if(event->dwOfs==4)add_motion(0,(int32_t)event->dwData,0);else if(event->dwOfs==8)add_motion(0,0,(int32_t)event->dwData);}
  }
  if(mouse&&event->dwOfs==8&&p.owns_world&&!p.wheel&&!p.console){
   if(flags&DIGDD_PEEK){if(shoulder_camera_scroll_ready())event->dwData=0;}
   else if(capture_path.load(std::memory_order_relaxed)==2&&shoulder_camera_scroll((int32_t)event->dwData))event->dwData=0;
  }
  event->dwData=input_policy::event(mouse,event->dwOfs,event->dwData,p);
 }return result;
}
bool add_hook(void* target,void* detour,void** original){
 for(unsigned i=0;i<target_count;i++)if(targets[i]==target)return true;
 if(MH_CreateHook(target,detour,original)!=0)return false;targets[target_count++]=target;return true;
}
void cleanup(){for(unsigned i=0;i<target_count;i++){MH_DisableHook(targets[i]);MH_RemoveHook(targets[i]);}target_count=0;}
}
extern "C" int native_input_install(){
 if(ready.load(std::memory_order_acquire))return 1;
 process_id=GetCurrentProcessId();HMODULE pinned=nullptr;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&native_input_install,&pinned))return 0;
 int status=MH_Initialize();if(status!=0&&status!=1)return 0;
 IDirectInput8W* input=nullptr;IDirectInputDevice8W* mouse=nullptr;IDirectInputDevice8W* keyboard=nullptr;
 if(FAILED(DirectInput8Create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,(void**)&input,nullptr)))return 0;
 bool ok=SUCCEEDED(input->CreateDevice(GUID_SysMouse,&mouse,nullptr))&&SUCCEEDED(input->CreateDevice(GUID_SysKeyboard,&keyboard,nullptr));
 if(ok){
  // No acquisition or cooperative-level changes: these dummy objects only locate COM methods.
  void** m=*(void***)mouse;void** k=*(void***)keyboard;
  ok=add_hook(m[9],(void*)&state_hook<0>,(void**)&original_state[0])&&add_hook(m[10],(void*)&data_hook<0>,(void**)&original_data[0]);
  if(ok&&k[9]!=m[9])ok=add_hook(k[9],(void*)&state_hook<1>,(void**)&original_state[1]);
  if(ok&&k[10]!=m[10])ok=add_hook(k[10],(void*)&data_hook<1>,(void**)&original_data[1]);
  if(ok)for(unsigned i=0;i<target_count;i++)if(MH_EnableHook(targets[i])!=0){ok=false;break;}
 }
 if(keyboard)keyboard->Release();if(mouse)mouse->Release();input->Release();
 if(!ok){cleanup();return 0;}ready.store(true,std::memory_order_release);return 1;
}
extern "C" void native_input_set_context(int world,int wheel){
 if(!world||!wheel){dx.store(0);dy.store(0);scroll.store(0);}
 context_time.store(GetTickCount64(),std::memory_order_relaxed);
 wheel_open.store(world&&wheel,std::memory_order_relaxed);
 owns_world.store(world!=0,std::memory_order_release);
}
extern "C" void native_input_set_aim(int aiming){animation_aim.store(aiming!=0,std::memory_order_relaxed);}
extern "C" void native_input_take(NativeMouseInput* out){
 if(!out)return;
 out->dx=dx.exchange(0);out->dy=dy.exchange(0);out->wheel=scroll.exchange(0);out->buttons=buttons.load();
 out->state_calls=state_calls.load();out->buffer_calls=buffer_calls.load();
 // Do not let the caller take ownership until BOTH real device paths have been observed.
 // This prevents LMB double firing if another wrapper implementation bypasses our hook.
 out->ready=ready.load(std::memory_order_acquire)&&keyboard_seen.load()&&(out->state_calls||out->buffer_calls)?1:0;
}

extern "C" void native_input_set_console(int block){console_time.store(GetTickCount64());console_block.store(block!=0);}
