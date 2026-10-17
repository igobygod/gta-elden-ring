# Compatibility and testing

## Supported configuration

- Windows 10/11 x64, .NET Framework 4.8 and a DirectX 12 GPU with feature level 12.0 or higher.
- GTA V Legacy. Enhanced is unsupported.
- Elden Ring executable matching the SHA-256 in `builder/release.json`.
- Offline mode, separate mod save and keyboard/mouse controls.
- English UI/death-message assets and Limgrave/overworld map data.

Other executable hashes, other mods, controller mappings and all-version compatibility are untested. The Buffalo feature is a prototype; native two-hand-left grip is unsupported.

## Completed tests

| Check | Result |
| --- | --- |
| Full installation | Completed in Windows 11 LTSC ARM64 with x64 tools |
| Installed-file integrity | All 35 files match the installation receipt |
| Reference comparison | 29 of 35 files byte-identical |
| Melee animation | All 64 clips equivalent; timing and sword anchors exact |
| Roll/jump animation | All 62 clips checked; two position values differ by at most 1.46e-11 metres |
| Vehicle | Geometry, materials and topology match; 116 signed-zero differences |
| Gameplay parameters | All 194 decrypted archive entries match |
| Original mod assets | 23 checked files match the reference |
| Runtime startup | Character, weapons, animation and vehicle packets load; hooks initialize |
| File corruption | Altered payloads rejected |
| Long Windows paths | .NET SDK extraction passes with the system long-path setting disabled |
| Gameplay on supported Windows hardware | Untested |
| Code signing | Unsigned |

The rebuilt Windows DLL differs from the CrossOver-built reference. Windows rendering and gameplay need testing on a supported GPU. The test VM's Parallels adapter rejects DirectX feature level 12.0; the base game also crashes there without the mod.

The earlier macOS build passed installation and launch checks through CrossOver/D3DMetal. Those results do not establish Windows gameplay compatibility.

## Known installation limits

First setup requires internet access, several GB of downloads and at least 35 GB free. Microsoft tools may require administrator permission and a restart. Tools remain available in the installation directory for later launches.

Changing the game or mod paths requires a new installation with the current launcher. There is no automatic updater or Windows uninstall entry. Version checks reject unsupported executables before conversion.
