#!/usr/bin/env python3
"""Import generated PNGs into public/img as WebP. Usage: import.py DIR [names...]
Opaque paintings keep their size (max 1920); transparent images are trimmed and capped at 1024."""
import sys
from pathlib import Path
from PIL import Image

OUT = Path(__file__).resolve().parent.parent / "public" / "img"


def main() -> None:
    src = Path(sys.argv[1])
    names = sys.argv[2:]
    OUT.mkdir(parents=True, exist_ok=True)
    for p in sorted(src.glob("*.png")):
        if names and p.stem not in names:
            continue
        im = Image.open(p)
        if im.mode == "RGBA" and im.getextrema()[3][0] < 250:
            bb = im.getchannel("A").point(lambda a: 255 if a > 8 else 0).getbbox()
            if bb:
                im = im.crop(bb)
            im.thumbnail((1024, 1024), Image.LANCZOS)
            im.save(OUT / f"{p.stem}.webp", "WEBP", quality=90, method=6)
        else:
            im = im.convert("RGB")
            im.thumbnail((1920, 1920), Image.LANCZOS)
            im.save(OUT / f"{p.stem}.webp", "WEBP", quality=88, method=6)
        print(p.stem, im.size)


if __name__ == "__main__":
    main()
