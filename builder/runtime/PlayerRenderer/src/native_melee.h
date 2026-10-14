#pragma once
#include <stdint.h>
// These operations belong to the verified native post-physics task, not the
// render/input thread. Parent must first pass its full executable hash check.
extern "C" int native_melee_init();
extern "C" int native_melee_bind_game_thread();
struct NativeMeleeState {uint32_t mode,left_hand,slot,item;int32_t status;};
// desired: 0 = restore leased equipment for firearms, 1 = both fists,
// 2 = Sword of Night and Flame in active right slot + empty left hand.
// expected_chr must be the current live native player. Do not call during
// loading/menu/death or from a render thread. Repeated unchanged calls are safe.
// Returns 1 only after native equipment readback confirms both hand slots.
// On failure, out.mode=0. Published snapshots should be copied to RifleState.
// Missing fist/sword items may be granted once per PlayerGameData session and
// persist in the offline save. No player stats, HP or combat memory are written.
extern "C" int native_melee_select(uintptr_t expected_chr,uint32_t desired,NativeMeleeState* out);
// 1 initialized, 2 confirmed; -1 ABI, -2 thread, -3 context,
// -4 missing param, -5 grant failed, -6 invalid mode, -7 equip readback,
// -8 left-hand sword unsupported, -9 previous item no longer in inventory,
// -10 waiting for retry (unchanged failures are throttled to once per second).
extern "C" int native_melee_status();
