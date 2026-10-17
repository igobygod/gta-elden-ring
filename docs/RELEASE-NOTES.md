# Release notes

## 0.2.7 — Windows preview

- Simplified setup messages and documentation.
- Updated compatibility details and installation test results.
- Includes the Windows installation fixes from 0.2.1–0.2.6.

## 0.2.6

Accepted the verified Windows roll/jump packet fingerprint after comparing all 62 clips. Only two position values differ from the reference, by at most 1.46e-11 metres. Corrupted files remain rejected.

## 0.2.5

Accepted the verified Windows Buffalo packet fingerprint. All geometry values match the reference; differences are confined to 116 positive/negative zero representations. Verification reports all mismatched files together.

## 0.2.4

Accepted the verified Windows melee packet fingerprint after checking all 64 clips. Rotation signs differ while representing the same motion. Error dialogs now show the builder's failure reason.

## 0.2.3

Made duplicate weapon texture selection deterministic across Windows and macOS. The complete corrected atlas matches the reference.

## 0.2.2

Fixed managed-tool build output paths inherited from the Microsoft x64 compiler environment.

## 0.2.1

Fixed .NET SDK extraction beyond Windows MAX_PATH without changing the system long-path setting.

## 0.2.0

Initial Windows setup preview with folder selection, automatic tool preparation, local asset conversion and a desktop Play shortcut.

Gameplay on supported Windows x64 hardware remains untested. The installer is unsigned and supports one exact Elden Ring executable.
