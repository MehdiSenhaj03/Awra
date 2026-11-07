/* Awra 2 layout helpers: the revealer (GtkRevealer), application-defined
 * layouts (a GtkWidget subclass with its own measure and size_allocate),
 * the aspect frame (GtkAspectFrame) and the window handle (GtkWindowHandle).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

/* ======================================================================= */
/* Revealer (gtkrevealer.c) */

typedef struct RevealerExt {
  AwraWidget *child;
  AwraRevealerTransition transition;
  unsigned duration; /* ms */
  bool reveal;       /* target */
  double from, to;   /* progress animation */
  double progress;   /* 0 hidden .. 1 shown */
  bool animating;
} RevealerExt;

#define RVX(w) AWRA_EXT(w, RevealerExt)

static bool revealer_slides(const RevealerExt *x, int orientation)
{
  if (orientation == AWRA_ORIENTATION_HORIZONTAL) {
    return x->transition == AWRA_REVEALER_SLIDE_LEFT || x->transition == AWRA_REVEALER_SLIDE_RIGHT ||
           x->transition == AWRA_REVEALER_SWING_LEFT || x->transition == AWRA_REVEALER_SWING_RIGHT;
  }
  return x->transition == AWRA_REVEALER_SLIDE_UP || x->transition == AWRA_REVEALER_SLIDE_DOWN ||
         x->transition == AWRA_REVEALER_SWING_UP || x->transition == AWRA_REVEALER_SWING_DOWN;
}

static void revealer_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  RevealerExt *x = RVX(w);
  int mn = 0, nt = 0;
  /* gtk_revealer_measure measures a visible child whatever the progress:
   * across the sliding direction a closed revealer keeps its size */
  if (x->child && awra_widget_visible(x->child)) {
    /* hidden while closed (child-visible false), measured all the same */
    const bool closed = !(x->child->flags & NF_VISIBLE);
    x->child->flags |= NF_VISIBLE;
    awra_measure(x->child, o, for_size, &mn, &nt);
    if (closed) {
      x->child->flags &= ~NF_VISIBLE;
    }
  }
  const bool scaled = revealer_slides(x, o) || x->transition == AWRA_REVEALER_CROSSFADE ||
                      x->transition == AWRA_REVEALER_NONE;
  if (scaled && x->progress < 1.0) {
    /* gtk_revealer_measure: sizes scaled by the progress, rounded up
     * (get_child_size_scale: crossfade and none in both directions) */
    mn = (int)ceil(mn * x->progress);
    nt = (int)ceil(nt * x->progress);
  }
  *r_min = mn;
  *r_nat = nt;
}

static void revealer_allocate(AwraWidget *w)
{
  RevealerExt *x = RVX(w);
  if (!x->child || x->progress <= 0.0) {
    return;
  }
  int cw = w->w, ch = w->h;
  int mn, nt;
  /* the child keeps its natural size in the sliding direction */
  if (revealer_slides(x, AWRA_ORIENTATION_VERTICAL) && x->progress < 1.0) {
    awra_measure(x->child, AWRA_ORIENTATION_VERTICAL, cw, &mn, &nt);
    ch = BK_MAX(ch, mn);
  }
  else if (revealer_slides(x, AWRA_ORIENTATION_HORIZONTAL) && x->progress < 1.0) {
    awra_measure(x->child, AWRA_ORIENTATION_HORIZONTAL, ch, &mn, &nt);
    cw = BK_MAX(cw, mn);
  }
  int cx = w->x, cy = w->y;
  if (x->transition == AWRA_REVEALER_SLIDE_DOWN || x->transition == AWRA_REVEALER_SWING_DOWN) {
    cy += w->h - ch;
  }
  else if (x->transition == AWRA_REVEALER_SLIDE_RIGHT || x->transition == AWRA_REVEALER_SWING_RIGHT) {
    cx += w->w - cw;
  }
  awra_allocate(x->child, cx, cy, cw, ch);
}

static void revealer_sync(AwraWidget *w)
{
  RevealerExt *x = RVX(w);
  if (x->child) {
    /* gtk_widget_set_child_visible(child, progress > 0) */
    awra_widget_set_child_visible(x->child, x->progress > 0.0);
    const bool fade = x->transition == AWRA_REVEALER_CROSSFADE;
    awra_aux(x->child)->opacity = fade ? (float)x->progress : 1.0f;
  }
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

static void revealer_anim(AwraWidget *w, double t, void *data)
{
  (void)data;
  RevealerExt *x = RVX(w);
  /* GTK eases the revealer out (cubic) */
  x->progress = x->from + (x->to - x->from) * t;
  revealer_sync(w);
}

static void revealer_done(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  RevealerExt *x = RVX(w);
  x->animating = false;
  x->progress = x->to;
  revealer_sync(w);
  awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "child-revealed");
  awra_widget_unref(w);
}

static void revealer_map(AwraWidget *w, bool mapped)
{
  RevealerExt *x = RVX(w);
  if (!mapped && x->animating) {
    awra_anim_stop_for(w);
  }
}

static bool revealer_get_double(AwraWidget *w, const char *name, double *r)
{
  if (!strcmp(name, "progress")) {
    *r = RVX(w)->progress;
    return true;
  }
  return false;
}

static AwraClass revealer_class = {
    .bk = AWRA_BK("AwraRevealer", 0),
    .type = "AwraRevealer",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_CLIP,
    .ext_size = sizeof(RevealerExt),
    .measure = revealer_measure,
    .allocate = revealer_allocate,
    .map = revealer_map,
    .get_double = revealer_get_double,
};

AwraWidget *awra_revealer_new(void)
{
  AwraWidget *w = awra_widget_new(&revealer_class);
  RevealerExt *x = RVX(w);
  x->transition = AWRA_REVEALER_SLIDE_DOWN;
  x->duration = 250;
  return w;
}

void awra_revealer_set_child(AwraWidget *w, AwraWidget *child)
{
  awra_set_slot(w, &RVX(w)->child, child);
  revealer_sync(w);
}

AwraWidget *awra_revealer_child(AwraWidget *w)
{
  return RVX(w)->child;
}

void awra_revealer_set_transition(AwraWidget *w, AwraRevealerTransition transition, unsigned ms)
{
  RevealerExt *x = RVX(w);
  x->transition = transition;
  x->duration = ms;
  revealer_sync(w);
}

void awra_revealer_set_reveal_child(AwraWidget *w, bool reveal)
{
  RevealerExt *x = RVX(w);
  if (x->reveal == reveal && (x->animating || x->progress == (reveal ? 1.0 : 0.0))) {
    return;
  }
  x->reveal = reveal;
  const double target = reveal ? 1.0 : 0.0;
  const unsigned ms = x->transition == AWRA_REVEALER_NONE ? 0 :
                      awra_style_reduced_motion()         ? 0 :
                                                            x->duration;
  if (x->animating) {
    awra_anim_stop_for(w);
    x->animating = false;
  }
  if (ms == 0 || !awra_aux(w)->mapped) {
    x->progress = target;
    x->to = target;
    revealer_sync(w);
    awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "child-revealed");
    return;
  }
  x->from = x->progress;
  x->to = target;
  /* the remaining part of the way takes its share of the duration */
  const unsigned dur = (unsigned)(ms * fabs(target - x->progress));
  x->animating = true;
  awra_widget_ref(w);
  awra_anim_start(w, dur ? dur : 1, revealer_anim, w, revealer_done);
}

bool awra_revealer_reveal_child(AwraWidget *w)
{
  return RVX(w)->reveal;
}

bool awra_revealer_child_revealed(AwraWidget *w)
{
  const RevealerExt *x = RVX(w);
  return x->reveal ? x->progress >= 1.0 : x->progress > 0.0;
}

/* ======================================================================= */
/* Layouts the application defines */

typedef struct LayoutExt {
  AwraLayoutMeasureFunc measure;
  AwraLayoutAllocateFunc allocate;
  void *data;
  AwraDestroyNotify destroy;
} LayoutExt;

#define LYX(w) AWRA_EXT(w, LayoutExt)

static void layout_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  LayoutExt *x = LYX(w);
  const float s = awra_scale();
  int mn = 0, nt = 0;
  if (x->measure) {
    x->measure(w, (AwraOrientation)o, for_size >= 0 ? (int)floorf(for_size / s) : -1, &mn, &nt, x->data);
  }
  else {
    /* GtkBinLayout: the largest child */
    for (int i = 0; i < w->nkids; i++) {
      if (!awra_child_visible(w->kids[i])) {
        continue;
      }
      int cm, cn;
      awra_measure(w->kids[i], o, for_size, &cm, &cn);
      mn = BK_MAX(mn, cm);
      nt = BK_MAX(nt, cn);
    }
    *r_min = mn;
    *r_nat = nt;
    return;
  }
  *r_min = awra_px(mn);
  *r_nat = awra_px(BK_MAX(mn, nt));
}

static void layout_allocate(AwraWidget *w)
{
  LayoutExt *x = LYX(w);
  if (x->allocate) {
    awra_widget_ref(w);
    x->allocate(w, awra_widget_width(w), awra_widget_height(w), x->data);
    awra_widget_unref(w);
    return;
  }
  for (int i = 0; i < w->nkids; i++) {
    awra_allocate(w->kids[i], w->x, w->y, w->w, w->h);
  }
}

static void layout_finalize(AwraWidget *w)
{
  LayoutExt *x = LYX(w);
  if (x->destroy) {
    x->destroy(x->data);
  }
}

static AwraClass layout_class = {
    .bk = AWRA_BK("AwraLayout", 0),
    .type = "AwraLayout",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(LayoutExt),
    .finalize = layout_finalize,
    .measure = layout_measure,
    .allocate = layout_allocate,
};

AwraWidget *awra_layout_new(AwraLayoutMeasureFunc measure, AwraLayoutAllocateFunc allocate,
                            void *data, AwraDestroyNotify destroy)
{
  AwraWidget *w = awra_widget_new(&layout_class);
  LayoutExt *x = LYX(w);
  x->measure = measure;
  x->allocate = allocate;
  x->data = data;
  x->destroy = destroy;
  return w;
}

void awra_layout_set_clip(AwraWidget *w, bool clip)
{
  /* a per-widget overflow: the layout keeps a class of its own when it clips */
  static AwraClass clip_class;
  if (!clip_class.type) {
    clip_class = layout_class;
    clip_class.bk.name = "AwraLayout";
    clip_class.flags |= AWRA_CLASS_CLIP;
  }
  w->cls = clip ? &clip_class.bk : &layout_class.bk;
  awra_invalidate(w);
}

void awra_widget_allocate(AwraWidget *child, int x, int y, int width, int height)
{
  AwraWidget *p = (AwraWidget *)child->parent;
  if (!p) {
    return;
  }
  awra_allocate(child, p->x + awra_px(x), p->y + awra_px(y), awra_px(width), awra_px(height));
}

/* ======================================================================= */
/* Aspect frame (GtkAspectFrame with obey-child off) */

typedef struct AspectExt {
  AwraWidget *child;
  float ratio, xalign, yalign;
} AspectExt;

#define ASX(w) AWRA_EXT(w, AspectExt)

static void aspect_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  AspectExt *x = ASX(w);
  if (!x->child) {
    *r_min = *r_nat = 0;
    return;
  }
  awra_measure(x->child, o, for_size, r_min, r_nat);
}

static void aspect_allocate(AwraWidget *w)
{
  AspectExt *x = ASX(w);
  if (!x->child) {
    return;
  }
  const float ratio = x->ratio > 0 ? x->ratio : 1.0f;
  int cw, ch;
  if ((float)w->w / BK_MAX(1, w->h) > ratio) {
    ch = w->h;
    cw = (int)(ch * ratio + 0.5f);
  }
  else {
    cw = w->w;
    ch = (int)(cw / ratio + 0.5f);
  }
  const int cx = w->x + (int)((w->w - cw) * x->xalign);
  const int cy = w->y + (int)((w->h - ch) * x->yalign);
  awra_allocate(x->child, cx, cy, cw, ch);
}

static AwraClass aspect_class = {
    .bk = AWRA_BK("AwraAspectFrame", 0),
    .type = "AwraAspectFrame",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(AspectExt),
    .measure = aspect_measure,
    .allocate = aspect_allocate,
};

AwraWidget *awra_aspect_frame_new(float xalign, float yalign, float ratio)
{
  AwraWidget *w = awra_widget_new(&aspect_class);
  AspectExt *x = ASX(w);
  x->xalign = xalign;
  x->yalign = yalign;
  x->ratio = ratio;
  return w;
}

void awra_aspect_frame_set_child(AwraWidget *w, AwraWidget *child)
{
  awra_set_slot(w, &ASX(w)->child, child);
}

void awra_aspect_frame_set_ratio(AwraWidget *w, float ratio)
{
  ASX(w)->ratio = ratio;
  awra_queue_allocate(w);
  awra_invalidate(w);
}

/* ======================================================================= */
/* Window handle: dragging it moves the window, a double click maximizes */

typedef struct HandleExt {
  AwraWidget *child;
  bool dragging;
} HandleExt;

#define HDX(w) AWRA_EXT(w, HandleExt)

static void handle_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  HandleExt *x = HDX(w);
  *r_min = *r_nat = 0;
  if (x->child) {
    awra_measure(x->child, o, for_size, r_min, r_nat);
  }
}

static void handle_allocate(AwraWidget *w)
{
  if (HDX(w)->child) {
    awra_allocate(HDX(w)->child, w->x, w->y, w->w, w->h);
  }
}

static int handle_press(AwraWidget *w, BKEvent *e)
{
  (void)e;
  HDX(w)->dragging = false;
  return 1;
}

void awra_window_begin_move_internal(AwraWidget *win);

static int handle_drag(AwraWidget *w, BKEvent *e)
{
  HandleExt *x = HDX(w);
  AwraWidget *win = awra_window_of(w);
  if (x->dragging || !win || (abs(e->dx) < 4 && abs(e->dy) < 4)) {
    return 1;
  }
  x->dragging = true;
  awra_window_begin_move_internal(win);
  return 1;
}

static int handle_release(AwraWidget *w, BKEvent *e)
{
  BKContext *ctx = bk_ctx(w);
  AwraWidget *win = awra_window_of(w);
  if (ctx && win && !HDX(w)->dragging && ctx->last_click_node == NULL && e->inside) {
    if (awra_window_state(win) & AWRA_WINDOW_STATE_MAXIMIZED) {
      awra_window_unmaximize(win);
    }
    else {
      awra_window_maximize(win);
    }
  }
  HDX(w)->dragging = false;
  return 1;
}

static AwraClass handle_class = {
    .bk = AWRA_BK("AwraWindowHandle", 0),
    .type = "AwraWindowHandle",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(HandleExt),
    .measure = handle_measure,
    .allocate = handle_allocate,
    .press = handle_press,
    .drag = handle_drag,
    .release = handle_release,
};

AwraWidget *awra_window_handle_new(void)
{
  AwraWidget *w = awra_widget_new(&handle_class);
  w->flags |= NF_EVENTS;
  return w;
}

void awra_window_handle_set_child(AwraWidget *w, AwraWidget *child)
{
  awra_set_slot(w, &HDX(w)->child, child);
}
