# What setup downloads, runs and changes

Applies to **0.2.7-windows-preview**. Implementation details are recorded in `installer/Engine.cs`, `installer/tools.json`, `builder/bootstrap.py`, `builder/setup.py` and the dependency lock files.

## Before installation

Setup checks for 64-bit Windows, closed Elden Ring, the expected game archive files, and the exact supported Elden Ring executable hash. It rejects a mod destination overlapping a game directory, a linked destination path or an unrelated nonempty directory. First setup requires at least 35 GB free. Selecting a folder does not grant compatibility to another game version.

The default destination is `%LocalAppData%\igobygod\GTA-EldenRing`; the player may choose another new or empty folder. Steam library detection is provided, with Browse as a fallback. .NET Framework 4.8 must be available to start the setup application; it is not installed by this EXE.

## Downloads and purpose

| Component | Version/pin | Why it is used | Where it goes |
| --- | --- | --- | --- |
| Microsoft Visual Studio 2022 Build Tools | Pinned bootstrap EXE; C++ workload, Clang and recommended components | Compile native runtime code against Windows libraries | System-managed Microsoft installation, if suitable tools are absent |
| Microsoft Visual C++ x64 Redistributable | Pinned installer EXE | Runtime libraries required by the built mod | System-managed Microsoft installation |
| Python | 3.13.12, official NuGet distribution | Run setup/conversion scripts and create the local Python environment | `RuntimeTools/python` |
| MinGit | 2.49.0 | Fetch pinned upstream source revisions | `RuntimeTools/git` |
| .NET SDK | 8.0.425 | Build C# asset/archive helper programs | `RuntimeTools/dotnet` |
| Eclipse Temurin Java JRE | 21.0.12.1+1 | Run the JPEXS Flash/HUD extractor | `RuntimeTools/java` |
| rustup / Rust | rustup 1.28.2; Rust 1.99.0 minimal toolchain | Build the runtime and Oodle helper | Private Cargo/Rust directories in `RuntimeTools` |
| CodeWalker | Exact commit in `builder/dependencies.lock.json` | Read GTA Legacy archives/models/animations | `BuildTools/CodeWalker-src` |
| SoulsFormats | Exact commit in the same lock file | Read Elden Ring formats | `BuildTools/SoulsFormats` |
| UXM Selective Unpack | Exact commit in the same lock file | Elden Ring archive support | `BuildTools/UXM` |
| Paramdex | Exact commit in the same lock file | Parameter definitions | `BuildTools/Paramdex` |
| JPEXS FFDec | 26.3.0 | Extract/rebuild HUD resources locally | `BuildTools/ffdec` |
| ME3 | 0.13.0 Windows amd64 | Launch the offline mod profile with its separate save | Local tools and generated mod installation |
| Python packages | Exact direct versions in `builder/requirements-build.txt` | Soulstruct/Havok, NumPy, SciPy, Pillow and supporting conversion libraries | Private Python environment in `BuildTools/python` |
| NuGet / Cargo packages | Project files and Cargo lock files | Dependencies for locally compiled helper programs/runtime | Tool-managed package/build caches |

Exact download URLs and SHA-256/SHA-512 checksums for setup tools are in [tools.json](../installer/tools.json). Builder archive hashes and source commits are in [dependencies.lock.json](../builder/dependencies.lock.json). Direct Python requirements are in [requirements-build.txt](../builder/requirements-build.txt).

**Pinning has limits:** the Visual Studio bootstrap downloads the components offered by Microsoft when it runs; the complete workload payload is not independently frozen by this project. Package managers resolve transitive dependencies according to their metadata/lock files; not every downloaded package has a project-maintained hash allowlist. Upstream availability and changes can still cause setup failures.

Network requests go to the listed upstream repositories/distributors and package infrastructure: GitHub and its asset hosts, Microsoft/NuGet, Rust distribution/crate services, and Python package services. These services receive ordinary download request information such as IP address. No GTA or Elden Ring assets are downloaded by the project. The builder uses the selected game's own Oodle library for decompression.

## Permissions and system changes

- The setup UI normally runs as the current user.
- Microsoft Build Tools and Visual C++ Redistributable setup are launched with administrator approval. They can install system components, maintain their own caches/settings and require a restart. They remain installed after the mod folder is removed.
- The application's portable Python/Git/.NET/Java/Rust environments live under the chosen mod destination. Their paths are supplied to child processes; setup does not persistently add those portable directories to the user's global PATH. Rust is invoked with `--no-modify-path`.
- The project opts out of .NET CLI telemetry for its build processes. It does not claim to override every upstream tool's telemetry/privacy settings.
- NuGet/.NET and vendor tools may create normal per-user/system caches outside the chosen folder. Not every temporary/cache file is confined to it.
- Setup creates a **Play igobygod Mod** desktop shortcut. It does not register a Windows service, scheduled task, automatic updater or startup launcher.
- No project Windows uninstaller entry is created; removal is described below.
- Archive extraction uses extended Windows paths so the .NET SDK can unpack even with the system long-path policy disabled. Setup does not change the Windows `LongPathsEnabled` registry setting.

## Files created in the chosen folder

| Path | Contents |
| --- | --- |
| `.igobygod-setup` | Ownership marker for safe retries |
| `RuntimeTools/` | Portable runtimes, compilers and Rust/Cargo state |
| `BuildTools/` | Downloaded source, converters, environment and build products |
| `Downloads/` | Verified setup download cache |
| `Builder-…/`, `source.zip`, `tools.json` | Extracted embedded builder source and setup tool metadata |
| `Mod/` | Generated game-derived assets, local runtime DLL, launcher scripts and isolated profile |
| `igobygod.exe`, `windows_worker.py`, `launch.json` | Installed GUI launcher and launch configuration |
| `setup.log`, `launch.log` | Local installation/launch diagnostics |

Conversion may also create temporary or retained build work inside the destination. Keep the complete destination after installation; deleting its tools can break launching or repair.

## Game files, saves and local data

Setup reads the player's installed game files and builds the mod separately. It is designed to leave the original game files and normal saves unchanged. The launcher verifies the installed payload, uses ME3 in offline mode and selects a separate save named `igobygod.sl2` in the game's save area. Normal save progress is not automatically copied. Keep Steam running and signed in to play.

The GUI is not a Windows network firewall. The offline profile is not a blanket guarantee about network behavior of Steam, Windows or third-party software. Do not use this preview for online play or mix it with an online launcher.

The project has no automatic support-log upload or account/login collection. Local logs, `launch.json`, `install.local.json` and `tools.local.json` can contain usernames, absolute paths and installation details. Inspect and redact them before sharing. Do not post game archives, generated models/textures/audio, generated headers, the asset-bearing mod DLL, saves, or a completed installation.

## Retry, updates and removal

Setup keeps completed downloads for retries. “Stop after this step” waits for the running step; it does not immediately terminate a compiler or Microsoft installer. A completed installation can have its shortcut/launcher repaired. That behavior is **not an upgrade mechanism**: this EXE embeds a fixed source snapshot and does not fetch newer mod releases.

To remove the mod, close Elden Ring, back up any mod save you want, and delete the chosen mod folder and desktop shortcut. The separate save can be kept or removed separately. Use Windows Installed Apps to manage Microsoft components; other applications may depend on them. Vendor/package caches may remain. The project's installer does not remove normal game saves.
