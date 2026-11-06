<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Awra 1 (GTK 4.22) vs Awra 2 (BUI)

KeyToMusic (the same application, the same `keytomusic-core`, ported from
Awra 1 to Awra 2) and each version's showcase, on the same machine (4 cores,
no GPU), under Xvfb (X11), with the same fonts (the GTK build's
fontconfig). `tools/bench_gtk_vs_bui.py`: a fresh home per run, the window
mapped, 6 s to settle, 20 s idle, 10 s of the pointer sweeping over the
keyboard (hover animations), medians of 5 runs (3 for the showcases).

GTK renders with GL by default; without a GPU that is Mesa's llvmpipe, so
GTK is also measured with its software renderer (`GSK_RENDERER=cairo`),
the fairer comparison for Awra 2's software raster.

## KeyToMusic

| | GTK (GL/llvmpipe) | GTK (Cairo) | Awra 2 | Awra 2 vs best GTK |
| --- | ---: | ---: | ---: | ---: |
| window mapped after launch | 238 ms | 218 ms | **61 ms** | 3.6× faster |
| CPU spent to start (first 6 s) | 3.67 s | 0.38 s | **0.13 s** | 2.9× less |
| memory (PSS) | 277 MB | 127 MB | **37.7 MB** | 3.4× less |
| memory (RSS) | 304 MB | 156 MB | **43.3 MB** | 3.6× less |
| private heap (RssAnon) | 191 MB | 53.5 MB | **21.9 MB** | 2.4× less |
| threads | 32 | 31 | **17** | |
| CPU idle | 0.2 % | 0.2 % | 0.25 % | (the audio engine) |
| CPU while hovering keys | 18.3 % | 19.6 % | **11.8 %** | 1.6× less |
| files mapped (libs, fonts, caches) | 123 / 295 MB | 122 / 295 MB | **57 / 39.5 MB** | 7.5× less |
| executable + shared libraries on disk | 14.9 + 46.5 MB (69 libs) | same | **18.0 + 9.2 MB (17 libs)** | 2.3× less |

## Showcase (C)

| | GTK (GL/llvmpipe) | GTK (Cairo) | Awra 2 |
| --- | ---: | ---: | ---: |
| window mapped after launch | 357 ms | 361 ms | **15 ms** |
| CPU spent to start | 2.98 s | 0.42 s | **0.02 s** |
| memory (PSS) | 277 MB | 123 MB | **12.1 MB** |
| memory (RSS) | 303 MB | 150 MB | **16.0 MB** |
| threads | 15 | 15 | **1** |
| executable + shared libraries | 0.2 + 45.4 MB (67 libs) | same | **3.6 + 6.3 MB (8 libs)** |

(The two showcases have different pages, so their hover figures are not
compared.)

## Rendering

A full frame of the Awra 2 showcase (1100×900 basics page / 1400×900
gallery, every pixel repainted): 8–9 ms / 16 ms on one core, software. After
the first frame only damaged areas are repainted: a hover repaints the
component under the pointer, not the window.
