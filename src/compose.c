/* Awra 2 composition: surfaces, cards, pages, sections, forms, action rows,
 * badges, status banners, empty / loading states, the sidebar, navigation
 * items and the split view (spec A §4-13, spec B §13).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include <stdio.h>

void awra_spinner_set_size_internal(AwraWidget *w, int size);
void awra_button_set_look(AwraWidget *w, int look);

/* ======================================================================= */
/* Bin: one child inside paddings (the base of most of these) */

typedef struct BinExt {
  AwraWidget *child;
  short pad[4]; /* top end bottom start, logical */
  /* surface */
  int material;
  int role;
  int elevation;
  float radius; /* < 0: the material's */
  int corner_mask, edge_mask;
  bool paints;
  /* cards */
  bool activatable;
  /* badges / banners: their look */
  int variant;
  bool custom_color;
  AwraRGBA color;
  AwraWidget *a, *b, *c, *d; /* parts */
} BinExt;

#define BINX(w) AWRA_EXT(w, BinExt)

static void bin_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  BinExt *x = BINX(w);
  const int ph = awra_px(x->pad[1]) + awra_px(x->pad[3]);
  const int pv = awra_px(x->pad[0]) + awra_px(x->pad[2]);
  const int pad = o == AWRA_ORIENTATION_HORIZONTAL ? ph : pv;
  const int other = o == AWRA_ORIENTATION_HORIZONTAL ? pv : ph;
  int mn = 0, nt = 0;
  if (x->child) {
    awra_measure(x->child, o, for_size >= 0 ? BK_MAX(0, for_size - other) : -1, &mn, &nt);
  }
  *r_min = mn + pad;
  *r_nat = nt + pad;
}

static void bin_allocate(AwraWidget *w)
{
  BinExt *x = BINX(w);
  if (!x->child) {
    return;
  }
  const int t = awra_px(x->pad[0]), e = awra_px(x->pad[1]), b = awra_px(x->pad[2]),
            s = awra_px(x->pad[3]);
  awra_allocate(x->child, w->x + s, w->y + t, BK_MAX(0, w->w - s - e), BK_MAX(0, w->h - t - b));
}

static void surface_draw(AwraWidget *w, AwraDraw *d)
{
  BinExt *x = BINX(w);
  if (!x->paints) {
    return;
  }
  AwraResolvedMaterial m;
  awra_material_resolve((AwraMaterial)x->material, x->role, (AwraElevation)x->elevation, false, &m);
  const float r = x->radius >= 0 ? awra_pxf(x->radius) : awra_pxf(m.radius);
  awra_paint_material(d, awra_box_of(w), &m, r, x->corner_mask, x->edge_mask,
                      (AwraElevation)x->elevation);
}

static void bin_init(AwraWidget *w)
{
  BinExt *x = BINX(w);
  x->radius = -1;
  x->corner_mask = 15;
  x->edge_mask = AWRA_EDGE_ALL;
  x->material = AWRA_MATERIAL_CONTENT;
}

static AwraClass surface_class = {
    .bk = AWRA_BK("AwraSurface", 0),
    .type = "AwraSurface",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(BinExt),
    .init = bin_init,
    .measure = bin_measure,
    .allocate = bin_allocate,
    .draw = surface_draw,
};

static void bin_set_child(AwraWidget *w, AwraWidget *child)
{
  awra_set_slot(w, &BINX(w)->child, child);
}

static void bin_pad(AwraWidget *w, int t, int e, int b, int s)
{
  BinExt *x = BINX(w);
  x->pad[0] = (short)t;
  x->pad[1] = (short)e;
  x->pad[2] = (short)b;
  x->pad[3] = (short)s;
  awra_widget_queue_resize(w);
}

AwraWidget *awra_surface_new(AwraMaterial material)
{
  AwraWidget *w = awra_widget_new(&surface_class);
  BINX(w)->material = material;
  BINX(w)->paints = true;
  return w;
}

void awra_surface_set_child(AwraWidget *w, AwraWidget *child)
{
  bin_set_child(w, child);
}

void awra_surface_set_material(AwraWidget *w, AwraMaterial m)
{
  BINX(w)->material = m;
  awra_invalidate(w);
}

void awra_surface_set_elevation(AwraWidget *w, AwraElevation e)
{
  BINX(w)->elevation = e;
  awra_invalidate(w);
}

void awra_surface_set_radius(AwraWidget *w, double r)
{
  BINX(w)->radius = (float)r;
  awra_invalidate(w);
}

void awra_surface_set_corner_mask(AwraWidget *w, int mask)
{
  BINX(w)->corner_mask = mask;
  awra_invalidate(w);
}

void awra_surface_set_padding(AwraWidget *w, int t, int e, int b, int s)
{
  bin_pad(w, t, e, b, s);
}

void awra_surface_set_role_internal(AwraWidget *w, int role, int edge_mask);
void awra_surface_set_role_internal(AwraWidget *w, int role, int edge_mask)
{
  BINX(w)->role = role;
  BINX(w)->edge_mask = edge_mask;
  awra_invalidate(w);
}

/* ======================================================================= */
/* Card (spec A §9): Tinted = Layer (radius 17), Plain = nothing, 16 px inset */

static int card_release(AwraWidget *w, BKEvent *e)
{
  if (BINX(w)->activatable && e->inside) {
    awra_emit_simple(w, AWRA_SIGNAL_CLICKED);
  }
  return 1;
}

static void card_draw(AwraWidget *w, AwraDraw *d)
{
  surface_draw(w, d);
  BinExt *x = BINX(w);
  if (x->activatable && awra_hover_t(w) > 0) {
    AwraRGBA h = awra_tokens()->control_cap_hover;
    h.a *= awra_hover_t(w) * 0.6f;
    awra_paint_rbox(d, awra_box_of(w), awra_pxf(17), &h, 0, NULL);
  }
}

static AwraClass card_class = {
    .bk = AWRA_BK("AwraCard", 0),
    .type = "AwraCard",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_HOVER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(BinExt),
    .init = bin_init,
    .measure = bin_measure,
    .allocate = bin_allocate,
    .draw = card_draw,
    .release = card_release,
};

AwraWidget *awra_card_new(void)
{
  AwraWidget *w = awra_widget_new(&card_class);
  BinExt *x = BINX(w);
  x->paints = true;
  x->material = AWRA_MATERIAL_LAYER;
  x->role = AWRA_ROLE_GENERIC;
  bin_pad(w, 16, 16, 16, 16);
  return w;
}

void awra_card_set_child(AwraWidget *w, AwraWidget *child)
{
  bin_set_child(w, child);
}

void awra_card_set_appearance(AwraWidget *w, AwraCardAppearance a)
{
  BinExt *x = BINX(w);
  x->material = a == AWRA_CARD_PLAIN ? AWRA_MATERIAL_CONTENT : AWRA_MATERIAL_LAYER;
  x->elevation = a == AWRA_CARD_RAISED ? AWRA_ELEVATION_RAISED : AWRA_ELEVATION_FLAT;
  x->radius = a == AWRA_CARD_PLAIN ? 0 : -1;
  awra_invalidate(w);
}

void awra_card_set_activatable(AwraWidget *w, bool a)
{
  BINX(w)->activatable = a;
  if (a) {
    w->flags |= NF_EVENTS;
    awra_widget_set_cursor(w, "pointer");
  }
}

void awra_card_set_padding(AwraWidget *w, AwraSpacing s)
{
  const int p = s == AWRA_SPACING_NONE ? 0 : awra_spacing(s);
  bin_pad(w, p, p, p, p);
}

/* ======================================================================= */
/* Page (spec A §7): a scrolled column, at most 960 wide, 32 px inset */

typedef struct PageCol {
  AwraWidget *box;
} PageCol;

static void pagecol_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  AwraWidget *c = BINX(w)->child;
  const intptr_t mw = (intptr_t)awra_widget_get_data(w, "awra-maxw");
  const int maxw = awra_px(mw > 0 ? (int)mw : 960);
  if (!c) {
    *mn = *nt = 0;
    return;
  }
  awra_measure(c, o, o == AWRA_ORIENTATION_VERTICAL && f >= 0 ? BK_MIN(f, maxw) : f, mn, nt);
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    *nt = BK_MIN(*nt, maxw);
    *mn = BK_MIN(*mn, *nt);
  }
}

static void pagecol_allocate(AwraWidget *w)
{
  AwraWidget *c = BINX(w)->child;
  if (!c) {
    return;
  }
  const intptr_t mw = (intptr_t)awra_widget_get_data(w, "awra-maxw");
  const int cw = BK_MIN(w->w, awra_px(mw > 0 ? (int)mw : 960));
  awra_allocate(c, w->x + (w->w - cw) / 2, w->y, cw, w->h);
}

static AwraClass pagecol_class = {
    .bk = AWRA_BK("AwraPageViewport", 0),
    .type = "AwraPageViewport",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(BinExt),
    .init = bin_init,
    .measure = pagecol_measure,
    .allocate = pagecol_allocate,
};

static AwraClass page_class = {
    .bk = AWRA_BK("AwraPage", 0),
    .type = "AwraPage",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(BinExt),
    .init = bin_init,
    .measure = bin_measure,
    .allocate = bin_allocate,
};

AwraWidget *awra_page_new(void)
{
  AwraWidget *w = awra_widget_new(&page_class);
  BinExt *x = BINX(w);
  AwraWidget *scroll = awra_scrolled_new();
  awra_scrolled_set_policy(scroll, AWRA_POLICY_NEVER, AWRA_POLICY_AUTOMATIC);
  AwraWidget *col = awra_widget_new(&pagecol_class);
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  awra_widget_set_margins(box, 32, 32, 32, 32);
  bin_set_child(col, box);
  awra_scrolled_set_child(scroll, col);
  bin_set_child(w, scroll);
  x->a = scroll;
  x->b = box;
  return w;
}

void awra_page_set_max_content_width(AwraWidget *w, int width)
{
  AwraWidget *scroll = BINX(w)->a;
  AwraWidget *col = scroll ? awra_scrolled_child(scroll) : NULL;
  if (col) {
    awra_widget_set_data(col, "awra-maxw", (void *)(intptr_t)width, NULL);
    awra_widget_queue_resize(col);
  }
}

void awra_page_set_child(AwraWidget *w, AwraWidget *child)
{
  BinExt *x = BINX(w);
  if (x->c) {
    awra_box_remove(x->b, x->c);
  }
  x->c = child;
  if (child) {
    awra_box_append(x->b, child);
  }
}

void awra_page_set_header(AwraWidget *w, AwraWidget *header)
{
  BinExt *x = BINX(w);
  if (x->d) {
    awra_box_remove(x->b, x->d);
  }
  x->d = header;
  if (header) {
    awra_box_prepend(x->b, header);
  }
}

void awra_page_set_scrollable(AwraWidget *w, bool scrollable)
{
  awra_scrolled_set_policy(BINX(w)->a, AWRA_POLICY_NEVER,
                           scrollable ? AWRA_POLICY_AUTOMATIC : AWRA_POLICY_NEVER);
}

/* ======================================================================= */
/* Page header and section (spec A §7-8): heading row, titles, actions */

typedef struct Heading {
  AwraWidget *root, *heading, *titles, *title, *subtitle, *actions;
} Heading;

static void heading_free(void *p)
{
  free(p);
}

static Heading *heading_of(AwraWidget *w)
{
  return (Heading *)awra_widget_get_data(w, "awra-heading");
}

static AwraWidget *heading_new(const char *title, const char *subtitle, AwraTypography title_t)
{
  Heading *h = (Heading *)calloc(1, sizeof(Heading));
  AwraWidget *root = awra_box_new(AWRA_ORIENTATION_VERTICAL, 12);
  awra_widget_set_data(root, "awra-heading", h, heading_free);
  h->root = root;
  h->heading = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 16);
  h->titles = awra_box_new(AWRA_ORIENTATION_VERTICAL, 4);
  awra_widget_set_hexpand(h->titles, true);
  awra_widget_set_valign(h->titles, title_t == AWRA_TYPOGRAPHY_TITLE_1 ? AWRA_ALIGN_CENTER :
                                                                          AWRA_ALIGN_FILL);
  h->title = awra_label_new(title ? title : "");
  awra_widget_set_typography(h->title, title_t);
  awra_label_set_xalign(h->title, 0.0f);
  h->subtitle = awra_label_new(subtitle ? subtitle : "");
  awra_widget_set_typography(h->subtitle, AWRA_TYPOGRAPHY_MUTED);
  awra_label_set_xalign(h->subtitle, 0.0f);
  awra_label_set_wrap(h->subtitle, AWRA_WRAP_WORD);
  awra_box_append(h->titles, h->title);
  awra_box_append(h->titles, h->subtitle);
  awra_box_append(h->heading, h->titles);
  awra_box_append(root, h->heading);
  awra_widget_set_visible(h->title, title && title[0]);
  awra_widget_set_visible(h->subtitle, subtitle && subtitle[0]);
  awra_widget_set_visible(h->heading, (title && title[0]) || (subtitle && subtitle[0]));
  awra_widget_isolate_expand(root);
  return root;
}

static void heading_sync(Heading *h)
{
  const char *t = awra_label_text(h->title), *s = awra_label_text(h->subtitle);
  awra_widget_set_visible(h->title, t && t[0]);
  awra_widget_set_visible(h->subtitle, s && s[0]);
  awra_widget_set_visible(h->heading, (t && t[0]) || (s && s[0]) || h->actions);
}

static void heading_add_action(Heading *h, AwraWidget *action)
{
  if (!h->actions) {
    h->actions = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 8);
    awra_widget_set_valign(h->actions, AWRA_ALIGN_START);
    awra_box_append(h->heading, h->actions);
    awra_widget_set_valign(h->titles, AWRA_ALIGN_START);
    awra_widget_set_size_request(h->title, -1, 34);
  }
  awra_box_append(h->actions, action);
  heading_sync(h);
}

AwraWidget *awra_page_header_new(const char *title, const char *subtitle)
{
  return heading_new(title, subtitle, AWRA_TYPOGRAPHY_TITLE_1);
}

void awra_page_header_set_title(AwraWidget *w, const char *t)
{
  Heading *h = heading_of(w);
  awra_label_set_text(h->title, t);
  heading_sync(h);
}

void awra_page_header_set_subtitle(AwraWidget *w, const char *s)
{
  Heading *h = heading_of(w);
  awra_label_set_text(h->subtitle, s);
  heading_sync(h);
}

void awra_page_header_add_action(AwraWidget *w, AwraWidget *a)
{
  heading_add_action(heading_of(w), a);
}

AwraWidget *awra_section_new(const char *title)
{
  return heading_new(title, NULL, AWRA_TYPOGRAPHY_TITLE_3);
}

void awra_section_set_title(AwraWidget *w, const char *t)
{
  awra_page_header_set_title(w, t);
}

void awra_section_set_description(AwraWidget *w, const char *d)
{
  awra_page_header_set_subtitle(w, d);
}

void awra_section_append(AwraWidget *w, AwraWidget *child)
{
  awra_box_append(w, child);
}

void awra_section_set_header_suffix(AwraWidget *w, AwraWidget *suffix)
{
  heading_add_action(heading_of(w), suffix);
}

/* ======================================================================= */
/* Form and form rows (spec A §10) */

AwraWidget *awra_form_new(void)
{
  return awra_box_new(AWRA_ORIENTATION_VERTICAL, 16);
}

void awra_form_append(AwraWidget *form, AwraWidget *row)
{
  awra_box_append(form, row);
}

typedef struct FormRowExt {
  AwraWidget *label, *field, *control, *help;
  bool vertical, compact;
} FormRowExt;

#define FRX(w) AWRA_EXT(w, FormRowExt)

static bool row_compact(AwraWidget *w, int width)
{
  FormRowExt *x = FRX(w);
  if (x->vertical) {
    return true;
  }
  if (!x->label || !awra_child_visible(x->label)) {
    return false;
  }
  int lm, ln, fm, fn;
  awra_measure(x->label, AWRA_ORIENTATION_HORIZONTAL, -1, &lm, &ln);
  awra_measure(x->field, AWRA_ORIENTATION_HORIZONTAL, -1, &fm, &fn);
  return width >= 0 && (width < awra_px(560) ||
                        width < BK_MAX(lm, awra_px(224)) + awra_px(16) + fm);
}

/* The label column beside the control for a row width: the row is a
 * GtkBox (label at least 224 wide, the field expanding), so the label gets
 * its natural width when there is room for it. */
static int formrow_label_width(AwraWidget *w, int width)
{
  FormRowExt *x = FRX(w);
  int lm, ln, fm, fn;
  awra_measure(x->label, AWRA_ORIENTATION_HORIZONTAL, -1, &lm, &ln);
  awra_measure(x->field, AWRA_ORIENTATION_HORIZONTAL, -1, &fm, &fn);
  int sizes[2] = {BK_MAX(lm, awra_px(224)), fm};
  const int nats[2] = {BK_MAX(ln, awra_px(224)), BK_MAX(fn, fm)};
  const int extra = width - awra_px(16) - sizes[0] - sizes[1];
  if (extra > 0) {
    awra_distribute(extra, 2, sizes, nats);
  }
  return sizes[0];
}

static void formrow_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  FormRowExt *x = FRX(w);
  const bool has_label = x->label && awra_child_visible(x->label);
  int lm = 0, ln = 0, fm = 0, fn = 0;
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    if (has_label) {
      awra_measure(x->label, o, -1, &lm, &ln);
    }
    awra_measure(x->field, o, -1, &fm, &fn);
    *r_min = BK_MAX(lm, fm);
    *r_nat = has_label && !x->vertical ? BK_MAX(ln, awra_px(224)) + awra_px(16) + fn : BK_MAX(ln, fn);
    return;
  }
  const bool compact = row_compact(w, for_size);
  if (compact || !has_label) {
    if (has_label) {
      awra_measure(x->label, o, for_size, &lm, &ln);
    }
    awra_measure(x->field, o, for_size, &fm, &fn);
    const int gap = has_label ? awra_px(8) : 0;
    *r_min = lm + gap + fm;
    *r_nat = ln + gap + fn;
  }
  else {
    const int lw = formrow_label_width(w, for_size);
    awra_measure(x->label, o, lw, &lm, &ln);
    awra_measure(x->field, o, for_size >= 0 ? for_size - lw - awra_px(16) : -1, &fm, &fn);
    *r_min = BK_MAX(lm, fm);
    *r_nat = BK_MAX(ln, fn);
  }
  *r_min = BK_MAX(*r_min, compact ? 0 : awra_px(34));
  *r_nat = BK_MAX(*r_nat, *r_min);
}

static void formrow_allocate(AwraWidget *w)
{
  FormRowExt *x = FRX(w);
  const bool has_label = x->label && awra_child_visible(x->label);
  if (!has_label) {
    awra_allocate(x->field, w->x, w->y, w->w, w->h);
    return;
  }
  if (row_compact(w, w->w)) {
    int lm, ln;
    awra_measure(x->label, AWRA_ORIENTATION_VERTICAL, w->w, &lm, &ln);
    awra_allocate(x->label, w->x, w->y, w->w, ln);
    const int y = ln + awra_px(8);
    awra_allocate(x->field, w->x, w->y + y, w->w, BK_MAX(0, w->h - y));
    return;
  }
  const int lw = formrow_label_width(w, w->w);
  int lh, fh, tmp;
  awra_measure(x->label, AWRA_ORIENTATION_VERTICAL, lw, &tmp, &lh);
  const int fx = lw + awra_px(16);
  awra_measure(x->field, AWRA_ORIENTATION_VERTICAL, w->w - fx, &tmp, &fh);
  /* the label sits on the control's first line */
  int ch = 0;
  if (x->control) {
    awra_measure(x->control, AWRA_ORIENTATION_VERTICAL, w->w - fx, &tmp, &ch);
  }
  int off = ch > 3 * lh ? awra_px(10) : (ch - lh) / 2;
  off = BK_CLAMP(off, 0, BK_MAX(0, w->h - lh));
  awra_allocate(x->label, w->x, w->y + off, lw, lh);
  awra_allocate(x->field, w->x + fx, w->y, BK_MAX(0, w->w - fx), w->h);
}

static AwraClass formrow_class = {
    .bk = AWRA_BK("AwraFormRow", 0),
    .type = "AwraFormRow",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(FormRowExt),
    .measure = formrow_measure,
    .allocate = formrow_allocate,
};

AwraWidget *awra_form_row_new(const char *title, const char *subtitle)
{
  AwraWidget *w = awra_widget_new(&formrow_class);
  FormRowExt *x = FRX(w);
  x->label = awra_label_new(title ? title : "");
  awra_label_set_xalign(x->label, 0.0f);
  awra_label_set_wrap(x->label, AWRA_WRAP_WORD);
  awra_widget_set_typography(x->label, AWRA_TYPOGRAPHY_BODY);
  awra_widget_set_visible(x->label, title && title[0]);
  x->field = awra_box_new(AWRA_ORIENTATION_VERTICAL, 4);
  awra_widget_set_hexpand(x->field, true);
  x->help = awra_label_new(subtitle ? subtitle : "");
  awra_widget_set_typography(x->help, AWRA_TYPOGRAPHY_MUTED);
  awra_label_set_wrap(x->help, AWRA_WRAP_WORD);
  awra_label_set_xalign(x->help, 0.0f);
  awra_widget_set_visible(x->help, subtitle && subtitle[0]);
  awra_box_append(x->field, x->help);
  awra_child_insert(w, x->label, -1);
  awra_child_insert(w, x->field, -1);
  return w;
}

void awra_form_row_set_control(AwraWidget *w, AwraWidget *control)
{
  FormRowExt *x = FRX(w);
  if (x->control) {
    awra_box_remove(x->field, x->control);
  }
  x->control = control;
  if (control) {
    /* text fields and pickers fill the field column; compact controls
     * (spin buttons, switches, segmented controls, captures...) keep their
     * natural width; an explicit hexpand wins (awra-form-row.c) */
    if (!awra_aux(control)->hexpand_set) {
      const bool fills = (awra_widget_is(control, "AwraEntry") &&
                          !awra_widget_is(control, "AwraSpinButton")) ||
                         awra_widget_is(control, "AwraDropdown") ||
                         awra_widget_is(control, "AwraScrolledWindow");
      if (fills) {
        awra_widget_set_hexpand(control, true);
      }
      else {
        awra_widget_set_halign(control, AWRA_ALIGN_START);
      }
    }
    awra_box_prepend(x->field, control);
  }
}

void awra_form_row_set_title(AwraWidget *w, const char *t)
{
  awra_label_set_text(FRX(w)->label, t);
  awra_widget_set_visible(FRX(w)->label, t && t[0]);
}

void awra_form_row_set_subtitle(AwraWidget *w, const char *s)
{
  awra_label_set_text(FRX(w)->help, s);
  awra_widget_set_visible(FRX(w)->help, s && s[0]);
}

void awra_form_row_set_vertical(AwraWidget *w, bool v)
{
  FRX(w)->vertical = v;
  awra_widget_queue_resize(w);
}

/* ======================================================================= */
/* Action row (spec A §11): 58 px, radius 11, hover cap-hover */

typedef struct ActionRowExt {
  AwraWidget *box, *icon, *title, *subtitle, *suffix, *prefix;
  bool activatable;
} ActionRowExt;

#define ARX(w) AWRA_EXT(w, ActionRowExt)

static void arow_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  const int ph = awra_px(20), pv = awra_px(16);
  awra_measure(ARX(w)->box, o, f >= 0 ? f - (o == AWRA_ORIENTATION_HORIZONTAL ? pv : ph) : -1, mn, nt);
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    *mn += ph;
    *nt += ph;
  }
  else {
    *mn = BK_MAX(*mn, awra_px(42)) + pv;
    *nt = BK_MAX(*nt, awra_px(42)) + pv;
  }
}

static void arow_allocate(AwraWidget *w)
{
  awra_allocate(ARX(w)->box, w->x + awra_px(10), w->y + awra_px(8), w->w - awra_px(20),
                w->h - awra_px(16));
}

static void arow_draw(AwraWidget *w, AwraDraw *d)
{
  if (!ARX(w)->activatable) {
    return;
  }
  AwraButtonColors c;
  awra_button_colors(AWRA_LOOK_ACTION_ROW, awra_hover_t(w) > 0.5f, awra_pressed(w), false,
                     awra_sensitive(w), &c);
  c.bg.a *= awra_pressed(w) ? 1.0f : awra_hover_t(w);
  awra_paint_button_bg(d, awra_box_of(w), awra_pxf(11), &c, 0.0f, awra_focus_visible(w));
}

static int arow_release(AwraWidget *w, BKEvent *e)
{
  if (ARX(w)->activatable && e->inside) {
    awra_emit_simple(w, AWRA_SIGNAL_CLICKED);
  }
  return 1;
}

static void arow_activate(AwraWidget *w)
{
  if (ARX(w)->activatable) {
    awra_emit_simple(w, AWRA_SIGNAL_CLICKED);
  }
}

static AwraClass arow_class = {
    .bk = AWRA_BK("AwraActionRow", 0),
    .type = "AwraActionRow",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_HOVER | AWRA_CLASS_ACTIVATABLE,
    .ext_size = sizeof(ActionRowExt),
    .measure = arow_measure,
    .allocate = arow_allocate,
    .draw = arow_draw,
    .release = arow_release,
    .activate = arow_activate,
    .overdraw = 4,
};

AwraWidget *awra_action_row_new(const char *title, const char *subtitle)
{
  AwraWidget *w = awra_widget_new(&arow_class);
  ActionRowExt *x = ARX(w);
  x->box = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 12);
  x->icon = awra_image_new();
  awra_image_set_pixel_size(x->icon, 18);
  awra_image_set_color(x->icon, &awra_tokens()->muted);
  awra_widget_set_visible(x->icon, false);
  AwraWidget *texts = awra_box_new(AWRA_ORIENTATION_VERTICAL, 2);
  awra_widget_set_hexpand(texts, true);
  awra_widget_set_valign(texts, AWRA_ALIGN_CENTER);
  x->title = awra_label_new(title ? title : "");
  awra_label_set_xalign(x->title, 0.0f);
  awra_label_set_ellipsize(x->title, AWRA_ELLIPSIZE_END);
  awra_label_set_weight(x->title, 590);
  x->subtitle = awra_label_new(subtitle ? subtitle : "");
  awra_label_set_xalign(x->subtitle, 0.0f);
  awra_label_set_ellipsize(x->subtitle, AWRA_ELLIPSIZE_END);
  awra_widget_set_typography(x->subtitle, AWRA_TYPOGRAPHY_MUTED);
  awra_label_set_scale(x->subtitle, 0.86 / 0.88);
  awra_widget_set_visible(x->subtitle, subtitle && subtitle[0]);
  awra_box_append(texts, x->title);
  awra_box_append(texts, x->subtitle);
  awra_box_append(x->box, x->icon);
  awra_box_append(x->box, texts);
  awra_child_insert(w, x->box, -1);
  return w;
}

void awra_action_row_set_title(AwraWidget *w, const char *t)
{
  awra_label_set_text(ARX(w)->title, t);
}

void awra_action_row_set_subtitle(AwraWidget *w, const char *s)
{
  awra_label_set_text(ARX(w)->subtitle, s);
  awra_widget_set_visible(ARX(w)->subtitle, s && s[0]);
}

void awra_action_row_set_icon_name(AwraWidget *w, const char *icon)
{
  awra_image_set_icon_name(ARX(w)->icon, icon);
  awra_widget_set_visible(ARX(w)->icon, icon && icon[0]);
}

void awra_action_row_set_prefix(AwraWidget *w, AwraWidget *prefix)
{
  ActionRowExt *x = ARX(w);
  if (x->prefix) {
    awra_box_remove(x->box, x->prefix);
  }
  x->prefix = prefix;
  if (prefix) {
    awra_widget_set_valign(prefix, AWRA_ALIGN_CENTER);
    awra_box_prepend(x->box, prefix);
  }
}

void awra_action_row_add_suffix(AwraWidget *w, AwraWidget *suffix)
{
  awra_widget_set_valign(suffix, AWRA_ALIGN_CENTER);
  awra_box_append(ARX(w)->box, suffix);
}

void awra_action_row_set_activatable(AwraWidget *w, bool a)
{
  ARX(w)->activatable = a;
  if (a) {
    w->flags |= NF_EVENTS;
  }
  else {
    w->flags &= ~NF_EVENTS;
  }
  awra_invalidate(w);
}

/* ======================================================================= */
/* Badge (spec B §13): a pill, .80em weight 800, 10 / 4 px insets */

static void badge_colors(AwraWidget *w, AwraRGBA *bg, AwraRGBA *fg)
{
  BinExt *x = BINX(w);
  const AwraTokens *t = awra_tokens();
  const bool light = !t->dark;
  switch (x->variant) {
    case AWRA_BADGE_NEUTRAL:
      *bg = awra_with_alpha(t->foreground, 0.08f);
      *fg = t->muted;
      break;
    case AWRA_BADGE_INFO:
      awra_parse_color("#55a9ff", bg);
      bg->a = 0.18f;
      awra_parse_color(light ? "#1f6097" : "#a9d4ff", fg);
      break;
    case AWRA_BADGE_SUCCESS:
      awra_parse_color("#52c982", bg);
      bg->a = 0.18f;
      awra_parse_color(light ? "#256b40" : "#9de7b9", fg);
      break;
    case AWRA_BADGE_WARNING:
      awra_parse_color("#e8b44f", bg);
      bg->a = 0.19f;
      awra_parse_color(light ? "#795511" : "#f3d591", fg);
      break;
    case AWRA_BADGE_DANGER:
      awra_parse_color("#ee6573", bg);
      bg->a = 0.19f;
      awra_parse_color(light ? "#972d3a" : "#ffadb5", fg);
      break;
    default:
      *bg = t->accent_soft;
      *fg = t->accent_content;
  }
  if (x->custom_color) {
    *bg = x->color;
    const float l = 0.2126f * x->color.r + 0.7152f * x->color.g + 0.0722f * x->color.b;
    awra_parse_color(l > 0.6f ? "#1c1c1c" : "#ffffff", fg);
  }
}

static void badge_draw(AwraWidget *w, AwraDraw *d)
{
  AwraRGBA bg, fg;
  badge_colors(w, &bg, &fg);
  awra_paint_rbox(d, awra_box_of(w), w->h * 0.5f, &bg, 0, NULL);
}

static bool badge_child_style(AwraWidget *w, AwraRGBA *c, int *weight)
{
  AwraRGBA bg;
  badge_colors(w, &bg, c);
  *weight = 800;
  return true;
}

static AwraClass badge_class = {
    .bk = AWRA_BK("AwraBadge", 0),
    .type = "AwraBadge",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(BinExt),
    .init = bin_init,
    .measure = bin_measure,
    .allocate = bin_allocate,
    .draw = badge_draw,
    .child_style = badge_child_style,
};

AwraWidget *awra_badge_new(const char *label, AwraBadgeAppearance a)
{
  AwraWidget *w = awra_widget_new(&badge_class);
  BinExt *x = BINX(w);
  x->variant = a;
  AwraWidget *l = awra_label_new(label);
  awra_label_set_scale(l, 0.80);
  awra_label_set_weight(l, 800);
  bin_set_child(w, l);
  x->a = l;
  bin_pad(w, 4, 10, 4, 10);
  awra_widget_set_halign(w, AWRA_ALIGN_START);
  awra_widget_set_size_request(w, 18, 18);
  return w;
}

void awra_badge_set_label(AwraWidget *w, const char *label)
{
  awra_label_set_text(BINX(w)->a, label);
}

void awra_badge_set_appearance(AwraWidget *w, AwraBadgeAppearance a)
{
  BINX(w)->variant = a;
  awra_invalidate(w);
}

void awra_badge_set_icon_name(AwraWidget *w, const char *icon)
{
  (void)w;
  (void)icon;
}

void awra_badge_set_color(AwraWidget *w, const AwraRGBA *c);
void awra_badge_set_color(AwraWidget *w, const AwraRGBA *c)
{
  BinExt *x = BINX(w);
  x->custom_color = c != NULL;
  if (c) {
    x->color = *c;
  }
  awra_invalidate(w);
}

/* ======================================================================= */
/* Status banner (spec A §13): a Layer card, icon, title, description */

static const char *status_icons[] = {"awra-status-info-symbolic", "awra-status-success-symbolic",
                                     "awra-status-warning-symbolic", "awra-status-error-symbolic"};

static AwraRGBA status_color(int a)
{
  static const char *dark[] = {"#9bc9ff", "#80dca5", "#f0c66b", "#ff929c"};
  static const char *light[] = {"#2869a8", "#217846", "#8a6215", "#a02f3a"};
  AwraRGBA c;
  awra_parse_color(awra_tokens()->dark ? dark[a & 3] : light[a & 3], &c);
  return c;
}

static void banner_style(AwraWidget *w)
{
  BinExt *x = BINX(w);
  const AwraRGBA c = status_color(x->variant);
  awra_image_set_color(x->a, &c);
}

static AwraClass banner_class = {
    .bk = AWRA_BK("AwraStatusBanner", 0),
    .type = "AwraStatusBanner",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(BinExt),
    .init = bin_init,
    .measure = bin_measure,
    .allocate = bin_allocate,
    .draw = surface_draw,
    .style_changed = banner_style,
};

static void banner_dismiss(AwraWidget *b, const AwraArgs *a, void *data)
{
  awra_widget_set_visible((AwraWidget *)data, false);
  awra_emit_simple((AwraWidget *)data, AWRA_SIGNAL_HIDE);
}

AwraWidget *awra_status_banner_new(const char *title, const char *description,
                                   AwraStatusAppearance appearance)
{
  AwraWidget *w = awra_widget_new(&banner_class);
  BinExt *x = BINX(w);
  x->paints = true;
  x->material = AWRA_MATERIAL_LAYER;
  x->variant = appearance;
  AwraWidget *root = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 12);
  x->a = awra_image_new_from_icon_name(status_icons[appearance & 3]);
  awra_image_set_pixel_size(x->a, 20);
  awra_widget_set_valign(x->a, AWRA_ALIGN_START);
  AwraWidget *copy = awra_box_new(AWRA_ORIENTATION_VERTICAL, 4);
  awra_widget_set_hexpand(copy, true);
  x->b = awra_label_new(title ? title : "");
  awra_widget_set_typography(x->b, AWRA_TYPOGRAPHY_TITLE_3);
  awra_label_set_wrap(x->b, AWRA_WRAP_WORD);
  awra_label_set_xalign(x->b, 0.0f);
  awra_widget_set_visible(x->b, title && title[0]);
  x->c = awra_label_new(description ? description : "");
  awra_widget_set_typography(x->c, AWRA_TYPOGRAPHY_MUTED);
  awra_label_set_wrap(x->c, AWRA_WRAP_WORD);
  awra_label_set_xalign(x->c, 0.0f);
  awra_widget_set_visible(x->c, description && description[0]);
  awra_box_append(copy, x->b);
  awra_box_append(copy, x->c);
  awra_box_append(root, x->a);
  awra_box_append(root, copy);
  x->d = awra_button_new_from_icon_name("awra-close-symbolic");
  awra_button_set_appearance(x->d, AWRA_BUTTON_GHOST);
  awra_widget_set_visible(x->d, false);
  awra_widget_set_valign(x->d, AWRA_ALIGN_START);
  awra_signal_connect(x->d, AWRA_SIGNAL_CLICKED, banner_dismiss, w, NULL);
  awra_box_append(root, x->d);
  bin_set_child(w, root);
  bin_pad(w, 12, 12, 12, 12);
  awra_widget_set_size_request(w, -1, 52);
  banner_style(w);
  return w;
}

void awra_status_banner_set_title(AwraWidget *w, const char *t)
{
  awra_label_set_text(BINX(w)->b, t);
  awra_widget_set_visible(BINX(w)->b, t && t[0]);
}

void awra_status_banner_set_description(AwraWidget *w, const char *d)
{
  awra_label_set_text(BINX(w)->c, d);
  awra_widget_set_visible(BINX(w)->c, d && d[0]);
}

void awra_status_banner_set_appearance(AwraWidget *w, AwraStatusAppearance a)
{
  BINX(w)->variant = a;
  awra_image_set_icon_name(BINX(w)->a, status_icons[a & 3]);
  banner_style(w);
}

void awra_status_banner_add_action(AwraWidget *w, AwraWidget *action)
{
  BinExt *x = BINX(w);
  awra_widget_set_valign(action, AWRA_ALIGN_CENTER);
  awra_box_insert_after(BINX(w)->child, action, x->b->parent);
}

void awra_status_banner_set_dismissible(AwraWidget *w, bool d)
{
  awra_widget_set_visible(BINX(w)->d, d);
}

/* ======================================================================= */
/* Empty / loading states (spec A §12) */

typedef struct StateParts {
  AwraWidget *icon, *title, *desc, *col;
} StateParts;

static StateParts *sp(AwraWidget *w)
{
  return (StateParts *)awra_widget_get_data(w, "awra-state");
}

static AwraWidget *state_new(AwraWidget *icon, const char *title, const char *desc)
{
  AwraWidget *w = awra_widget_new(&page_class); /* a bin */
  StateParts *p = (StateParts *)calloc(1, sizeof(StateParts));
  awra_widget_set_data(w, "awra-state", p, free);
  awra_widget_set_size_request(w, -1, 220);
  p->col = awra_box_new(AWRA_ORIENTATION_VERTICAL, 8);
  awra_widget_set_margins(p->col, 24, 24, 24, 24);
  awra_widget_set_halign(p->col, AWRA_ALIGN_CENTER);
  awra_widget_set_valign(p->col, AWRA_ALIGN_CENTER);
  p->icon = icon;
  awra_widget_set_halign(icon, AWRA_ALIGN_CENTER);
  awra_widget_set_size_request(icon, 36, 36);
  awra_widget_set_margin_bottom(icon, 4);
  p->title = awra_label_new(title ? title : "");
  awra_widget_set_typography(p->title, AWRA_TYPOGRAPHY_TITLE_2);
  awra_label_set_justify(p->title, AWRA_JUSTIFY_CENTER);
  awra_label_set_wrap(p->title, AWRA_WRAP_WORD);
  awra_widget_set_halign(p->title, AWRA_ALIGN_CENTER);
  p->desc = awra_label_new(desc ? desc : "");
  awra_widget_set_typography(p->desc, AWRA_TYPOGRAPHY_MUTED);
  awra_label_set_justify(p->desc, AWRA_JUSTIFY_CENTER);
  awra_label_set_wrap(p->desc, AWRA_WRAP_WORD);
  awra_label_set_max_width_chars(p->desc, 48);
  awra_widget_set_halign(p->desc, AWRA_ALIGN_CENTER);
  awra_box_append(p->col, icon);
  awra_box_append(p->col, p->title);
  awra_box_append(p->col, p->desc);
  bin_set_child(w, p->col);
  awra_widget_isolate_expand(w);
  return w;
}

AwraWidget *awra_empty_state_new(const char *icon_name, const char *title, const char *desc)
{
  AwraWidget *icon = awra_image_new_from_icon_name(icon_name ? icon_name : "folder-open-symbolic");
  awra_image_set_pixel_size(icon, 32);
  awra_image_set_color(icon, &awra_tokens()->muted);
  return state_new(icon, title, desc);
}

void awra_empty_state_set_title(AwraWidget *w, const char *t)
{
  awra_label_set_text(sp(w)->title, t);
}

void awra_empty_state_set_description(AwraWidget *w, const char *d)
{
  awra_label_set_text(sp(w)->desc, d);
}

void awra_empty_state_set_icon_name(AwraWidget *w, const char *icon)
{
  awra_image_set_icon_name(sp(w)->icon, icon);
}

void awra_empty_state_add_action(AwraWidget *w, AwraWidget *action)
{
  awra_widget_set_halign(action, AWRA_ALIGN_CENTER);
  awra_box_append(sp(w)->col, action);
}

AwraWidget *awra_loading_state_new(const char *title, const char *desc)
{
  AwraWidget *s = awra_spinner_new();
  awra_spinner_set_size_internal(s, 36);
  awra_spinner_set_spinning(s, true);
  awra_widget_set_data(s, "awra-accent", (void *)1, NULL); /* @awra_accent_content */
  AwraWidget *w = state_new(s, title, desc);
  return w;
}

void awra_loading_state_set_title(AwraWidget *w, const char *t)
{
  awra_label_set_text(sp(w)->title, t);
}

void awra_loading_state_set_description(AwraWidget *w, const char *t)
{
  awra_empty_state_set_description(w, t);
}


/* ======================================================================= */
/* Sidebar (spec A §4): a Chrome surface, inset 12, radius 17, right edge */

AwraWidget *awra_sidebar_new(void)
{
  AwraWidget *w = awra_widget_new(&surface_class);
  BinExt *x = BINX(w);
  x->paints = true;
  x->material = AWRA_MATERIAL_CHROME;
  x->role = AWRA_ROLE_SIDEBAR;
  awra_widget_set_margins(w, 12, 12, 12, 12);
  AwraWidget *col = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  AwraWidget *list = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  awra_widget_set_vexpand(list, true);
  awra_box_append(col, list);
  bin_set_child(w, col);
  x->a = list;
  x->b = col;
  awra_widget_set_name(w, "AwraSidebar");
  return w;
}

void awra_sidebar_set_child(AwraWidget *w, AwraWidget *child)
{
  BinExt *x = BINX(w);
  awra_container_remove_all(x->a);
  if (child) {
    awra_box_append(x->a, child);
  }
}

void awra_sidebar_append(AwraWidget *w, AwraWidget *child)
{
  awra_box_append(BINX(w)->a, child);
}

void awra_sidebar_append_heading(AwraWidget *w, const char *title)
{
  AwraWidget *l = awra_label_new(title);
  awra_widget_set_typography(l, AWRA_TYPOGRAPHY_EYEBROW);
  awra_label_set_xalign(l, 0.0f);
  awra_label_set_scale(l, 0.76 / 0.78);
  awra_label_set_weight(l, 650);
  awra_widget_set_margins(l, 20, 16, 6, 16);
  awra_box_append(BINX(w)->a, l);
}

void awra_sidebar_set_footer(AwraWidget *w, AwraWidget *footer)
{
  BinExt *x = BINX(w);
  if (x->c) {
    awra_box_remove(x->b, x->c);
  }
  x->c = footer;
  if (footer) {
    awra_box_append(x->b, footer);
  }
}

void awra_sidebar_set_inset(AwraWidget *w, AwraSpacing inset)
{
  const int m = inset == AWRA_SPACING_NONE ? 0 : awra_spacing(inset);
  awra_widget_set_margins(w, m, m, m, m);
  BINX(w)->corner_mask = m > 0 ? 15 : 0;
  BINX(w)->edge_mask = m > 0 ? AWRA_EDGE_ALL : AWRA_EDGE_END;
  awra_invalidate(w);
}

/* ======================================================================= */
/* Navigation item (spec A §5): 42 px rows, radius 9, 22 px icon gap */

typedef struct NavExt {
  AwraWidget *box, *icon, *label, *badge;
  bool selected;
  AwraWidget *group_next;
  bool in_sidebar;
} NavExt;

#define NX(w) AWRA_EXT(w, NavExt)

static void nav_margins(AwraWidget *w, int *mx)
{
  *mx = 0;
  for (BKNode *p = w->parent; p; p = p->parent) {
    if (awra_is_widget(p) && awra_widget_name(p) && !strcmp(awra_widget_name(p), "AwraSidebar")) {
      *mx = awra_px(8);
      return;
    }
  }
}

/* Dense navigation (Awra 1's compact-navigation role, class "compact"):
 * 6 x 8 padding, 20 px minimum, regular weight (600 when selected). */
static bool nav_compact(const AwraWidget *w)
{
  return awra_has_class_bit(w, AWRA_CB_COMPACT);
}

static void nav_pads(AwraWidget *w, int *ph, int *pv, int *min_h)
{
  if (nav_compact(w)) {
    *ph = awra_px(8);
    *pv = awra_px(6);
    *min_h = awra_px(24);
  }
  else {
    *ph = awra_px(11);
    *pv = awra_px(7);
    *min_h = awra_px(28);
  }
}

static void nav_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  int mx, ph, pv, min_h;
  nav_margins(w, &mx);
  nav_pads(w, &ph, &pv, &min_h);
  awra_measure(NX(w)->box, o, -1, mn, nt);
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    *mn += 2 * ph + 2 * mx;
    *nt += 2 * ph + 2 * mx;
  }
  else {
    *mn = BK_MAX(*mn, min_h) + 2 * pv + awra_px(2);
    *nt = BK_MAX(*nt, min_h) + 2 * pv + awra_px(2);
  }
}

static AwraBox nav_box(AwraWidget *w)
{
  int mx;
  nav_margins(w, &mx);
  return (AwraBox){(float)(w->x + mx), (float)(w->y + awra_px(1)), (float)(w->w - 2 * mx),
                   (float)(w->h - awra_px(2))};
}

static void nav_allocate(AwraWidget *w)
{
  AwraBox b = nav_box(w);
  int ph, pv, min_h;
  nav_pads(w, &ph, &pv, &min_h);
  awra_allocate(NX(w)->box, (int)b.x + ph, (int)b.y + pv, (int)b.w - 2 * ph, (int)b.h - 2 * pv);
}

static void nav_colors(AwraWidget *w, AwraButtonColors *c)
{
  const float ht = awra_hover_t(w);
  AwraButtonColors rest, hov;
  awra_button_colors(AWRA_LOOK_NAV_ITEM, false, awra_pressed(w), NX(w)->selected, awra_sensitive(w), &rest);
  awra_button_colors(AWRA_LOOK_NAV_ITEM, true, awra_pressed(w), NX(w)->selected, awra_sensitive(w), &hov);
  c->fg = awra_mix(rest.fg, hov.fg, ht);
  c->bg = awra_mix(rest.bg, hov.bg, ht);
  c->rim = NX(w)->selected ? rest.rim : awra_rgba(0, 0, 0, 0);
  c->ring = awra_rgba(0, 0, 0, 0);
  c->pressed_shadow = 0;
}

static void nav_draw(AwraWidget *w, AwraDraw *d)
{
  AwraButtonColors c;
  nav_colors(w, &c);
  awra_paint_button_bg(d, nav_box(w), awra_pxf(9), &c,
                       awra_style_reduced_motion() ? 0.0f : awra_press_t(w), false);
  if (awra_focus_visible(w)) {
    const float ow = awra_pxf(2), off = awra_pxf(1);
    AwraBox b = nav_box(w);
    AwraBox o = {b.x - off - ow, b.y - off - ow, b.w + 2 * (off + ow), b.h + 2 * (off + ow)};
    awra_paint_rbox(d, o, awra_pxf(9) + off + ow, NULL, ow, &awra_tokens()->focus_ring);
  }
}

static bool nav_child_style(AwraWidget *w, AwraRGBA *col, int *weight)
{
  AwraButtonColors c;
  nav_colors(w, &c);
  *col = c.fg;
  *weight = nav_compact(w) ? (NX(w)->selected ? 600 : 400) : 560;
  return true;
}

static void nav_select(AwraWidget *w, bool selected, bool notify)
{
  NavExt *x = NX(w);
  if (x->selected == selected) {
    return;
  }
  x->selected = selected;
  /* the selected item's icon takes the accent */
  if (selected) {
    awra_image_set_color(x->icon, &awra_tokens()->accent_content);
  }
  else {
    awra_image_set_color(x->icon, NULL);
  }
  awra_invalidate(w);
  if (selected) {
    for (AwraWidget *o = x->group_next; o && o != w; o = NX(o)->group_next) {
      nav_select(o, false, true);
    }
  }
  if (notify) {
    awra_emit_active(w, AWRA_SIGNAL_TOGGLED, selected);
  }
}

static void nav_activate(AwraWidget *w)
{
  nav_select(w, true, true);
  awra_emit_simple(w, AWRA_SIGNAL_CLICKED);
}

static int nav_release(AwraWidget *w, BKEvent *e)
{
  if (e->inside) {
    nav_activate(w);
  }
  return 1;
}

static void nav_style_changed(AwraWidget *w)
{
  if (NX(w)->selected) {
    awra_image_set_color(NX(w)->icon, &awra_tokens()->accent_content);
  }
}

/* a suffix (free space, a count) after the label */
static void nav_add(AwraWidget *w, AwraWidget *child)
{
  awra_box_append(NX(w)->box, child);
}

static void nav_init(AwraWidget *w)
{
  NX(w)->group_next = w;
}

static void nav_finalize(AwraWidget *w)
{
  NavExt *x = NX(w);
  if (x->group_next && x->group_next != w) {
    AwraWidget *p = x->group_next;
    while (NX(p)->group_next != w) {
      p = NX(p)->group_next;
    }
    NX(p)->group_next = x->group_next;
  }
}

static AwraClass nav_class = {
    .bk = AWRA_BK("AwraNavigationItem", CLS_INTERACTIVE),
    .type = "AwraNavigationItem",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_HOVER | AWRA_CLASS_FOCUSABLE | AWRA_CLASS_ACTIVATABLE,
    .ext_size = sizeof(NavExt),
    .init = nav_init,
    .finalize = nav_finalize,
    .measure = nav_measure,
    .allocate = nav_allocate,
    .draw = nav_draw,
    .release = nav_release,
    .activate = nav_activate,
    .child_style = nav_child_style,
    .style_changed = nav_style_changed,
    .add = nav_add,
    .overdraw = 4,
};

AwraWidget *awra_navigation_item_new(const char *label, const char *icon)
{
  AwraWidget *w = awra_widget_new(&nav_class);
  NavExt *x = NX(w);
  x->box = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 22);
  x->icon = awra_image_new_from_icon_name(icon);
  awra_widget_set_valign(x->icon, AWRA_ALIGN_CENTER);
  awra_widget_set_visible(x->icon, icon && icon[0]);
  x->label = awra_label_new(label ? label : "");
  awra_label_set_xalign(x->label, 0.0f);
  awra_label_set_ellipsize(x->label, AWRA_ELLIPSIZE_END);
  awra_widget_set_hexpand(x->label, true);
  awra_box_append(x->box, x->icon);
  awra_box_append(x->box, x->label);
  awra_child_insert(w, x->box, -1);
  return w;
}

void awra_navigation_item_set_label(AwraWidget *w, const char *l)
{
  awra_label_set_text(NX(w)->label, l);
}

void awra_navigation_item_set_icon_name(AwraWidget *w, const char *icon)
{
  awra_image_set_icon_name(NX(w)->icon, icon);
  awra_widget_set_visible(NX(w)->icon, icon && icon[0]);
}

void awra_navigation_item_set_selected(AwraWidget *w, bool s)
{
  nav_select(w, s, false);
}

bool awra_navigation_item_selected(AwraWidget *w)
{
  return NX(w)->selected;
}

void awra_navigation_item_set_badge(AwraWidget *w, const char *badge)
{
  NavExt *x = NX(w);
  if (!x->badge && badge) {
    x->badge = awra_badge_new(badge, AWRA_BADGE_NEUTRAL);
    awra_widget_set_valign(x->badge, AWRA_ALIGN_CENTER);
    awra_widget_set_margin_start(x->badge, -18);
    awra_box_append(x->box, x->badge);
  }
  if (x->badge) {
    awra_badge_set_label(x->badge, badge ? badge : "");
    awra_widget_set_visible(x->badge, badge && badge[0]);
  }
}

void awra_navigation_item_set_group(AwraWidget *w, AwraWidget *m)
{
  if (!m || m == w) {
    return;
  }
  NX(w)->group_next = NX(m)->group_next;
  NX(m)->group_next = w;
}

/* ======================================================================= */
/* Split view (spec A §6): sidebar | 1 px separator | content [| end pane] */

typedef struct SplitExt {
  AwraWidget *sidebar, *content, *end;
  int sidebar_width, end_width; /* logical; 0 = the breakpoints' */
  bool show_sidebar;
} SplitExt;

#define SPLX(w) AWRA_EXT(w, SplitExt)

static int split_sidebar_w(AwraWidget *w, int width)
{
  SplitExt *x = SPLX(w);
  if (!x->sidebar || !x->show_sidebar || !awra_child_visible(x->sidebar)) {
    return -1;
  }
  const int lw = (int)(width / awra_scale());
  if (lw < 600) {
    return -1;
  }
  int inset = 0;
  if (awra_is_widget(x->sidebar)) {
    inset = awra_aux(x->sidebar)->margin[1] + awra_aux(x->sidebar)->margin[3];
  }
  if (x->sidebar_width > 0) {
    return awra_px(x->sidebar_width + inset);
  }
  return awra_px((lw < 900 ? 220 : 244) + inset);
}

static void split_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  SplitExt *x = SPLX(w);
  int sm = 0, sn = 0, cm = 0, cn = 0, em = 0, en = 0;
  if (x->sidebar && x->show_sidebar) {
    awra_measure(x->sidebar, o, -1, &sm, &sn);
  }
  if (x->content) {
    awra_measure(x->content, o, -1, &cm, &cn);
  }
  if (x->end && awra_child_visible(x->end)) {
    awra_measure(x->end, o, -1, &em, &en);
  }
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    *mn = cm + em;
    *nt = sn + awra_px(1) + cn + en;
  }
  else {
    *mn = BK_MAX(sm, BK_MAX(cm, em));
    *nt = BK_MAX(sn, BK_MAX(cn, en));
  }
}

static void split_allocate(AwraWidget *w)
{
  SplitExt *x = SPLX(w);
  const int sw = split_sidebar_w(w, w->w);
  if (x->sidebar) {
    awra_widget_set_visible(x->sidebar, x->show_sidebar && sw >= 0);
  }
  int cx = w->x;
  if (sw >= 0 && x->sidebar && awra_child_visible(x->sidebar)) {
    awra_allocate(x->sidebar, w->x, w->y, sw, w->h);
    cx += sw + awra_px(1);
  }
  int ew = 0;
  if (x->end && awra_child_visible(x->end)) {
    int mn, nt;
    awra_measure(x->end, AWRA_ORIENTATION_HORIZONTAL, w->h, &mn, &nt);
    ew = x->end_width > 0 ? awra_px(x->end_width) : nt;
    awra_allocate(x->end, w->x + w->w - ew, w->y, ew, w->h);
  }
  if (x->content) {
    awra_allocate(x->content, cx, w->y, BK_MAX(0, w->x + w->w - ew - cx), w->h);
  }
}

static void split_draw(AwraWidget *w, AwraDraw *d)
{
  SplitExt *x = SPLX(w);
  const int sw = split_sidebar_w(w, w->w);
  if (sw >= 0 && x->sidebar && awra_child_visible(x->sidebar)) {
    awra_paint_rect(d, (AwraBox){(float)(w->x + sw), (float)w->y, awra_pxf(1), (float)w->h},
                    &awra_tokens()->separator);
  }
}

static AwraClass split_class = {
    .bk = AWRA_BK("AwraSplitView", 0),
    .type = "AwraSplitView",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(SplitExt),
    .measure = split_measure,
    .allocate = split_allocate,
    .draw = split_draw,
};

AwraWidget *awra_split_view_new(void)
{
  AwraWidget *w = awra_widget_new(&split_class);
  SPLX(w)->show_sidebar = true;
  return w;
}

void awra_split_view_set_sidebar(AwraWidget *w, AwraWidget *s)
{
  awra_set_slot(w, &SPLX(w)->sidebar, s);
}

void awra_split_view_set_content(AwraWidget *w, AwraWidget *c)
{
  awra_set_slot(w, &SPLX(w)->content, c);
}

void awra_split_view_set_sidebar_width(AwraWidget *w, int width)
{
  SPLX(w)->sidebar_width = width;
  awra_widget_queue_resize(w);
}

void awra_split_view_set_show_sidebar(AwraWidget *w, bool show)
{
  SPLX(w)->show_sidebar = show;
  awra_widget_queue_resize(w);
}

void awra_split_view_set_end_pane(AwraWidget *w, AwraWidget *pane)
{
  awra_set_slot(w, &SPLX(w)->end, pane);
}

void awra_split_view_set_end_pane_width(AwraWidget *w, int width)
{
  SPLX(w)->end_width = width;
  awra_widget_queue_resize(w);
}

