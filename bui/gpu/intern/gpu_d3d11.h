/* BUI GPU renderer (Direct3D 11) — private interface for gpu_raster.c.
 *
 * A BuiRaster in GPU mode records its primitives as instances instead of
 * rasterizing them; this backend draws them into a persistent render target
 * (so damage-region redraws work exactly like the software raster) and
 * presents it with a flip-model swap chain plus dirty rects.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <stddef.h>
#include <stdint.h>

/* instance kinds: keep in sync with gpu/shaders/bui_ui.hlsl */
enum {
  BG_RECT = 0,
  BG_WIDGET = 1,
  BG_ELLIPSE = 2,
  BG_LINE = 3,
  BG_MASK = 4,
  BG_IMAGE = 5,
  BG_SHADOW = 6,
  BG_GRADIENT = 7,
  BG_HSV = 8,
  BG_SPHERE = 9,
  BG_SLUG = 10, /* glyph outline (Slug): rect = pen x, baseline y, px per unit, shear */
  /* BUI-Awra: a snapshot of the frame put back over what was drawn since:
   * rect.xy = where the snapshot's (0, 0) lies; p[1] 0: a vertical fade
   * (c1 = y, height, alpha at the top, at the bottom), 1: outside a rounded
   * box (c1 = x0 y0 x1 y1, c2 = radii TL TR BR BL) */
  BG_RESTORE = 11,
  /* BUI-Awra, as the software raster's (gpu_raster_awra.inc.c), drawn by
   * bui_gpu_inst_awra: a rounded box with per-corner radii and per-side
   * borders (rect = x y w h, c0 fill, c1 border, c2 radii TL TR BR BL,
   * c3 widths top right bottom left, p[1] has fill, p[2] has border) */
  BG_RBOX = 12,
  /* a box shadow (rect = the shadow's box x0 y0 x1 y1, c0 color, c1 its
   * radii, c2 the box x0 y0 x1 y1, c3 the box's radii, uv = 1 / (sigma *
   * sqrt 2), blurred, clipped out) */
  BG_BOX_SHADOW = 13,
  /* a rounded box's gradient (rect = x y w h, c0 from, c1 to, c2 radii,
   * p[1] horizontal) */
  BG_RBOX_GRADIENT = 14,
};
enum {
  BG_W_SHADE_H = 1,
  BG_W_CHECKER = 2,
  BG_W_LEGACY = 4,
  BG_W_INNER = 8,
};

typedef struct BuiGpuInst {
  float rect[4];
  float bounds[4]; /* x0, y0, x1, y1: pixel box, already clipped */
  float c0[4], c1[4], c2[4], c3[4];
  float p[4]; /* p[0] = kind */
  float uv[4];
} BuiGpuInst;

typedef struct BuiGpu BuiGpu;

/* Starts creating the device on a worker thread (the first frames keep
 * using the software raster). NULL when D3D11 isn't available. */
BuiGpu *bui_gpu_create(void *hwnd);
void bui_gpu_free(BuiGpu *g);
/* 1 once the device is ready, -1 when creation failed, 0 while pending
 * (never 0 with `wait`). */
int bui_gpu_state(BuiGpu *g, int wait);

void bui_gpu_begin(BuiGpu *g, int w, int h);
/* Appends an instance (zeroed except kind and bounds). */
BuiGpuInst *bui_gpu_inst(BuiGpu *g, int kind, int x0, int y0, int x1, int y1);
/* Atlas placement of an 8-bit mask keyed by `key` (NULL key: not cached).
 * Returns 0 when it can't be placed. */
int bui_gpu_mask(BuiGpu *g, const void *key, unsigned gen, const uint8_t *mask, int w, int h,
                 int *r_x, int *r_y);
/* Slug text (Eric Lengyel's algorithm): 1 when this device can render
 * glyph outlines (creates the curve/band textures on first call). Whether
 * it is used is BUI_gpu_slug(). */
int bui_gpu_slug_on(BuiGpu *g);
/* Curve/band data for glyph `glyph` of font face `face` in outline variant
 * `var` (bui_text_glyph_outline's), uploaded on first use: its box in font
 * units, the band transform (instance c1) and its band location (instance
 * uv). 0 for empty glyphs or when unavailable. `gen` changes whenever faces
 * may have been freed. */
int bui_gpu_slug_glyph(BuiGpu *g, int face, int glyph, unsigned var, unsigned gen, float r_bbox[4],
                       float r_band[4], int r_loc[4]);
/* Provided by the text engine (gpu_raster.c): a glyph's outline as
 * quadratic curves (6 floats each, font units, y up) and its box. `var`:
 * the instance of a variable face (bits 0-9 wght, 10-17 opsz in halves, 0:
 * the face's) and bit 18 cairo-ft's synthetic bold (FT_GlyphSlot_Embolden),
 * as the text engine gives them (0 for a plain face). */
int bui_text_glyph_outline(int face, int glyph, unsigned var, float **r_curves, int *r_n,
                           float r_bbox[4]);

/* Atlas placement of a premultiplied 0xAARRGGBB image, cached by `key`
 * (BuiPixmap.key, or its content hash). 0 when it can't be placed. */
int bui_gpu_image(BuiGpu *g, uint64_t key, int format, const uint32_t *argb, int w, int h, int *r_x, int *r_y);
/* BUI-Awra: the pixels to upload for `px` (`format`, rows `*stride` pixels
 * apart): ARGB and NV12 as they are, straight RGBA premultiplied into ARGB
 * and I420's chroma planes interleaved into NV12, in `*scratch` (grown with
 * realloc, `*cap` bytes; the renderer frees it), `*stride` then w. NULL when
 * out of memory. The atlas takes ARGB only (`format` ARGB or RGBA). */
const uint32_t *bui_pixmap_upload(int format, const uint32_t *px, int *stride, int w, int h,
                                  uint32_t **scratch, size_t *cap);
/* Images too big for the atlas, and video frames, get a texture of their
 * own, uploaded once per `key`; `base` is what BUI_image_forget names (the
 * image's key, or its stream). With `stream`, a new key of the same size and
 * format updates the stream's texture in place (a video: one texture, no
 * allocation per frame). `format` is BUI_PIXMAP_*: ARGB (or RGBA) rows are `stride`
 * pixels apart; NV12 or I420 (stride w) becomes an R8 luma and an R8G8 chroma
 * texture, which image instances sample as YCbCr when their p[3] is the
 * format. Returns a texture handle > 0 for bui_gpu_inst_image, 0 when it
 * can't be made. Unused ones are freed. */
int bui_gpu_image_tex(BuiGpu *g, uint64_t key, uint64_t base, int stream, int format,
                      const uint32_t *px, int stride, int w, int h);
/* The largest texture side the device takes (images over it are tiled). */
int bui_gpu_max_texture(BuiGpu *g);
/* An image instance sampling texture `tex` (0: the atlas), batched with
 * other instances that use the same one. */
BuiGpuInst *bui_gpu_inst_image(BuiGpu *g, int tex, int x0, int y0, int x1, int y1);
/* BUI-Awra: copies of the frame, for fades and rounded clips over what a
 * widget draws (BUI_raster_read / _corners_save). snap_take draws what is
 * queued and readies snapshot `snap` (< BUI_GPU_SNAPS; the caller reuses one
 * once what it restores has been queued) at least w x h; snap_copy copies the
 * frame's (x, y, w, h) to (dx, dy) in it; inst_snap draws part of it back
 * (BG_RESTORE). inst_awra: an instance of the kinds above that upstream's
 * shaders lack. */
#define BUI_GPU_SNAPS 64
int bui_gpu_snap_take(BuiGpu *g, int snap, int w, int h);
BuiGpuInst *bui_gpu_inst_awra(BuiGpu *g, int kind, int x0, int y0, int x1, int y1);
void bui_gpu_snap_copy(BuiGpu *g, int snap, int x, int y, int w, int h, int dx, int dy);
BuiGpuInst *bui_gpu_inst_snap(BuiGpu *g, int snap, int x0, int y0, int x1, int y1);
void bui_gpu_triangles(BuiGpu *g,
                       const float (*co)[2],
                       const float (*col)[4],
                       int count,
                       const int clip[4]);
/* Textured meshes (BUI_raster_mesh): `vtx` translated by (ox, oy), indices
 * uint16 or uint32 (`idx32`), each < n_vtx (others drop their triangle);
 * `clip` is the pixel scissor (x0, y0, x1, y1), non-empty. Texture `tex`
 * (0: white) comes from the registry below and is uploaded (or updated) on
 * first use. */
struct BuiMeshVertex;
void bui_gpu_mesh(BuiGpu *g,
                  const struct BuiMeshVertex *vtx,
                  int n_vtx,
                  const void *idx,
                  int idx32,
                  int n_idx,
                  float ox,
                  float oy,
                  int tex,
                  int nearest,
                  const int clip[4]);

/* Texture registry shared by the renderers (gpu_raster.c). */
typedef struct BuiTexView {
  int w, h;
  const uint8_t *rgba; /* w * 4 bytes per row */
  unsigned gen;        /* bumped by every update */
} BuiTexView;
int bui_texture_view(int tex, BuiTexView *r_view); /* 0 when not live */
/* Rects (x, y, w, h) updated after `since`; -1 when the log no longer
 * reaches back that far (upload everything). */
int bui_texture_changes(int tex, unsigned since, int (*r_rects)[4], int max);
/* Bumped whenever a texture is freed (GPU caches sweep then). */
unsigned bui_texture_free_gen(void);
/* Keys BUI_image_forget named since `*seen` (advanced): their count, or -1
 * when more went by than the log keeps (free whatever isn't in use). */
int bui_image_forgotten(unsigned *seen, uint64_t *r_keys, int max);

/* Records damage for present (x, y, w, h); w <= 0 means the whole frame. */
void bui_gpu_damage(BuiGpu *g, int x, int y, int w, int h);
/* Draws everything recorded and presents. Returns 0 on device loss. */
int bui_gpu_present(BuiGpu *g, int full);
/* Reads the frame back as 0x00RRGGBB rows. 0 when there is none yet. */
int bui_gpu_read(BuiGpu *g, uint32_t *dst, int w, int h);
