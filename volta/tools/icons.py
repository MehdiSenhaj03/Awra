#!/usr/bin/env python3
"""Slice 4x4 icon sheets into public/img/ic_<sheet>_<i>.webp (trimmed, 256 px). Usage: icons.py sheet.png prefix"""
import sys
from pathlib import Path
from PIL import Image

OUT = Path(__file__).resolve().parent.parent / "public" / "img"
src, prefix = sys.argv[1], sys.argv[2]
im = Image.open(src).convert("RGBA")
w, h = im.size
for r in range(4):
    for c in range(4):
        cell = im.crop((c * w // 4, r * h // 4, (c + 1) * w // 4, (r + 1) * h // 4))
        bb = cell.getchannel("A").point(lambda a: 255 if a > 10 else 0).getbbox()
        if bb:
            cell = cell.crop(bb)
        cell.thumbnail((256, 256), Image.LANCZOS)
        cell.save(OUT / f"ic_{prefix}{r * 4 + c}.webp", "WEBP", quality=90, method=6)
print("ok", prefix)
