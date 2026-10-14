# Local asset reconstruction

This pipeline is authored conversion code. Each viewer supplies GTA V Legacy and Elden Ring installations; extracted meshes, textures, animations, audio, fonts, parameter tables, and compiled game-specific headers stay in the viewer's private build folder.

The installer invokes:

```
python pipeline/build_assets.py --gta GTA_DIRECTORY --elden-ring ER_GAME_DIRECTORY --output NEW_PRIVATE_WORKSPACE --tool-root PREPARED_TOOLS
```

On macOS, also pass `--wine` and `--bottle` for the existing CrossOver environment. `--java` can select Java 21 explicitly. The full Python environment needs NumPy, Pillow, SciPy, Soulstruct, and Soulstruct Havok, with the versions pinned by setup.

Tool paths default to the prepared tools layout. The flags `--dotnet`, `--python`, `--ffdec`, `--er-extractor`, `--er-params`, `--oodle-helper`, `--soulsformats-dll`, and `--paramdefs` can override individual locations.

Successful full asset reconstruction creates:

- `Package/assets`: the locally rebuilt regulation, six-placement HUD patch, and one English death-message override.
- `Package/runtime/data`: every mesh, animation, texture, and audio packet consumed by the portable runtime.
- `generated`: the locally rebuilt headers listed in `runtime/generated-headers.json`.
- `asset-build.json`: sizes and SHA-256 hashes for every required payload file and header.

Only `Package` and `generated` are consumed by the installer. All other folders are extraction and conversion intermediates. No part of a generated build belongs in a public source release.

A full asset receipt is written only after every stage succeeds. It does not claim runtime compilation, Windows validation, or gameplay acceptance. `--stage gta-core` produces a partial diagnostic build and never writes a complete asset receipt. `--resume-extraction` accepts only extraction recorded in that same workspace from the same selected GTA executable and unchanged source archives.

The terrain atlas retains the working mod's original Limgrave coverage. The vehicle stage rebuilds the existing parked Buffalo model. Audio preserves the existing source-sample mixing policy; original game synth/reverb graphs are not recreated. The message override is English only. These choices preserve the existing mod's scope.
