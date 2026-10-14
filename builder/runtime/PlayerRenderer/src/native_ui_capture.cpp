#include "native_ui_capture.h"
#include <windows.h>
#include <intrin.h>
#include <atomic>
#include <cstring>
#include <cstdio>
extern "C" int WINAPI MH_Initialize();
extern "C" int WINAPI MH_CreateHook(void*,void*,void**);
extern "C" int WINAPI MH_EnableHook(void*);
extern "C" int WINAPI MH_RemoveHook(void*);
namespace {
using Barrier=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,const D3D12_RESOURCE_BARRIER*);
Barrier original=nullptr;uintptr_t base=0;ID3D12Device*device=nullptr;
ID3D12Resource*captures[2]={};D3D12_RESOURCE_DESC format={};
std::atomic<uint64_t> completed{0};std::atomic<bool> ready{false};
SRWLOCK lock=SRWLOCK_INIT;
// Native begin/end may be recorded on different command lists/worker threads.
// Pair by the render target; commands retain their original native queue order.
SRWLOCK passLock=SRWLOCK_INIT;
ID3D12Resource*source=nullptr;
ID3D12GraphicsCommandList*command=nullptr;
uint64_t began=0;
DWORD beginThread=0;
void report(const char*what){FILE*f=igobygod::open(igobygod::log_path("ui-capture.log").c_str(),"a");if(f){fprintf(f,"%llu %s\n",GetTickCount64(),what);fclose(f);}}
bool signature(uintptr_t at,const unsigned char*expected,size_t n){MEMORY_BASIC_INFORMATION m={};return VirtualQuery((void*)at,&m,sizeof(m))&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&at+n<=(uintptr_t)m.BaseAddress+m.RegionSize&&!memcmp((void*)at,expected,n);}
bool texture(ID3D12Resource*r){
 auto d=r->GetDesc();
 static unsigned observations=0;if(observations++<3){char line[128];sprintf_s(line,"native UI target %llux%u format=%u",d.Width,d.Height,unsigned(d.Format));report(line);}
 if(d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||d.SampleDesc.Count!=1||d.DepthOrArraySize!=1||d.MipLevels!=1||d.Width<320||d.Width>4096||d.Height<200||d.Height>2160||
    (d.Format!=DXGI_FORMAT_R8G8B8A8_UNORM&&d.Format!=DXGI_FORMAT_B8G8R8A8_UNORM&&d.Format!=DXGI_FORMAT_R10G10B10A2_UNORM))return false;
 if(ready.load(std::memory_order_acquire))return d.Width==format.Width&&d.Height==format.Height&&d.Format==format.Format;
 AcquireSRWLockExclusive(&lock);
 if(!ready.load()){
  D3D12_HEAP_PROPERTIES heap={};heap.Type=D3D12_HEAP_TYPE_DEFAULT;heap.CreationNodeMask=heap.VisibleNodeMask=1;
  auto c=d;c.Flags=D3D12_RESOURCE_FLAG_NONE;c.Alignment=0;
  ID3D12Resource*a=nullptr,*b=nullptr;
  bool ok=SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&c,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,__uuidof(ID3D12Resource),(void**)&a))&&
          SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&c,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,__uuidof(ID3D12Resource),(void**)&b));
  if(ok){captures[0]=a;captures[1]=b;format=d;ready.store(true,std::memory_order_release);char line[128];sprintf_s(line,"allocated two UI textures %llux%u format=%u",d.Width,d.Height,unsigned(d.Format));report(line);}
  else {if(a)a->Release();if(b)b->Release();}
 }
 bool result=ready.load()&&d.Width==format.Width&&d.Height==format.Height&&d.Format==format.Format;
 ReleaseSRWLockExclusive(&lock);return result;
}
void copy(ID3D12GraphicsCommandList*cmd,ID3D12Resource*src,ID3D12Resource*dst){
 D3D12_RESOURCE_BARRIER b[2]={};
 for(auto&x:b){x.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;x.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;}
 b[0].Transition={src,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE};
 b[1].Transition={dst,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST};
 original(cmd,2,b);cmd->CopyResource(dst,src);
 for(auto&x:b){auto s=x.Transition.StateBefore;x.Transition.StateBefore=x.Transition.StateAfter;x.Transition.StateAfter=s;}
 original(cmd,2,b);
}
bool pair(UINT n,const D3D12_RESOURCE_BARRIER*b,bool begin){
 if(n!=2||!b)return false;
 const auto&c=b[0];const auto&d=b[1];
 return c.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION&&d.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION&&
  c.Flags==D3D12_RESOURCE_BARRIER_FLAG_NONE&&d.Flags==D3D12_RESOURCE_BARRIER_FLAG_NONE&&
  c.Transition.pResource&&c.Transition.Subresource==D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES&&
  c.Transition.StateBefore==(begin?0xc0:4)&&c.Transition.StateAfter==(begin?4:0xc0)&&
  d.Transition.StateBefore==(begin?0xc0:0x10)&&d.Transition.StateAfter==(begin?0x10:0xc0);
}
__declspec(noinline) void STDMETHODCALLTYPE hook(ID3D12GraphicsCommandList*cmd,UINT n,const D3D12_RESOURCE_BARRIER*b){
 auto caller=(uintptr_t)_ReturnAddress();
 bool begin=caller==base+0xD7F367,end=caller==base+0xD7F4A9;
 if(!begin&&!end){original(cmd,n,b);return;}
 static std::atomic<unsigned> diagnostics{0};
 if(!pair(n,b,begin)){
  if(diagnostics.fetch_add(1)<8)report(begin?"UI begin barrier mismatch":"UI end barrier mismatch");
  original(cmd,n,b);return;
 }
 AcquireSRWLockExclusive(&passLock);
 auto target=b[0].Transition.pResource;
 if(begin){
  original(cmd,n,b);
  if(texture(target)){
   source=target;command=cmd;began=GetTickCount64();beginThread=GetCurrentThreadId();
   completed.store(0,std::memory_order_release);copy(cmd,target,captures[0]);
  }
 }else{
  auto now=GetTickCount64();
  if(source==target&&began&&now>=began&&now-began<=1000){
   copy(cmd,target,captures[1]);completed.store(now,std::memory_order_release);
   static unsigned samples=0;if(samples++<8){char line[192];sprintf_s(line,"UI pair completed same_list=%u same_thread=%u elapsed_ms=%llu",command==cmd,beginThread==GetCurrentThreadId(),now-began);report(line);}
   source=nullptr;command=nullptr;began=0;
  }else if(diagnostics.fetch_add(1)<8){char line[192];sprintf_s(line,"UI end unmatched source=%p target=%p same_list=%u age=%llu",source,target,command==cmd,began&&now>=began?now-began:0);report(line);}
  original(cmd,n,b);
 }
 ReleaseSRWLockExclusive(&passLock);
}
}
int native_ui_capture_init(ID3D12Device*dev,ID3D12GraphicsCommandList*cmd){
 if(original)return 1;base=(uintptr_t)GetModuleHandleW(nullptr);
 const unsigned char a[]={0x48,0x8B,0x01,0xFF,0x90,0xD0,0,0,0};
 // Exact native begin/end barriers, independently disassembled from retail.
 if(!signature(base+0xD7F35F,a,sizeof(a))||!signature(base+0xD7F4A1,a,sizeof(a))){report("signature mismatch; native UI untouched");return 0;}
 // Patch only the two audited CALL instructions in the native game. The
 // CrossOver COM method table lives outside Wine's writable image regions.
 // A nearby relay preserves the native call ABI; the original COM method is
 // obtained directly from each command list and remains completely untouched.
 HMODULE pinned=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&native_ui_capture_init,&pinned))return 0;
 unsigned char*relay=nullptr;
 for(uintptr_t at=(base+0x6000000+65535)&~uintptr_t(65535);at<base+0x60000000;at+=65536){
  MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery((void*)at,&m,sizeof(m)))break;
  if(m.State==MEM_FREE){relay=(unsigned char*)VirtualAlloc((void*)at,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(relay)break;}
  else {uintptr_t end=(uintptr_t)m.BaseAddress+m.RegionSize;if(end>at+65536)at=((end+65535)&~uintptr_t(65535))-65536;}
 }
 if(!relay){report("UI relay allocation failed");return 0;}
 relay[0]=0x48;relay[1]=0xB8;uintptr_t destination=(uintptr_t)&hook;memcpy(relay+2,&destination,8);relay[10]=0xFF;relay[11]=0xE0;
 DWORD old=0;if(!VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old)){VirtualFree(relay,0,MEM_RELEASE);return 0;}
 FlushInstructionCache(GetCurrentProcess(),relay,12);
 struct Patch{uintptr_t at;LONG64 before,after;};Patch edits[2]={};
 const uintptr_t calls[2]={0xD7F362,0xD7F4A4};
 for(int i=0;i<2;i++){auto&e=edits[i];e.at=base+calls[i];memcpy(&e.before,(void*)e.at,8);e.after=e.before;
  int64_t delta=(intptr_t)relay-intptr_t(e.at+5);if(delta<INT32_MIN||delta>INT32_MAX){VirtualFree(relay,0,MEM_RELEASE);return 0;}
  unsigned char*bytes=(unsigned char*)&e.after;bytes[0]=0xE8;int32_t rel=(int32_t)delta;memcpy(bytes+1,&rel,4);bytes[5]=0x90;
 }
 original=[](ID3D12GraphicsCommandList*list,UINT n,const D3D12_RESOURCE_BARRIER*b){auto call=(Barrier)(*(void***)list)[26];call(list,n,b);};
 device=dev;device->AddRef();int changed=0;
 for(auto&e:edits){DWORD access=0;if(!VirtualProtect((void*)e.at,8,PAGE_EXECUTE_READWRITE,&access))break;
  bool ok=InterlockedCompareExchange64((volatile LONG64*)e.at,e.after,e.before)==e.before;
  DWORD ignored=0;VirtualProtect((void*)e.at,8,access,&ignored);FlushInstructionCache(GetCurrentProcess(),(void*)e.at,8);if(!ok)break;changed++;
 }
 if(changed!=2){for(int i=0;i<changed;i++){auto&e=edits[i];DWORD access=0;if(VirtualProtect((void*)e.at,8,PAGE_EXECUTE_READWRITE,&access)){InterlockedCompareExchange64((volatile LONG64*)e.at,e.before,e.after);DWORD ignored=0;VirtualProtect((void*)e.at,8,access,&ignored);FlushInstructionCache(GetCurrentProcess(),(void*)e.at,8);}}
  // Keep relay/device alive if an already-entered callback is finishing.
  report("UI callsite installation incomplete; originals restored");return 0;
 }
 report("native UI capture hook installed");return 1;
}
int native_ui_capture_snapshot(uint64_t now,UINT w,UINT h,DXGI_FORMAT f,ID3D12Resource**a,ID3D12Resource**b){
 uint64_t t=completed.load(std::memory_order_acquire);
 if(!a||!b||!t||now<t||now-t>150||!ready.load(std::memory_order_acquire)||format.Width!=w||format.Height!=h||format.Format!=f)return 0;
 *a=captures[0];*b=captures[1];return 1;
}
