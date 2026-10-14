import os
from pathlib import Path
import hashlib
import json

from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "PlayerRenderer/polish-next/hud"
OUT = Path(__file__).resolve().parent
SLOT_ANIMAL = (704, 128)
SLOT_PERSON = (768, 128)
CELL = 64


def intersects(a, b):
    return a[0] < b[0] + b[2] and b[0] < a[0] + a[2] and a[1] < b[1] + b[3] and b[1] < a[1] + a[3]


def main():
    base_manifest = json.loads((SOURCE / "hud-manifest.json").read_text())
    if (base_manifest["width"], base_manifest["height"]) != (1024, 1024):
        raise ValueError("source HUD atlas dimensions changed")
    base_bytes = (SOURCE / "hud-atlas.rgba").read_bytes()
    if hashlib.sha256(base_bytes).hexdigest() != base_manifest["atlasSHA256"]:
        raise ValueError("source HUD atlas no longer matches its manifest")
    atlas = Image.frombytes("RGBA", (1024, 1024), base_bytes)

    # Both 64x64 cells were verified fully transparent in the original atlas
    # and lie outside all declared rectangles and glyph rectangles.
    original_rects = base_manifest["rects"] + base_manifest["glyphs"]
    for name, (x, y) in (("radar_animal", SLOT_ANIMAL), ("radar_person", SLOT_PERSON)):
        slot = (x, y, CELL, CELL)
        if any(intersects(slot, (r["x"], r["y"], r["w"], r["h"])) for r in original_rects):
            raise ValueError(f"{name} slot overlaps an original HUD rectangle")
        if atlas.crop((x, y, x + CELL, y + CELL)).getchannel("A").getbbox() is not None:
            raise ValueError(f"{name} slot is no longer transparent")

    sheet_path = SOURCE / "donor/x64b.rpf/minimap.ytd/blips_texturesheet_ng.dds"
    sheet = Image.open(sheet_path).convert("RGBA")
    animal = sheet.crop((960, 192, 1024, 256))
    person = sheet.crop((512, 448, 576, 512))
    for name, icon in (("radar_animal", animal), ("radar_person", person)):
        if icon.size != (CELL, CELL) or icon.getchannel("A").getbbox() is None:
            raise ValueError(f"invalid source glyph for {name}")

    atlas.alpha_composite(animal, SLOT_ANIMAL)
    atlas.alpha_composite(person, SLOT_PERSON)
    rects = [dict(r) for r in base_manifest["rects"]]
    rects.extend([
        {"name": "radar_animal", "x": SLOT_ANIMAL[0], "y": SLOT_ANIMAL[1], "w": CELL, "h": CELL,
         "source": f"{sheet_path.relative_to(ROOT)} crop x=960..1023 y=192..255 (deer-face glyph)"},
        {"name": "radar_person", "x": SLOT_PERSON[0], "y": SLOT_PERSON[1], "w": CELL, "h": CELL,
         "source": f"{sheet_path.relative_to(ROOT)} crop x=512..575 y=448..511 (radar_friend glyph)"},
    ])

    raw = atlas.tobytes()
    sha = hashlib.sha256(raw).hexdigest()
    atlas.save(OUT / "hud-atlas.png")
    (OUT / "hud-atlas.rgba").write_bytes(raw)

    # Emit the complete original interface plus the two appended icons, so
    # the header and staged atlas are a matched pair for a single SRV/sampler.
    h = ["#pragma once", "#include <cstring>", "namespace gta_hud {",
         "inline constexpr int AtlasWidth=1024,AtlasHeight=1024;",
         "inline constexpr float FontSize=32;",
         "struct Rect { const char* name; int x,y,w,h; };",
         "struct Glyph { Rect rect; float advance,bearingX,bearingY; };",
         "inline constexpr Rect Rects[]={"]
    for r in rects:
        h.append('{"%s",%d,%d,%d,%d},' % (r["name"], r["x"], r["y"], r["w"], r["h"]))
    h.extend(["};", "inline constexpr Glyph Glyphs[95]={"])
    for g in base_manifest["glyphs"]:
        h.append('{{"%s",%d,%d,%d,%d},%.6ff,%.6ff,%.6ff},' %
                 (g["name"], g["x"], g["y"], g["w"], g["h"], g["advance"], g["bearingX"], g["bearingY"]))
    h.extend(["};",
              "inline const Rect* find(const char* name) { for(const auto& r:Rects)if(std::strcmp(name,r.name)==0)return &r;return nullptr;}",
              "inline const Glyph* glyph(unsigned char c) {return c>=32&&c<=126?&Glyphs[c-32]:nullptr;}",
              "}"])
    (OUT / "hud_atlas.h").write_text("\n".join(h) + "\n")

    manifest = {
        **{k: v for k, v in base_manifest.items() if k not in ("rects", "atlasSHA256", "limitations")},
        "rects": rects,
        "limitations": list(base_manifest["limitations"]) + [
            "radar_animal is an unlabelled deer-face glyph from the original blip texture sheet.",
            "radar_person is the original radar_friend waving-person glyph; draw tint can distinguish passive and allied contacts.",
            "Two icons occupy previously transparent cells at (704,64) and (768,64); all original rects and glyphs are preserved.",
            "This full atlas is staged separately and is not loaded by the live renderer.",
        ],
        "addedIcons": ["radar_animal", "radar_person"],
        "atlasSHA256": sha,
        "sourceAtlasSHA256": base_manifest["atlasSHA256"],
        "addedIconSource": str(sheet_path.relative_to(ROOT)),
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({"atlas": str(OUT / "hud-atlas.rgba"), "bytes": len(raw), "rects": len(rects), "sha256": sha}))


if __name__ == "__main__":
    main()
