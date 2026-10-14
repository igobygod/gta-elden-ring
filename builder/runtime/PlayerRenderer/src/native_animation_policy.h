#pragma once
#include <stdint.h>
// Exact-build evidence: helper1B4090 returns raw - (raw/1000000)*1000000;
// producer3F2063 combines bank*1000000+animation. Original TAE/HKX filenames
// independently confirm banks. Special202xxx animations remain202xxx.
namespace native_animation_policy {
inline int32_t base(int32_t raw){return raw<0?raw:raw%1000000;}
inline bool control_action(int32_t raw){int32_t id=base(raw);return id>=0&&id<27000;}
// Camera framing remains live during native jump/dodge/roll actions (27xxx)
// and the verified 202xxx special-action family. The weapon adapter continues
// to use control_action() so ER retains those actions and their input priority.
inline bool camera_action(int32_t raw){int32_t id=base(raw);return id>=0&&(id<28000||(id>=202000&&id<203000));}
// The only observed weapon action that currently interrupts the camera/control
// adapter is the RPG's exact banked action. Keep this exact-build exception
// narrow; neighboring IDs and other weapon slots remain native-only.
inline bool rpg_weapon_action(int32_t raw,unsigned weapon){return raw==42034000&&weapon==6;}
}
