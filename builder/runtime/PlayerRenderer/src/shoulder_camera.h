#pragma once
#include <stdint.h>
extern "C" void shoulder_camera_start();
extern "C" void shoulder_camera_heartbeat(uintptr_t owner,uint64_t now);
// Independent mouse aiming while the native Q camera/body target remain locked.
extern "C" void shoulder_camera_mouse_motion(int dx,int dy);
extern "C" int shoulder_camera_mouse_ready(); // capture/filter XY only while Q gun aim owns it
extern "C" float shoulder_camera_scope();

extern "C" int shoulder_camera_ground(uintptr_t owner,float* point,float* height);

extern "C" void shoulder_camera_input_blocked(int blocked);
extern "C" int shoulder_camera_scroll(int delta); // Win32 wheel units; returns consumed.
extern "C" float shoulder_camera_zoom(); // Relative to 42 degree aimed FOV.
extern "C" int shoulder_camera_scroll_ready(); // Peek filtering without accumulating wheel.

extern "C" void shoulder_camera_blast(uintptr_t owner,float strength,float duration); // angular radians, max .03; no Q/action shake.

struct ShoulderAimSnapshot {
 uintptr_t owner;uint64_t time,sequence;
 uint32_t ready,locked,free_aim,scoped;
 float origin[3],direction[3],ndc[2],view[16],fov,aspect;
};
// Includes the final camera origin after horizontal Q centering and cover sweep.
// Camera task107 is the only producer. Rifle task117 claims its newest sequence
// once on the SAME game thread and validates the current camera matrix. Call
// every rifle tick, not only on firing. Repeated/old/mismatched claims fail.
extern "C" int shoulder_camera_aim_for_tick(uintptr_t owner,uint64_t now,const float*current_view,ShoulderAimSnapshot*out);
// Nonconsuming copy for renderer UI and pose. Never publishes a Present ray.
extern "C" int shoulder_camera_aim_snapshot(uintptr_t owner,uint64_t now,ShoulderAimSnapshot*out);
