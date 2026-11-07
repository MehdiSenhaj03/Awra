/* Awra 2 transition bin (Awra 1's AwraTransitionBin): its child changes in
 * place (a folder's rows, a preview) and the change plays as a short
 * motion. The frame shown before the change drifts away and fades out; the
 * new one comes in from (dx, dy) and fades in, a little later, so two sets
 * of text are never legible over each other. A strip at the top (a list's
 * column header, which does not change) can stay where it is.
 *
 * The old frame is copied from the window's last frame when the change is
 * played (the software renderer keeps it); it lives as long as the motion.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include "GPU_raster.h"

/* The old frame only drifts: it must read as leaving, not as moving away. */
#define OUTGOING_DRIFT 0.45
/* Fade through: the old frame is gone before the new one is fully there. */
#define OUTGOING_SPAN 0.45
#define INCOMING_DELAY 0.2

typedef struct TransitionExt {
  AwraWidget *child;
  AwraTexture *outgoing; /* the frame before the change, raster px */
  int fixed_top;         /* logical px kept in place at the top */
  float offset[2];       /* logical px the new frame comes from */
  float shift[2];        /* raster px the child is moved by, now */
  float child_opacity;   /* the child's own, given back at the end */
  double started;
  unsigned ms;     /* this motion's duration */
  int preset;      /* AwraMotionPreset of the next ones (0: Normal) */
  double progress; /* 0..1, linear */
  bool playing;
} TransitionExt;

#define TRX(w) AWRA_EXT(w, TransitionExt)

static double incoming_of(double progress)
{
  const double t = (progress - INCOMING_DELAY) / (1.0 - INCOMING_DELAY);
  return awra_ease_out_cubic(t < 0 ? 0 : t > 1 ? 1 : t);
}

static void transition_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  TransitionExt *x = TRX(w);
  *r_min = *r_nat = 0;
  if (x->child) {
    awra_measure(x->child, o, for_size, r_min, r_nat);
  }
}

static void transition_allocate(AwraWidget *w)
{
  TransitionExt *x = TRX(w);
  if (x->child) {
    awra_allocate(x->child, w->x + (int)lroundf(x->shift[0]), w->y + (int)lroundf(x->shift[1]), w->w,
                  w->h);
  }
}

static int fixed_px(const AwraWidget *w)
{
  const int top = awra_px(TRX(w)->fixed_top);
  return top < w->h ? top : w->h;
}

/* Below the children: the old frame on its way out. */
static void transition_draw(AwraWidget *w, AwraDraw *d)
{
  TransitionExt *x = TRX(w);
  if (!x->playing || !x->outgoing || x->progress >= OUTGOING_SPAN) {
    return;
  }
  const double eased = awra_ease_out_cubic(x->progress);
  const float dx = (float)(-awra_pxf(x->offset[0]) * OUTGOING_DRIFT * eased);
  const float dy = (float)(-awra_pxf(x->offset[1]) * OUTGOING_DRIFT * eased);
  const float saved = d->opacity;
  d->opacity = saved * (float)(1.0 - x->progress / OUTGOING_SPAN);
  const int top = fixed_px(w);
  bk_clip_push(w->x, w->y + top, w->w, w->h - top);
  awra_paint_texture(d, x->outgoing, (AwraBox){w->x + dx, w->y + dy, (float)w->w, (float)w->h}, 0.0f, 0);
  bk_clip_pop();
  d->opacity = saved;
}

/* Above the children: the fixed strip as it was, still, over the new frame
 * sliding in under it. */
static void transition_draw_over(AwraWidget *w, AwraDraw *d)
{
  TransitionExt *x = TRX(w);
  const int top = fixed_px(w);
  if (!x->playing || !x->outgoing || top <= 0) {
    return;
  }
  bk_clip_push(w->x, w->y, w->w, top);
  awra_paint_texture(d, x->outgoing, awra_box_of(w), 0.0f, 0);
  bk_clip_pop();
}

static void transition_finish(AwraWidget *w)
{
  TransitionExt *x = TRX(w);
  if (x->outgoing) {
    awra_texture_unref(x->outgoing);
    x->outgoing = NULL;
  }
  if (x->playing && x->child) {
    awra_aux(x->child)->opacity = x->child_opacity;
  }
  x->playing = false;
  x->progress = 1.0;
  x->shift[0] = x->shift[1] = 0.0f;
  awra_queue_allocate(w);
  awra_invalidate(w);
}

static void transition_anim(AwraWidget *w, double t, void *data)
{
  (void)t;
  (void)data;
  TransitionExt *x = TRX(w);
  if (!x->playing) {
    return;
  }
  x->progress = x->ms > 0 ? (awra_now_ms() - x->started) / x->ms : 1.0;
  if (x->progress >= 1.0) {
    return; /* transition_done gives everything back */
  }
  const double incoming = incoming_of(x->progress);
  x->shift[0] = (float)(awra_pxf(x->offset[0]) * (1.0 - incoming));
  x->shift[1] = (float)(awra_pxf(x->offset[1]) * (1.0 - incoming));
  if (x->child) {
    awra_aux(x->child)->opacity = x->child_opacity * (float)incoming;
  }
  if (x->shift[0] != 0.0f || x->shift[1] != 0.0f) {
    awra_aux(w)->needs_allocate = 1;
    awra_queue_allocate(w);
  }
  awra_invalidate(w);
}

static void transition_done(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  transition_finish(w);
  awra_widget_unref(w);
}

static void transition_map(AwraWidget *w, bool mapped)
{
  if (!mapped && TRX(w)->playing) {
    awra_anim_stop_for(w);
  }
}

static void transition_remove(AwraWidget *w, AwraWidget *child)
{
  TransitionExt *x = TRX(w);
  if (x->child == child) {
    if (x->playing) {
      awra_aux(child)->opacity = x->child_opacity;
      x->playing = false;
    }
    x->child = NULL;
  }
}

static void transition_finalize(AwraWidget *w)
{
  TransitionExt *x = TRX(w);
  if (x->outgoing) {
    awra_texture_unref(x->outgoing);
    x->outgoing = NULL;
  }
}

static AwraClass transition_class = {
    .bk = AWRA_BK("AwraTransitionBin", 0),
    .type = "AwraTransitionBin",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_CLIP,
    .ext_size = sizeof(TransitionExt),
    .measure = transition_measure,
    .allocate = transition_allocate,
    .draw = transition_draw,
    .draw_over = transition_draw_over,
    .map = transition_map,
    .remove = transition_remove,
    .finalize = transition_finalize,
};

AwraWidget *awra_transition_bin_new(void)
{
  return awra_widget_new(&transition_class);
}

void awra_transition_bin_set_child(AwraWidget *w, AwraWidget *child)
{
  TransitionExt *x = TRX(w);
  if (x->playing) {
    awra_anim_stop_for(w);
  }
  awra_set_slot(w, &x->child, child);
  awra_widget_queue_resize(w);
}

AwraWidget *awra_transition_bin_child(AwraWidget *w)
{
  return TRX(w)->child;
}

void awra_transition_bin_set_fixed_top(AwraWidget *w, int height)
{
  TRX(w)->fixed_top = height < 0 ? 0 : height;
}

void awra_transition_bin_set_motion(AwraWidget *w, AwraMotionPreset preset)
{
  TRX(w)->preset = (int)preset;
}

void awra_transition_bin_play(AwraWidget *w, double dx, double dy)
{
  TransitionExt *x = TRX(w);
  const unsigned ms = awra_motion_duration(x->preset ? (AwraMotionPreset)x->preset : AWRA_MOTION_NORMAL);
  if (!x->child || ms == 0 || !awra_widget_mapped(w) || w->w <= 0 || w->h <= 0) {
    return;
  }
  /* a motion already running ends where it is: the frame it shows is the
   * one leaving now */
  if (x->playing) {
    awra_anim_stop_for(w);
  }
  BuiRaster *r = awra_window_raster(awra_window_of(w));
  if (r && !BUI_raster_is_gpu(r) && BUI_raster_pixels(r)) {
    x->outgoing = awra_texture_new_argb_region(BUI_raster_pixels(r), BUI_raster_width(r),
                                               BUI_raster_height(r), w->x, w->y, w->w, w->h);
  }
  x->offset[0] = (float)dx;
  x->offset[1] = (float)dy;
  x->child_opacity = awra_aux(x->child)->opacity;
  x->started = awra_now_ms();
  x->ms = ms;
  x->progress = 0.0;
  x->playing = true;
  awra_widget_ref(w);
  awra_anim_start(w, ms, transition_anim, w, transition_done);
}
