#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

// ER WW 2.7.1.0 / App 1.17.1 only. Call on the existing ER game task,
// with the existing executable hash/vtable gates already satisfied.
// Primary ABI source: Dasaav-dsv/erfps2 src/player.rs head_matrix(),
// src/rva/ww.rs GET_DMY_POS_RVA. Local disassembly confirms an integer
// count return (not the upstream pointer annotation) and zero on failure.
namespace nativefx {
using Readable = bool (*)(const void*, size_t);
struct alignas(16) DummyMatrix { float rows[4][4]; };
constexpr uintptr_t kGetDummyMatrix = 0x3e96b0;
// DSAnimStudio NewChrAsm.cs GlobalDummyPolyIDOffset uses 10000 for
// RightWeapon0; local ER routing independently confirms group10/remainder181.
constexpr uint32_t kRightWeaponMuzzle = 10181;
inline bool dummy_signature_matches(uintptr_t base, Readable readable) {
    static const uint8_t expected[] = {
        0x4c,0x8b,0xdc,0x55,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x83,
        0xec,0x50,0x49,0xc7,0x43,0xa8,0xfe,0xff,0xff,0xff
    };
    const void* address = (const void*)(base + kGetDummyMatrix);
    return readable(address, sizeof(expected)) && !memcmp(address, expected, sizeof(expected));
}

// encoded_dummy_id is deliberately caller-supplied. Local code routes
// (id / 1000) % 100 to a model group and id % 1000 to the FLVER dummy.
// Raw 181 is a BODY dummy. Use kRightWeaponMuzzle for the converted rifle;
// this helper does not silently reinterpret 181 or use a chest fallback.
inline bool get_dummy_matrix(uintptr_t base, const void* player,
                             uint32_t encoded_dummy_id, const float* player_havok,
                             Readable readable, DummyMatrix& output, int* reason = nullptr) {
    // Diagnostic codes: 0=success, -1=player/signature, -2=lookup count,
    // -3=nonfinite matrix/player position, -4=axis length, -5=distance/w.
    auto fail = [reason](int code) { if (reason) *reason = code; return false; };
    if (reason) *reason = 0;
    if (!readable(player, 0x650) || !dummy_signature_matches(base, readable)) return fail(-1);
    DummyMatrix candidate;
    for (auto& row : candidate.rows) for (float& component : row) component = NAN;
    using GetDummy = int (__fastcall*)(const void*, DummyMatrix*, const uint32_t*, int);
    int count = ((GetDummy)(base + kGetDummyMatrix))(player, &candidate, &encoded_dummy_id, 1);
    if (count != 1) return fail(-2);
    for (const auto& row : candidate.rows)
        for (float component : row) if (!isfinite(component)) return fail(-3);
    float distance_squared = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (!isfinite(player_havok[i])) return fail(-3);
        float delta = candidate.rows[3][i] - player_havok[i];
        distance_squared += delta * delta;
        float axis_length_squared = 0;
        for (unsigned j = 0; j < 3; ++j) axis_length_squared += candidate.rows[i][j] * candidate.rows[i][j];
        if (axis_length_squared < 0.25f || axis_length_squared > 4.f) return fail(-4);
    }
    // Reject a missing/stale/misframed transform rather than render elsewhere.
    if (distance_squared > 16.f || fabsf(candidate.rows[3][3] - 1.f) > 0.01f) return fail(-5);
    output = candidate;
    return true;
}
}
