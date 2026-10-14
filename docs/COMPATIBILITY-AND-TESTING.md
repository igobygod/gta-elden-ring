# Compatibility and testing status

**Experimental preview, not release-ready.** The preview is enabled for testing but has not passed a native Windows end-to-end installation or gameplay session.

## Current compatibility boundary

- Elden Ring file version 1.17.1.0 with the single SHA-256 in `builder/release.json`. The same hash must pass setup, launch and runtime checks. A matching displayed version alone is insufficient.
- Availability of that exact executable on normal retail/Steam installations is unverified. This must be established before promoting a general release.
- GTA V Legacy is the asset source. Enhanced is unsupported. Changed Legacy archives can fail conversion or expected output checks; no all-Legacy-version claim is made.
- Windows x64 10/11 and .NET Framework 4.8 are the intended GUI environment. Windows ARM, Linux/Proton and consoles are unsupported by this installer.
- The earlier macOS builder used CrossOver/D3DMetal. That does not establish generic Wine/DXVK support or Windows compatibility.
- Offline play, separate mod save, keyboard/mouse custom controls, English-targeted UI/death assets. Other languages/controller mappings and other mods are not validated.
- The Buffalo feature is a prototype. Full-world radar/map coverage is not claimed; the current data targets Limgrave/overworld. Native two-hand-left weapon grip is unsupported.
- No automatic updates or dynamic game-version adapters. Removing a hash check does not adapt game offsets, hooks, parameters or conversion recipes.

## Recorded checks before this GitHub preview

| Check | Result and scope |
| --- | --- |
| Windows setup compilation | Passed, zero warnings/errors; x64 .NET Framework 4.8 GUI |
| Installer core checks | Passed: path quoting/containment, empty folders, retry markers, archive extraction, child process arguments and failure propagation |
| Hidden worker wrapper | Argument/exit propagation test passed |
| Builder unit checks | 24 passed |
| Embedded resources | Verified byte-for-byte against the release inputs |
| Source/privacy packaging audit | Passed for the packaged inputs; no game assets included |
| Earlier macOS clean asset build | Completed; 35 installed files passed integrity verification |
| Earlier macOS launcher smoke | Runtime/assets loaded and graphics initialized; no interactive gameplay verification |
| Native Windows clean installation | **Not performed** |
| Native Windows gameplay | **Not performed** |
| Digital signing | **Not signed** |

The macOS results are prior recorded results, not tests rerun during repository publication. They cannot certify Windows behavior. Machine-readable setup status is in [preview/VALIDATION.json](../preview/VALIDATION.json). The embedded builder retains its original validation records.

## Windows acceptance checklist — all pending

1. Confirm normal Steam/retail executable compatibility and record edition/version without distributing the executable.
2. Test a fresh Windows 10/11 x64 setup with no developer tools installed: folder detection/Browse, paths containing spaces, download interruption/retry, denied admin permission and restart handling.
3. Verify Microsoft/portable tool setup, local asset conversion and final mod generation complete; inspect logs for failures.
4. Verify the desktop shortcut, normal Steam launch requirements, offline profile and separate save. Confirm normal game files/save are unchanged.
5. Exercise movement, roll/jump/lock-on, aiming, every weapon class, reload, weapon wheel, cheat console, HUD/WASTED, sword and Buffalo spawn/clear.
6. Test quit/relaunch, failed install recovery, removal, and that missing/modified generated files fail safely.
7. Record GPU/driver, Windows edition/build, game versions, mod commit and findings. Redact personal paths before posting.

Until completed, do not change Windows verification flags to true or present the installer as broadly compatible. Future version support requires analysis, adaptation and this testing process for each supported configuration.
