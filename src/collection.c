/* Awra 2 collection view: a virtualized grid of tiles or list of rows with
 * columns (Awra 1's AwraGridView over GtkGridView and AwraDataView over
 * GtkColumnView), for collections of any size: only the items on screen
 * have widgets, built by the application's factory and recycled as the
 * view scrolls.
 *
 * Geometry is Awra 1's (awra.css .awra-grid child, awradataview with the
 * compact-table, canvas-table and spaced-rows roles): grid tiles take
 * GtkGridView's columns (as many as fit, at most max-columns, the width
 * shared evenly) with a 4 px margin and 8 px padding, radius 8, hover in
 * control-hover, selection in selection with a 1 px inner focus line; list
 * rows are radius 4 with selection and cap-hover, cells padded 5 x 10, and
 * column headers are transparent buttons (4 10 8 padding, 24 px min,
 * .86em weight 500, muted) with GTK's sort indicator.
 *
 * Interaction is GTK's list grammar: click selects, Ctrl toggles, Shift
 * extends, double click / Return activates, arrows move the cursor; a press
 * in the blank (between tiles, beside a row's name) draws a rubberband that
 * scrolls the view at its edges; a drag from an item emits drag-begin for
 * the application's drag and drop.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

#include <stdio.h>

#define CELL_PAD_X 10
#define CELL_PAD_Y 5
#define TILE_MARGIN 4
#define TILE_PAD 8
#define DRAG_THRESHOLD 8
#define EDGE_SCROLL 32

typedef struct Column {
  char *title;
  int width; /* logical, -1: natural of the title */
  bool expand, visible, resizable;
  int x, w; /* computed, raster, relative to the view */
} Column;

typedef struct Bound {
  int index;
  AwraWidget **cells; /* one per column (grid: one) */
} Bound;

typedef struct CollExt {
  int layout;
  int n;
  AwraCollectionSetup setup;
  AwraCollectionBind bind;
  AwraCollectionUnbind unbind;
  void *data;
  AwraDestroyNotify destroy;
  Column *cols;
  int ncols;
  int sort_col;
  bool sort_desc;
  bool show_header;
  int item_w, item_h; /* grid content size, logical */
  int row_h;          /* list row height, logical */
  int row_gap;        /* list: blank below each row, logical (Awra 1's spaced-rows) */
  int max_cols;
  /* computed, raster */
  int header_h, grid_cols, tile_h, content_h, view_h;
  double col_w;
  double vvalue;
  /* bound items: a window over [first, first + nvis) */
  Bound *vis;
  int first, nvis;
  /* recycled cells, per column */
  AwraWidget ***pool;
  int *npool, *cappool;
  /* selection */
  uint8_t *sel;
  int sel_cap, sel_count;
  int cursor, anchor;
  int hover, drop;
  bool layout_dirty;
  /* pointer */
  int press_index, press_col;
  bool press_content, press_selected, press_header, press_scrollbar, press_dbl;
  int press_mods;
  int press_x, press_y; /* window, raster */
  double press_v;       /* vvalue at press */
  bool band, dnd;
  double band_x0, band_y0, band_x1, band_y1; /* content coordinates, raster */
  uint8_t *band_initial;
  unsigned edge_timer;
  int last_x, last_y;
  int resize_col, resize_w0;
  int header_hover;
  /* scroll indicator */
  float bar_t;
  double bar_until;
  unsigned fade_timer;
  AwraTextLayout **titles;
} CollExt;

#define CX(w) AWRA_EXT(w, CollExt)

static void coll_emit_selection(AwraWidget *w);

/* ----------------------------------------------------------------------- */
/* Geometry */

static int list_row_h(const CollExt *x)
{
  return awra_px(x->row_h > 0 ? x->row_h : 30);
}

static AwraFont header_font(void)
{
  AwraFont f = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  f.size *= 0.86f;
  f.weight = 500;
  return f;
}

static int compute_header_h(const CollExt *x)
{
  if (x->layout != AWRA_COLLECTION_LIST || !x->show_header || x->ncols == 0) {
    return 0;
  }
  AwraFont f = header_font();
  const int text = (int)ceilf(awra_font_line_height(&f));
  return BK_MAX(awra_px(24), BK_MAX(text, awra_px(16))) + awra_px(4) + awra_px(8);
}

static int title_width(CollExt *x, int c)
{
  AwraFont f = header_font();
  const char *t = x->cols[c].title ? x->cols[c].title : "";
  return (int)ceilf(awra_text_width(&f, t, (int)strlen(t))) + awra_px(2 * CELL_PAD_X) +
         (c == x->sort_col ? awra_px(16 + 6) : 0);
}

static void compute_columns(AwraWidget *w)
{
  CollExt *x = CX(w);
  int fixed = 0, nexp = 0;
  for (int c = 0; c < x->ncols; c++) {
    Column *col = &x->cols[c];
    if (!col->visible) {
      col->w = 0;
      continue;
    }
    if (col->expand) {
      nexp++;
      col->w = col->width > 0 ? awra_px(col->width) : title_width(x, c);
    }
    else {
      col->w = col->width > 0 ? awra_px(col->width) : title_width(x, c);
    }
    fixed += col->w;
  }
  int extra = w->w - fixed;
  int cx = 0;
  for (int c = 0; c < x->ncols; c++) {
    Column *col = &x->cols[c];
    if (!col->visible) {
      col->x = cx;
      continue;
    }
    if (col->expand && extra > 0 && nexp > 0) {
      const int add = extra / nexp;
      col->w += add;
      extra -= add;
      nexp--;
    }
    col->x = cx;
    cx += col->w;
  }
}

static void compute_geometry(AwraWidget *w)
{
  CollExt *x = CX(w);
  x->header_h = compute_header_h(x);
  x->view_h = BK_MAX(0, w->h - x->header_h);
  if (x->layout == AWRA_COLLECTION_GRID) {
    const int min_w = awra_px(x->item_w + 2 * (TILE_PAD + TILE_MARGIN));
    int cols = min_w > 0 ? w->w / min_w : 1;
    cols = BK_CLAMP(cols, 1, x->max_cols > 0 ? x->max_cols : 7);
    x->grid_cols = cols;
    x->col_w = (double)w->w / cols;
    x->tile_h = awra_px(x->item_h + 2 * (TILE_PAD + TILE_MARGIN));
    const int rows = (x->n + cols - 1) / cols;
    x->content_h = rows * x->tile_h;
  }
  else {
    compute_columns(w);
    x->grid_cols = 1;
    x->col_w = w->w;
    x->tile_h = list_row_h(x) + awra_px(x->row_gap);
    x->content_h = x->n * x->tile_h;
  }
  const double maxv = BK_MAX(0, x->content_h - x->view_h);
  if (x->vvalue > maxv) {
    x->vvalue = maxv;
  }
  if (x->vvalue < 0) {
    x->vvalue = 0;
  }
}

/* An item's rect in window coordinates (raster); the tile without margins
 * in a grid, the row in a list. */
static void item_rect(AwraWidget *w, int i, int *rx, int *ry, int *rw, int *rh)
{
  CollExt *x = CX(w);
  const int top = w->y + x->header_h - (int)x->vvalue;
  if (x->layout == AWRA_COLLECTION_GRID) {
    const int col = i % x->grid_cols, row = i / x->grid_cols;
    const int x0 = (int)floor(col * x->col_w + 0.5), x1 = (int)floor((col + 1) * x->col_w + 0.5);
    const int m = awra_px(TILE_MARGIN);
    *rx = w->x + x0 + m;
    *ry = top + row * x->tile_h + m;
    *rw = x1 - x0 - 2 * m;
    *rh = x->tile_h - 2 * m;
  }
  else {
    *rx = w->x;
    *ry = top + i * x->tile_h;
    *rw = w->w;
    *rh = x->tile_h - awra_px(x->row_gap);
  }
}

/* The index of the item at a window point, -1 in the blank; content: the
 * point is on what the item shows (tile, or the name cell's content). */
static int index_at(AwraWidget *w, int px, int py, bool *r_content)
{
  CollExt *x = CX(w);
  if (r_content) {
    *r_content = false;
  }
  if (px < w->x || px >= w->x + w->w || py < w->y + x->header_h || py >= w->y + w->h) {
    return -1;
  }
  const double cy = py - (w->y + x->header_h) + x->vvalue;
  int i;
  if (x->layout == AWRA_COLLECTION_GRID) {
    const int col = (int)((px - w->x) / x->col_w);
    const int row = (int)(cy / x->tile_h);
    if (col < 0 || col >= x->grid_cols) {
      return -1;
    }
    i = row * x->grid_cols + col;
  }
  else {
    i = (int)(cy / x->tile_h);
    /* the gap below a row is blank */
    if (x->row_gap > 0 && cy - (double)i * x->tile_h >= x->tile_h - awra_px(x->row_gap)) {
      return -1;
    }
  }
  if (i < 0 || i >= x->n) {
    return -1;
  }
  int rx, ry, rw, rh;
  item_rect(w, i, &rx, &ry, &rw, &rh);
  if (x->layout == AWRA_COLLECTION_GRID) {
    if (px < rx || px >= rx + rw || py < ry || py >= ry + rh) {
      return -1;
    }
    if (r_content) {
      *r_content = true;
    }
    return i;
  }
  if (r_content && i >= x->first && i < x->first + x->nvis) {
    /* the first column's content: from the cell start to its natural width */
    const Bound *b = &x->vis[i - x->first];
    int c0 = -1;
    for (int c = 0; c < x->ncols && c0 < 0; c++) {
      if (x->cols[c].visible) {
        c0 = c;
      }
    }
    if (c0 >= 0 && b->cells && b->cells[c0]) {
      AwraWidget *cell = b->cells[c0];
      int mn, nat;
      awra_measure(cell, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nat);
      const int cx0 = cell->x, cx1 = cell->x + BK_MIN(nat, cell->w);
      *r_content = px >= cx0 && px < cx1 && py >= cell->y && py < cell->y + cell->h;
    }
  }
  return i;
}

/* ----------------------------------------------------------------------- */
/* Binding */

static AwraWidget *take_cell(AwraWidget *w, int c)
{
  CollExt *x = CX(w);
  if (x->npool[c] > 0) {
    AwraWidget *cell = x->pool[c][--x->npool[c]];
    awra_widget_set_visible(cell, true);
    return cell;
  }
  AwraWidget *cell = x->setup ? x->setup(w, x->layout == AWRA_COLLECTION_GRID ? -1 : c, x->data) : NULL;
  if (!cell) {
    cell = awra_label_new("");
  }
  awra_child_insert(w, cell, -1);
  awra_widget_unref(cell); /* the view holds it */
  return cell;
}

static void give_cell(AwraWidget *w, int c, AwraWidget *cell)
{
  CollExt *x = CX(w);
  if (x->npool[c] == x->cappool[c]) {
    x->cappool[c] = x->cappool[c] ? x->cappool[c] * 2 : 16;
    x->pool[c] = (AwraWidget **)realloc(x->pool[c], sizeof(AwraWidget *) * x->cappool[c]);
  }
  x->pool[c][x->npool[c]++] = cell;
  awra_widget_set_visible(cell, false);
}

static int ncells(const CollExt *x)
{
  return x->layout == AWRA_COLLECTION_GRID ? 1 : BK_MAX(1, x->ncols);
}

static void unbind_one(AwraWidget *w, Bound *b)
{
  CollExt *x = CX(w);
  if (!b->cells) {
    return;
  }
  const int nc = ncells(x);
  for (int c = 0; c < nc; c++) {
    if (b->cells[c]) {
      if (x->unbind) {
        x->unbind(w, b->cells[c], x->layout == AWRA_COLLECTION_GRID ? -1 : c, b->index, x->data);
      }
      give_cell(w, c, b->cells[c]);
    }
  }
  free(b->cells);
  b->cells = NULL;
}

static void bind_one(AwraWidget *w, Bound *b, int index)
{
  CollExt *x = CX(w);
  const int nc = ncells(x);
  b->index = index;
  b->cells = (AwraWidget **)calloc((size_t)nc, sizeof(AwraWidget *));
  for (int c = 0; c < nc; c++) {
    const bool grid = x->layout == AWRA_COLLECTION_GRID;
    if (!grid && x->ncols && !x->cols[c].visible) {
      continue;
    }
    b->cells[c] = take_cell(w, c);
    if (x->bind) {
      x->bind(w, b->cells[c], grid ? -1 : c, index, x->data);
    }
  }
}

static void unbind_all(AwraWidget *w)
{
  CollExt *x = CX(w);
  for (int k = 0; k < x->nvis; k++) {
    unbind_one(w, &x->vis[k]);
  }
  x->nvis = 0;
  x->first = 0;
}

/* Drops every cell (layout or columns changed: the factory builds new ones). */
static void drop_pools(AwraWidget *w)
{
  CollExt *x = CX(w);
  unbind_all(w);
  for (int c = 0; c < x->ncols + 1; c++) {
    for (int k = 0; k < x->npool[c]; k++) {
      awra_child_remove(w, x->pool[c][k]);
    }
    x->npool[c] = 0;
  }
}

static void ensure_pools(AwraWidget *w)
{
  CollExt *x = CX(w);
  const int n = x->ncols + 1;
  x->pool = (AwraWidget ***)realloc(x->pool, sizeof(AwraWidget **) * n);
  x->npool = (int *)realloc(x->npool, sizeof(int) * n);
  x->cappool = (int *)realloc(x->cappool, sizeof(int) * n);
  x->titles = (AwraTextLayout **)realloc(x->titles, sizeof(AwraTextLayout *) * n);
  x->pool[n - 1] = NULL;
  x->npool[n - 1] = 0;
  x->cappool[n - 1] = 0;
  x->titles[n - 1] = NULL;
}

static void place_cells(AwraWidget *w, Bound *b)
{
  CollExt *x = CX(w);
  int rx, ry, rw, rh;
  item_rect(w, b->index, &rx, &ry, &rw, &rh);
  if (x->layout == AWRA_COLLECTION_GRID) {
    const int p = awra_px(TILE_PAD);
    if (b->cells[0]) {
      awra_allocate(b->cells[0], rx + p, ry + p, BK_MAX(0, rw - 2 * p), BK_MAX(0, rh - 2 * p));
    }
    return;
  }
  const int px = awra_px(CELL_PAD_X), py = awra_px(CELL_PAD_Y);
  for (int c = 0; c < x->ncols; c++) {
    if (!b->cells[c]) {
      continue;
    }
    const Column *col = &x->cols[c];
    awra_allocate(b->cells[c], w->x + col->x + px, ry + py, BK_MAX(0, col->w - 2 * px),
                  BK_MAX(0, rh - 2 * py));
  }
}

static void update_window(AwraWidget *w)
{
  CollExt *x = CX(w);
  int first = 0, last = -1;
  if (x->n > 0 && x->tile_h > 0 && x->view_h > 0) {
    const int row0 = (int)(x->vvalue / x->tile_h);
    const int row1 = (int)((x->vvalue + x->view_h - 1) / x->tile_h);
    first = row0 * x->grid_cols;
    last = BK_MIN(x->n - 1, (row1 + 1) * x->grid_cols - 1);
  }
  const int nnew = last >= first ? last - first + 1 : 0;
  Bound *nv = (Bound *)calloc((size_t)BK_MAX(1, nnew), sizeof(Bound));
  /* keep what stays visible, unbind the rest */
  for (int k = 0; k < x->nvis; k++) {
    Bound *b = &x->vis[k];
    if (b->cells && b->index >= first && b->index <= last) {
      nv[b->index - first] = *b;
      b->cells = NULL;
    }
    else {
      unbind_one(w, b);
    }
  }
  free(x->vis);
  x->vis = nv;
  x->first = first;
  x->nvis = nnew;
  for (int k = 0; k < nnew; k++) {
    if (!nv[k].cells) {
      bind_one(w, &nv[k], first + k);
    }
    place_cells(w, &nv[k]);
  }
}

static void coll_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  CollExt *x = CX(w);
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    *r_min = awra_px(x->layout == AWRA_COLLECTION_GRID ? x->item_w + 2 * (TILE_PAD + TILE_MARGIN) : 0);
    *r_nat = *r_min;
  }
  else {
    *r_min = compute_header_h(x);
    *r_nat = *r_min;
  }
}

static void coll_allocate(AwraWidget *w)
{
  compute_geometry(w);
  update_window(w);
}

/* ----------------------------------------------------------------------- */
/* Painting */

static void paint_item_bg(AwraWidget *w, AwraDraw *d, int i)
{
  CollExt *x = CX(w);
  const AwraTokens *t = awra_tokens();
  const bool selected = i < x->sel_cap && x->sel[i];
  const bool hover = i == x->hover && awra_hovered(w);
  const bool drop = i == x->drop;
  int rx, ry, rw, rh;
  item_rect(w, i, &rx, &ry, &rw, &rh);
  AwraBox b = {(float)rx, (float)ry, (float)rw, (float)rh};
  const bool grid = x->layout == AWRA_COLLECTION_GRID;
  const float radius = awra_pxf(grid ? 8 : 4);
  if (selected || drop) {
    awra_paint_rbox(d, b, radius, &t->selection, 0, NULL);
    if (grid) {
      AwraRGBA line = awra_with_alpha(t->focus_ring, t->focus_ring.a * 0.28f);
      awra_paint_rbox(d, b, radius, NULL, awra_pxf(1), &line);
    }
  }
  else if (hover) {
    awra_paint_rbox(d, b, radius, grid ? &t->control_cap_hover : &t->control_cap_hover, 0, NULL);
  }
  if (drop) {
    awra_paint_rbox(d, b, awra_pxf(8), NULL, awra_pxf(2), &t->focus_ring);
  }
  if (i == x->cursor && awra_focus_visible(w)) {
    awra_paint_rbox(d, b, radius, NULL, awra_pxf(2), &t->focus_ring);
  }
}

static void coll_draw(AwraWidget *w, AwraDraw *d)
{
  CollExt *x = CX(w);
  bk_clip_push(w->x, w->y + x->header_h, w->w, x->view_h);
  for (int k = 0; k < x->nvis; k++) {
    paint_item_bg(w, d, x->vis[k].index);
  }
  bk_clip_pop();
  if (x->header_h > 0) {
    const AwraTokens *t = awra_tokens();
    AwraFont f = header_font();
    const float asc = awra_font_ascent(&f);
    const float lh = awra_font_line_height(&f);
    const int pt = awra_px(4), pb = awra_px(8);
    const float inner = (float)(x->header_h - pt - pb);
    for (int c = 0; c < x->ncols; c++) {
      const Column *col = &x->cols[c];
      if (!col->visible || col->w <= 0) {
        continue;
      }
      AwraBox hb = {(float)(w->x + col->x), (float)w->y, (float)col->w, (float)x->header_h};
      const bool hov = c == x->header_hover && awra_hovered(w);
      if (hov) {
        awra_paint_rbox(d, hb, awra_pxf(7), &t->control_cap_hover, 0, NULL);
      }
      AwraRGBA color = hov ? t->foreground : t->muted;
      const char *title = col->title ? col->title : "";
      const float tx = hb.x + awra_px(CELL_PAD_X);
      const float base = w->y + pt + floorf((inner - lh) * 0.5f + 0.5f) + asc;
      bk_clip_push((int)hb.x, (int)hb.y, (int)hb.w, (int)hb.h);
      awra_draw_text(d, &f, tx, base, title, (int)strlen(title), &color);
      if (c == x->sort_col) {
        const float tw = awra_text_width(&f, title, (int)strlen(title));
        const int s = awra_px(16);
        awra_paint_icon(d, x->sort_desc ? "pan-down-symbolic" : "pan-up-symbolic",
                        tx + tw + awra_px(6), w->y + pt + floorf((inner - s) * 0.5f), s, &color);
      }
      bk_clip_pop();
    }
  }
}

static void coll_draw_over(AwraWidget *w, AwraDraw *d)
{
  CollExt *x = CX(w);
  const AwraTokens *t = awra_tokens();
  if (x->band) {
    const double top = w->y + x->header_h - x->vvalue;
    const double bx0 = fmin(x->band_x0, x->band_x1), bx1 = fmax(x->band_x0, x->band_x1);
    const double by0 = fmin(x->band_y0, x->band_y1), by1 = fmax(x->band_y0, x->band_y1);
    AwraBox b = {(float)(w->x + bx0), (float)(top + by0), (float)(bx1 - bx0), (float)(by1 - by0)};
    AwraRGBA fill = awra_with_alpha(t->focus_ring, 0.15f);
    bk_clip_push(w->x, w->y + x->header_h, w->w, x->view_h);
    awra_paint_rbox(d, b, awra_pxf(6), &fill, awra_pxf(1), &t->focus_ring);
    bk_clip_pop();
  }
  /* the overlay indicator, as in a scrolled window */
  if (x->content_h > x->view_h && x->bar_t > 0.0f) {
    const float track = (float)x->view_h;
    float len = track * (float)x->view_h / (float)x->content_h;
    len = fmaxf(len, awra_pxf(18));
    const float pos = (float)(x->vvalue / (x->content_h - x->view_h)) * (track - len);
    AwraRGBA c = t->control_thumb;
    c.a *= x->bar_t;
    const float bw = awra_pxf(5);
    awra_paint_rbox(d, (AwraBox){w->x + w->w - awra_pxf(6), w->y + x->header_h + pos, bw, len},
                    bw * 0.5f, &c, 0, NULL);
  }
}

/* ----------------------------------------------------------------------- */
/* Scrolling */

static bool fade_tick(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  CollExt *x = CX(w);
  const double now = awra_now_ms();
  if (now < x->bar_until || x->press_scrollbar) {
    return true;
  }
  const float t = (float)(1.0 - (now - x->bar_until) / 1000.0);
  x->bar_t = t > 0 ? t : 0;
  BKContext *ctx = bk_ctx(w);
  if (ctx) {
    bk_ctx_dirty_rect(ctx, bk_rect(w->x + w->w - awra_px(8), w->y, awra_px(8), w->h));
  }
  if (x->bar_t <= 0.0f) {
    x->fade_timer = 0;
    return false;
  }
  return true;
}

static void show_bar(AwraWidget *w)
{
  CollExt *x = CX(w);
  x->bar_t = 1.0f;
  x->bar_until = awra_now_ms() + 2000.0;
  if (!x->fade_timer) {
    x->fade_timer = awra_timeout_add(33, fade_tick, w, NULL);
  }
}

static void scroll_to_value(AwraWidget *w, double v)
{
  CollExt *x = CX(w);
  const double maxv = BK_MAX(0, x->content_h - x->view_h);
  v = v < 0 ? 0 : v > maxv ? maxv : v;
  if (v == x->vvalue) {
    return;
  }
  x->vvalue = v;
  show_bar(w);
  awra_queue_allocate(w);
  awra_invalidate(w);
  awra_emit_simple(w, AWRA_SIGNAL_VALUE_CHANGED);
}

static int coll_scroll(AwraWidget *w, BKEvent *e)
{
  CollExt *x = CX(w);
  if (x->content_h <= x->view_h) {
    return 0;
  }
  const double step = pow((double)BK_MAX(1, x->view_h), 2.0 / 3.0);
  scroll_to_value(w, x->vvalue - e->wheel_y * step);
  return 1;
}

static void reveal_index(AwraWidget *w, int i)
{
  CollExt *x = CX(w);
  if (i < 0 || i >= x->n || x->tile_h <= 0) {
    return;
  }
  const int row = x->layout == AWRA_COLLECTION_GRID ? i / x->grid_cols : i;
  const double top = (double)row * x->tile_h, bottom = top + x->tile_h;
  if (top < x->vvalue) {
    scroll_to_value(w, top);
  }
  else if (bottom > x->vvalue + x->view_h) {
    scroll_to_value(w, bottom - x->view_h);
  }
}

/* ----------------------------------------------------------------------- */
/* Selection */

static void ensure_sel(CollExt *x)
{
  if (x->sel_cap < x->n) {
    const int cap = BK_MAX(x->n, 64);
    x->sel = (uint8_t *)realloc(x->sel, (size_t)cap);
    memset(x->sel + x->sel_cap, 0, (size_t)(cap - x->sel_cap));
    x->sel_cap = cap;
  }
}

static void set_sel(CollExt *x, int i, bool on)
{
  if (i < 0 || i >= x->n) {
    return;
  }
  ensure_sel(x);
  if ((bool)x->sel[i] != on) {
    x->sel[i] = on;
    x->sel_count += on ? 1 : -1;
  }
}

static void clear_sel(CollExt *x)
{
  if (x->sel) {
    memset(x->sel, 0, (size_t)x->sel_cap);
  }
  x->sel_count = 0;
}

static void coll_emit_selection(AwraWidget *w)
{
  awra_invalidate(w);
  awra_emit_text(w, AWRA_SIGNAL_NOTIFY, "selection");
}

static void move_cursor(AwraWidget *w, int to, int mods)
{
  CollExt *x = CX(w);
  if (x->n == 0) {
    return;
  }
  const int from = x->cursor;
  to = BK_CLAMP(to, 0, x->n - 1);
  x->cursor = to;
  if (mods & BK_SHIFT) {
    if (x->anchor < 0) {
      x->anchor = from >= 0 ? from : to;
    }
    const int a = x->anchor;
    if (!(mods & BK_CTRL)) {
      clear_sel(x);
    }
    for (int i = BK_MIN(a, to); i <= BK_MAX(a, to); i++) {
      set_sel(x, i, true);
    }
  }
  else if (!(mods & BK_CTRL)) {
    clear_sel(x);
    set_sel(x, to, true);
    x->anchor = to;
  }
  reveal_index(w, to);
  coll_emit_selection(w);
}

static void click_select(AwraWidget *w, int i, int mods)
{
  CollExt *x = CX(w);
  if (mods & BK_SHIFT) {
    move_cursor(w, i, mods);
    return;
  }
  if (mods & BK_CTRL) {
    ensure_sel(x);
    set_sel(x, i, !x->sel[i]);
    x->cursor = i;
    x->anchor = i;
    coll_emit_selection(w);
    return;
  }
  move_cursor(w, i, 0);
}

/* the rubberband: every item crossing the band, against the selection at
 * the press (Ctrl inverts them, Shift adds them, otherwise they replace it) */
static void band_apply(AwraWidget *w)
{
  CollExt *x = CX(w);
  ensure_sel(x);
  const double bx0 = fmin(x->band_x0, x->band_x1), bx1 = fmax(x->band_x0, x->band_x1);
  const double by0 = fmin(x->band_y0, x->band_y1), by1 = fmax(x->band_y0, x->band_y1);
  const bool ctrl = x->press_mods & BK_CTRL, shift = x->press_mods & BK_SHIFT;
  int count = 0;
  for (int i = 0; i < x->n; i++) {
    x->sel[i] = (ctrl || shift) && x->band_initial ? x->band_initial[i] : 0;
  }
  if (x->tile_h > 0 && x->n > 0) {
    const int row0 = BK_MAX(0, (int)(by0 / x->tile_h));
    const int row1 = (int)(by1 / x->tile_h);
    for (int row = row0; row <= row1; row++) {
      for (int col = 0; col < x->grid_cols; col++) {
        const int i = row * x->grid_cols + col;
        if (i >= x->n) {
          break;
        }
        int rx, ry, rw, rh;
        item_rect(w, i, &rx, &ry, &rw, &rh);
        /* to content coordinates */
        const double cx0 = rx - w->x, cy0 = ry - (w->y + x->header_h - x->vvalue);
        double ix0 = cx0, ix1 = cx0 + rw;
        if (x->layout == AWRA_COLLECTION_LIST) {
          /* a row is its name: the band must reach the first column's text */
          ix0 = 0;
          ix1 = x->ncols ? x->cols[0].w : rw;
        }
        if (ix1 >= bx0 && ix0 <= bx1 && cy0 + rh >= by0 && cy0 <= by1) {
          x->sel[i] = ctrl ? !x->sel[i] : 1;
        }
      }
    }
  }
  for (int i = 0; i < x->n; i++) {
    count += x->sel[i] != 0;
  }
  x->sel_count = count;
  coll_emit_selection(w);
}

static bool edge_tick(void *data)
{
  AwraWidget *w = (AwraWidget *)data;
  CollExt *x = CX(w);
  if (!x->band) {
    x->edge_timer = 0;
    return false;
  }
  const int top = w->y + x->header_h, bottom = w->y + w->h;
  double dv = 0;
  if (x->last_y < top + awra_px(EDGE_SCROLL)) {
    dv = -(top + awra_px(EDGE_SCROLL) - x->last_y) * 0.5;
  }
  else if (x->last_y > bottom - awra_px(EDGE_SCROLL)) {
    dv = (x->last_y - (bottom - awra_px(EDGE_SCROLL))) * 0.5;
  }
  if (dv != 0) {
    const double old = x->vvalue;
    scroll_to_value(w, x->vvalue + dv);
    if (x->vvalue != old) {
      x->band_y1 = x->last_y - (w->y + x->header_h) + x->vvalue;
      band_apply(w);
    }
  }
  return true;
}

/* ----------------------------------------------------------------------- */
/* Input */

static int header_column_at(AwraWidget *w, int px, int py, bool *r_edge)
{
  CollExt *x = CX(w);
  if (r_edge) {
    *r_edge = false;
  }
  if (x->header_h <= 0 || py < w->y || py >= w->y + x->header_h) {
    return -1;
  }
  for (int c = 0; c < x->ncols; c++) {
    const Column *col = &x->cols[c];
    if (!col->visible) {
      continue;
    }
    const int x0 = w->x + col->x, x1 = x0 + col->w;
    if (px >= x0 && px < x1) {
      if (r_edge) {
        *r_edge = col->resizable && px >= x1 - awra_px(4) && !col->expand;
      }
      return c;
    }
  }
  return -1;
}

static bool on_scrollbar(AwraWidget *w, int px, int py)
{
  CollExt *x = CX(w);
  return x->content_h > x->view_h && px >= w->x + w->w - awra_px(10) && py >= w->y + x->header_h;
}

static int coll_press(AwraWidget *w, BKEvent *e)
{
  CollExt *x = CX(w);
  BKContext *ctx = bk_ctx(w);
  x->press_dbl = ctx && ctx->last_click_node == NULL;
  x->press_mods = e->mods;
  x->press_x = e->x;
  x->press_y = e->y;
  x->press_v = x->vvalue;
  x->band = false;
  x->dnd = false;
  x->resize_col = -1;
  x->press_scrollbar = false;
  x->press_header = false;
  bool edge = false;
  const int hc = header_column_at(w, e->x, e->y, &edge);
  if (hc >= 0) {
    x->press_header = true;
    x->press_col = hc;
    if (edge) {
      x->resize_col = hc;
      x->resize_w0 = x->cols[hc].w;
    }
    return 1;
  }
  if (on_scrollbar(w, e->x, e->y)) {
    x->press_scrollbar = true;
    show_bar(w);
    return 1;
  }
  bool content = false;
  const int i = index_at(w, e->x, e->y, &content);
  x->press_index = i;
  x->press_content = content;
  x->press_selected = i >= 0 && i < x->sel_cap && x->sel[i];
  if (i >= 0 && x->press_dbl && !(e->mods & (BK_CTRL | BK_SHIFT))) {
    awra_emit_index(w, AWRA_SIGNAL_ACTIVATE, i);
    x->press_index = -2; /* consumed */
    return 1;
  }
  /* select at once, unless the press may start dragging the selection */
  if (i >= 0 && !(x->press_selected && !(e->mods & (BK_CTRL | BK_SHIFT)))) {
    click_select(w, i, e->mods);
  }
  return 1;
}

static int coll_drag(AwraWidget *w, BKEvent *e)
{
  CollExt *x = CX(w);
  x->last_x = e->x;
  x->last_y = e->y;
  if (x->press_header) {
    if (x->resize_col >= 0) {
      Column *col = &x->cols[x->resize_col];
      col->width = BK_MAX(40, (int)((x->resize_w0 + (e->x - x->press_x)) / awra_scale()));
      awra_queue_allocate(w);
      awra_invalidate(w);
    }
    return 1;
  }
  if (x->press_scrollbar) {
    const double track = x->view_h;
    const double ratio = x->content_h > 0 ? (double)x->content_h / track : 1;
    scroll_to_value(w, x->press_v + (e->y - x->press_y) * ratio);
    show_bar(w);
    return 1;
  }
  if (x->press_index == -2 || x->dnd) {
    return 1;
  }
  const int dx = e->x - x->press_x, dy = e->y - x->press_y;
  if (!x->band && dx * dx + dy * dy < awra_px(DRAG_THRESHOLD) * awra_px(DRAG_THRESHOLD)) {
    return 1;
  }
  if (!x->band && x->press_index >= 0 && x->press_content) {
    /* a drag from an item: the application carries it */
    x->dnd = true;
    if (!x->press_selected || (x->press_mods & (BK_CTRL | BK_SHIFT))) {
      /* the press selected it already */
    }
    AwraArgs a = {0};
    a.signal = AWRA_SIGNAL_DRAG_BEGIN;
    a.index = x->press_index;
    a.x = (x->press_x - w->x) / awra_scale();
    a.y = (x->press_y - w->y) / awra_scale();
    a.mods = x->press_mods & (BK_SHIFT | BK_CTRL | BK_ALT);
    awra_emit(w, &a);
    return 1;
  }
  if (!x->band) {
    x->band = true;
    ensure_sel(x);
    free(x->band_initial);
    x->band_initial = (uint8_t *)malloc((size_t)BK_MAX(1, x->n));
    if (x->band_initial && x->n) {
      memcpy(x->band_initial, x->sel, (size_t)x->n);
    }
    x->band_x0 = x->press_x - w->x;
    x->band_y0 = x->press_y - (w->y + x->header_h) + x->press_v;
    if (!x->edge_timer) {
      x->edge_timer = awra_timeout_add(16, edge_tick, w, NULL);
    }
  }
  x->band_x1 = BK_CLAMP(e->x - w->x, 0, w->w);
  x->band_y1 = BK_CLAMP(e->y - (w->y + x->header_h), 0, x->view_h) + x->vvalue;
  band_apply(w);
  return 1;
}

static int coll_release(AwraWidget *w, BKEvent *e)
{
  CollExt *x = CX(w);
  if (x->press_header) {
    x->press_header = false;
    if (x->resize_col < 0 && e->inside) {
      const int c = header_column_at(w, e->x, e->y, NULL);
      if (c >= 0 && c == x->press_col) {
        AwraArgs a = {0};
        a.signal = AWRA_SIGNAL_SELECTED;
        a.index = c;
        a.active = c == x->sort_col ? !x->sort_desc : false;
        awra_emit(w, &a);
      }
    }
    x->resize_col = -1;
    return 1;
  }
  if (x->press_scrollbar) {
    x->press_scrollbar = false;
    return 1;
  }
  if (x->band) {
    x->band = false;
    free(x->band_initial);
    x->band_initial = NULL;
    awra_invalidate(w);
    return 1;
  }
  if (x->dnd) {
    x->dnd = false;
    return 1;
  }
  if (x->press_index == -2) {
    return 1;
  }
  if (x->press_index >= 0) {
    if (x->press_selected && !(x->press_mods & (BK_CTRL | BK_SHIFT))) {
      click_select(w, x->press_index, 0);
    }
  }
  else if (!(x->press_mods & (BK_CTRL | BK_SHIFT)) && x->sel_count > 0) {
    clear_sel(x);
    coll_emit_selection(w);
  }
  return 1;
}

static int coll_motion(AwraWidget *w, BKEvent *e)
{
  CollExt *x = CX(w);
  int i = index_at(w, e->x, e->y, NULL);
  if (x->layout == AWRA_COLLECTION_GRID || on_scrollbar(w, e->x, e->y)) {
    bool content;
    i = on_scrollbar(w, e->x, e->y) ? -1 : index_at(w, e->x, e->y, &content);
  }
  const int hc = header_column_at(w, e->x, e->y, NULL);
  if (i != x->hover || hc != x->header_hover) {
    x->hover = i;
    x->header_hover = hc;
    awra_invalidate(w);
  }
  if (x->content_h > x->view_h && e->x >= w->x + w->w - awra_px(16)) {
    show_bar(w);
  }
  return 0;
}

static int coll_cursor(AwraWidget *w, int px, int py)
{
  bool edge = false;
  if (header_column_at(w, px, py, &edge) >= 0 && edge) {
    return BK_CURSOR_EW;
  }
  return -1;
}

static void coll_leave(AwraWidget *w, const AwraArgs *a, void *data)
{
  CollExt *x = CX(w);
  (void)a;
  (void)data;
  if (x->hover >= 0 || x->header_hover >= 0) {
    x->hover = -1;
    x->header_hover = -1;
    awra_invalidate(w);
  }
}

static int coll_key(AwraWidget *w, BKEvent *e)
{
  CollExt *x = CX(w);
  if (x->n == 0) {
    return 0;
  }
  const int cols = x->layout == AWRA_COLLECTION_GRID ? x->grid_cols : 1;
  const int page = x->tile_h > 0 ? BK_MAX(1, x->view_h / x->tile_h) * cols : cols;
  const int cur = x->cursor >= 0 ? x->cursor : -1;
  const int mods = e->mods & (BK_SHIFT | BK_CTRL);
  int to = -2;
  if (e->mods & BK_ALT) {
    return 0;
  }
  if (bk_keysym_is(e->keysym, "Down")) {
    to = cur < 0 ? 0 : (cur + cols < x->n ? cur + cols : cur);
  }
  else if (bk_keysym_is(e->keysym, "Up")) {
    to = cur < 0 ? 0 : (cur - cols >= 0 ? cur - cols : cur);
  }
  else if (bk_keysym_is(e->keysym, "Right") && cols > 1) {
    to = cur < 0 ? 0 : cur + 1;
  }
  else if (bk_keysym_is(e->keysym, "Left") && cols > 1) {
    to = cur < 0 ? 0 : cur - 1;
  }
  else if (bk_keysym_is(e->keysym, "Home")) {
    to = 0;
  }
  else if (bk_keysym_is(e->keysym, "End")) {
    to = x->n - 1;
  }
  else if (bk_keysym_is(e->keysym, "Page_Down") || bk_keysym_is(e->keysym, "Next")) {
    to = cur < 0 ? 0 : BK_MIN(x->n - 1, cur + page);
  }
  else if (bk_keysym_is(e->keysym, "Page_Up") || bk_keysym_is(e->keysym, "Prior")) {
    to = cur < 0 ? 0 : BK_MAX(0, cur - page);
  }
  else if ((bk_keysym_is(e->keysym, "Return") || bk_keysym_is(e->keysym, "KP_Enter")) && cur >= 0) {
    awra_emit_index(w, AWRA_SIGNAL_ACTIVATE, cur);
    return 1;
  }
  else if (bk_keysym_is(e->keysym, "space") && (mods & BK_CTRL) && cur >= 0) {
    ensure_sel(x);
    set_sel(x, cur, !x->sel[cur]);
    coll_emit_selection(w);
    return 1;
  }
  if (to == -2) {
    return 0;
  }
  awra_aux(w)->focus_visible = 1;
  move_cursor(w, to, mods);
  return 1;
}

static void coll_finalize(AwraWidget *w)
{
  CollExt *x = CX(w);
  for (int k = 0; k < x->nvis; k++) {
    free(x->vis[k].cells);
  }
  free(x->vis);
  for (int c = 0; c < x->ncols + 1 && x->pool; c++) {
    free(x->pool[c]);
    if (x->titles && x->titles[c]) {
      awra_text_layout_free(x->titles[c]);
    }
  }
  free(x->pool);
  free(x->npool);
  free(x->cappool);
  free(x->titles);
  for (int c = 0; c < x->ncols; c++) {
    free(x->cols[c].title);
  }
  free(x->cols);
  free(x->sel);
  free(x->band_initial);
  if (x->fade_timer) {
    awra_source_remove(x->fade_timer);
  }
  if (x->edge_timer) {
    awra_source_remove(x->edge_timer);
  }
  if (x->destroy) {
    x->destroy(x->data);
  }
}

/* GtkGridView / GtkColumnView: a grid whose bound cells are its cells
 * (only the items on screen, as GTK's), selected with their item, the
 * cursor's focused while the view has the focus */
static void coll_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  CollExt *x = CX(w);
  info->role = AWRA_ACCESSIBLE_ROLE_GRID;
  info->selection = true;
  info->flags |= AWRA_A11Y_MULTISELECTABLE;
  (void)x;
}

/* the item a bound cell shows (-1: none) */
static int cell_index(CollExt *x, AwraWidget *child, int *r_col)
{
  for (int k = 0; k < x->nvis; k++) {
    for (int c = 0; c < (x->ncols > 0 ? x->ncols : 1); c++) {
      if (x->vis[k].cells && x->vis[k].cells[c] == child) {
        *r_col = c;
        return x->vis[k].index;
      }
    }
  }
  return -1;
}

static void coll_a11y_child(AwraWidget *w, AwraWidget *child, AwraA11yInfo *info)
{
  CollExt *x = CX(w);
  int col = 0;
  const int i = cell_index(x, child, &col);
  if (i < 0) {
    return;
  }
  info->role = AWRA_ACCESSIBLE_ROLE_GRID_CELL;
  awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_SELECTED, i < x->sel_cap && x->sel[i]);
  info->flags |= AWRA_A11Y_FOCUSABLE;
  info->action = "activate";
  AwraWidget *win = awra_window_of(w);
  if (i == x->cursor && col == 0 && win && awra_window_focus(win) == w) {
    info->flags |= AWRA_A11Y_FOCUSED;
  }
}

static bool coll_a11y_child_do(AwraWidget *w, AwraWidget *child, int verb, double value)
{
  CollExt *x = CX(w);
  int col = 0;
  const int i = cell_index(x, child, &col);
  if (i < 0 || !awra_sensitive(w)) {
    return false;
  }
  switch (verb) {
    case AWRA_A11Y_DO_SELECT:
      awra_collection_view_set_selected(w, i, true);
      return true;
    case AWRA_A11Y_DO_DESELECT:
      awra_collection_view_set_selected(w, i, false);
      return true;
    case AWRA_A11Y_DO_ACTIVATE:
      awra_emit_index(w, AWRA_SIGNAL_ACTIVATE, i);
      return true;
    case AWRA_A11Y_DO_FOCUS:
      awra_collection_view_set_cursor(w, i);
      awra_widget_grab_focus(w);
      return true;
    case AWRA_A11Y_DO_SCROLL_TO:
      awra_collection_view_scroll_to(w, i);
      return true;
    default:
      return false;
  }
}

static AwraClass coll_class = {
    .a11y = coll_a11y,
    .a11y_child = coll_a11y_child,
    .a11y_child_do = coll_a11y_child_do,
    .bk = AWRA_BK("AwraCollectionView", CLS_INTERACTIVE),
    .type = "AwraCollectionView",
    .flags = AWRA_CLASS_CONTAINER | AWRA_CLASS_CLIP | AWRA_CLASS_FOCUSABLE | AWRA_CLASS_EXPAND_ISOLATED,
    .ext_size = sizeof(CollExt),
    .finalize = coll_finalize,
    .measure = coll_measure,
    .allocate = coll_allocate,
    .draw = coll_draw,
    .draw_over = coll_draw_over,
    .press = coll_press,
    .drag = coll_drag,
    .release = coll_release,
    .motion = coll_motion,
    .key = coll_key,
    .scroll = coll_scroll,
    .cursor = coll_cursor,
};

/* ----------------------------------------------------------------------- */
/* Public API */

AwraWidget *awra_collection_view_new(AwraCollectionLayout layout)
{
  AwraWidget *w = awra_widget_new(&coll_class);
  CollExt *x = CX(w);
  x->layout = layout;
  x->sort_col = -1;
  x->show_header = true;
  x->item_w = 108;
  x->item_h = 96;
  x->row_h = 30;
  x->max_cols = 7;
  x->cursor = -1;
  x->anchor = -1;
  x->hover = -1;
  x->drop = -1;
  x->header_hover = -1;
  x->resize_col = -1;
  ensure_pools(w);
  awra_widget_set_hexpand(w, true);
  awra_widget_set_vexpand(w, true);
  awra_signal_connect(w, AWRA_SIGNAL_LEAVE, coll_leave, NULL, NULL);
  return w;
}

void awra_collection_view_set_layout(AwraWidget *w, AwraCollectionLayout layout)
{
  CollExt *x = CX(w);
  if (x->layout == (int)layout) {
    return;
  }
  drop_pools(w);
  x->layout = layout;
  x->vvalue = 0;
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

AwraCollectionLayout awra_collection_view_layout(AwraWidget *w)
{
  return (AwraCollectionLayout)CX(w)->layout;
}

void awra_collection_view_set_factory(AwraWidget *w, AwraCollectionSetup setup,
                                      AwraCollectionBind bind, AwraCollectionUnbind unbind,
                                      void *data, AwraDestroyNotify destroy)
{
  CollExt *x = CX(w);
  drop_pools(w);
  if (x->destroy) {
    x->destroy(x->data);
  }
  x->setup = setup;
  x->bind = bind;
  x->unbind = unbind;
  x->data = data;
  x->destroy = destroy;
  awra_queue_allocate(w);
}

void awra_collection_view_set_item_count(AwraWidget *w, int n)
{
  CollExt *x = CX(w);
  unbind_all(w);
  x->n = BK_MAX(0, n);
  const bool had_selection = x->sel_count > 0;
  clear_sel(x);
  ensure_sel(x);
  x->cursor = -1;
  x->anchor = -1;
  x->hover = -1;
  x->drop = -1;
  x->vvalue = 0;
  awra_widget_queue_resize(w);
  awra_queue_allocate(w);
  awra_invalidate(w);
  if (had_selection) {
    coll_emit_selection(w);
  }
}

int awra_collection_view_item_count(AwraWidget *w)
{
  return CX(w)->n;
}

void awra_collection_view_items_changed(AwraWidget *w, int position, int removed, int added)
{
  CollExt *x = CX(w);
  position = BK_CLAMP(position, 0, x->n);
  removed = BK_CLAMP(removed, 0, x->n - position);
  added = BK_MAX(0, added);
  unbind_all(w);
  const int nn = x->n - removed + added;
  /* the selection follows the items that stay */
  ensure_sel(x);
  uint8_t *ns = (uint8_t *)calloc((size_t)BK_MAX(64, nn), 1);
  int count = 0;
  for (int i = 0; i < x->n; i++) {
    int j = i < position ? i : i < position + removed ? -1 : i - removed + added;
    if (j >= 0 && x->sel[i]) {
      ns[j] = 1;
      count++;
    }
  }
  const bool changed = count != x->sel_count;
  free(x->sel);
  x->sel = ns;
  x->sel_cap = BK_MAX(64, nn);
  x->sel_count = count;
  const int shift = added - removed;
  if (x->cursor >= position + removed) {
    x->cursor += shift;
  }
  else if (x->cursor >= position) {
    x->cursor = BK_MIN(position, nn - 1);
  }
  if (x->anchor >= position) {
    x->anchor = BK_CLAMP(x->anchor + shift, -1, nn - 1);
  }
  x->n = nn;
  x->hover = -1;
  x->drop = -1;
  awra_queue_allocate(w);
  awra_invalidate(w);
  if (changed) {
    coll_emit_selection(w);
  }
}

void awra_collection_view_rebind(AwraWidget *w, int index)
{
  CollExt *x = CX(w);
  for (int k = 0; k < x->nvis; k++) {
    Bound *b = &x->vis[k];
    if (b->cells && (index < 0 || b->index == index)) {
      const int i = b->index;
      unbind_one(w, b);
      bind_one(w, b, i);
      place_cells(w, b);
    }
  }
  awra_invalidate(w);
}

AwraWidget *awra_collection_view_cell(AwraWidget *w, int index, int column)
{
  CollExt *x = CX(w);
  if (index < x->first || index >= x->first + x->nvis) {
    return NULL;
  }
  Bound *b = &x->vis[index - x->first];
  if (!b->cells) {
    return NULL;
  }
  const int c = x->layout == AWRA_COLLECTION_GRID ? 0 : column;
  return c >= 0 && c < ncells(x) ? b->cells[c] : NULL;
}

void awra_collection_view_visible_range(AwraWidget *w, int *r_first, int *r_count)
{
  CollExt *x = CX(w);
  if (r_first) {
    *r_first = x->first;
  }
  if (r_count) {
    *r_count = x->nvis;
  }
}

void awra_collection_view_set_item_size(AwraWidget *w, int width, int height)
{
  CollExt *x = CX(w);
  if (x->item_w == width && x->item_h == height) {
    return;
  }
  x->item_w = width;
  x->item_h = height;
  awra_widget_queue_resize(w);
  awra_queue_allocate(w);
  awra_invalidate(w);
}

void awra_collection_view_set_row_height(AwraWidget *w, int height)
{
  CollExt *x = CX(w);
  if (x->row_h == height) {
    return;
  }
  /* keep the first visible row in place */
  const double ratio = x->tile_h > 0 ? x->vvalue / x->tile_h : 0;
  x->row_h = height;
  if (x->layout == AWRA_COLLECTION_LIST) {
    x->vvalue = ratio * (list_row_h(x) + awra_px(x->row_gap));
  }
  awra_queue_allocate(w);
  awra_invalidate(w);
}

void awra_collection_view_set_row_spacing(AwraWidget *w, int spacing)
{
  CollExt *x = CX(w);
  spacing = BK_MAX(0, spacing);
  if (x->row_gap == spacing) {
    return;
  }
  const double ratio = x->tile_h > 0 ? x->vvalue / x->tile_h : 0;
  x->row_gap = spacing;
  if (x->layout == AWRA_COLLECTION_LIST) {
    x->vvalue = ratio * (list_row_h(x) + awra_px(x->row_gap));
  }
  awra_queue_allocate(w);
  awra_invalidate(w);
}

void awra_collection_view_set_max_columns(AwraWidget *w, int n)
{
  CX(w)->max_cols = n;
  awra_queue_allocate(w);
}

int awra_collection_view_add_column(AwraWidget *w, const char *title, int width, bool expand)
{
  CollExt *x = CX(w);
  drop_pools(w);
  x->cols = (Column *)realloc(x->cols, sizeof(Column) * (size_t)(x->ncols + 1));
  Column *c = &x->cols[x->ncols];
  memset(c, 0, sizeof(*c));
  c->title = awra_strdup(title ? title : "");
  c->width = width;
  c->expand = expand;
  c->visible = true;
  c->resizable = true;
  x->ncols++;
  ensure_pools(w);
  awra_widget_queue_resize(w);
  awra_queue_allocate(w);
  return x->ncols - 1;
}

void awra_collection_view_set_column_title(AwraWidget *w, int column, const char *title)
{
  CollExt *x = CX(w);
  if (column < 0 || column >= x->ncols) {
    return;
  }
  awra_set_str(&x->cols[column].title, title ? title : "");
  awra_invalidate(w);
}

void awra_collection_view_set_column_visible(AwraWidget *w, int column, bool visible)
{
  CollExt *x = CX(w);
  if (column < 0 || column >= x->ncols || x->cols[column].visible == visible) {
    return;
  }
  x->cols[column].visible = visible;
  /* rows get (or lose) that cell */
  unbind_all(w);
  awra_queue_allocate(w);
  awra_invalidate(w);
}

bool awra_collection_view_column_visible(AwraWidget *w, int column)
{
  CollExt *x = CX(w);
  return column >= 0 && column < x->ncols && x->cols[column].visible;
}

void awra_collection_view_set_column_width(AwraWidget *w, int column, int width)
{
  CollExt *x = CX(w);
  if (column < 0 || column >= x->ncols) {
    return;
  }
  x->cols[column].width = width;
  awra_queue_allocate(w);
  awra_invalidate(w);
}

int awra_collection_view_column_width(AwraWidget *w, int column)
{
  CollExt *x = CX(w);
  if (column < 0 || column >= x->ncols) {
    return 0;
  }
  return (int)floorf(x->cols[column].w / awra_scale() + 0.5f);
}

void awra_collection_view_set_sort_column(AwraWidget *w, int column, bool descending)
{
  CollExt *x = CX(w);
  x->sort_col = column;
  x->sort_desc = descending;
  awra_invalidate(w);
}

void awra_collection_view_set_show_header(AwraWidget *w, bool show)
{
  CX(w)->show_header = show;
  awra_widget_queue_resize(w);
  awra_queue_allocate(w);
}

int awra_collection_view_header_height(AwraWidget *w)
{
  return (int)floorf(compute_header_h(CX(w)) / awra_scale() + 0.5f);
}

int awra_collection_view_column_at(AwraWidget *w, double x, double y)
{
  return header_column_at(w, w->x + awra_px(x), w->y + awra_px(y), NULL);
}

int awra_collection_view_index_at(AwraWidget *w, double x, double y, bool *r_on_content)
{
  bool content = false;
  const int i = index_at(w, w->x + awra_px(x), w->y + awra_px(y), &content);
  if (r_on_content) {
    *r_on_content = content;
  }
  return i;
}

bool awra_collection_view_item_bounds(AwraWidget *w, int index, double *r_x, double *r_y,
                                      double *r_w, double *r_h)
{
  CollExt *x = CX(w);
  if (index < 0 || index >= x->n) {
    return false;
  }
  int rx, ry, rw, rh;
  item_rect(w, index, &rx, &ry, &rw, &rh);
  const float s = awra_scale();
  *r_x = (rx - w->x) / s;
  *r_y = (ry - w->y) / s;
  *r_w = rw / s;
  *r_h = rh / s;
  return true;
}

bool awra_collection_view_is_selected(AwraWidget *w, int index)
{
  CollExt *x = CX(w);
  return index >= 0 && index < x->n && index < x->sel_cap && x->sel[index];
}

int awra_collection_view_selected_count(AwraWidget *w)
{
  return CX(w)->sel_count;
}

int awra_collection_view_get_selected(AwraWidget *w, int *out, int max)
{
  CollExt *x = CX(w);
  int k = 0;
  for (int i = 0; i < x->n && i < x->sel_cap && (max < 0 || k < max); i++) {
    if (x->sel[i]) {
      if (out) {
        out[k] = i;
      }
      k++;
    }
  }
  return k;
}

void awra_collection_view_set_selected(AwraWidget *w, int index, bool selected)
{
  CollExt *x = CX(w);
  if (index < 0 || index >= x->n) {
    return;
  }
  ensure_sel(x);
  if ((bool)x->sel[index] == selected) {
    return;
  }
  set_sel(x, index, selected);
  coll_emit_selection(w);
}

void awra_collection_view_select_only(AwraWidget *w, const int *indices, int n)
{
  CollExt *x = CX(w);
  clear_sel(x);
  ensure_sel(x);
  for (int k = 0; k < n; k++) {
    set_sel(x, indices[k], true);
  }
  if (n > 0 && indices[0] >= 0 && indices[0] < x->n) {
    x->cursor = indices[0];
    x->anchor = indices[0];
  }
  coll_emit_selection(w);
}

void awra_collection_view_select_all(AwraWidget *w)
{
  CollExt *x = CX(w);
  ensure_sel(x);
  for (int i = 0; i < x->n; i++) {
    x->sel[i] = 1;
  }
  x->sel_count = x->n;
  coll_emit_selection(w);
}

void awra_collection_view_unselect_all(AwraWidget *w)
{
  CollExt *x = CX(w);
  if (x->sel_count == 0) {
    return;
  }
  clear_sel(x);
  coll_emit_selection(w);
}

int awra_collection_view_cursor(AwraWidget *w)
{
  return CX(w)->cursor;
}

void awra_collection_view_set_cursor(AwraWidget *w, int index)
{
  CollExt *x = CX(w);
  x->cursor = index >= 0 && index < x->n ? index : -1;
  x->anchor = x->cursor;
  awra_invalidate(w);
}

void awra_collection_view_scroll_to(AwraWidget *w, int index)
{
  CollExt *x = CX(w);
  if (x->tile_h <= 0 || x->view_h <= 0) {
    /* not laid out yet: once it is */
    compute_geometry(w);
  }
  reveal_index(w, index);
}

double awra_collection_view_vvalue(AwraWidget *w)
{
  return CX(w)->vvalue / awra_scale();
}

void awra_collection_view_set_vvalue(AwraWidget *w, double value)
{
  CollExt *x = CX(w);
  x->vvalue = value * awra_scale();
  awra_queue_allocate(w);
  awra_invalidate(w);
}

void awra_collection_view_set_drop_index(AwraWidget *w, int index)
{
  CollExt *x = CX(w);
  if (x->drop == index) {
    return;
  }
  x->drop = index;
  awra_invalidate(w);
}

int awra_collection_view_grid_columns(AwraWidget *w)
{
  return CX(w)->grid_cols;
}
