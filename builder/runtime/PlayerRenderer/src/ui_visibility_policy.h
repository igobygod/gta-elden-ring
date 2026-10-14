#pragma once
namespace ui_visibility {
// Rewards are world overlays. Capture availability must never control whether
// the player exists; real menus, titles and invalid HUD state remain hidden.
inline bool hide_world(int hud,bool menuBlocking,bool reward,bool) {
 return hud<1||hud>3||((hud!=3||menuBlocking)&&!reward);
}
}
