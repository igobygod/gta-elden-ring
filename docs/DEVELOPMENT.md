# Development

`builder/` contains conversion recipes and runtime source. `installer/payload.zip` embeds the same files in the Windows setup executable.

## Build setup

With Python 3 and the .NET 8 SDK installed, run from the repository root:

```sh
python scripts/payload.py --refresh
python scripts/payload.py
dotnet build installer/Setup.csproj -c Release
```

Output: `installer/bin/Release/net48/igobygod Setup.exe`. The GUI targets Windows x64 and .NET Framework 4.8. Building the setup executable does not require game assets. Building the game-specific runtime does; see [builder/README.md](../builder/README.md).

## Tests

```sh
dotnet run --project installer/tests/Tests.csproj
python installer/tests/test_worker.py
python -m unittest discover -s tests
python builder/runtime/tests/run_policies.py
```

The process tests need `dotnet` on PATH or `DOTNET_HOST_PATH` set to its location. Runtime policy tests need a C++ compiler.

Windows path tests run under .NET Framework:

```sh
dotnet build installer/tests/windows/WindowsPaths.csproj -c Release
installer/tests/windows/bin/Release/net48/WindowsPaths.exe
```

`--sdk <archive.zip>` also tests full SDK extraction at a long temporary path.

## Packaging

Refresh the embedded payload before building. Copy the resulting EXE to `preview/`, update its guide and checksums, and verify the embedded resources against `installer/`. `SOURCE_MANIFEST.json` records each source file's size and SHA-256.

Local conversion outputs belong outside the checkout. Completed installations, generated headers, game-specific runtime DLLs, tools, logs, saves and credentials are excluded from source packages. The original-code license and third-party notices accompany the source.
