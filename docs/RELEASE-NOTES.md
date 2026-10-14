# 0.2.1-windows-preview — Windows long-path extraction fix

Fixes setup stopping while unpacking .NET SDK files below the user's default installation folder. The previous ZIP extraction used ordinary Windows paths; deep SDK filenames plus the temporary staging suffix exceeded MAX_PATH when Windows long-path policy was disabled. The installer now extracts, commits and cleans up using extended Windows paths and explicitly enables modern .NET Framework path handling. Traversal/symlink rejection and atomic staging remain enforced. No Windows registry change is made.

The failure was reproduced in a Windows 11 LTSC ARM64 VM with `LongPathsEnabled=0`. The updated x64 .NET Framework test passes extraction beyond MAX_PATH and traversal rejection; the full cached .NET SDK archive also extracts successfully at that destination. Full Windows installation and gameplay remain pending.

To retry: close 0.2.0, open 0.2.1, select the same games and mod destination, then click Install mod. Existing verified downloads are reused. All game-version restrictions, tool downloads and storage requirements are unchanged.

## Previous preview: 0.2.0-windows-preview

This private preview preserves the existing Windows setup binary and its embedded mod snapshot. Repository preparation adds readable source, installation disclosures, compatibility/test status and maintainer documentation; it does not certify new game versions or Windows gameplay.

## Included

- Graphical setup with game folder selection, automatic tool preparation, local asset build, progress/error messages and a desktop Play shortcut.
- Existing Franklin/weapons/wheel/console/HUD/sword implementation and Buffalo prototype.
- Source conversion pipeline and dependency pins; no game assets distributed.
- One exact Elden Ring executable gate, GTA V Legacy asset input and offline separate-save launch profile.

## Known release limitations

- Complete native Windows installation and gameplay remain unverified.
- Compatibility of the accepted executable with ordinary Steam/retail installs remains unverified.
- No GTA V Enhanced support, automatic updates or all-version support.
- Unsigned executable. Microsoft component installation may need admin approval and restart; setup requires internet, several GB of downloads and at least 35 GB free.
- See [compatibility and testing](COMPATIBILITY-AND-TESTING.md) for the full scope and [installation transparency](INSTALLATION-TRANSPARENCY.md) for downloaded tools/system changes.
