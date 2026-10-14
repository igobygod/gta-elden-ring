#pragma once
#include <stdint.h>
namespace native_ui {
struct State {bool item_acquisition_valid=false,item_acquisition=false,item_log_valid=false,item_log=false;bool boss_valid=false;uint8_t boss_visible_mask=0;uint64_t visible_mask0=0;uint8_t visible_mask1=0;bool popup_valid=false,popup_job_active=false;bool ui_valid=false,loading=false,subtitles_valid=false,subtitles=false,menu_fade_valid=false,menu_fade=false;bool valid=false,prompt_selected=false,can_execute=false,grayed=false,menu_blocking=false;int32_t text_id=-1;};
// Exact supported-build signature validation; read-only, no action input/memory writes.
bool init();
State snapshot();
}
