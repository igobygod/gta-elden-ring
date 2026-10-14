# igobygod — GTA × Elden Ring

**Experimental GitHub preview · 0.2.0-windows-preview · one exact Elden Ring executable · Windows testing pending**

This project brings Franklin, GTA-style weapons, a weapon wheel, cheat console, HUD, sounds, sword support and a Buffalo vehicle prototype into Elden Ring. The Windows installer rebuilds the required assets from the player's own installed games. No game assets are included in this repository or in the setup executable.

**This is a private development preview, not a broadly tested release.** The setup executable compiles and its core automated checks pass. A complete native Windows installation and gameplay test have **not** been completed. Some included features are prototypes; inclusion in the source does not mean verified Windows gameplay.

## Start here

The Windows preview is in [preview/](preview/). Open [igobygod Setup.exe](preview/igobygod%20Setup.exe), check the detected game folders or choose them with Browse, choose a new or empty mod folder, select the automatic-preparation checkbox, then click **Install mod**. Follow any Microsoft permission/restart prompts. After successful installation, use **Play mod** or the desktop shortcut.

Players do not need to type commands. The first installation downloads and runs supporting tools and can take considerable time. Read [what setup installs and changes](docs/INSTALLATION-TRANSPARENCY.md) before testing. The executable is **unsigned**.

## Requirements and compatibility

| Requirement | Current scope |
| --- | --- |
| Windows | 64-bit Windows 10 or 11, with .NET Framework 4.8 already available |
| Elden Ring | Executable file version **1.17.1.0**, matching the exact SHA-256 below |
| GTA V | **Legacy** installed locally; Enhanced is unsupported |
| Space | At least **35 GB free** for first setup; actual space needs can vary |
| Internet | Required for tool downloads and dependency restoration; several GB on first setup |
| Permissions | Microsoft C++ tools/runtime installation may require administrator approval and a restart |
| Play mode | Offline mod profile with a separate save; normal Steam/game requirements still apply |
| Controls | Custom features require keyboard and mouse |

Accepted Elden Ring executable SHA-256:

```text
1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891
```

**The same displayed version number is not enough.** Availability of this exact executable across ordinary Steam/retail installations is unverified. Setup refuses another executable before installing tools. GTA Legacy archives and generated outputs are also checked; compatibility with every Legacy update is not established. Do not bypass these checks to force an unsupported version to run.

This is not an all-version installer, GTA Enhanced converter, online multiplayer mod, automatic updater, or general mod manager. Compatibility with other mods is untested. See [known limitations and validation](docs/COMPATIBILITY-AND-TESTING.md).

## Included behavior

- Franklin rendering, character animation and movement integration.
- GTA-style guns, aiming/firing/reloading, native damage integration and effects.
- Weapon wheel and cheat console.
- GTA-style HUD/radar and WASTED presentation/audio.
- Sword/melee support and a Buffalo vehicle/driver prototype.
- Local conversion of models, textures, animations, audio, HUD and map data from installed games.
- Separate mod installation, offline launcher profile and separate save.

The radar/map data targets the current Limgrave/overworld implementation; full-world coverage is not claimed. English UI/death-message assets are targeted. Native two-hand-left grip is unsupported. The vehicle feature remains a prototype.

| Action | Key |
| --- | --- |
| Weapon wheel | Hold Tab |
| Aim / fire | Right / left mouse button |
| Reload | R |
| Cheat console | Backtick/tilde |
| Submit / close console | Enter / Esc |

Cheats include `MAXHEALTH`, `INVINCIBLE`, `MAXAMMO`, `NORELOAD`, `BUFFALO` and `CARCLEAR`. Roll, jump and lock-on keep the player's Elden Ring bindings.

## What is in this repository?

| Folder | Contents |
| --- | --- |
| [installer/](installer/) | C# Windows setup source, pinned tool downloads, tests and the exact embedded source payload |
| [builder/](builder/) | Readable copy of that payload: conversion pipeline, runtime source, dependency pins and tests |
| [preview/](preview/) | Original unsigned setup EXE, user guide, checksums and recorded validation status |
| [docs/](docs/) | Installation transparency, compatibility/testing, maintainer instructions and release notes |
| [scripts/](scripts/) | Repository packaging and source/payload consistency checks |

The checked-in `installer/payload.zip` is authored source, not a completed mod installation. Its extracted files match `builder/`. The preview EXE embeds that source and the setup UI; it does not embed game assets or the asset-bearing mod DLL. A developer rebuilding from changed source must refresh the payload first: see [maintainer instructions](docs/DEVELOPMENT.md).

## Privacy, licenses and credits

Game assets are rebuilt locally and must not be uploaded with generated mod installations, logs, saves or private paths. Setup has no project-operated account service or automatic log uploader; it contacts upstream download/package services. Microsoft and other upstream tools have their own terms and behavior. See the [full installation disclosure](docs/INSTALLATION-TRANSPARENCY.md).

Original project code is offered under [LICENSE.txt](LICENSE.txt). Third-party code retains its own terms; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the preserved notices under `builder/runtime/`. This does not license GTA/Elden Ring assets or trademarks. The CodeWalker dependency has an educational-use source notice; a broader public release still requires a dependency/rights review.

Created for **igobygod** with AI-assisted development using **OpenAI Codex**. This is an unofficial fan project, not affiliated with or endorsed by the game publishers or developers. Keeping the repository private does not change third-party terms.

## Next milestones

1. Validate the accepted executable against a normal Steam installation on Windows.
2. Complete a clean Windows install, launch and gameplay checklist.
3. Fix findings and document the first tested configuration.
4. Add other game versions only after adaptation and testing; consider a version-aware installer and updater afterwards.

See [CONTRIBUTING.md](CONTRIBUTING.md) for reporting and contribution rules.
