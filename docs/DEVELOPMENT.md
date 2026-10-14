# Maintainer instructions

Players use the setup EXE. The commands here are only for developers working on the source. Do not build game-derived assets inside this checkout or commit a completed installation.

## Source layout and snapshot

`builder/` is a readable, byte-for-byte extraction of `installer/payload.zip`. `installer/` contains the GUI release source and source manifest. The committed preview binary is rebuilt for 0.2.1 with the Windows long-path extraction fix. The two copies are intentional: one is browsable on GitHub, and one is the exact embedded setup payload.

From the repository root, Python 3 checks the source manifests and payload consistency:

```sh
python scripts/payload.py
```

After intentional changes to `builder/` or `installer/`, regenerate the manifests and payload, then review the diff:

```sh
python scripts/payload.py --refresh
python scripts/payload.py
```

The refresh script copies `builder/release.json` and `builder/windows_worker.py` into the installer resources. It refuses common game/binary/cache types in the builder source. This check does not replace a full privacy, game-data and license review. Update `runtime/PUBLIC_FILES.json` and related internal runtime manifests if their source inventory changes, following their existing format.

Refreshing the payload does **not** update `preview/igobygod Setup.exe`. Rebuild, test, and deliberately replace release artifacts and checksums for each new preview version. Keep historical validation claims separate from new results.

## Build the Windows GUI

With the .NET 8 SDK installed:

```sh
dotnet build installer/Setup.csproj -c Release
```

The project targets Windows x64 .NET Framework 4.8 and restores Microsoft's framework reference assemblies from NuGet. Output is `installer/bin/Release/net48/igobygod Setup.exe`. The source ZIP is embedded at build time. The GUI build itself does not need either game's assets. Running the GUI requires Windows and .NET Framework 4.8. A successful cross-build on macOS does not prove Windows execution or gameplay.

## Relevant checks

```sh
dotnet run --project installer/tests/Tests.csproj
python installer/tests/test_worker.py
python -m unittest discover -s tests
python builder/runtime/tests/run_policies.py
```

For the Windows .NET Framework path regression, build and run the dedicated test executable in Windows without changing the system long-path policy:

```sh
dotnet build installer/tests/windows/WindowsPaths.csproj -c Release
installer/tests/windows/bin/Release/net48/WindowsPaths.exe
```

An optional `--sdk <downloaded-dotnet.zip>` argument exercises the full SDK archive at a deliberately long temporary destination and removes that fixture afterwards. Do not point test outputs at a game installation or existing mod installation.

The core process tests require `dotnet` on PATH or `DOTNET_HOST_PATH` pointing to it. Runtime policy tests require a C++ compiler. Run only the checks relevant to the change, then complete native Windows acceptance testing before claiming Windows readiness. The existing builder README contains advanced conversion prerequisites and platform-specific commands; the Windows GUI automates those for players.

## Release hygiene

- Confirm `release.json` states the exact supported executable and truthful verification flags.
- Verify no local game assets, generated headers, compiled game-specific DLL, saves, log files, credentials or personal paths entered the repository.
- Retain all third-party notices. Review the CodeWalker educational source notice before any broader public release.
- Include the unsigned/code-signing status, installation disclosures, required games and native Windows testing status in release notes.
- Generate checksums after building the final artifacts. Do not describe a binary as matching changed source unless its embedded resources have been verified.
- Use an experimental/pre-release designation until the documented acceptance criteria pass. Keep repository visibility private unless the owner explicitly requests otherwise.
