#pragma once
#include <stdint.h>
struct RifleState {uint64_t time,shot_time,reload_start;uint32_t active,aiming,ammo,sequence;float from[3],to[3];uint32_t weapon,wheel_open,wheel_hover,capacity,effect_kind;uint64_t throw_start;uint32_t prop_hidden,no_reload,lock_on;int32_t native_action;uint32_t gate_reason,launcher_variant,lock_target_valid;float lock_target[3];uint64_t grenade_release_time;uint32_t reserve;uint32_t melee_variant,melee_mode,melee_left_hand;int32_t melee_status;};
extern "C" void rifle_start();
extern "C" int rifle_state(RifleState* out);
extern "C" void rifle_muzzle(uintptr_t owner,uint64_t time,const float* position,int ready);

// Accepted launch events; simulated distinguishes bounded grenade/launcher flight from native firearm spawn.
// Endpoint is the camera ray, NOT a confirmed impact.
struct ArsenalSpawnEvent {uint64_t time;uint32_t sequence,weapon,native_handle;float from[3],target[3];uint32_t simulated,launcher_variant,bullet_id;};
struct ArsenalEvents {uint32_t count;ArsenalSpawnEvent events[16];};
extern "C" int rifle_events(ArsenalEvents* out);

// Game-thread snapshot: native CSBulletIns positions and bounded grenade/launcher flight.
struct ArsenalProjectile {uint64_t handle;uint32_t bullet_id,weapon;float age,radius,position[3],velocity[3],rotation[4];uint32_t simulated;};
struct ArsenalImpact {uint64_t handle,time;uint32_t bullet_id,weapon;float radius,position[3];uint32_t ground_contact;};
struct ArsenalWorld {uint64_t time;uint32_t projectile_count,impact_count;ArsenalProjectile projectiles[16];ArsenalImpact impacts[32];};
extern "C" int rifle_world(ArsenalWorld* out);

// Final visual camera ray. Optional adapter; falls back to verified native camera.
extern "C" void rifle_view(uintptr_t owner,uint64_t time,const float* origin,const float* direction,int ready);

extern "C" void rifle_cheat_request(uint32_t kind,uintptr_t owner); //3 fill all magazines,4 toggle no reload; consumed on rifle task

extern "C" void rifle_grenade_recovery(uint32_t duration_ms); //renderer supplies actual slot7/action4 clip duration

extern "C" void rifle_rpg_reload_duration(uint32_t duration_ms);
extern "C" void rifle_rpg_fire_duration(uint32_t duration_ms);
