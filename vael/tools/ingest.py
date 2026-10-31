#!/usr/bin/env python3
"""Ingest a downloaded batch: paintings and props to WebP, animation sheets to atlases, icon sheets to icons.

Usage: ingest.py BATCH_DIR (containing assets_out/*.png)
"""
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"
IMG = ROOT / "public" / "img"
SPR = ROOT / "public" / "sprites"

# In-game height of each foe sheet's frames (px), for resampling only (the renderer scales again).
ENEMY_H = {
    "en_forgeron_mineur": 140, "en_enclume": 230, "en_etincelle": 100, "en_noye": 170, "en_anguille": 90,
    "en_geolier": 210, "en_corbeau": 120, "en_astre": 110, "en_ombre": 160, "en_sentinelle": 220, "en_veilleur": 200,
}
BOSS_H = 420
SHARED_SCALE = {"boss_geoliers", "boss_bibliothecaire"}

ICON_NAMES = {
    "icons_seals": [f"ic_seal_{i}" for i in range(16)],
    "icons_items": [f"ic_item_{i}" for i in range(16)],
    "icons_arts": [f"ic_art_{i}" for i in range(16)],
}
# Aliases for ability and art icons, taken from the item and art sheets.
ALIASES = {
    "icons_items": {10: "ic_ab_blade", 11: "ic_ab_double", 12: "ic_ab_walljump", 13: "ic_ab_blink", 14: "ic_ab_ring", 15: "ic_ab_comet"},
    "icons_arts": {0: "ic_art_comete", 1: "ic_art_comete2", 2: "ic_art_ascension", 3: "ic_art_ascension2", 4: "ic_art_chute", 5: "ic_art_chute2", 6: "ic_art_constellation", 7: "ic_ab_dash"},
}


def sheet(src: Path, name: str, *args: str) -> str:
    r = subprocess.run([sys.executable, str(TOOLS / "sheet.py"), str(src), "4", "2", str(SPR / name), *args], capture_output=True, text=True)
    return r.stdout.strip().splitlines()[0] if r.stdout.strip() else r.stderr[-300:]


def icons(src: Path, stem: str) -> None:
    im = Image.open(src).convert("RGBA")
    a = np.asarray(im)
    mask = a[..., 3] > 30
    lab, n = ndimage.label(ndimage.binary_dilation(mask, iterations=6))
    H, W = mask.shape
    cells: dict[int, list[int]] = {}
    sizes = ndimage.sum(mask, lab, range(1, n + 1))
    centers = ndimage.center_of_mass(mask, lab, range(1, n + 1))
    for i in range(n):
        if sizes[i] < 300:
            continue
        cy, cx = centers[i]
        idx = min(3, int(cy // (H / 4))) * 4 + min(3, int(cx // (W / 4)))
        cells.setdefault(idx, []).append(i + 1)
    names = ICON_NAMES[stem]
    for idx, labels in cells.items():
        m = np.isin(lab, labels) & mask
        ys, xs = np.nonzero(m)
        crop = im.crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))
        crop.thumbnail((256, 256), Image.LANCZOS)
        out = [names[idx]] + ([ALIASES[stem][idx]] if idx in ALIASES.get(stem, {}) else [])
        for nm in out:
            crop.save(IMG / f"{nm}.webp", "WEBP", quality=90, method=6, exact=True)
    print(stem, len(cells), "icons")


def main() -> None:
    d = Path(sys.argv[1]) / "assets_out"
    plain = []
    bosses: dict[str, list[Path]] = {}
    for p in sorted(d.glob("*.png")):
        n = p.stem
        if n in ENEMY_H:
            print(n, sheet(p, n, "--height", str(ENEMY_H[n])))
        elif n.startswith("boss_") and n[-2:] in ("_a", "_b", "_c"):
            bosses.setdefault(n[:-2], []).append(p)
        elif n == "boss_etoile":
            print(n, sheet(p, n, "--height", "460"))
        elif n in ICON_NAMES:
            icons(p, n)
        else:
            plain.append(str(p))
    for key, files in bosses.items():
        files.sort()
        # The generator does not always keep the same scale between sheets: each one is matched on its median height,
        # except keepers whose later sheets are mostly crouched or collapsing poses (checked by eye): those reuse the
        # scale of their standing sheet.
        if key in SHARED_SCALE:
            out = sheet(files[0], files[0].stem, "--height", str(BOSS_H))
            print(files[0].stem, out)
            sc = out.split("scale ")[-1].split()[0]
            for f in files[1:]:
                print(f.stem, sheet(f, f.stem, "--scale", sc))
        else:
            for f in files:
                print(f.stem, sheet(f, f.stem, "--height", str(BOSS_H)))
    if plain:
        subprocess.run([sys.executable, str(TOOLS / "import_img.py"), str(IMG), *plain], check=True)
    subprocess.run([sys.executable, str(TOOLS / "manifest.py")], check=True)


if __name__ == "__main__":
    main()
