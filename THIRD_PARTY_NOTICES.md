# Third-party notices

Author identity for this mod project: **igobygod**. New project glue and installer code are supplied under the included MIT license. This grant does not cover game assets, upstream code or third-party trademarks.

Vendored source notices are retained at:

- `builder/runtime/Bridge/vendor/er-reference/LICENSE` — memory helper code, MIT.
- `builder/runtime/Source/vendor/hudhook/vendor/minhook/LICENSE.txt` — MinHook and its bundled disassembler notices.

The bootstrap retrieves dependencies from their upstream projects onto the viewer's own computer. Their original licenses and notices remain in those local downloads. They are not bundled with this repository:

- [CodeWalker](https://github.com/dexyfex/CodeWalker): GTA archive and asset formats. Upstream `Notice.txt` and `Readme_Src.txt` must be consulted; the latter describes the source as for educational purposes only; this project does not grant redistribution rights to CodeWalker. Material/shader and vehicle conversion recipes cite the corresponding algorithms as references.
- [SoulsFormats](https://github.com/JKAnderson/SoulsFormats): FromSoftware data formats.
- [UXM Selective Unpack](https://github.com/Nordgaren/UXM-Selective-Unpack): archive lookup/decryption support; its source is fetched locally.
- [Paramdex](https://github.com/soulsmods/Paramdex): parameter field definitions, fetched locally.
- [JPEXS FFDec](https://github.com/jindrapetrik/jpexs-decompiler): local HUD shape/font extraction.
- [ME3](https://github.com/garyttierney/me3/releases/tag/v0.13.0): mod loader and isolated save profile.
- [Soulstruct](https://github.com/Grimrukh/soulstruct), [Soulstruct Havok](https://github.com/Grimrukh/soulstruct-havok), NumPy, SciPy and Pillow: local conversion libraries.

The viewer must supply the game installations. No game assets, proprietary Oodle DLL, generated game-specific headers, compiled asset-bearing runtime or saved game may be taken from a local build and added to the source release.
