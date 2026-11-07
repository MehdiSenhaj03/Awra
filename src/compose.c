/* Awra 2 composition: surfaces, cards, pages, sections, forms, action rows,
 * badges, status banners, empty / loading states, the sidebar, navigation
 * items and the split view (spec A §4-13, spec B §13).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

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
  AwraWidget *backdrop;      /* surfaces: a local backdrop source */
  short inset;               /* sidebars: logical px around the painted surface,
                              * inside the size request (Awra 1's inner surface margins) */
  short inset_end;           /* sidebars: at the end edge (-1: inset) */
  bool drew_backdrop;        /* surfaces: the last draw went over the backdrop */
} BinExt;

bool awra_paint_material_backdrop(AwraDraw *d, AwraBox b, AwraMaterial material, int role,
                                  AwraElevation elevation, float radius, int corner_mask,
                                  int edge_mask, AwraWidget *source);

#define BINX(w) AWRA_EXT(w, BinExt)

static void bin_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  BinExt *x = BINX(w);
  const int ie = x->inset_end >= 0 ? x->inset_end : x->inset;
  const int ph = awra_px(x->pad[1]) + awra_px(x->pad[3]) + awra_px(x->inset) + awra_px(ie);
  const int pv = awra_px(x->pad[0]) + awra_px(x->pad[2]) + 2 * awra_px(x->inset);
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
  const int in = awra_px(x->inset);
  const int ie = awra_px(x->inset_end >= 0 ? x->inset_end : x->inset);
  const int t = awra_px(x->pad[0]) + in, e = awra_px(x->pad[1]) + ie, b = awra_px(x->pad[2]) + in,
            s = awra_px(x->pad[3]) + in;
  awra_allocate(x->child, w->x + s, w->y + t, BK_MAX(0, w->w - s - e), BK_MAX(0, w->h - t - b));
}

/* Awra 1's surfaces clip their child to their rounded outline (clip-child,
 * on by default): the pixels under the corners are kept on a stack as a
 * surface draws, and what its child drew outside the outline goes back to
 * them after (nested surfaces push and pop in turn). */
static uint32_t *g_corners;
static size_t g_corners_top, g_corners_cap;

static bool surface_clip_box(AwraWidget *w, AwraBox *r_box, float radii[4])
{
  BinExt *x = BINX(w);
  if (!x->paints || !x->child || !awra_child_visible(x->child)) {
    return false;
  }
  AwraResolvedMaterial m;
  awra_material_resolve((AwraMaterial)x->material, x->role, (AwraElevation)x->elevation, false, &m);
  const float r = x->radius >= 0 ? awra_pxf(x->radius) : awra_pxf(m.radius);
  if (r <= 0.0f) {
    return false;
  }
  *r_box = awra_box_inset(awra_box_of(w), awra_pxf(x->inset));
  if (x->inset_end >= 0) { /* as surface_draw paints it */
    r_box->w += awra_pxf(x->inset) - awra_pxf(x->inset_end);
  }
  for (int i = 0; i < 4; i++) {
    radii[i] = (x->corner_mask & (1 << i)) ? r : 0.0f;
  }
  return true;
}

static void surface_save_corners(AwraWidget *w, AwraDraw *d)
{
  AwraBox b;
  float radii[4];
  awra_aux(w)->corners_saved = 0;
  if (!surface_clip_box(w, &b, radii)) {
    return;
  }
  const size_t n = (size_t)BUI_raster_corners_size(b.w, b.h, radii);
  if (g_corners_top + n > g_corners_cap) {
    const size_t cap = BK_MAX(g_corners_top + n, g_corners_cap * 2);
    uint32_t *grown = (uint32_t *)realloc(g_corners, sizeof(uint32_t) * cap);
    if (!grown) {
      return;
    }
    g_corners = grown;
    g_corners_cap = cap;
  }
  if (SR_corners_save(d->r, b.x, b.y, b.w, b.h, radii, g_corners + g_corners_top)) {
    g_corners_top += n;
    awra_aux(w)->corners_saved = 1;
  }
}

static AwraBox surface_paint_box(AwraWidget *w)
{
  BinExt *x = BINX(w);
  AwraBox box = awra_box_inset(awra_box_of(w), awra_pxf(x->inset));
  if (x->inset_end >= 0) {
    box.w += awra_pxf(x->inset) - awra_pxf(x->inset_end);
  }
  return box;
}

static void surface_draw_over(AwraWidget *w, AwraDraw *d)
{
  AwraBox b;
  float radii[4];
  if (awra_aux(w)->corners_saved) {
    awra_aux(w)->corners_saved = 0;
    if (surface_clip_box(w, &b, radii)) { /* (it can't change while it draws) */
      const size_t n = (size_t)BUI_raster_corners_size(b.w, b.h, radii);
      g_corners_top -= n;
      SR_corners_clip(d->r, b.x, b.y, b.w, b.h, radii, g_corners + g_corners_top);
    }
  }
  BinExt *x = BINX(w);
  if (!x->paints) {
    return;
  }
  AwraResolvedMaterial m;
  awra_material_resolve((AwraMaterial)x->material, x->role, (AwraElevation)x->elevation,
                        x->drew_backdrop, &m);
  const float r = x->radius >= 0 ? awra_pxf(x->radius) : awra_pxf(m.radius);
  awra_paint_material_edges(d, surface_paint_box(w), &m, r, x->corner_mask, x->edge_mask);
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
  AwraBox box = surface_paint_box(w);
  /* the fill under the content; the outline and rim over it
   * (surface_draw_over), as AwraSurface's snapshot */
  x->drew_backdrop = x->backdrop &&
                     awra_paint_material_backdrop(d, box, (AwraMaterial)x->material, x->role,
                                                  (AwraElevation)x->elevation, r, x->corner_mask, 0,
                                                  x->backdrop);
  if (!x->drew_backdrop) {
    awra_paint_material(d, box, &m, r, x->corner_mask, 0, (AwraElevation)x->elevation);
  }
  surface_save_corners(w, d);
}

static void bin_finalize(AwraWidget *w)
{
  if (BINX(w)->backdrop) {
    awra_widget_unref(BINX(w)->backdrop);
  }
}

void awra_surface_set_backdrop(AwraWidget *w, AwraWidget *source)
{
  BinExt *x = BINX(w);
  if (source) {
    awra_widget_ref(source);
  }
  if (x->backdrop) {
    awra_widget_unref(x->backdrop);
  }
  x->backdrop = source;
  awra_invalidate(w);
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
    .finalize = bin_finalize,
    .measure = bin_measure,
    .allocate = bin_allocate,
    .draw = surface_draw,
    .draw_over = surface_draw_over,
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

static void card_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  if (BINX(w)->activatable) {
    info->action = "click";
  }
}

static bool card_a11y_do(AwraWidget *w, int item, int verb, double value)
{
  if (item < 0 && verb == AWRA_A11Y_DO_ACTIVATE && BINX(w)->activatable && awra_sensitive(w)) {
    awra_emit_simple(w, AWRA_SIGNAL_CLICKED);
    return true;
  }
  return false;
}

static AwraClass card_class = {
    .a11y = card_a11y,
    .a11y_do = card_a11y_do,
    .bk = AWRA_BK("AwraCard", 0),
    .type = "AwraCard",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_HOVER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(BinExt),
    .init = bin_init,
    .measure = bin_measure,
    .allocate = bin_allocate,
    .draw = card_draw,
    .draw_over = surface_draw_over,
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
  /* Awra 1: translated by (width - child_width) / 2.0f, half a pixel
   * further when that is odd */
  awra_widget_set_draw_shift(c, (float)((w->w - cw) & 1) * 0.5f, 0.0f);
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

typedef struct StateParts StateParts;
static StateParts *sp(AwraWidget *w);
static void state_a11y(AwraWidget *w, StateParts *p, AwraA11yInfo *info);

static void page_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  StateParts *p = sp(w);
  if (p) {
    state_a11y(w, p, info);
  }
}

static AwraClass page_class = {
    .a11y = page_a11y,
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
  awra_widget_unref(box); /* its parent holds it */
  awra_scrolled_set_child(scroll, col);
  awra_widget_unref(col); /* its parent holds it */
  bin_set_child(w, scroll);
  awra_widget_unref(scroll); /* its parent holds it */
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
  awra_widget_unref(h->title); /* its parent holds it */
  awra_box_append(h->titles, h->subtitle);
  awra_widget_unref(h->subtitle); /* its parent holds it */
  awra_box_append(h->heading, h->titles);
  awra_widget_unref(h->titles); /* its parent holds it */
  awra_box_append(root, h->heading);
  awra_widget_unref(h->heading); /* its parent holds it */
  awra_widget_set_visible(h->title, title && title[0]);
  awra_widget_set_visible(h->subtitle, subtitle && subtitle[0]);
  awra_widget_set_visible(h->heading, (title && title[0]) || (subtitle && subtitle[0]));
  awra_widget_isolate_expand(root);
  /* Awra 1's page header and section: a group named by its title, which is
   * a heading (level 1 for pages, 2 for sections) */
  awra_accessible_set_role(root, AWRA_ACCESSIBLE_ROLE_GROUP);
  awra_accessible_update_relation(root, AWRA_ACCESSIBLE_RELATION_LABELLED_BY, &h->title, 1);
  awra_accessible_set_role(h->title, AWRA_ACCESSIBLE_ROLE_HEADING);
  awra_accessible_update_property_number(h->title, AWRA_ACCESSIBLE_PROPERTY_LEVEL,
                                         title_t == AWRA_TYPOGRAPHY_TITLE_1 ? 1 : 2);
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
    awra_widget_unref(h->actions); /* its parent holds it */
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
  AwraWidget *w = awra_box_new(AWRA_ORIENTATION_VERTICAL, 16);
  /* its rows' labels are one column (Awra 1's horizontal size group) */
  awra_widget_set_data(w, "awra-form", (void *)1, NULL);
  return w;
}

void awra_form_append(AwraWidget *form, AwraWidget *row)
{
  awra_box_append(form, row);
}

typedef struct FormRowExt {
  AwraWidget *label, *field, *control, *help, *error;
  bool vertical, compact;
  bool has_help, has_error;
  int validation;
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

/* A row's label width request: in a form, the widest of the form's
 * labels (Awra 1's AwraForm puts them in a horizontal GtkSizeGroup). */
static void formrow_label_request(AwraWidget *w, int *r_min, int *r_nat)
{
  FormRowExt *x = FRX(w);
  awra_measure(x->label, AWRA_ORIENTATION_HORIZONTAL, -1, r_min, r_nat);
  AwraWidget *form = (AwraWidget *)w->parent;
  if (!form || !awra_is_widget(form) || !awra_widget_get_data(form, "awra-form")) {
    return;
  }
  for (int i = 0; i < form->nkids; i++) {
    AwraWidget *row = form->kids[i];
    if (row == w || !awra_widget_is(row, "AwraFormRow") || !awra_child_visible(row)) {
      continue;
    }
    FormRowExt *rx = FRX(row);
    if (!rx->label || !awra_child_visible(rx->label) || rx->vertical) {
      continue;
    }
    int mn, nt;
    awra_measure(rx->label, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nt);
    *r_min = BK_MAX(*r_min, mn);
    *r_nat = BK_MAX(*r_nat, nt);
  }
}

/* The label column beside the control for a row width: the row is a
 * GtkBox (label at least 224 wide, the field expanding), so the label gets
 * its natural width when there is room for it. */
static int formrow_label_width(AwraWidget *w, int width)
{
  FormRowExt *x = FRX(w);
  int lm, ln, fm, fn;
  formrow_label_request(w, &lm, &ln);
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

/* Awra 1's AwraFormRow: a group named by its label, described by its help
 * or its error */
static void formrow_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  FormRowExt *x = FRX(w);
  if (x->label && awra_widget_visible(x->label)) {
    info->label = awra_label_text(x->label);
  }
  if (x->error && awra_widget_visible(x->error)) {
    info->description = awra_label_text(x->error);
  }
  else if (x->help && awra_widget_visible(x->help)) {
    info->description = awra_label_text(x->help);
  }
}

static AwraClass formrow_class = {
    .a11y = formrow_a11y,
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
  x->error = awra_label_new("");
  awra_widget_set_typography(x->error, AWRA_TYPOGRAPHY_MUTED);
  awra_widget_add_class(x->error, "error");
  awra_label_set_wrap(x->error, AWRA_WRAP_WORD);
  awra_label_set_xalign(x->error, 0.0f);
  awra_widget_set_visible(x->error, false);
  awra_box_append(x->field, x->error);
  x->has_help = subtitle && subtitle[0];
  awra_child_insert(w, x->label, -1);
  awra_child_insert(w, x->field, -1);
  /* the row's tree holds its parts (they were leaked with it) */
  awra_widget_unref(x->label);
  awra_widget_unref(x->field);
  awra_widget_unref(x->help);
  awra_widget_unref(x->error);
  return w;
}

void awra_form_row_set_control(AwraWidget *w, AwraWidget *control)
{
  FormRowExt *x = FRX(w);
  if (x->control) {
    awra_accessible_reset_relation(x->control, AWRA_ACCESSIBLE_RELATION_LABELLED_BY);
    awra_accessible_reset_relation(x->control, AWRA_ACCESSIBLE_RELATION_DESCRIBED_BY);
    awra_box_remove(x->field, x->control);
  }
  x->control = control;
  if (control) {
    /* the row's label names its control, its help describes it */
    awra_accessible_update_relation(control, AWRA_ACCESSIBLE_RELATION_LABELLED_BY, &x->label, 1);
    AwraWidget *desc[2] = {x->help, x->error};
    awra_accessible_update_relation(control, AWRA_ACCESSIBLE_RELATION_DESCRIBED_BY, desc, 2);
    /* text fields and pickers fill the field column; compact controls
     * (spin buttons, switches, segmented controls, captures...) keep their
     * natural width; an explicit hexpand wins (awra-form-row.c) */
    if (!awra_aux(control)->hexpand_set) {
      const char *name = awra_widget_name(control);
      const bool fills = (awra_widget_is(control, "AwraEntry") &&
                          !awra_widget_is(control, "AwraSpinButton")) ||
                         awra_widget_is(control, "AwraDropdown") ||
                         awra_widget_is(control, "AwraScrolledWindow") ||
                         awra_widget_is(control, "AwraTextView") ||
                         (name && (!strcmp(name, "AwraPasswordEntry") || !strcmp(name, "AwraTagEntry") ||
                                   !strcmp(name, "AwraTextArea")));
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

/* Awra 1's update_validation: the error message, in the danger colour,
 * takes the help text's place while the state is an error; the control's
 * border follows the state. */
static void formrow_update_validation(AwraWidget *w)
{
  FormRowExt *x = FRX(w);
  const bool show_error = x->validation == AWRA_VALIDATION_ERROR && x->has_error;
  awra_widget_set_visible(x->error, show_error);
  awra_widget_set_visible(x->help, !show_error && x->has_help);
  static const char *const classes[] = {NULL, "validation-success", "validation-warning", "validation-error"};
  if (x->control) {
    for (int i = 1; i < 4; i++) {
      awra_widget_remove_class(x->control, classes[i]);
    }
    if (x->validation > 0 && x->validation < 4) {
      awra_widget_add_class(x->control, classes[x->validation]);
    }
    if (show_error) {
      awra_accessible_update_state(x->control, AWRA_ACCESSIBLE_STATE_INVALID,
                                   AWRA_ACCESSIBLE_INVALID_TRUE);
    }
    else {
      awra_accessible_reset_state(x->control, AWRA_ACCESSIBLE_STATE_INVALID);
    }
    awra_invalidate(x->control);
  }
}

void awra_form_row_set_subtitle(AwraWidget *w, const char *s)
{
  awra_label_set_text(FRX(w)->help, s);
  FRX(w)->has_help = s && s[0];
  formrow_update_validation(w);
}

void awra_form_row_set_error_message(AwraWidget *w, const char *message)
{
  awra_label_set_text(FRX(w)->error, message);
  FRX(w)->has_error = message && message[0];
  formrow_update_validation(w);
}

void awra_form_row_set_validation_state(AwraWidget *w, AwraValidationState state)
{
  FRX(w)->validation = state;
  formrow_update_validation(w);
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

static void arow_set_activatable(AwraWidget *w, bool a);

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
    /* awra.css: .awra-action-row { margin: 1px 0 } outside the row */
    *mn = BK_MAX(*mn, awra_px(42)) + pv + 2 * awra_px(1);
    *nt = BK_MAX(*nt, awra_px(42)) + pv + 2 * awra_px(1);
  }
}

/* The row inside its 1 px vertical CSS margin. */
static AwraBox arow_box(AwraWidget *w)
{
  AwraBox b = awra_box_of(w);
  b.y += awra_pxf(1);
  b.h -= 2 * awra_pxf(1);
  return b;
}

static void arow_allocate(AwraWidget *w)
{
  awra_allocate(ARX(w)->box, w->x + awra_px(10), w->y + awra_px(1) + awra_px(8), w->w - awra_px(20),
                w->h - awra_px(16) - 2 * awra_px(1));
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
  awra_paint_button_bg(d, arow_box(w), awra_pxf(11), &c, 0.0f, awra_focus_visible(w));
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

/* Awra 1's AwraActionRow: a button (activatable) or a group, named by its
 * title, described by its subtitle */
static void arow_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  ActionRowExt *x = ARX(w);
  info->role = x->activatable ? AWRA_ACCESSIBLE_ROLE_BUTTON : AWRA_ACCESSIBLE_ROLE_GROUP;
  info->label = awra_label_text(x->title);
  if (awra_widget_visible(x->subtitle)) {
    info->description = awra_label_text(x->subtitle);
  }
  info->action = x->activatable ? "click" : NULL;
  if (x->activatable) {
    info->flags |= AWRA_A11Y_FOCUSABLE;
  }
}

static bool arow_a11y_do(AwraWidget *w, int item, int verb, double value)
{
  if (item < 0 && verb == AWRA_A11Y_DO_ACTIVATE && ARX(w)->activatable && awra_sensitive(w)) {
    awra_emit_simple(w, AWRA_SIGNAL_ACTIVATE);
    awra_emit_simple(w, AWRA_SIGNAL_CLICKED);
    return true;
  }
  return false;
}

static AwraClass arow_class = {
    .a11y = arow_a11y,
    .a11y_do = arow_a11y_do,
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
  awra_widget_unref(x->title); /* its parent holds it */
  awra_box_append(texts, x->subtitle);
  awra_widget_unref(x->subtitle); /* its parent holds it */
  awra_box_append(x->box, x->icon);
  awra_widget_unref(x->icon); /* its parent holds it */
  awra_box_append(x->box, texts);
  awra_widget_unref(texts); /* its parent holds it */
  awra_child_insert(w, x->box, -1);
  awra_widget_unref(x->box); /* its parent holds it */
  /* Awra 1's row is a flat GtkButton: it hovers, takes focus and clicks */
  arow_set_activatable(w, true);
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

/* awra_action_row_set_suffix: Awra 1's one suffix, replaced, appended to the
 * row's box as it comes (a button fills the row's height, as in GTK) */
void awra_action_row_set_suffix(AwraWidget *w, AwraWidget *suffix)
{
  ActionRowExt *x = ARX(w);
  if (x->suffix == suffix) {
    return;
  }
  if (x->suffix) {
    awra_box_remove(x->box, x->suffix);
  }
  x->suffix = suffix;
  if (suffix) {
    awra_box_append(x->box, suffix);
  }
}

AwraWidget *awra_action_row_get_suffix(AwraWidget *w)
{
  return ARX(w)->suffix;
}

static void arow_set_activatable(AwraWidget *w, bool a)
{
  ARX(w)->activatable = a;
  if (a) {
    w->flags |= NF_EVENTS;
  }
  else {
    w->flags &= ~NF_EVENTS;
  }
  awra_widget_set_focusable(w, a);
  awra_invalidate(w);
}

void awra_action_row_set_activatable(AwraWidget *w, bool a)
{
  arow_set_activatable(w, a);
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
  awra_widget_unref(l);
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

static void banner_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  BinExt *x = BINX(w);
  if (x->b && awra_widget_visible(x->b)) {
    info->label = awra_label_text(x->b);
  }
  if (x->c && awra_widget_visible(x->c)) {
    info->description = awra_label_text(x->c);
  }
}

static AwraClass banner_class = {
    .a11y = banner_a11y,
    .bk = AWRA_BK("AwraStatusBanner", 0),
    .type = "AwraStatusBanner",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(BinExt),
    .init = bin_init,
    .measure = bin_measure,
    .allocate = bin_allocate,
    .draw = surface_draw,
    .draw_over = surface_draw_over,
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
  awra_image_set_pixel_size(x->a, 20); /* -gtk-icon-size: 20px, centred in the row (Awra 1) */
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
  awra_widget_unref(x->b); /* its parent holds it */
  awra_box_append(copy, x->c);
  awra_widget_unref(x->c); /* its parent holds it */
  awra_box_append(root, x->a);
  awra_widget_unref(x->a); /* its parent holds it */
  awra_box_append(root, copy);
  awra_widget_unref(copy); /* its parent holds it */
  x->d = awra_button_new_from_icon_name("awra-close-symbolic");
  awra_accessible_update_property_string(x->d, AWRA_ACCESSIBLE_PROPERTY_LABEL, "Dismiss");
  awra_button_set_appearance(x->d, AWRA_BUTTON_GHOST);
  awra_widget_set_visible(x->d, false);
  awra_widget_set_valign(x->d, AWRA_ALIGN_START);
  awra_signal_connect(x->d, AWRA_SIGNAL_CLICKED, banner_dismiss, w, NULL);
  awra_box_append(root, x->d);
  awra_widget_unref(x->d); /* its parent holds it */
  bin_set_child(w, root);
  awra_widget_unref(root); /* its parent holds it */
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

struct StateParts {
  AwraWidget *icon, *title, *desc, *col;
  int kind; /* 0 empty, 1 loading, 2 error */
};

/* Awra 1's state views: empty a group, loading a busy progress bar, error
 * an alert; named by their title, described by their text */
static void state_a11y(AwraWidget *w, StateParts *p, AwraA11yInfo *info)
{
  info->role = p->kind == 1 ? AWRA_ACCESSIBLE_ROLE_PROGRESS_BAR :
               p->kind == 2 ? AWRA_ACCESSIBLE_ROLE_ALERT : AWRA_ACCESSIBLE_ROLE_GROUP;
  info->label = awra_label_text(p->title);
  info->description = awra_label_text(p->desc);
  if (p->kind == 1) {
    awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_BUSY, 1);
  }
}

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
  awra_widget_unref(icon); /* its parent holds it (the caller gave it) */
  awra_box_append(p->col, p->title);
  awra_widget_unref(p->title); /* its parent holds it */
  awra_box_append(p->col, p->desc);
  awra_widget_unref(p->desc); /* its parent holds it */
  bin_set_child(w, p->col);
  awra_widget_unref(p->col); /* its parent holds it */
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

AwraWidget *awra_error_state_new(const char *title, const char *desc)
{
  AwraWidget *icon = awra_image_new_from_icon_name("awra-status-error-symbolic");
  awra_image_set_pixel_size(icon, 32);
  awra_image_set_color(icon, &awra_tokens()->danger);
  AwraWidget *w = state_new(icon, title, desc);
  sp(w)->kind = 2;
  return w;
}

AwraWidget *awra_loading_state_new(const char *title, const char *desc)
{
  AwraWidget *s = awra_spinner_new();
  awra_spinner_set_size_internal(s, 36);
  awra_spinner_set_spinning(s, true);
  awra_widget_set_data(s, "awra-accent", (void *)1, NULL); /* @awra_accent_content */
  AwraWidget *w = state_new(s, title, desc);
  sp(w)->kind = 1;
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
  x->inset = 12;
  x->inset_end = -1;
  AwraWidget *col = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  AwraWidget *list = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  awra_widget_set_vexpand(list, true);
  awra_box_append(col, list);
  awra_widget_unref(list); /* its parent holds it */
  bin_set_child(w, col);
  awra_widget_unref(col); /* its parent holds it */
  x->a = list;
  x->b = col;
  awra_widget_set_name(w, "AwraSidebar");
  awra_accessible_set_role(w, AWRA_ACCESSIBLE_ROLE_NAVIGATION);
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
  awra_widget_unref(l); /* its parent holds it */
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
  BINX(w)->inset = (short)m;
  awra_widget_queue_resize(w);
  BINX(w)->corner_mask = m > 0 ? 15 : 0;
  BINX(w)->edge_mask = m > 0 ? AWRA_EDGE_ALL : AWRA_EDGE_END;
  awra_invalidate(w);
}

void awra_sidebar_set_end_inset(AwraWidget *w, int px)
{
  BINX(w)->inset_end = (short)(px < 0 ? -1 : px);
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

/* ======================================================================= */
/* Navigation item (spec A §5): 42 px rows, radius 9, 22 px icon gap */

typedef struct NavExt {
  AwraWidget *box, *icon, *label, *badge, *texts, *subtitle, *prefix, *suffix, *child;
  bool selected, muted, emphasized, has_label_color;
  AwraRGBA label_color;
  AwraWidget *group_next;
  bool in_sidebar;
} NavExt;

#define NX(w) AWRA_EXT(w, NavExt)

/* the list role an ancestor gives its items: 0 none, 1 compact navigation
 * (32 px rows), 2 people list */
static int nav_role(const AwraWidget *w)
{
  static int compact = -2, people = -2;
  if (compact == -2) {
    compact = awra_class_bit("compact-navigation");
    people = awra_class_bit("people-list");
  }
  for (const BKNode *p = w->parent; p; p = p->parent) {
    if (!awra_is_widget(p)) {
      continue;
    }
    if (awra_has_class_bit(p, compact)) {
      return 1;
    }
    if (awra_has_class_bit(p, people)) {
      return 2;
    }
  }
  return 0;
}

static void nav_margins(AwraWidget *w, int *mx)
{
  *mx = 0;
  if (nav_role(w)) {
    *mx = awra_px(8);
    return;
  }
  for (BKNode *p = w->parent; p; p = p->parent) {
    if (awra_is_widget(p) && awra_widget_name(p) && !strcmp(awra_widget_name(p), "AwraSidebar")) {
      *mx = awra_px(8);
      return;
    }
  }
}

/* padding x, y and the least height of the content (logical) */
static void nav_pads(AwraWidget *w, int *px, int *py, int *min_h)
{
  switch (nav_role(w)) {
    case 1: *px = 8; *py = 6; *min_h = 20; break;
    case 2: *px = 8; *py = 6; *min_h = 28; break;
    default: *px = 11; *py = 7; *min_h = 28; break;
  }
}

static void nav_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  int mx, px, py, min_h;
  nav_margins(w, &mx);
  nav_pads(w, &px, &py, &min_h);
  awra_measure(NX(w)->box, o, -1, mn, nt);
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    *mn += awra_px(2 * px) + 2 * mx;
    *nt += awra_px(2 * px) + 2 * mx;
  }
  else {
    *mn = BK_MAX(*mn, awra_px(min_h)) + awra_px(2 * py) + awra_px(2);
    *nt = BK_MAX(*nt, awra_px(min_h)) + awra_px(2 * py) + awra_px(2);
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
  int px, py, min_h;
  nav_pads(w, &px, &py, &min_h);
  AwraBox b = nav_box(w);
  awra_allocate(NX(w)->box, (int)b.x + awra_px(px), (int)b.y + awra_px(py), (int)b.w - awra_px(2 * px),
                (int)b.h - awra_px(2 * py));
}

static void nav_colors(AwraWidget *w, AwraButtonColors *c)
{
  const float ht = awra_hover_t(w);
  AwraButtonColors rest, hov;
  awra_button_colors(AWRA_LOOK_NAV_ITEM, false, awra_pressed(w), NX(w)->selected, awra_sensitive(w), &rest);
  awra_button_colors(AWRA_LOOK_NAV_ITEM, true, awra_pressed(w), NX(w)->selected, awra_sensitive(w), &hov);
  const AwraTokens *t = awra_tokens();
  const int role = nav_role(w);
  if (role == 2 || NX(w)->emphasized) {
    rest.fg = t->foreground; /* people, or something new: full contrast */
  }
  if (NX(w)->muted && !NX(w)->selected) {
    rest.fg = role == 2 ? t->muted : awra_with_alpha(t->muted, t->muted.a * 0.6f);
  }
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
  const float radius = awra_pxf(nav_role(w) == 1 ? 8 : 9);
  awra_paint_button_bg(d, nav_box(w), radius, &c, awra_style_reduced_motion() ? 0.0f : awra_press_t(w), false);
  if (awra_focus_visible(w)) {
    const float ow = awra_pxf(2), off = awra_pxf(1);
    AwraBox b = nav_box(w);
    AwraBox o = {b.x - off - ow, b.y - off - ow, b.w + 2 * (off + ow), b.h + 2 * (off + ow)};
    awra_paint_rbox(d, o, radius + off + ow, NULL, ow, &awra_tokens()->focus_ring);
  }
}

static int nav_label_weight(AwraWidget *w)
{
  NavExt *x = NX(w);
  switch (nav_role(w)) {
    case 1: return x->selected ? 600 : x->emphasized ? 650 : 400;
    case 2: return x->emphasized ? 700 : 500;
    default: return x->emphasized ? 700 : 560;
  }
}

static bool nav_child_style(AwraWidget *w, AwraRGBA *col, int *weight)
{
  AwraButtonColors c;
  nav_colors(w, &c);
  *col = c.fg;
  *weight = nav_label_weight(w);
  return true;
}

/* a colour a label reads in on this appearance (Awra 1's awra_rgba_readable) */
AwraRGBA awra_rgba_readable(AwraRGBA c)
{
  const bool dark = awra_tokens()->dark;
  const float l = 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
  if (dark && l < 0.35f) {
    return awra_mix(c, awra_rgba(1, 1, 1, 1), (0.35f - l) / 0.65f + 0.15f);
  }
  if (!dark && l > 0.6f) {
    return awra_mix(c, awra_rgba(0, 0, 0, 1), (l - 0.6f) / 0.4f + 0.2f);
  }
  return c;
}

static void nav_sync_label(AwraWidget *w)
{
  NavExt *x = NX(w);
  awra_label_set_weight(x->label, nav_label_weight(w));
  if (x->has_label_color) {
    AwraRGBA c = awra_rgba_readable(x->label_color);
    awra_label_set_color(x->label, &c);
  }
  else {
    awra_label_set_color(x->label, NULL);
  }
  const int role = nav_role(w);
  /* GTK adds the box's spacing (MD, 12) to the CSS border-spacing (8, 12, 10) */
  awra_box_set_spacing(x->box, role == 1 ? 20 : role == 2 ? 24 : 22);
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
  nav_sync_label(w);
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
  nav_sync_label(w);
}

static void nav_map(AwraWidget *w, bool mapped)
{
  if (mapped) {
    nav_sync_label(w); /* its list's role is known once it's in place */
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

/* Awra 1's AwraNavigationItem: a button named by its label, described by
 * its subtitle, selected in its group */
static void nav_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  NavExt *x = NX(w);
  if (x->subtitle && awra_widget_visible(x->subtitle)) {
    info->description = awra_label_text(x->subtitle);
  }
  if (x->in_sidebar || (x->group_next && x->group_next != w) || x->selected) {
    awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_SELECTED, x->selected);
  }
}

static AwraClass nav_class = {
    .a11y = nav_a11y,
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
    .map = nav_map,
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
  x->subtitle = awra_label_new("");
  awra_label_set_xalign(x->subtitle, 0.0f);
  awra_label_set_ellipsize(x->subtitle, AWRA_ELLIPSIZE_END);
  awra_label_set_scale(x->subtitle, 0.86);
  awra_label_set_weight(x->subtitle, 400);
  awra_label_set_color(x->subtitle, &awra_tokens()->muted);
  awra_widget_set_visible(x->subtitle, false);
  x->texts = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  awra_widget_set_hexpand(x->texts, true);
  awra_widget_set_valign(x->texts, AWRA_ALIGN_CENTER);
  awra_box_append(x->texts, x->label);
  awra_box_append(x->texts, x->subtitle);
  awra_box_append(x->box, x->icon);
  awra_box_append(x->box, x->texts);
  awra_child_insert(w, x->box, -1);
  awra_widget_unref(x->label);
  awra_widget_unref(x->subtitle);
  awra_widget_unref(x->texts);
  awra_widget_unref(x->icon);
  awra_widget_unref(x->box);
  nav_sync_label(w);
  return w;
}

void awra_navigation_item_set_label(AwraWidget *w, const char *l)
{
  awra_label_set_text(NX(w)->label, l);
}

const char *awra_navigation_item_label(AwraWidget *w)
{
  return awra_label_text(NX(w)->label);
}

/* Awra 1's custom child: it takes the label and subtitle's place. */
void awra_navigation_item_set_child(AwraWidget *w, AwraWidget *child)
{
  NavExt *x = NX(w);
  if (x->child) {
    awra_box_remove(x->box, x->child);
  }
  x->child = child;
  awra_widget_set_visible(x->texts, child == NULL);
  if (child) {
    awra_widget_set_hexpand(child, true);
    awra_box_insert_after(x->box, child, x->texts);
  }
}

void awra_navigation_item_set_icon_name(AwraWidget *w, const char *icon)
{
  awra_image_set_icon_name(NX(w)->icon, icon);
  awra_widget_set_visible(NX(w)->icon, icon && icon[0] && !NX(w)->prefix);
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
    awra_widget_unref(x->badge);
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

void awra_navigation_item_set_prefix(AwraWidget *w, AwraWidget *prefix)
{
  NavExt *x = NX(w);
  if (x->prefix == prefix) {
    return;
  }
  if (x->prefix) {
    awra_box_remove(x->box, x->prefix);
  }
  x->prefix = prefix;
  if (prefix) {
    awra_widget_set_valign(prefix, AWRA_ALIGN_CENTER);
    awra_widget_set_margins(prefix, -3, 0, -3, 0);
    awra_box_prepend(x->box, prefix);
  }
  awra_widget_set_visible(x->icon, !prefix && awra_widget_visible(x->icon));
}

AwraWidget *awra_navigation_item_prefix(AwraWidget *w)
{
  return NX(w)->prefix;
}

void awra_navigation_item_set_suffix(AwraWidget *w, AwraWidget *suffix)
{
  NavExt *x = NX(w);
  if (x->suffix == suffix) {
    return;
  }
  if (x->suffix) {
    awra_box_remove(x->box, x->suffix);
  }
  x->suffix = suffix;
  if (suffix) {
    /* Awra 1 leaves its alignment alone: a badge fills the row's height */
    awra_widget_set_margin_start(suffix, 4);
    awra_box_append(x->box, suffix);
  }
}

AwraWidget *awra_navigation_item_suffix(AwraWidget *w)
{
  return NX(w)->suffix;
}

void awra_navigation_item_set_subtitle(AwraWidget *w, const char *subtitle)
{
  NavExt *x = NX(w);
  awra_label_set_text(x->subtitle, subtitle ? subtitle : "");
  awra_widget_set_visible(x->subtitle, subtitle && subtitle[0]);
}

void awra_navigation_item_set_muted(AwraWidget *w, bool muted)
{
  NX(w)->muted = muted;
  awra_invalidate(w);
}

void awra_navigation_item_set_emphasized(AwraWidget *w, bool emphasized)
{
  NX(w)->emphasized = emphasized;
  nav_sync_label(w);
  awra_invalidate(w);
}

void awra_navigation_item_set_label_color(AwraWidget *w, const AwraRGBA *color)
{
  NavExt *x = NX(w);
  x->has_label_color = color != NULL;
  if (color) {
    x->label_color = *color;
  }
  nav_sync_label(w);
}

/* ======================================================================= */
/* Split view (spec A §6): sidebar | 1 px separator | content [| end pane] */

typedef struct SplitExt {
  AwraWidget *sidebar, *content, *end;
  int sidebar_width, end_width; /* logical; 0 = the breakpoints' */
  bool show_sidebar;
  /* Awra 1's split view is a GtkPaned: the sidebar's width (`pos`, raster
   * px, -1 unplaced) is set for each layout mode (expanded from 900 px,
   * medium from 600, compact below: no sidebar), then follows the paned's
   * width proportionally as the window resizes, and the user drags it. */
  int pos, last_alloc;
  int placed_mode, placed_inset;
  bool dragging;
  int drag_pos0;
} SplitExt;

#define SPLX(w) AWRA_EXT(w, SplitExt)
/* GtkPaned's handle: a 1 px separator taking presses 6 px on each side */
#define SPLIT_HANDLE_EXTRA 6

/* Awra 1's get_sidebar_inset (times two): an AwraSidebar's inset around its
 * surface, else the sidebar's own margins */
static int sidebar_inset(const SplitExt *x)
{
  if (!awra_is_widget(x->sidebar)) {
    return 0;
  }
  if (AWRA_CLASS_OF(x->sidebar) == &surface_class && BINX(x->sidebar)->role == AWRA_ROLE_SIDEBAR) {
    const BinExt *b = BINX(x->sidebar);
    return b->inset + (b->inset_end >= 0 ? b->inset_end : b->inset);
  }
  return awra_aux(x->sidebar)->margin[1] + awra_aux(x->sidebar)->margin[3];
}

/* 0 compact, 1 medium, 2 expanded (AwraLayoutMode) for a width in px */
static int split_mode(int width)
{
  const int lw = (int)(width / awra_scale());
  return lw < 600 ? 0 : lw < 900 ? 1 : 2;
}

static bool split_has_sidebar(AwraWidget *w)
{
  SplitExt *x = SPLX(w);
  return x->sidebar && x->show_sidebar && split_mode(w->w) != 0;
}

/* The sidebar's least width: the size request of its mode, or its own. */
static int sidebar_req(AwraWidget *w, int mode)
{
  SplitExt *x = SPLX(w);
  int mn, nt;
  awra_measure(x->sidebar, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nt);
  if (x->sidebar_width > 0) {
    return mn;
  }
  return BK_MAX(mn, awra_px((mode == 1 ? 210 : 230) + sidebar_inset(x)));
}

/* gtk_paned_compute_position: the sidebar's width for this allocation. */
static int split_sidebar_w(AwraWidget *w, int width)
{
  SplitExt *x = SPLX(w);
  if (!split_has_sidebar(w)) {
    return -1;
  }
  const int mode = split_mode(width);
  if (x->sidebar_width > 0) {
    return BK_MAX(awra_px(x->sidebar_width + sidebar_inset(x)), sidebar_req(w, mode));
  }
  const int alloc = width - awra_px(1);
  int pos = x->pos;
  if (mode != x->placed_mode || sidebar_inset(x) != x->placed_inset || pos < 0) {
    pos = awra_px((mode == 1 ? 220 : 244) + sidebar_inset(x));
  }
  if (x->last_alloc > 0 && alloc != x->last_alloc && !x->dragging) {
    pos = (int)(alloc * ((double)pos / x->last_alloc) + 0.5);
  }
  int cmn = 0, cnt;
  if (x->content) {
    awra_measure(x->content, AWRA_ORIENTATION_HORIZONTAL, -1, &cmn, &cnt);
  }
  const int mn = sidebar_req(w, mode);
  const int mx = BK_MAX(mn, BK_MAX(1, alloc - cmn));
  return BK_CLAMP(pos, mn, mx);
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
    /* GtkPaned's: the shown sidebar's minimum (it doesn't shrink), the
     * handle and the content's */
    const bool sb = x->sidebar && x->show_sidebar && (x->sidebar->flags & NF_VISIBLE);
    *mn = (sb ? sm + awra_px(1) : 0) + cm + em;
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
  int ew = 0;
  if (x->end && awra_child_visible(x->end)) {
    int mn, nt;
    awra_measure(x->end, AWRA_ORIENTATION_HORIZONTAL, w->h, &mn, &nt);
    ew = x->end_width > 0 ? awra_px(x->end_width) : nt;
    awra_allocate(x->end, w->x + w->w - ew, w->y, ew, w->h);
  }
  const int width = w->w - ew;
  const int sw = split_sidebar_w(w, width);
  if (x->sidebar) {
    awra_widget_set_visible(x->sidebar, sw >= 0);
  }
  int cx = w->x;
  if (sw >= 0) {
    awra_allocate(x->sidebar, w->x, w->y, sw, w->h);
    cx += sw + awra_px(1);
    x->pos = sw;
    x->last_alloc = width - awra_px(1);
    x->placed_mode = split_mode(width);
    x->placed_inset = sidebar_inset(x);
  }
  else {
    x->placed_mode = 0;
  }
  if (x->content) {
    /* GtkPaned gives a child at least its minimum, past the window's end
     * (right to left, mirrored: past its start) */
    int cmn = 0, cnt;
    awra_measure(x->content, AWRA_ORIENTATION_HORIZONTAL, w->h, &cmn, &cnt);
    awra_allocate(x->content, cx, w->y, BK_MAX(cmn, w->x + w->w - ew - cx), w->h);
  }
}

/* The separator's x, or -1 without a sidebar. */
static int split_sep(AwraWidget *w)
{
  SplitExt *x = SPLX(w);
  if (!x->sidebar || !awra_child_visible(x->sidebar) || !split_has_sidebar(w)) {
    return -1;
  }
  /* right to left, the sidebar is mirrored to the right: the line is at its left */
  return awra_widget_rtl(w) ? x->sidebar->x - awra_px(1) : x->sidebar->x + x->sidebar->w;
}

static bool on_split_handle(AwraWidget *w, int px)
{
  const int sep = split_sep(w);
  return sep >= 0 && SPLX(w)->sidebar_width <= 0 && px >= sep - awra_px(SPLIT_HANDLE_EXTRA) &&
         px < sep + awra_px(1) + awra_px(SPLIT_HANDLE_EXTRA);
}

/* the handle takes presses over the children's edges, as GtkPaned's does */
static BKNode *split_hit(BKNode *n, int px, int py)
{
  if (!bk_contains(n, px, py) || !(n->flags & NF_VISIBLE)) {
    return NULL;
  }
  if (on_split_handle(n, px) && awra_aux(n)->can_target) {
    return n;
  }
  return awra_bk_hit(n, px, py);
}

static int split_press(AwraWidget *w, BKEvent *e)
{
  SplitExt *x = SPLX(w);
  if (!on_split_handle(w, e->x)) {
    return 0;
  }
  x->dragging = true;
  x->drag_pos0 = x->pos;
  return 1;
}

static int split_drag(AwraWidget *w, BKEvent *e)
{
  SplitExt *x = SPLX(w);
  if (!x->dragging) {
    return 0;
  }
  x->pos = x->drag_pos0 + (awra_widget_rtl(w) ? -e->dx : e->dx); /* clamped as it is allocated */
  awra_queue_allocate(w);
  awra_invalidate(w);
  return 1;
}

static int split_release(AwraWidget *w, BKEvent *e)
{
  (void)e;
  SplitExt *x = SPLX(w);
  if (!x->dragging) {
    return 0;
  }
  x->dragging = false;
  return 1;
}

static int split_cursor(AwraWidget *w, int px, int py)
{
  (void)py;
  return SPLX(w)->dragging || on_split_handle(w, px) ? BK_CURSOR_EW : -1;
}

static void split_draw(AwraWidget *w, AwraDraw *d)
{
  const int sep = split_sep(w);
  if (sep >= 0) {
    awra_paint_rect(d, (AwraBox){(float)sep, (float)w->y, awra_pxf(1), (float)w->h},
                    &awra_tokens()->separator);
  }
}

AwraWidget *awra_split_view_sidebar(AwraWidget *w)
{
  return SPLX(w)->sidebar;
}

AwraWidget *awra_split_view_content(AwraWidget *w)
{
  return SPLX(w)->content;
}

static AwraClass split_class = {
    .bk = AWRA_BK("AwraSplitView", 0),
    .type = "AwraSplitView",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(SplitExt),
    .measure = split_measure,
    .allocate = split_allocate,
    .draw = split_draw,
    .press = split_press,
    .drag = split_drag,
    .release = split_release,
    .cursor = split_cursor,
};

AwraWidget *awra_split_view_new(void)
{
  split_class.bk.hit = split_hit;
  AwraWidget *w = awra_widget_new(&split_class);
  SPLX(w)->pos = -1;
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

