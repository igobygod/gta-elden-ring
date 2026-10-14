#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// All functions are thread-safe. Hook installation occurs once, outside DllMain.
// No input is removed until the caller reports verified normal gameplay.
int native_input_install(void);
void native_input_set_context(int owns_world,int wheel_open);
// ready means installed AND real mouse/keyboard reads observed; caller must gate ownership on ready.
struct NativeMouseInput {int32_t dx,dy,wheel;uint32_t buttons,state_calls,buffer_calls,ready;};
void native_input_take(struct NativeMouseInput* out);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C"
#endif
void native_input_set_console(int blocking);
