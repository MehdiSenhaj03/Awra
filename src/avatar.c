/* Awra 2 avatars (Awra 1's AwraAvatar, AwraAvatarGroup): a picture, or
 * initials on a plate of the name's own hue, a presence dot cut into the
 * bottom end corner, a speaking ring; groups overlap avatars by a third.
 *
 * The face is composed once into a small buffer at its drawn size (picture
 * cropped to cover, plate and initials, hairline, the rounded shape minus
 * the presence cutout and the next avatar of a group), then drawn as an
 * image until something it shows changes.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

#include <stdio.h>

#include "GPU_raster.h"

#define DEFAULT_SIZE 32
#define DEFAULT_ICON "awra-avatar-default-symbolic"
#define PULSE_MS 1200.0

static const double avatar_hues[] = {4, 22, 38, 145, 168, 188, 205, 222, 240, 262, 284, 318, 338};

typedef struct AvatarExt {
  int size; /* logical */
  char *text, *initials, *icon_name;
  int hue;
  bool show_initials;
  AwraTexture *image;
  int shape; /* 0 circle, 1 rounded */
  int presence, shown_presence;
  bool speaking;
  float presence_scale, speaking_reveal, pulse;
  /* a group's next avatar: a circle cut out of this one (logical, local) */
  bool has_cut;
  float cut_x, cut_y, cut_r;
  /* a rounded box cut out of it (a rail's mention badge), logical, local */
  bool has_cut_rect;
  float cut_rect[5]; /* x y w h radius */
  float radius_override; /* < 0: the shape's */
  /* the composed face */
  uint32_t *face;
  int face_px;
  uint64_t face_key;
  bool face_dirty;
  unsigned style_gen;
  float face_scale;
  unsigned pulse_timer;
  double pulse_start;
} AvatarExt;

#define AVX(w) AWRA_EXT(w, AvatarExt)

static uint64_t g_face_keys = 1;

static AwraRGBA hsl(double hue, double s, double l)
{
  const double c = (1.0 - fabs(2.0 * l - 1.0)) * s;
  const double sector = fmod(hue / 60.0, 6.0);
  const double x = c * (1.0 - fabs(fmod(sector, 2.0) - 1.0));
  const double m = l - c / 2.0;
  double r = 0, g = 0, b = 0;
  if (sector < 1) {
    r = c;
    g = x;
  }
  else if (sector < 2) {
    r = x;
    g = c;
  }
  else if (sector < 3) {
    g = c;
    b = x;
  }
  else if (sector < 4) {
    g = x;
    b = c;
  }
  else if (sector < 5) {
    r = x;
    b = c;
  }
  else {
    r = c;
    b = x;
  }
  return awra_rgba((float)(r + m), (float)(g + m), (float)(b + m), 1.0f);
}

static int hue_for_text(const char *text)
{
  uint32_t hash = 2166136261u; /* FNV-1a */
  for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
    hash ^= *p;
    hash *= 16777619u;
  }
  return (int)(hash % (sizeof(avatar_hues) / sizeof(avatar_hues[0])));
}

static void face_colors(AvatarExt *x, AwraRGBA *bg, AwraRGBA *fg)
{
  const AwraTokens *t = awra_tokens();
  if (!x->text || !x->text[0]) {
    *bg = awra_with_alpha(t->foreground, t->dark ? 0.14f : 0.10f);
    *fg = t->muted;
    return;
  }
  const double hue = avatar_hues[x->hue];
  if (t->dark) {
    *bg = hsl(hue, 0.48, t->high_contrast ? 0.26 : 0.35);
    *fg = hsl(hue, 0.85, t->high_contrast ? 0.94 : 0.87);
  }
  else {
    *bg = hsl(hue, 0.70, t->high_contrast ? 0.90 : 0.86);
    *fg = hsl(hue, 0.60, t->high_contrast ? 0.20 : 0.30);
  }
}

static int utf8_next(const char *s, int i)
{
  const unsigned char c = (unsigned char)s[i];
  return i + (c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1);
}

static bool is_alnum_at(const char *s, int i)
{
  const unsigned char c = (unsigned char)s[i];
  if (c < 0x80) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  }
  /* letters beyond ASCII count as letters; symbols and emoji (U+2000 and
   * up, outside the letter blocks) do not */
  if (c >= 0xF0) {
    return false;
  }
  if (c >= 0xE2 && c <= 0xE3) {
    return false;
  }
  return true;
}

static void upcase_ascii(char *s)
{
  for (; *s; s++) {
    if (*s >= 'a' && *s <= 'z') {
      *s = (char)(*s - 'a' + 'A');
    }
  }
}

/* the first letter of the first and the last word */
static char *compute_initials(const char *text)
{
  if (!text || !text[0]) {
    return NULL;
  }
  int first = -1, last = -1;
  bool at_start = true;
  for (int i = 0; text[i]; i = utf8_next(text, i)) {
    const char c = text[i];
    if (is_alnum_at(text, i)) {
      if (at_start) {
        if (first < 0) {
          first = i;
        }
        else {
          last = i;
        }
      }
      at_start = false;
    }
    else if (c == ' ' || c == '\t' || c == '-' || c == '_' || c == '.') {
      at_start = true;
    }
  }
  if (first < 0) {
    if (text[0] == ' ') {
      return NULL;
    }
    first = 0;
  }
  char buf[16];
  int n = utf8_next(text, first) - first;
  memcpy(buf, text + first, (size_t)n);
  if (last >= 0) {
    const int m = utf8_next(text, last) - last;
    memcpy(buf + n, text + last, (size_t)m);
    n += m;
  }
  buf[n] = 0;
  upcase_ascii(buf);
  return awra_strdup(buf);
}

static float corner_radius(const AvatarExt *x)
{
  if (x->radius_override >= 0.0f) {
    return fminf(x->radius_override, x->size / 2.0f);
  }
  return x->shape == 0 ? x->size / 2.0f : x->size * 0.26f;
}

static float dot_diameter(const AvatarExt *x)
{
  return fmaxf(6.0f, roundf(x->size * 0.3f));
}

static float cutout_width(const AvatarExt *x)
{
  return fmaxf(1.5f, x->size * 0.07f);
}

/* the bottom end corner: bottom left right to left (Awra 1's dot_center) */
static void dot_center(const AwraWidget *w, const AvatarExt *x, float *cx, float *cy)
{
  const float r = dot_diameter(x) / 2.0f;
  const float inset = x->shape == 0 ? r : r - 1.0f;
  *cx = awra_widget_rtl(w) ? inset : x->size - inset;
  *cy = x->size - inset;
}

/* ----------------------------------------------------------------------- */
/* Offscreen composition */

static BuiRaster *g_scratch;

void awra_paint_offscreen(uint32_t *px, int w, int h, void (*fn)(AwraDraw *d, void *data), void *data);
void awra_paint_offscreen(uint32_t *px, int w, int h, void (*fn)(AwraDraw *d, void *data), void *data)
{
  if (!g_scratch) {
    g_scratch = BUI_raster_create(w, h);
    BUI_raster_set_diff(g_scratch, 0);
  }
  memset(px, 0, sizeof(uint32_t) * (size_t)w * h);
  BUI_raster_set_target(g_scratch, px, w, h);
  BUI_raster_begin_frame(g_scratch, w, h);
  BUI_raster_set_clip(g_scratch, 0, 0, w, h);
  AwraDraw d = {g_scratch, 1.0f};
  fn(&d, data);
  BUI_raster_end_frame(g_scratch);
  BUI_raster_set_target(g_scratch, NULL, 0, 0);
}

/* coverage of a point in a rounded box (anti-aliased by distance) */
static float rbox_coverage(float px, float py, float x, float y, float w, float h, float r)
{
  const float cx = fminf(fmaxf(px, x + r), x + w - r);
  const float cy = fminf(fmaxf(py, y + r), y + h - r);
  const float dx = px - cx, dy = py - cy;
  const float d = sqrtf(dx * dx + dy * dy) - r;
  /* inside the straight edges */
  if (r <= 0.0f) {
    const float ex = fminf(px - x, x + w - px), ey = fminf(py - y, y + h - py);
    return fminf(1.0f, fmaxf(0.0f, fminf(ex, ey) + 0.5f));
  }
  if (px >= x + r && px <= x + w - r) {
    const float e = fminf(py - y, y + h - py);
    return fminf(1.0f, fmaxf(0.0f, e + 0.5f));
  }
  if (py >= y + r && py <= y + h - r) {
    const float e = fminf(px - x, x + w - px);
    return fminf(1.0f, fmaxf(0.0f, e + 0.5f));
  }
  return fminf(1.0f, fmaxf(0.0f, 0.5f - d));
}

static float circle_coverage(float px, float py, float cx, float cy, float r)
{
  const float d = sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy)) - r;
  return fminf(1.0f, fmaxf(0.0f, 0.5f - d));
}

typedef struct FaceCtx {
  AwraWidget *w;
  int s; /* raster px */
} FaceCtx;

static void draw_face_cb(AwraDraw *d, void *data)
{
  FaceCtx *fc = (FaceCtx *)data;
  AvatarExt *x = AVX(fc->w);
  const float s = (float)fc->s;
  const float scale = s / x->size;
  if (x->image) {
    awra_paint_texture(d, x->image, (AwraBox){0, 0, s, s}, 0.0f, 2);
  }
  else {
    AwraRGBA bg, fg;
    face_colors(x, &bg, &fg);
    awra_paint_rect(d, (AwraBox){0, 0, s, s}, &bg);
    if (x->show_initials && x->initials) {
      AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
      int chars = 0;
      for (int i = 0; x->initials[i]; i = utf8_next(x->initials, i)) {
        chars++;
      }
      f.size = x->size * (chars > 1 ? 0.38f : 0.44f) * scale;
      f.weight = 600;
      f.letter_spacing = 0;
      const float tw = awra_text_width(&f, x->initials, -1);
      /* Pango's ink box of capitals: the cap height above the baseline */
      const float cap = f.size * 0.729f;
      const float bx = roundf((s - tw) / 2.0f);
      const float base = roundf((s - cap) / 2.0f) + cap;
      awra_draw_text(d, &f, bx, base, x->initials, -1, &fg);
    }
    else {
      const int px = BK_MAX(8, (int)roundf(x->size * 0.56f * scale));
      const float o = roundf((s - px) / 2.0f);
      awra_paint_icon(d, x->icon_name ? x->icon_name : DEFAULT_ICON, o, o, px, &fg);
    }
  }
  if (x->image || awra_tokens()->high_contrast) {
    /* a hairline keeps a pale picture from dissolving into a pale canvas */
    AwraRGBA line = awra_with_alpha(awra_tokens()->foreground, 0.09f);
    awra_paint_rbox(d, (AwraBox){0, 0, s, s}, corner_radius(x) * scale, NULL, scale, &line);
  }
}

static void compose_face(AwraWidget *w, int s)
{
  AvatarExt *x = AVX(w);
  if (x->face_px != s) {
    free(x->face);
    x->face = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)s * s);
    x->face_px = s;
  }
  if (!x->face) {
    return;
  }
  FaceCtx fc = {w, s};
  awra_paint_offscreen(x->face, s, s, draw_face_cb, &fc);
  /* the shape, minus the presence cutout and a group's next avatar */
  const float scale = (float)s / x->size;
  const float r = corner_radius(x) * scale;
  const bool cutout = x->shown_presence != AWRA_PRESENCE_NONE && x->presence_scale > 0.0f;
  float ccx = 0, ccy = 0, cr = 0;
  if (cutout) {
    dot_center(w, x, &ccx, &ccy);
    cr = (dot_diameter(x) / 2.0f + cutout_width(x)) * x->presence_scale * scale;
    ccx *= scale;
    ccy *= scale;
  }
  for (int py = 0; py < s; py++) {
    for (int px = 0; px < s; px++) {
      const float fx = px + 0.5f, fy = py + 0.5f;
      float cov = rbox_coverage(fx, fy, 0, 0, (float)s, (float)s, r);
      if (cutout && cov > 0.0f) {
        cov *= 1.0f - circle_coverage(fx, fy, ccx, ccy, cr);
      }
      if (x->has_cut_rect && cov > 0.0f) {
        const float *c = x->cut_rect;
        cov *= 1.0f - rbox_coverage(fx, fy, c[0] * scale, c[1] * scale, c[2] * scale, c[3] * scale, c[4] * scale);
      }
      if (x->has_cut && cov > 0.0f) {
        cov *= 1.0f - circle_coverage(fx, fy, x->cut_x * scale, x->cut_y * scale, x->cut_r * scale);
      }
      uint32_t *p = &x->face[(size_t)py * s + px];
      if (cov >= 0.999f) {
        continue;
      }
      if (cov <= 0.001f) {
        *p = 0;
        continue;
      }
      const uint32_t c = *p;
      const uint32_t a = (uint32_t)((c >> 24) * cov + 0.5f), rr = (uint32_t)(((c >> 16) & 255) * cov + 0.5f),
                     g = (uint32_t)(((c >> 8) & 255) * cov + 0.5f), b = (uint32_t)((c & 255) * cov + 0.5f);
      *p = (a << 24) | (rr << 16) | (g << 8) | b;
    }
  }
  x->face_key = (1ull << 62) | g_face_keys++;
  x->face_dirty = false;
  x->style_gen = awra_style_generation();
}

static void face_changed(AwraWidget *w)
{
  AVX(w)->face_dirty = true;
  awra_invalidate(w);
}

/* ----------------------------------------------------------------------- */
/* Drawing */

static void avatar_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  (void)o;
  (void)for_size;
  *r_min = *r_nat = awra_px(AVX(w)->size);
}

static void avatar_draw(AwraWidget *w, AwraDraw *d)
{
  AvatarExt *x = AVX(w);
  const int s = awra_px(x->size);
  if (s <= 0) {
    return;
  }
  const float ox = w->x + roundf((w->w - s) / 2.0f), oy = w->y + roundf((w->h - s) / 2.0f);
  const float scale = (float)s / x->size;
  /* the speaking ring emerges from the edge, then breathes */
  if (x->speaking_reveal > 0.0f) {
    const float size = (float)x->size;
    const float gapw = fmaxf(1.5f, size * 0.05f);
    const float width = fmaxf(2.0f, size * 0.055f);
    const float spread = gapw * x->speaking_reveal + width + x->pulse * fmaxf(0.75f, size * 0.025f);
    const float opacity = x->speaking_reveal * (0.72f + 0.28f * (1.0f - x->pulse));
    AwraRGBA c = awra_with_alpha(awra_tokens()->accent, opacity);
    awra_paint_rbox(d, (AwraBox){ox - spread * scale, oy - spread * scale, s + 2 * spread * scale, s + 2 * spread * scale},
                    (corner_radius(x) + spread) * scale, NULL, width * scale, &c);
  }
  if (x->face_dirty || x->face_px != s || x->style_gen != awra_style_generation()) {
    compose_face(w, s);
  }
  if (x->face) {
    const BuiPixmap pm = {x->face, s, s, x->face_key, BUI_PIXMAP_ARGB, 0};
    BUI_raster_image(d->r, ox, oy, (float)s, (float)s, &pm, NULL, d->opacity, 0);
  }
  if (x->shown_presence != AWRA_PRESENCE_NONE && x->presence_scale > 0.0f) {
    const AwraTokens *t = awra_tokens();
    float cx, cy;
    dot_center(w, x, &cx, &cy);
    cx = ox + cx * scale;
    cy = oy + cy * scale;
    const float r = dot_diameter(x) / 2.0f * x->presence_scale * scale;
    AwraRGBA c;
    switch (x->shown_presence) {
      case AWRA_PRESENCE_ONLINE: c = t->success; break;
      case AWRA_PRESENCE_IDLE: c = t->warning; break;
      case AWRA_PRESENCE_BUSY: c = t->danger; break;
      default: c = t->muted; break;
    }
    if (x->shown_presence == AWRA_PRESENCE_ONLINE) {
      awra_paint_circle(d, cx, cy, r, &c);
      return;
    }
    /* shapes besides colour: a crescent, a bar, a ring; drawn as a mask */
    const int n = (int)ceilf(r * 2) + 2;
    const int x0 = (int)floorf(cx - r) - 1, y0 = (int)floorf(cy - r) - 1;
    uint8_t stackbuf[64 * 64];
    uint8_t *mask = n <= 64 ? stackbuf : (uint8_t *)malloc((size_t)n * n);
    if (!mask) {
      return;
    }
    for (int py = 0; py < n; py++) {
      for (int px = 0; px < n; px++) {
        const float fx = x0 + px + 0.5f, fy = y0 + py + 0.5f;
        float cov = circle_coverage(fx, fy, cx, cy, r);
        if (cov > 0.0f) {
          float hole = 0.0f;
          if (x->shown_presence == AWRA_PRESENCE_IDLE) {
            /* the crescent's bite faces the avatar's middle */
            hole = circle_coverage(fx, fy, cx + (awra_widget_rtl(w) ? 0.42f : -0.42f) * r, cy - 0.42f * r,
                                   r * 0.66f);
          }
          else if (x->shown_presence == AWRA_PRESENCE_BUSY) {
            hole = rbox_coverage(fx, fy, cx - r * 0.58f, cy - r * 0.2f, r * 1.16f, r * 0.4f, r * 0.2f);
          }
          else {
            hole = circle_coverage(fx, fy, cx, cy, r * 0.46f);
          }
          cov *= 1.0f - hole;
        }
        mask[py * n + px] = (uint8_t)(cov * 255.0f + 0.5f);
      }
    }
    float col[4] = {c.r, c.g, c.b, c.a * d->opacity};
    BUI_raster_mask(d->r, x0, y0, mask, n, n, n, col);
    if (mask != stackbuf) {
      free(mask);
    }
  }
}

static void avatar_finalize(AwraWidget *w)
{
  AvatarExt *x = AVX(w);
  free(x->text);
  free(x->initials);
  free(x->icon_name);
  free(x->face);
  awra_texture_unref(x->image);
  if (x->pulse_timer) {
    awra_source_remove(x->pulse_timer);
  }
}

static bool pulse_tick(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  AvatarExt *x = AVX(w);
  /* ease-in-out sine, alternating */
  const double t = fmod((awra_now_ms() - x->pulse_start) / PULSE_MS, 2.0);
  const double u = t < 1.0 ? t : 2.0 - t;
  x->pulse = (float)(-(cos(M_PI * u) - 1.0) / 2.0);
  awra_invalidate(w);
  return true;
}

static void update_pulse(AwraWidget *w)
{
  AvatarExt *x = AVX(w);
  const bool wanted = x->speaking && awra_aux(w)->mapped && !awra_style_reduced_motion();
  if (wanted && !x->pulse_timer) {
    x->pulse_start = awra_now_ms();
    x->pulse_timer = awra_timeout_add(33, pulse_tick, w, NULL);
  }
  else if (!wanted && x->pulse_timer) {
    awra_source_remove(x->pulse_timer);
    x->pulse_timer = 0;
    x->pulse = 0;
  }
}

static void avatar_map(AwraWidget *w, bool mapped)
{
  (void)mapped;
  update_pulse(w);
}

static void avatar_style_changed(AwraWidget *w)
{
  face_changed(w);
}

/* Awra 1's AwraAvatar: an image named by its person, described by their
 * presence and speaking */
static void avatar_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  AvatarExt *x = AVX(w);
  info->label = x->text && x->text[0] ? x->text : "Avatar";
  static const char *const presence[] = {NULL, "Online", "Idle", "Do not disturb", "Offline"};
  const char *p = x->presence > 0 && x->presence < 5 ? presence[x->presence] : NULL;
  if (p && x->speaking) {
    snprintf(info->buf, sizeof(info->buf), "%s, Speaking", p);
    info->description = info->buf;
  }
  else if (p) {
    info->description = p;
  }
  else if (x->speaking) {
    info->description = "Speaking";
  }
}

static AwraClass avatar_class = {
    .a11y = avatar_a11y,
    .bk = AWRA_BK("AwraAvatar", 0),
    .type = "AwraAvatar",
    .flags = AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(AvatarExt),
    .finalize = avatar_finalize,
    .measure = avatar_measure,
    .draw = avatar_draw,
    .map = avatar_map,
    .style_changed = avatar_style_changed,
    .overdraw = 8,
};

/* What it paints outside its box: the speaking ring at its widest (the
 * gap, the ring, the breath; see avatar_draw), or the class's 8 px. */
static void avatar_set_overdraw(AwraWidget *w)
{
  const float size = (float)AVX(w)->size;
  const float ring = fmaxf(1.5f, size * 0.05f) + fmaxf(2.0f, size * 0.055f) + fmaxf(0.75f, size * 0.025f);
  w->overdraw = (short)BK_MAX(awra_px(8), awra_px((int)ceilf(ring) + 1));
}

AwraWidget *awra_avatar_new(int size, const char *text)
{
  AwraWidget *w = awra_widget_new(&avatar_class);
  AvatarExt *x = AVX(w);
  x->size = size >= 8 ? size : DEFAULT_SIZE;
  x->show_initials = true;
  x->face_dirty = true;
  x->radius_override = -1.0f;
  avatar_set_overdraw(w);
  awra_avatar_set_text(w, text);
  return w;
}

void awra_avatar_set_size(AwraWidget *w, int size)
{
  AvatarExt *x = AVX(w);
  if (size < 8 || x->size == size) {
    return;
  }
  x->size = size;
  avatar_set_overdraw(w);
  face_changed(w);
  awra_widget_queue_resize(w);
}

int awra_avatar_size(AwraWidget *w)
{
  return AVX(w)->size;
}

void awra_avatar_set_text(AwraWidget *w, const char *text)
{
  AvatarExt *x = AVX(w);
  if ((x->text == NULL && text == NULL) || (x->text && text && !strcmp(x->text, text))) {
    return;
  }
  awra_set_str(&x->text, text);
  free(x->initials);
  x->initials = compute_initials(text);
  x->hue = text ? hue_for_text(text) : 0;
  if (!x->image) {
    face_changed(w);
  }
}

const char *awra_avatar_text(AwraWidget *w)
{
  return AVX(w)->text;
}

void awra_avatar_set_show_initials(AwraWidget *w, bool show)
{
  AVX(w)->show_initials = show;
  face_changed(w);
}

void awra_avatar_set_texture(AwraWidget *w, AwraTexture *texture)
{
  AvatarExt *x = AVX(w);
  if (x->image == texture) {
    return;
  }
  awra_texture_ref(texture);
  awra_texture_unref(x->image);
  x->image = texture;
  face_changed(w);
}

AwraTexture *awra_avatar_texture(AwraWidget *w)
{
  return AVX(w)->image;
}

void awra_avatar_set_icon_name(AwraWidget *w, const char *icon)
{
  awra_set_str(&AVX(w)->icon_name, icon);
  face_changed(w);
}

void awra_avatar_set_shape(AwraWidget *w, int shape)
{
  AvatarExt *x = AVX(w);
  if (x->shape != shape) {
    x->shape = shape;
    face_changed(w);
  }
}

static void presence_anim(AwraWidget *w, double t, void *data)
{
  AvatarExt *x = AVX(w);
  const int to = (int)(intptr_t)data;
  /* a new kind shrinks the old dot away, then grows */
  if (to != x->shown_presence) {
    x->presence_scale = (float)(1.0 - t * 2.0);
    if (x->presence_scale <= 0.0f) {
      x->shown_presence = to;
      x->presence_scale = 0.0f;
    }
  }
  else {
    x->presence_scale = (float)fmin(1.0, x->presence_scale + t);
  }
  face_changed(w);
}

void awra_avatar_set_presence(AwraWidget *w, AwraPresence presence)
{
  AvatarExt *x = AVX(w);
  if (x->presence == (int)presence) {
    return;
  }
  x->presence = presence;
  if (!awra_aux(w)->mapped || awra_style_reduced_motion()) {
    x->shown_presence = presence;
    x->presence_scale = presence == AWRA_PRESENCE_NONE ? 0.0f : 1.0f;
    face_changed(w);
    return;
  }
  if (x->shown_presence == AWRA_PRESENCE_NONE) {
    x->shown_presence = presence;
    x->presence_scale = 0.0f;
  }
  awra_anim_start(w, awra_motion_duration(AWRA_MOTION_NORMAL), presence_anim, (void *)(intptr_t)presence,
                  NULL);
}

static void speaking_anim(AwraWidget *w, double t, void *data)
{
  AvatarExt *x = AVX(w);
  float from;
  memcpy(&from, &data, sizeof(float));
  x->speaking_reveal = from + ((x->speaking ? 1.0f : 0.0f) - from) * (float)t;
  awra_invalidate(w);
}

void awra_avatar_set_speaking(AwraWidget *w, bool speaking)
{
  AvatarExt *x = AVX(w);
  if (x->speaking == speaking) {
    return;
  }
  x->speaking = speaking;
  update_pulse(w);
  if (!awra_aux(w)->mapped || awra_style_reduced_motion()) {
    x->speaking_reveal = speaking ? 1.0f : 0.0f;
    awra_invalidate(w);
    return;
  }
  void *from = NULL;
  memcpy(&from, &x->speaking_reveal, sizeof(float));
  awra_anim_start(w, awra_motion_duration(AWRA_MOTION_FAST), speaking_anim, from, NULL);
}

void awra_avatar_set_cut_internal(AwraWidget *w, bool cut, float cx, float cy, float r);
void awra_avatar_set_cut_internal(AwraWidget *w, bool cut, float cx, float cy, float r)
{
  AvatarExt *x = AVX(w);
  if (x->has_cut == cut && x->cut_x == cx && x->cut_y == cy && x->cut_r == r) {
    return;
  }
  x->has_cut = cut;
  x->cut_x = cx;
  x->cut_y = cy;
  x->cut_r = r;
  face_changed(w);
}

/* ======================================================================= */
/* Avatar group: each avatar overlaps the one before by a third, cut where
 * the next one and its 2 px gap go */

#define OVERLAP 0.34
#define GAP 2.0f

static int step_for(int size)
{
  return (int)(size * (1.0 - OVERLAP) + 0.5);
}

static void group_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  (void)for_size;
  int across = 0, along = 0, count = 0;
  for (int i = 0; i < w->nkids; i++) {
    AwraWidget *c = w->kids[i];
    if (!awra_child_visible(c)) {
      continue;
    }
    int mn, cw, ch;
    awra_measure(c, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &cw);
    awra_measure(c, AWRA_ORIENTATION_VERTICAL, -1, &mn, &ch);
    along = count == 0 ? cw : along + step_for(cw);
    across = BK_MAX(across, ch);
    count++;
  }
  *r_min = *r_nat = o == AWRA_ORIENTATION_HORIZONTAL ? along : across;
}

static void group_allocate(AwraWidget *w)
{
  int x = 0;
  AwraWidget *prev = NULL;
  int prev_x = 0, prev_y = 0;
  for (int i = 0; i < w->nkids; i++) {
    AwraWidget *c = w->kids[i];
    if (!awra_child_visible(c)) {
      continue;
    }
    int mn, cw, ch;
    awra_measure(c, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &cw);
    awra_measure(c, AWRA_ORIENTATION_VERTICAL, -1, &mn, &ch);
    const int y = (w->h - ch) / 2;
    awra_allocate(c, w->x + x, w->y + y, cw, ch);
    if (prev && awra_widget_is(prev, "AwraAvatar")) {
      /* the previous avatar loses the part where this one goes */
      const float s = awra_scale();
      const float r = cw / s / 2.0f + GAP;
      awra_avatar_set_cut_internal(prev, true, (x - prev_x + cw / 2.0f) / s, (y - prev_y + ch / 2.0f) / s, r);
    }
    prev = c;
    prev_x = x;
    prev_y = y;
    x += step_for(cw);
  }
  if (prev && awra_widget_is(prev, "AwraAvatar")) {
    awra_avatar_set_cut_internal(prev, false, 0, 0, 0);
  }
}

static AwraClass group_class = {
    .bk = AWRA_BK("AwraAvatarGroup", 0),
    .type = "AwraAvatarGroup",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .measure = group_measure,
    .allocate = group_allocate,
};

AwraWidget *awra_avatar_group_new(void)
{
  return awra_widget_new(&group_class);
}

void awra_avatar_group_append(AwraWidget *group, AwraWidget *avatar)
{
  awra_child_insert(group, avatar, -1);
}

void awra_avatar_group_remove_all(AwraWidget *group)
{
  while (group->nkids) {
    awra_child_remove(group, group->kids[group->nkids - 1]);
  }
}

void awra_avatar_set_radius_internal(AwraWidget *w, float radius);
void awra_avatar_set_radius_internal(AwraWidget *w, float radius)
{
  AvatarExt *x = AVX(w);
  if (x->radius_override != radius) {
    x->radius_override = radius;
    face_changed(w);
  }
}

void awra_avatar_set_cut_rect_internal(AwraWidget *w, bool cut, float rx, float ry, float rw, float rh, float r);
void awra_avatar_set_cut_rect_internal(AwraWidget *w, bool cut, float rx, float ry, float rw, float rh, float r)
{
  AvatarExt *x = AVX(w);
  const float v[5] = {rx, ry, rw, rh, r};
  if (x->has_cut_rect == cut && (!cut || !memcmp(v, x->cut_rect, sizeof(v)))) {
    return;
  }
  x->has_cut_rect = cut;
  memcpy(x->cut_rect, v, sizeof(v));
  face_changed(w);
}

/* A rounded box's coverage mask minus a rounded cutout, drawn in `color`
 * (raster px): shapes with a hole in them. */
void awra_paint_rbox_cut(AwraDraw *d, AwraBox b, float radius, AwraBox cut, float cut_radius,
                         const AwraRGBA *color);
void awra_paint_rbox_cut(AwraDraw *d, AwraBox b, float radius, AwraBox cut, float cut_radius,
                         const AwraRGBA *color)
{
  const int x0 = (int)floorf(b.x), y0 = (int)floorf(b.y);
  const int n_w = (int)ceilf(b.x + b.w) - x0, n_h = (int)ceilf(b.y + b.h) - y0;
  if (n_w <= 0 || n_h <= 0) {
    return;
  }
  uint8_t *mask = (uint8_t *)malloc((size_t)n_w * n_h);
  if (!mask) {
    return;
  }
  for (int py = 0; py < n_h; py++) {
    for (int px = 0; px < n_w; px++) {
      const float fx = x0 + px + 0.5f, fy = y0 + py + 0.5f;
      float cov = rbox_coverage(fx, fy, b.x, b.y, b.w, b.h, radius);
      if (cov > 0.0f && cut.w > 0) {
        cov *= 1.0f - rbox_coverage(fx, fy, cut.x, cut.y, cut.w, cut.h, cut_radius);
      }
      mask[py * n_w + px] = (uint8_t)(cov * 255.0f + 0.5f);
    }
  }
  float c[4] = {color->r, color->g, color->b, color->a * d->opacity};
  BUI_raster_mask(d->r, x0, y0, mask, n_w, n_h, n_w, c);
  free(mask);
}
