#pragma once
#include <stdint.h>
struct PlayerHostile {uint64_t handle;float position[3];uint32_t team,category,model_id,flags;};
struct PlayerStatus {
 uint64_t time,radar_time;uintptr_t owner;
 int32_t hp,max_hp,stamina,max_stamina,fp,max_fp;
 uint32_t valid,dead,stamina_valid,fp_valid,health_hotkey_enabled,health_mode,hostile_count;
 float position[3];PlayerHostile hostiles[32];uint32_t invincible,memory_queries,memory_cache_hits,radar_scanned;
 uint32_t map_block_id,map_valid;float map_local_position[3];
};
extern "C" int player_status_init();
// Call only from the already verified game-thread CameraStep listener107.
// Snapshot remains available during death; no native functions are called here.
extern "C" void player_status_tick();
extern "C" int player_status_snapshot(PlayerStatus* out);
