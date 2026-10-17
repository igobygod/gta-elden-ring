#pragma once
// Native Windows bootstrap. Wine retains the existing D3DMetal-only path.
// These hooks are a compiled candidate until verified on native Windows hardware.
#include <MinHook.h>
namespace windows_swapchain {
using Execute=void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*,UINT,ID3D12CommandList*const*);
using Create=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**);
using CreateHwnd=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,HWND,const DXGI_SWAP_CHAIN_DESC1*,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,IDXGIOutput*,IDXGISwapChain1**);
using CreateComposition=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,const DXGI_SWAP_CHAIN_DESC1*,IDXGIOutput*,IDXGISwapChain1**);
static Execute originalExecute=nullptr;static Create originalCreate=nullptr;
static CreateHwnd originalHwnd=nullptr;static CreateComposition originalComposition=nullptr;
static SRWLOCK queuesLock=SRWLOCK_INIT;
struct Queue {ID3D12CommandQueue* queue=nullptr;ID3D12Device* device=nullptr;};
struct Association {IUnknown* swapchain=nullptr;ID3D12CommandQueue* queue=nullptr;};
static Queue queues[8];static Association associations[8];static bool overflow=false;
static bool same_identity(IUnknown*a,IUnknown*b){
 if(!a||!b)return false;IUnknown*x=nullptr,*y=nullptr;
 bool ok=SUCCEEDED(a->QueryInterface(__uuidof(IUnknown),(void**)&x))&&SUCCEEDED(b->QueryInterface(__uuidof(IUnknown),(void**)&y))&&x==y;
 if(x)x->Release();if(y)y->Release();return ok;
}
static void remember(ID3D12CommandQueue*q){
 if(!q||q->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT)return;
 AcquireSRWLockExclusive(&queuesLock);
 for(auto&v:queues)if(v.queue==q){ReleaseSRWLockExclusive(&queuesLock);return;}
 for(auto&v:queues)if(!v.queue){ID3D12Device*d=nullptr;if(SUCCEEDED(q->GetDevice(__uuidof(ID3D12Device),(void**)&d))){q->AddRef();v={q,d};}ReleaseSRWLockExclusive(&queuesLock);return;}
 overflow=true;ReleaseSRWLockExclusive(&queuesLock);
}
static void associate(IUnknown*sc,IUnknown*device){
 if(!sc||!device)return;ID3D12CommandQueue*q=nullptr;IUnknown*identity=nullptr;
 if(FAILED(device->QueryInterface(__uuidof(ID3D12CommandQueue),(void**)&q)))return;
 if(q->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT||FAILED(sc->QueryInterface(__uuidof(IUnknown),(void**)&identity))){q->Release();return;}
 remember(q);AcquireSRWLockExclusive(&queuesLock);
 for(auto&v:associations)if(v.swapchain==identity){ReleaseSRWLockExclusive(&queuesLock);identity->Release();q->Release();return;}
 for(auto&v:associations)if(!v.swapchain){v={identity,q};ReleaseSRWLockExclusive(&queuesLock);return;}
 ReleaseSRWLockExclusive(&queuesLock);identity->Release();q->Release();
}
static void STDMETHODCALLTYPE execute(ID3D12CommandQueue*q,UINT count,ID3D12CommandList*const*lists){remember(q);originalExecute(q,count,lists);}
static HRESULT STDMETHODCALLTYPE create(IDXGIFactory*f,IUnknown*d,DXGI_SWAP_CHAIN_DESC*desc,IDXGISwapChain**sc){auto hr=originalCreate(f,d,desc,sc);if(SUCCEEDED(hr)&&sc)associate(*sc,d);return hr;}
static HRESULT STDMETHODCALLTYPE hwnd(IDXGIFactory2*f,IUnknown*d,HWND w,const DXGI_SWAP_CHAIN_DESC1*desc,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*fs,IDXGIOutput*o,IDXGISwapChain1**sc){auto hr=originalHwnd(f,d,w,desc,fs,o,sc);if(SUCCEEDED(hr)&&sc)associate(*sc,d);return hr;}
static HRESULT STDMETHODCALLTYPE composition(IDXGIFactory2*f,IUnknown*d,const DXGI_SWAP_CHAIN_DESC1*desc,IDXGIOutput*o,IDXGISwapChain1**sc){auto hr=originalComposition(f,d,desc,o,sc);if(SUCCEEDED(hr)&&sc)associate(*sc,d);return hr;}
static ID3D12CommandQueue* for_swapchain(IDXGISwapChain*sc,ID3D12Device*dev){
 IUnknown*identity=nullptr;if(FAILED(sc->QueryInterface(__uuidof(IUnknown),(void**)&identity)))return nullptr;
 AcquireSRWLockShared(&queuesLock);ID3D12CommandQueue*answer=nullptr;
 for(auto&v:associations)if(v.swapchain==identity){answer=v.queue;break;}
 // Attach-late fallback: accept only one direct queue for the exact device.
 // Multiple queues are ambiguous, so skip drawing rather than submit unsafely.
 if(!answer&&!overflow){for(auto&v:queues)if(v.queue&&same_identity(v.device,dev)){if(answer){answer=nullptr;break;}answer=v.queue;}}
 ReleaseSRWLockShared(&queuesLock);identity->Release();return answer;
}
static bool install(){
 if(g_pp)return true;
 HINSTANCE instance=GetModuleHandleW(nullptr);const wchar_t*cls=L"igobygod-D3D12-bootstrap";
 WNDCLASSW wc={};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=instance;wc.lpszClassName=cls;
 ATOM atom=RegisterClassW(&wc);if(!atom&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
 HWND window=CreateWindowExW(0,cls,L"igobygod bootstrap",WS_OVERLAPPED,0,0,32,32,nullptr,nullptr,instance,nullptr);
 if(!window){if(atom)UnregisterClassW(cls,instance);return false;}
 IDXGIFactory4*factory=nullptr;ID3D12Device*device=nullptr;ID3D12CommandQueue*queue=nullptr;IDXGISwapChain1*swap=nullptr;IDXGISwapChain3*swap3=nullptr;
 auto cleanup=[&]{safe_release(swap3);safe_release(swap);safe_release(queue);safe_release(device);safe_release(factory);DestroyWindow(window);if(atom)UnregisterClassW(cls,instance);};
 D3D12_COMMAND_QUEUE_DESC qd={};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
 DXGI_SWAP_CHAIN_DESC1 sd={};sd.Width=sd.Height=32;sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=2;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
 if(FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory4),(void**)&factory))||FAILED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,__uuidof(ID3D12Device),(void**)&device))||FAILED(device->CreateCommandQueue(&qd,__uuidof(ID3D12CommandQueue),(void**)&queue))||FAILED(factory->CreateSwapChainForHwnd(queue,window,&sd,nullptr,nullptr,&swap))||FAILED(swap->QueryInterface(__uuidof(IDXGISwapChain3),(void**)&swap3))){cleanup();return false;}
 auto pg=new PresentPage{};auto table=*(void***)swap3;auto ft=*(void***)factory;auto qt=*(void***)queue;
 void* targets[8]={table[8],table[13],table[22],table[39],qt[10],ft[10],ft[15],ft[24]};
 void* hooks[8]={(void*)&hkPresent,(void*)&hkResizeBuffers,(void*)&hkPresent1,(void*)&hkResizeBuffers1,(void*)&execute,(void*)&create,(void*)&hwnd,(void*)&composition};
 void** outputs[8]={&pg->orig[0],&pg->orig[1],&pg->orig[2],&pg->orig[3],reinterpret_cast<void**>(&originalExecute),reinterpret_cast<void**>(&originalCreate),reinterpret_cast<void**>(&originalHwnd),reinterpret_cast<void**>(&originalComposition)};
 MH_STATUS init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED){delete pg;cleanup();return false;}
 int created=0;for(;created<8;created++)if(MH_CreateHook(targets[created],hooks[created],outputs[created])!=MH_OK)break;
 if(created!=8){for(int i=0;i<created;i++)MH_RemoveHook(targets[i]);delete pg;cleanup();return false;}
 // Publish original trampolines before enabling any callback.
 g_pp=pg;g_nativeWindows=true;
 bool enabled=true;for(auto target:targets)if(MH_QueueEnableHook(target)!=MH_OK)enabled=false;
 if(enabled)enabled=MH_ApplyQueued()==MH_OK;
 if(!enabled){
  for(auto target:targets)MH_DisableHook(target);
  // A callback may already be inside an original trampoline. Keep that tiny
  // allocation and its originals alive; removing/freeing them here can race it.
  // Do not retry hooks against an incompletely installed set.
  log("Native Windows hook activation failed; drawing disabled until restart");
  g_failed=true;cleanup();return true;
 }
 cleanup();log("Native Windows swapchain hooks installed");return true;
}
}
static ID3D12CommandQueue* native_windows_queue(IDXGISwapChain*sc,ID3D12Device*dev){return windows_swapchain::for_swapchain(sc,dev);}
