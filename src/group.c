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
  if (g->saved && BUI_raster_read(d->r, g->x, g->y, g->w, g->h, g->saved)) {
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
    BUI_raster_fade(d->r, g->x, g->y, g->w, g->h, g->saved, g->alpha, g->alpha);
    free(g->saved);
    g->saved = NULL;
  }
  d->opacity = g->opacity;
}
