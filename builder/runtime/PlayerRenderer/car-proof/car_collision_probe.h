#pragma once
#include <stdint.h>
enum CarProbeResult : int32_t { CAR_PROBE_INVALID=-1,CAR_PROBE_DISABLED=-2,CAR_PROBE_GUARD_FAILED=-3,CAR_PROBE_WRONG_THREAD=-4,CAR_PROBE_BUDGET=-5,CAR_PROBE_MISS=0,CAR_PROBE_HIT=1 };
struct CarProbeHit {float position[3],normal[3],fraction,elapsed_ms;};
struct CarNativeProbe {
    uintptr_t base,world,owner;uint32_t game_thread,verified,enabled,ray_limit,sphere_limit,ray_count,sphere_count;
    uint64_t frame;float elapsed_ms;
};
// Initialization performs static checks only. Caller SHA256 approval must come
// from the existing exact-executable loader. No native query is made here.
extern "C" int car_native_probe_init(CarNativeProbe*,uintptr_t module,uint32_t game_thread,int loader_sha256_verified);
extern "C" void car_native_probe_arm(CarNativeProbe*,int ray,int sphere);
extern "C" int car_native_probe_begin_frame(CarNativeProbe*,uintptr_t owner,int loaded_alive_world,uint32_t ray_limit,uint32_t sphere_limit);
extern "C" int car_native_probe_ray(CarNativeProbe*,const float*from3,const float*to3,CarProbeHit*);
// position is the travelled sphere CENTER. Initial-overlap recovery is unproven.
extern "C" int car_native_probe_sphere(CarNativeProbe*,const float*from3,const float*to3,float radius,CarProbeHit*);
