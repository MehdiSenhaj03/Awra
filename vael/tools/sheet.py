#!/usr/bin/env python3
"""Cut an animation sheet produced by the image generator into aligned, trimmed frames.

- Removes a flat magenta (#FF00FF) background (or keeps real transparency) with a soft edge and despill.
- Finds the drawings (connected pieces), assigns every piece to a grid cell by its centre, so a floating
  halo or a detached cape tip stays with its frame.
- Anchors every frame at the feet (lowest point) and at the head column (median x of the head band),
  scales the whole sheet to a target character height, and packs the frames into one atlas.

Usage: sheet.py IN.png COLS ROWS OUT_BASENAME [--height 128] [--order rows] [--flip]
Writes OUT_BASENAME.png (atlas) and OUT_BASENAME.json ({frames: [{x,y,w,h,ax,ay}], cell}).
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage


def key_out(img: Image.Image) -> np.ndarray:
    a = np.asarray(img.convert("RGBA")).astype(np.float32)
    rgb, alpha = a[..., :3], a[..., 3]
    if alpha.min() < 250:  # real transparency
        return a
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    # Distance from pure magenta, measured on the chroma that matters.
    mag = np.minimum(r, b) - g  # high for magenta
    dist = np.sqrt((255 - r) ** 2 + g ** 2 + (255 - b) ** 2)
    alpha = np.clip((dist - 60) / 90, 0, 1) * 255
    alpha = np.where(mag > 150, 0, alpha)
    # Despill: pull the magenta cast out of soft edges.
    excess = np.clip(mag, 0, None)
    k = 1 - alpha / 255 * 0.5
    rgb[..., 0] = np.clip(r - excess * k, 0, 255)
    rgb[..., 2] = np.clip(b - excess * k, 0, 255)
    out = np.dstack([rgb, alpha])
    return out


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("cols", type=int)
    ap.add_argument("rows", type=int)
    ap.add_argument("out")
    ap.add_argument("--height", type=float, default=128, help="target median frame height in px")
    ap.add_argument("--scale", type=float, default=0, help="explicit scale (overrides --height)")
    ap.add_argument("--flip", action="store_true", help="mirror so the character faces right")
    ap.add_argument("--min-area", type=int, default=400)
    ap.add_argument("--frames", type=str, default="", help="comma list of cell indices to keep, in order")
    ap.add_argument("--star", type=float, default=0, help="scale so the floating star above the hood is this tall (px)")
    args = ap.parse_args()

    img = Image.open(args.src)
    a = key_out(img)
    H, W = a.shape[:2]
    mask = a[..., 3] > 40
    lab, n = ndimage.label(ndimage.binary_dilation(mask, iterations=2))
    cw, ch = W / args.cols, H / args.rows
    cells: dict[int, list[int]] = {}
    sizes = ndimage.sum(mask, lab, range(1, n + 1))
    centers = ndimage.center_of_mass(mask, lab, range(1, n + 1))
    for i in range(n):
        if sizes[i] < args.min_area:
            continue
        cy, cx = centers[i]
        col = min(args.cols - 1, int(cx // cw))
        row = min(args.rows - 1, int(cy // ch))
        cells.setdefault(row * args.cols + col, []).append(i + 1)
    order = [int(v) for v in args.frames.split(",")] if args.frames else sorted(cells)
    frames = []
    for idx in order:
        labels = cells.get(idx)
        if not labels:
            print(f"warning: cell {idx} empty")
            continue
        m = np.isin(lab, labels) & mask
        ys, xs = np.nonzero(m)
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        crop = a[y0:y1, x0:x1].copy()
        crop[..., 3] *= m[y0:y1, x0:x1]
        h = y1 - y0
        # Head band: skip the top 8 % (halo) and take 8-30 %.
        band = m[y0 + int(h * 0.10): y0 + int(h * 0.30), x0:x1]
        bx = np.nonzero(band)[1]
        head_x = float(np.median(bx)) if len(bx) else (x1 - x0) / 2
        # The floating star: the largest small piece near the top of the frame (undilated pieces).
        sub = m[y0:y1, x0:x1]
        slab, sn = ndimage.label(sub)
        star_h = 0
        best = 0
        if sn > 1:
            objs = ndimage.find_objects(slab)
            for k, sl in enumerate(objs):
                if sl is None:
                    continue
                hh = sl[0].stop - sl[0].start
                ww = sl[1].stop - sl[1].start
                area = int((slab[sl] == k + 1).sum())
                if sl[0].start < h * 0.3 and 150 <= area < sub.sum() * 0.08 and hh < h * 0.25 and 0.5 < ww / max(1, hh) < 2.2 and area > best:
                    best = area
                    star_h = max(hh, ww)
        frames.append({"img": crop, "h": h, "ax": head_x, "ay": float(h), "star": star_h})
    heights = sorted(f["h"] for f in frames)
    median_h = heights[len(heights) // 2]
    stars = sorted(f["star"] for f in frames if f["star"] > 0)
    median_star = stars[len(stars) // 2] if stars else 0
    if args.star and median_star:
        scale = args.star / median_star
    else:
        scale = args.scale or args.height / median_h
    print(f"{len(frames)} frames, median height {median_h}px, star {median_star}px ({len(stars)} found), scale {scale:.3f}")
    outs = []
    for f in frames:
        im = Image.fromarray(np.clip(f["img"], 0, 255).astype(np.uint8), "RGBA")
        nw, nh = max(1, round(im.width * scale)), max(1, round(im.height * scale))
        im = im.resize((nw, nh), Image.LANCZOS)
        ax, ay = f["ax"] * scale, f["ay"] * scale
        if args.flip:
            im = im.transpose(Image.FLIP_LEFT_RIGHT)
            ax = nw - ax
        outs.append((im, ax, ay))
    # Pack in rows of up to 8, with 2 px padding.
    pad = 2
    per_row = 8
    rows = [outs[i:i + per_row] for i in range(0, len(outs), per_row)]
    aw = max(sum(o[0].width + pad * 2 for o in r) for r in rows)
    ah = sum(max(o[0].height for o in r) + pad * 2 for r in rows)
    atlas = Image.new("RGBA", (aw, ah))
    meta = []
    y = 0
    for r in rows:
        x = 0
        rh = max(o[0].height for o in r)
        for im, ax, ay in r:
            atlas.paste(im, (x + pad, y + pad))
            meta.append({"x": x + pad, "y": y + pad, "w": im.width, "h": im.height, "ax": round(ax, 1), "ay": round(ay, 1)})
            x += im.width + pad * 2
        y += rh + pad * 2
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(out.with_suffix(".png"))
    json.dump({"frames": meta, "scale": scale}, open(out.with_suffix(".json"), "w"))
    try:
        print(f"atlas {aw}x{ah} -> {out.with_suffix('.png')}", flush=True)
    except BrokenPipeError:
        pass


if __name__ == "__main__":
    main()
