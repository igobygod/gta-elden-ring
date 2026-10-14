#pragma once
#include <d3d12.h>
#include <stdint.h>
// Read-only capture of the native Scaleform pass. Game render targets, movies,
// blend states and draw calls are never replaced or replayed.
int native_ui_capture_init(ID3D12Device*,ID3D12GraphicsCommandList*);
int native_ui_capture_snapshot(uint64_t now,UINT width,UINT height,DXGI_FORMAT,
                               ID3D12Resource**before,ID3D12Resource**after);
