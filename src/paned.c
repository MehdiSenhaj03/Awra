/* Awra 2 paned: two children side by side or one above the other, with a
 * 1 px separator (awra.css window.awra-window paned > separator) that the
 * pointer drags from a wider band. A double click on it shares the space
 * evenly. Each child keeps its minimum size. Panes nest for any number of
 * views (Awra 1's AwraTileView over GtkPaned).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#define HANDLE 6

typedef struct PanedExt {
  AwraWidget *start, *end;
  int orientation;
  double fraction;
  int sep; /* raster, along the axis, relative */
  bool dragging;
  double frac0;
  int min_start, min_end;
} PanedExt;

#define PNX(w) AWRA_EXT(w, PanedExt)

static void paned_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  PanedExt *x = PNX(w);
  int a_min = 0, a_nat = 0, b_min = 0, b_nat = 0;
  if (x->start && awra_child_visible(x->start)) {
    awra_measure(x->start, o, -1, &a_min, &a_nat);
  }
  if (x->end && awra_child_visible(x->end)) {
    awra_measure(x->end, o, -1, &b_min, &b_nat);
  }
  if (o == x->orientation) {
    *r_min = a_min + b_min + awra_px(1);
    *r_nat = a_nat + b_nat + awra_px(1);
  }
  else {
    *r_min = BK_MAX(a_min, b_min);
    *r_nat = BK_MAX(a_nat, b_nat);
  }
}

static void paned_allocate(AwraWidget *w)
{
  PanedExt *x = PNX(w);
  const bool h = x->orientation == AWRA_ORIENTATION_HORIZONTAL;
  const int total = h ? w->w : w->h;
  const int one = awra_px(1);
  const bool has_a = x->start && awra_child_visible(x->start);
  const bool has_b = x->end && awra_child_visible(x->end);
  if (!has_a || !has_b) {
    AwraWidget *only = has_a ? x->start : has_b ? x->end : NULL;
    if (only) {
      awra_allocate(only, w->x, w->y, w->w, w->h);
    }
    x->sep = -1;
    return;
  }
  int amin, anat, bmin, bnat;
  awra_measure(x->start, x->orientation, -1, &amin, &anat);
  awra_measure(x->end, x->orientation, -1, &bmin, &bnat);
  x->min_start = amin;
  x->min_end = bmin;
  int a = (int)floor((total - one) * x->fraction + 0.5);
  a = BK_CLAMP(a, amin, BK_MAX(amin, total - one - bmin));
  x->sep = a;
  if (h) {
    awra_allocate(x->start, w->x, w->y, a, w->h);
    awra_allocate(x->end, w->x + a + one, w->y, BK_MAX(0, total - a - one), w->h);
  }
  else {
    awra_allocate(x->start, w->x, w->y, w->w, a);
    awra_allocate(x->end, w->x, w->y + a + one, w->w, BK_MAX(0, total - a - one));
  }
}

static void paned_draw_over(AwraWidget *w, AwraDraw *d)
{
  PanedExt *x = PNX(w);
  if (x->sep < 0) {
    return;
  }
  const AwraRGBA *c = &awra_tokens()->separator;
  if (x->orientation == AWRA_ORIENTATION_HORIZONTAL) {
    /* right to left, the start child is at the right (GtkPaned) */
    awra_paint_rect(d, awra_mirror_box(w, (AwraBox){(float)(w->x + x->sep), (float)w->y, awra_pxf(1), (float)w->h}), c);
  }
  else {
    awra_paint_rect(d, (AwraBox){(float)w->x, (float)(w->y + x->sep), (float)w->w, awra_pxf(1)}, c);
  }
}

static bool on_handle(AwraWidget *w, int px, int py)
{
  PanedExt *x = PNX(w);
  if (x->sep < 0) {
    return false;
  }
  const int along = x->orientation == AWRA_ORIENTATION_HORIZONTAL ? awra_mirror_px(w, px) - w->x : py - w->y;
  return abs(along - x->sep) <= awra_px(HANDLE / 2) + 1;
}

/* the handle sits above the children: hits there are the paned's */
static BKNode *paned_hit(BKNode *n, int px, int py)
{
  if (!bk_contains(n, px, py) || !(n->flags & NF_VISIBLE)) {
    return NULL;
  }
  if (on_handle(n, px, py)) {
    return n;
  }
  return awra_bk_hit(n, px, py);
}

static int paned_press(AwraWidget *w, BKEvent *e)
{
  PanedExt *x = PNX(w);
  BKContext *ctx = bk_ctx(w);
  if (ctx && ctx->last_click_node == NULL) {
    x->fraction = 0.5;
    x->dragging = false;
    awra_queue_allocate(w);
    awra_invalidate(w);
    awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "position");
    return 1;
  }
  x->dragging = true;
  x->frac0 = x->fraction;
  return 1;
}

static int paned_drag(AwraWidget *w, BKEvent *e)
{
  PanedExt *x = PNX(w);
  if (!x->dragging) {
    return 1;
  }
  const bool h = x->orientation == AWRA_ORIENTATION_HORIZONTAL;
  const int total = (h ? w->w : w->h) - awra_px(1);
  if (total <= 0) {
    return 1;
  }
  const int dx = awra_widget_rtl(w) ? -e->dx : e->dx;
  const double f = x->frac0 + (double)(h ? dx : e->dy) / total;
  x->fraction = BK_CLAMP(f, (double)x->min_start / total, 1.0 - (double)x->min_end / total);
  awra_queue_allocate(w);
  awra_invalidate(w);
  return 1;
}

static int paned_release(AwraWidget *w, BKEvent *e)
{
  PanedExt *x = PNX(w);
  if (x->dragging) {
    x->dragging = false;
    awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "position");
  }
  return 1;
}

static int paned_cursor(AwraWidget *w, int px, int py)
{
  if (on_handle(w, px, py)) {
    return PNX(w)->orientation == AWRA_ORIENTATION_HORIZONTAL ? BK_CURSOR_EW : BK_CURSOR_NS;
  }
  return -1;
}

static AwraClass paned_class = {
    .bk = AWRA_BK("AwraPaned", 0),
    .type = "AwraPaned",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(PanedExt),
    .measure = paned_measure,
    .allocate = paned_allocate,
    .draw_over = paned_draw_over,
    .press = paned_press,
    .drag = paned_drag,
    .release = paned_release,
    .cursor = paned_cursor,
};

AwraWidget *awra_paned_new(AwraOrientation orientation)
{
  paned_class.bk.hit = paned_hit;
  AwraWidget *w = awra_widget_new(&paned_class);
  PanedExt *x = PNX(w);
  x->orientation = orientation;
  x->fraction = 0.5;
  x->sep = -1;
  return w;
}

void awra_paned_set_start_child(AwraWidget *w, AwraWidget *child)
{
  awra_set_slot(w, &PNX(w)->start, child);
}

void awra_paned_set_end_child(AwraWidget *w, AwraWidget *child)
{
  awra_set_slot(w, &PNX(w)->end, child);
}

AwraWidget *awra_paned_start_child(AwraWidget *w)
{
  return PNX(w)->start;
}

AwraWidget *awra_paned_end_child(AwraWidget *w)
{
  return PNX(w)->end;
}

void awra_paned_set_orientation(AwraWidget *w, AwraOrientation orientation)
{
  PNX(w)->orientation = orientation;
  awra_widget_queue_resize(w);
}

AwraOrientation awra_paned_orientation(AwraWidget *w)
{
  return (AwraOrientation)PNX(w)->orientation;
}

void awra_paned_set_fraction(AwraWidget *w, double fraction)
{
  PNX(w)->fraction = BK_CLAMP(fraction, 0.0, 1.0);
  awra_queue_allocate(w);
  awra_invalidate(w);
}

double awra_paned_fraction(AwraWidget *w)
{
  return PNX(w)->fraction;
}
