/* Awra 2 header (title bar): start / center / end slots, titles and the
 * window controls, dragging and double-click maximize (Awra 1's AwraHeader,
 * spec A §3: content 58 px + 8 / 20 px padding = 74 px).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

extern AwraClass awra_button_class;
void awra_button_set_look(AwraWidget *w, int look);

typedef struct HeaderExt {
  AwraWidget *start_box, *end_box, *center, *titles, *title, *subtitle;
  AwraWidget *controls;           /* box of window control buttons */
  AwraWidget *minimize, *maximize, *close;
  bool show_controls, blend, drag, compact, expand_center;
  bool close_only; /* awra_header_set_close_only */
  bool dragging;
  int material;     /* when not blended (default Chrome) */
  int max_width;    /* logical, 0: none */
  AwraWidget *backdrop;
  bool drew_backdrop;
} HeaderExt;

bool awra_paint_material_backdrop(AwraDraw *d, AwraBox b, AwraMaterial material, int role,
                                  AwraElevation elevation, float radius, int corner_mask,
                                  int edge_mask, AwraWidget *source);

#define HX(w) AWRA_EXT(w, HeaderExt)

static void header_paddings(AwraWidget *w, int *px, int *py, int *min_h)
{
  const HeaderExt *x = HX(w);
  if (x->compact) {
    *px = awra_px(14);
    *py = awra_px(6);
    *min_h = awra_px(36);
  }
  else if (awra_widget_get_data(w, "awra-dialog-header")) {
    *px = awra_px(16);
    *py = awra_px(6);
    *min_h = awra_px(48);
  }
  else {
    *px = awra_px(20);
    *py = awra_px(8);
    *min_h = awra_px(58);
  }
}

static void header_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  HeaderExt *x = HX(w);
  int px, py, mh;
  header_paddings(w, &px, &py, &mh);
  int smin, snat, emin, enat, cmin = 0, cnat = 0;
  awra_measure(x->start_box, o, -1, &smin, &snat);
  awra_measure(x->end_box, o, -1, &emin, &enat);
  if (x->center) {
    awra_measure(x->center, o, -1, &cmin, &cnat);
  }
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    *r_min = smin + emin + cmin + 2 * px;
    *r_nat = cnat + 2 * BK_MAX(snat, enat) + 2 * px;
    if (x->center && awra_widget_hexpand(x->center)) {
      cnat = BK_MAX(cnat, awra_px(568));
      *r_nat = cnat + 2 * BK_MAX(snat, enat) + 2 * px;
    }
    if (x->expand_center) {
      *r_nat = snat + cnat + enat + 2 * px;
    }
    if (x->max_width > 0) {
      /* Awra 1: the header spans max-width (centered) when there is room */
      *r_nat = BK_MAX(*r_min, awra_px(x->max_width));
    }
  }
  else {
    *r_min = BK_MAX(mh, BK_MAX(smin, BK_MAX(emin, cmin))) + 2 * py;
    *r_nat = BK_MAX(mh, BK_MAX(snat, BK_MAX(enat, cnat))) + 2 * py;
  }
}

static void header_allocate(AwraWidget *w)
{
  HeaderExt *x = HX(w);
  int px, py, mh;
  header_paddings(w, &px, &py, &mh);
  const int W = BK_MAX(0, w->w - 2 * px), H = BK_MAX(0, w->h - 2 * py);
  const int X = w->x + px, Y = w->y + py;
  int smin, snat, emin, enat, cmin = 0, cnat = 0;
  awra_measure(x->start_box, AWRA_ORIENTATION_HORIZONTAL, H, &smin, &snat);
  awra_measure(x->end_box, AWRA_ORIENTATION_HORIZONTAL, H, &emin, &enat);
  if (x->center) {
    awra_measure(x->center, AWRA_ORIENTATION_HORIZONTAL, H, &cmin, &cnat);
    /* an expanding centre asks for 568 (awra-header.c AwraHeaderCenter) */
    if (awra_widget_hexpand(x->center)) {
      cnat = BK_MAX(cnat, awra_px(568));
    }
  }
  const int avail = BK_MAX(0, W - cmin);
  const int start_w = BK_MAX(smin, BK_MIN(snat, avail - emin));
  const int end_w = BK_MAX(emin, BK_MIN(enat, avail - start_w));
  awra_allocate(x->start_box, X, Y, start_w, H);
  awra_allocate(x->end_box, X + W - end_w, Y, end_w, H);
  if (x->center) {
    const int gap_start = start_w, gap_end = W - end_w;
    const int gap = BK_MAX(0, gap_end - gap_start);
    /* expand-center: the centre takes the whole gap; otherwise it keeps
     * its preferred span */
    int cw = x->expand_center ? gap : BK_MIN(cnat, gap);
    cw = BK_MAX(cw, BK_MIN(cmin, gap));
    int cx = (W - cw) / 2;
    cx = BK_CLAMP(cx, gap_start, BK_MAX(gap_start, gap_end - cw));
    awra_allocate(x->center, X + cx, Y, cw, H);
  }
}

static void header_draw(AwraWidget *w, AwraDraw *d)
{
  HeaderExt *x = HX(w);
  if (x->blend) {
    return; /* the window's Canvas is under it */
  }
  AwraResolvedMaterial m;
  awra_material_resolve((AwraMaterial)x->material, AWRA_ROLE_GENERIC, AWRA_ELEVATION_FLAT, false, &m);
  /* the fill under the content, the outline and rim over it
   * (header_draw_over), as AwraHeader's surface */
  x->drew_backdrop = awra_paint_material_backdrop(d, awra_box_of(w), (AwraMaterial)x->material,
                                                  AWRA_ROLE_GENERIC, AWRA_ELEVATION_FLAT,
                                                  awra_pxf(m.radius), 15, 0, x->backdrop);
  if (!x->drew_backdrop) {
    awra_paint_material(d, awra_box_of(w), &m, awra_pxf(m.radius), 15, 0, AWRA_ELEVATION_FLAT);
  }
}

static void header_draw_over(AwraWidget *w, AwraDraw *d)
{
  HeaderExt *x = HX(w);
  if (x->blend) {
    return;
  }
  AwraResolvedMaterial m;
  awra_material_resolve((AwraMaterial)x->material, AWRA_ROLE_GENERIC, AWRA_ELEVATION_FLAT,
                        x->drew_backdrop, &m);
  awra_paint_material_edges(d, awra_box_of(w), &m, awra_pxf(m.radius), 15, AWRA_EDGE_ALL);
}

static int header_press(AwraWidget *w, BKEvent *e)
{
  HX(w)->dragging = false;
  return 1;
}

static int header_drag(AwraWidget *w, BKEvent *e)
{
  HeaderExt *x = HX(w);
  AwraWidget *win = awra_window_of(w);
  if (!x->drag || x->dragging || !win) {
    return 0;
  }
  x->dragging = true;
  void *h = awra_window_native_handle(win);
  if (h) {
    extern void awra_window_begin_move_internal(AwraWidget *win);
    awra_window_begin_move_internal(win);
  }
  return 1;
}

static int header_release(AwraWidget *w, BKEvent *e)
{
  BKContext *ctx = bk_ctx(w);
  AwraWidget *win = awra_window_of(w);
  /* a double click (the kit cleared last_click_node on it) toggles maximize */
  if (ctx && win && !HX(w)->dragging && ctx->last_click_node == NULL && e->inside) {
    if (awra_window_state(win) & AWRA_WINDOW_STATE_MAXIMIZED) {
      awra_window_unmaximize(win);
    }
    else {
      awra_window_maximize(win);
    }
  }
  HX(w)->dragging = false;
  return 1;
}

static void header_finalize(AwraWidget *w)
{
  if (HX(w)->backdrop) {
    awra_widget_unref(HX(w)->backdrop);
  }
}

static AwraClass header_class = {
    .bk = AWRA_BK("AwraHeader", 0),
    .type = "AwraHeader",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(HeaderExt),
    .finalize = header_finalize,
    .measure = header_measure,
    .allocate = header_allocate,
    .draw = header_draw,
    .draw_over = header_draw_over,
    .press = header_press,
    .drag = header_drag,
    .release = header_release,
};

/* ----------------------------------------------------------------------- */

static void on_control(AwraWidget *b, const AwraArgs *a, void *data)
{
  AwraWidget *header = (AwraWidget *)data;
  AwraWidget *win = awra_window_of(header);
  HeaderExt *x = HX(header);
  if (!win) {
    return;
  }
  if (b == x->close) {
    awra_window_close_request(win);
  }
  else if (b == x->minimize) {
    awra_window_minimize(win);
  }
  else if (b == x->maximize) {
    if (awra_window_state(win) & AWRA_WINDOW_STATE_MAXIMIZED) {
      awra_window_unmaximize(win);
    }
    else {
      awra_window_maximize(win);
    }
  }
}

static AwraWidget *control_button(AwraWidget *header, const char *icon)
{
  AwraWidget *b = awra_button_new_from_icon_name(icon);
  awra_button_set_look(b, AWRA_LOOK_WINDOW_CONTROL);
  awra_widget_set_focusable(b, false);
  awra_widget_set_valign(b, AWRA_ALIGN_CENTER);
  awra_signal_connect(b, AWRA_SIGNAL_CLICKED, on_control, header, NULL);
  return b;
}

static void sync_titles(AwraWidget *w)
{
  HeaderExt *x = HX(w);
  const char *t = awra_label_text(x->title), *s = awra_label_text(x->subtitle);
  awra_widget_set_visible(x->title, t && t[0]);
  awra_widget_set_visible(x->subtitle, s && s[0]);
  awra_widget_set_visible(x->titles, (t && t[0]) || (s && s[0]));
}

AwraWidget *awra_header_new(void)
{
  AwraWidget *w = awra_widget_new(&header_class);
  HeaderExt *x = HX(w);
  x->drag = true;
  x->show_controls = true;
  x->blend = true;
  x->material = AWRA_MATERIAL_CHROME;
  w->flags |= NF_EVENTS; /* the bar itself takes presses: drags */
  x->start_box = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 12);
  x->end_box = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 8);
  awra_child_insert(w, x->start_box, -1);
  awra_child_insert(w, x->end_box, -1);
  x->titles = awra_box_new(AWRA_ORIENTATION_VERTICAL, 1);
  awra_widget_set_valign(x->titles, AWRA_ALIGN_CENTER);
  x->title = awra_label_new("");
  awra_label_set_xalign(x->title, 0.0f);
  awra_label_set_ellipsize(x->title, AWRA_ELLIPSIZE_END);
  awra_label_set_scale(x->title, 1.08);
  awra_label_set_weight(x->title, 700);
  x->subtitle = awra_label_new("");
  awra_label_set_xalign(x->subtitle, 0.0f);
  awra_label_set_ellipsize(x->subtitle, AWRA_ELLIPSIZE_END);
  awra_widget_set_typography(x->subtitle, AWRA_TYPOGRAPHY_MUTED);
  awra_box_append(x->titles, x->title);
  awra_box_append(x->titles, x->subtitle);
  awra_box_append(x->start_box, x->titles);
  x->controls = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 6);
  awra_widget_set_valign(x->controls, AWRA_ALIGN_CENTER);
  x->minimize = control_button(w, "window-minimize-symbolic");
  x->maximize = control_button(w, "window-maximize-symbolic");
  x->close = control_button(w, "window-close-symbolic");
  /* GtkWindowControls' names */
  awra_accessible_update_property_string(x->minimize, AWRA_ACCESSIBLE_PROPERTY_LABEL, "Minimize");
  awra_accessible_update_property_string(x->maximize, AWRA_ACCESSIBLE_PROPERTY_LABEL, "Maximize");
  awra_accessible_update_property_string(x->close, AWRA_ACCESSIBLE_PROPERTY_LABEL, "Close");
  awra_box_append(x->controls, x->minimize);
  awra_box_append(x->controls, x->maximize);
  awra_box_append(x->controls, x->close);
  awra_box_append(x->end_box, x->controls);
  /* the header's tree holds its parts (they were leaked with it) */
  AwraWidget *parts[] = {x->start_box, x->end_box, x->titles, x->title, x->subtitle,
                         x->controls, x->minimize, x->maximize, x->close};
  for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); i++) {
    awra_widget_unref(parts[i]);
  }
  sync_titles(w);
  return w;
}

AwraWidget *awra_header_new_default(AwraWidget *window);
AwraWidget *awra_header_new_default(AwraWidget *window)
{
  AwraWidget *h = awra_header_new();
  awra_widget_set_data(h, "awra-default-header", (void *)1, NULL);
  awra_header_set_title(h, awra_window_title(window));
  return h;
}

void awra_header_window_state_changed(AwraWidget *header);
void awra_header_window_state_changed(AwraWidget *header)
{
  if (!awra_widget_is(header, "AwraHeader")) {
    return;
  }
  HeaderExt *x = HX(header);
  AwraWidget *win = awra_window_of(header);
  const bool max = win && (awra_window_state(win) & AWRA_WINDOW_STATE_MAXIMIZED);
  /* GtkWindowControls: no maximize for a window that cannot be resized */
  if (win && !awra_widget_get_data(header, "awra-dialog-header")) {
    awra_widget_set_visible(x->maximize, awra_window_get_resizable(win) && !x->close_only);
  }
  awra_button_set_icon_name(x->maximize, max ? "window-restore-symbolic" : "window-maximize-symbolic");
  awra_accessible_update_property_string(x->maximize, AWRA_ACCESSIBLE_PROPERTY_LABEL,
                                         max ? "Restore" : "Maximize");
}

void awra_header_set_title(AwraWidget *w, const char *title)
{
  awra_label_set_text(HX(w)->title, title);
  sync_titles(w);
}

void awra_header_set_subtitle(AwraWidget *w, const char *subtitle)
{
  awra_label_set_text(HX(w)->subtitle, subtitle);
  sync_titles(w);
}

void awra_header_pack_start(AwraWidget *w, AwraWidget *child)
{
  HeaderExt *x = HX(w);
  /* before the titles */
  awra_widget_set_valign(child, AWRA_ALIGN_CENTER);
  awra_child_insert(x->start_box, child, awra_child_index(x->start_box, x->titles));
}

void awra_header_pack_end(AwraWidget *w, AwraWidget *child)
{
  HeaderExt *x = HX(w);
  awra_widget_set_valign(child, AWRA_ALIGN_CENTER);
  awra_child_insert(x->end_box, child, awra_child_index(x->end_box, x->controls));
}

void awra_header_set_center(AwraWidget *w, AwraWidget *child)
{
  HeaderExt *x = HX(w);
  if (child) {
    awra_widget_set_valign(child, AWRA_ALIGN_CENTER);
  }
  awra_set_slot(w, &x->center, child);
}

void awra_header_set_show_controls(AwraWidget *w, bool show)
{
  HX(w)->show_controls = show;
  awra_widget_set_visible(HX(w)->controls, show);
}

void awra_header_set_blend_with_window(AwraWidget *w, bool blend)
{
  HX(w)->blend = blend;
  awra_invalidate(w);
}

void awra_header_set_drag_enabled(AwraWidget *w, bool drag);
void awra_header_set_drag_enabled(AwraWidget *w, bool drag)
{
  HX(w)->drag = drag;
}

void awra_header_set_expand_center(AwraWidget *w, bool expand)
{
  HX(w)->expand_center = expand;
  awra_widget_queue_resize(w);
}

/* Toolbar buttons inside a compact header are compact too (awra.css
 * awraheader.awra-compact .awra-toolbar-button: 5 x 8 padding, 22 px min). */
bool awra_in_compact_header(const AwraWidget *w)
{
  for (const BKNode *p = w->parent; p; p = p->parent) {
    if (awra_is_widget(p) && awra_widget_is(p, "AwraHeader")) {
      return HX(p)->compact;
    }
  }
  return false;
}

void awra_header_set_compact(AwraWidget *w, bool compact);
void awra_header_set_compact(AwraWidget *w, bool compact)
{
  HX(w)->compact = compact;
  awra_widget_queue_resize(w);
}

void awra_header_set_dialog(AwraWidget *w, bool dialog);
void awra_header_set_dialog(AwraWidget *w, bool dialog)
{
  awra_widget_set_data(w, "awra-dialog-header", dialog ? (void *)1 : NULL, NULL);
  HeaderExt *x = HX(w);
  awra_widget_set_visible(x->minimize, !dialog);
  awra_widget_set_visible(x->maximize, !dialog);
  awra_widget_queue_resize(w);
}

/* A window's header with only its close button (a non-resizable transient
 * window: Awra 1's command palette) */
void awra_header_set_close_only(AwraWidget *w);
void awra_header_set_close_only(AwraWidget *w)
{
  HeaderExt *x = HX(w);
  x->close_only = true;
  awra_widget_set_visible(x->minimize, false);
  awra_widget_set_visible(x->maximize, false);
}

void awra_header_set_material(AwraWidget *w, AwraMaterial material)
{
  HX(w)->material = material;
  awra_invalidate(w);
}

void awra_header_set_max_width(AwraWidget *w, int width)
{
  HX(w)->max_width = BK_MAX(0, width);
  awra_widget_set_halign(w, width > 0 ? AWRA_ALIGN_CENTER : AWRA_ALIGN_FILL);
  awra_widget_queue_resize(w);
}

void awra_header_set_backdrop(AwraWidget *w, AwraWidget *source)
{
  HeaderExt *x = HX(w);
  if (source) {
    awra_widget_ref(source);
  }
  if (x->backdrop) {
    awra_widget_unref(x->backdrop);
  }
  x->backdrop = source;
  awra_invalidate(w);
}
