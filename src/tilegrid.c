/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Tile grid: children laid out as GtkGridView lays out its items
 * (gtkgridview.c): every column as wide as the widest child's minimum
 * allows, as many columns as fit (between the minimum and the maximum),
 * columns of (width + spacing) / n - spacing px placed at ceil'd
 * positions (the leftover stays at the end), and each row as tall as its
 * tallest child's minimum (GTK_SCROLL_MINIMUM, the default) or natural
 * height for the column width. A child then shares out what its row gives
 * beyond its minimum as GtkBox does: a still with a 190 x 118 request and
 * a 16:9 picture grows towards its natural height in a row made taller by
 * a neighbour. No virtualization: the children are the items. */

#include "awra-private.h"

#include <math.h>

typedef struct TileExt {
  int min_cols, max_cols;
  int xspacing, yspacing; /* logical px */
  bool natural_rows;      /* GTK_SCROLL_NATURAL */
} TileExt;

#define TX(w) AWRA_EXT(w, TileExt)

/* gtk_grid_view_measure_column_size */
static void tile_column_size(AwraWidget *g, int *r_min, int *r_nat)
{
  *r_min = *r_nat = 0;
  for (int i = 0; i < g->nkids; i++) {
    AwraWidget *c = g->kids[i];
    if (!awra_child_visible(c)) {
      continue;
    }
    int mn, nt;
    awra_measure(c, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nt);
    *r_min = BK_MAX(*r_min, mn);
    *r_nat = BK_MAX(*r_nat, nt);
  }
}

/* gtk_grid_view_compute_n_columns */
static int tile_n_columns(AwraWidget *g, int for_size, int min, int nat)
{
  const TileExt *x = TX(g);
  const int xs = awra_px(x->xspacing);
  /* the across scroll policy: minimum (nothing scrolls sideways) */
  int n = (for_size + xs) / BK_MAX(1, min + xs);
  (void)nat;
  n = BK_MAX(n, x->min_cols);
  n = BK_MIN(n, x->max_cols);
  return BK_MAX(1, n);
}

static int column_start(double cw, int xs, int col)
{
  return (int)ceil((cw + xs) * col);
}

static int column_end(double cw, int xs, int col)
{
  return (int)ceil(cw * (col + 1) + (double)xs * col);
}

/* The rows for a width: each row's height, the column width and count. */
static int tile_rows(AwraWidget *g, int width, int *heights, int max_rows, int *r_cw, int *r_n)
{
  const TileExt *x = TX(g);
  const int xs = awra_px(x->xspacing);
  int col_min, col_nat;
  tile_column_size(g, &col_min, &col_nat);
  width = BK_MAX(width, col_min * x->min_cols);
  const int n = tile_n_columns(g, width, col_min, col_nat);
  const int cw = BK_MAX((width + xs) / n - xs, col_min);
  int rows = 0, k = 0, row_h = 0;
  for (int i = 0; i < g->nkids; i++) {
    AwraWidget *c = g->kids[i];
    if (!awra_child_visible(c)) {
      continue;
    }
    int mn, nt;
    awra_measure(c, AWRA_ORIENTATION_VERTICAL, cw, &mn, &nt);
    row_h = BK_MAX(row_h, x->natural_rows ? nt : mn);
    if (++k == n) {
      if (rows < max_rows) {
        heights[rows] = row_h;
      }
      rows++;
      k = row_h = 0;
    }
  }
  if (k > 0) {
    if (rows < max_rows) {
      heights[rows] = row_h;
    }
    rows++;
  }
  *r_cw = cw;
  *r_n = n;
  return rows;
}

static void tile_measure(AwraWidget *g, int o, int for_size, int *r_min, int *r_nat)
{
  const TileExt *x = TX(g);
  if (o == AWRA_ORIENTATION_HORIZONTAL) { /* gtk_grid_view_measure_across */
    const int xs = awra_px(x->xspacing);
    int mn, nt;
    tile_column_size(g, &mn, &nt);
    *r_min = (mn + xs) * x->min_cols - xs;
    *r_nat = (nt + xs) * x->max_cols - xs;
    if (mn == 0 && nt == 0) {
      *r_min = *r_nat = 0;
    }
    return;
  }
  const int ys = awra_px(x->yspacing);
  const int max_rows = g->nkids + 1;
  int *heights = (int *)malloc(sizeof(int) * (size_t)max_rows);
  int cw, n;
  const int rows = heights ? tile_rows(g, for_size < 0 ? 0 : for_size, heights, max_rows, &cw, &n) : 0;
  int h = 0;
  for (int r = 0; r < rows; r++) {
    h += heights[r] + ys;
  }
  if (h) {
    h -= ys;
  }
  free(heights);
  *r_min = *r_nat = h;
}

static void tile_allocate(AwraWidget *g)
{
  const TileExt *x = TX(g);
  const int xs = awra_px(x->xspacing), ys = awra_px(x->yspacing);
  const int max_rows = g->nkids + 1;
  int *heights = (int *)malloc(sizeof(int) * (size_t)max_rows);
  if (!heights) {
    return;
  }
  int cw, n;
  const int rows = tile_rows(g, g->w, heights, max_rows, &cw, &n);
  /* gtk_grid_view_size_allocate: a row is at least a 30th of the view */
  const int min_row = (int)ceil((double)g->h / 30.0);
  int y = g->y, k = 0, r = 0;
  for (int i = 0; i < g->nkids && r < rows; i++) {
    AwraWidget *c = g->kids[i];
    if (!awra_child_visible(c)) {
      continue;
    }
    const int rh = BK_MAX(heights[r], min_row);
    const int x0 = column_start(cw, xs, k);
    awra_allocate(c, g->x + x0, y, column_end(cw, xs, k) - x0, rh);
    if (++k == n) {
      y += rh + ys;
      k = 0;
      r++;
    }
  }
  free(heights);
}

static AwraClass tile_class = {
    .bk = AWRA_BK("AwraTileGrid", 0),
    .type = "AwraTileGrid",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(TileExt),
    .measure = tile_measure,
    .allocate = tile_allocate,
};

AwraWidget *awra_tile_grid_new(void)
{
  AwraWidget *w = awra_widget_new(&tile_class);
  TileExt *x = TX(w);
  x->min_cols = 1;  /* GtkGridView's defaults */
  x->max_cols = 7;
  return w;
}

void awra_tile_grid_append(AwraWidget *grid, AwraWidget *child)
{
  awra_child_insert(grid, child, -1);
}

void awra_tile_grid_insert(AwraWidget *grid, AwraWidget *child, int position)
{
  awra_child_insert(grid, child, position);
}

void awra_tile_grid_set_columns(AwraWidget *grid, int min_columns, int max_columns)
{
  TileExt *x = TX(grid);
  x->min_cols = BK_MAX(1, min_columns);
  x->max_cols = BK_MAX(x->min_cols, max_columns);
  awra_widget_queue_resize(grid);
}

void awra_tile_grid_set_spacing(AwraWidget *grid, int row_spacing, int column_spacing)
{
  TileExt *x = TX(grid);
  x->yspacing = row_spacing;
  x->xspacing = column_spacing;
  awra_widget_queue_resize(grid);
}

void awra_tile_grid_set_natural_rows(AwraWidget *grid, bool natural)
{
  TX(grid)->natural_rows = natural;
  awra_widget_queue_resize(grid);
}

int awra_tile_grid_columns(AwraWidget *grid)
{
  int cw, n;
  int h[1];
  tile_rows(grid, grid->w, h, 0, &cw, &n);
  return n;
}
