"""Feather image borders so cut-off silhouettes never show a hard rectangular edge.
usage: python3 tools/feather.py file.webp [frac=0.14] [sides=ltr]"""
import sys
import numpy as np
from PIL import Image

def feather(path, frac=0.14, sides='ltr'):
    im = np.array(Image.open(path).convert('RGBA')).astype(np.float32)
    h, w = im.shape[:2]
    fx, fy = max(1, int(w * frac)), max(1, int(h * frac))
    m = np.ones((h, w), np.float32)
    ramp = lambda n: (np.linspace(0, 1, n) ** 1.6)
    if 'l' in sides: m[:, :fx] *= ramp(fx)[None, :]
    if 'r' in sides: m[:, -fx:] *= ramp(fx)[::-1][None, :]
    if 't' in sides: m[:fy, :] *= ramp(fy)[:, None]
    if 'b' in sides: m[-fy:, :] *= ramp(fy)[::-1][:, None]
    im[..., 3] *= m
    im[im[..., 3] < 4] = 0
    Image.fromarray(im.clip(0, 255).astype(np.uint8)).save(path, quality=90, method=6)

if __name__ == '__main__':
    feather(sys.argv[1], float(sys.argv[2]) if len(sys.argv) > 2 else 0.14, sys.argv[3] if len(sys.argv) > 3 else 'ltr')
