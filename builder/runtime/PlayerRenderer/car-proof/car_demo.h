#pragma once
#include <stdint.h>

// Stable renderer prefix. Column-major matrices in native ER Y-up metres.
// Convert GTA vertices/normals to (x,z,-y) first; local car forward is -Z.
struct CarSnapshot {
    uint64_t time;
    uintptr_t owner;
    uint32_t visible,driving;
    float chassis[16],wheels[4][16];
    uint32_t status,spawn_sequence,legacy_rays,probe_rays,probe_sweeps;
    uint32_t normal_probe_verified,sphere_probe_verified,drive_blocked;
    float ground_spread_m,last_probe_ms;
    uint32_t driver_phase;float phase_seconds,speed,steer,entry[3],exit[3];
};
enum CarDemoStatus : uint32_t {
    CAR_DEMO_EMPTY=0, CAR_DEMO_CHECKING_SPAWN=1,
    CAR_DEMO_PARKED_DIAGNOSTIC=2, CAR_DEMO_SPAWN_REJECTED=3,
    CAR_DEMO_NATIVE_GUARD_FAILED=4
};
extern "C" void car_request_spawn(uintptr_t owner); // queue only, any thread
extern "C" void car_request_despawn(); // queue only, any thread
extern "C" void car_request_remove(uintptr_t owner); // CARCLEAR queue only
extern "C" void car_tick(); // only verified game task107, after player_status_tick
extern "C" int car_snapshot(CarSnapshot* out); // copy only, no native query
extern "C" void car_probe_enable(int ray_probe,int sphere_probe); // default off; explicit diagnostic build opt-in

extern "C" int car_controls_owned(uintptr_t owner);
