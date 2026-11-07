/* Dashed borders as GTK draws them (gtkrenderborder.c render_frame_stroke,
 * the uniform case): one stroke along the rounded rectangle shrunk by half
 * the width, starting at the top-left corner's left end and going round
 * clockwise (gsk_rounded_rect_path), dashes `w` long and gaps `2w` (or the
 * length divided evenly into dash + gap periods when `w` doesn't divide
 * it), each dash with square caps. Each dash is filled as one polygon, so
 * a translucent colour covers every pixel once.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include <math.h>

typedef struct DashPath {
  float x0, y0, w, h, r;
  float seg[8]; /* lengths: TL arc, top, TR arc, right, BR arc, bottom, BL arc, left */
} DashPath;

/* the point at `s` along the path, with its unit tangent */
static void dash_point(const DashPath *p, float s, float *x, float *y, float *tx, float *ty)
{
  const float r = p->r, x0 = p->x0, y0 = p->y0, x1 = p->x0 + p->w, y1 = p->y0 + p->h;
  int i = 0;
  while (i < 7 && s > p->seg[i]) {
    s -= p->seg[i];
    i++;
  }
  /* arcs: centre, start angle (cairo's, y down), clockwise */
  static const float start[4] = {(float)M_PI, -(float)M_PI_2, 0.0f, (float)M_PI_2};
  if ((i & 1) == 0) {
    const int k = i / 2;
    const float cx = (k == 0 || k == 3) ? x0 + r : x1 - r;
    const float cy = (k < 2) ? y0 + r : y1 - r;
    const float a = start[k] + (r > 0.0f ? s / r : 0.0f);
    *x = cx + r * cosf(a);
    *y = cy + r * sinf(a);
    *tx = -sinf(a);
    *ty = cosf(a);
    return;
  }
  switch (i) {
    case 1:
      *x = x0 + r + s, *y = y0, *tx = 1, *ty = 0;
      break;
    case 3:
      *x = x1, *y = y0 + r + s, *tx = 0, *ty = 1;
      break;
    case 5:
      *x = x1 - r - s, *y = y1, *tx = -1, *ty = 0;
      break;
    default:
      *x = x0, *y = y1 - r - s, *tx = 0, *ty = -1;
      break;
  }
}

void awra_paint_dashed_rbox(AwraDraw *d, AwraBox b, float radius, float width, const AwraRGBA *color)
{
  if (!color || color->a <= 0.0f || width <= 0.0f) {
    return;
  }
  DashPath p;
  const float hw = width * 0.5f;
  p.x0 = b.x + hw;
  p.y0 = b.y + hw;
  p.w = b.w - width;
  p.h = b.h - width;
  p.r = fmaxf(0.0f, fminf(radius - hw, fminf(p.w, p.h) * 0.5f));
  if (p.w <= 0.0f || p.h <= 0.0f) {
    return;
  }
  const float arc = (float)M_PI * 0.5f * p.r;
  const float sides[4] = {p.w - 2 * p.r, p.h - 2 * p.r, p.w - 2 * p.r, p.h - 2 * p.r};
  float total = 0.0f;
  for (int i = 0; i < 4; i++) {
    p.seg[2 * i] = arc;
    p.seg[2 * i + 1] = sides[i];
    total += arc + sides[i];
  }
  /* set_stroke_style */
  float on, off;
  const double n = total / width;
  if (n == nearbyint(n)) {
    on = width;
    off = 2 * width;
  }
  else {
    const double k = round(n / 3.0);
    on = k ? (float)(total / (3.0 * k)) : 1.0f;
    off = 2 * on;
  }
  /* the raster's colour: straight, the draw's opacity in its alpha (paint.c) */
  const float c[4] = {color->r, color->g, color->b, color->a * d->opacity};
  for (float s0 = 0.0f; s0 < total; s0 += on + off) {
    const float s1 = fminf(s0 + on, total);
    /* the centre line sampled every half pixel at most */
    const int steps = BK_CLAMP((int)ceilf((s1 - s0) * 2.0f), 1, 256);
    float pts[2 * (256 + 1) + 4][2];
    int np = 0;
    float out[257][2], in[257][2];
    for (int k = 0; k <= steps; k++) {
      const float s = s0 + (s1 - s0) * (float)k / (float)steps;
      float x, y, tx, ty;
      dash_point(&p, s, &x, &y, &tx, &ty);
      if (k == 0) { /* square cap */
        x -= tx * hw;
        y -= ty * hw;
      }
      else if (k == steps) {
        x += tx * hw;
        y += ty * hw;
      }
      /* the normal, outwards (left of a clockwise path, y down) */
      out[k][0] = x + ty * hw;
      out[k][1] = y - tx * hw;
      in[k][0] = x - ty * hw;
      in[k][1] = y + tx * hw;
    }
    for (int k = 0; k <= steps; k++) {
      pts[np][0] = out[k][0];
      pts[np++][1] = out[k][1];
    }
    for (int k = steps; k >= 0; k--) {
      pts[np][0] = in[k][0];
      pts[np++][1] = in[k][1];
    }
    SR_polygon(d->r, (const float(*)[2])pts, np, c);
  }
}
