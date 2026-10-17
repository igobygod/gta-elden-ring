# igobygod — GTA × Elden Ring local builder

This source package rebuilds the Franklin mod from your own GTA V Legacy and Elden Ring installations. It contains installer scripts, conversion recipes and mod source code. It does not contain game models, textures, animations, sounds, fonts, maps, modified game archives, saved games, or a precompiled game-specific mod DLL.

**Windows preview.** Full installation completed in a Windows 11 ARM64 VM using x64 tools. All 35 installed files passed integrity checks; reference asset and parameter comparisons passed. Windows gameplay on a supported GPU remains untested. The VM lacks the graphics feature level required by the base game. See `VALIDATION.json` for test results.

## What it preserves

Franklin's character and movement, guns and native damage, the weapon wheel, cheat console, aiming and firing fixes, explosions, the GTA-style HUD/radar and WASTED presentation, sword support, and the existing Buffalo vehicle prototype. The map retains the original mod's Limgrave coverage; the death-message override is English only.

## Required installations

The Windows setup app prepares the developer tools below automatically. Windows viewers only need to supply their installed games and follow the setup window.

- Your own installed GTA V **Legacy** and Elden Ring. Enhanced GTA V is not supported by this converter.
- Elden Ring executable file version **1.17.1.0**, with SHA-256 `1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891`. The exact executable has been checked on the original setup. Availability of this same hash across retail/Steam installations has not been established; the setup checks yours before writing anything.
- Python **3.13**, Git, .NET SDK **8**, Rust with the `x86_64-pc-windows-msvc` target, Java **21**, and the Microsoft Visual C++ x64 runtime.
- Windows GUI: setup prepares the required tools. Developers building manually need the Visual Studio C++ build tools and an x64 developer shell.
- macOS: CrossOver with **D3DMetal enabled** and an existing bottle that already runs Elden Ring, the Visual C++ runtime in that bottle, and `cargo-xwin` with its required LLVM tools. The bottle must map `Z:` to the macOS root. The Wine renderer expects D3DMetal; DXVK is not supported. The original environment uses CrossOver 26.3 (26.3.0.39832). A working CrossOver game installation is a prerequisite; this package does not install either game or CrossOver.
- At least **16 GB of free build space**, plus space for the two games. Keep the download, tools and output in separate folders.

Official dependencies: [Python](https://www.python.org/downloads/), [.NET](https://dotnet.microsoft.com/download/dotnet/8.0), [Rust](https://rustup.rs/), [cargo-xwin](https://github.com/rust-cross/cargo-xwin), [Java](https://adoptium.net/), [Microsoft C++ tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/), and [VC++ runtime](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist).

## Select the games and check compatibility

```text
python3.13 setup.py --check
```

Select the GTA V Legacy folder containing `GTA5.exe`, then Elden Ring's folder containing `eldenring.exe` or its parent containing `Game`. If a graphical picker is unavailable, provide `--gta "..." --elden-ring "..."` on the command line.

## Build the local tools

Complete the compatibility check above first. On macOS, install the full LLVM tools and make `clang-cl` and `lld-link` available on PATH. Set up the Rust target and cross-builder with:

```text
rustup target add x86_64-pc-windows-msvc
cargo install --locked cargo-xwin --version 0.23.1
```

Windows needs the same Rust target, but does not use cargo-xwin.

Extract this ZIP first. Run commands from its extracted folder. Replace the example paths below with your own. On Windows use `py -3.13` where macOS examples use `python3.13`.

```text
python3.13 bootstrap.py --output "/path/to/igobygod-tools"
```

The bootstrap fetches exact pinned versions of CodeWalker, SoulsFormats, UXM, Paramdex, JPEXS FFDec and ME3 from their upstream projects. It verifies archive hashes, creates a private Python environment, and builds the small extraction helpers. It does not download any game assets. Git commits, release hashes and Python versions are listed in `dependencies.lock.json` and `requirements-build.txt`.

## Build on macOS with CrossOver

Save and close Elden Ring first. The output folder must not already exist.

```text
python3.13 setup.py --build --tool-root "/path/to/igobygod-tools" --output "/path/to/igobygod-mod" --bottle "/path/to/your/CrossOver/Bottles/GameBottle"
```

The two game-folder pickers appear automatically. Add `--java "/path/to/java"` if Java 21 is not on PATH. Add `--crossover "/path/to/CrossOver.app"` if it is installed outside `/Applications`. You can pass `--gta` and `--elden-ring` explicitly instead of using the pickers.

## Windows setup app

Open **igobygod Setup.exe**, select your GTA V Legacy and Elden Ring folders, choose an installation folder, and click **Install mod**. The setup app downloads and prepares the required tools, builds the mod locally, and creates a **Play igobygod Mod** desktop shortcut. No terminal commands are required. Microsoft C++ tools may show an administrator prompt; complete any required Microsoft prompts. Restart Windows and rerun setup if Microsoft requests a restart.

Use 64-bit Windows 10 or 11 with .NET Framework 4.8 (included in Windows 11 and current Windows 10 installations), internet access, and at least 35 GB free space. Keep the entire chosen setup folder after installation: the shortcut uses its local tools. Failed downloads can be retried by reopening setup and selecting the same folder. Existing unrelated folders are rejected.

This is a Windows preview. Its desktop executable has been cross-compiled and its core path/archive handling tested; a complete native Windows installation and gameplay test remain pending. The app is unsigned. Broad compatibility with retail game executable variants remains unverified. The exact supported executable check remains in effect.

Only distribute the original setup executable and its source archive. A completed local installation contains game-derived material and must not be uploaded.

## Launch, saves, and removal

After a successful local build:

```text
python3.13 setup.py --launch "/path/to/igobygod-mod"
```

Keep the tools folder after installation: the launcher uses its ME3 executable. Moving the tools, either game, or the selected CrossOver bottle requires rebuilding with the new paths.

The launcher verifies the executable and every installed payload hash before starting ME3. ME3's profile uses offline mode and a separate save named `igobygod.sl2`. The original game files and existing normal save are not edited by the installer. Start a new mod save; normal progress is not copied automatically. Steam and the game's normal installation requirements still apply. Keep Steam running and logged in; on macOS it must run in the selected CrossOver bottle.

The viewer's locally built folder contains game-derived material. **Do not upload or redistribute it, its generated headers, or its compiled DLL. Share only this source ZIP.**

To uninstall, close the game and remove the separate local mod output folder. Your normal game installation remains intact. The isolated `igobygod.sl2` save, if created by playing, lives in the game's save directory and can be kept or removed separately. Keep a backup before removing any save.

## Controls

The custom features currently require keyboard and mouse.

| Action | Control |
| --- | --- |
| Weapon wheel | Hold Tab, select, then release to equip |
| Aim | Right mouse button |
| Fire | Left mouse button |
| Reload | R |
| Cheat console | Backtick/tilde key (` / ~) |
| Submit cheat | Enter |
| Close console | Esc |

The console lists `MAXHEALTH`, `INVINCIBLE`, `MAXAMMO`, `NORELOAD`, `BUFFALO`, and `CARCLEAR`. These commands preserve the existing mod behavior; the Buffalo feature remains a prototype.

Roll, jump and lock-on retain your Elden Ring keybindings. The installer does not change them. Use a normal or right-hand weapon grip; native two-hand-left mode is unsupported.

## If setup stops

- Unsupported executable: this build cannot safely use that executable. Keep the version check intact.
- Missing tool: install the prerequisite, ensure it is on PATH, and run bootstrap again.
- Existing output or partial tools directory: choose a new empty folder; the builder avoids overwriting unrelated files.
- Missing source archive, failed conversion or hash mismatch: the installation is not finalized. Preserve the terminal error when reporting the issue.
- Low disk space: choose a new output folder on a drive with at least 16 GB free.
- Black screen at startup: check that the base game runs normally and the GPU supports DirectX 12 feature level 12.0. Parallels on Apple silicon is unsuitable for gameplay testing.

Runtime diagnostic logs are in the completed local mod's `runtime/logs` folder. The private `install.local.json` records selected paths and payload hashes; do not include it in public uploads.

## How it works

The installer first validates both game folders and the version-specific executable. It reads GTA assets through locally built CodeWalker tools, reads Elden Ring archives through locally built SoulsFormats/UXM helpers, and uses the selected game's own Oodle library for its compressed data. Conversion recipes rebuild the mesh, animation, texture, audio, HUD and map packets. Small game-specific headers and combat parameters are generated locally as well.

It then compiles the portable runtime against those generated headers. The runtime resolves its files relative to its DLL instead of depending on the author's drive paths. Only after every required output passes its checks does the installer move the complete build into the requested output folder.

Created by **igobygod**. See `THIRD_PARTY_NOTICES.md` and the retained vendored notices. This is an unofficial mod project; game names identify the required installations, and their assets remain with the original rights holders.
