# igobygod — GTA × Elden Ring

Franklin, GTA weapons, a weapon wheel, cheat console, HUD, sounds and a Buffalo vehicle prototype in Elden Ring.

**Windows preview · 0.2.7**

Setup builds the mod from your installed GTA V Legacy and Elden Ring files. The repository and installer contain source code, with no game assets bundled.

## Install

1. Download [igobygod Setup.exe](preview/igobygod%20Setup.exe).
2. Select your GTA V Legacy folder and Elden Ring Game folder.
3. Choose a new or empty folder for the mod.
4. Enable automatic tool setup and click **Install mod**.
5. When installation finishes, use **Play mod** or **Play igobygod Mod** on your desktop.

No terminal commands are needed. First setup downloads several GB of tools and needs at least 35 GB of free working space. Microsoft components may request administrator permission or a restart. The installer is unsigned.

[Downloads and system changes](docs/INSTALLATION-TRANSPARENCY.md) · [Compatibility and testing](docs/COMPATIBILITY-AND-TESTING.md) · [Release notes](docs/RELEASE-NOTES.md)

## Requirements

| Component | Requirement |
| --- | --- |
| Windows | Windows 10/11 x64 and .NET Framework 4.8 |
| Graphics | DirectX 12, feature level 12.0 or higher; Elden Ring must run normally first |
| GTA V | Legacy; Enhanced is unsupported |
| Elden Ring | Exact executable SHA-256 below |
| Storage | 35 GB free for setup; final mod is approximately 1.3 GB |
| Internet | Required during setup |
| Controls | Keyboard and mouse for custom features |

Supported Elden Ring executable SHA-256:

```text
1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891
```

The original executable was recorded as version 1.17.1.0. The hash is the compatibility check; displayed version numbers alone are insufficient. Other executables and GTA Legacy updates may need additional support. Compatibility across ordinary Steam/retail installations has not been established.

The mod uses offline mode and a separate save, `igobygod.sl2`. Normal game and launcher requirements still apply. Compatibility with other mods is untested.

## Features and controls

- Franklin character, movement and animations.
- Guns, aiming, firing, reloads, weapon wheel and effects.
- GTA-style HUD, radar, audio and WASTED presentation.
- Sword support and a Buffalo vehicle/driver prototype.

| Action | Control |
| --- | --- |
| Weapon wheel | Hold Tab |
| Aim / fire | Right / left mouse button |
| Reload | R |
| Cheat console | Backtick/tilde |
| Submit / close console | Enter / Esc |

Cheats: `MAXHEALTH`, `INVINCIBLE`, `MAXAMMO`, `NORELOAD`, `BUFFALO`, `CARCLEAR`. Roll, jump and lock-on use your Elden Ring bindings.

Map coverage targets Limgrave/overworld. UI and death-message changes target English. Native two-hand-left grip is unsupported. The vehicle remains a prototype.

## Test status

The complete installer ran successfully in a Windows 11 ARM64 VM using x64 tools. All 35 installed files passed an integrity check. Asset comparisons against the reference build passed, including all 194 decrypted gameplay-parameter entries. Known animation and vehicle fingerprint differences are numerically equivalent.

Gameplay on a Windows x64 PC with a supported GPU is still untested. The Parallels VM used for setup testing lacks graphics feature level 12.0, so it cannot serve as the gameplay test machine.

## Source

| Folder | Contents |
| --- | --- |
| [installer/](installer/) | Windows setup source and embedded builder payload |
| [builder/](builder/) | Asset conversion and runtime source |
| [preview/](preview/) | Installer, quick-start guide and checksums |
| [docs/](docs/) | Setup details, compatibility, development and release notes |

[Build from source](docs/DEVELOPMENT.md) · [Contributing](CONTRIBUTING.md)

## Credits and license

Created by **igobygod**. Original project code is available under [LICENSE.txt](LICENSE.txt). Third-party code keeps its own license; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). CodeWalker carries an educational-use source notice.

This is an unofficial fan project. GTA and Elden Ring assets and trademarks belong to their respective owners. Completed local installations contain game-derived material and are excluded from the source distribution.
