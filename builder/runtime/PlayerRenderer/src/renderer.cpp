#include "../car-drive/driver_pose.h"
#include "motion_blend.h"
#include "rifle.h"
#include "shoulder_camera.h"
#include "scope_reticle.h"
#include "../../NativePort/combat-source/native/gameplay_gate.h"
#include "../../NativePort/combat-source/native/effects.h"
#include "arsenal_animation.h"
#include "../polish-v4.4/melee-assets/melee_animation.h"
#include "../polish-v4.5/sword/sword_carry.h"
#include "../polish-v4.6/sword/sword_attack.h"
#include "grenade_pose.h"
#include "../polish-next/movement/donor_strafe.h"
#include "native_ui_state.h"
#include "native_lock_marker.h"
#include "ui_visibility_policy.h"
#include "../car-proof/car_render.h"
#include "../car-proof/vehicle_demo.h"
#include "player_status.h"
#include "cheat_console.h"
#include "scene_lifecycle.h"
#include "native_animation_policy.h"
#include "locomotion_mode_policy.h"
#include "mapped_animation.h"
#include "../polish-fix/muzzle_visual.h"
#include "../repair-next/fx/fx_runtime.h"
#include "common.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include "native_ui_capture.h"
#include "../car-drive/ui-composite.h"
#include <vector>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <intrin.h>
namespace mb {
volatile LONG g_inflight=0;
static SRWLOCK loglock=SRWLOCK_INIT;
uint64_t now_ms(){return GetTickCount64();}
void log(const char* fmt,...){AcquireSRWLockExclusive(&loglock);FILE* f=igobygod::open(igobygod::log_path("renderer.log").c_str(),"a");if(f){va_list a;va_start(a,fmt);vfprintf(f,fmt,a);va_end(a);fputc('\n',f);fclose(f);}ReleaseSRWLockExclusive(&loglock);}
enum { kSlotPresent = 0, kSlotResize, kSlotPresent1, kSlotResize1, kHookCount };
static const int kVtblIndex[kHookCount] = {8, 13, 22, 39};  // IDXGISwapChain4 table slots
static const char* const kHookNames[kHookCount] = {"Present", "ResizeBuffers", "Present1", "ResizeBuffers1"};

struct PresentPage {
    uint64_t magic;
    void** table;              // D3DMetal's shared IDXGISwapChain4 table that we patched
    void* orig[kHookCount];    // its original entries
    void* target[kHookCount];  // this core's hooks (null: the stubs go straight to the originals)
    void* unused;
    uint8_t stub[kHookCount][32];
};

static PresentPage* g_pp = nullptr;
static bool g_nativeWindows=false;
static ID3D12CommandQueue* native_windows_queue(IDXGISwapChain*,ID3D12Device*);

static uint64_t page_magic() { return 0x5052455345545250ull ^ GetCurrentProcessId(); }

// mov rax,[rip+target]; test rax,rax; jz +2; jmp rax; jmp [rip+orig]
static void write_stub(PresentPage* pg, int k) {
    uint8_t* s = pg->stub[k];
    uint8_t code[20] = {0x48, 0x8B, 0x05, 0, 0, 0, 0, 0x48, 0x85, 0xC0, 0x74, 0x02, 0xFF, 0xE0, 0xFF, 0x25, 0, 0, 0, 0};
    int32_t d1 = (int32_t)((uint8_t*)&pg->target[k] - (s + 7));
    int32_t d2 = (int32_t)((uint8_t*)&pg->orig[k] - (s + 20));
    memcpy(code + 3, &d1, 4);
    memcpy(code + 16, &d2, 4);
    memcpy(s, code, sizeof(code));
}

// D3DMetal's objects live in Mach-O memory, which Wine's VirtualQuery reports as free, so
// mem_readable() rejects them. IsBadReadPtr really reads (with a fault handler) instead.
static bool readable(const void* p, size_t n) { return p && !IsBadReadPtr(p, n); }

template <typename T>
static void safe_release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

// D3DMetal builds the hookable IDXGISwapChain4 table (41 entries) and its 16-byte stubs
// (`sub rcx,0x10; mov rax,<D3DMetal method>; jmp rax`) inside dxgi.dll's own .data scratch
// space. Find the table there. Never make a swapchain to learn it: the
// windows that needs, destroyed from our thread, deadlocked Wine's surface code against the
// game's main thread.
// Only the stub's tail is checked: other tools (the Steam overlay) overwrite the first 5
// bytes of some stubs (Release, Present, ResizeBuffers...) with a jmp to their own hooks.
static bool is_dxgi_stub(uintptr_t a, uintptr_t lo, uintptr_t hi) {
    if (a < lo || a + 16 > hi || (a & 0xF)) return false;
    const uint8_t* p = (const uint8_t*)a;
    return p[14] == 0xFF && p[15] == 0xE0;
}

static void** find_swapchain_table() {
    uint8_t* base = (uint8_t*)GetModuleHandleA("dxgi.dll");
    if (!base || !mem_readable(base, 0x1000)) return nullptr;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    uintptr_t lo = (uintptr_t)base, hi = lo + nt->OptionalHeader.SizeOfImage;
    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    void** found = nullptr;
    int tables41 = 0, tables32 = 0;
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (!(sec[i].Characteristics & IMAGE_SCN_MEM_WRITE)) continue;
        uintptr_t a = lo + sec[i].VirtualAddress, e = a + sec[i].Misc.VirtualSize;
        a = (a + 7) & ~(uintptr_t)7;
        // Page by page: only committed, readable pages are looked at.
        int run = 0;
        uintptr_t runStart = 0;
        for (uintptr_t page = a & ~(uintptr_t)0xFFF; page < e; page += 0x1000) {
            if (!mem_readable((void*)page, 0x1000)) {
                run = 0;
                continue;
            }
            uintptr_t q = page < a ? a : page, qe = page + 0x1000 < e ? page + 0x1000 : e;
            for (; q + 8 <= qe; q += 8) {
                uintptr_t v = *(const uintptr_t*)q;
                if (is_dxgi_stub(v, lo, hi)) {
                    if (run++ == 0) runStart = q;
                    continue;
                }
                if (run == 41) {
                    tables41++;
                    if (!found) found = (void**)runStart;
                } else if (run == 32) {
                    tables32++;
                }
                run = 0;
            }
        }
    }
    log("compositor: dxgi.dll scratch space holds %d swapchain table(s) and %d factory table(s)", tables41, tables32);
    return tables41 == 1 ? found : nullptr;
}

static const int kRing=3,kMaxBuffers=8,kSrvHostDepth=1;
struct RingSlot{ID3D12CommandAllocator* alloc=nullptr;ID3D12Resource* upload=nullptr;uint8_t* uploadPtr=nullptr;uint64_t fence=0;};
static IDXGISwapChain3* g_sc=nullptr;static ID3D12Device* g_dev=nullptr;static ID3D12CommandQueue* g_queue=nullptr;
static ID3D12RootSignature* g_rootSig=nullptr;static ID3D12PipelineState* g_pso=nullptr;static ID3D12PipelineState* g_fxPso=nullptr;static ID3D12PipelineState* g_uiPso=nullptr;static ID3D12PipelineState *g_blastAddPso=nullptr,*g_blastAlphaPso=nullptr;
static ID3D12GraphicsCommandList* g_list=nullptr;static ID3D12Fence* g_fence=nullptr;static HANDLE g_fenceEvent=nullptr;
static uint64_t g_fenceNext=0;static RingSlot g_ring[kRing];static unsigned g_ringPos=0;
static ID3D12DescriptorHeap *g_srvHeap=nullptr,*g_rtvHeap=nullptr,*g_dsvHeap=nullptr;
static UINT g_srvInc=0,g_rtvInc=0;static ID3D12Resource* g_bb[kMaxBuffers]={};static UINT g_bbCount=0,g_bbW=0,g_bbH=0;static DXGI_FORMAT g_bbFormat=DXGI_FORMAT_UNKNOWN;
static ID3D12Resource *ownDepth=nullptr,*atlas=nullptr,*atlasUpload=nullptr;static bool atlasReady=false,g_failed=false;
static ID3D12Resource *blastAtlas=nullptr,*blastUpload=nullptr,*mapAtlas=nullptr,*mapUpload=nullptr,*swordAtlas=nullptr,*swordUpload=nullptr;
static mapped_animation::Model modelFile;static mapped_animation::Rolls rollFile,deathFile;static mapped_animation::Span<uint32_t> indices;static mapped_animation::Span<float> uvs,poses;static unsigned nv=0,nf=0,nc=0;static float durations[4]={};static uint8_t* activePlayer=nullptr;static uint8_t* hiddenPlayer=nullptr;static bool wasVisible=false;static float facing[2]={0,1};static uint64_t lastDrawMs=0;static int nativeAnim=-1,nativeRawAnim=-1;static float nativeTime=0,nativeLength=0;
using RollClip=mapped_animation::Clip;
static std::vector<RollClip> rollClips,deathClips;static bool nativeDead=false;static donor_strafe::DirectionalRig directionalRig;

static arsenal::Rig arsenalRig,rpgRig,swordRig;
static car_driver::Animation driverAnimation;
static carasset::Asset carAsset;static ID3D12Resource *carAtlas=nullptr,*carUpload=nullptr;static ID3D12PipelineState* carPso=nullptr;static bool carReady=false;
static melee_animation::Packet meleePacket;
static size_t vertexCapacity=0;
struct Vertex{float x,y,z,w,u,v;float r=1,g=1,b=1,a=1,nu=0,nv=0,frameBlend=0,softness=0;};
static_assert(sizeof(Vertex)==56&&offsetof(Vertex,r)==24&&offsetof(Vertex,nu)==40&&offsetof(Vertex,softness)==52,"Vertex shader layout");
#include "gta_hud_renderer.h"
#include "cheat_ui.h"
#include "../polish-v4/body_lighting.h"
#include "body_projection.h"
#include "scene_integration.h"
#include "arsenal_ui.h"
typedef HRESULT(WINAPI* D3DCompile_t)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR,
                                      UINT, UINT, ID3DBlob**, ID3DBlob**);

static D3D12_HEAP_PROPERTIES heap_props(D3D12_HEAP_TYPE t) {
    D3D12_HEAP_PROPERTIES h = {};
    h.Type = t;
    h.CreationNodeMask = 1;
    h.VisibleNodeMask = 1;
    return h;
}

static D3D12_RESOURCE_DESC tex_desc(UINT w, UINT h, DXGI_FORMAT f) {
    D3D12_RESOURCE_DESC d = {};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = w;
    d.Height = h;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.Format = f;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    return d;
}

static void transition(ID3D12Resource* r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = from;
    b.Transition.StateAfter = to;
    g_list->ResourceBarrier(1, &b);
}

static D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu(int i) {
    D3D12_CPU_DESCRIPTOR_HANDLE h = g_srvHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += (SIZE_T)i * g_srvInc;
    return h;
}

static D3D12_CPU_DESCRIPTOR_HANDLE rtv_cpu(int i) {
    D3D12_CPU_DESCRIPTOR_HANDLE h = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += (SIZE_T)i * g_rtvInc;
    return h;
}

// r may be null: a null descriptor reads as 0 (the host depth slot stays null until the game's
// depth buffer is known; the shader only reads it when told to).
static void make_srv(ID3D12Resource* r, DXGI_FORMAT f, int slot) {
    D3D12_SHADER_RESOURCE_VIEW_DESC d = {};
    d.Format = f;
    d.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    d.Texture2D.MipLevels = 1;
    g_dev->CreateShaderResourceView(r, &d, srv_cpu(slot));
}

static bool wait_fence(uint64_t value, DWORD ms) {
    if (!value || g_fence->GetCompletedValue() >= value) return true;
    if (FAILED(g_fence->SetEventOnCompletion(value, g_fenceEvent))) return false;
    WaitForSingleObject(g_fenceEvent, ms);
    return g_fence->GetCompletedValue() >= value;
}

static void wait_idle() {
    if (!g_fence || !g_queue) return;
    uint64_t v = ++g_fenceNext;
    if (SUCCEEDED(g_queue->Signal(g_fence, v))) wait_fence(v, 2000);
}

constexpr uintptr_t kVtCGDepthStencilView = 0x30A7850;
constexpr uintptr_t kVtCGTexture2D = 0x30A7030;
static const int kMaxDepthScan = 32;
static const int kMaxDepthCands = 8;
struct DepthScanHit {
    uint8_t* dsv;
    uint8_t* tex;
    ID3D12Resource* res;
    uint32_t dsvFormat;
};
static DepthScanHit g_scanHits[kMaxDepthScan];
static int g_scanHitCount = 0;
static volatile LONG g_depthScanState = 0;  // 0 idle, 1 requested, 2 scanning (worker), 3 results ready
static ID3D12Resource* g_depthCands[kMaxDepthCands] = {};  // AddRef'd, full-screen, discovery order
static DXGI_FORMAT g_depthSrvFmt[kMaxDepthCands];
static int g_depthCandCount = 0;
static int g_depthBound = -1;           // candidate whose SRV is in the table
static uint64_t g_depthScanAtMs = 0;

static DXGI_FORMAT depth_srv_format(DXGI_FORMAT f) {
    switch (f) {
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_D16_UNORM: return DXGI_FORMAT_R16_UNORM;
        default: return DXGI_FORMAT_UNKNOWN;
    }
}

// Worker thread (compositor_poll): the heap scan takes a few seconds.
static void scan_depth_views() {
    uintptr_t base = main_module_base();
    uint64_t pat = base + kVtCGDepthStencilView;
    uint8_t mask[8];
    memset(mask, 0xFF, sizeof(mask));
    static uintptr_t hits[256];
    size_t n = mem_scan(0x10000, 0x7FFFFFFFFFFFull, (const uint8_t*)&pat, mask, 8, ERMC_SCAN_PRIVATE, hits, 256);
    int m = 0;
    for (size_t i = 0; i < n && m < kMaxDepthScan; i++) {
        uint8_t* dsv = (uint8_t*)hits[i];
        if (!mem_readable(dsv, 0x20)) continue;
        uint8_t* tex = *(uint8_t**)(dsv + 8);
        if (!tex || !mem_readable(tex, 0x28) || *(uintptr_t*)tex != base + kVtCGTexture2D) continue;
        ID3D12Resource* res = *(ID3D12Resource**)(tex + 0x20);
        if (!res) continue;
        bool dup = false;
        for (int k = 0; k < m; k++) dup |= g_scanHits[k].res == res;
        if (dup) continue;
        g_scanHits[m++] = {dsv, tex, res, *(uint32_t*)(dsv + 0x18)};
    }
    g_scanHitCount = m;
    log("compositor: %zu depth-stencil views in the heap, %d distinct textures", n, m);
}

static void release_depth() {
    for (int i = 0; i < g_depthCandCount; i++) safe_release(g_depthCands[i]);
    g_depthCandCount = 0;
    g_depthBound = -1;
    if (g_srvHeap) make_srv(nullptr, DXGI_FORMAT_R32_FLOAT, kSrvHostDepth);
}

// Present thread: take the worker's results.
static void adopt_depth_views() {
    uintptr_t base = main_module_base();
    release_depth();
    for (int i = 0; i < g_scanHitCount && g_depthCandCount < kMaxDepthCands; i++) {
        DepthScanHit& h = g_scanHits[i];
        // Still the same view -> texture -> resource chain (nothing was destroyed meanwhile)?
        if (!mem_readable(h.dsv, 0x20) || *(uintptr_t*)h.dsv != base + kVtCGDepthStencilView ||
            *(uint8_t**)(h.dsv + 8) != h.tex || !mem_readable(h.tex, 0x28) ||
            *(uintptr_t*)h.tex != base + kVtCGTexture2D || *(ID3D12Resource**)(h.tex + 0x20) != h.res)
            continue;
        if (!readable(h.res, 8)) continue;
        D3D12_RESOURCE_DESC d = h.res->GetDesc();
        bool full = d.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.Width == g_bbW && d.Height == g_bbH &&
                    d.SampleDesc.Count == 1 && d.DepthOrArraySize == 1 &&
                    !(d.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
        DXGI_FORMAT sf = depth_srv_format(d.Format);
        log("compositor: depth view %p: %llux%u format %u (dsv %u) %s", (void*)h.res, (unsigned long long)d.Width,
            d.Height, d.Format, h.dsvFormat & 0xFFFF, full && sf != DXGI_FORMAT_UNKNOWN ? "-> candidate" : "");
        if (!full || sf == DXGI_FORMAT_UNKNOWN) continue;
        h.res->AddRef();
        g_depthCands[g_depthCandCount] = h.res;
        g_depthSrvFmt[g_depthCandCount] = sf;
        g_depthCandCount++;
    }
    log("compositor: %d full-screen depth candidate(s)", g_depthCandCount);
}

static bool buffer(uint64_t bytes,ID3D12Resource** out){auto hp=heap_props(D3D12_HEAP_TYPE_UPLOAD);D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return SUCCEEDED(g_dev->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,__uuidof(ID3D12Resource),(void**)out));}
static ID3D12PipelineState* shadowPso=nullptr;
static ID3D12Resource* hudAtlas=nullptr;
static ID3D12Resource* hudUpload=nullptr;
static ID3D12Resource* sceneCopy=nullptr;
static ID3D12Resource* wastedAtlas=nullptr;
static ID3D12Resource* wastedUpload=nullptr;
static const char* shader=UI_COMPOSITE_HLSL R"(
Texture2D<float4> colorTex:register(t0);Texture2D<float> sceneDepth:register(t1);Texture2D<float4> blastTex:register(t2);Texture2D<float4> sceneColor:register(t3);Texture2D<float4> wastedTex:register(t4);Texture2D<float4> hudTex:register(t5);Texture2D<float4> mapTex:register(t6);Texture2D<float4> swordTex:register(t7);Texture2D<float4> carTex:register(t8);Texture2D<float4> uiBefore:register(t9);Texture2D<float4> uiAfter:register(t10);SamplerState smp:register(s0);
cbuffer Params:register(b0){float useDepth;float nearZ;float farZ;float padding;float2 viewportSize;float2 lightAnchor;};
struct V{float4 pos:SV_Position;float2 uv:TEXCOORD0;float4 color:COLOR0;float4 next:TEXCOORD1;};
V VS(float4 p:POSITION,float2 uv:TEXCOORD0,float4 color:COLOR0,float4 next:TEXCOORD1){V o;o.pos=p;o.uv=uv;o.color=color;o.next=next;return o;}
float4 UnderNativeUI(float4 custom,int2 pixel){
 if(padding<.5)return custom;
 float4 before=uiBefore.Load(int3(pixel,0)),after=uiAfter.Load(int3(pixel,0));
 // after = premultiplied UI + before * (1 - coverage). Alpha recovers
 // coverage exactly on transparent targets. An opaque target has no alpha
 // information: RGB supplies a conservative coverage bound, within capture
 // quantization for black subtitle fades and white/black glyphs.
 float coverage=uiCoverage(before.r,before.g,before.b,before.a,after.r,after.g,after.b,after.a);
 float3 ui=float3(uiPremul(before.r,after.r,coverage),uiPremul(before.g,after.g,coverage),uiPremul(before.b,after.b,coverage));
 // Transform only the new custom contribution. The destination already
 // contains native UI; replaying it fullscreen would apply its fade twice.
 // custom.a=0 also correctly attenuates additive effects beneath the UI.
 return float4(uiCompose(custom.r,ui.r,coverage,custom.a),uiCompose(custom.g,ui.g,coverage,custom.a),uiCompose(custom.b,ui.b,coverage,custom.a),custom.a);
}
float4 PS(V i):SV_Target{
 float scene=sceneDepth.Load(int3(i.pos.xy,0));if(useDepth>.5 && i.pos.z+0.000002<scene)discard;
 // Fixed-cost ambient tint: the former nine scene samples per fragment caused
 // avoidable GPU work and required a full-resolution scene copy every frame.
 // Fade the body if the collision camera enters its capsule. Dither keeps
 // proper depth writes without sorting transparent body triangles.
 const float threshold[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
 uint2 cell=uint2(i.pos.xy)&3;if(i.color.a<(threshold[cell.y*4+cell.x]+.5)/16)discard;
 float4 c=i.next.z>9.5?swordTex.Sample(smp,i.uv):colorTex.Sample(smp,i.uv);
 // A tiny scene patch supplies bounded local exposure/tint; no full-screen copy.
 int2 anchor=int2(lightAnchor*viewportSize);float3 ambient=0;
 ambient+=sceneColor.Load(int3(anchor+int2(-8,-8),0)).rgb;
 ambient+=sceneColor.Load(int3(anchor+int2(8,-8),0)).rgb;
 ambient+=sceneColor.Load(int3(anchor+int2(-8,8),0)).rgb;
 ambient+=sceneColor.Load(int3(anchor+int2(8,8),0)).rgb;ambient*=.25;
 float lum=dot(ambient,float3(.2126,.7152,.0722));
 // Scene-matched display exposure: the old 50% minimum left Franklin bright
 // even in shade/night. This remains an inexpensive local-light approximation.
 float exposure=clamp(.06+.90*sqrt(saturate(lum)),.08,.96);
 float3 tint=lerp(float3(1,1,1),clamp(ambient/max(lum,.015),.65,1.35),.38);
 float shade=1;
 if(i.next.w< -19){
  float3 n=i.color.rgb*rsqrt(max(dot(i.color.rgb,i.color.rgb),.00001));
  float hemisphere=saturate(i.next.x*.5+.5);
  // A broad, rough surface response supplies form without a plastic highlight.
  float key=saturate(dot(n,float3(-.272,.680,-.680)));
  shade=.34+.38*hemisphere+.28*key;
 }else shade=dot(i.color.rgb,float3(.333333,.333333,.333333));
 return UnderNativeUI(float4(c.rgb*sqrt(max(shade,0))*exposure*tint,1),int2(i.pos.xy));
}
// Original Buffalo2 material lookup; generated from local conversion manifest.
static const float4 carRect[14]={float4(0.00122070312,0.00122070312,0.247558594,0.247558594),float4(0.251220703,0.00122070312,0.247558594,0.247558594),float4(0.501220703,0.00122070312,0.247558594,0.247558594),float4(0.751220703,0.00122070312,0.247558594,0.247558594),float4(0.00122070312,0.251220703,0.247558594,0.247558594),float4(0.251220703,0.251220703,0.247558594,0.247558594),float4(0.501220703,0.251220703,0.247558594,0.247558594),float4(0.751220703,0.251220703,0.247558594,0.247558594),float4(0.00122070312,0.501220703,0.247558594,0.247558594),float4(0.251220703,0.501220703,0.247558594,0.247558594),float4(0.501220703,0.501220703,0.247558594,0.247558594),float4(0.751220703,0.501220703,0.247558594,0.247558594),float4(0.00122070312,0.751220703,0.247558594,0.247558594),float4(0.251220703,0.751220703,0.247558594,0.247558594)};
static const float4 carTint[14]={float4(0.9,0.91,0.9,1),float4(0.9,0.91,0.9,1),float4(1,1,1,1),float4(1,1,1,1),float4(1,1,1,1),float4(1,1,1,1),float4(1,1,1,1),float4(1,1,1,1),float4(1,1,1,1),float4(1,1,1,1),float4(1,1,1,1),float4(1,1,1,1),float4(1,1,1,1),float4(1,1,1,1)};
static const uint carKind[14]={1,1,0,3,0,0,0,2,2,0,2,4,0,3};
float4 CAR(V i):SV_Target{
 float scene=sceneDepth.Load(int3(i.pos.xy,0));if(useDepth>.5&&i.pos.z+.000002<scene)discard;
 uint id=(uint)clamp(round(i.next.z-100),0,13);float4 tex=carTex.Sample(smp,carRect[id].xy+frac(i.uv)*carRect[id].zw);
 float3 n=normalize(i.color.rgb);float light=.32+.38*saturate(n.y*.5+.5)+.30*saturate(dot(n,normalize(float3(-.27,.68,-.68))));
 float3 ambient=sceneColor.Load(int3(int2(lightAnchor*viewportSize),0)).rgb;float lum=dot(ambient,float3(.2126,.7152,.0722));float exposure=clamp(.06+.90*sqrt(saturate(lum)),.08,.96);
 float3 c=tex.rgb*carTint[id].rgb;if(carKind[id]==1)c=carTint[id].rgb;
 float alpha=carKind[id]==3?.28:(carKind[id]==2?tex.a:1);
 return UnderNativeUI(float4(c*sqrt(light)*exposure*alpha,alpha),int2(i.pos.xy));
}

float4 FX(V i):SV_Target{float scene=sceneDepth.Load(int3(i.pos.xy,0));if(useDepth>.5 && i.pos.z+0.000002<scene)discard;float4 c=colorTex.Sample(smp,i.uv);return UnderNativeUI(float4(c.rgb*c.a,0),int2(i.pos.xy));}
float4 BLAST(V i):SV_Target{
 float scene=sceneDepth.Load(int3(i.pos.xy,0));if(useDepth>.5 && i.pos.z+0.000002<scene)discard;
 uint material=(uint)floor(max(i.next.z,0));
 float4 a=blastTex.Sample(smp,i.uv),b=blastTex.Sample(smp,i.next.xy);
 if(a.a+b.a<.005)discard;
 // Original ptfx_sprite PS squares both texture frames before interpolation.
 a.rgb*=a.rgb;b.rgb*=b.rgb;
 if((material&4)!=0){a.a=dot(a.rgb,float3(1,1,1));b.a=dot(b.rgb,float3(1,1,1));
  if((material&16)!=0){float upper=saturate((1.001-a.a)*10000);a.a=saturate(a.a*10000)*upper;b.a=saturate(b.a*10000)*upper;}}
 float4 c=lerp(a,b,frac(i.next.z));c*=float4(i.color.rgb*i.color.rgb,i.color.a);
 if((material&2)!=0 && useDepth>.5){
  float worldZ=nearZ*farZ/max(nearZ+scene*(farZ-nearZ),.000001),spriteZ=1/max(i.pos.w,.000001);
  float gap=max(0,worldZ-spriteZ);c.a*=saturate(gap*gap*i.next.w);
 }
 // Adapt native lit sprites to the ER scene's local ambient level. Native GTA
 // directional lights, fog and HDR tone mapping are not available at Present.
 if((material&8)!=0){int2 anchor=int2(lightAnchor*viewportSize);float3 ambient=sceneColor.Load(int3(anchor,0)).rgb;
  float lum=dot(ambient*ambient,float3(.2126,.7152,.0722));c.rgb*=.45+.55*sqrt(saturate(lum));}
 // The target already contains the game's display image: encode our linear
 // particle contribution for this LDR compositor before premultiplied blending.
 c.rgb=sqrt(max(c.rgb,0));c.a=saturate(c.a);
 // Bit 0 selects the additive PSO, whose source alpha must not replay UI.
 return UnderNativeUI(float4(c.rgb*c.a,(material&1)!=0?0:c.a),int2(i.pos.xy));
}
float4 SHADOW(V i):SV_Target{float scene=sceneDepth.Load(int3(i.pos.xy,0));if(useDepth>.5 && i.pos.z+.000002<scene)discard;float d=length((i.uv-.5)*2);float strength=i.next.w< -29?1:.30;float a=(1-smoothstep(.15,1,d))*strength*i.color.a;return UnderNativeUI(float4(0,0,0,a),int2(i.pos.xy));}
float4 CustomUI(V i){
 if(i.next.z< -9.5){float4 c=mapTex.Sample(smp,i.uv)*i.color;return float4(c.rgb*c.a,c.a);}
 if(i.next.z< -8.5){float4 c=hudTex.Sample(smp,i.uv)*i.color;return float4(c.rgb*c.a,c.a);}
 if(i.next.z< -7.5)return float4(sceneColor.Load(int3(i.pos.xy,0)).rgb,1);
 if(i.next.z< -6.5){float2 p=(i.pos.xy-viewportSize*.5)/viewportSize.y;float mask=smoothstep(.424,.43,length(p));return float4(0,0,0,mask*i.color.a);}
 if(i.next.z< -5.5)return float4(i.color.rgb*i.color.a,i.color.a);
 if(i.next.z< -4.5){float a=wastedTex.Sample(smp,i.uv).a*i.color.a;return float4(i.color.rgb*a,a);}
 if(i.uv.x<0)return float4((-i.uv.x-.01).xxx,1);return float4(sqrt(saturate(colorTex.Sample(smp,i.uv).rgb))*.85+.10,1);
}
float4 UI(V i):SV_Target{float4 c=CustomUI(i);return i.next.w>.5?UnderNativeUI(c,int2(i.pos.xy)):c;}
)";
#include "../polish-v4/gpu_timing.h"
static GpuTiming gpuTiming;
// One dialogue-frame diagnostic: 36 pixels (18 KiB), consumed only after the
// normal submit fence completes. No wait, recurring readback, or full texture
// download. This distinguishes native RGBA conventions during live validation.
static ID3D12Resource* uiSampleReadback=nullptr;
static uint64_t uiSampleFence=0;
static bool uiSampleIssued=false;
static void consume_ui_sample(){
 if(!uiSampleReadback||!uiSampleFence||!g_fence||g_fence->GetCompletedValue()<uiSampleFence)return;
 void*data=nullptr;D3D12_RANGE range={0,36*512};
 if(SUCCEEDED(uiSampleReadback->Map(0,&range,&data))){
  for(unsigned i=0;i<12;i++){uint32_t pixels[3]={};for(unsigned layer=0;layer<3;layer++)memcpy(&pixels[layer],(char*)data+(layer*12+i)*512,4);
   log("UI_RGBA_SAMPLE format=%u x=%.1f y=%.1f before=%08x after=%08x native=%08x",unsigned(g_bbFormat),.3f+.2f*(i%3),i/3==0?.1f:(i/3==1?.5f:(i/3==2?.7f:.9f)),pixels[0],pixels[1],pixels[2]);}
  D3D12_RANGE noWrite={0,0};uiSampleReadback->Unmap(0,&noWrite);
 }
 safe_release(uiSampleReadback);uiSampleFence=0;
}
static void record_ui_sample(ID3D12Resource*before,ID3D12Resource*after,ID3D12Resource*frame){
 if(uiSampleIssued)return;uiSampleIssued=true;
 auto hp=heap_props(D3D12_HEAP_TYPE_READBACK);D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=36*512;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 if(FAILED(g_dev->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,__uuidof(ID3D12Resource),(void**)&uiSampleReadback)))return;
 ID3D12Resource*sources[3]={before,after,frame};
 for(unsigned layer=0;layer<3;layer++){
  auto state=layer<2?D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_RENDER_TARGET;
  transition(sources[layer],state,D3D12_RESOURCE_STATE_COPY_SOURCE);
  for(unsigned i=0;i<12;i++){
   float u=.3f+.2f*(i%3),v=i/3==0?.1f:(i/3==1?.5f:(i/3==2?.7f:.9f));UINT x=UINT(u*g_bbW),y=UINT(v*g_bbH);
   D3D12_TEXTURE_COPY_LOCATION from={},to={};from.pResource=sources[layer];from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;to.pResource=uiSampleReadback;to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint.Offset=(layer*12+i)*512;to.PlacedFootprint.Footprint={g_bbFormat,1,1,1,256};D3D12_BOX box={x,y,0,x+1,y+1,1};g_list->CopyTextureRegion(&to,0,0,0,&from,&box);
  }
  transition(sources[layer],D3D12_RESOURCE_STATE_COPY_SOURCE,state);
 }
 uiSampleFence=g_fenceNext+1;
}
static bool initialize(){
 D3D12_DESCRIPTOR_HEAP_DESC hd={};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=11;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
 if(FAILED(g_dev->CreateDescriptorHeap(&hd,__uuidof(ID3D12DescriptorHeap),(void**)&g_srvHeap)))return false;
 hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=kMaxBuffers;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;if(FAILED(g_dev->CreateDescriptorHeap(&hd,__uuidof(ID3D12DescriptorHeap),(void**)&g_rtvHeap)))return false;
 hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;hd.NumDescriptors=1;if(FAILED(g_dev->CreateDescriptorHeap(&hd,__uuidof(ID3D12DescriptorHeap),(void**)&g_dsvHeap)))return false;
 g_srvInc=g_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);g_rtvInc=g_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
 for(auto& r:g_ring){if(FAILED(g_dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,__uuidof(ID3D12CommandAllocator),(void**)&r.alloc))||!buffer(vertexCapacity*sizeof(Vertex),&r.upload))return false;D3D12_RANGE noRead={0,0};if(FAILED(r.upload->Map(0,&noRead,(void**)&r.uploadPtr)))return false;}
 if(FAILED(g_dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,g_ring[0].alloc,nullptr,__uuidof(ID3D12GraphicsCommandList),(void**)&g_list)))return false;g_list->Close();
 native_ui_capture_init(g_dev,g_list);
 make_srv(nullptr,g_bbFormat,9);make_srv(nullptr,g_bbFormat,10);
 if(FAILED(g_dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,__uuidof(ID3D12Fence),(void**)&g_fence)))return false;g_fenceEvent=CreateEventA(nullptr,FALSE,FALSE,nullptr);/* V4 returned only zero GPU timestamps on this CrossOver backend; do not issue unusable per-frame queries. */
 D3D12_DESCRIPTOR_RANGE range={};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;range.NumDescriptors=11;D3D12_ROOT_PARAMETER p[2]={};p[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;p[0].DescriptorTable.NumDescriptorRanges=1;p[0].DescriptorTable.pDescriptorRanges=&range;p[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;p[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;p[1].Constants.Num32BitValues=8;p[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 D3D12_STATIC_SAMPLER_DESC sampler={};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 D3D12_ROOT_SIGNATURE_DESC rsd={};rsd.NumParameters=2;rsd.pParameters=p;rsd.NumStaticSamplers=1;rsd.pStaticSamplers=&sampler;rsd.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
 ID3DBlob *rs=nullptr,*err=nullptr,*vs=nullptr,*ps=nullptr;auto serialize=(PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)GetProcAddress(GetModuleHandleA("d3d12.dll"),"D3D12SerializeRootSignature");HMODULE dc=LoadLibraryA("d3dcompiler_47.dll");auto compile=(D3DCompile_t)GetProcAddress(dc,"D3DCompile");if(!serialize||!compile)return false;
 if(FAILED(serialize(&rsd,D3D_ROOT_SIGNATURE_VERSION_1,&rs,&err))||FAILED(g_dev->CreateRootSignature(0,rs->GetBufferPointer(),rs->GetBufferSize(),__uuidof(ID3D12RootSignature),(void**)&g_rootSig)))return false;
 if(FAILED(compile(shader,strlen(shader),"proof",nullptr,nullptr,"VS","vs_5_0",0,0,&vs,&err))||FAILED(compile(shader,strlen(shader),"proof",nullptr,nullptr,"PS","ps_5_0",0,0,&ps,&err))){log("shader failed %s",err?(char*)err->GetBufferPointer():"unknown");return false;}
 D3D12_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,40,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
 D3D12_GRAPHICS_PIPELINE_STATE_DESC pd={};pd.pRootSignature=g_rootSig;pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pd.PS={ps->GetBufferPointer(),ps->GetBufferSize()};pd.InputLayout={input,4};pd.SampleMask=UINT_MAX;pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pd.RasterizerState.DepthClipEnable=TRUE;pd.DepthStencilState.DepthEnable=TRUE;pd.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;pd.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_GREATER_EQUAL;pd.DSVFormat=DXGI_FORMAT_D32_FLOAT;auto& blend=pd.BlendState.RenderTarget[0];blend.RenderTargetWriteMask=15;blend.SrcBlend=blend.SrcBlendAlpha=D3D12_BLEND_ONE;blend.DestBlend=blend.DestBlendAlpha=D3D12_BLEND_ZERO;blend.BlendOp=blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;blend.LogicOp=D3D12_LOGIC_OP_NOOP;pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;pd.NumRenderTargets=1;pd.RTVFormats[0]=g_bbFormat;pd.SampleDesc.Count=1;
 HRESULT hr=g_dev->CreateGraphicsPipelineState(&pd,__uuidof(ID3D12PipelineState),(void**)&g_pso);if(FAILED(hr)){log("PSO failed %lx",hr);return false;}
 // Car uses original tiled UVs and a separate atlas; on-foot PS stays unchanged.
 ID3DBlob* cps=nullptr;if(FAILED(compile(shader,strlen(shader),"buffalo",nullptr,nullptr,"CAR","ps_5_0",0,0,&cps,&err)))return false;
 pd.PS={cps->GetBufferPointer(),cps->GetBufferSize()};blend.BlendEnable=TRUE;blend.SrcBlend=D3D12_BLEND_ONE;blend.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;
 hr=g_dev->CreateGraphicsPipelineState(&pd,__uuidof(ID3D12PipelineState),(void**)&carPso);safe_release(cps);if(FAILED(hr))return false;
 ID3DBlob* fx=nullptr;if(FAILED(compile(shader,strlen(shader),"rifle_fx",nullptr,nullptr,"FX","ps_5_0",0,0,&fx,&err)))return false;
 pd.PS={fx->GetBufferPointer(),fx->GetBufferSize()};pd.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;blend.BlendEnable=TRUE;blend.SrcBlend=D3D12_BLEND_ONE;blend.DestBlend=D3D12_BLEND_ONE;blend.SrcBlendAlpha=D3D12_BLEND_ZERO;blend.DestBlendAlpha=D3D12_BLEND_ONE;
 hr=g_dev->CreateGraphicsPipelineState(&pd,__uuidof(ID3D12PipelineState),(void**)&g_fxPso);if(FAILED(hr))return false;
 ID3DBlob* blast=nullptr;if(FAILED(compile(shader,strlen(shader),"gta_blast",nullptr,nullptr,"BLAST","ps_5_0",0,0,&blast,&err)))return false;
 pd.PS={blast->GetBufferPointer(),blast->GetBufferSize()};
 hr=g_dev->CreateGraphicsPipelineState(&pd,__uuidof(ID3D12PipelineState),(void**)&g_blastAddPso);if(FAILED(hr))return false;
 blend.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;
 hr=g_dev->CreateGraphicsPipelineState(&pd,__uuidof(ID3D12PipelineState),(void**)&g_blastAlphaPso);safe_release(blast);if(FAILED(hr))return false;
 ID3DBlob* shadow=nullptr;if(FAILED(compile(shader,strlen(shader),"contact_shadow",nullptr,nullptr,"SHADOW","ps_5_0",0,0,&shadow,&err)))return false;pd.PS={shadow->GetBufferPointer(),shadow->GetBufferSize()};hr=g_dev->CreateGraphicsPipelineState(&pd,__uuidof(ID3D12PipelineState),(void**)&shadowPso);safe_release(shadow);if(FAILED(hr))return false;
 ID3DBlob* ui=nullptr;if(FAILED(compile(shader,strlen(shader),"rifle_ui",nullptr,nullptr,"UI","ps_5_0",0,0,&ui,&err)))return false;
 pd.PS={ui->GetBufferPointer(),ui->GetBufferSize()};pd.DepthStencilState.DepthEnable=FALSE;blend.BlendEnable=TRUE;blend.SrcBlend=D3D12_BLEND_ONE;blend.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;
 hr=g_dev->CreateGraphicsPipelineState(&pd,__uuidof(ID3D12PipelineState),(void**)&g_uiPso);safe_release(ui);safe_release(fx);safe_release(vs);safe_release(ps);safe_release(rs);safe_release(err);if(FAILED(hr))return false;
 make_srv(nullptr,DXGI_FORMAT_R8G8B8A8_UNORM,8);
 if(!carAsset.meshes.empty()){
  auto ch=heap_props(D3D12_HEAP_TYPE_DEFAULT);auto cd=tex_desc(2048,2048,DXGI_FORMAT_R8G8B8A8_UNORM);
  if(SUCCEEDED(g_dev->CreateCommittedResource(&ch,D3D12_HEAP_FLAG_NONE,&cd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,__uuidof(ID3D12Resource),(void**)&carAtlas))&&buffer(2048ull*2048*4,&carUpload)){
   void*m=nullptr;D3D12_RANGE nr={0,0};if(SUCCEEDED(carUpload->Map(0,&nr,&m))){FILE*f=igobygod::open(igobygod::data_path("buffalo2-atlas.rgba").c_str(),"rb");carReady=f&&fread(m,1,2048ull*2048*4,f)==2048ull*2048*4;if(f)fclose(f);carUpload->Unmap(0,nullptr);if(carReady)make_srv(carAtlas,DXGI_FORMAT_R8G8B8A8_UNORM,8);}
  }
  if(!carReady){safe_release(carAtlas);safe_release(carUpload);log("Buffalo atlas unavailable; car disabled");}
 }
 auto hp=heap_props(D3D12_HEAP_TYPE_DEFAULT);auto td=tex_desc(4096,4096,DXGI_FORMAT_R8G8B8A8_UNORM);if(FAILED(g_dev->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,__uuidof(ID3D12Resource),(void**)&atlas))||!buffer(4096ull*4096*4,&atlasUpload))return false;
 void* mapped=nullptr;D3D12_RANGE noRead={0,0};atlasUpload->Map(0,&noRead,&mapped);FILE* f=igobygod::open(igobygod::data_path("atlas.rgba").c_str(),"rb");if(!f)return false;bool ok=fread(mapped,1,4096ull*4096*4,f)==4096ull*4096*4;fclose(f);atlasUpload->Unmap(0,nullptr);make_srv(atlas,DXGI_FORMAT_R8G8B8A8_UNORM,0);make_srv(nullptr,DXGI_FORMAT_R32_FLOAT,1);if(!ok)return false;
 if(FAILED(g_dev->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,__uuidof(ID3D12Resource),(void**)&blastAtlas))||!buffer(4096ull*4096*4,&blastUpload))return false;
 mapped=nullptr;if(FAILED(blastUpload->Map(0,&noRead,&mapped)))return false;
 f=igobygod::open(igobygod::data_path("fx-atlas.rgba").c_str(),"rb");if(!f){blastUpload->Unmap(0,nullptr);return false;}
 ok=fread(mapped,1,4096ull*4096*4,f)==4096ull*4096*4;fclose(f);blastUpload->Unmap(0,nullptr);make_srv(blastAtlas,DXGI_FORMAT_R8G8B8A8_UNORM,2);if(!ok)return false;
 auto wt=tex_desc(1024,256,DXGI_FORMAT_R8G8B8A8_UNORM);
 if(FAILED(g_dev->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&wt,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,__uuidof(ID3D12Resource),(void**)&wastedAtlas))||!buffer(1024ull*256*4,&wastedUpload))return false;
 mapped=nullptr;if(FAILED(wastedUpload->Map(0,&noRead,&mapped)))return false;
 memset(mapped,0,1024*256*4);f=igobygod::open(igobygod::data_path("wasted-original.rgba").c_str(),"rb");
 ok=f!=nullptr;if(f){for(unsigned y=0;y<184;y++)if(fread((uint8_t*)mapped+y*4096,1,690*4,f)!=690*4){ok=false;break;}fclose(f);}
 wastedUpload->Unmap(0,nullptr);make_srv(wastedAtlas,DXGI_FORMAT_R8G8B8A8_UNORM,4);if(!ok)return false;
 auto ht=tex_desc(1024,1024,DXGI_FORMAT_R8G8B8A8_UNORM);if(FAILED(g_dev->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&ht,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,__uuidof(ID3D12Resource),(void**)&hudAtlas))||!buffer(1024ull*1024*4,&hudUpload))return false;
 mapped=nullptr;if(FAILED(hudUpload->Map(0,&noRead,&mapped)))return false;f=igobygod::open(igobygod::data_path("hud-atlas.rgba").c_str(),"rb");if(!f){hudUpload->Unmap(0,nullptr);return false;}ok=fread(mapped,1,1024ull*1024*4,f)==1024ull*1024*4;fclose(f);hudUpload->Unmap(0,nullptr);make_srv(hudAtlas,DXGI_FORMAT_R8G8B8A8_UNORM,5);if(!ok)return false;
 auto mt=tex_desc(4096,4096,DXGI_FORMAT_R8G8B8A8_UNORM);if(FAILED(g_dev->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&mt,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,__uuidof(ID3D12Resource),(void**)&mapAtlas))||!buffer(4096ull*4096*4,&mapUpload))return false;
 mapped=nullptr;if(FAILED(mapUpload->Map(0,&noRead,&mapped)))return false;f=igobygod::open(igobygod::data_path("terrain-map.rgba").c_str(),"rb");if(!f){mapUpload->Unmap(0,nullptr);return false;}ok=fread(mapped,1,4096ull*4096*4,f)==4096ull*4096*4;fclose(f);mapUpload->Unmap(0,nullptr);make_srv(mapAtlas,DXGI_FORMAT_R8G8B8A8_UNORM,6);if(!ok)return false;
 auto swordDesc=tex_desc(1024,1024,DXGI_FORMAT_R8G8B8A8_UNORM);
 if(FAILED(g_dev->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&swordDesc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,__uuidof(ID3D12Resource),(void**)&swordAtlas))||!buffer(1024ull*1024*4,&swordUpload))return false;
 mapped=nullptr;if(FAILED(swordUpload->Map(0,&noRead,&mapped)))return false;
 f=igobygod::open(igobygod::data_path("sword.rgba").c_str(),"rb");
 if(!f){swordUpload->Unmap(0,nullptr);return false;}
 ok=fread(mapped,1,1024ull*1024*4,f)==1024ull*1024*4;fclose(f);swordUpload->Unmap(0,nullptr);make_srv(swordAtlas,DXGI_FORMAT_R8G8B8A8_UNORM,7);return ok;
}
static void release_backbuffers(){for(auto& b:g_bb)safe_release(b);g_bbCount=0;safe_release(ownDepth);safe_release(sceneCopy);release_depth();}
static bool acquire_backbuffers(){DXGI_SWAP_CHAIN_DESC1 d={};if(FAILED(g_sc->GetDesc1(&d))||d.BufferCount>kMaxBuffers||d.Format!=g_bbFormat)return false;for(UINT i=0;i<d.BufferCount;i++){if(FAILED(g_sc->GetBuffer(i,__uuidof(ID3D12Resource),(void**)&g_bb[i])))return false;g_dev->CreateRenderTargetView(g_bb[i],nullptr,rtv_cpu(i));}g_bbCount=d.BufferCount;g_bbW=d.Width;g_bbH=d.Height;auto sceneProps=heap_props(D3D12_HEAP_TYPE_DEFAULT);auto sceneDesc=tex_desc(g_bbW,g_bbH,g_bbFormat);if(FAILED(g_dev->CreateCommittedResource(&sceneProps,D3D12_HEAP_FLAG_NONE,&sceneDesc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,__uuidof(ID3D12Resource),(void**)&sceneCopy)))return false;make_srv(sceneCopy,g_bbFormat,3);auto hp=heap_props(D3D12_HEAP_TYPE_DEFAULT);auto td=tex_desc(g_bbW,g_bbH,DXGI_FORMAT_D32_FLOAT);td.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;D3D12_CLEAR_VALUE clear={};clear.Format=DXGI_FORMAT_D32_FLOAT;clear.DepthStencil.Depth=0;if(FAILED(g_dev->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear,__uuidof(ID3D12Resource),(void**)&ownDepth)))return false;g_dev->CreateDepthStencilView(ownDepth,nullptr,g_dsvHeap->GetCPUDescriptorHandleForHeapStart());InterlockedExchange(&g_depthScanState,1);return true;}
static uint8_t* typed(void* p,uintptr_t vt,size_t n){return mem_readable(p,n)&&*(uintptr_t*)p==main_module_base()+vt?(uint8_t*)p:nullptr;}
static void restore_player(){if(hiddenPlayer&&wasVisible&&typed(hiddenPlayer,0x2A7FBB0,0x6D4))_InterlockedOr8((volatile char*)(hiddenPlayer+0x1C5),8);hiddenPlayer=nullptr;wasVisible=false;}
static void hide_player(){if(!activePlayer)return;if(hiddenPlayer!=activePlayer){restore_player();hiddenPlayer=activePlayer;wasVisible=(activePlayer[0x1C5]&8)!=0;}_InterlockedAnd8((volatile char*)(activePlayer+0x1C5),(char)~8);}
static bool snapshot(float* cam,float* pos,float& fov,float& aspect,float& nearZ,float& farZ){MemReadScope scope;auto base=main_module_base();void* cs=nullptr;void* w=nullptr;if(!mem_read((void*)(base+0x3D669C8),&cs,8)||!mem_read((void*)(base+0x3D69FF8),&w,8)||!mem_readable(cs,16))return false;auto c=typed(*(void**)((uint8_t*)cs+8),0x2AA0A40,0x70);auto world=typed(w,0x2A4E980,0x1E510);if(!c||!world)return false;auto player=typed(*(void**)(world+0x1E508),0x2A7FBB0,0x6D4);if(!player||!*(uint32_t*)(player+0x6D0)||*(uint32_t*)(player+0x6D0)==0xFFFFFFFFu)return false;auto mods=*(uint8_t**)(player+0x190);if(!mem_readable(mods,0x70))return false;auto phys=typed(*(void**)(mods+0x68),0x2A3C890,0x2E8);if(!phys)return false;auto health=typed(*(void**)mods,0x2A380B8,0x140);if(!health)return false;nativeDead=*(int*)(health+0x138)<=0||(player[0x1C5]&0x80);activePlayer=player;nativeAnim=nativeRawAnim=-1;
 { auto ta=*(uint8_t**)(mods+0x18); if(mem_readable(ta,0xC8)&&*(void**)(ta+8)==player){unsigned idx=*(unsigned*)(ta+0xC4);if(idx<10){int id=*(int*)(ta+0x20+idx*16);if(id>=0&&id<100000000)nativeRawAnim=id;float t=*(float*)(ta+0x24+idx*16),len=*(float*)(ta+0x2C+idx*16);if(id>=0&&isfinite(t)&&t>=0&&isfinite(len)&&len>0&&len<120){nativeAnim=native_animation_policy::base(id);nativeTime=t;nativeLength=len;}static int lastId=-1;static uint64_t lastAnimLog=0;if(id>=0&&isfinite(t)&&isfinite(len)&&len>=0&&len<120&&(id!=lastId||now_ms()-lastAnimLog>1000)){log("NATIVE_ANIM id=%d time=%.3f length=%.3f",id,t,len);lastId=id;lastAnimLog=now_ms();}}}}
 float q[4];memcpy(q,phys+0x60,16);float qn=0;for(float x:q)qn+=x*x;if(!isfinite(qn)||qn<.25f||qn>4.f)return false;for(float& x:q)x/=sqrtf(qn);facing[0]=2*(q[0]*q[2]+q[3]*q[1]);facing[1]=1-2*(q[0]*q[0]+q[1]*q[1]);float fl=hypotf(facing[0],facing[1]);if(fl<.001f)return false;facing[0]/=fl;facing[1]/=fl;memcpy(cam,c+0x10,64);memcpy(pos,phys+0x70,12);fov=*(float*)(c+0x50);aspect=*(float*)(c+0x54);nearZ=*(float*)(c+0x58);farZ=*(float*)(c+0x5C);return isfinite(fov)&&fov>.1&&fov<3&&aspect>.5&&aspect<4&&nearZ>.001&&farZ>nearZ;}
static SceneLifecycle sceneLife;
static uint64_t atlasUploadFence=0;
struct FrameCost {
 LARGE_INTEGER begin{}; bool drew=false; static uint64_t window,count,draws,fenceDrops; static double total,peak; double previous=0,section[4]={};
 void mark(unsigned i){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);double elapsed=1000.*double(t.QuadPart-begin.QuadPart)/double(f.QuadPart);section[i]=elapsed-previous;previous=elapsed;}
 FrameCost(){QueryPerformanceCounter(&begin);}
 ~FrameCost(){LARGE_INTEGER end,freq;QueryPerformanceCounter(&end);QueryPerformanceFrequency(&freq);double ms=1000.*double(end.QuadPart-begin.QuadPart)/double(freq.QuadPart);static double stages[4]={};for(unsigned i=0;i<4;i++)stages[i]+=section[i];total+=ms;peak=fmax(peak,ms);count++;draws+=drew?1:0;uint64_t now=now_ms();if(!window)window=now;if(now-window>=5000){log("RENDER_COST present_fps=%.1f drawn_fps=%.1f cpu_avg_ms=%.2f cpu_peak_ms=%.2f fence_skips=%llu",count*1000./(now-window),draws*1000./(now-window),total/count,peak,fenceDrops);log("RENDER_STAGES state_ms=%.2f pose_ms=%.2f geometry_fx_ms=%.2f ui_ms=%.2f",stages[0]/count,stages[1]/count,stages[2]/count,stages[3]/count);for(double&v:stages)v=0;window=now;count=draws=fenceDrops=0;total=peak=0;}}
};
uint64_t FrameCost::window=0,FrameCost::count=0,FrameCost::draws=0,FrameCost::fenceDrops=0;double FrameCost::total=0,FrameCost::peak=0;
static void composite(IDXGISwapChain* sc,UINT flags){
 if(flags&DXGI_PRESENT_TEST)return;if(g_failed){restore_player();return;}
 if(!g_sc){if(FAILED(sc->GetDevice(__uuidof(ID3D12Device),(void**)&g_dev))){g_failed=true;return;}ID3D12CommandQueue*q=nullptr;if(g_nativeWindows){q=native_windows_queue(sc,g_dev);if(!q){safe_release(g_dev);return;}}else{if(!readable((uint8_t*)sc+0x28,8)){g_failed=true;return;}auto inner=*(uint8_t**)((uint8_t*)sc+0x28);q=inner?(ID3D12CommandQueue*)(inner-0x58):nullptr;ID3D12CommandQueue* mine=nullptr;D3D12_COMMAND_QUEUE_DESC qd={};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;bool same=SUCCEEDED(g_dev->CreateCommandQueue(&qd,__uuidof(ID3D12CommandQueue),(void**)&mine))&&q&&readable(q,8)&&*(void**)q==*(void**)mine;safe_release(mine);if(!same){log("Queue validation failed; disabled");g_failed=true;return;}}g_queue=q;q->AddRef();if(FAILED(sc->QueryInterface(__uuidof(IDXGISwapChain3),(void**)&g_sc))){g_failed=true;return;}DXGI_SWAP_CHAIN_DESC1 desc={};g_sc->GetDesc1(&desc);g_bbFormat=desc.Format;if(!initialize()){log("GPU initialization failed; disabled");g_failed=true;return;}log("GPU initialized");}
 if((IDXGISwapChain*)g_sc!=sc)return;
 consume_ui_sample();
 FrameCost frameCost;
 if(atlasUploadFence&&g_fence->GetCompletedValue()>=atlasUploadFence){safe_release(atlasUpload);safe_release(blastUpload);safe_release(wastedUpload);safe_release(hudUpload);safe_release(mapUpload);safe_release(swordUpload);safe_release(carUpload);atlasUploadFence=0;}
 if(!g_bbCount&&!acquire_backbuffers()){g_failed=true;return;}
 if(InterlockedCompareExchange(&g_depthScanState,0,3)==3){wait_idle();adopt_depth_views();if(g_depthCandCount){g_depthBound=0;make_srv(g_depthCands[0],g_depthSrvFmt[0],1);log("Depth candidate zero selected; needs wall test");}}
 static uint64_t lastRescan=0;if(g_depthBound<0&&now_ms()-lastRescan>15000&&InterlockedCompareExchange(&g_depthScanState,1,0)==0){lastRescan=now_ms();}
 const uint64_t sceneNow=now_ms();const auto nativePrompt=native_ui::snapshot();int nativeHudState=gameplay_hud_state();
 float cam[16],pos[3],fov,aspect,nz,fz;bool sceneValid=snapshot(cam,pos,fov,aspect,nz,fz);
 if(sceneValid)for(float v:pos)if(!isfinite(v))sceneValid=false;
 ID3D12Resource*uiA=nullptr,*uiB=nullptr;
 bool uiLayer=native_ui_capture_snapshot(sceneNow,g_bbW,g_bbH,g_bbFormat,&uiA,&uiB)!=0;
 static ID3D12Resource*boundUI=nullptr;if(uiLayer&&boundUI!=uiA){make_srv(uiA,g_bbFormat,9);make_srv(uiB,g_bbFormat,10);boundUI=uiA;log("UI_CAPTURE pixel layer active");}
 bool normalGameplay=nativeHudState==3&&!nativePrompt.menu_blocking&&!nativePrompt.popup_job_active&&!nativePrompt.item_acquisition;
 bool menu=ui_visibility::hide_world(nativeHudState,nativePrompt.menu_blocking,nativePrompt.item_acquisition,uiLayer);
 static uint32_t lastUI=~0u;static unsigned uiLogs=0;uint32_t uiBits=(sceneValid?1:0)|(nativePrompt.ui_valid?2:0)|(nativePrompt.loading?4:0)|(nativePrompt.menu_fade?8:0)|(menu?16:0)|(nativePrompt.prompt_selected?32:0)|(nativePrompt.subtitles?64:0);if(uiBits!=lastUI&&uiLogs<128){lastUI=uiBits;uiLogs++;log("UI_PHASE bits=%u hud=%d visible=%llx/%x popup=%d",uiBits,nativeHudState,(unsigned long long)nativePrompt.visible_mask0,nativePrompt.visible_mask1,nativePrompt.popup_job_active);}
 bool show=sceneLife.update(sceneNow,sceneValid?(uintptr_t)activePlayer:0,sceneValid&&nativePrompt.ui_valid&&nativePrompt.menu_fade_valid,nativePrompt.loading,nativePrompt.menu_fade,menu,nativeDead);
 frameCost.mark(0);
 cheat_console_tick(show&&normalGameplay&&!nativeDead);shoulder_camera_input_blocked(cheat_console_blocking());
 if(!show){shoulder_camera_heartbeat(0,sceneNow);rifle_view(0,sceneNow,nullptr,nullptr,0);if(sceneValid&&nativePrompt.ui_valid&&nativePrompt.menu_fade_valid)hide_player();else restore_player();return;}
 shoulder_camera_heartbeat((uintptr_t)activePlayer,sceneNow);
 rifle_view((uintptr_t)activePlayer,sceneNow,cam+12,cam+8,!nativeDead&&normalGameplay&&!cheat_console_blocking());
 float scopeAmount=shoulder_camera_scope();bool scoped=scopeAmount>.85f;
 if(sceneLife.deathStarted)native_sound_play(7);
 RingSlot& r=g_ring[g_ringPos%kRing];if(!wait_fence(r.fence,0)){FrameCost::fenceDrops++;return;}
 static uint8_t* lifeOwner=nullptr;static bool wasDead=false;static uint32_t lifeEpoch=0;static uint64_t diedAt=0;static unsigned lastDeathId=17002;static bool holdCorpse=false;
 bool lifeChanged=lifeOwner!=activePlayer||wasDead!=nativeDead||lifeEpoch!=sceneLife.epoch;
 if(lifeChanged){log("PLAYER_LIFE dead=%d native_animation=%d",nativeDead,nativeAnim);diedAt=nativeDead?sceneNow:0;lastDeathId=17002;holdCorpse=false;lifeOwner=activePlayer;wasDead=nativeDead;lifeEpoch=sceneLife.epoch;}
 const RollClip* activeAction=nullptr;
 const int nativeActionBase=native_animation_policy::base(nativeRawAnim);
 if(nativeDead){if(nativeActionBase>=18000&&nativeActionBase<19000)holdCorpse=true;for(const auto&clip:deathClips)if(clip.id<18000&&nativeActionBase==int(clip.id)){activeAction=&clip;lastDeathId=clip.id;holdCorpse=false;break;}
  if(!activeAction)for(const auto&clip:deathClips)if(clip.id==lastDeathId){activeAction=&clip;break;}
 }else for(const auto&clip:rollClips)if(nativeAnim==int(clip.id)&&nativeLength>0){activeAction=&clip;break;}

 static uint64_t lastTime=0;static float previous[3]={},speed=0;static float phase[4]={};uint64_t now=now_ms();float dt=lastTime?float(now-lastTime)/1000.f:0;float vx=pos[0]-previous[0],vz=pos[2]-previous[2];float distance=hypotf(vx,vz);float measured=(dt>.0001f&&dt<.25f&&distance<3.f)?distance/dt:0;if(!lastTime||lifeChanged||dt>.25f||distance>=3.f)speed=0;else if(!activeAction)speed+=(measured-speed)*(1-expf(-dt*18.f));lastTime=now;memcpy(previous,pos,12);
 // All three donor gaits share a left-foot phase; blend only adjacent speed tiers.
 float weights[4]={0,0,0,0};float moving=fminf(1.f,fmaxf(0.f,(speed-.04f)/.30f));moving=moving*moving*(3-2*moving);weights[0]=1-moving;
 if(speed<1.63f)weights[1]=moving;
 else if(speed<4.43f){float t=(speed-1.63f)/2.8f;weights[1]=(1-t)*moving;weights[2]=t*moving;}
 else {float t=fminf(1.f,(speed-4.43f)/1.14f);weights[2]=(1-t)*moving;weights[3]=t*moving;}
 static float gaitPhase=0;const float gaitSpeed[4]={1.f,1.71552f,4.65419f,6.82399f};float cycleDistance=0;
 for(unsigned c=1;c<4;c++)cycleDistance+=weights[c]*gaitSpeed[c]*durations[c];
 if(moving>.001f&&!activeAction)gaitPhase=fmodf(gaitPhase+fminf(dt,.1f)*speed/fmaxf(.1f,cycleDistance/moving),1.f);
 phase[0]=fmodf(phase[0]+fminf(dt,.1f)/durations[0],1.f);for(unsigned c=1;c<4;c++)phase[c]=gaitPhase;
 static std::vector<float> model;model.resize(nv*3);std::fill(model.begin(),model.end(),0.f);
 for(unsigned c=0;c<4;c++){if(weights[c]<.001f)continue;float t=phase[c]*nf;unsigned a=unsigned(t)%nf,b=(a+1)%nf;float u=t-a,weight=weights[c];const float* first=poses.data()+(size_t(c)*nf+a)*nv*3;const float* second=poses.data()+(size_t(c)*nf+b)*nv*3;
  for(unsigned v=0;v<nv*3;v++)model[v]+=(first[v]*(1-u)+second[v]*u)*weight;
 }
 // Sample the native action directly, without fading through unrelated locomotion at clip boundaries.
 if(activeAction){const auto& clip=*activeAction;float actionTime=nativeTime,actionLength=nativeLength;
  if(nativeDead&&(nativeAnim!=int(clip.id)||actionLength<=0)){actionTime=float(now-diedAt)/1000.f;actionLength=clip.duration;}
  float progress=nativeDead&&holdCorpse?1.f:fminf(1.f,fmaxf(0.f,actionTime/actionLength));float sample=progress*(clip.frames-1);unsigned a=unsigned(sample),b=a+1<clip.frames?a+1:a;float u=sample-a;
  for(unsigned v=0;v<nv*3;v++){float target=clip.vertices[size_t(a)*nv*3+v]*(1-u)+clip.vertices[size_t(b)*nv*3+v]*u;model[v]=v%3!=2?-target:target;}
 }

 CarSnapshot carState={};bool carVisible=carReady&&car_snapshot(&carState)&&carState.owner==(uintptr_t)activePlayer;bool driving=carVisible&&carState.driving&&!nativeDead;
 RifleState rifle={};bool stateFresh=rifle_state(&rifle)!=0;unsigned weapon=stateFresh&&rifle.weapon<8?rifle.weapon:0;
 bool armed=stateFresh&&weapon>0&&!activeAction&&!nativeDead&&!driving;
 bool rpg=weapon==6&&rifle.launcher_variant==1;
 const auto&heldMesh=rpg?rpgRig.mesh[6]:arsenalRig.mesh[weapon];
 auto findWeaponClip=[&](unsigned slot,unsigned action)->const arsenal::Clip*{if(slot==6&&rpg){if(auto c=rpgRig.find(6,action))return c;}return arsenalRig.find(slot,action);};
 bool liveRifle=armed&&rifle.active;
 static float rifleWeight=0;float rifleGoal=armed?1.f:0.f;rifleWeight+=(rifleGoal-rifleWeight)*(1-expf(-fmaxf(0.f,dt)*16.f));if(activeAction)rifleWeight=0;
 static donor_motion::SignedPhase lowerClock;static donor_motion::DirectionFilter moveFilter;static uint8_t* motionOwner=nullptr;static bool directionalResetPending=true;
 float aimDirection[3]={cam[8],cam[9],cam[10]};
 ShoulderAimSnapshot freeAim={};
 bool freeLockedAim=stateFresh&&weapon>0&&rifle.lock_on&&shoulder_camera_aim_snapshot((uintptr_t)activePlayer,now,&freeAim)&&freeAim.locked&&freeAim.free_aim;
 if(freeLockedAim)memcpy(aimDirection,freeAim.direction,sizeof(aimDirection));
 float aimYaw=atan2f(-aimDirection[0],-aimDirection[2]);bool nativeRpgAction=native_animation_policy::rpg_weapon_action(rifle.native_action,weapon);
 auto movementMode=locomotion_mode::select(armed,rifle.aiming,nativeRpgAction,rifle.lock_on);bool raised=movementMode.raised;
 float bodyYaw=atan2f(facing[0],facing[1]);
 bool resetMotion=motionOwner!=activePlayer||lifeChanged||dt<0||dt>.25f||distance>=3.f;
 static float visualYaw=0;static uint8_t* yawOwner=nullptr;static locomotion_mode::Heading movementHeading;
 // Q controls the camera target, not whether Franklin raises the weapon.
 // Relaxed Q walking uses real displacement; entering Q at rest holds the
 // previous displayed heading. Native physics and action orientation stay intact.
 float targetYaw=movementHeading.update(movementMode,rifle.lock_on,freeLockedAim,vx,vz,dt,bodyYaw,aimYaw,visualYaw,resetMotion);
 float locomotionYaw=raised?targetYaw:bodyYaw;
 if(resetMotion){lowerClock.reset();motionOwner=activePlayer;directionalResetPending=true;}
 if(activeAction)directionalResetPending=true;
 auto motion=moveFilter.update(vx,vz,locomotionYaw,raised,dt,resetMotion);
 float armedPhase=lowerClock.advance(activeAction?0.f:dt,speed,weights,false),pelvisYaw=motion.pelvisYaw;
 static float actionWeight=0;float actionGoal=(raised||rpg)?1.f:0.f;
 if(dt<0||dt>.25f||activeAction)actionWeight=0;else actionWeight+=(actionGoal-actionWeight)*(1-expf(-dt*20.f));
 static arsenal::Pose rigPose,part,act,previousRig,fromRig;static PoseTransition rigTransition;
 arsenal::M gunWorld={};float tip[3]={0,0,0};
 static uint64_t throwVisual=0;if(lifeChanged)throwVisual=0;if(rifle.throw_start)throwVisual=rifle.throw_start;const auto* throwClip=arsenalRig.find(7,4);float throwDuration=throwClip?throwClip->duration:2.4f;if(weapon!=7||activeAction||now<throwVisual||float(now-throwVisual)>throwDuration*1000.f)throwVisual=0;
 const auto* fireClip=findWeaponClip(weapon,2);float fireSeconds=rpg&&fireClip?fireClip->duration:.190f;
 unsigned gunAction=throwVisual?4:rifle.reload_start?3:rifle.shot_time&&now>=rifle.shot_time&&float(now-rifle.shot_time)<fireSeconds*1000.f?2:1;
 float gunTime=gunAction==4?float(now-throwVisual)/1000.f:gunAction==3?float(now-rifle.reload_start)/1000.f:gunAction==2?float(now-rifle.shot_time)/1000.f:float(now%1000000)/1000.f;
 if(armed){
  float total=0;const float offsets[4]={0,46.f/120.f,16.f/120.f,22.f/120.f};
  for(unsigned c=0;c<4;c++){if(weights[c]<.001f)continue;const auto*clip=findWeaponClip(weapon,c?c+4:0);if(!clip)clip=arsenalRig.find(4,c?c+4:0);if(!clip)continue;
   float ph=c?armedPhase+offsets[c]:float(now%1000000)/1000.f/clip->duration;
   arsenalRig.sample(clip,ph*clip->duration,true,part);
   if(total==0)rigPose=part;else arsenalRig.blend(rigPose,part,weights[c]/(total+weights[c]));total+=weights[c];
  }
  arsenal::Pose directional;bool useDirectional=raised&&moving>.001f&&directionalRig.sample(arsenalRig,weapon,vx,vz,locomotionYaw,speed,dt,directionalResetPending,directional);
  if(useDirectional){directionalResetPending=false;arsenalRig.blend(rigPose,directional,moving);}
  const auto* actionClip=findWeaponClip(weapon,gunAction);if(!actionClip)actionClip=findWeaponClip(weapon,1);
  if(actionClip){float t=gunTime;if(gunAction==2)t=gunTime/fireSeconds*actionClip->duration;arsenalRig.sample(actionClip,t,gunAction==1,act);
   // All interpolation occurs in local joint space, then the hierarchy is skinned once.
   if(weapon==7&&gunAction==4)grenade_pose::blend(arsenalRig,rigPose,act,moving);
   else{
   act.q[45]=donor_motion::multiply(donor_motion::multiply(donor_motion::inverse(rigPose.q[0]),act.q[0]),act.q[45]);
   for(unsigned j=0;j<arsenalRig.bones;j++)if(arsenalRig.upper[j]){rigPose.q[j]=arsenal::mix(rigPose.q[j],act.q[j],actionWeight);if(j!=45)rigPose.t[j]=arsenal::mix(rigPose.t[j],act.t[j],actionWeight);}
   }
  }
  if(raised&&moving>.001f&&!useDirectional)rigPose.q[1]=donor_motion::pelvisDirection(rigPose.q[0],rigPose.q[1],pelvisYaw*actionWeight);
  // Camera pitch rotates the spine around its real joint, never a vertex-space waist mask.
  static float pitch=0;float goal=raised&&gunAction!=4?asinf(fmaxf(-.98f,fminf(.98f,aimDirection[1]))):0.f;pitch+=(goal-pitch)*(1-expf(-fmaxf(0.f,dt)*16.f));
  if(weapon==7&&gunAction==4)pitch=0;
  arsenal::Q pq={sinf(pitch*.5f),0,0,cosf(pitch*.5f)};
  rigPose.q[45]=donor_motion::multiply(donor_motion::multiply(donor_motion::multiply(donor_motion::inverse(rigPose.q[0]),pq),rigPose.q[0]),rigPose.q[45]);
  int state=int(weapon*16+(raised?gunAction:0)+(rpg?128:0));auto blend=rigTransition.update((uintptr_t)activePlayer,state,gunTime,dt,dt<0||dt>.25f||distance>=3.f);
  if(blend.reset||previousRig.q.size()!=rigPose.q.size()){previousRig=rigPose;fromRig=rigPose;}
  else{if(blend.capture)fromRig=previousRig;if(blend.alpha<1){part=fromRig;arsenalRig.blend(part,rigPose,blend.alpha);rigPose=part;}previousRig=rigPose;}
  arsenalRig.matrices(rigPose);static std::vector<float> skinned;arsenalRig.skin(skinned);for(unsigned v=0;v<nv*3;v++)model[v]+=(skinned[v]-model[v])*rifleWeight;
  gunWorld=arsenal::mul(arsenalRig.world[arsenalRig.hand],heldMesh.attachment);
  // A grenade releases from the actual prop centre, not its hand-grip marker.
  auto release=heldMesh.muzzle;
  if(weapon==7){release={0,0,0};const auto&points=arsenalRig.mesh[7].p;if(!points.empty()){auto lo=points[0],hi=points[0];for(auto p:points){lo.x=fminf(lo.x,p.x);lo.y=fminf(lo.y,p.y);lo.z=fminf(lo.z,p.z);hi.x=fmaxf(hi.x,p.x);hi.y=fmaxf(hi.y,p.y);hi.z=fmaxf(hi.z,p.z);}release={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f,(lo.z+hi.z)*.5f};}}
  auto mt=arsenal::point(gunWorld,release);tip[0]=mt.x;tip[1]=mt.y;tip[2]=mt.z;
 }
 // Native melee owns attack timing and damage. Only its visible pose is retargeted.
 bool melee=stateFresh&&weapon==0&&rifle.melee_mode>0&&!nativeDead&&!driving;
 const melee_animation::Clip* meleeClip=nullptr;arsenal::M swordWorld={};bool showSword=false;
 if(melee&&!activeAction){
  unsigned bank=nativeRawAnim<0?~0u:unsigned(nativeRawAnim)/1000000;
  if(bank==(rifle.melee_mode==2?23u:42u))meleeClip=meleePacket.find(unsigned(nativeRawAnim));
  static arsenal::Pose meleePose,meleePart,previousMelee,fromMelee;
  static arsenal::M previousAttachment={},fromAttachment={};static PoseTransition meleeBlend;static uint32_t previousMode=0;
  arsenal::M anchor={},localAttachment={};bool ready=false;
  if(meleeClip)ready=meleePacket.sampleNative(meleeClip,nativeTime,nativeLength,meleePose,anchor);
  else{
   float total=0;
   for(unsigned c=0;c<4;c++){if(weights[c]<.001f)continue;
    if(!meleePacket.samplePhase(meleePacket.find(melee_animation::kGaitIdle+c),phase[c],true,meleePart,anchor))continue;
    if(total==0)meleePose=meleePart;else arsenalRig.blend(meleePose,meleePart,weights[c]/(total+weights[c]));total+=weights[c];
   }
   ready=total>.99f;
   if(ready&&rifle.melee_mode==2)ready=sword_carry::apply(meleePose,arsenalRig);
   if(ready){localAttachment=rifle.melee_mode==2?sword_carry::attachment():meleePacket.fallbackAttachment;arsenalRig.matrices(meleePose);anchor=arsenal::mul(arsenalRig.world[arsenalRig.hand],localAttachment);}
  }
  if(ready&&meleeClip){
   arsenalRig.matrices(meleePose);arsenal::M handInverse={};
   ready=arsenal::invert(arsenalRig.world[arsenalRig.hand],handInverse);
   if(ready)localAttachment=arsenal::mul(handInverse,anchor);
  }
  if(ready){
   int key=meleeClip?int(meleeClip->raw_id):int(melee_animation::kGaitIdle);
   auto blend=meleeBlend.update((uintptr_t)activePlayer,key,meleeClip?nativeTime:0,dt,resetMotion||previousMode!=rifle.melee_mode);meleeBlend.duration=.065f;previousMode=rifle.melee_mode;
   bool attachmentBlended=false;
   if(blend.reset||previousMelee.q.size()!=meleePose.q.size()){previousMelee=meleePose;previousAttachment=localAttachment;}
   else{if(blend.capture){fromMelee=previousMelee;fromAttachment=previousAttachment;}if(blend.alpha<1&&fromMelee.q.size()==meleePose.q.size()){meleePart=fromMelee;arsenalRig.blend(meleePart,meleePose,blend.alpha);meleePose=meleePart;localAttachment=rifle.melee_mode==2?sword_attack::blendAttachment(fromAttachment,localAttachment,blend.alpha):sword_carry::blendAttachment(fromAttachment,localAttachment,blend.alpha);attachmentBlended=true;}}
   previousMelee=meleePose;previousAttachment=localAttachment;
   // Blend attachment in the displayed hand's local frame, preventing the
   // hilt from sliding away while the arm transitions into a native attack.
   arsenalRig.matrices(meleePose);arsenalRig.skin(model);swordWorld=meleeClip&&!attachmentBlended?anchor:arsenal::mul(arsenalRig.world[arsenalRig.hand],localAttachment);showSword=rifle.melee_mode==2;
   static int lastMelee=-1;static unsigned meleeLogs=0;if(lastMelee!=key&&meleeLogs<192){log("MELEE_POSE mode=%u raw=%d clip=%d time=%.3f native_length=%.3f",rifle.melee_mode,nativeRawAnim,key,nativeTime,nativeLength);lastMelee=key;meleeLogs++;}
  }
 }
 static PoseTransition transitionState;static std::vector<float> shownPose,transitionFrom;
 auto blendState=transitionState.update(reinterpret_cast<uintptr_t>(activePlayer),activeAction?int(activeAction->id):0,activeAction?nativeTime:0,dt,dt<0||dt>.25f||distance>=3.f);
 if(activeAction||melee||blendState.reset||shownPose.size()!=model.size()){shownPose=model;transitionFrom.clear();}
 else {if(blendState.capture)transitionFrom=shownPose;if(blendState.alpha<1&&transitionFrom.size()==model.size())for(size_t v=0;v<model.size();v++)model[v]=transitionFrom[v]+(model[v]-transitionFrom[v])*blendState.alpha;shownPose=model;}

 for(float value:model)if(!isfinite(value)){log("Invalid sampled animation pose; disabled");g_failed=true;restore_player();return;}
 // Smooth shortest-arc turns visually; ER continues to own position and collision.
 if(meleeClip||yawOwner!=activePlayer||resetMotion){visualYaw=targetYaw;yawOwner=activePlayer;}
 else {float delta=atan2f(sinf(targetYaw-visualYaw),cosf(targetYaw-visualYaw));visualYaw+=delta*(1-expf(-dt*18.f));}
 // Bounded transitions only: distinguish camera lock from raised-gun mode
 // without adding a per-frame log or any game-state writes.
 static unsigned motionLogCount=0;static int previousMotionKey=-1;
 bool headingMoving=measured>.10f&&dt>.0001f&&dt<.25f&&distance<3.f;
 int motionKey=(rifle.lock_on?1:0)|(raised?2:0)|(headingMoving?4:0)|(int(movementHeading.source)<<3);
 if(stateFresh&&weapon>0&&motionLogCount<64&&motionKey!=previousMotionKey){previousMotionKey=motionKey;motionLogCount++;log("LOCOMOTION_MODE q=%u raised=%u moving=%u source=%s heading=%.3f target=%.3f native_heading=%.3f",rifle.lock_on,unsigned(raised),unsigned(headingMoving),locomotion_mode::name(movementHeading.source),visualYaw,targetYaw,bodyYaw);}
 float drawFacing[2]={sinf(visualYaw),cosf(visualYaw)};
 if(driving&&driverAnimation.pose(arsenalRig,carState,float(now%1000000)*.001f,model,pos)){drawFacing[0]=0;drawFacing[1]=1;}
 frameCost.mark(1);
 Vertex* dst=(Vertex*)r.uploadPtr;size_t written=0;float k=1.f/tanf(fov*.5f);unsigned frame=unsigned(phase[1]*nf);
 // Collision/locked camera may enter the replacement torso at close range.
 float capsuleY=fmaxf(pos[1]+.65f,fminf(pos[1]+1.90f,cam[13]));
 float torsoDistance=sqrtf((pos[0]-cam[12])*(pos[0]-cam[12])+(capsuleY-cam[13])*(capsuleY-cam[13])+(pos[2]-cam[14])*(pos[2]-cam[14]));
 float bodyAlpha=fmaxf(0.f,fminf(1.f,(torsoDistance-.32f)/.45f));bodyAlpha=bodyAlpha*bodyAlpha*(3-2*bodyAlpha);
 // Shade the actual displayed pose, keeping normals continuous across triangles.
 static std::vector<float> bodyNormals,bodyLight;
 if(!scoped)body_lighting::shade(model.data(),nv,indices.data(),indices.size(),bodyNormals,bodyLight);
 if(!scoped){static std::vector<Vertex> projectedBody;body_projection::project_unique<Vertex>(model.data(),nv,cam,drawFacing,pos,uvs.data(),bodyLight.data(),bodyAlpha,k,aspect,nz,fz,projectedBody);scene_integration::body_normals(projectedBody,bodyNormals.data(),drawFacing,cam);for(uint32_t index:indices)dst[written++]=projectedBody[index];}

 auto worldPoint=[&](const float* m,float* w){float x=-m[0],z=-m[1];w[0]=x*drawFacing[1]+z*drawFacing[0]+pos[0];w[1]=m[2]+pos[1]+.92871094f;w[2]=-x*drawFacing[0]+z*drawFacing[1]+pos[2];};
 auto vertex=[&](const float* w,float u,float v){if(written>=vertexCapacity){g_failed=true;return;}float d[3]={w[0]-cam[12],w[1]-cam[13],w[2]-cam[14]},x=0,y=0,z=0;for(int i=0;i<3;i++){x+=d[i]*cam[i];y+=d[i]*cam[i+4];z+=d[i]*cam[i+8];}dst[written++]={x*k/aspect,y*k,nz*(fz-z)/(fz-nz),z,u,v};};
 if(!scoped&&armed&&rifleWeight>.5f&&!rifle.prop_hidden){const auto&mesh=heldMesh;for(auto index:mesh.indices){auto m=arsenal::point(gunWorld,mesh.p[index]);float p[3]={m.x,m.y,m.z},w[3];worldPoint(p,w);vertex(w,mesh.uv[index*2],mesh.uv[index*2+1]);}}
 if(!scoped&&showSword){const auto&mesh=swordRig.mesh[0];for(auto index:mesh.indices){auto m=arsenal::point(swordWorld,mesh.p[index]);float local[3]={m.x,m.y,m.z},w[3];worldPoint(local,w);vertex(w,mesh.uv[index*2],mesh.uv[index*2+1]);dst[written-1].frameBlend=10;dst[written-1].a=bodyAlpha;}}
 float renderedMuzzle[3];worldPoint(tip,renderedMuzzle);rifle_muzzle((uintptr_t)activePlayer,now,renderedMuzzle,liveRifle&&actionWeight>.90f&&rifleWeight>.98f);
 ArsenalWorld world={};rifle_world(&world);
 // Draw the imported grenade on native physics positions, not predicted endpoints.
 for(unsigned j=0;j<world.projectile_count&&j<16;j++){const auto&b=world.projectiles[j];const auto&mesh=b.weapon==6?(b.bullet_id==98000700?rpgRig.projectile_mesh:arsenalRig.projectile_mesh):arsenalRig.mesh[7];arsenal::Q q={b.rotation[0],b.rotation[1],b.rotation[2],b.rotation[3]};float sc=1.f;auto matrix=arsenal::trs({b.position[0],b.position[1],b.position[2]},q,{sc,sc,sc});if(b.weapon==6){float len=sqrtf(b.velocity[0]*b.velocity[0]+b.velocity[1]*b.velocity[1]+b.velocity[2]*b.velocity[2]);if(len>.01f){float f[3]={b.velocity[0]/len,b.velocity[1]/len,b.velocity[2]/len},side[3]={f[2],0,-f[0]},l=hypotf(side[0],side[2]);if(l>.001f){for(int k=0;k<3;k++)side[k]/=l;float up[3]={f[1]*side[2]-f[2]*side[1],f[2]*side[0]-f[0]*side[2],f[0]*side[1]-f[1]*side[0]};for(int k=0;k<3;k++){matrix.v[k*4]=f[k];matrix.v[k*4+1]=side[k];matrix.v[k*4+2]=up[k];}}}}for(auto index:mesh.indices){auto w=arsenal::point(matrix,mesh.p[index]);float p[3]={w.x,w.y,w.z};vertex(p,mesh.uv[index*2],mesh.uv[index*2+1]);}}
 auto quad=[&](const float* a,const float* b,float halfWidth,int tile){float corners[4][3];for(int i=0;i<3;i++){corners[0][i]=a[i]-cam[4+i]*halfWidth;corners[1][i]=a[i]+cam[4+i]*halfWidth;corners[2][i]=b[i]+cam[4+i]*halfWidth;corners[3][i]=b[i]-cam[4+i]*halfWidth;}const int ids[]={0,1,2,0,2,3};const float uv[4][2]={{0,0},{0,1},{1,1},{1,0}};for(int j:ids)vertex(corners[j],(tile+uv[j][0])/8.f,(6+uv[j][1])/8.f);};
 auto billboard=[&](const float*p,float size,int tile){float a[3],b[3];for(int i=0;i<3;i++){a[i]=p[i]-cam[i]*size;b[i]=p[i]+cam[i]*size;}quad(a,b,size,tile);};
 size_t characterEnd=written;
 if(carVisible){if(!car_render::emit(carAsset,carState,dst,written,vertexCapacity,cam,k,aspect,nz,fz)){log("Car render packet rejected");written=characterEnd;}}
 size_t geometryCount=written;
 float floor[3],height=0;if(!driving&&!scoped&&!nativeDead&&shoulder_camera_ground((uintptr_t)activePlayer,floor,&height)&&height<2.5f){float radius=.45f+height*.20f;const float corner[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};const int order[]={0,1,2,0,2,3};for(int i:order){float p[3]={floor[0]+corner[i][0]*radius,floor[1]+.018f,floor[2]+corner[i][1]*radius};vertex(p,(corner[i][0]+1)*.5f,(corner[i][1]+1)*.5f);dst[written-1].a=expf(-height*1.2f);}}
 // Two small sole contacts follow the displayed feet. The broad original
 // shadow remains underneath; these fade independently when a foot is lifted.
 if(!driving&&!scoped&&!nativeDead&&shoulder_camera_ground((uintptr_t)activePlayer,floor,&height)&&height<.4f){
  static scene_integration::FootContacts contacts;contacts.prepare(arsenalRig.body);
  const int order[]={0,1,2,0,2,3};const float xy[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
  for(unsigned side=0;side<2;side++){auto f=contacts.sample(side,model.data(),nv,pos[1],floor[1]);if(!f.valid)continue;float center[3];worldPoint(f.local,center);
   for(int j:order){float p[3]={center[0]+xy[j][0]*.16f,floor[1]+.022f,center[2]+xy[j][1]*.20f};vertex(p,(xy[j][0]+1)*.5f,(xy[j][1]+1)*.5f);dst[written-1].a=f.opacity;dst[written-1].softness=-30;}
  }
 }
 size_t shadowEnd=written;
 if(liveRifle&&!rpg&&rifle.shot_time&&now>=rifle.shot_time){auto flash=muzzle_visual::evaluate(weapon,float(now-rifle.shot_time)/1000.f,rifle.sequence);
  if(flash.valid){auto local=heldMesh.muzzle;local.x+=1;auto end=arsenal::point(gunWorld,local);float m[3]={end.x,end.y,end.z},endpoint[3],axis[3],side[3]={cam[0],cam[1],cam[2]},len=0;worldPoint(m,endpoint);
   for(int a=0;a<3;a++){axis[a]=endpoint[a]-renderedMuzzle[a];len+=axis[a]*axis[a];}len=sqrtf(len);if(len>.00001f){for(float&a:axis)a/=len;
    if(flash.sideview){float view[3]={cam[12]-renderedMuzzle[0],cam[13]-renderedMuzzle[1],cam[14]-renderedMuzzle[2]};side[0]=axis[1]*view[2]-axis[2]*view[1];side[1]=axis[2]*view[0]-axis[0]*view[2];side[2]=axis[0]*view[1]-axis[1]*view[0];float sl=sqrtf(side[0]*side[0]+side[1]*side[1]+side[2]*side[2]);if(sl>.0001f)for(float&a:side)a/=sl;else for(int a=0;a<3;a++)side[a]=cam[4+a];}
    const float uv[4][2]={{0,0},{1,0},{1,1},{0,1}};const int order[]={0,1,2,0,2,3};for(int j:order){float point[3];for(int a=0;a<3;a++){float along=(uv[j][0]-flash.pivotAlong)*flash.length,across=(.5f-uv[j][1])*flash.width;point[a]=renderedMuzzle[a]+axis[a]*flash.centerAlong+(flash.sideview?axis[a]:cam[a])*along+(flash.sideview?side[a]:cam[4+a])*across;}float edge=.5f/4096.f;vertex(point,flash.uv[0]+edge+uv[j][0]*(flash.uv[2]-2*edge),flash.uv[1]+edge+uv[j][1]*(flash.uv[3]-2*edge));}
   }
  }
 }
 ArsenalEvents events={};rifle_events(&events);for(unsigned j=0;j<events.count&&j<16;j++){const auto&e=events.events[j];if(e.weapon>=6||now<e.time||now-e.time>100)continue;float progress=fminf(1.f,float(now-e.time)/90.f),dx=e.target[0]-e.from[0],dy=e.target[1]-e.from[1],dz=e.target[2]-e.from[2],len=sqrtf(dx*dx+dy*dy+dz*dz),tail=fmaxf(0.f,progress-fminf(.5f,3.f/fmaxf(len,.01f)));float a[3],b[3];for(int i=0;i<3;i++){a[i]=e.from[i]+(e.target[i]-e.from[i])*tail;b[i]=e.from[i]+(e.target[i]-e.from[i])*progress;}quad(a,b,.012f,1);}
 size_t ordinaryEffectEnd=written;
 // New explosions use original GTA sheets and independently aged particles.
 // Native impact coordinates are fixed world coordinates; no player transform applies.
 static std::vector<gta_fx::Sprite> blastSprites,perEvent;blastSprites.clear();
 struct CachedBlast {uint64_t handle=0,time=0;gta_fx::Event event;};static CachedBlast cachedBlasts[32];static unsigned cacheCursor=0;
 std::vector<const ArsenalImpact*> recentImpacts;for(unsigned j=0;j<world.impact_count&&j<32;j++){const auto&e=world.impacts[j];if(now>=e.time&&now-e.time<=5500)recentImpacts.push_back(&e);}
 std::sort(recentImpacts.begin(),recentImpacts.end(),[](const auto*a,const auto*b){return a->time>b->time;});
 if(recentImpacts.size()>8)recentImpacts.resize(8);
 unsigned rpgSpriteCount=0;for(const auto*ep:recentImpacts){const auto&e=*ep;CachedBlast* cached=nullptr;for(auto&c:cachedBlasts)if(c.handle==e.handle&&c.time==e.time){cached=&c;break;}
  if(!cached){cached=&cachedBlasts[cacheCursor++%32];cached->handle=e.handle;cached->time=e.time;cached->event=gta_fx::makeEvent(e.handle^e.time,{e.position[0],e.position[1],e.position[2]},gta_fx::impactVariant(e.bullet_id==98000701,e.ground_contact!=0));
   if(now>=e.time&&now-e.time<250){float dx=e.position[0]-pos[0],dy=e.position[1]-pos[1],dz=e.position[2]-pos[2],d=sqrtf(dx*dx+dy*dy+dz*dz);float falloff=fmaxf(0.f,1.f-d/35.f);if(falloff>0)shoulder_camera_blast((uintptr_t)activePlayer,.025f*falloff*falloff,.6f);}
  }
  unsigned budget=std::min(128u,256u-unsigned(blastSprites.size()));if(e.bullet_id==98000701)budget=std::min(budget,128u-rpgSpriteCount);gta_fx::evaluate(cached->event,float(now-e.time)/1000.f,perEvent,budget);if(e.bullet_id==98000701)rpgSpriteCount+=unsigned(perEvent.size());blastSprites.insert(blastSprites.end(),perEvent.begin(),perEvent.end());
 }
 // Original RPG smoke follows observed projectile history, never a guessed ray endpoint.
 struct TrailCache{uint64_t handle=0,lastSeen=0;float age=0;gta_fx::RpgTrail trail;};static TrailCache trailCache[2];
 if(lifeChanged)for(auto&t:trailCache)t=TrailCache{};
 for(unsigned j=0;j<world.projectile_count&&j<16;j++){const auto&p=world.projectiles[j];if(p.bullet_id!=98000700)continue;TrailCache*t=nullptr;
  for(auto&c:trailCache)if(c.handle==p.handle){t=&c;break;}
  if(!t){t=trailCache[0].lastSeen<=trailCache[1].lastSeen?&trailCache[0]:&trailCache[1];*t=TrailCache{};t->handle=p.handle;t->trail.reset(p.handle^world.time);}
  if(p.age+.04f<t->age)t->trail.reset(p.handle^world.time);t->age=p.age;t->lastSeen=world.time;
  t->trail.feed(world.time,{p.position[0],p.position[1],p.position[2]},{p.velocity[0],p.velocity[1],p.velocity[2]});
 }
 for(auto&t:trailCache)if(t.lastSeen&&now>=t.lastSeen&&now-t.lastSeen<3000){unsigned budget=std::min(24u,std::min(128u-rpgSpriteCount,256u-unsigned(blastSprites.size())));gta_fx::evaluateRpgTrail(t.trail,now,perEvent,budget);rpgSpriteCount+=unsigned(perEvent.size());blastSprites.insert(blastSprites.end(),perEvent.begin(),perEvent.end());}
 if(liveRifle&&rpg&&rifle.shot_time&&now>=rifle.shot_time&&now-rifle.shot_time<300){
  auto endLocal=heldMesh.muzzle;endLocal.x+=1;auto ep=arsenal::point(gunWorld,endLocal);float e[3]={ep.x,ep.y,ep.z},end[3];worldPoint(e,end);
  gta_fx::Vec3 forward={end[0]-renderedMuzzle[0],end[1]-renderedMuzzle[1],end[2]-renderedMuzzle[2]};float len=sqrtf(forward.x*forward.x+forward.y*forward.y+forward.z*forward.z);
  if(len>.0001f){forward=gta_fx::scale(forward,1/len);gta_fx::Vec3 right={forward.z,0,-forward.x};float rl=sqrtf(right.x*right.x+right.z*right.z);right=rl>.001f?gta_fx::scale(right,1/rl):gta_fx::Vec3{1,0,0};gta_fx::Vec3 up={forward.y*right.z-forward.z*right.y,forward.z*right.x-forward.x*right.z,forward.x*right.y-forward.y*right.x};
   gta_fx::evaluateRpgMuzzle(rifle.sequence,float(now-rifle.shot_time)*.001f,{renderedMuzzle[0],renderedMuzzle[1],renderedMuzzle[2]},forward,right,up,perEvent,std::min(16u,256u-unsigned(blastSprites.size())));blastSprites.insert(blastSprites.end(),perEvent.begin(),perEvent.end());
  }
 }
 // Alpha smoke/debris is sorted back-to-front; additive fire follows it.
 auto cameraDepth=[&](const gta_fx::Sprite&p){return (p.center.x-cam[12])*cam[8]+(p.center.y-cam[13])*cam[9]+(p.center.z-cam[14])*cam[10];};
 std::stable_sort(blastSprites.begin(),blastSprites.end(),[&](const auto&a,const auto&b){if(a.additive!=b.additive)return !a.additive;return cameraDepth(a)>cameraDepth(b);});
 size_t alphaEnd=written;bool additiveBegun=false;
 for(const auto&p:blastSprites){if(p.additive&&!additiveBegun){alphaEnd=written;additiveBegun=true;}if(written+6>vertexCapacity){g_failed=true;break;}
  float right[3]={cam[0],cam[1],cam[2]},up[3]={cam[4],cam[5],cam[6]};
  if(p.oriented){up[0]=p.up.x;up[1]=p.up.y;up[2]=p.up.z;float view[3]={p.center.x-cam[12],p.center.y-cam[13],p.center.z-cam[14]};right[0]=up[1]*view[2]-up[2]*view[1];right[1]=up[2]*view[0]-up[0]*view[2];right[2]=up[0]*view[1]-up[1]*view[0];float len=sqrtf(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]);if(len>.0001f)for(int a=0;a<3;a++)right[a]/=len;else for(int a=0;a<3;a++)right[a]=cam[a];}
  // Keep the original texture/color; the V3 HDR gain clipped its gradients to white.
  const float emission=1.f; // Donor texture gradients must not be multiplied by HDR gain in an LDR pass.
  float cs=cosf(p.rotation),sn=sinf(p.rotation),origin[3]={p.center.x,p.center.y,p.center.z};const int order[]={0,1,2,0,2,3};const float corners[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
  for(int n:order){float x=corners[n][0]*p.width*.5f,y=corners[n][1]*p.height*.5f;float xx=x*cs-y*sn,yy=x*sn+y*cs,point[3];for(int a=0;a<3;a++)point[a]=origin[a]+right[a]*xx+up[a]*yy;
   float u=(corners[n][0]+1)*.5f,v=(1-corners[n][1])*.5f,edge=.5f/4096.f;
   vertex(point,p.uv[0]+edge+u*(p.uv[2]-2*edge),p.uv[1]+edge+v*(p.uv[3]-2*edge));auto& out=dst[written-1];out.r=p.rgba[0]*emission;out.g=p.rgba[1]*emission;out.b=p.rgba[2]*emission;out.a=p.rgba[3];out.nu=p.nextUv[0]+edge+u*(p.nextUv[2]-2*edge);out.nv=p.nextUv[1]+edge+v*(p.nextUv[3]-2*edge);out.frameBlend=p.frameBlend+(p.additive?1.f:0.f)+(p.soft?2.f:0.f)+(p.rgbAlpha?4.f:0.f)+(p.lit?8.f:0.f)+(p.rgbCutoff?16.f:0.f);out.softness=p.softness;
  }
 }
 if(!additiveBegun)alphaEnd=written;
 size_t effectEnd=written;frameCost.mark(2);
 auto rectUI=[&](float l,float t,float rr,float b,float color){if(written+6>vertexCapacity){g_failed=true;return;}float pts[4][2]={{l,t},{rr,t},{rr,b},{l,b}};const int order[]={0,1,2,0,2,3};for(int i:order)dst[written++]={pts[i][0]*2.f/g_bbW,-pts[i][1]*2.f/g_bbH,0,1,-color-.01f,0};};
 float reticleX=0,reticleY=0;
 if(freeLockedAim){reticleX=freeAim.ndc[0]*g_bbW*.5f;reticleY=-freeAim.ndc[1]*g_bbH*.5f;}
 auto reticleRect=[&](float l,float t,float r,float b,float c){rectUI(l+reticleX,t+reticleY,r+reticleX,b+reticleY,c);};
 if(liveRifle&&!scoped){float gap=rifle.aiming?3.f:5.f;for(int pass=0;pass<2;pass++){float edge=pass?0.f:1.f,c=pass?1.f:0.f;reticleRect(-1-edge,-1-edge,1+edge,1+edge,c);reticleRect(-gap-5-edge,-.6f-edge,-gap+edge,.6f+edge,c);reticleRect(gap-edge,-.6f-edge,gap+5+edge,.6f+edge,c);reticleRect(-.6f-edge,-gap-5-edge,.6f+edge,-gap+edge,c);reticleRect(-.6f-edge,gap-edge,.6f+edge,gap+5+edge,c);}}
 auto colorRect=[&](float l,float t,float rr,float b,float red,float green,float blue,float alpha,float flag){size_t start=written;rectUI(l,t,rr,b,0);for(size_t i=start;i<written;i++){dst[i].r=red;dst[i].g=green;dst[i].b=blue;dst[i].a=alpha;dst[i].frameBlend=flag;}};
 if(liveRifle&&scopeAmount>.01f){colorRect(-float(g_bbW)*.5f,-float(g_bbH)*.5f,g_bbW*.5f,g_bbH*.5f,0,0,0,scopeAmount,-7);
  if(scoped){scope_reticle::draw(g_bbH*.415f,reticleX,reticleY,rectUI);GtaHud scopeHud(dst,written,float(g_bbW),float(g_bbH));char zoomText[48];snprintf(zoomText,sizeof(zoomText),"%.1fx   SCROLL: ZOOM",shoulder_camera_zoom());scopeHud.text(zoomText,0,g_bbH*.37f,16*g_bbH/720.f,1,1,1,1,true);}}
 if(!nativeDead&&nativeHudState==3){PlayerStatus status={};if(player_status_snapshot(&status)&&status.owner==(uintptr_t)activePlayer&&!scoped)draw_gta_hud(dst,written,g_bbW,g_bbH,status,rifle,stateFresh,cam,drawFacing,now);if(!scoped)draw_arsenal_ui(dst,written,rifle,stateFresh,g_bbW,g_bbH);}
 // Reticle/HUD stay below native prompts, as before. Death/console overlays
 // appended afterwards intentionally remain above native UI.
 for(size_t i=effectEnd;i<written;i++)dst[i].softness=1;
 if(float fade=sceneLife.wasted(now);fade>0){colorRect(-float(g_bbW)*.5f,-float(g_bbH)*.5f,g_bbW*.5f,g_bbH*.5f,0,0,0,.22f*fade,-6);float h=g_bbH*.155f,w=h*(690.f/184.f);colorRect(-float(g_bbW)*.5f,-h*.65f,g_bbW*.5f,h*.65f,0,0,0,.38f*fade,-6);
  if(written+6<=vertexCapacity){const int order[]={0,1,2,0,2,3};const float xy[4][2]={{-.5f,-.5f},{.5f,-.5f},{.5f,.5f},{-.5f,.5f}};for(int j:order){Vertex v={xy[j][0]*w*2/g_bbW,-xy[j][1]*h*2/g_bbH,0,1,(xy[j][0]+.5f)*690/1024,(xy[j][1]+.5f)*184/256};v.r=.72f;v.g=.04f;v.b=.04f;v.a=fade;v.frameBlend=-5;dst[written++]=v;}}}
 CheatConsoleSnapshot consoleState={};if(!nativeDead&&cheat_console_snapshot(&consoleState))draw_cheat_ui(dst,written,g_bbW,g_bbH,consoleState,now);
 frameCost.mark(3);
 if(g_failed||written>vertexCapacity){log("Arsenal vertex budget exceeded");g_failed=true;restore_player();return;}
 if(FAILED(r.alloc->Reset())||FAILED(g_list->Reset(r.alloc,g_pso))){g_failed=true;return;}gpuTiming.begin(g_list,g_ringPos%kRing);
 if(!atlasReady){D3D12_TEXTURE_COPY_LOCATION to={};to.pResource=atlas;to.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;D3D12_TEXTURE_COPY_LOCATION from={};from.pResource=atlasUpload;from.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;from.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,4096,4096,1,16384};g_list->CopyTextureRegion(&to,0,0,0,&from,nullptr);transition(atlas,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);to.pResource=blastAtlas;from.pResource=blastUpload;g_list->CopyTextureRegion(&to,0,0,0,&from,nullptr);transition(blastAtlas,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);to.pResource=wastedAtlas;from.pResource=wastedUpload;from.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,1024,256,1,4096};g_list->CopyTextureRegion(&to,0,0,0,&from,nullptr);transition(wastedAtlas,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);to.pResource=hudAtlas;from.pResource=hudUpload;from.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,1024,1024,1,4096};g_list->CopyTextureRegion(&to,0,0,0,&from,nullptr);transition(hudAtlas,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);to.pResource=mapAtlas;from.pResource=mapUpload;from.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,4096,4096,1,16384};g_list->CopyTextureRegion(&to,0,0,0,&from,nullptr);transition(mapAtlas,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);to.pResource=swordAtlas;from.pResource=swordUpload;from.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,1024,1024,1,4096};g_list->CopyTextureRegion(&to,0,0,0,&from,nullptr);transition(swordAtlas,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);if(carReady&&carUpload){to.pResource=carAtlas;from.pResource=carUpload;from.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,2048,2048,1,8192};g_list->CopyTextureRegion(&to,0,0,0,&from,nullptr);transition(carAtlas,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);}atlasReady=true;}
 float lightDelta[3]={pos[0]-cam[12],pos[1]+.4f-cam[13],pos[2]-cam[14]},lightX=0,lightY=0,lightZ=0;for(int a=0;a<3;a++){lightX+=lightDelta[a]*cam[a];lightY+=lightDelta[a]*cam[4+a];lightZ+=lightDelta[a]*cam[8+a];}float lightU=lightZ>.05f?(.5f+lightX*k/aspect/lightZ*.5f):.5f,lightV=lightZ>.05f?(.5f-lightY*k/lightZ*.5f):.5f;lightU=fmaxf(13.f/g_bbW,fminf(1.f-13.f/g_bbW,lightU));lightV=fmaxf(13.f/g_bbH,fminf(1.f-13.f/g_bbH,lightV));
 UINT bi=g_sc->GetCurrentBackBufferIndex();if(bi>=g_bbCount){g_list->Close();g_failed=true;return;}if(!scoped||effectEnd>ordinaryEffectEnd){transition(g_bb[bi],D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_SOURCE);transition(sceneCopy,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
  D3D12_TEXTURE_COPY_LOCATION from={},to={};from.pResource=g_bb[bi];from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;to.pResource=sceneCopy;to.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  UINT x=UINT(lightU*g_bbW),y=UINT(lightV*g_bbH);D3D12_BOX patch={x-12,y-12,0,x+13,y+13,1};g_list->CopyTextureRegion(&to,patch.left,patch.top,0,&from,&patch);

  transition(sceneCopy,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);transition(g_bb[bi],D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
 }else transition(g_bb[bi],D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
 if(uiLayer&&nativePrompt.subtitles)record_ui_sample(uiA,uiB,g_bb[bi]);
 ID3D12Resource* depth=g_depthBound>=0?g_depthCands[g_depthBound]:nullptr;if(depth)transition(depth,D3D12_RESOURCE_STATE_DEPTH_WRITE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
 auto rtv=rtv_cpu(bi);auto dsv=g_dsvHeap->GetCPUDescriptorHandleForHeapStart();g_list->ClearDepthStencilView(dsv,D3D12_CLEAR_FLAG_DEPTH,0,0,0,nullptr);g_list->OMSetRenderTargets(1,&rtv,FALSE,&dsv);D3D12_VIEWPORT vp={0,0,float(g_bbW),float(g_bbH),0,1};D3D12_RECT rect={0,0,LONG(g_bbW),LONG(g_bbH)};g_list->RSSetViewports(1,&vp);g_list->RSSetScissorRects(1,&rect);g_list->SetGraphicsRootSignature(g_rootSig);g_list->SetDescriptorHeaps(1,&g_srvHeap);g_list->SetGraphicsRootDescriptorTable(0,g_srvHeap->GetGPUDescriptorHandleForHeapStart());float params[8]={depth?1.f:0.f,nz,fz,uiLayer?1.f:0.f,float(g_bbW),float(g_bbH),lightU,lightV};g_list->SetGraphicsRoot32BitConstants(1,8,params,0);D3D12_VERTEX_BUFFER_VIEW vb={r.upload->GetGPUVirtualAddress(),UINT(written*sizeof(Vertex)),sizeof(Vertex)};g_list->IASetVertexBuffers(0,1,&vb);g_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);if(shadowEnd>geometryCount){g_list->SetPipelineState(shadowPso);g_list->DrawInstanced(UINT(shadowEnd-geometryCount),1,UINT(geometryCount),0);g_list->SetPipelineState(g_pso);}g_list->DrawInstanced(UINT(characterEnd),1,0,0);if(geometryCount>characterEnd){g_list->SetPipelineState(carPso);g_list->DrawInstanced(UINT(geometryCount-characterEnd),1,UINT(characterEnd),0);}if(ordinaryEffectEnd>shadowEnd){g_list->SetPipelineState(g_fxPso);g_list->DrawInstanced(UINT(ordinaryEffectEnd-shadowEnd),1,UINT(shadowEnd),0);}if(alphaEnd>ordinaryEffectEnd){g_list->SetPipelineState(g_blastAlphaPso);g_list->DrawInstanced(UINT(alphaEnd-ordinaryEffectEnd),1,UINT(ordinaryEffectEnd),0);}if(effectEnd>alphaEnd){g_list->SetPipelineState(g_blastAddPso);g_list->DrawInstanced(UINT(effectEnd-alphaEnd),1,UINT(alphaEnd),0);}if(written>effectEnd){g_list->SetPipelineState(g_uiPso);g_list->DrawInstanced(UINT(written-effectEnd),1,UINT(effectEnd),0);}if(depth)transition(depth,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_DEPTH_WRITE);transition(g_bb[bi],D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);gpuTiming.end(g_list,g_ringPos%kRing);if(FAILED(g_list->Close())){g_failed=true;return;}ID3D12CommandList* lists[]={g_list};g_queue->ExecuteCommandLists(1,lists);r.fence=++g_fenceNext;g_queue->Signal(g_fence,r.fence);if(atlasUpload&&!atlasUploadFence)atlasUploadFence=r.fence;frameCost.drew=true;g_ringPos++;lastDrawMs=now_ms();hide_player();static uint64_t last=0;if(now_ms()-last>5000){last=now_ms();log("DRAW frame=%u vertices=%zu depth=%d player=%.2f,%.2f,%.2f near=%.3f far=%.1f speed=%.2f",frame,written,g_depthBound,pos[0],pos[1],pos[2],nz,fz,speed);}
}
typedef HRESULT(STDMETHODCALLTYPE* Present_t)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* Present1_t)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
typedef HRESULT(STDMETHODCALLTYPE* Resize_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* Resize1_t)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*,
                                              IUnknown* const*);

static volatile LONG g_inPresent = 0;

static HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {
    InflightGuard guard;
    bool owns=InterlockedCompareExchange(&g_inPresent,1,0)==0;
    if(owns)composite(sc,flags);
    HRESULT result=((Present_t)g_pp->orig[kSlotPresent])(sc,sync,flags);
    if(owns)InterlockedExchange(&g_inPresent,0);
    return result;
}

static HRESULT STDMETHODCALLTYPE hkPresent1(IDXGISwapChain1* sc, UINT sync, UINT flags,
                                            const DXGI_PRESENT_PARAMETERS* params) {
    InflightGuard guard;
    bool owns=InterlockedCompareExchange(&g_inPresent,1,0)==0;
    if(owns)composite(sc,flags);
    HRESULT result=((Present1_t)g_pp->orig[kSlotPresent1])(sc,sync,flags,params);
    if(owns)InterlockedExchange(&g_inPresent,0);
    return result;
}

// The swapchain can only resize once nobody holds its buffers.
static void before_resize(void* sc) {
    if ((IDXGISwapChain3*)sc != g_sc || !g_bbCount) return;
    wait_idle();
    release_backbuffers();
    log("compositor: swapchain resizing; back buffers released");
}

static HRESULT STDMETHODCALLTYPE hkResizeBuffers(IDXGISwapChain* sc, UINT n, UINT w, UINT h, DXGI_FORMAT f, UINT fl) {
    InflightGuard guard;
    before_resize(sc);
    return ((Resize_t)g_pp->orig[kSlotResize])(sc, n, w, h, f, fl);
}

static HRESULT STDMETHODCALLTYPE hkResizeBuffers1(IDXGISwapChain3* sc, UINT n, UINT w, UINT h, DXGI_FORMAT f, UINT fl,
                                                  const UINT* mask, IUnknown* const* queues) {
    InflightGuard guard;
    before_resize(sc);
    return ((Resize1_t)g_pp->orig[kSlotResize1])(sc, n, w, h, f, fl, mask, queues);
}


#include "windows_swapchain.h"
static bool install(){if(g_pp)return true;if(!GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"wine_get_version"))return windows_swapchain::install();void** table=find_swapchain_table();if(!table||!mem_readable(table,41*sizeof(void*)))return false;auto pg=(PresentPage*)VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!pg)return false;memset(pg,0,sizeof(*pg));pg->table=table;for(int k=0;k<kHookCount;k++){pg->orig[k]=table[kVtblIndex[k]];write_stub(pg,k);}FlushInstructionCache(GetCurrentProcess(),pg,sizeof(*pg));pg->magic=page_magic();g_pp=pg;pg->target[0]=(void*)&hkPresent;pg->target[1]=(void*)&hkResizeBuffers;pg->target[2]=(void*)&hkPresent1;pg->target[3]=(void*)&hkResizeBuffers1;for(int k=0;k<kHookCount;k++)table[kVtblIndex[k]]=pg->stub[k];log("Original rig proof hooked");return true;}
}
extern "C" void proof_start(){using namespace mb;
 if(!modelFile.load(igobygod::data_path("model.bin").c_str())){log("Invalid mapped model");return;}
 nv=modelFile.view.nv;nf=modelFile.view.nf;nc=modelFile.view.nc;indices=modelFile.view.indices;uvs=modelFile.view.uvs;poses=modelFile.view.poses;memcpy(durations,modelFile.view.durations,sizeof(durations));log("Mapped %u vertices; original animation floats demand-paged",nv);
 if(!arsenalRig.load(igobygod::data_path("arsenal-joints.bin").c_str(),igobygod::data_path("arsenal-mesh.bin").c_str(),nv)){log("Arsenal packet invalid; disabled");return;}
 if(!arsenalRig.loadProjectile(igobygod::data_path("projectile-mesh.bin").c_str())){log("Projectile mesh invalid; disabled");return;}
 if(!rpgRig.load(igobygod::data_path("rpg-joints.bin").c_str(),igobygod::data_path("arsenal-mesh.bin").c_str(),nv)||!rpgRig.loadWeapon(igobygod::data_path("rpg-mesh.bin").c_str(),6)||!rpgRig.loadProjectile(igobygod::data_path("rpg-projectile-mesh.bin").c_str())){log("Original RPG packet invalid; disabled");return;}
 if(auto c=arsenalRig.find(7,4))rifle_grenade_recovery(unsigned(c->duration*1000.f+.5f));
 if(auto c=rpgRig.find(6,2))rifle_rpg_fire_duration(unsigned(c->duration*1000.f+.5f));
 if(auto c=rpgRig.find(6,3))rifle_rpg_reload_duration(unsigned(c->duration*1000.f+.5f));
 if(!directionalRig.load(igobygod::data_path("armed-directional.bin").c_str(),arsenalRig.bones)){log("Directional packet invalid; disabled");return;}
 if(!meleePacket.load(igobygod::data_path("melee-joints.bin").c_str(),arsenalRig.bones,igobygod::data_path("sword-fallback-attachment.bin").c_str())||!swordRig.loadWeapon(igobygod::data_path("sword-mesh.bin").c_str(),0)){log("Native melee packet invalid; disabled");return;}
 log("MELEE_ASSETS clips=%zu sword_vertices=%zu",meleePacket.clips.size(),swordRig.mesh[0].p.size());
 if(!native_ui::init()){log("Native UI validation failed; custom renderer disabled");return;}
 bool driverLoaded=driverAnimation.load(igobygod::data_path("driver.bin").c_str(),arsenalRig.bones);log("DRIVER_ANIMATION valid=%u",unsigned(driverLoaded));
 bool carLoaded=carAsset.load(igobygod::data_path("buffalo2.carbin").c_str());log("BUFFALO_ASSET valid=%u meshes=%zu",unsigned(carLoaded),carAsset.meshes.size());
 size_t carIndices=0;for(const auto&m:carAsset.meshes)carIndices+=m.indices.size();
 vertexCapacity=carIndices+indices.size()+swordRig.mesh[0].indices.size()+std::max(arsenalRig.maxMeshIndices(),rpgRig.maxMeshIndices())*26+65536;
 log("Loaded original GTA arsenal: %u bones, %zu clips",arsenalRig.bones,arsenalRig.clips.size());
 if(!rollFile.load(igobygod::data_path("roll.bin").c_str(),nv)||!deathFile.load(igobygod::data_path("death.bin").c_str(),nv)){log("Invalid mapped action packet; renderer disabled");return;}
 rollClips=rollFile.clips;deathClips=deathFile.clips;
 log("ANIMATION_MEMORY file_backed_bytes=%llu private_pose_bytes=0",(unsigned long long)(modelFile.mapping.size()+rollFile.mapping.size()+deathFile.mapping.size()));
 log("ARSENAL_POLISH_V4_9: previous Q behavior restored; cross-list UI capture and persistent reward character");
 native_lock_marker_start();
 // V4.9: native movement override removed at user request (V4.7 Q behavior).
 HANDLE rifleThread=CreateThread(nullptr,0,[](void*)->DWORD{rifle_start();return 0;},nullptr,0,nullptr);if(rifleThread)CloseHandle(rifleThread);
 static unsigned char wastedPcm[8*44100*2];FILE*audio=igobygod::open(igobygod::data_path("audio/wasted.pcm").c_str(),"rb");if(audio){auto size=fread(wastedPcm,1,sizeof(wastedPcm),audio);fclose(audio);native_sound_sample(7,wastedPcm,(uint32_t)size);}
 HANDLE cameraThread=CreateThread(nullptr,0,[](void*)->DWORD{shoulder_camera_start();return 0;},nullptr,0,nullptr);if(cameraThread)CloseHandle(cameraThread);
 for(int i=0;i<180&&!install();i++)Sleep(1000);
 while(g_pp){if(InterlockedCompareExchange(&g_depthScanState,2,1)==1){scan_depth_views();InterlockedExchange(&g_depthScanState,3);}Sleep(1000);}
}
