/* Awra 2 playback components: the video view (frames any thread produces,
 * shown without a copy), the frosted backdrop it lends to the surfaces
 * floating over it, the timeline, the overlay bar and the edge panel
 * (Awra 1's AwraTextureStream + GtkPicture, AwraSurface:backdrop-widget,
 * AwraTimeline, AwraOverlayBar and AwraEdgePanel).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

#include <stdio.h>

#ifdef _WIN32
#  include <windows.h>
typedef CRITICAL_SECTION VMutex;
static void vm_init(VMutex *m)
{
  InitializeCriticalSection(m);
}
static void vm_lock(VMutex *m)
{
  EnterCriticalSection(m);
}
static void vm_unlock(VMutex *m)
{
  LeaveCriticalSection(m);
}
static void vm_free(VMutex *m)
{
  DeleteCriticalSection(m);
}
#else
#  include <pthread.h>
typedef pthread_mutex_t VMutex;
static void vm_init(VMutex *m)
{
  pthread_mutex_init(m, NULL);
}
static void vm_lock(VMutex *m)
{
  pthread_mutex_lock(m);
}
static void vm_unlock(VMutex *m)
{
  pthread_mutex_unlock(m);
}
static void vm_free(VMutex *m)
{
  pthread_mutex_destroy(m);
}
#endif

void awra_window_place_overlay(AwraWidget *window, AwraWidget *child, double x, double y);

/* ======================================================================= */
/* Video view
 *
 * The producer (a decoder thread) writes each frame straight into one of a
 * few buffers the view owns, then presents it. The UI thread swaps the
 * newest presented frame in once per frame and damages only the view's
 * rectangle; the raster draws it fitted (contain / cover / fill), converting
 * NV12 only where it draws. No allocation happens per frame: buffers are
 * allocated when the format changes. */

enum { VB_FREE = 0, VB_WRITING, VB_PENDING, VB_SHOWN };

typedef struct VPool VPool;
typedef struct VBuf {
  VPool *pool;
  uint8_t *data;
  int state;
  bool discard;
  uint64_t key;
} VBuf;

struct VPool {
  int refs; /* the shared state while current, plus every buffer in use */
  int format, w, h;
  double par;
  int strides[2], lines[2];
  size_t offsets[2], size;
  int n; /* buffers, the last one is the discard buffer */
  VBuf bufs[];
};

typedef struct VShared {
  VMutex lock;
  int refs;
  AwraWidget *view; /* NULL once the view is gone */
  VPool *pool;
  VBuf *pending, *shown;
  bool posted;
  uint64_t next_key, stream;
  uint64_t presented, dropped;
} VShared;

typedef struct VideoExt {
  VShared *s;
  int fit; /* 0 fill, 1 contain, 2 cover */
  AwraRGBA background;
  bool custom_background;
  /* where the frame was last drawn (raster px) */
  float dx, dy, dw, dh;
  uint64_t drawn;
  /* the backdrop: a blurred, downscaled copy of what the view shows */
  uint32_t *thumb, *tmp;
  int tw, th, thumb_cap;
  uint64_t thumb_key;
  int thumb_vw, thumb_vh;
} VideoExt;

#define VVX(w) AWRA_EXT(w, VideoExt)

static uint64_t g_video_streams = 0x766964656f000000ull;

static void pool_unref_locked(VPool *p)
{
  if (p && --p->refs == 0) {
    for (int i = 0; i < p->n; i++) {
      free(p->bufs[i].data);
    }
    free(p);
  }
}

static void shared_unref(VShared *s)
{
  vm_lock(&s->lock);
  const int left = --s->refs;
  vm_unlock(&s->lock);
  if (left == 0) {
    if (s->pending) {
      pool_unref_locked(s->pending->pool);
    }
    if (s->shown) {
      pool_unref_locked(s->shown->pool);
    }
    pool_unref_locked(s->pool);
    vm_free(&s->lock);
    free(s);
  }
}

/* Puts a buffer back (lock held). */
static void buf_free_locked(VBuf *b)
{
  if (!b || b->state == VB_FREE) {
    return;
  }
  b->state = VB_FREE;
  pool_unref_locked(b->pool);
}

static void vv_frame_size(VPool *p, float *r_w, float *r_h)
{
  *r_w = (float)(p->w * (p->par > 0 ? p->par : 1.0));
  *r_h = (float)p->h;
}

static void vv_fit_rect(AwraWidget *w, VPool *p, float *x, float *y, float *dw, float *dh)
{
  VideoExt *x_ = VVX(w);
  float fw, fh;
  vv_frame_size(p, &fw, &fh);
  const float W = (float)w->w, H = (float)w->h;
  if (x_->fit == 0 || fw <= 0 || fh <= 0) {
    *x = (float)w->x;
    *y = (float)w->y;
    *dw = W;
    *dh = H;
    return;
  }
  const float sx = W / fw, sy = H / fh;
  const float s = x_->fit == 2 ? fmaxf(sx, sy) : fminf(sx, sy);
  /* whole pixels: a frame decoded at the size shown is a plain copy */
  *dw = floorf(fw * s + 0.5f);
  *dh = floorf(fh * s + 0.5f);
  *x = (float)w->x + floorf((W - *dw) * 0.5f);
  *y = (float)w->y + floorf((H - *dh) * 0.5f);
}

static void vv_draw(AwraWidget *w, AwraDraw *d)
{
  VideoExt *x = VVX(w);
  const AwraRGBA bg = x->custom_background ? x->background : awra_rgba(0, 0, 0, 1);
  VBuf *b = x->s->shown; /* changes on this thread only */
  if (!b) {
    awra_paint_rect(d, awra_box_of(w), &bg);
    return;
  }
  VPool *p = b->pool;
  float fx, fy, fw, fh;
  vv_fit_rect(w, p, &fx, &fy, &fw, &fh);
  x->dx = fx;
  x->dy = fy;
  x->dw = fw;
  x->dh = fh;
  /* letterbox: only the bars, not under the frame */
  const AwraBox wb = awra_box_of(w);
  if (fy > wb.y) {
    awra_paint_rect(d, (AwraBox){wb.x, wb.y, wb.w, fy - wb.y}, &bg);
  }
  if (fy + fh < wb.y + wb.h) {
    awra_paint_rect(d, (AwraBox){wb.x, fy + fh, wb.w, wb.y + wb.h - fy - fh}, &bg);
  }
  if (fx > wb.x) {
    awra_paint_rect(d, (AwraBox){wb.x, fy, fx - wb.x, fh}, &bg);
  }
  if (fx + fw < wb.x + wb.w) {
    awra_paint_rect(d, (AwraBox){fx + fw, fy, wb.x + wb.w - fx - fw, fh}, &bg);
  }
  const BuiPixmap pm = {(const uint32_t *)b->data, p->w, p->h, b->key,
                        p->format == AWRA_VIDEO_FORMAT_NV12 ?
                            (p->h > 576 ? BUI_PIXMAP_NV12_BT709 : BUI_PIXMAP_NV12_BT601) :
                            BUI_PIXMAP_ARGB,
                        x->s->stream};
  const bool clip = x->fit == 2;
  if (clip) {
    bk_clip_push(w->x, w->y, w->w, w->h);
  }
  BUI_raster_image(d->r, fx, fy, fw, fh, &pm, NULL, d->opacity, 0);
  if (clip) {
    bk_clip_pop();
  }
  x->drawn++;
}

static void vv_finalize(AwraWidget *w)
{
  VideoExt *x = VVX(w);
  vm_lock(&x->s->lock);
  x->s->view = NULL;
  vm_unlock(&x->s->lock);
  BUI_image_forget(x->s->stream);
  shared_unref(x->s);
  free(x->thumb);
  free(x->tmp);
}

static void vv_allocate(AwraWidget *w)
{
  awra_invalidate(w);
}

static void vv_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  *r_min = 0;
  *r_nat = 0;
}

static AwraClass video_class = {
    .bk = AWRA_BK("AwraVideoView", 0),
    .type = "AwraVideoView",
    .flags = AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(VideoExt),
    .finalize = vv_finalize,
    .measure = vv_measure,
    .allocate = vv_allocate,
    .draw = vv_draw,
};

AwraWidget *awra_video_view_new(void)
{
  AwraWidget *w = awra_widget_new(&video_class);
  VideoExt *x = VVX(w);
  x->fit = 1;
  x->s = (VShared *)calloc(1, sizeof(VShared));
  vm_init(&x->s->lock);
  x->s->refs = 1;
  x->s->view = w;
  x->s->stream = ++g_video_streams;
  return w;
}

bool awra_video_view_configure(AwraWidget *w, AwraVideoFormat format, int width, int height,
                               double pixel_aspect, int buffers, int *r_strides, int *r_lines)
{
  if (!w || width <= 0 || height <= 0) {
    return false;
  }
  if (format == AWRA_VIDEO_FORMAT_NV12 && ((width | height) & 1)) {
    return false;
  }
  buffers = BK_CLAMP(buffers, 2, 8);
  const int n = buffers + 1;
  VPool *p = (VPool *)calloc(1, sizeof(VPool) + sizeof(VBuf) * (size_t)n);
  if (!p) {
    return false;
  }
  p->format = format;
  p->w = width;
  p->h = height;
  p->par = pixel_aspect > 0 ? pixel_aspect : 1.0;
  p->n = n;
  if (format == AWRA_VIDEO_FORMAT_NV12) {
    p->strides[0] = p->strides[1] = width;
    p->lines[0] = height;
    p->lines[1] = height / 2;
    p->offsets[1] = (size_t)width * height;
    p->size = (size_t)width * height * 3 / 2;
  }
  else {
    p->strides[0] = width * 4;
    p->lines[0] = height;
    p->size = (size_t)width * height * 4;
  }
  for (int i = 0; i < n; i++) {
    p->bufs[i].pool = p;
    /* rows padded for decoders that write whole SIMD words past the end */
    p->bufs[i].data = (uint8_t *)malloc(p->size + 64);
    p->bufs[i].discard = i == n - 1;
    if (!p->bufs[i].data) {
      for (int k = 0; k < i; k++) {
        free(p->bufs[k].data);
      }
      free(p);
      return false;
    }
    if (format != AWRA_VIDEO_FORMAT_NV12) {
      memset(p->bufs[i].data, 0, p->size);
    }
  }
  p->refs = 1;
  VShared *s = VVX(w)->s;
  vm_lock(&s->lock);
  VPool *old = s->pool;
  s->pool = p;
  pool_unref_locked(old);
  vm_unlock(&s->lock);
  if (r_strides) {
    r_strides[0] = p->strides[0];
    r_strides[1] = p->strides[1];
  }
  if (r_lines) {
    r_lines[0] = p->lines[0];
    r_lines[1] = p->lines[1];
  }
  return true;
}

void *awra_video_view_acquire(AwraWidget *w, void **r_planes)
{
  VShared *s = VVX(w)->s;
  vm_lock(&s->lock);
  VPool *p = s->pool;
  VBuf *got = NULL;
  if (p) {
    for (int i = 0; i < p->n - 1 && !got; i++) {
      if (p->bufs[i].state == VB_FREE) {
        got = &p->bufs[i];
      }
    }
    if (!got) { /* every buffer is in use: decode into the discard buffer */
      got = &p->bufs[p->n - 1];
      if (got->state != VB_FREE) {
        got = NULL;
      }
      s->dropped++;
    }
    if (got) {
      got->state = VB_WRITING;
      p->refs++;
    }
  }
  vm_unlock(&s->lock);
  if (r_planes) {
    r_planes[0] = got ? got->data : NULL;
    r_planes[1] = got ? got->data + p->offsets[1] : NULL;
  }
  return got;
}

static void vv_deliver(void *data)
{
  VShared *s = (VShared *)data;
  AwraWidget *w = NULL;
  vm_lock(&s->lock);
  s->posted = false;
  if (s->view && s->pending) {
    buf_free_locked(s->shown);
    s->shown = s->pending;
    s->shown->state = VB_SHOWN;
    s->pending = NULL;
    w = s->view;
  }
  vm_unlock(&s->lock);
  if (w) {
    awra_invalidate(w);
    awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "frame");
  }
  shared_unref(s);
}

void awra_video_view_present(AwraWidget *w, void *frame)
{
  VBuf *b = (VBuf *)frame;
  VShared *s = VVX(w)->s;
  if (!b) {
    return;
  }
  bool post = false;
  vm_lock(&s->lock);
  if (b->state != VB_WRITING) {
    vm_unlock(&s->lock);
    return;
  }
  if (b->discard || b->pool != s->pool) {
    buf_free_locked(b); /* a dropped frame, or one of a former format */
    vm_unlock(&s->lock);
    return;
  }
  if (b->pool->format == AWRA_VIDEO_FORMAT_BGRX) {
    /* opaque: the raster blends premultiplied ARGB */
    uint32_t *px = (uint32_t *)b->data;
    const size_t n = (size_t)b->pool->w * b->pool->h;
    for (size_t i = 0; i < n; i++) {
      px[i] |= 0xFF000000u;
    }
  }
  if (s->pending) {
    s->dropped++;
    buf_free_locked(s->pending);
  }
  b->state = VB_PENDING;
  b->key = ++s->next_key | (s->stream << 32);
  s->pending = b;
  s->presented++;
  if (!s->posted && s->view) {
    s->posted = true;
    s->refs++;
    post = true;
  }
  vm_unlock(&s->lock);
  if (post) {
    awra_invoke(vv_deliver, s);
  }
}

void awra_video_view_release(AwraWidget *w, void *frame)
{
  VBuf *b = (VBuf *)frame;
  if (!b) {
    return;
  }
  VShared *s = VVX(w)->s;
  vm_lock(&s->lock);
  if (b->state == VB_WRITING) {
    buf_free_locked(b);
  }
  vm_unlock(&s->lock);
}

void awra_video_view_clear(AwraWidget *w)
{
  VShared *s = VVX(w)->s;
  vm_lock(&s->lock);
  buf_free_locked(s->shown);
  buf_free_locked(s->pending);
  s->shown = s->pending = NULL;
  vm_unlock(&s->lock);
  VVX(w)->thumb_key = 0;
  awra_invalidate(w);
}

bool awra_video_view_has_frame(AwraWidget *w)
{
  return VVX(w)->s->shown != NULL;
}

void awra_video_view_frame_size(AwraWidget *w, int *r_width, int *r_height, double *r_aspect)
{
  VBuf *b = VVX(w)->s->shown;
  if (r_width) {
    *r_width = b ? b->pool->w : 0;
  }
  if (r_height) {
    *r_height = b ? b->pool->h : 0;
  }
  if (r_aspect) {
    *r_aspect = b ? b->pool->w * b->pool->par / b->pool->h : 0;
  }
}

void awra_video_view_set_content_fit(AwraWidget *w, int fit)
{
  VVX(w)->fit = BK_CLAMP(fit, 0, 2);
  VVX(w)->thumb_key = 0;
  awra_invalidate(w);
}

int awra_video_view_content_fit(AwraWidget *w)
{
  return VVX(w)->fit;
}

void awra_video_view_set_background(AwraWidget *w, const AwraRGBA *color)
{
  VideoExt *x = VVX(w);
  x->custom_background = color != NULL;
  if (color) {
    x->background = *color;
  }
  awra_invalidate(w);
}

void awra_video_view_stats(AwraWidget *w, uint64_t *r_presented, uint64_t *r_dropped,
                           uint64_t *r_drawn)
{
  VideoExt *x = VVX(w);
  vm_lock(&x->s->lock);
  if (r_presented) {
    *r_presented = x->s->presented;
  }
  if (r_dropped) {
    *r_dropped = x->s->dropped;
  }
  vm_unlock(&x->s->lock);
  if (r_drawn) {
    *r_drawn = x->drawn;
  }
}

bool awra_video_view_save_png(AwraWidget *w, const char *path)
{
  VBuf *b = VVX(w)->s->shown;
  if (!b || !path) {
    return false;
  }
  VPool *p = b->pool;
  /* the display aspect: anamorphic frames are widened (nearest columns) */
  const int ow = BK_MAX(1, (int)lround(p->w * p->par)), oh = p->h;
  uint32_t *out = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)ow * oh);
  uint32_t *row = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)p->w);
  if (!out || !row) {
    free(out);
    free(row);
    return false;
  }
  const BuiPixmap pm = {(const uint32_t *)b->data, p->w, p->h, 0,
                        p->format == AWRA_VIDEO_FORMAT_NV12 ?
                            (p->h > 576 ? BUI_PIXMAP_NV12_BT709 : BUI_PIXMAP_NV12_BT601) :
                            BUI_PIXMAP_ARGB,
                        0};
  for (int y = 0; y < oh; y++) {
    BUI_pixmap_read(&pm, y, 1, row);
    uint32_t *o = out + (size_t)y * ow;
    for (int x = 0; x < ow; x++) {
      o[x] = row[ow == p->w ? x : (int)((int64_t)x * p->w / ow)] | 0xFF000000u;
    }
  }
  free(row);
  const bool ok = awra_write_png(path, out, ow, oh);
  free(out);
  return ok;
}

/* ----------------------------------------------------------------------- */
/* Backdrop: what the view shows, about 160 px wide, blurred like Awra 1's
 * local backdrop (gtk_snapshot_push_blur with the XL spacing, 24 px). */

static inline uint32_t sample_px(const VPool *p, const uint8_t *data, int fx, int fy)
{
  if (p->format == AWRA_VIDEO_FORMAT_NV12) {
    const uint8_t *yp = data, *uv = data + p->offsets[1];
    const int Y = yp[(size_t)fy * p->w + fx];
    const uint8_t *c = uv + (size_t)(fy >> 1) * p->w + (fx & ~1);
    const int u = c[0] - 128, v = c[1] - 128;
    const int k = p->h > 576;
    const int kk[2][5] = {{76309, 104597, 25675, 53279, 132201}, {76309, 117489, 13975, 34925, 138438}};
    const int l = (Y - 16) * kk[k][0] + 32768;
    const int r = BK_CLAMP((l + kk[k][1] * v) >> 16, 0, 255);
    const int g = BK_CLAMP((l - kk[k][2] * u - kk[k][3] * v) >> 16, 0, 255);
    const int bl = BK_CLAMP((l + kk[k][4] * u) >> 16, 0, 255);
    return (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)bl;
  }
  return ((const uint32_t *)data)[(size_t)fy * p->w + fx] & 0xFFFFFFu;
}

static void box_blur(uint32_t *src, uint32_t *tmp, int w, int h, int r)
{
  /* horizontal then vertical, separable running sums per channel */
  for (int pass = 0; pass < 2; pass++) {
    const int len = pass == 0 ? w : h, lines = pass == 0 ? h : w;
    const int step = pass == 0 ? 1 : w, lstep = pass == 0 ? w : 1;
    for (int l = 0; l < lines; l++) {
      const uint32_t *in = src + (size_t)l * lstep;
      uint32_t *out = tmp + (size_t)l * lstep;
      int sr = 0, sg = 0, sb = 0;
      const int n = 2 * r + 1;
      for (int i = -r; i <= r; i++) {
        const uint32_t p = in[(size_t)BK_CLAMP(i, 0, len - 1) * step];
        sr += (p >> 16) & 255;
        sg += (p >> 8) & 255;
        sb += p & 255;
      }
      for (int i = 0; i < len; i++) {
        out[(size_t)i * step] = 0xFF000000u | (uint32_t)(sr / n) << 16 | (uint32_t)(sg / n) << 8 |
                                (uint32_t)(sb / n);
        const uint32_t a = in[(size_t)BK_CLAMP(i - r, 0, len - 1) * step];
        const uint32_t b = in[(size_t)BK_CLAMP(i + r + 1, 0, len - 1) * step];
        sr += (int)((b >> 16) & 255) - (int)((a >> 16) & 255);
        sg += (int)((b >> 8) & 255) - (int)((a >> 8) & 255);
        sb += (int)(b & 255) - (int)(a & 255);
      }
    }
    memcpy(src, tmp, sizeof(uint32_t) * (size_t)w * h);
  }
}

static bool vv_update_thumb(AwraWidget *w)
{
  VideoExt *x = VVX(w);
  VBuf *b = x->s->shown;
  if (w->w <= 0 || w->h <= 0) {
    return false;
  }
  const uint64_t key = b ? b->key : 1;
  if (x->thumb && x->thumb_key == key && x->thumb_vw == w->w && x->thumb_vh == w->h) {
    return true;
  }
  const int tw = BK_MAX(8, BK_MIN(160, w->w / 4));
  const int th = BK_MAX(8, (int)((int64_t)tw * w->h / w->w));
  if (tw * th > x->thumb_cap) {
    free(x->thumb);
    free(x->tmp);
    x->thumb = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)tw * th);
    x->tmp = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)tw * th);
    x->thumb_cap = x->thumb && x->tmp ? tw * th : 0;
    if (!x->thumb_cap) {
      return false;
    }
  }
  x->tw = tw;
  x->th = th;
  const AwraRGBA bgc = x->custom_background ? x->background : awra_rgba(0, 0, 0, 1);
  const uint32_t bg = 0xFF000000u | (uint32_t)(bgc.r * 255) << 16 | (uint32_t)(bgc.g * 255) << 8 |
                      (uint32_t)(bgc.b * 255);
  float fx = 0, fy = 0, fw = 0, fh = 0;
  if (b) {
    vv_fit_rect(w, b->pool, &fx, &fy, &fw, &fh);
    fx -= w->x;
    fy -= w->y;
  }
  const float cell = (float)w->w / tw;
  for (int ty = 0; ty < th; ty++) {
    for (int tx = 0; tx < tw; tx++) {
      uint32_t acc_r = 0, acc_g = 0, acc_b = 0, n = 0;
      for (int sy = 0; sy < 3; sy++) {
        for (int sx = 0; sx < 3; sx++) {
          const float vx = (tx + (sx + 0.5f) / 3.0f) * cell;
          const float vy = (ty + (sy + 0.5f) / 3.0f) * cell;
          uint32_t p = bg;
          if (b && vx >= fx && vx < fx + fw && vy >= fy && vy < fy + fh) {
            const int ix = BK_CLAMP((int)((vx - fx) / fw * b->pool->w), 0, b->pool->w - 1);
            const int iy = BK_CLAMP((int)((vy - fy) / fh * b->pool->h), 0, b->pool->h - 1);
            p = sample_px(b->pool, b->data, ix, iy);
          }
          acc_r += (p >> 16) & 255;
          acc_g += (p >> 8) & 255;
          acc_b += p & 255;
          n++;
        }
      }
      x->thumb[(size_t)ty * tw + tx] =
          0xFF000000u | (acc_r / n) << 16 | (acc_g / n) << 8 | (acc_b / n);
    }
  }
  /* sigma 12 logical px, in thumbnail pixels: three box passes */
  const float sigma = awra_pxf(12) / cell;
  const int r = BK_CLAMP((int)lroundf(sqrtf(sigma * sigma * 4.0f + 1.0f) / 2.0f), 1, 6);
  for (int i = 0; i < 3; i++) {
    box_blur(x->thumb, x->tmp, tw, th, r);
  }
  x->thumb_key = key;
  x->thumb_vw = w->w;
  x->thumb_vh = w->h;
  return true;
}

/* Paints the blurred image of what `source` shows under box b (raster px),
 * clipped to the rounded corners. True when painted. */
bool awra_backdrop_paint(AwraWidget *source, AwraDraw *d, AwraBox b, const float radii[4]);
bool awra_backdrop_paint(AwraWidget *source, AwraDraw *d, AwraBox b, const float radii[4])
{
  if (!source || !awra_widget_is(source, "AwraVideoView") || !awra_widget_mapped(source) ||
      source->w <= 0 || source->h <= 0)
  {
    return false;
  }
  /* a view showing nothing lends no backdrop (an empty GtkPicture): the
   * surface paints its own material */
  if (!VVX(source)->s->shown || !vv_update_thumb(source)) {
    return false;
  }
  VideoExt *x = VVX(source);
  /* the part of b over the view */
  const float x0 = fmaxf(b.x, (float)source->x), y0 = fmaxf(b.y, (float)source->y);
  const float x1 = fminf(b.x + b.w, (float)(source->x + source->w));
  const float y1 = fminf(b.y + b.h, (float)(source->y + source->h));
  if (x1 <= x0 || y1 <= y0) {
    return false;
  }
  const float kx = (float)x->tw / source->w, ky = (float)x->th / source->h;
  const float src[4] = {(x0 - source->x) * kx, (y0 - source->y) * ky, (x1 - x0) * kx, (y1 - y0) * ky};
  const BuiPixmap pm = {x->thumb, x->tw, x->th, 0, BUI_PIXMAP_ARGB, 0};
  BUI_raster_image_rounded(d->r, x0, y0, x1 - x0, y1 - y0, &pm, src, d->opacity, radii);
  return true;
}

/* Material painting over a local backdrop (AwraSurface:backdrop-widget):
 * blur, Awra 1's neutral scrim in dark appearances, then the translucent
 * tint and the edges. False: no backdrop to paint (paint as usual). */
bool awra_paint_material_backdrop(AwraDraw *d, AwraBox b, AwraMaterial material, int role,
                                  AwraElevation elevation, float radius, int corner_mask,
                                  int edge_mask, AwraWidget *source);
bool awra_paint_material_backdrop(AwraDraw *d, AwraBox b, AwraMaterial material, int role,
                                  AwraElevation elevation, float radius, int corner_mask,
                                  int edge_mask, AwraWidget *source)
{
  const AwraTokens *t = awra_tokens();
  if (!source || awra_reduced_transparency() || t->high_contrast || !t->material_blur[material]) {
    return false;
  }
  const float radii[4] = {(corner_mask & 1) ? radius : 0, (corner_mask & 2) ? radius : 0,
                          (corner_mask & 4) ? radius : 0, (corner_mask & 8) ? radius : 0};
  if (!awra_backdrop_paint(source, d, b, radii)) {
    return false;
  }
  if (t->foreground.r > 0.5f) {
    const AwraRGBA shade = awra_rgba(0, 0, 0, 0.6f);
    awra_paint_rbox4(d, b, radii, &shade, NULL, NULL);
  }
  AwraResolvedMaterial m;
  awra_material_resolve(material, role, elevation, true, &m);
  /* request_blur needs native blur on; a local backdrop is enough here */
  if (m.fallback) {
    m.fill = t->material_tint[material];
  }
  awra_paint_material(d, b, &m, radius, corner_mask, edge_mask, elevation);
  return true;
}

/* ======================================================================= */
/* Timeline (Awra 1's AwraTimeline): a well, the track, intervals and
 * markers, the hovered time, the playhead; seeks on press / drag / keys;
 * editable mode drags a global offset; a preview floats above the hovered
 * time. */

typedef struct TInterval {
  double start, end;
  char *label;
  AwraTextLayout *layout;
} TInterval;

typedef struct TimelineExt {
  TInterval *iv;
  int niv, cap;
  double duration, position, view_start, view_duration, offset, hover_time;
  double step, page;
  double drag_x, drag_offset, drag_target;
  bool editable, dragging, inside;
  double hover_x;
  AwraWidget *preview;   /* the floating surface holding the preview child */
  AwraWidget *preview_child;
  bool preview_shown;
} TimelineExt;

#define TLX(w) AWRA_EXT(w, TimelineExt)

static double tl_span(TimelineExt *s)
{
  return s->view_duration > 0 ? s->view_duration : fmax(s->duration, 1.0);
}

static double tl_time_x(AwraWidget *w, double time)
{
  TimelineExt *s = TLX(w);
  const double f = (time - s->view_start) / tl_span(s);
  const double m = awra_pxf(8);
  return w->x + m + f * fmax(w->w - 2 * m, 1);
}

static double tl_x_time(AwraWidget *w, double px)
{
  TimelineExt *s = TLX(w);
  const double m = awra_pxf(8);
  const double f = fmin(fmax((px - w->x - m) / fmax(w->w - 2 * m, 1), 0), 1);
  return fmin(fmax(s->view_start + f * tl_span(s), 0), s->duration);
}

static void tl_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  *mn = awra_px(o == AWRA_ORIENTATION_HORIZONTAL ? 64 : 40);
  *nt = awra_px(o == AWRA_ORIENTATION_HORIZONTAL ? 320 : 56);
}

static void tl_rect(AwraDraw *d, const AwraRGBA *c, float x, float y, float w, float h, float r)
{
  if (w <= 0 || h <= 0) {
    return;
  }
  awra_paint_rbox(d, (AwraBox){x, y, w, h}, fminf(r, fminf(w, h) * 0.5f), c, 0, NULL);
}

static void tl_draw(AwraWidget *w, AwraDraw *d)
{
  TimelineExt *s = TLX(w);
  const AwraTokens *t = awra_tokens();
  const float X = (float)w->x, Y = (float)w->y, W = (float)w->w, H = (float)w->h;
  const float radius = awra_pxf(t->radii[AWRA_RADIUS_SMALL]);
  const bool sens = awra_sensitive(w);
  const AwraRGBA fg = sens ? t->foreground : t->disabled;
  const AwraRGBA accent = sens ? t->accent : fg;
  const float p4 = awra_pxf(4), p8 = awra_pxf(8);
  const double pos = fmin(fmax(tl_time_x(w, s->dragging && !s->editable ? s->drag_target : s->position),
                               X + p8),
                          fmax(X + W - p8, X + p8));
  tl_rect(d, &t->control_well, X + p4, Y + p4, W - 2 * p4, H - 2 * p4, radius);
  tl_rect(d, &t->control_track, X + p8, Y + H / 2 - p4, W - 2 * p8, p8, p4);
  tl_rect(d, &accent, X + p8, Y + H / 2 - p4, (float)pos - X - p8, p8, p4);
  for (int n = 0; n < s->niv; n++) {
    TInterval *i = &s->iv[n];
    const double a = tl_time_x(w, i->start + s->offset), b = tl_time_x(w, i->end + s->offset);
    double left = fmin(a, b), right = fmax(a, b);
    const bool point = i->start == i->end;
    if (right < X + p8 || left > X + W - p8) {
      continue;
    }
    left = fmax(left, X + p8);
    right = fmin(right, X + W - p8);
    const bool active = s->position >= i->start + s->offset && s->position <= i->end + s->offset;
    tl_rect(d, active ? &t->accent_soft : &t->control_cap, (float)left, Y + p8,
            (float)fmax(right - left, point ? awra_pxf(3) : awra_pxf(1)), H - 2 * p8, radius / 2);
    if (point) {
      tl_rect(d, &accent, (float)left, Y + p8, awra_pxf(3), H - 2 * p8, awra_pxf(1));
    }
    else if (i->label && right - left > awra_pxf(40)) {
      if (!i->layout) {
        i->layout = awra_text_layout_new();
        awra_text_layout_set_text(i->layout, i->label, false);
      }
      const AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
      awra_text_layout_set_font(i->layout, &f);
      awra_text_layout_set_wrap(i->layout, AWRA_WRAP_NONE, AWRA_ELLIPSIZE_END, 1);
      const int lw = (int)(right - left - awra_pxf(12));
      bk_clip_push((int)left, (int)(Y + p8), (int)(right - left), (int)(H - 2 * p8));
      awra_text_layout_draw(i->layout, d, (float)left + awra_pxf(6), Y + H / 2 - awra_pxf(8), lw, &fg);
      bk_clip_pop();
    }
  }
  if (s->hover_time >= 0) {
    tl_rect(d, &t->muted, (float)tl_time_x(w, s->hover_time), Y + awra_pxf(6), awra_pxf(1),
            H - awra_pxf(12), 0);
  }
  tl_rect(d, &fg, (float)pos - awra_pxf(1), Y + p4, awra_pxf(2), H - 2 * p4, awra_pxf(1));
  tl_rect(d, &accent, (float)pos - p4, Y + H / 2 - awra_pxf(6), p8, awra_pxf(12), p4);
  if (awra_focus_visible(w) || t->high_contrast) {
    const AwraBox fb = {X + awra_pxf(1), Y + awra_pxf(1), fmaxf(W - awra_pxf(2), 0),
                        fmaxf(H - awra_pxf(2), 0)};
    awra_paint_rbox(d, fb, radius, NULL, awra_pxf(2), &t->focus_ring);
  }
}

static void tl_place_preview(AwraWidget *w)
{
  TimelineExt *s = TLX(w);
  AwraWidget *win = awra_window_of(w);
  if (!s->preview || !win) {
    return;
  }
  int mn, pw, ph;
  awra_measure(s->preview, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &pw);
  awra_measure(s->preview, AWRA_ORIENTATION_VERTICAL, pw, &mn, &ph);
  const float sc = awra_scale();
  const double cx = (w->x + s->hover_x * sc) / sc;
  const double top = w->y / sc;
  awra_window_place_overlay(win, s->preview, cx - pw / sc / 2.0, top - ph / sc - 8);
}

static void tl_show_preview(AwraWidget *w, bool show)
{
  TimelineExt *s = TLX(w);
  AwraWidget *win = awra_window_of(w);
  if (!s->preview || !win) {
    return;
  }
  if (show) {
    if (!s->preview_shown) {
      awra_window_add_overlay(win, s->preview);
      s->preview_shown = true;
    }
    tl_place_preview(w);
  }
  else if (s->preview_shown) {
    s->preview_shown = false;
    awra_window_remove_overlay(win, s->preview);
  }
}

static void tl_update_hover(AwraWidget *w, double lx)
{
  TimelineExt *s = TLX(w);
  const float sc = awra_scale();
  s->hover_x = fmin(fmax(lx, 0), w->w / sc);
  s->hover_time = tl_x_time(w, w->x + lx * sc);
  if (s->preview && s->duration > 0 && awra_widget_mapped(w)) {
    tl_show_preview(w, true);
  }
  awra_invalidate(w);
  awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "hover-time");
}

static void tl_hide_hover(AwraWidget *w)
{
  TimelineExt *s = TLX(w);
  tl_show_preview(w, false);
  if (s->hover_time != -1) {
    s->hover_x = s->hover_time = -1;
    awra_invalidate(w);
    awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "hover-time");
  }
}

static bool tl_request(AwraWidget *w, double time)
{
  TimelineExt *s = TLX(w);
  if (s->duration <= 0 || !isfinite(time) || !awra_sensitive(w)) {
    return false;
  }
  awra_emit_value(w, AWRA_SIGNAL_SEEK, fmin(fmax(time, 0), s->duration));
  return true;
}

void awra_timeline_set_offset(AwraWidget *w, double offset);

static int tl_motion(AwraWidget *w, BKEvent *e)
{
  TLX(w)->inside = true;
  if (!TLX(w)->dragging) {
    tl_update_hover(w, (e->x - w->x) / awra_scale());
  }
  return 0;
}

static void tl_leave(AwraWidget *w, const AwraArgs *a, void *data)
{
  TimelineExt *s = TLX(w);
  s->inside = false;
  if (!s->dragging) {
    tl_hide_hover(w);
  }
}

static void tl_enter(AwraWidget *w, const AwraArgs *a, void *data)
{
  TLX(w)->inside = true;
}

static int tl_press(AwraWidget *w, BKEvent *e)
{
  TimelineExt *s = TLX(w);
  if (s->duration <= 0 || !awra_sensitive(w)) {
    return 1;
  }
  AwraWidget *win = awra_window_of(w);
  if (win) {
    awra_set_focus(win, w, false);
  }
  s->dragging = true;
  s->drag_x = e->x;
  s->drag_offset = s->offset;
  s->drag_target = tl_x_time(w, e->x);
  awra_invalidate(w);
  return 1;
}

static void tl_drag_to(AwraWidget *w, int px)
{
  TimelineExt *s = TLX(w);
  if (s->editable) {
    const double m = awra_pxf(8);
    awra_timeline_set_offset(w, s->drag_offset + (px - s->drag_x) * tl_span(s) / fmax(w->w - 2 * m, 1));
  }
  else {
    s->drag_target = tl_x_time(w, px);
    tl_update_hover(w, (px - w->x) / awra_scale());
  }
  awra_invalidate(w);
}

static int tl_drag(AwraWidget *w, BKEvent *e)
{
  if (TLX(w)->dragging) {
    tl_drag_to(w, e->x);
  }
  return 1;
}

static int tl_release(AwraWidget *w, BKEvent *e)
{
  TimelineExt *s = TLX(w);
  if (!s->dragging) {
    return 1;
  }
  tl_drag_to(w, e->x);
  s->dragging = false;
  if (s->editable) {
    awra_emit_value(w, AWRA_SIGNAL_VALUE_CHANGED, s->offset);
  }
  else {
    tl_request(w, s->drag_target);
  }
  if (!e->inside) {
    tl_hide_hover(w);
  }
  awra_invalidate(w);
  return 1;
}

static int tl_key(AwraWidget *w, BKEvent *e)
{
  TimelineExt *s = TLX(w);
  if (s->duration <= 0 || !awra_sensitive(w) || !e->keysym) {
    return 0;
  }
  const double delta = (e->mods & BK_SHIFT) ? s->page : s->step;
  double target = s->position;
  const bool horizontal = bk_keysym_is(e->keysym, "Left") || bk_keysym_is(e->keysym, "Right");
  if (bk_keysym_is(e->keysym, "Left")) {
    target -= delta;
  }
  else if (bk_keysym_is(e->keysym, "Right")) {
    target += delta;
  }
  else if (bk_keysym_is(e->keysym, "Home")) {
    target = 0;
  }
  else if (bk_keysym_is(e->keysym, "End")) {
    target = s->duration;
  }
  else {
    return 0;
  }
  if (s->editable && horizontal) {
    awra_timeline_set_offset(w, s->offset + (target - s->position) * 0.1);
    awra_emit_value(w, AWRA_SIGNAL_VALUE_CHANGED, s->offset);
    return 1;
  }
  return tl_request(w, target) ? 1 : 0;
}

static void tl_clear_intervals(TimelineExt *s)
{
  for (int i = 0; i < s->niv; i++) {
    free(s->iv[i].label);
    if (s->iv[i].layout) {
      awra_text_layout_free(s->iv[i].layout);
    }
  }
  s->niv = 0;
}

static void tl_map(AwraWidget *w, bool mapped)
{
  if (!mapped) {
    TLX(w)->inside = TLX(w)->dragging = false;
    tl_hide_hover(w);
  }
}

static void tl_finalize(AwraWidget *w)
{
  TimelineExt *s = TLX(w);
  tl_clear_intervals(s);
  free(s->iv);
  if (s->preview) {
    awra_widget_unref(s->preview);
  }
}

static void tl_style_changed(AwraWidget *w)
{
  for (int i = 0; i < TLX(w)->niv; i++) {
    if (TLX(w)->iv[i].layout) {
      awra_text_layout_free(TLX(w)->iv[i].layout);
      TLX(w)->iv[i].layout = NULL;
    }
  }
}

/* Awra 1's AwraTimeline: a slider named "Timeline" over the position (or,
 * editable, the offset in seconds) */
static void timeline_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  TimelineExt *s = TLX(w);
  info->label = "Timeline";
  info->has_value = true;
  if (s->editable) {
    const double m = fmax(s->duration, fabs(s->offset));
    info->min = -m;
    info->max = m;
    info->value = s->offset;
    snprintf(info->buf, sizeof(info->buf), "%+.2f s", s->offset);
  }
  else {
    const int p = s->position > 0 ? (int)s->position : 0;
    info->min = 0;
    info->max = s->duration;
    info->value = s->position;
    if (p >= 3600) {
      snprintf(info->buf, sizeof(info->buf), "%d:%02d:%02d", p / 3600, p / 60 % 60, p % 60);
    }
    else {
      snprintf(info->buf, sizeof(info->buf), "%d:%02d", p / 60, p % 60);
    }
  }
  info->value_text = info->buf;
  info->step = s->step > 0 ? s->step : 1.0;
  info->flags |= AWRA_A11Y_HORIZONTAL;
}

static bool timeline_a11y_do(AwraWidget *w, int item, int verb, double value)
{
  TimelineExt *s = TLX(w);
  if (item < 0 && verb == AWRA_A11Y_DO_SET_VALUE && !s->editable && awra_sensitive(w)) {
    awra_emit_value(w, AWRA_SIGNAL_SEEK, fmin(fmax(value, 0), s->duration));
    return true;
  }
  return false;
}

static AwraClass timeline_class = {
    .a11y = timeline_a11y,
    .a11y_do = timeline_a11y_do,
    .bk = AWRA_BK("AwraTimeline", CLS_INTERACTIVE),
    .type = "AwraTimeline",
    .flags = AWRA_CLASS_FOCUSABLE | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(TimelineExt),
    .finalize = tl_finalize,
    .measure = tl_measure,
    .draw = tl_draw,
    .press = tl_press,
    .drag = tl_drag,
    .release = tl_release,
    .motion = tl_motion,
    .key = tl_key,
    .map = tl_map,
    .style_changed = tl_style_changed,
    .overdraw = 2,
};

AwraWidget *awra_timeline_new(void)
{
  AwraWidget *w = awra_widget_new(&timeline_class);
  TimelineExt *s = TLX(w);
  s->hover_time = s->hover_x = -1;
  s->step = 5;
  s->page = 30;
  awra_signal_connect(w, AWRA_SIGNAL_LEAVE, tl_leave, NULL, NULL);
  awra_signal_connect(w, AWRA_SIGNAL_ENTER, tl_enter, NULL, NULL);
  return w;
}

void awra_timeline_set_duration(AwraWidget *w, double duration)
{
  TimelineExt *s = TLX(w);
  duration = isfinite(duration) && duration > 0 ? duration : 0;
  if (s->duration != duration) {
    s->duration = duration;
    awra_invalidate(w);
  }
}

double awra_timeline_duration(AwraWidget *w)
{
  return TLX(w)->duration;
}

void awra_timeline_set_position(AwraWidget *w, double position)
{
  TimelineExt *s = TLX(w);
  position = isfinite(position) ? fmax(position, 0) : 0;
  if (s->position != position) {
    s->position = position;
    awra_invalidate(w);
  }
}

double awra_timeline_position(AwraWidget *w)
{
  return TLX(w)->position;
}

void awra_timeline_set_view(AwraWidget *w, double start, double duration)
{
  TimelineExt *s = TLX(w);
  s->view_start = fmax(start, 0);
  s->view_duration = fmax(duration, 0);
  awra_invalidate(w);
}

void awra_timeline_set_editable(AwraWidget *w, bool editable)
{
  TLX(w)->editable = editable;
  awra_invalidate(w);
}

void awra_timeline_set_offset(AwraWidget *w, double offset)
{
  TimelineExt *s = TLX(w);
  if (!isfinite(offset) || s->offset == offset) {
    return;
  }
  s->offset = offset;
  awra_invalidate(w);
  awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "offset");
}

double awra_timeline_offset(AwraWidget *w)
{
  return TLX(w)->offset;
}

void awra_timeline_set_increments(AwraWidget *w, double step, double page)
{
  TLX(w)->step = step;
  TLX(w)->page = page;
}

void awra_timeline_add_interval(AwraWidget *w, double start, double end, const char *label)
{
  TimelineExt *s = TLX(w);
  if (s->niv == s->cap) {
    const int cap = s->cap ? s->cap * 2 : 16;
    TInterval *n = (TInterval *)realloc(s->iv, sizeof(TInterval) * (size_t)cap);
    if (!n) {
      return;
    }
    s->iv = n;
    s->cap = cap;
  }
  s->iv[s->niv++] = (TInterval){fmin(start, end), fmax(start, end), awra_strdup(label), NULL};
  awra_invalidate(w);
}

void awra_timeline_clear_intervals(AwraWidget *w)
{
  tl_clear_intervals(TLX(w));
  awra_invalidate(w);
}

double awra_timeline_hover_time(AwraWidget *w)
{
  return TLX(w)->hover_time;
}

void awra_timeline_set_preview(AwraWidget *w, AwraWidget *child)
{
  TimelineExt *s = TLX(w);
  tl_show_preview(w, false);
  if (s->preview) {
    awra_widget_unref(s->preview);
    s->preview = NULL;
  }
  s->preview_child = child;
  if (child) {
    /* Awra 1: an AwraPopover (Floating, radius 17) above the hovered time */
    s->preview = awra_surface_new(AWRA_MATERIAL_FLOATING);
    awra_surface_set_elevation(s->preview, AWRA_ELEVATION_FLOATING);
    awra_surface_set_radius(s->preview, 17);
    awra_surface_set_child(s->preview, child);
    awra_widget_set_can_target(s->preview, false);
  }
}

AwraWidget *awra_timeline_preview_surface(AwraWidget *w)
{
  return TLX(w)->preview;
}

/* ======================================================================= */
/* Overlay bar (Awra 1's AwraOverlayBar): content at full size, a floating
 * bar centered at its bottom (max width 1120, inset LG), revealed by
 * pointer motion, hidden after a delay while auto-hide is on. */

typedef struct OBarExt {
  AwraWidget *content, *surface, *inset_box, *bar;
  bool revealed, auto_hide, inhibit, hovering;
  unsigned hide_delay, hide_source;
  int max_width;
  AwraSpacing inset;
  float fade; /* 0 hidden .. 1 shown */
} OBarExt;

#define OBX(w) AWRA_EXT(w, OBarExt)

void awra_overlay_bar_set_revealed(AwraWidget *w, bool revealed);

static void ob_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  OBarExt *x = OBX(w);
  *mn = *nt = 0;
  if (x->content) {
    awra_measure(x->content, o, f, mn, nt);
  }
  if (x->bar && o == AWRA_ORIENTATION_HORIZONTAL) {
    int bmin, bnat;
    awra_measure(x->surface, o, -1, &bmin, &bnat);
    bmin += 2 * awra_px(awra_spacing(x->inset));
    *mn = BK_MAX(*mn, bmin);
    *nt = BK_MAX(*nt, *mn);
  }
}

static void ob_allocate(AwraWidget *w)
{
  OBarExt *x = OBX(w);
  if (x->content) {
    awra_allocate(x->content, w->x, w->y, w->w, w->h);
  }
  if (x->bar) {
    const int inset = awra_px(awra_spacing(x->inset));
    int mn, nt;
    awra_measure(x->surface, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nt);
    const int bw = BK_MAX(mn, BK_MIN(w->w - 2 * inset, awra_px(x->max_width)));
    awra_measure(x->surface, AWRA_ORIENTATION_VERTICAL, bw, &mn, &nt);
    const int bh = BK_MIN(BK_MAX(nt, mn), BK_MAX(0, w->h - 2 * inset));
    awra_allocate(x->surface, w->x + (w->w - bw) / 2, w->y + BK_MAX(inset, w->h - inset - bh), bw, bh);
  }
}

static void ob_cancel_hide(OBarExt *x)
{
  if (x->hide_source) {
    awra_source_remove(x->hide_source);
    x->hide_source = 0;
  }
}

static bool ob_hide_cb(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  OBarExt *x = OBX(w);
  x->hide_source = 0;
  if (x->auto_hide && !x->inhibit && !x->hovering) {
    awra_overlay_bar_set_revealed(w, false);
  }
  return false;
}

static void ob_schedule_hide(AwraWidget *w)
{
  OBarExt *x = OBX(w);
  ob_cancel_hide(x);
  if (x->auto_hide && x->revealed && !x->inhibit && !x->hovering && awra_widget_mapped(w)) {
    x->hide_source = awra_timeout_add(x->hide_delay, ob_hide_cb, w, NULL);
  }
}

static void ob_fade(AwraWidget *w, double t, void *data)
{
  OBarExt *x = OBX(w);
  const bool showing = data != NULL;
  x->fade = (float)(showing ? t : 1.0 - t);
  awra_widget_set_opacity(x->surface, x->fade);
  if (!showing && t >= 1.0) {
    awra_widget_set_visible(x->surface, false);
  }
}

static void ob_motion(AwraWidget *w, const AwraArgs *a, void *data)
{
  OBarExt *x = OBX(w);
  if (x->bar) {
    awra_overlay_bar_set_revealed(w, true);
    ob_schedule_hide(w);
  }
}

static void ob_bar_enter(AwraWidget *s, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  OBX(w)->hovering = true;
  ob_cancel_hide(OBX(w));
  awra_overlay_bar_set_revealed(w, true);
}

static void ob_bar_leave(AwraWidget *s, const AwraArgs *a, void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  OBX(w)->hovering = false;
  ob_schedule_hide(w);
}

static void ob_map(AwraWidget *w, bool mapped)
{
  OBarExt *x = OBX(w);
  if (mapped) {
    ob_schedule_hide(w);
  }
  else {
    ob_cancel_hide(x);
    x->hovering = false;
  }
}

static void ob_finalize(AwraWidget *w)
{
  ob_cancel_hide(OBX(w));
}

static AwraClass overlay_bar_class = {
    .bk = AWRA_BK("AwraOverlayBar", 0),
    .type = "AwraOverlayBar",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(OBarExt),
    .finalize = ob_finalize,
    .measure = ob_measure,
    .allocate = ob_allocate,
    .map = ob_map,
};

AwraWidget *awra_overlay_bar_new(void)
{
  AwraWidget *w = awra_widget_new(&overlay_bar_class);
  OBarExt *x = OBX(w);
  x->revealed = true;
  x->fade = 1.0f;
  x->hide_delay = 2200;
  x->max_width = 1120;
  x->inset = AWRA_SPACING_LG;
  x->surface = awra_surface_new(AWRA_MATERIAL_FLOATING);
  awra_surface_set_elevation(x->surface, AWRA_ELEVATION_FLOATING);
  x->inset_box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  awra_widget_set_margin(x->inset_box, AWRA_SPACING_SM);
  awra_surface_set_child(x->surface, x->inset_box);
  awra_widget_unref(x->inset_box);
  awra_child_insert(w, x->surface, -1);
  awra_widget_unref(x->surface);
  awra_widget_set_visible(x->surface, false);
  awra_signal_connect(w, AWRA_SIGNAL_MOTION, ob_motion, NULL, NULL);
  awra_signal_connect(x->surface, AWRA_SIGNAL_ENTER, ob_bar_enter, w, NULL);
  awra_signal_connect(x->surface, AWRA_SIGNAL_LEAVE, ob_bar_leave, w, NULL);
  return w;
}

void awra_overlay_bar_set_content(AwraWidget *w, AwraWidget *child)
{
  OBarExt *x = OBX(w);
  if (x->content == child) {
    return;
  }
  if (x->content) {
    awra_child_remove(w, x->content);
  }
  x->content = child;
  if (child) {
    if (child->parent) {
      awra_widget_ref(child);
      awra_container_remove(child->parent, child);
      awra_child_insert(w, child, 0);
      awra_widget_unref(child);
    }
    else {
      awra_child_insert(w, child, 0); /* below the bar */
    }
  }
  awra_widget_queue_resize(w);
}

void awra_overlay_bar_set_bar(AwraWidget *w, AwraWidget *child)
{
  OBarExt *x = OBX(w);
  if (x->bar == child) {
    return;
  }
  if (x->bar) {
    awra_box_remove(x->inset_box, x->bar);
  }
  x->bar = child;
  if (child) {
    awra_box_append(x->inset_box, child);
  }
  awra_widget_set_visible(x->surface, child != NULL && x->revealed);
  awra_widget_queue_resize(w);
}

AwraWidget *awra_overlay_bar_surface(AwraWidget *w)
{
  return OBX(w)->surface;
}

void awra_overlay_bar_set_revealed(AwraWidget *w, bool revealed)
{
  OBarExt *x = OBX(w);
  if (x->revealed == revealed) {
    return;
  }
  x->revealed = revealed;
  awra_anim_stop_for(w);
  if (x->bar) {
    if (revealed) {
      awra_widget_set_visible(x->surface, true);
    }
    const unsigned ms = awra_motion_duration(AWRA_MOTION_FAST);
    awra_anim_start(w, ms, ob_fade, revealed ? (void *)1 : NULL, NULL);
  }
  ob_schedule_hide(w);
  awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "revealed");
}

bool awra_overlay_bar_revealed(AwraWidget *w)
{
  return OBX(w)->revealed;
}

void awra_overlay_bar_reveal(AwraWidget *w)
{
  awra_overlay_bar_set_revealed(w, true);
  ob_schedule_hide(w);
}

void awra_overlay_bar_set_auto_hide(AwraWidget *w, bool auto_hide)
{
  OBarExt *x = OBX(w);
  if (x->auto_hide == auto_hide) {
    return;
  }
  x->auto_hide = auto_hide;
  if (!auto_hide) {
    ob_cancel_hide(x);
  }
  awra_overlay_bar_reveal(w);
}

void awra_overlay_bar_set_inhibit_auto_hide(AwraWidget *w, bool inhibit)
{
  OBarExt *x = OBX(w);
  if (x->inhibit == inhibit) {
    return;
  }
  x->inhibit = inhibit;
  if (inhibit) {
    awra_overlay_bar_set_revealed(w, true);
  }
  ob_schedule_hide(w);
}

void awra_overlay_bar_set_hide_delay(AwraWidget *w, unsigned ms)
{
  OBX(w)->hide_delay = BK_CLAMP(ms, 100u, 60000u);
  ob_schedule_hide(w);
}

void awra_overlay_bar_set_max_width(AwraWidget *w, int width)
{
  OBX(w)->max_width = BK_CLAMP(width, 240, 8192);
  awra_queue_allocate(w);
}

void awra_overlay_bar_set_inset(AwraWidget *w, AwraSpacing inset)
{
  OBX(w)->inset = inset;
  awra_queue_allocate(w);
}

int awra_overlay_bar_inset_px(AwraWidget *w)
{
  return awra_spacing(OBX(w)->inset);
}

AwraSpacing awra_overlay_bar_inset(AwraWidget *w)
{
  return OBX(w)->inset;
}

/* ======================================================================= */
/* Edge panel (Awra 1's AwraEdgePanel): content at full size, a floating
 * panel sliding in from the start or end edge (margin MD). */

typedef struct EdgeExt {
  AwraWidget *content, *surface, *panel;
  int edge; /* 0 start, 1 end */
  bool revealed;
  int panel_width;
  float t; /* 0 hidden .. 1 shown */
} EdgeExt;

#define EDX(w) AWRA_EXT(w, EdgeExt)

static void edge_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  EdgeExt *x = EDX(w);
  *mn = *nt = 0;
  if (x->content) {
    awra_measure(x->content, o, f, mn, nt);
  }
}

static void edge_allocate(AwraWidget *w)
{
  EdgeExt *x = EDX(w);
  if (x->content) {
    awra_allocate(x->content, w->x, w->y, w->w, w->h);
  }
  if (x->surface && (x->surface->flags & NF_VISIBLE)) {
    /* Awra 1's revealer around the surface keeps MD from the edge and from
     * the top and bottom; the surface's own margins (the application's)
     * add to it */
    const int inset = awra_px(awra_spacing(AWRA_SPACING_MD));
    int mn, nt;
    awra_measure(x->surface, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nt);
    const int sw = BK_MAX(0, BK_MIN(nt, w->w - inset)); /* with margins */
    const int slide = (int)lroundf((sw + inset) * (1.0f - x->t));
    const int sx = x->edge == 0 ? w->x + inset - slide : w->x + w->w - inset - sw + slide;
    awra_allocate(x->surface, sx, w->y + inset, sw, BK_MAX(0, w->h - 2 * inset));
  }
}

static AwraClass edge_class = {
    .bk = AWRA_BK("AwraEdgePanel", 0),
    .type = "AwraEdgePanel",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_CLIP | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(EdgeExt),
    .measure = edge_measure,
    .allocate = edge_allocate,
};

AwraWidget *awra_edge_panel_new(void)
{
  AwraWidget *w = awra_widget_new(&edge_class);
  EdgeExt *x = EDX(w);
  x->panel_width = 300;
  x->surface = awra_surface_new(AWRA_MATERIAL_FLOATING);
  awra_surface_set_elevation(x->surface, AWRA_ELEVATION_FLOATING);
  awra_widget_set_size_request(x->surface, x->panel_width, -1);
  awra_child_insert(w, x->surface, -1);
  awra_widget_unref(x->surface);
  awra_widget_set_visible(x->surface, false);
  return w;
}

void awra_edge_panel_set_content(AwraWidget *w, AwraWidget *child)
{
  EdgeExt *x = EDX(w);
  if (x->content == child) {
    return;
  }
  if (x->content) {
    awra_child_remove(w, x->content);
  }
  x->content = child;
  if (child) {
    if (child->parent) {
      awra_widget_ref(child);
      awra_container_remove(child->parent, child);
      awra_child_insert(w, child, 0);
      awra_widget_unref(child);
    }
    else {
      awra_child_insert(w, child, 0);
    }
  }
  awra_widget_queue_resize(w);
}

void awra_edge_panel_set_panel(AwraWidget *w, AwraWidget *panel)
{
  EdgeExt *x = EDX(w);
  x->panel = panel;
  awra_surface_set_child(x->surface, panel);
}

AwraWidget *awra_edge_panel_surface(AwraWidget *w)
{
  return EDX(w)->surface;
}

void awra_edge_panel_set_edge(AwraWidget *w, int edge)
{
  EDX(w)->edge = edge ? 1 : 0;
  awra_queue_allocate(w);
}

void awra_edge_panel_set_panel_width(AwraWidget *w, int width)
{
  EdgeExt *x = EDX(w);
  x->panel_width = BK_CLAMP(width, 160, 720);
  awra_widget_set_size_request(x->surface, x->panel_width, -1);
}

static void edge_anim(AwraWidget *w, double t, void *data)
{
  EdgeExt *x = EDX(w);
  x->t = (float)(data ? t : 1.0 - t);
  if (!data && t >= 1.0) {
    awra_widget_set_visible(x->surface, false);
  }
  awra_queue_allocate(w);
  awra_invalidate(w);
}

void awra_edge_panel_set_revealed(AwraWidget *w, bool revealed)
{
  EdgeExt *x = EDX(w);
  if (x->revealed == revealed) {
    return;
  }
  x->revealed = revealed;
  awra_anim_stop_for(w);
  if (revealed) {
    awra_widget_set_visible(x->surface, true);
  }
  if (awra_widget_mapped(w)) {
    awra_anim_start(w, awra_motion_duration(AWRA_MOTION_NORMAL), edge_anim, revealed ? (void *)1 : NULL,
                    NULL);
  }
  else {
    edge_anim(w, 1.0, revealed ? (void *)1 : NULL);
  }
  awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "revealed");
}

bool awra_edge_panel_revealed(AwraWidget *w)
{
  return EDX(w)->revealed;
}
