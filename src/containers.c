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
  for (int i = 0; i < w->nkids; i++) {
    AwraWidget *c = w->kids[i];
    if (!awra_child_visible(c)) {
      continue;
    }
    awra_measure(c, o, for_cross, &mn[k], &nt[k]);
    sum_min += mn[k];
    n_expand += awra_compute_expand(c, o);
    k++;
  }
  int extra = avail - sp;
  if (b->homogeneous) {
    const int each = n > 0 ? BK_MAX(0, extra) / n : 0;
    int rem = n > 0 ? BK_MAX(0, extra) - each * n : 0;
    for (int i = 0; i < n; i++) {
      sizes[i] = BK_MAX(mn[i], each + (i < rem ? 1 : 0));
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

static AwraClass box_class = {
    .bk = AWRA_BK("AwraBox", 0),
    .type = "AwraBox",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(BoxExt),
    .measure = box_measure,
    .allocate = box_allocate,
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

/* Line sizes along `o` (columns when horizontal); returns the count. */
static int grid_lines(AwraWidget *g, int o, int **r_min, int **r_nat, bool **r_expand)
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
  /* single-span children first, then spans grow their lines if needed */
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
      awra_measure(k, o, -1, &cm, &cn);
      if (awra_compute_expand(k, o)) {
        for (int s = start; s < start + span; s++) {
          ex[s] = true;
        }
      }
      if (span == 1) {
        mn[start] = BK_MAX(mn[start], cm);
        nt[start] = BK_MAX(nt[start], cn);
      }
      else {
        int have_m = sp * (span - 1), have_n = sp * (span - 1);
        for (int s = start; s < start + span; s++) {
          have_m += mn[s];
          have_n += nt[s];
        }
        for (int s = start; s < start + span; s++) {
          if (cm > have_m) {
            mn[s] += (cm - have_m) / span + 1;
          }
          if (cn > have_n) {
            nt[s] += (cn - have_n) / span + 1;
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

static void grid_measure(AwraWidget *g, int o, int for_size, int *r_min, int *r_nat)
{
  int *mn, *nt;
  bool *ex;
  const int n = grid_lines(g, o, &mn, &nt, &ex);
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

static int *grid_sizes(AwraWidget *g, int o, int avail, int *r_n)
{
  int *mn, *nt;
  bool *ex;
  const int n = grid_lines(g, o, &mn, &nt, &ex);
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
  int *cw = grid_sizes(g, AWRA_ORIENTATION_HORIZONTAL, g->w, &nc);
  int *rh = grid_sizes(g, AWRA_ORIENTATION_VERTICAL, g->h, &nr);
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
} FlowExt;

static bool flow_shown(AwraWidget *f, AwraWidget *c)
{
  const FlowExt *x = AWRA_EXT(f, FlowExt);
  if (!awra_child_visible(c)) {
    return false;
  }
  return !x->filter || x->filter(c, x->filter_data);
}

static void flow_child_size(AwraWidget *f, int *r_min_w, int *r_nat_w, int *r_n)
{
  int mw = 0, nw = 0, n = 0;
  for (int i = 0; i < f->nkids; i++) {
    AwraWidget *c = f->kids[i];
    if (!flow_shown(f, c)) {
      continue;
    }
    int cm, cn;
    awra_measure(c, AWRA_ORIENTATION_HORIZONTAL, -1, &cm, &cn);
    mw = BK_MAX(mw, cm);
    nw = BK_MAX(nw, cn);
    n++;
  }
  *r_min_w = mw;
  *r_nat_w = nw;
  *r_n = n;
}

/* GtkFlowBox (homogeneous or not): how many children per line for a width */
static int flow_columns(AwraWidget *f, int width)
{
  const FlowExt *x = AWRA_EXT(f, FlowExt);
  int mw, nw, n;
  flow_child_size(f, &mw, &nw, &n);
  if (n == 0) {
    return 1;
  }
  const int cs = awra_px(x->col_spacing);
  const int maxl = x->max_per_line > 0 ? x->max_per_line : n;
  const int minl = BK_MAX(1, x->min_per_line);
  int cols = (width + cs) / BK_MAX(1, mw + cs);
  cols = BK_CLAMP(cols, minl, BK_MAX(minl, maxl));
  return BK_MIN(cols, BK_MAX(1, n));
}

static void flow_measure(AwraWidget *f, int o, int for_size, int *r_min, int *r_nat)
{
  const FlowExt *x = AWRA_EXT(f, FlowExt);
  int mw, nw, n;
  flow_child_size(f, &mw, &nw, &n);
  const int cs = awra_px(x->col_spacing), rs = awra_px(x->row_spacing);
  if (n == 0) {
    *r_min = *r_nat = 0;
    return;
  }
  const int minl = BK_MAX(1, x->min_per_line);
  const int maxl = x->max_per_line > 0 ? BK_MIN(x->max_per_line, n) : n;
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    *r_min = mw * BK_MIN(minl, n) + cs * (BK_MIN(minl, n) - 1);
    *r_nat = nw * maxl + cs * (maxl - 1);
    return;
  }
  const int cols = for_size >= 0 ? flow_columns(f, for_size) : maxl;
  const int cw = for_size >= 0 ? (for_size - cs * (cols - 1)) / cols : nw;
  int total_m = 0, total_n = 0, row_m = 0, row_n = 0, k = 0;
  for (int i = 0; i < f->nkids; i++) {
    AwraWidget *c = f->kids[i];
    if (!flow_shown(f, c)) {
      continue;
    }
    int cm, cn;
    awra_measure(c, AWRA_ORIENTATION_VERTICAL, cw, &cm, &cn);
    row_m = BK_MAX(row_m, cm);
    row_n = BK_MAX(row_n, cn);
    if (++k % cols == 0) {
      total_m += row_m + rs;
      total_n += row_n + rs;
      row_m = row_n = 0;
    }
  }
  if (k % cols) {
    total_m += row_m + rs;
    total_n += row_n + rs;
  }
  *r_min = BK_MAX(0, total_m - rs);
  *r_nat = BK_MAX(0, total_n - rs);
}

static void flow_allocate(AwraWidget *f)
{
  const FlowExt *x = AWRA_EXT(f, FlowExt);
  const int cs = awra_px(x->col_spacing), rs = awra_px(x->row_spacing);
  const int cols = flow_columns(f, f->w);
  const int cw = (f->w - cs * (cols - 1)) / cols;
  int rem_w = f->w - cs * (cols - 1) - cw * cols;
  int y = f->y, k = 0;
  /* rows: measure each row's height first */
  int row_start = 0;
  while (row_start < f->nkids) {
    int row_h = 0, taken = 0, i = row_start;
    for (; i < f->nkids && taken < cols; i++) {
      AwraWidget *c = f->kids[i];
      if (!flow_shown(f, c)) {
        continue;
      }
      int cm, cn;
      awra_measure(c, AWRA_ORIENTATION_VERTICAL, cw, &cm, &cn);
      row_h = BK_MAX(row_h, cn);
      taken++;
    }
    int xx = f->x, col = 0;
    for (int j = row_start; j < i; j++) {
      AwraWidget *c = f->kids[j];
      if (!flow_shown(f, c)) {
        if (awra_child_visible(c)) {
          c->w = c->h = 0; /* filtered out: nothing to draw or hit */
        }
        continue;
      }
      const int w = cw + (col < rem_w ? 1 : 0);
      awra_allocate(c, xx, y, w, row_h);
      xx += w + cs;
      col++;
      k++;
    }
    y += row_h + rs;
    row_start = i;
  }
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
    awra_measure(c, o, for_size, &cm, &cn);
    mn = BK_MAX(mn, cm);
    nt = BK_MAX(nt, cn);
  }
  *r_min = mn;
  *r_nat = nt;
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
      awra_allocate(c, s->x + ox, s->y, s->w, s->h);
    }
  }
}

static void stack_draw_tree(BKNode *n);

static AwraClass stack_class = {
    .bk = AWRA_BK("AwraStack", 0),
    .type = "AwraStack",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_CLIP,
    .ext_size = sizeof(StackExt),
    .measure = stack_measure,
    .allocate = stack_allocate,
};

static void stack_sync_visibility(AwraWidget *s)
{
  StackExt *x = AWRA_EXT(s, StackExt);
  for (int i = 0; i < s->nkids; i++) {
    AwraWidget *c = s->kids[i];
    const bool show = c == x->visible || c == x->previous;
    if (((c->flags & NF_VISIBLE) != 0) != show) {
      awra_widget_set_visible(c, show);
    }
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
  }
  x->previous = x->transition != AWRA_TRANSITION_NONE && awra_widget_mapped(s) ? x->visible : NULL;
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
    awra_allocate(w->kids[i], w->x, w->y, w->w, w->h);
  }
  (void)x;
}

static AwraClass overlay_class = {
    .bk = AWRA_BK("AwraOverlay", 0),
    .type = "AwraOverlay",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(OverlayExt),
    .measure = overlay_measure,
    .allocate = overlay_allocate,
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
    awra_child_insert(w, child, 0); /* below the overlays */
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
  bool prop_w, prop_h;
  int max_h, min_h; /* logical, -1 none */
  double vvalue, hvalue; /* raster px */
  int content_w, content_h;
  float bar_t;      /* indicator opacity */
  double bar_until; /* ms: keep showing */
  bool dragging;
  int drag_y0;
  double drag_v0;
  unsigned fade_timer;
} ScrollExt;

#define SX(w) AWRA_EXT(w, ScrollExt)

static void scroll_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  ScrollExt *x = SX(w);
  int cm = 0, cn = 0;
  if (x->child) {
    awra_measure(x->child, o, o == AWRA_ORIENTATION_VERTICAL ? for_size : -1, &cm, &cn);
  }
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    *r_min = x->hpolicy == AWRA_POLICY_NEVER ? cm : 0;
    *r_nat = x->prop_w || x->hpolicy == AWRA_POLICY_NEVER ? cn : *r_min;
  }
  else {
    *r_min = x->vpolicy == AWRA_POLICY_NEVER ? cm : 0;
    *r_nat = x->prop_h || x->vpolicy == AWRA_POLICY_NEVER ? cn : *r_min;
    if (x->min_h > 0) {
      *r_min = BK_MAX(*r_min, awra_px(x->min_h));
      *r_nat = BK_MAX(*r_nat, *r_min);
    }
    if (x->max_h > 0 && x->prop_h) {
      *r_nat = BK_MIN(*r_nat, awra_px(x->max_h));
      *r_nat = BK_MAX(*r_nat, *r_min);
    }
  }
}

static void scroll_clamp(AwraWidget *w)
{
  ScrollExt *x = SX(w);
  const double maxv = BK_MAX(0, x->content_h - w->h);
  const double maxh = BK_MAX(0, x->content_w - w->w);
  x->vvalue = x->vvalue < 0 ? 0 : x->vvalue > maxv ? maxv : x->vvalue;
  x->hvalue = x->hvalue < 0 ? 0 : x->hvalue > maxh ? maxh : x->hvalue;
}

static void scroll_allocate(AwraWidget *w)
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
  x->content_w = cw;
  x->content_h = ch;
  scroll_clamp(w);
  awra_allocate(x->child, w->x - (int)x->hvalue, w->y - (int)x->vvalue, cw, ch);
}

/* the overlay indicator (awra.css L919-954): a 5 px pill at the right edge */
static void scroll_draw_over(AwraWidget *w, AwraDraw *d)
{
  ScrollExt *x = SX(w);
  if (x->content_h <= w->h || x->bar_t <= 0.0f || x->vpolicy == AWRA_POLICY_EXTERNAL) {
    return;
  }
  const float track = (float)w->h;
  float len = track * (float)w->h / (float)x->content_h;
  len = fmaxf(len, awra_pxf(18));
  const float pos = (float)(x->vvalue / (x->content_h - w->h)) * (track - len);
  AwraRGBA c = awra_tokens()->control_thumb;
  c.a *= x->bar_t;
  const float bw = awra_pxf(5);
  awra_paint_rbox(d, (AwraBox){w->x + w->w - awra_pxf(6), w->y + pos, bw, len}, bw * 0.5f, &c, 0,
                  NULL);
}

static bool fade_tick(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  ScrollExt *x = SX(w);
  const double now = awra_now_ms();
  if (now < x->bar_until) {
    return true;
  }
  /* 1 s fade after the 2 s delay */
  const float t = (float)(1.0 - (now - x->bar_until) / 1000.0);
  x->bar_t = t > 0 ? t : 0;
  BKRect r = bk_rect(w->x + w->w - awra_px(8), w->y, awra_px(8), w->h);
  BKContext *ctx = bk_ctx(w);
  if (ctx) {
    bk_ctx_dirty_rect(ctx, r);
  }
  if (x->bar_t <= 0.0f) {
    x->fade_timer = 0;
    return false;
  }
  return true;
}

static void show_bar(AwraWidget *w)
{
  ScrollExt *x = SX(w);
  x->bar_t = 1.0f;
  x->bar_until = awra_now_ms() + 2000.0;
  if (!x->fade_timer) {
    x->fade_timer = awra_timeout_add(33, fade_tick, w, NULL);
  }
}

static void scroll_to(AwraWidget *w, double v, double h)
{
  ScrollExt *x = SX(w);
  const double ov = x->vvalue, oh = x->hvalue;
  x->vvalue = v;
  x->hvalue = h;
  scroll_clamp(w);
  if (x->vvalue != ov || x->hvalue != oh) {
    show_bar(w);
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
  /* GTK: a wheel step is page_size^(2/3) pixels */
  const double step_v = pow((double)w->h, 2.0 / 3.0), step_h = pow((double)w->w, 2.0 / 3.0);
  double dy = -e->wheel_y, dx = e->wheel_x;
  if ((e->mods & BK_SHIFT) && !can_v) {
    dx = dy;
    dy = 0;
  }
  const double nv = x->vvalue + (can_v ? dy * step_v : 0);
  const double nh = x->hvalue + (can_h ? dx * step_h : 0);
  const double ov = x->vvalue, oh = x->hvalue;
  scroll_to(w, nv, nh);
  return x->vvalue != ov || x->hvalue != oh || can_v;
}

static void scroll_finalize(AwraWidget *w)
{
  if (SX(w)->fade_timer) {
    awra_source_remove(SX(w)->fade_timer);
  }
}

static AwraClass scrolled_class = {
    .bk = AWRA_BK("AwraScrolledWindow", 0),
    .type = "AwraScrolledWindow",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_CLIP,
    .ext_size = sizeof(ScrollExt),
    .finalize = scroll_finalize,
    .measure = scroll_measure,
    .allocate = scroll_allocate,
    .draw_over = scroll_draw_over,
    .scroll = scroll_scroll,
};

AwraWidget *awra_scrolled_new(void)
{
  AwraWidget *w = awra_widget_new(&scrolled_class);
  ScrollExt *x = SX(w);
  x->hpolicy = AWRA_POLICY_AUTOMATIC;
  x->vpolicy = AWRA_POLICY_AUTOMATIC;
  return w;
}

void awra_scrolled_set_child(AwraWidget *w, AwraWidget *child)
{
  awra_set_slot(w, &SX(w)->child, child);
}

AwraWidget *awra_scrolled_child(AwraWidget *w)
{
  return SX(w)->child;
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
