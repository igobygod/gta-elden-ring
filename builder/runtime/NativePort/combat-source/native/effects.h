#pragma once
#include <stdint.h>
struct FxSprite {uint32_t kind,seed;float x,y,x2,y2,scale,age;};
struct FxMesh {float position[3],rotation[4];};
struct FxFrame {uint64_t time;uint32_t count,mesh_count;float camera[16],fov,aspect;FxSprite sprites[64];FxMesh meshes[8];};
static_assert(sizeof(FxSprite)==32&&sizeof(FxMesh)==28&&sizeof(FxFrame)==2360,"FX snapshot ABI");
extern "C" int native_fx_frame(FxFrame* out);
extern "C" void native_sound_play(int kind);
extern "C" int native_sound_init();
extern "C" void native_sound_sample(int kind,const unsigned char* pcm,uint32_t size);
