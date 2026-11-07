/* Awra 2 painting: primitives in the current opacity, and Awra's materials
 * (Awra 1's AwraMaterialResolver + AwraSurface snapshot, same values).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

static void col(const AwraDraw *d, const AwraRGBA *c, float out[4])
{
  out[0] = c->r;
  out[1] = c->g;
  out[2] = c->b;
  out[3] = c->a * d->opacity;
}

void awra_paint_rect(AwraDraw *d, AwraBox b, const AwraRGBA *color)
{
  if (!color || color->a <= 0.0f || b.w <= 0 || b.h <= 0) {
    return;
  }
  float c[4];
  col(d, color, c);
  BUI_raster_fill_rect(d->r, b.x, b.y, b.w, b.h, c);
}

void awra_paint_rbox4(AwraDraw *d, AwraBox b, const float radii[4], const AwraRGBA *fill,
                      const float widths[4], const AwraRGBA *border)
{
  float f[4], o[4];
  if (fill) {
    col(d, fill, f);
  }
  if (border) {
    col(d, border, o);
  }
  BUI_raster_rbox(d->r, b.x, b.y, b.w, b.h, radii, fill ? f : NULL, widths,
                  border ? o : NULL);
}

void awra_paint_rbox(AwraDraw *d, AwraBox b, float radius, const AwraRGBA *fill,
                     float border_width, const AwraRGBA *border)
{
  const float r4[4] = {radius, radius, radius, radius};
  const float w4[4] = {border_width, border_width, border_width, border_width};
  awra_paint_rbox4(d, b, r4, fill, border_width > 0 ? w4 : NULL,
                   border_width > 0 ? border : NULL);
}

void awra_paint_inset_shadow(AwraDraw *d, AwraBox b, float radius, float dx, float dy, float spread,
                             float blur, const AwraRGBA *color)
{
  if (!color || color->a <= 0.0f) {
    return;
  }
  float c[4];
  col(d, color, c);
  const float r4[4] = {radius, radius, radius, radius};
  BUI_raster_box_shadow_inset(d->r, b.x, b.y, b.w, b.h, r4, dx, dy, spread, blur, c);
}

void awra_paint_shadow(AwraDraw *d, AwraBox b, float radius, float dx, float dy, float spread,
                       float blur, const AwraRGBA *color, bool clip_out)
{
  if (!color || color->a <= 0.0f) {
    return;
  }
  float c[4];
  col(d, color, c);
  const float r4[4] = {radius, radius, radius, radius};
  BUI_raster_box_shadow(d->r, b.x, b.y, b.w, b.h, r4, dx, dy, spread, blur, c, clip_out);
}

void awra_paint_circle(AwraDraw *d, float cx, float cy, float r, const AwraRGBA *fill)
{
  if (!fill || fill->a <= 0.0f) {
    return;
  }
  float c[4];
  col(d, fill, c);
  BUI_raster_circle(d->r, cx, cy, r, 0.0f, c);
}

void awra_paint_line(AwraDraw *d, float x0, float y0, float x1, float y1, float width,
                     const AwraRGBA *color)
{
  if (!color || color->a <= 0.0f) {
    return;
  }
  float c[4];
  col(d, color, c);
  BUI_raster_line(d->r, x0, y0, x1, y1, width, c);
}

/* cairo_stroke of an open polyline with round caps and round joins
 * (CAIRO_LINE_CAP_ROUND, CAIRO_LINE_JOIN_ROUND): the outline of the stroke
 * filled once, so the segments don't overlap where they meet (two lines
 * and dots would be darker at the joints). */
void awra_paint_polyline(AwraDraw *d, const float (*pts)[2], int n, float width, const AwraRGBA *color)
{
  if (!color || color->a <= 0.0f || n < 2 || n > 16) {
    return;
  }
  enum { ARC = 12 }; /* segments per half turn */
  float out[16 * 2 * (ARC + 2) + 8][2];
  int k = 0;
  const float h = width * 0.5f;
  float nx[16], ny[16]; /* each segment's left normal */
  for (int i = 0; i + 1 < n; i++) {
    float dx = pts[i + 1][0] - pts[i][0], dy = pts[i + 1][1] - pts[i][1];
    const float l = sqrtf(dx * dx + dy * dy);
    if (l <= 0.0f) {
      return;
    }
    nx[i] = -dy / l;
    ny[i] = dx / l;
  }
  /* an arc around c from angle a0 turning by `sweep` (radians) */
#define ARC_TO(cx, cy, a0, sweep)                                              \
  do {                                                                         \
    const int steps_ = BK_MAX(1, (int)ceilf(fabsf(sweep) / (float)M_PI * ARC)); \
    for (int s_ = 0; s_ <= steps_; s_++) {                                     \
      const float t_ = (a0) + (sweep) * (float)s_ / (float)steps_;             \
      out[k][0] = (cx) + h * cosf(t_);                                         \
      out[k][1] = (cy) + h * sinf(t_);                                         \
      k++;                                                                     \
    }                                                                          \
  } while (0)
  /* one side forward (left normal), the other back, joined by the caps */
  for (int side = 0; side < 2; side++) {
    const float sg = side == 0 ? 1.0f : -1.0f;
    for (int j = 0; j + 1 < n; j++) {
      const int i = side == 0 ? j : n - 2 - j; /* segment */
      const int a = side == 0 ? i : i + 1, b = side == 0 ? i + 1 : i;
      const float ox = sg * nx[i] * h, oy = sg * ny[i] * h;
      if (j == 0) {
        out[k][0] = pts[a][0] + ox;
        out[k][1] = pts[a][1] + oy;
        k++;
      }
      if (j + 2 < n) { /* the joint at b with the next segment of this side */
        const int i2 = side == 0 ? i + 1 : i - 1;
        const float ox2 = sg * nx[i2] * h, oy2 = sg * ny[i2] * h;
        /* this side is the outer one when the next segment turns away
         * from it */
        const int c2 = side == 0 ? i + 2 : i - 1;
        const float nxd = pts[c2][0] - pts[b][0], nyd = pts[c2][1] - pts[b][1];
        const bool outer = nxd * ox + nyd * oy < 0.0f;
        if (outer) {
          float a0 = atan2f(oy, ox), a1 = atan2f(oy2, ox2);
          float sweep = a1 - a0;
          while (sweep > (float)M_PI) {
            sweep -= 2.0f * (float)M_PI;
          }
          while (sweep < -(float)M_PI) {
            sweep += 2.0f * (float)M_PI;
          }
          ARC_TO(pts[b][0], pts[b][1], a0, sweep);
        }
        else { /* inner: where the two offset edges cross */
          const float px1 = pts[a][0] + ox, py1 = pts[a][1] + oy;
          const float px2 = pts[c2][0] + ox2, py2 = pts[c2][1] + oy2;
          const float ex1 = pts[b][0] - pts[a][0], ey1 = pts[b][1] - pts[a][1];
          const float ex2 = pts[b][0] - pts[c2][0], ey2 = pts[b][1] - pts[c2][1];
          const float den = ex1 * ey2 - ey1 * ex2;
          if (fabsf(den) > 1e-6f) {
            const float t = ((px2 - px1) * ey2 - (py2 - py1) * ex2) / den;
            out[k][0] = px1 + t * ex1;
            out[k][1] = py1 + t * ey1;
          }
          else {
            out[k][0] = pts[b][0] + ox;
            out[k][1] = pts[b][1] + oy;
          }
          k++;
        }
      }
      else { /* the end: round cap half a turn around it */
        out[k][0] = pts[b][0] + ox;
        out[k][1] = pts[b][1] + oy;
        k++;
        ARC_TO(pts[b][0], pts[b][1], atan2f(oy, ox), -(float)M_PI);
      }
    }
  }
#undef ARC_TO
  float c[4];
  col(d, color, c);
  BUI_raster_polygon(d->r, (const float(*)[2])out, k, c);
}

/* Keyboard focus: a 2px accent ring just outside the widget's shape
 * (awra.css: outline: 2px solid @awra_focus_ring; outline-offset: 2px). */
void awra_paint_focus_ring(AwraDraw *d, AwraBox b, float radius)
{
  const float w = awra_pxf(2), off = awra_pxf(2);
  AwraBox o = {b.x - off - w, b.y - off - w, b.w + 2 * (off + w), b.h + 2 * (off + w)};
  awra_paint_rbox(d, o, radius + off + w, NULL, w, &awra_tokens()->focus_ring);
}

/* ----------------------------------------------------------------------- */
/* Materials */

void awra_material_resolve(AwraMaterial m, int role, AwraElevation elevation, bool blur_available,
                           AwraResolvedMaterial *r)
{
  const AwraTokens *t = awra_tokens();
  memset(r, 0, sizeof(*r));
  const float shadow_alpha = elevation == AWRA_ELEVATION_RAISED     ? 0.08f :
                             elevation == AWRA_ELEVATION_FLOATING   ? 0.16f :
                             elevation >= AWRA_ELEVATION_MODAL      ? 0.22f :
                                                                       0.0f;
  r->fill = t->material_tint[m];
  r->border = t->outline;
  r->highlight = t->rim;
  r->shadow = awra_rgba(0.008f, 0.007f, 0.012f, shadow_alpha);
  r->radius = t->material_radius[m];
  r->border_width = t->border_width;
  r->highlight_width = 0.75f;
  const float grain = t->material_grain[m];
  const bool reduced = awra_reduced_transparency();
  const bool profile_blur = t->material_blur[m];
  switch (m) {
    case AWRA_MATERIAL_CANVAS:
      r->border_edges = AWRA_EDGE_ALL;
      r->highlight_edges = AWRA_EDGE_TOP | AWRA_EDGE_START;
      r->grain_opacity = 0.016f * grain;
      break;
    case AWRA_MATERIAL_CONTENT:
      break;
    case AWRA_MATERIAL_CHROME:
      r->border = t->separator;
      r->border_edges = role == AWRA_ROLE_SIDEBAR ? AWRA_EDGE_END : AWRA_EDGE_BOTTOM;
      r->highlight.a *= 0.34f;
      r->highlight_edges = AWRA_EDGE_TOP;
      break;
    case AWRA_MATERIAL_LAYER:
      if (t->high_contrast) {
        r->border_edges = AWRA_EDGE_ALL;
      }
      r->highlight.a *= 0.30f;
      r->highlight_edges = AWRA_EDGE_TOP;
      break;
    case AWRA_MATERIAL_FLOATING:
      r->border_edges = AWRA_EDGE_ALL;
      r->highlight_edges = AWRA_EDGE_TOP | AWRA_EDGE_START;
      r->highlight_width = 1.0f;
      r->grain_opacity = 0.010f * grain;
      break;
    case AWRA_MATERIAL_OPAQUE:
      r->fill.a = 1.0f;
      r->border_edges = role == AWRA_ROLE_CONTENT ? 0 : AWRA_EDGE_ALL;
      break;
    case AWRA_MATERIAL_SOLID:
      r->border_edges = AWRA_EDGE_ALL;
      r->fill.a = 1.0f;
      break;
    case AWRA_MATERIAL_TRANSLUCENT:
      r->border_edges = AWRA_EDGE_ALL;
      if (reduced) {
        r->fill.a = 1.0f;
        r->fallback = true;
      }
      break;
    case AWRA_MATERIAL_FROSTED:
      r->border_edges = AWRA_EDGE_ALL;
      r->highlight_edges = AWRA_EDGE_TOP | AWRA_EDGE_START;
      r->grain_opacity = 0.014f * grain;
      break;
    default:
      break;
  }
  if (m == AWRA_MATERIAL_CANVAS || m == AWRA_MATERIAL_FLOATING || m == AWRA_MATERIAL_FROSTED) {
    if (profile_blur && blur_available && !reduced && !t->high_contrast &&
        awra_native_blur_enabled())
    {
      r->request_blur = true;
    }
    else {
      r->fill.a = 1.0f;
      if (m == AWRA_MATERIAL_FROSTED) {
        r->border.a = fmaxf(r->border.a, 0.48f);
      }
      r->fallback = true;
    }
  }
  else if (reduced && m != AWRA_MATERIAL_CONTENT && m != AWRA_MATERIAL_OPAQUE &&
           m != AWRA_MATERIAL_SOLID)
  {
    r->fill.a = 1.0f;
    r->fallback = true;
  }
  if (t->high_contrast) {
    r->border.a = fminf(1.0f, r->border.a);
  }
}

/* The grain: Awra 1's deterministic 96x96 xorshift texture (alpha 0..63),
 * premultiplied gray, tiled. Drawn only where it can change a pixel: at the
 * default calibration its contribution is below half an 8-bit level. */
static uint32_t *g_grain;

static const uint32_t *grain_texture(void)
{
  if (!g_grain) {
    g_grain = (uint32_t *)malloc(sizeof(uint32_t) * 96 * 96);
    if (!g_grain) {
      return NULL;
    }
    uint32_t s = 0x61777261u;
    for (int i = 0; i < 96 * 96; i++) {
      s ^= s << 13;
      s ^= s >> 17;
      s ^= s << 5;
      const uint32_t v = s & 0x3f;
      g_grain[i] = v << 24 | v << 16 | v << 8 | v;
    }
  }
  return g_grain;
}

static void paint_grain(AwraDraw *d, AwraBox b, const float radii[4], float opacity)
{
  if (opacity * d->opacity * 63.0f < 0.5f) {
    return; /* invisible at 8 bits per channel */
  }
  const uint32_t *tex = grain_texture();
  if (!tex) {
    return;
  }
  const BuiPixmap pm = {tex, 96, 96, 0x6772616961ull, BUI_PIXMAP_ARGB, 0};
  for (float y = b.y; y < b.y + b.h; y += 96) {
    for (float x = b.x; x < b.x + b.w; x += 96) {
      const float w = fminf(96, b.x + b.w - x), h = fminf(96, b.y + b.h - y);
      const float src[4] = {0, 0, w, h};
      BUI_raster_image_rounded(d->r, x, y, w, h, &pm, src, opacity * d->opacity, radii);
    }
  }
}

void awra_paint_material(AwraDraw *d, AwraBox b, const AwraResolvedMaterial *m, float radius,
                         int corner_mask, int edge_mask, AwraElevation elevation)
{
  if (b.w <= 0 || b.h <= 0) {
    return;
  }
  const float r = radius;
  const float radii[4] = {(corner_mask & 1) ? r : 0, (corner_mask & 2) ? r : 0,
                          (corner_mask & 4) ? r : 0, (corner_mask & 8) ? r : 0};
  /* Awra 1's elevation shadow is clipped away by AwraSurface's own
   * overflow:hidden (GTK clips a widget's snapshot to its box), so it never
   * shows; AWRA_SHADOWS=1 draws the intent anyway. */
  static int shadows = -1;
  if (shadows < 0) {
    const char *e = getenv("AWRA_SHADOWS");
    shadows = e && e[0] == '1';
  }
  /* the shadow, outside the rounded box: within the widget's own box (GTK
   * clips a widget's snapshot to it) it shows only past the rounded
   * corners; AWRA_SHADOWS=1 draws all of it */
  if (elevation > AWRA_ELEVATION_FLAT && m->shadow.a > 0.0f &&
      (shadows || radii[0] > 0 || radii[1] > 0 || radii[2] > 0 || radii[3] > 0))
  {
    const float off = elevation == AWRA_ELEVATION_RAISED ? 1.5f :
                      elevation == AWRA_ELEVATION_FLOATING ? 7.0f : 12.0f;
    const float blur = elevation == AWRA_ELEVATION_RAISED ? 8.0f :
                       elevation == AWRA_ELEVATION_FLOATING ? 20.0f : 32.0f;
    float c[4] = {m->shadow.r, m->shadow.g, m->shadow.b, m->shadow.a * d->opacity};
    if (!shadows) {
      bk_clip_push((int)floorf(b.x), (int)floorf(b.y), (int)ceilf(b.w), (int)ceilf(b.h));
    }
    BUI_raster_box_shadow(d->r, b.x, b.y, b.w, b.h, radii, 0, awra_pxf(off), 0, awra_pxf(blur),
                          c, 1);
    if (!shadows) {
      bk_clip_pop();
    }
  }
  /* fill */
  awra_paint_rbox4(d, b, radii, &m->fill, NULL, NULL);
  if (m->grain_opacity > 0.0f) {
    paint_grain(d, b, radii, m->grain_opacity);
  }
  awra_paint_material_edges(d, b, m, radius, corner_mask, edge_mask);
}

/* The outline, then the rim highlight on its edges (start and end: right
 * and left in a right-to-left widget), as AwraSurface draws them after its
 * child: a surface with content paints its material with edge_mask 0 and
 * these over its content. */
void awra_paint_material_edges(AwraDraw *d, AwraBox b, const AwraResolvedMaterial *m, float radius,
                               int corner_mask, int edge_mask)
{
  if (b.w <= 0 || b.h <= 0) {
    return;
  }
  const float r = radius;
  const float radii[4] = {(corner_mask & 1) ? r : 0, (corner_mask & 2) ? r : 0,
                          (corner_mask & 4) ? r : 0, (corner_mask & 8) ? r : 0};
  const int be = m->border_edges & edge_mask, he = m->highlight_edges & edge_mask;
  const int right = d->rtl ? AWRA_EDGE_START : AWRA_EDGE_END;
  const int left = d->rtl ? AWRA_EDGE_END : AWRA_EDGE_START;
  if (be) {
    const float bw = awra_pxf(m->border_width);
    const float w4[4] = {(be & AWRA_EDGE_TOP) ? bw : 0, (be & right) ? bw : 0,
                         (be & AWRA_EDGE_BOTTOM) ? bw : 0, (be & left) ? bw : 0};
    awra_paint_rbox4(d, b, radii, NULL, w4, &m->border);
  }
  if (he && m->highlight.a > 0.0f) {
    const float hw = awra_pxf(m->highlight_width);
    const float w4[4] = {(he & AWRA_EDGE_TOP) ? hw : 0, (he & right) ? hw : 0,
                         (he & AWRA_EDGE_BOTTOM) ? hw : 0, (he & left) ? hw : 0};
    awra_paint_rbox4(d, b, radii, NULL, w4, &m->highlight);
  }
}
