# igobygod runtime source

This source preserves the current Franklin, weapon wheel, cheat console, firing,
movement, UI compositing, and vehicle prototype logic. It contains no game meshes,
textures, audio, or sampled animation/effect payloads. Nine generated header
wrappers require local game-data preparation before compilation.

Build from `PlayerRenderer` with `IGOBYGOD_GENERATED_ROOT` set to the generated
workspace containing the nine paths in `generated-headers.json`:

- Windows x64 Developer Command Prompt: `cargo build --release --locked --target x86_64-pc-windows-msvc -j1`
- macOS with the documented cross tools: `cargo xwin build --release --locked --target x86_64-pc-windows-msvc -j1`

The DLL requires the Microsoft Visual C++ x64 runtime (`MSVCP140`,
`VCRUNTIME140`, and `VCRUNTIME140_1`) and the Windows Universal C Runtime.
CrossOver must provide these in the selected bottle as well.

Install the resulting DLL at `Package/runtime/franklin_player_renderer.dll` and
locally generated assets beneath `Package/runtime/data`. Audio belongs in
`Package/runtime/data/audio`. Logs are created in `Package/runtime/logs`.
Runtime paths are relative to the DLL; the process working directory and account
name do not matter. `runtime-data-map.json` maps the old source paths to paths
relative to that DLL folder. Its log entries are created at runtime.

The generated headers are compiled into the local DLL. Consequently the DLL,
generated headers, and generated data are private build outputs: distribute only
the authored source allowlisted in `PUBLIC_FILES.json`, never a built validation
DLL or the `.private-validation` directory. The hand grip in `sword_attack.h` is
also generated locally from the selected source animation. The fingerprints in
`expected_params.h` come from the locally generated regulation file, keeping the
runtime's parameter checks synchronized with that installation.

The native executable SHA-256 gate is retained and checks the actual running
process with a streaming hash. Unsupported executables are rejected before hooks
start. Do not bypass this check: game offsets and signatures are version-specific.

Validation: the portable runtime cross-compiles to the x64 Windows MSVC target;
25 standalone policy test programs pass on macOS. This does not establish a clean
asset rebuild or gameplay compatibility. The native Windows swapchain/queue hook
is a compiled, statically reviewed candidate and still needs Windows gameplay
validation. Wine keeps the existing D3DMetal path; this is not a generic Wine or
DXVK compatibility claim. Native Windows captures direct D3D12 queues and their
swapchain associations. A late attachment draws only when there is one matching
direct queue; an ambiguous queue selection leaves the compositor inactive.

The terrain-map source retains small coordinate-mapping constants and native
field offsets needed to place the generated map texture. These are
interoperability parameters; the map image itself is prepared locally.

Run asset-free policies with `python tests/run_policies.py` using clang++ or g++.

Vendored memory helpers carry the MIT notice in
`Bridge/vendor/er-reference/LICENSE`. MinHook and its disassembler carry their
notices in `Source/vendor/hudhook/vendor/minhook/LICENSE.txt`. Those notices must
stay with redistributed source/binaries. New project code is credited to igobygod.
