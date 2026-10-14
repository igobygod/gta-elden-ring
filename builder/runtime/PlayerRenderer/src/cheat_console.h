#pragma once
#include <stdint.h>
struct CheatConsoleSnapshot {uint64_t time,result_time;uint32_t active,blocking,pending;char input[96],result[160];};
struct CheatCommand {uint32_t kind;int32_t value;}; // 1=maxhealth (0 restore),2=invincible,3=maxammo,4=noreload,5=Buffalo test spawn,6=car removal
extern "C" void cheat_console_tick(int allowed); // Present: normal gameplay, living player, foreground checked internally
extern "C" int cheat_console_snapshot(CheatConsoleSnapshot* out);
extern "C" int cheat_console_active();
extern "C" int cheat_console_blocking();
extern "C" int cheat_console_take(CheatCommand* out); // verified game thread only
extern "C" void cheat_console_result(const char* result);
