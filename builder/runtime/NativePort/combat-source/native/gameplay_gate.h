#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Call once after the game's singleton/reflection registry is initialized (first valid player is sufficient).
// Initialization scans only once. Failure is closed and permanent for this process.
int gameplay_gate_init(void);
int gameplay_hud_normal(void);
// -1 unavailable; 0 HideAll, 1 ShowAll/menu, 2 Popup, 3 default.
int gameplay_hud_state(void);
uintptr_t gameplay_frontend_instance(void);
// Specific initialization diagnostics, so a closed gate is distinguishable from a menu.
enum GameplayGateStatus {
 GAMEPLAY_GATE_UNINITIALIZED=0, GAMEPLAY_GATE_RESOLVED=1,
 GAMEPLAY_GATE_BAD_SECTIONS=-1, GAMEPLAY_GATE_BAD_GETTER=-2,
 GAMEPLAY_GATE_NO_CANDIDATES=-3, GAMEPLAY_GATE_NAMES_UNAVAILABLE=-4,
 GAMEPLAY_GATE_NAME_NOT_FOUND=-5, GAMEPLAY_GATE_AMBIGUOUS=-6
};
int gameplay_gate_status(void);
#ifdef __cplusplus
}
#endif
