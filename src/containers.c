/* Awra 2 layout containers: box, grid, flow box, stack, overlay, scrolled
 * window, separator and spacer, with GTK's measuring and allocation rules
 * (GtkBoxLayout's natural-size distribution and expansion, GtkGrid spans,
 * GtkFlowBox lines, GtkScrolledWindow's overlay indicator).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#define OTHER(o) ((o) == AWRA_ORIENTATION_HORIZONTAL ? AWRA_ORIENTATION_VERTICAL : AWRA_ORIENTATION_HORIZONTAL)

/* ======================================================================= */
/* Box */

typedef struct BoxExt {
  int orientation;
  int spacing; /* logical */
  bool homogeneous;
} BoxExt;

static int visible_count(const AwraWidget *w)
{
  int n = 0;
  for (int i = 0; i < w->nkids; i++) {
    n += awra_child_visible(w->kids[i]);
  }
  return n;
}

/* Sizes of the visible children along the box's axis for a total `avail`. */
static void box_distribute(AwraWidget *w, int avail, int for_cross, int *sizes, int n)
{
  const BoxExt *b = AWRA_EXT(w, BoxExt);
  const int o = b->orientation;
  const int sp = awra_px(b->spacing) * (n > 0 ? n - 1 : 0);
  int mins[256], nats[256];
  int *mn = n <= 256 ? mins : (int *)malloc(sizeof(int) * (size_t)n);
  int *nt = n <= 256 ? nats : (int *)malloc(sizeof(int) * (size_t)n);
  int k = 0, sum_min = 0, n_expand = 0;
  /* GtkBoxLayout, in a row: constant-size children first, measured for any
   * height; a sole expanding child whose size depends on the other axis then takes
   * what remains without being measured against its request mode (a
   * wrapping label beside an avatar fills the row, rather than asking for
   * the width its current height would need) */
  int n_inconstant = 0, sole = -1;
  bool constant_same = true;
  for (int i = 0; i < w->nkids; i++) {
    AwraWidget *c = w->kids[i];
    if (!awra_child_visible(c)) {
      continue;
    }
    n_expand += awra_compute_expand(c, o);
    if (o == AWRA_ORIENTATION_HORIZONTAL && for_cross >= 0 && awra_height_for_width(c)) {
      n_inconstant++;
      sole = k;
      mn[k] = nt[k] = -1;
    }
    else {
      /* a column measures each child's height for its width: a flow box or
       * a keyboard that wraps is height-for-width without saying so */
      awra_measure(c, o, o == AWRA_ORIENTATION_HORIZONTAL ? -1 : for_cross, &mn[k], &nt[k]);
      sum_min += mn[k];
      constant_same &= mn[k] == nt[k];
    }
    k++;
  }
  if (n_inconstant > 0) {
    bool shortcut = !b->homogeneous && n_inconstant == 1 && n_expand == 1 && constant_same;
    if (shortcut) {
      k = 0;
      for (int i = 0; i < w->nkids; i++) {
        AwraWidget *c = w->kids[i];
        if (!awra_child_visible(c)) {
          continue;
        }
        if (k == sole) {
          shortcut = awra_compute_expand(c, o);
        }
        k++;
      }
    }
    if (shortcut) {
      mn[sole] = nt[sole] = BK_MAX(0, avail - sp - sum_min);
      sum_min += mn[sole];
    }
    else {
      k = 0;
      for (int i = 0; i < w->nkids; i++) {
        AwraWidget *c = w->kids[i];
        if (!awra_child_visible(c)) {
          continue;
        }
        if (mn[k] < 0) {
          awra_measure(c, o, for_cross, &mn[k], &nt[k]);
          sum_min += mn[k];
        }
        k++;
      }
    }
  }
  int extra = avail - sp;
  if (b->homogeneous) {
    /* gtk_box_layout_allocate: each child the same share, whatever its
     * minimum (a box short of room clips them, as GTK does) */
    const int each = n > 0 ? BK_MAX(0, extra) / n : 0;
    int rem = n > 0 ? BK_MAX(0, extra) - each * n : 0;
    for (int i = 0; i < n; i++) {
      sizes[i] = each + (i < rem ? 1 : 0);
    }
  }
  else {
    extra -= sum_min;
    for (int i = 0; i < n; i++) {
      sizes[i] = mn[i];
    }
    if (extra > 0) {
      extra = awra_distribute(extra, n, sizes, nt);
    }
    if (extra > 0 && n_expand > 0) {
      const int each = extra / n_expand;
      int rem = extra - each * n_expand;
      k = 0;
      for (int i = 0; i < w->nkids; i++) {
        AwraWidget *c = w->kids[i];
        if (!awra_child_visible(c)) {
          continue;
        }
        if (awra_compute_expand(c, o)) {
          sizes[k] += each + (rem > 0 ? 1 : 0);
          rem--;
        }
        k++;
      }
    }
  }
  if (mn != mins) {
    free(mn);
  }
  if (nt != nats) {
    free(nt);
  }
}

static void box_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  const BoxExt *b = AWRA_EXT(w, BoxExt);
  const int n = visible_count(w);
  if (n == 0) {
    *r_min = *r_nat = 0;
    return;
  }
  const int sp = awra_px(b->spacing) * (n - 1);
  if (o == b->orientation) {
    int smin = 0, snat = 0, mmin = 0, mnat = 0;
    for (int i = 0; i < w->nkids; i++) {
      AwraWidget *c = w->kids[i];
      if (!awra_child_visible(c)) {
        continue;
      }
      int cm, cn;
      awra_measure(c, o, for_size, &cm, &cn);
      smin += cm;
      snat += cn;
      mmin = BK_MAX(mmin, cm);
      mnat = BK_MAX(mnat, cn);
    }
    if (b->homogeneous) {
      *r_min = mmin * n + sp;
      *r_nat = mnat * n + sp;
    }
    else {
      *r_min = smin + sp;
      *r_nat = snat + sp;
    }
    return;
  }
  /* across: each child measured for the size it would get along the axis */
  int stack[256];
  int *sizes = n <= 256 ? stack : (int *)malloc(sizeof(int) * (size_t)n);
  if (for_size >= 0) {
    box_distribute(w, for_size, -1, sizes, n);
  }
  int mx_min = 0, mx_nat = 0, k = 0;
  for (int i = 0; i < w->nkids; i++) {
    AwraWidget *c = w->kids[i];
    if (!awra_child_visible(c)) {
      continue;
    }
    int cm, cn;
    awra_measure(c, o, for_size >= 0 ? sizes[k] : -1, &cm, &cn);
    mx_min = BK_MAX(mx_min, cm);
    mx_nat = BK_MAX(mx_nat, cn);
    k++;
  }
  if (sizes != stack) {
    free(sizes);
  }
  *r_min = mx_min;
  *r_nat = mx_nat;
}

static void box_allocate(AwraWidget *w)
{
  const BoxExt *b = AWRA_EXT(w, BoxExt);
  const int n = visible_count(w);
  if (n == 0) {
    return;
  }
  const bool horiz = b->orientation == AWRA_ORIENTATION_HORIZONTAL;
  int stack[256];
  int *sizes = n <= 256 ? stack : (int *)malloc(sizeof(int) * (size_t)n);
  box_distribute(w, horiz ? w->w : w->h, horiz ? w->h : w->w, sizes, n);
  const int sp = awra_px(b->spacing);
  int pos = horiz ? w->x : w->y, k = 0;
  for (int i = 0; i < w->nkids; i++) {
    AwraWidget *c = w->kids[i];
    if (!awra_child_visible(c)) {
      continue;
    }
    if (horiz) {
      awra_allocate(c, pos, w->y, sizes[k], w->h);
    }
    else {
      awra_allocate(c, w->x, pos, w->w, sizes[k]);
    }
    pos += sizes[k] + sp;
    k++;
  }
  if (sizes != stack) {
    free(sizes);
  }
}

static bool box_set_double(AwraWidget *w, const char *name, double v)
{
  if (!strcmp(name, "spacing")) {
    awra_box_set_spacing(w, (int)v);
    return true;
  }
  return false;
}

/* A box of linked view buttons (Awra 1's view-switcher role, class
 * "view-switcher"): a quiet well, alpha(fg, .05) with a .045 line, radius 8. */
static void box_draw(AwraWidget *w, AwraDraw *d)
{
  static int bit = -2;
  if (bit == -2) {
    bit = awra_class_bit("view-switcher");
  }
  if (!awra_has_class_bit(w, bit)) {
    return;
  }
  const AwraTokens *t = awra_tokens();
  const AwraRGBA fill = awra_with_alpha(t->foreground, 0.05f);
  const AwraRGBA line = awra_with_alpha(t->foreground, 0.045f);
  awra_paint_rbox(d, awra_box_of(w), awra_pxf(8), &fill, awra_pxf(1), &line);
}

static AwraClass box_class = {
    .bk = AWRA_BK("AwraBox", 0),
    .type = "AwraBox",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(BoxExt),
    .measure = box_measure,
    .allocate = box_allocate,
    .draw = box_draw,
    .set_double = box_set_double,
};

AwraWidget *awra_box_new(AwraOrientation o, int spacing)
{
  AwraWidget *w = awra_widget_new(&box_class);
  BoxExt *b = AWRA_EXT(w, BoxExt);
  b->orientation = o;
  b->spacing = spacing;
  return w;
}

#define BOX_OK(w) (awra_widget_is(w, "AwraBox"))

void awra_box_append(AwraWidget *box, AwraWidget *child)
{
  if (child && child->parent) {
    awra_widget_ref(child);
    awra_container_remove(child->parent, child);
    awra_child_insert(box, child, -1);
    awra_widget_unref(child);
    return;
  }
  awra_child_insert(box, child, -1);
}

void awra_box_prepend(AwraWidget *box, AwraWidget *child)
{
  awra_child_insert(box, child, 0);
}

void awra_box_insert_after(AwraWidget *box, AwraWidget *child, AwraWidget *sibling)
{
  const int i = sibling ? awra_child_index(box, sibling) : -1;
  awra_child_insert(box, child, sibling ? i + 1 : 0);
}

void awra_box_remove(AwraWidget *box, AwraWidget *child)
{
  awra_child_remove(box, child);
}

void awra_box_remove_all(AwraWidget *box)
{
  awra_container_remove_all(box);
}

void awra_box_set_spacing(AwraWidget *box, int spacing)
{
  if (!BOX_OK(box)) {
    return;
  }
  AWRA_EXT(box, BoxExt)->spacing = spacing;
  awra_widget_queue_resize(box);
}

void awra_box_set_homogeneous(AwraWidget *box, bool h)
{
  if (!BOX_OK(box)) {
    return;
  }
  AWRA_EXT(box, BoxExt)->homogeneous = h;
  awra_widget_queue_resize(box);
}

void awra_box_set_orientation(AwraWidget *box, AwraOrientation o)
{
  if (!BOX_OK(box)) {
    return;
  }
  AWRA_EXT(box, BoxExt)->orientation = o;
  awra_widget_queue_resize(box);
}

/* ======================================================================= */
/* Spacer, separator */

static AwraClass spacer_class = {
    .bk = AWRA_BK("AwraSpacer", 0),
    .type = "AwraSpacer",
};

AwraWidget *awra_spacer_new(void)
{
  AwraWidget *w = awra_widget_new(&spacer_class);
  awra_widget_set_hexpand(w, true);
  awra_widget_set_vexpand(w, true);
  return w;
}

typedef struct SepExt {
  int orientation;
} SepExt;

static void sep_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  const int along = AWRA_EXT(w, SepExt)->orientation;
  *r_min = *r_nat = o == along ? 0 : awra_px(1);
}

static void sep_draw(AwraWidget *w, AwraDraw *d)
{
  awra_paint_rect(d, awra_box_of(w), &awra_tokens()->separator);
}

static AwraClass sep_class = {
    .bk = AWRA_BK("AwraSeparator", 0),
    .type = "AwraSeparator",
    .flags = AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(SepExt),
    .measure = sep_measure,
    .draw = sep_draw,
};

AwraWidget *awra_separator_new(AwraOrientation o)
{
  AwraWidget *w = awra_widget_new(&sep_class);
  AWRA_EXT(w, SepExt)->orientation = o;
  return w;
}

/* ======================================================================= */
/* Grid */

typedef struct GridCell {
  short col, row, w, h;
} GridCell;

typedef struct GridExt {
  int row_spacing, col_spacing;
  bool row_homo, col_homo;
} GridExt;

static GridCell *cell_of(AwraWidget *c)
{
  return (GridCell *)awra_widget_get_data(c, "awra-grid-cell");
}

/* The size along the other orientation a child spans, given those lines'
 * sizes (`other`, `n_other` of them): what GtkGridLayout measures it for
 * (height for width); -1 without them. */
static int grid_span_size(AwraWidget *g, const GridCell *c, int o, const int *other, int n_other)
{
  if (!other) {
    return -1;
  }
  const GridExt *ge = AWRA_EXT(g, GridExt);
  const bool rows = o == AWRA_ORIENTATION_HORIZONTAL; /* the other lines are rows */
  const int start = rows ? c->row : c->col, span = rows ? c->h : c->w;
  const int sp = awra_px(rows ? ge->row_spacing : ge->col_spacing);
  int size = 0;
  for (int s = start; s < start + span && s < n_other; s++) {
    size += other[s] + (s > start ? sp : 0);
  }
  return size;
}

/* Line sizes along `o` (columns when horizontal); returns the count.
 * `other`: the sizes of the lines across (columns when measuring rows),
 * children measured for the size they span (NULL: for any). */
static int grid_lines_for(AwraWidget *g, int o, const int *other, int n_other, int **r_min, int **r_nat,
                          bool **r_expand)
{
  int count = 0;
  for (int i = 0; i < g->nkids; i++) {
    GridCell *c = cell_of(g->kids[i]);
    if (c && awra_child_visible(g->kids[i])) {
      count = BK_MAX(count, o == AWRA_ORIENTATION_HORIZONTAL ? c->col + c->w : c->row + c->h);
    }
  }
  int *mn = (int *)calloc((size_t)count + 1, sizeof(int));
  int *nt = (int *)calloc((size_t)count + 1, sizeof(int));
  bool *ex = (bool *)calloc((size_t)count + 1, sizeof(bool));
  const GridExt *ge = AWRA_EXT(g, GridExt);
  const int sp = awra_px(o == AWRA_ORIENTATION_HORIZONTAL ? ge->col_spacing : ge->row_spacing);
  /* grid_request_compute_expand: lines of expanding single-span children
   * expand; a spanning child that expands makes its lines expand only when
   * none of them does already */
  bool *need = (bool *)calloc((size_t)count + 1, sizeof(bool));
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < g->nkids; i++) {
      AwraWidget *k = g->kids[i];
      GridCell *c = cell_of(k);
      if (!c || !awra_child_visible(k)) {
        continue;
      }
      const int start = o == AWRA_ORIENTATION_HORIZONTAL ? c->col : c->row;
      const int span = o == AWRA_ORIENTATION_HORIZONTAL ? c->w : c->h;
      if ((span == 1) != (pass == 0)) {
        continue;
      }
      if (span == 1) {
        if (awra_compute_expand(k, o)) {
          ex[start] = true;
        }
        continue;
      }
      bool has = false;
      for (int s = start; s < start + span; s++) {
        has |= ex[s];
      }
      if (!has && awra_compute_expand(k, o)) {
        for (int s = start; s < start + span; s++) {
          need[s] = true;
        }
      }
    }
  }
  for (int s = 0; s < count; s++) {
    ex[s] |= need[s];
  }
  free(need);
  /* single-span children first (grid_request_non_spanning), then spans
   * grow their lines, the expanding ones if any (grid_request_spanning) */
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < g->nkids; i++) {
      AwraWidget *k = g->kids[i];
      GridCell *c = cell_of(k);
      if (!c || !awra_child_visible(k)) {
        continue;
      }
      const int start = o == AWRA_ORIENTATION_HORIZONTAL ? c->col : c->row;
      const int span = o == AWRA_ORIENTATION_HORIZONTAL ? c->w : c->h;
      if ((span == 1) != (pass == 0)) {
        continue;
      }
      int cm, cn;
      awra_measure(k, o, grid_span_size(g, c, o, other, n_other), &cm, &cn);
      if (span == 1) {
        mn[start] = BK_MAX(mn[start], cm);
        nt[start] = BK_MAX(nt[start], cn);
        continue;
      }
      int have_m = sp * (span - 1), have_n = sp * (span - 1), n_ex = 0;
      for (int s = start; s < start + span; s++) {
        have_m += mn[s];
        have_n += nt[s];
        n_ex += ex[s];
      }
      const bool force = n_ex == 0;
      if (force) {
        n_ex = span;
      }
      for (int which = 0; which < 2; which++) {
        int *line = which ? nt : mn;
        int extra = which ? cn - have_n : cm - have_m;
        if (extra <= 0) {
          continue;
        }
        int left = n_ex;
        for (int s = start; s < start + span; s++) {
          if (force || ex[s]) {
            const int e = extra / left;
            line[s] += e;
            extra -= e;
            left--;
          }
        }
      }
    }
  }
  for (int s = 0; s < count; s++) {
    nt[s] = BK_MAX(nt[s], mn[s]);
  }
  const bool homo = o == AWRA_ORIENTATION_HORIZONTAL ? ge->col_homo : ge->row_homo;
  if (homo) {
    int m = 0, n = 0;
    for (int s = 0; s < count; s++) {
      m = BK_MAX(m, mn[s]);
      n = BK_MAX(n, nt[s]);
    }
    for (int s = 0; s < count; s++) {
      mn[s] = m;
      nt[s] = n;
    }
  }
  *r_min = mn;
  *r_nat = nt;
  *r_expand = ex;
  return count;
}

static int grid_lines(AwraWidget *g, int o, int **r_min, int **r_nat, bool **r_expand)
{
  return grid_lines_for(g, o, NULL, 0, r_min, r_nat, r_expand);
}

static int *grid_sizes_for(AwraWidget *g, int o, int avail, const int *other, int n_other, int *r_n);

static void grid_measure(AwraWidget *g, int o, int for_size, int *r_min, int *r_nat)
{
  int *mn, *nt;
  bool *ex;
  int n;
  if (o == AWRA_ORIENTATION_VERTICAL && for_size >= 0) {
    /* height for width: the columns that width gives, each child's
     * height for the columns it spans (gtk_grid_layout_measure) */
    int nc;
    int *cw = grid_sizes_for(g, AWRA_ORIENTATION_HORIZONTAL, for_size, NULL, 0, &nc);
    n = grid_lines_for(g, o, cw, nc, &mn, &nt, &ex);
    free(cw);
  }
  else {
    n = grid_lines(g, o, &mn, &nt, &ex);
  }
  const GridExt *ge = AWRA_EXT(g, GridExt);
  const int sp = awra_px(o == AWRA_ORIENTATION_HORIZONTAL ? ge->col_spacing : ge->row_spacing);
  int sm = 0, sn = 0;
  for (int s = 0; s < n; s++) {
    sm += mn[s];
    sn += nt[s];
  }
  if (n > 1) {
    sm += sp * (n - 1);
    sn += sp * (n - 1);
  }
  free(mn);
  free(nt);
  free(ex);
  *r_min = sm;
  *r_nat = sn;
}

static int *grid_sizes_for(AwraWidget *g, int o, int avail, const int *other, int n_other, int *r_n)
{
  int *mn, *nt;
  bool *ex;
  const int n = grid_lines_for(g, o, other, n_other, &mn, &nt, &ex);
  const GridExt *ge = AWRA_EXT(g, GridExt);
  const int sp = awra_px(o == AWRA_ORIENTATION_HORIZONTAL ? ge->col_spacing : ge->row_spacing);
  const bool homo = o == AWRA_ORIENTATION_HORIZONTAL ? ge->col_homo : ge->row_homo;
  int extra = avail - sp * (n > 0 ? n - 1 : 0);
  if (homo && n > 0) {
    const int each = BK_MAX(0, extra) / n;
    for (int s = 0; s < n; s++) {
      mn[s] = BK_MAX(mn[s], each);
    }
  }
  else {
    for (int s = 0; s < n; s++) {
      extra -= mn[s];
    }
    if (extra > 0) {
      extra = awra_distribute(extra, n, mn, nt);
    }
    int n_ex = 0;
    for (int s = 0; s < n; s++) {
      n_ex += ex[s];
    }
    if (extra > 0 && n_ex) {
      const int each = extra / n_ex;
      int rem = extra - each * n_ex;
      for (int s = 0; s < n; s++) {
        if (ex[s]) {
          mn[s] += each + (rem-- > 0 ? 1 : 0);
        }
      }
    }
  }
  free(nt);
  free(ex);
  *r_n = n;
  return mn;
}

static void grid_allocate(AwraWidget *g)
{
  int nc, nr;
  int *cw = grid_sizes_for(g, AWRA_ORIENTATION_HORIZONTAL, g->w, NULL, 0, &nc);
  int *rh = grid_sizes_for(g, AWRA_ORIENTATION_VERTICAL, g->h, cw, nc, &nr);
  const GridExt *ge = AWRA_EXT(g, GridExt);
  const int cs = awra_px(ge->col_spacing), rs = awra_px(ge->row_spacing);
  for (int i = 0; i < g->nkids; i++) {
    AwraWidget *k = g->kids[i];
    GridCell *c = cell_of(k);
    if (!c || !awra_child_visible(k)) {
      continue;
    }
    int x = g->x, y = g->y, w = 0, h = 0;
    for (int s = 0; s < c->col; s++) {
      x += cw[s] + cs;
    }
    for (int s = 0; s < c->row; s++) {
      y += rh[s] + rs;
    }
    for (int s = c->col; s < c->col + c->w && s < nc; s++) {
      w += cw[s] + (s > c->col ? cs : 0);
    }
    for (int s = c->row; s < c->row + c->h && s < nr; s++) {
      h += rh[s] + (s > c->row ? rs : 0);
    }
    awra_allocate(k, x, y, w, h);
  }
  free(cw);
  free(rh);
}

static AwraClass grid_class = {
    .bk = AWRA_BK("AwraGrid", 0),
    .type = "AwraGrid",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(GridExt),
    .measure = grid_measure,
    .allocate = grid_allocate,
};

AwraWidget *awra_grid_new(void)
{
  return awra_widget_new(&grid_class);
}

void awra_grid_attach(AwraWidget *grid, AwraWidget *child, int column, int row, int width,
                      int height)
{
  GridCell *c = (GridCell *)calloc(1, sizeof(GridCell));
  c->col = (short)BK_MAX(0, column);
  c->row = (short)BK_MAX(0, row);
  c->w = (short)BK_MAX(1, width);
  c->h = (short)BK_MAX(1, height);
  awra_widget_set_data(child, "awra-grid-cell", c, free);
  awra_child_insert(grid, child, -1);
}

void awra_grid_set_row_spacing(AwraWidget *g, int s)
{
  AWRA_EXT(g, GridExt)->row_spacing = s;
  awra_widget_queue_resize(g);
}

void awra_grid_set_column_spacing(AwraWidget *g, int s)
{
  AWRA_EXT(g, GridExt)->col_spacing = s;
  awra_widget_queue_resize(g);
}

void awra_grid_set_homogeneous(AwraWidget *g, bool rows, bool columns)
{
  AWRA_EXT(g, GridExt)->row_homo = rows;
  AWRA_EXT(g, GridExt)->col_homo = columns;
  awra_widget_queue_resize(g);
}

/* ======================================================================= */
/* Flow box */

typedef struct FlowExt {
  int row_spacing, col_spacing;
  int min_per_line, max_per_line;
  bool homogeneous;
  AwraSelectionMode mode;
  AwraWidget *selected;
  bool (*filter)(AwraWidget *child, void *data);
  void *filter_data;
  AwraDestroyNotify filter_destroy;
  int child_pad; /* logical px; -1: the theme's */
} FlowExt;

static bool flow_shown(AwraWidget *f, AwraWidget *c)
{
  const FlowExt *x = AWRA_EXT(f, FlowExt);
  if (!awra_child_visible(c)) {
    return false;
  }
  return !x->filter || x->filter(c, x->filter_data);
}

/* GtkFlowBoxChild's padding in GTK's default theme (flowboxchild { padding:
 * 3px }); none in a flat list (awra.css .awra-role-flat-list) or where set */
static int flow_pad(AwraWidget *f)
{
  static int flat = -2;
  if (flat == -2) {
    flat = awra_class_bit("flat-list");
  }
  const int set = AWRA_EXT(f, FlowExt)->child_pad;
  if (set >= 0) {
    return awra_px(set);
  }
  return flat >= 0 && awra_has_class_bit(f, flat) ? 0 : awra_px(3);
}
#define FLOW_PAD flow_pad(f)
#define FLOW_MAX_ITEMS 64

/* The shown children with their widths (padding included). */
typedef struct FlowItems {
  AwraWidget *c[FLOW_MAX_ITEMS * 4];
  int min[FLOW_MAX_ITEMS * 4], nat[FLOW_MAX_ITEMS * 4];
  int n, max_min, max_nat;
  int pad; /* each child's padding */
} FlowItems;

static void flow_items(AwraWidget *f, FlowItems *it)
{
  it->n = it->max_min = it->max_nat = 0;
  it->pad = FLOW_PAD;
  for (int i = 0; i < f->nkids && it->n < FLOW_MAX_ITEMS * 4; i++) {
    AwraWidget *c = f->kids[i];
    if (!flow_shown(f, c)) {
      continue;
    }
    int cm, cn;
    awra_measure(c, AWRA_ORIENTATION_HORIZONTAL, -1, &cm, &cn);
    it->c[it->n] = c;
    it->min[it->n] = cm + 2 * FLOW_PAD;
    it->nat[it->n] = cn + 2 * FLOW_PAD;
    it->max_min = BK_MAX(it->max_min, it->min[it->n]);
    it->max_nat = BK_MAX(it->max_nat, it->nat[it->n]);
    it->n++;
  }
}

static int flow_max_items(const FlowExt *x)
{
  return x->max_per_line > 0 ? x->max_per_line : 7; /* GTK's default */
}

/* gather_aligned_item_requests: column sizes for a line length; returns
 * the natural line size */
static int flow_gather(const FlowItems *it, int len, int cs, int *mins, int *nats)
{
  for (int k = 0; k < len; k++) {
    mins[k] = nats[k] = 0;
  }
  for (int i = 0; i < it->n; i++) {
    const int k = i % len;
    mins[k] = BK_MAX(mins[k], it->min[i]);
    nats[k] = BK_MAX(nats[k], it->nat[i]);
  }
  int line = (len - 1) * cs;
  for (int k = 0; k < len; k++) {
    line += nats[k];
  }
  return line;
}

/* The flow for a width (gtk_flow_box_size_allocate / measure): items per
 * line and each column's width. */
static int flow_plan(AwraWidget *f, const FlowItems *it, int avail, int *cols)
{
  const FlowExt *x = AWRA_EXT(f, FlowExt);
  const int cs = awra_px(x->col_spacing);
  const int min_items = BK_MAX(1, x->min_per_line);
  const int max_items = BK_MIN(flow_max_items(x), FLOW_MAX_ITEMS);
  if (it->n == 0 || it->max_nat <= 0) {
    return 1;
  }
  int len = avail / (it->max_nat + cs);
  if (len * cs + (len + 1) * it->max_nat <= avail) {
    len++;
  }
  len = BK_MAX(min_items, len);
  len = BK_MIN(len, max_items);
  len = BK_MAX(1, len);
  if (x->homogeneous) {
    const int size = (avail - (len - 1) * cs) / len;
    const int extra = avail - (len - 1) * cs - size * len;
    for (int k = 0; k < len; k++) {
      cols[k] = size + extra / len + (k < extra % len ? 1 : 0);
    }
    return len;
  }
  /* fit_aligned_item_requests */
  int mins[FLOW_MAX_ITEMS], nats[FLOW_MAX_ITEMS];
  int tmin[FLOW_MAX_ITEMS], tnat[FLOW_MAX_ITEMS];
  int line = flow_gather(it, len, cs, mins, nats);
  for (int t = len + 1; line < avail && t <= FLOW_MAX_ITEMS; t++) {
    line = flow_gather(it, t, cs, tmin, tnat);
    if (line <= avail && max_items >= t) {
      len = t;
      memcpy(mins, tmin, sizeof(int) * (size_t)t);
      memcpy(nats, tnat, sizeof(int) * (size_t)t);
    }
    else {
      break;
    }
  }
  int rest = avail - (len - 1) * cs;
  for (int k = 0; k < len; k++) {
    rest -= mins[k];
  }
  int extra = 0;
  if (rest > 0) {
    extra = awra_distribute(rest, len, mins, nats);
  }
  /* halign fill (the default): the rest is shared by the items */
  for (int k = 0; k < len; k++) {
    cols[k] = mins[k] + extra / len + (k < extra % len ? 1 : 0);
  }
  return len;
}

/* Height of each line for the columns. */
static int flow_line_heights(const FlowItems *it, int len, const int *cols, int *mins, int *nats)
{
  const int lines = (it->n + len - 1) / len;
  for (int l = 0; l < lines; l++) {
    mins[l] = nats[l] = 0;
  }
  for (int i = 0; i < it->n; i++) {
    int cm, cn;
    awra_measure(it->c[i], AWRA_ORIENTATION_VERTICAL, BK_MAX(0, cols[i % len] - 2 * it->pad), &cm, &cn);
    mins[i / len] = BK_MAX(mins[i / len], cm + 2 * it->pad);
    nats[i / len] = BK_MAX(nats[i / len], cn + 2 * it->pad);
  }
  return lines;
}

static void flow_measure(AwraWidget *f, int o, int for_size, int *r_min, int *r_nat)
{
  const FlowExt *x = AWRA_EXT(f, FlowExt);
  const int cs = awra_px(x->col_spacing), rs = awra_px(x->row_spacing);
  FlowItems *it = (FlowItems *)malloc(sizeof(FlowItems));
  flow_items(f, it);
  *r_min = *r_nat = 0;
  if (it->n == 0) {
    free(it);
    return;
  }
  const int min_items = BK_MAX(1, x->min_per_line);
  const int nat_items = BK_MAX(min_items, flow_max_items(x));
  int mins[FLOW_MAX_ITEMS], nats[FLOW_MAX_ITEMS];
  /* widths */
  int min_w, nat_w;
  if (x->homogeneous) {
    min_w = it->max_min * min_items + (min_items - 1) * cs;
    nat_w = it->max_nat * nat_items + (nat_items - 1) * cs;
  }
  else {
    flow_gather(it, BK_MIN(min_items, FLOW_MAX_ITEMS), cs, mins, nats);
    min_w = (BK_MIN(min_items, FLOW_MAX_ITEMS) - 1) * cs;
    for (int k = 0; k < BK_MIN(min_items, FLOW_MAX_ITEMS); k++) {
      min_w += mins[k];
    }
    nat_w = flow_gather(it, BK_MIN(nat_items, FLOW_MAX_ITEMS), cs, mins, nats);
  }
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    /* GTK counts the empty columns of lines longer than the children */
    *r_min = min_w;
    *r_nat = nat_w;
    free(it);
    return;
  }
  /* height for a width (the natural width when none is given) */
  const int avail = BK_MAX(for_size >= 0 ? for_size : nat_w, min_w);
  int cols[FLOW_MAX_ITEMS];
  const int len = flow_plan(f, it, avail, cols);
  const int lines = flow_line_heights(it, len, cols, mins, nats);
  for (int l = 0; l < lines; l++) {
    *r_min += mins[l] + (l ? rs : 0);
    *r_nat += nats[l] + (l ? rs : 0);
  }
  free(it);
}

static void flow_allocate(AwraWidget *f)
{
  const FlowExt *x = AWRA_EXT(f, FlowExt);
  const int cs = awra_px(x->col_spacing), rs = awra_px(x->row_spacing);
  for (int i = 0; i < f->nkids; i++) {
    AwraWidget *c = f->kids[i];
    if (awra_child_visible(c) && !flow_shown(f, c)) {
      c->w = c->h = 0; /* filtered out: nothing to draw or hit */
    }
  }
  FlowItems *it = (FlowItems *)malloc(sizeof(FlowItems));
  flow_items(f, it);
  if (it->n == 0) {
    free(it);
    return;
  }
  int cols[FLOW_MAX_ITEMS], mins[FLOW_MAX_ITEMS * 4], nats[FLOW_MAX_ITEMS * 4];
  const int len = flow_plan(f, it, f->w, cols);
  const int lines = flow_line_heights(it, len, cols, mins, nats);
  /* lines: their minimum, then the rest up to natural (valign fill: the
   * leftover is shared) */
  int rest = f->h - (lines - 1) * rs;
  for (int l = 0; l < lines; l++) {
    rest -= mins[l];
  }
  int extra = rest > 0 ? awra_distribute(rest, lines, mins, nats) : 0;
  int y = f->y;
  for (int l = 0; l < lines; l++) {
    const int lh = mins[l] + extra / lines + (l < extra % lines ? 1 : 0);
    int xx = f->x;
    for (int k = 0; k < len && l * len + k < it->n; k++) {
      awra_allocate(it->c[l * len + k], xx + FLOW_PAD, y + FLOW_PAD, BK_MAX(0, cols[k] - 2 * FLOW_PAD),
                    BK_MAX(0, lh - 2 * FLOW_PAD));
      xx += cols[k] + cs;
    }
    y += lh + rs;
  }
  free(it);
}


static void flow_finalize(AwraWidget *f)
{
  FlowExt *x = AWRA_EXT(f, FlowExt);
  if (x->filter_destroy) {
    x->filter_destroy(x->filter_data);
  }
}

static AwraClass flow_class = {
    .bk = AWRA_BK("AwraFlowBox", 0),
    .type = "AwraFlowBox",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(FlowExt),
    .finalize = flow_finalize,
    .measure = flow_measure,
    .allocate = flow_allocate,
};


AwraWidget *awra_flow_box_new(void)
{
  AwraWidget *w = awra_widget_new(&flow_class);
  FlowExt *x = AWRA_EXT(w, FlowExt);
  x->min_per_line = 1;
  x->max_per_line = 7; /* GtkFlowBox's default */
  x->mode = AWRA_SELECTION_SINGLE;
  x->child_pad = -1;
  return w;
}

void awra_flow_box_append(AwraWidget *f, AwraWidget *child)
{
  awra_child_insert(f, child, -1);
}

void awra_flow_box_insert(AwraWidget *f, AwraWidget *child, int position)
{
  awra_child_insert(f, child, position);
}

void awra_flow_box_set_spacing(AwraWidget *f, int row_spacing, int column_spacing)
{
  FlowExt *x = AWRA_EXT(f, FlowExt);
  x->row_spacing = row_spacing;
  x->col_spacing = column_spacing;
  awra_widget_queue_resize(f);
}

void awra_flow_box_set_columns(AwraWidget *f, int min_per_line, int max_per_line)
{
  FlowExt *x = AWRA_EXT(f, FlowExt);
  x->min_per_line = min_per_line;
  x->max_per_line = max_per_line;
  awra_widget_queue_resize(f);
}

void awra_flow_box_set_homogeneous(AwraWidget *f, bool h)
{
  AWRA_EXT(f, FlowExt)->homogeneous = h;
  awra_widget_queue_resize(f);
}

void awra_flow_box_set_selection_mode(AwraWidget *f, AwraSelectionMode mode)
{
  AWRA_EXT(f, FlowExt)->mode = mode;
}

void awra_flow_box_select(AwraWidget *f, AwraWidget *child)
{
  AWRA_EXT(f, FlowExt)->selected = child;
  awra_emit_index(f, AWRA_SIGNAL_SELECTED, awra_child_index(f, child));
}

AwraWidget *awra_flow_box_selected(AwraWidget *f)
{
  return AWRA_EXT(f, FlowExt)->selected;
}

void awra_flow_box_set_filter(AwraWidget *f, bool (*keep)(AwraWidget *child, void *data),
                              void *data, AwraDestroyNotify destroy)
{
  FlowExt *x = AWRA_EXT(f, FlowExt);
  if (x->filter_destroy) {
    x->filter_destroy(x->filter_data);
  }
  x->filter = keep;
  x->filter_data = data;
  x->filter_destroy = destroy;
  awra_flow_box_invalidate_filter(f);
}

void awra_flow_box_invalidate_filter(AwraWidget *f)
{
  /* hidden children: zero-size rects are neither drawn nor hit */
  FlowExt *x = AWRA_EXT(f, FlowExt);
  for (int i = 0; i < f->nkids; i++) {
    AwraWidget *c = f->kids[i];
    const bool keep = !x->filter || x->filter(c, x->filter_data);
    if (!keep) {
      awra_invalidate(c);
      c->w = c->h = 0;
    }
  }
  awra_widget_queue_resize(f);
  awra_invalidate(f);
}

/* ======================================================================= */
/* Stack */

typedef struct StackExt {
  AwraWidget *visible, *previous;
  AwraTransition transition;
  unsigned ms;
  float t; /* transition progress 0..1 */
  bool hhomo, vhomo;
} StackExt;

static char *stack_name(AwraWidget *c)
{
  return (char *)awra_widget_get_data(c, "awra-stack-name");
}

static void stack_measure(AwraWidget *s, int o, int for_size, int *r_min, int *r_nat)
{
  const StackExt *x = AWRA_EXT(s, StackExt);
  const bool homo = o == AWRA_ORIENTATION_HORIZONTAL ? x->hhomo : x->vhomo;
  int mn = 0, nt = 0;
  for (int i = 0; i < s->nkids; i++) {
    AwraWidget *c = s->kids[i];
    if (!homo && c != x->visible) {
      continue;
    }
    int cm, cn;
    /* pages the stack hides still count when homogeneous (GtkStack measures
     * every visible child, shown or not) */
    const bool page_hidden = !(c->flags & NF_VISIBLE) && awra_is_widget(c) &&
                             awra_aux(c)->child_hidden && !awra_aux(c)->user_hidden;
    if (page_hidden) {
      c->flags |= NF_VISIBLE;
    }
    awra_measure(c, o, for_size, &cm, &cn);
    if (page_hidden) {
      c->flags &= ~NF_VISIBLE;
    }
    mn = BK_MAX(mn, cm);
    nt = BK_MAX(nt, cn);
  }
  *r_min = mn;
  *r_nat = nt;
}

/* GtkStack's adjust_child_allocation: a page is given at least its minimum
 * size, centred (fill, center) or at its end where the stack is smaller. */
static void stack_adjust(AwraWidget *s, AwraWidget *c, int *x, int *y, int *w, int *h)
{
  int mn, nt;
  awra_measure(c, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nt);
  const int width = BK_MAX(*w, mn);
  awra_measure(c, AWRA_ORIENTATION_VERTICAL, width, &mn, &nt);
  const int height = BK_MAX(*h, mn);
  const AwraAux *a = awra_aux(c);
  if (width > *w) {
    /* in the left-to-right frame: awra_allocate mirrors it right to left,
     * where a start-aligned page moves as GTK moves it (and half of an odd
     * excess rounds the other way) */
    if (a->halign == AWRA_ALIGN_CENTER || a->halign == AWRA_ALIGN_FILL) {
      *x -= awra_widget_rtl(s) ? (width - *w + 1) / 2 : (width - *w) / 2;
    }
    else if (a->halign == AWRA_ALIGN_END) {
      *x -= width - *w;
    }
    *w = width;
  }
  if (height > *h) {
    if (a->valign == AWRA_ALIGN_CENTER || a->valign == AWRA_ALIGN_FILL) {
      *y -= (height - *h) / 2;
    }
    else if (a->valign == AWRA_ALIGN_END) {
      *y -= height - *h;
    }
    *h = height;
  }
}

static void stack_allocate(AwraWidget *s)
{
  StackExt *x = AWRA_EXT(s, StackExt);
  for (int i = 0; i < s->nkids; i++) {
    AwraWidget *c = s->kids[i];
    if (c == x->visible || c == x->previous) {
      int ox = 0;
      if (x->previous && x->transition == AWRA_TRANSITION_SLIDE_LEFT_RIGHT) {
        const int dir = awra_child_index(s, x->visible) > awra_child_index(s, x->previous) ? 1 : -1;
        ox = c == x->visible ? (int)(dir * s->w * (1.0f - x->t)) : (int)(-dir * s->w * x->t);
      }
      int cx = s->x + ox, cy = s->y, cw = s->w, ch = s->h;
      if (awra_is_widget(c)) {
        stack_adjust(s, c, &cx, &cy, &cw, &ch);
      }
      awra_allocate(c, cx, cy, cw, ch);
    }
  }
}

static void stack_draw_tree(BKNode *n);
static void stack_remove(AwraWidget *s, AwraWidget *child);

static AwraClass stack_class = {
    .bk = AWRA_BK("AwraStack", 0),
    .type = "AwraStack",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_CLIP | AWRA_CLASS_TAKES_UNDERSIZE,
    .ext_size = sizeof(StackExt),
    .measure = stack_measure,
    .allocate = stack_allocate,
    .remove = stack_remove,
};

static void stack_sync_visibility(AwraWidget *s)
{
  StackExt *x = AWRA_EXT(s, StackExt);
  for (int i = 0; i < s->nkids; i++) {
    AwraWidget *c = s->kids[i];
    const bool show = c == x->visible || c == x->previous;
    awra_widget_set_child_visible(c, show);
  }
}

AwraWidget *awra_stack_new(void)
{
  AwraWidget *w = awra_widget_new(&stack_class);
  StackExt *x = AWRA_EXT(w, StackExt);
  x->hhomo = x->vhomo = true;
  x->t = 1.0f;
  (void)stack_draw_tree;
  return w;
}

static void stack_draw_tree(BKNode *n)
{
  awra_bk_draw_tree(n);
}

/* A page going away ends a transition it is part of, and the visible one
 * hands over to a remaining page (GtkStack shows the next one). */
static void stack_remove(AwraWidget *s, AwraWidget *child)
{
  StackExt *x = AWRA_EXT(s, StackExt);
  if (x->previous && (child == x->previous || child == x->visible)) {
    awra_anim_stop_for(s);
    awra_widget_set_opacity(x->previous, 1.0);
    awra_widget_set_opacity(x->visible, 1.0);
    x->previous = NULL;
    x->t = 1.0f;
  }
  if (child == x->visible) {
    x->visible = NULL;
    for (int i = 0; i < s->nkids && !x->visible; i++) {
      if (s->kids[i] != child) {
        x->visible = s->kids[i];
      }
    }
  }
  awra_child_remove(s, child);
  stack_sync_visibility(s);
  awra_widget_queue_resize(s);
}

void awra_stack_add_named(AwraWidget *s, AwraWidget *child, const char *name)
{
  if (name) {
    awra_widget_set_data(child, "awra-stack-name", awra_strdup(name), free);
  }
  StackExt *x = AWRA_EXT(s, StackExt);
  awra_child_insert(s, child, -1);
  if (!x->visible) {
    x->visible = child;
  }
  stack_sync_visibility(s);
}

static void stack_anim(AwraWidget *s, double t, void *data)
{
  StackExt *x = AWRA_EXT(s, StackExt);
  x->t = (float)t;
  if (x->previous && x->transition == AWRA_TRANSITION_CROSSFADE) {
    awra_widget_set_opacity(x->previous, 1.0 - t);
    awra_widget_set_opacity(x->visible, t);
  }
  if (t >= 1.0) {
    if (x->previous) {
      awra_widget_set_opacity(x->previous, 1.0);
      awra_widget_remove_class(x->previous, "stack-leaving");
    }
    awra_widget_set_opacity(x->visible, 1.0);
    x->previous = NULL;
    stack_sync_visibility(s);
  }
  awra_queue_allocate(s);
  awra_invalidate(s);
}

void awra_stack_set_visible_child(AwraWidget *s, AwraWidget *child)
{
  StackExt *x = AWRA_EXT(s, StackExt);
  if (!child || child->parent != s || x->visible == child) {
    return;
  }
  awra_anim_stop_for(s);
  if (x->previous) {
    awra_widget_set_opacity(x->previous, 1.0);
    awra_widget_remove_class(x->previous, "stack-leaving");
  }
  x->previous = x->transition != AWRA_TRANSITION_NONE && awra_widget_mapped(s) ? x->visible : NULL;
  if (x->previous) {
    /* GtkStack unmaps the page it leaves and draws a snapshot of it: it
     * takes no focus while it fades out */
    awra_widget_add_class(x->previous, "stack-leaving");
  }
  x->visible = child;
  stack_sync_visibility(s);
  awra_widget_queue_resize(s);
  awra_invalidate(s);
  if (x->previous) {
    awra_anim_start(s, x->ms ? x->ms : awra_motion_duration(AWRA_MOTION_NORMAL), stack_anim, NULL,
                    NULL);
  }
  else {
    x->t = 1.0f;
  }
  awra_emit_index(s, AWRA_SIGNAL_SELECTED, awra_child_index(s, child));
}

AwraWidget *awra_stack_child_by_name(AwraWidget *s, const char *name)
{
  for (int i = 0; i < s->nkids && name; i++) {
    const char *n = stack_name(s->kids[i]);
    if (n && !strcmp(n, name)) {
      return s->kids[i];
    }
  }
  return NULL;
}

void awra_stack_set_visible_child_name(AwraWidget *s, const char *name)
{
  awra_stack_set_visible_child(s, awra_stack_child_by_name(s, name));
}

AwraWidget *awra_stack_visible_child(AwraWidget *s)
{
  return AWRA_EXT(s, StackExt)->visible;
}

const char *awra_stack_visible_child_name(AwraWidget *s)
{
  AwraWidget *v = AWRA_EXT(s, StackExt)->visible;
  return v ? stack_name(v) : NULL;
}

void awra_stack_set_transition(AwraWidget *s, AwraTransition t, unsigned ms)
{
  StackExt *x = AWRA_EXT(s, StackExt);
  x->transition = t;
  x->ms = ms;
}

void awra_stack_set_hhomogeneous(AwraWidget *s, bool h)
{
  AWRA_EXT(s, StackExt)->hhomo = h;
  awra_widget_queue_resize(s);
}

void awra_stack_set_vhomogeneous(AwraWidget *s, bool h)
{
  AWRA_EXT(s, StackExt)->vhomo = h;
  awra_widget_queue_resize(s);
}

/* ======================================================================= */
/* Overlay */

typedef struct OverlayExt {
  AwraWidget *child;
  AwraOverlayPositionFunc position;
  void *position_data;
  AwraDestroyNotify position_destroy;
} OverlayExt;

static void overlay_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  OverlayExt *x = AWRA_EXT(w, OverlayExt);
  if (x->child) {
    awra_measure(x->child, o, for_size, r_min, r_nat);
  }
  else {
    *r_min = *r_nat = 0;
  }
}

static void overlay_allocate(AwraWidget *w)
{
  OverlayExt *x = AWRA_EXT(w, OverlayExt);
  for (int i = 0; i < w->nkids; i++) {
    AwraWidget *c = w->kids[i];
    int rx, ry, rw, rh;
    /* get-child-position: the application places an overlay child itself */
    if (c != x->child && x->position && x->position(w, c, &rx, &ry, &rw, &rh, x->position_data)) {
      awra_allocate(c, w->x + awra_px(rx), w->y + awra_px(ry), awra_px(rw), awra_px(rh));
      continue;
    }
    AwraAux *a = awra_aux(c);
    if (c != x->child && (a->halign == AWRA_ALIGN_CENTER || a->valign == AWRA_ALIGN_CENTER) &&
        awra_child_visible(c))
    {
      /* GtkOverlay's own get-child-position: the preferred size within
       * the overlay, centred as width / 2 - size / 2 in whole logical px
       * (half a pixel lower and further right than gtk_widget's
       * (size - natural) / 2) */
      const int k = BK_MAX(1, (int)awra_scale());
      int min_w, nat_w, min_h, nat_h, unused;
      awra_measure(c, AWRA_ORIENTATION_HORIZONTAL, -1, &min_w, &nat_w);
      awra_measure(c, AWRA_ORIENTATION_VERTICAL, min_w, &min_h, &unused);
      awra_measure(c, AWRA_ORIENTATION_VERTICAL, nat_w, &unused, &nat_h);
      int cw = BK_MAX(min_w, BK_MIN(w->w, nat_w)), ch = BK_MAX(min_h, BK_MIN(w->h, nat_h));
      int cx = 0, cy = 0;
      if (a->halign == AWRA_ALIGN_CENTER) {
        cx = (w->w / k / 2 - cw / k / 2) * k;
      }
      else if (a->halign == AWRA_ALIGN_FILL) {
        cw = BK_MAX(cw, w->w);
      }
      else {
        cx = 0;
        cw = w->w; /* start / end: aligned by awra_allocate */
      }
      if (a->valign == AWRA_ALIGN_CENTER) {
        cy = (w->h / k / 2 - ch / k / 2) * k;
      }
      else if (a->valign == AWRA_ALIGN_FILL) {
        ch = BK_MAX(ch, w->h);
      }
      else {
        cy = 0;
        ch = w->h;
      }
      if (awra_widget_rtl(w) && a->halign == AWRA_ALIGN_CENTER) {
        cx = w->w - cx - cw; /* mirrored back by awra_allocate */
      }
      awra_allocate(c, w->x + cx, w->y + cy, cw, ch);
      continue;
    }
    awra_allocate(c, w->x, w->y, w->w, w->h);
  }
}

static void overlay_finalize(AwraWidget *w)
{
  OverlayExt *x = AWRA_EXT(w, OverlayExt);
  if (x->position_destroy) {
    x->position_destroy(x->position_data);
  }
}

void awra_overlay_set_position_func(AwraWidget *w, AwraOverlayPositionFunc fn, void *data,
                                    AwraDestroyNotify destroy)
{
  OverlayExt *x = AWRA_EXT(w, OverlayExt);
  if (x->position_destroy) {
    x->position_destroy(x->position_data);
  }
  x->position = fn;
  x->position_data = data;
  x->position_destroy = destroy;
  awra_queue_allocate(w);
}

static void overlay_remove(AwraWidget *w, AwraWidget *child)
{
  OverlayExt *x = AWRA_EXT(w, OverlayExt);
  if (x->child == child) {
    x->child = NULL;
  }
  awra_child_remove(w, child);
}

static AwraWidget *overlay_bin_child(AwraWidget *w)
{
  return AWRA_EXT(w, OverlayExt)->child;
}

static AwraClass overlay_class = {
    .bk = AWRA_BK("AwraOverlay", 0),
    .type = "AwraOverlay",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(OverlayExt),
    .measure = overlay_measure,
    .allocate = overlay_allocate,
    .remove = overlay_remove,
    .finalize = overlay_finalize,
    .bin_child = overlay_bin_child,
};

AwraWidget *awra_overlay_new(void)
{
  return awra_widget_new(&overlay_class);
}

void awra_overlay_set_child(AwraWidget *w, AwraWidget *child)
{
  OverlayExt *x = AWRA_EXT(w, OverlayExt);
  if (x->child == child) {
    return;
  }
  if (x->child) {
    awra_child_remove(w, x->child);
  }
  x->child = child;
  if (child) {
    /* moved from another parent (GTK unparents first): that parent forgets it */
    awra_widget_ref(child);
    if (child->parent) {
      awra_container_remove(child->parent, child);
    }
    awra_child_insert(w, child, 0); /* below the overlays */
    awra_widget_unref(child);
  }
}

void awra_overlay_add_overlay(AwraWidget *w, AwraWidget *child)
{
  /* overlays default to their natural size, aligned like GtkOverlay */
  awra_child_insert(w, child, -1);
}

/* ======================================================================= */
/* Scrolled window */

typedef struct ScrollExt {
  AwraWidget *child;
  AwraPolicy hpolicy, vpolicy;
  bool whole_steps; /* GtkListBase's adjustments: whole px after each step */
  bool prop_w, prop_h;
  int max_h, min_h; /* logical, -1 none */
  int max_w, min_w; /* logical, 0 none (GtkScrolledWindow's content widths) */
  double vvalue, hvalue; /* raster px */
  int content_w, content_h;
  int view_w;       /* the width hvalue was kept for (right to left) */
  bool shifting;    /* a list inside keeps its rows in place, not the user */
  AwraWidget *vbar; /* the overlay scrollbar (scrollbar.c), its last child */
  /* edge fades (logical px; 0 none) and, while a frame draws, what lies
   * under each faded band: top rows then bottom rows */
  int edge_fade[2];
  int fade_h[2], fade_skip;
  uint32_t *fade_under;
  size_t fade_cap;
} ScrollExt;

#define SX(w) AWRA_EXT(w, ScrollExt)

static void scroll_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  /* gtk_scrolled_window_measure */
  ScrollExt *x = SX(w);
  const bool horizontal = o == AWRA_ORIENTATION_HORIZONTAL;
  const bool prop = horizontal ? x->prop_w : x->prop_h;
  const int policy = horizontal ? x->hpolicy : x->vpolicy;
  int mn = 0, nat = 0;
  if (x->child && (prop || policy == AWRA_POLICY_NEVER)) {
    const int other = horizontal ? x->vpolicy : x->hpolicy;
    int cm = 0, cn = 0;
    awra_measure(x->child, o, for_size >= 0 && other == AWRA_POLICY_NEVER ? for_size : -1, &cm, &cn);
    if (prop) {
      nat = cn;
    }
    if (policy == AWRA_POLICY_NEVER) {
      mn = cm;
    }
    else {
      const int lmin = horizontal ? x->min_w : x->min_h, lmax = horizontal ? x->max_w : x->max_h;
      const int lo = lmin > 0 ? awra_px(lmin) : 0;
      const int hi = lmax > 0 ? awra_px(lmax) : INT32_MAX;
      mn = BK_CLAMP(mn, lo, hi);
      nat = BK_CLAMP(nat, lo, hi);
    }
  }
  else if ((horizontal ? x->min_w : x->min_h) > 0) {
    mn = awra_px(horizontal ? x->min_w : x->min_h);
  }
  *r_min = mn;
  *r_nat = BK_MAX(mn, nat);
}

static void scroll_clamp(AwraWidget *w)
{
  ScrollExt *x = SX(w);
  const double maxv = BK_MAX(0, x->content_h - w->h);
  const double maxh = BK_MAX(0, x->content_w - w->w);
  x->vvalue = x->vvalue < 0 ? 0 : x->vvalue > maxv ? maxv : x->vvalue;
  x->hvalue = x->hvalue < 0 ? 0 : x->hvalue > maxh ? maxh : x->hvalue;
}

void awra_scrollbar_allocate_internal(AwraWidget *bar);
void awra_scrollbar_value_changed_internal(AwraWidget *bar);
void awra_scrollbar_motion_internal(AwraWidget *bar, int px, int py, bool on_bar, bool buttons);
void awra_scrollbar_leave_internal(AwraWidget *bar);
void awra_scrollbar_set_style_internal(AwraWidget *bar, int style);
AwraWidget *awra_scrollbar_new_internal(AwraWidget *sw);
void awra_scrolled_forget_motion_internal(AwraWidget *sw);

static void scroll_place(AwraWidget *w);

static void scroll_allocate(AwraWidget *w)
{
  scroll_place(w);
  if (SX(w)->vbar) {
    awra_scrollbar_allocate_internal(SX(w)->vbar);
  }
}

static void scroll_place(AwraWidget *w)
{
  ScrollExt *x = SX(w);
  if (!x->child) {
    return;
  }
  int mn, nat;
  int cw = w->w;
  if (x->hpolicy != AWRA_POLICY_NEVER) {
    awra_measure(x->child, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nat);
    cw = BK_MAX(w->w, mn);
  }
  int ch = w->h;
  if (x->vpolicy != AWRA_POLICY_NEVER) {
    awra_measure(x->child, AWRA_ORIENTATION_VERTICAL, cw, &mn, &nat);
    ch = BK_MAX(w->h, mn);
  }
  if (awra_widget_rtl(w) && (cw != x->content_w || w->w != x->view_w)) {
    /* GtkViewport clamps to the right right to left: the distance from
     * the content's right end stays (it starts at the right end) */
    const double dist = x->content_w - x->hvalue - x->view_w;
    x->hvalue = cw - dist - w->w;
  }
  x->view_w = w->w;
  x->content_w = cw;
  x->content_h = ch;
  scroll_clamp(w);
  awra_allocate(x->child, w->x - (int)x->hvalue, w->y - (int)x->vvalue, cw, ch);
}

/* the overlay indicator (awra.css L919-954): a 5 px pill at the right edge */
/* Content scrolled past an edge fades into what is behind (Awra 1's
 * conversation view masks its scroller so): the bands under the fades are
 * kept before the children draw, and blended back in after. */
static void scroll_draw(AwraWidget *w, AwraDraw *d)
{
  ScrollExt *x = SX(w);
  x->fade_h[0] = x->fade_h[1] = 0;
  if (!x->edge_fade[0] && !x->edge_fade[1]) {
    return;
  }
  const double end = (double)(x->content_h - w->h);
  const float s = awra_scale();
  const int top = x->vvalue / s > 0.5 ? awra_px(x->edge_fade[0]) : 0;
  const int bottom = (end - x->vvalue) / s > 0.5 ? awra_px(x->edge_fade[1]) : 0;
  if ((!top && !bottom) || w->h <= 2 * (awra_px(x->edge_fade[0]) + awra_px(x->edge_fade[1]))) {
    return;
  }
  const size_t need = sizeof(uint32_t) * (size_t)w->w * (size_t)(top + bottom);
  if (x->fade_cap < need) {
    uint32_t *grown = (uint32_t *)realloc(x->fade_under, need);
    if (!grown) {
      return;
    }
    x->fade_under = grown;
    x->fade_cap = need;
  }
  /* top rows first, the bottom rows after them */
  x->fade_h[0] = top && SR_read(d->r, w->x, w->y, w->w, top, x->fade_under) ? top : 0;
  x->fade_h[1] = bottom && SR_read(d->r, w->x, w->y + w->h - bottom, w->w, bottom,
                                           x->fade_under + (size_t)w->w * top) ?
                     bottom :
                     0;
  x->fade_skip = top;
}

static void scroll_fade(AwraWidget *w, AwraDraw *d)
{
  ScrollExt *x = SX(w);
  if (x->fade_h[0] > 0) {
    SR_fade(d->r, w->x, w->y, w->w, x->fade_h[0], x->fade_under, 0.0f, 1.0f);
  }
  if (x->fade_h[1] > 0) {
    SR_fade(d->r, w->x, w->y + w->h - x->fade_h[1], w->w, x->fade_h[1],
                    x->fade_under + (size_t)w->w * x->fade_skip, 1.0f, 0.0f);
  }
  x->fade_h[0] = x->fade_h[1] = 0;
}

/* Whether a scrolled window's indicator is Awra's 5 px pill (awra.css's
 * awrapage scrollbar slider, and .awra-dropdown-popup's): under an
 * AwraPage, or marked so ("awra-pill-bar"). Widgets that style their own
 * scrollbar (list views, conversations, pickers) set its style instead. */
bool awra_scrolled_pill_bar_internal(AwraWidget *w);
bool awra_scrolled_pill_bar_internal(AwraWidget *w)
{
  if (awra_widget_get_data(w, "awra-pill-bar")) {
    return true;
  }
  for (AwraWidget *p = w->parent; p; p = p->parent) {
    if (awra_is_widget(p) && awra_widget_is(p, "AwraPage")) {
      return true;
    }
  }
  return false;
}

static void scroll_draw_over(AwraWidget *w, AwraDraw *d)
{
  /* the scrollbar is a child, drawn last; what is left are the fades */
  scroll_fade(w, d);
}

static void scroll_to(AwraWidget *w, double v, double h)
{
  ScrollExt *x = SX(w);
  const double ov = x->vvalue, oh = x->hvalue;
  x->vvalue = v;
  x->hvalue = h;
  scroll_clamp(w);
  if (x->vvalue != ov || x->hvalue != oh) {
    if (x->vbar && x->vvalue != ov) {
      awra_scrollbar_value_changed_internal(x->vbar); /* indicator_value_changed */
    }
    awra_queue_allocate(w);
    awra_invalidate(w);
    awra_emit_simple(w, AWRA_SIGNAL_VALUE_CHANGED);
  }
}

static int scroll_scroll(AwraWidget *w, BKEvent *e)
{
  ScrollExt *x = SX(w);
  const bool can_v = x->vpolicy != AWRA_POLICY_NEVER && x->content_h > w->h;
  const bool can_h = x->hpolicy != AWRA_POLICY_NEVER && x->content_w > w->w;
  if (!can_v && !can_h) {
    return 0;
  }
  /* GTK: a wheel step is page_size^(2/3) (logical) pixels */
  const double k = awra_scale();
  const double step_v = pow((double)w->h / k, 2.0 / 3.0) * k, step_h = pow((double)w->w / k, 2.0 / 3.0) * k;
  double dy = -e->wheel_y, dx = e->wheel_x;
  if ((e->mods & BK_SHIFT) && !can_v) {
    dx = dy;
    dy = 0;
  }
  double sv = can_v ? dy * step_v : 0, sh = can_h ? dx * step_h : 0;
  /* GtkListBase (a tile grid is GtkGridView's layout) keeps its
   * adjustments on whole px: each step moves a whole number of px, the
   * fraction dropped (73 px up or down for a 631 px page) */
  if (x->whole_steps || (x->child && awra_widget_is(x->child, "AwraTileGrid"))) {
    sv = trunc(sv / k) * k;
    sh = trunc(sh / k) * k;
  }
  const double nv = x->vvalue + sv, nh = x->hvalue + sh;
  const double ov = x->vvalue, oh = x->hvalue;
  scroll_to(w, nv, nh);
  return x->vvalue != ov || x->hvalue != oh || can_v;
}

static void scroll_finalize(AwraWidget *w)
{
  awra_scrolled_forget_motion_internal(w);
  free(SX(w)->fade_under);
}

/* GtkScrolledWindow's captured motion (core.c) */
void awra_scrolled_captured_motion_internal(AwraWidget *w, AwraWidget *under, int px, int py, bool buttons);
void awra_scrolled_captured_motion_internal(AwraWidget *w, AwraWidget *under, int px, int py, bool buttons)
{
  ScrollExt *x = SX(w);
  if (x->vbar && x->child) {
    awra_scrollbar_motion_internal(x->vbar, px, py, under == x->vbar, buttons);
  }
}

void awra_scrolled_pointer_left_internal(AwraWidget *w);
void awra_scrolled_pointer_left_internal(AwraWidget *w)
{
  if (SX(w)->vbar) {
    awra_scrollbar_leave_internal(SX(w)->vbar);
  }
}

/* value, upper and page (raster px), and whether it scrolls vertically */
void awra_scrolled_metrics_internal(AwraWidget *w, double *r_value, double *r_upper, double *r_page,
                                    bool *r_can);
void awra_scrolled_metrics_internal(AwraWidget *w, double *r_value, double *r_upper, double *r_page,
                                    bool *r_can)
{
  ScrollExt *x = SX(w);
  *r_value = x->vvalue;
  *r_upper = x->content_h;
  *r_page = w->h;
  /* vscrollbar_visible: AUTOMATIC and ALWAYS, while the content is taller */
  *r_can = x->child && (x->vpolicy == AWRA_POLICY_AUTOMATIC || x->vpolicy == AWRA_POLICY_ALWAYS) &&
           x->content_h > w->h;
}

void awra_scrolled_set_vvalue_px_internal(AwraWidget *w, double v);
void awra_scrolled_set_vvalue_px_internal(AwraWidget *w, double v)
{
  scroll_to(w, v, SX(w)->hvalue);
}

/* awra.css restyling the scrollbar in a context (scrollbar.c STYLE_*) */
void awra_scrolled_set_scrollbar_style_internal(AwraWidget *w, int style);
void awra_scrolled_set_scrollbar_style_internal(AwraWidget *w, int style)
{
  if (SX(w)->vbar) {
    awra_scrollbar_set_style_internal(SX(w)->vbar, style);
  }
}

void awra_scrolled_set_edge_fades(AwraWidget *w, int top, int bottom)
{
  ScrollExt *x = SX(w);
  x->edge_fade[0] = top > 0 ? top : 0;
  x->edge_fade[1] = bottom > 0 ? bottom : 0;
  if (!top && !bottom) {
    free(x->fade_under);
    x->fade_under = NULL;
    x->fade_cap = 0;
  }
  awra_invalidate(w);
}

static AwraClass scrolled_class = {
    .bk = AWRA_BK("AwraScrolledWindow", CLS_SCROLLER),
    .type = "AwraScrolledWindow",
    /* it places its content itself (scrolled, kept at the right end right to left) */
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_CLIP | AWRA_CLASS_NO_MIRROR,
    .ext_size = sizeof(ScrollExt),
    .finalize = scroll_finalize,
    .measure = scroll_measure,
    .allocate = scroll_allocate,
    .draw = scroll_draw,
    .draw_over = scroll_draw_over,
    .scroll = scroll_scroll,
};

bool awra_is_scrolled(const AwraWidget *w)
{
  return w && awra_is_widget((AwraWidget *)w) && AWRA_CLASS_OF(w) == &scrolled_class;
}

AwraWidget *awra_scrolled_new(void)
{
  AwraWidget *w = awra_widget_new(&scrolled_class);
  ScrollExt *x = SX(w);
  x->hpolicy = AWRA_POLICY_AUTOMATIC;
  x->vpolicy = AWRA_POLICY_AUTOMATIC;
  x->vbar = awra_scrollbar_new_internal(w);
  awra_child_insert(w, x->vbar, -1);
  awra_widget_unref(x->vbar); /* held as a child */
  awra_widget_set_child_visible(x->vbar, false);
  return w;
}

void awra_scrolled_set_child(AwraWidget *w, AwraWidget *child)
{
  ScrollExt *x = SX(w);
  if (x->child == child) {
    return;
  }
  AwraWidget *old = x->child;
  x->child = child;
  if (old) {
    awra_child_remove(w, old);
  }
  if (child) {
    /* under the scrollbar: first */
    awra_widget_ref(child);
    if (child->parent) {
      awra_container_remove(child->parent, child);
    }
    awra_child_insert(w, child, 0);
    awra_widget_unref(child);
  }
}

AwraWidget *awra_scrolled_child(AwraWidget *w)
{
  return SX(w)->child;
}

void awra_scrolled_set_whole_steps(AwraWidget *w, bool whole)
{
  SX(w)->whole_steps = whole;
}

void awra_scrolled_set_policy(AwraWidget *w, AwraPolicy h, AwraPolicy v)
{
  SX(w)->hpolicy = h;
  SX(w)->vpolicy = v;
  awra_widget_queue_resize(w);
}

void awra_scrolled_set_propagate_natural(AwraWidget *w, bool width, bool height)
{
  SX(w)->prop_w = width;
  SX(w)->prop_h = height;
  awra_widget_queue_resize(w);
}

void awra_scrolled_set_max_content_height(AwraWidget *w, int h)
{
  SX(w)->max_h = h;
  awra_widget_queue_resize(w);
}

void awra_scrolled_set_min_content_height(AwraWidget *w, int h)
{
  SX(w)->min_h = h;
  awra_widget_queue_resize(w);
}

void awra_scrolled_set_min_content_width(AwraWidget *w, int width)
{
  SX(w)->min_w = width;
  awra_widget_queue_resize(w);
}

void awra_scrolled_set_max_content_width(AwraWidget *w, int width)
{
  SX(w)->max_w = width;
  awra_widget_queue_resize(w);
}

double awra_scrolled_vvalue(AwraWidget *w)
{
  return SX(w)->vvalue / awra_scale();
}

void awra_scrolled_set_vvalue(AwraWidget *w, double v)
{
  scroll_to(w, v * awra_scale(), SX(w)->hvalue);
}

double awra_scrolled_hvalue(AwraWidget *w)
{
  return SX(w)->hvalue / awra_scale();
}

void awra_scrolled_set_hvalue(AwraWidget *w, double v)
{
  scroll_to(w, SX(w)->vvalue, v * awra_scale());
}

void awra_scrolled_scroll_to(AwraWidget *w, AwraWidget *child)
{
  ScrollExt *x = SX(w);
  if (!child || child->h <= 0) {
    return;
  }
  const int top = child->y - (w->y - (int)x->vvalue);
  double v = x->vvalue;
  if (top < v) {
    v = top;
  }
  else if (top + child->h > v + w->h) {
    v = top + child->h - w->h;
  }
  scroll_to(w, v, x->hvalue);
}

/* Moves the scroll position by `delta` raster px without the indicator,
 * clamped at the next allocation (lists keeping an item in place while the
 * heights above it become known). */
void awra_scrolled_shift_internal(AwraWidget *w, double delta);
void awra_scrolled_shift_internal(AwraWidget *w, double delta)
{
  ScrollExt *x = SX(w);
  if (delta == 0) {
    return;
  }
  x->vvalue += delta;
  if (x->vvalue < 0) {
    x->vvalue = 0;
  }
  awra_queue_allocate(w);
  awra_invalidate(w);
  x->shifting = true;
  awra_emit_simple(w, AWRA_SIGNAL_VALUE_CHANGED);
  x->shifting = false;
}

/* Whether the value is changing because a list inside keeps its rows where
 * they were (rows measured, items added above), not because anyone scrolled. */
bool awra_scrolled_shifting_internal(AwraWidget *w);
bool awra_scrolled_shifting_internal(AwraWidget *w)
{
  return SX(w)->shifting;
}

/* The scrolled window's vertical range: value, upper (content height) and
 * page size, logical px. */
void awra_scrolled_get_vadjustment(AwraWidget *w, double *value, double *upper, double *page)
{
  ScrollExt *x = SX(w);
  const float s = awra_scale();
  if (value) {
    *value = x->vvalue / s;
  }
  if (upper) {
    *upper = x->content_h / s;
  }
  if (page) {
    *page = w->h / s;
  }
}

void awra_flow_box_set_child_padding(AwraWidget *flow, int padding)
{
  AWRA_EXT(flow, FlowExt)->child_pad = padding;
  awra_widget_queue_resize(flow);
}

int awra_box_axis(const AwraWidget *w)
{
  return awra_widget_is(w, "AwraBox") ? AWRA_EXT(w, BoxExt)->orientation : -1;
}
