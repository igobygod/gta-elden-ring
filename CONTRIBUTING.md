# Contributing

Bug reports should include the mod version, Windows version, GPU, game editions, the failed step and a short log excerpt with personal paths removed. Setup logs are in the installation folder; runtime logs are under `Mod/runtime/logs`.

Compatibility changes need tests for the affected executable, runtime hooks, parameter data and asset conversion. A matching version label alone does not establish compatibility.

Source layout, build commands and tests are documented in [Development](docs/DEVELOPMENT.md). Game files, generated assets, game-specific DLLs, saves, tool caches and credentials are excluded from this repository. Third-party license notices belong with their source.
