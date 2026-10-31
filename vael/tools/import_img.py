#!/usr/bin/env python3
"""Convert generated images into game-ready WebP.

Kinds, chosen by name prefix:
- bg_*, key_*, fin_*: opaque paintings, kept at 1536 px wide.
- tex_*: tileable textures, made seamless (mirror-free offset blend), 512 px.
- fx_*: additive effects on black: black becomes transparent-friendly (kept RGB, loaded additive), 768 px.
- prop_*, fg_*, plat_*, shrine, fallen_star, blade, ui_*, it_*, npc_*: cut-outs with alpha, trimmed, capped at 1024 px.
Usage: import_img.py OUT_DIR file.png [...]
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


def seamless(im: Image.Image) -> Image.Image:
    a = np.asarray(im.convert("RGB")).astype(np.float32)
    h, w = a.shape[:2]
    shifted = np.roll(np.roll(a, h // 2, axis=0), w // 2, axis=1)
    # Blend weight: 1 near the original's centre, 0 near its edges, so the seams of the original fall where the
    # shifted copy is fully visible, and the seams of the copy where the original is.
    yy, xx = np.mgrid[0:h, 0:w]
    wy = 1 - np.abs((yy - h / 2) / (h / 2))
    wx = 1 - np.abs((xx - w / 2) / (w / 2))
    wgt = np.clip(np.minimum(wx, wy) * 3, 0, 1)[..., None]
    out = a * wgt + shifted * (1 - wgt)
    return Image.fromarray(out.clip(0, 255).astype(np.uint8))


def trim(im: Image.Image) -> Image.Image:
    # Fully transparent pixels must be black: their hidden colour bleeds when the GPU filters the texture.
    arr = np.asarray(im).copy()
    arr[arr[..., 3] < 4] = 0
    im = Image.fromarray(arr, "RGBA")
    a = im.getchannel("A").point(lambda v: 255 if v > 10 else 0)
    bb = a.getbbox()
    return im.crop(bb) if bb else im


def main() -> None:
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    for f in sys.argv[2:]:
        p = Path(f)
        name = p.stem
        im = Image.open(p)
        if name.startswith(("bg_", "key_", "fin_", "cut_")):
            im = im.convert("RGB")
            im.thumbnail((1536, 1536), Image.LANCZOS)
            im.save(out / f"{name}.webp", "WEBP", quality=86, method=6)
        elif name.startswith("tex_"):
            im = seamless(im)
            im = im.resize((512, 512), Image.LANCZOS)
            im.save(out / f"{name}.webp", "WEBP", quality=88, method=6)
        elif name.startswith("fx_") and im.mode != "RGBA":
            im = im.convert("RGB")
            im.thumbnail((768, 768), Image.LANCZOS)
            im.save(out / f"{name}.webp", "WEBP", quality=88, method=6)
        else:
            im = trim(im.convert("RGBA"))
            # Decor is never shown very large; foreground frames and portraits keep more detail.
            cap = 1024 if name.startswith(("fg_", "pt_", "npc_", "plat_", "edge_")) else 768
            im.thumbnail((cap, cap), Image.LANCZOS)
            im.save(out / f"{name}.webp", "WEBP", quality=88, method=6, exact=True)
            if name.startswith("fg_"):
                from feather import feather
                feather(str(out / f"{name}.webp"))
        print(name, im.size)


if __name__ == "__main__":
    main()
