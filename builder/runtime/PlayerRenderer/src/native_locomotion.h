#pragma once
#include <stdint.h>
struct NativeLocomotionStatus {
 int installed; uint64_t overrides[4]; uintptr_t owner;
 uint64_t time; int32_t raw;
};
// Once during renderer setup, after existing native input hooks initialize.
// Exact-build signatures are required; failure preserves original behavior.
extern "C" int native_locomotion_init();
// True only after a recent override for this same live player, while the
// current local-player/action/weapon/menu gate is still valid. The renderer
// can use native body yaw instead of the V4.7 displacement compensation.
extern "C" int native_locomotion_active(uintptr_t owner);
extern "C" void native_locomotion_status(NativeLocomotionStatus*out);
