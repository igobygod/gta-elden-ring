# Contributing to the experimental preview

The repository is private while the first Windows configuration is being validated. Source availability and an MIT license for original code do not mean universal game compatibility. Third-party terms remain in effect.

For a problem report, provide the mod preview version/commit, Windows version, GPU, GTA edition/build, Elden Ring executable version, the step that failed, and a short redacted log excerpt. Do not attach full logs without reviewing them for personal paths. Never upload game files, generated assets/headers, the asset-bearing runtime DLL or save files.

For another game version, begin with a compatibility report. Do not delete executable/payload checks to make setup continue. Runtime hooks/offsets, parameters, archive formats and expected conversion output may need changes. Add version-specific support only with documented tests.

Keep authored source under `builder/` and the GUI under `installer/`. Refresh the embedded payload after builder changes and run the relevant checks described in [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md). Preserve upstream notices. Do not commit tools, caches, credentials or local installation reports.

Before a broader public release, review dependency licensing/redistribution, complete native Windows testing, regenerate release checksums and update the compatibility table honestly. Do not turn the repository public without the owner's explicit decision.
