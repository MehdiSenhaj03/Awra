/* Group opacity: what a widget draws inside a box, faded as one layer
 * towards what was under it, as GTK renders a node with CSS `opacity`
 * (gtk_snapshot_push_opacity: an offscreen, then blended). Drawing each
 * primitive at that opacity instead lets the parts below show through the
 * parts on top (a slider's highlight through its thumb).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include <math.h>
#include <stdlib.h>

bool awra_group_begin(AwraDraw *d, AwraBox b, float alpha, AwraGroup *g)
{
  g->saved = NULL;
  g->alpha = alpha;
  g->opacity = d->opacity;
  if (alpha >= 1.0f) {
    return false;
  }
  g->x = (int)floorf(b.x);
  g->y = (int)floorf(b.y);
  g->w = (int)ceilf(b.x + b.w) - g->x;
  g->h = (int)ceilf(b.y + b.h) - g->y;
  if (g->w > 0 && g->h > 0) {
    g->saved = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)g->w * (size_t)g->h);
  }
  if (g->saved && SR_read(d->r, g->x, g->y, g->w, g->h, g->saved)) {
    return true;
  }
  free(g->saved);
  g->saved = NULL;
  d->opacity *= alpha; /* no read back: each primitive at the opacity */
  return false;
}

void awra_group_end(AwraDraw *d, AwraGroup *g)
{
  if (g->saved) {
    SR_fade(d->r, g->x, g->y, g->w, g->h, g->saved, g->alpha, g->alpha);
    free(g->saved);
    g->saved = NULL;
  }
  d->opacity = g->opacity;
}

/* A widget drawn under a scale transform about its centre (GTK's
 * `transform: scale(.96)` on a pressed button): it draws on cleared
 * pixels as usual, then that layer is put back over what was under it,
 * scaled. Software raster only (a GPU frame draws it unscaled). */
int awra_press_group_depth;

bool awra_scale_group_begin(AwraDraw *d, AwraBox b, float scale, AwraGroup *g)
{
  g->saved = NULL;
  g->alpha = scale;
  if (scale >= 1.0f || BUI_raster_is_gpu(d->r)) {
    return false;
  }
  g->x = (int)floorf(b.x);
  g->y = (int)floorf(b.y);
  g->w = (int)ceilf(b.x + b.w) - g->x;
  g->h = (int)ceilf(b.y + b.h) - g->y;
  if (g->w <= 0 || g->h <= 0) {
    return false;
  }
  g->saved = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)g->w * (size_t)g->h);
  if (!g->saved || !SR_read(d->r, g->x, g->y, g->w, g->h, g->saved)) {
    free(g->saved);
    g->saved = NULL;
    return false;
  }
  SR_clear(d->r, g->x, g->y, g->w, g->h);
  awra_press_group_depth++;
  return true;
}

void awra_scale_group_end(AwraDraw *d, AwraGroup *g)
{
  if (!g->saved) {
    return;
  }
  awra_press_group_depth--;
  uint32_t *layer = (uint32_t *)calloc((size_t)g->w * (size_t)g->h, sizeof(uint32_t));
  if (layer && SR_read(d->r, g->x, g->y, g->w, g->h, layer)) {
    /* what was under it back, then the layer over it, scaled */
    SR_fade(d->r, g->x, g->y, g->w, g->h, g->saved, 0.0f, 0.0f);
    static uint64_t serial;
    const BuiPixmap pm = {layer, g->w, g->h, (1ull << 61) | ++serial, BUI_PIXMAP_ARGB, 0};
    const float s = g->alpha, dw = g->w * s, dh = g->h * s;
    SR_image(d->r, g->x + (g->w - dw) * 0.5f, g->y + (g->h - dh) * 0.5f, dw, dh, &pm, NULL, 1.0f, 0);
  }
  else {
    SR_fade(d->r, g->x, g->y, g->w, g->h, g->saved, 1.0f, 1.0f);
  }
  free(layer);
  free(g->saved);
  g->saved = NULL;
}
