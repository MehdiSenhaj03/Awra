/* Awra 2 list view (GtkListView over a list model, with a factory): rows
 * exist only for the items near the visible part of the scrolled window it
 * is in; the others count by their last measured height, or the average.
 *
 * The model lives with the application: it tells the view how many items
 * there are and which changed (awra_list_view_items_changed); the view asks
 * the factory to make item widgets (setup), fill them for a position (bind),
 * empty them (unbind) and forget them (teardown). Item widgets are reused.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"

#include <limits.h>

#define INVALID_POSITION UINT_MAX
/* rows bound beyond the visible part, above and below (raster px) */
#define OVERSCAN_PAGES 0.5
#define DEFAULT_ESTIMATE 40
#define POOL_MAX 8

typedef struct ItemExt {
  AwraWidget *child;
  AwraWidget *list;
  unsigned position; /* INVALID_POSITION: in the pool */
  bool activatable, selectable, focusable;
  int dy; /* drawn this far below its place (raster px): arrivals rise in */
} ItemExt;

#define ITX(w) AWRA_EXT(w, ItemExt)

typedef struct ListExt {
  unsigned n_items;
  int *heights; /* raster px at `measured_width`, -1 unknown */
  unsigned cap;
  int measured_width;
  long long known_sum;
  unsigned known_count;
  /* bound items, by position (sorted), and the pool */
  AwraWidget **bound;
  unsigned n_bound, cap_bound;
  AwraWidget *pool[POOL_MAX];
  int n_pool;
  /* factory */
  AwraListItemFunc setup, bind, unbind, teardown;
  void *data;
  AwraDestroyNotify destroy;
  bool single_click;
  int nat_width;
  /* scroll requests */
  unsigned scroll_pos;
  int scroll_align;
  bool in_allocate;
  int spacing; /* logical */
  int pad_top, pad_bottom; /* logical */
  /* the first item on screen and how far its top is from the visible top:
   * kept in place when items or heights above it change */
  unsigned anchor;
  long long anchor_delta;
  bool anchor_valid, anchor_apply;
  /* rows' look: 0 plain, 1 menu rows (completion: 30 px, 3/10 padding,
   * radius 9, the selected one on the accent's soft tint) */
  int row_style;
  unsigned selected;
  bool hover_selects;
} ListExt;

#define LVX(w) AWRA_EXT(w, ListExt)

/* ----------------------------------------------------------------------- */
/* Item widgets (GtkListItemWidget) */

static int item_style(const AwraWidget *w)
{
  const ItemExt *x = ITX(w);
  return x->list ? LVX(x->list)->row_style : 0;
}

static void item_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  ItemExt *x = ITX(w);
  *r_min = *r_nat = 0;
  const bool menu = item_style(w) == 1;
  const int ph = menu ? awra_px(20) : 0, pv = menu ? awra_px(6) + awra_px(2) : 0;
  const int pad = o == AWRA_ORIENTATION_HORIZONTAL ? ph : pv;
  const int other = o == AWRA_ORIENTATION_HORIZONTAL ? pv : ph;
  if (x->child) {
    awra_measure(x->child, o, for_size >= 0 ? BK_MAX(0, for_size - other) : -1, r_min, r_nat);
  }
  if (menu && o == AWRA_ORIENTATION_VERTICAL) {
    *r_min = BK_MAX(*r_min, awra_px(30) - awra_px(6));
    *r_nat = BK_MAX(*r_nat, awra_px(30) - awra_px(6));
  }
  *r_min += pad;
  *r_nat += pad;
}

static void item_allocate(AwraWidget *w)
{
  ItemExt *x = ITX(w);
  if (!x->child) {
    return;
  }
  if (item_style(w) == 1) {
    const int m = awra_px(1);
    awra_allocate(x->child, w->x + awra_px(10), w->y + m + awra_px(3), w->w - awra_px(20),
                  w->h - 2 * m - awra_px(6));
    return;
  }
  awra_allocate(x->child, w->x, w->y, w->w, w->h);
}

static void item_draw(AwraWidget *w, AwraDraw *d)
{
  ItemExt *x = ITX(w);
  if (item_style(w) != 1 || !x->list || LVX(x->list)->selected != x->position) {
    return;
  }
  const AwraTokens *t = awra_tokens();
  const float m = awra_pxf(1);
  const AwraBox b = {(float)w->x, w->y + m, (float)w->w, w->h - 2 * m};
  awra_paint_rbox(d, b, awra_pxf(9), &t->accent_soft, 0, NULL);
  const AwraRGBA ring = awra_with_alpha(t->accent, 0.22f);
  awra_paint_rbox(d, b, awra_pxf(9), NULL, awra_pxf(1), &ring);
}

static void item_hover(AwraWidget *w, bool on)
{
  ItemExt *x = ITX(w);
  if (on && x->list && LVX(x->list)->hover_selects && x->position != INVALID_POSITION) {
    awra_list_view_set_selected(x->list, x->position);
  }
}

static int item_release(AwraWidget *w, BKEvent *e)
{
  ItemExt *x = ITX(w);
  if (!e->inside || !x->activatable || !x->list || x->position == INVALID_POSITION) {
    return 0;
  }
  BKContext *ctx = bk_ctx(w);
  const bool dbl = ctx && ctx->last_click_node == NULL;
  if (LVX(x->list)->single_click || dbl) {
    awra_emit_index(x->list, AWRA_SIGNAL_ACTIVATE, (int)x->position);
  }
  return 1;
}

static void item_activate(AwraWidget *w)
{
  ItemExt *x = ITX(w);
  if (x->activatable && x->list && x->position != INVALID_POSITION) {
    awra_emit_index(x->list, AWRA_SIGNAL_ACTIVATE, (int)x->position);
  }
}

/* GtkListItemWidget: a list item, selected with the list's selection,
 * activatable */
static void item_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  ItemExt *x = ITX(w);
  if (!x->list || x->position == INVALID_POSITION) {
    return;
  }
  if (x->selectable || LVX(x->list)->row_style == 1) {
    awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_SELECTED, LVX(x->list)->selected == x->position);
  }
  if (x->activatable) {
    info->action = "activate";
  }
  if (x->focusable) {
    info->flags |= AWRA_A11Y_FOCUSABLE;
  }
}

static bool item_a11y_do(AwraWidget *w, int item, int verb, double value)
{
  ItemExt *x = ITX(w);
  if (item >= 0 || !x->list || x->position == INVALID_POSITION || !awra_sensitive(w)) {
    return false;
  }
  if (verb == AWRA_A11Y_DO_ACTIVATE && x->activatable) {
    item_activate(w);
    return true;
  }
  if (verb == AWRA_A11Y_DO_SELECT) {
    awra_list_view_set_selected(x->list, x->position);
    return true;
  }
  if (verb == AWRA_A11Y_DO_SCROLL_TO) {
    awra_list_view_scroll_to(x->list, x->position, 0);
    return true;
  }
  return false;
}

static AwraClass item_class = {
    .a11y = item_a11y,
    .a11y_do = item_a11y_do,
    .bk = AWRA_BK("AwraListItem", 0),
    .type = "AwraListItem",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(ItemExt),
    .measure = item_measure,
    .allocate = item_allocate,
    .draw = item_draw,
    .release = item_release,
    .activate = item_activate,
    .hover = item_hover,
};

void awra_list_item_set_child(AwraWidget *item, AwraWidget *child)
{
  awra_set_slot(item, &ITX(item)->child, child);
}

AwraWidget *awra_list_item_child(AwraWidget *item)
{
  return ITX(item)->child;
}

unsigned awra_list_item_position(AwraWidget *item)
{
  return ITX(item)->position;
}

void awra_list_item_set_activatable(AwraWidget *item, bool activatable)
{
  ITX(item)->activatable = activatable;
  if (activatable) {
    item->flags |= NF_EVENTS;
  }
  else {
    item->flags &= ~NF_EVENTS;
  }
}

void awra_list_item_set_selectable(AwraWidget *item, bool selectable)
{
  ITX(item)->selectable = selectable;
}

void awra_list_item_set_focusable(AwraWidget *item, bool focusable)
{
  ITX(item)->focusable = focusable;
}

/* ----------------------------------------------------------------------- */
/* Heights */

static int estimate(const ListExt *x)
{
  if (x->known_count) {
    return (int)(x->known_sum / x->known_count);
  }
  return awra_px(DEFAULT_ESTIMATE);
}

static int height_of(const ListExt *x, unsigned i)
{
  return x->heights[i] >= 0 ? x->heights[i] : estimate(x);
}

static void set_height(ListExt *x, unsigned i, int h)
{
  if (x->heights[i] >= 0) {
    x->known_sum -= x->heights[i];
    x->known_count--;
  }
  x->heights[i] = h;
  if (h >= 0) {
    x->known_sum += h;
    x->known_count++;
  }
}

static int gap(const ListExt *x)
{
  return awra_px(x->spacing);
}

static long long total_height(const ListExt *x)
{
  const long long pad = awra_px(x->pad_top) + awra_px(x->pad_bottom);
  if (!x->n_items) {
    return pad;
  }
  const long long unknown = x->n_items - x->known_count;
  return pad + x->known_sum + unknown * estimate(x) + (long long)gap(x) * (x->n_items - 1);
}

/* the offset of item i from the list's top */
static long long offset_of(const ListExt *x, unsigned i)
{
  long long y = awra_px(x->pad_top);
  const int g = gap(x);
  for (unsigned k = 0; k < i && k < x->n_items; k++) {
    y += height_of(x, k) + g;
  }
  return y;
}

static void forget_heights(ListExt *x)
{
  for (unsigned i = 0; i < x->n_items; i++) {
    x->heights[i] = -1;
  }
  x->known_sum = 0;
  x->known_count = 0;
}

/* ----------------------------------------------------------------------- */
/* Binding */

static long bound_index(const ListExt *x, unsigned pos)
{
  long lo = 0, hi = (long)x->n_bound - 1;
  while (lo <= hi) {
    const long mid = (lo + hi) / 2;
    const unsigned p = ITX(x->bound[mid])->position;
    if (p == pos) {
      return mid;
    }
    if (p < pos) {
      lo = mid + 1;
    }
    else {
      hi = mid - 1;
    }
  }
  return -1;
}

static AwraWidget *take_item(AwraWidget *list)
{
  ListExt *x = LVX(list);
  if (x->n_pool) {
    AwraWidget *it = x->pool[--x->n_pool];
    awra_widget_set_child_visible(it, true);
    return it;
  }
  AwraWidget *it = awra_widget_new(&item_class);
  ItemExt *ix = ITX(it);
  ix->list = list;
  ix->position = INVALID_POSITION;
  ix->focusable = true;
  ix->selectable = true;
  awra_child_insert(list, it, -1);
  awra_widget_unref(it); /* the list holds it */
  if (x->setup) {
    x->setup(list, it, x->data);
  }
  return it;
}

static void drop_item(AwraWidget *list, AwraWidget *it)
{
  ListExt *x = LVX(list);
  ItemExt *ix = ITX(it);
  if (ix->position != INVALID_POSITION && x->unbind) {
    x->unbind(list, it, x->data);
  }
  ix->position = INVALID_POSITION;
  /* a row reused for another item starts as rows do */
  ix->dy = 0;
  if (awra_aux(it)->opacity != 1.0f) {
    awra_widget_set_opacity(it, 1.0);
  }
  if (x->n_pool < POOL_MAX) {
    awra_widget_set_child_visible(it, false);
    x->pool[x->n_pool++] = it;
    return;
  }
  if (x->teardown) {
    x->teardown(list, it, x->data);
  }
  awra_child_remove(list, it);
}

static void bound_insert(ListExt *x, AwraWidget *it)
{
  if (x->n_bound == x->cap_bound) {
    x->cap_bound = x->cap_bound ? x->cap_bound * 2 : 32;
    x->bound = (AwraWidget **)realloc(x->bound, sizeof(AwraWidget *) * x->cap_bound);
  }
  const unsigned pos = ITX(it)->position;
  unsigned i = x->n_bound;
  while (i > 0 && ITX(x->bound[i - 1])->position > pos) {
    x->bound[i] = x->bound[i - 1];
    i--;
  }
  x->bound[i] = it;
  x->n_bound++;
}

static AwraWidget *bind_at(AwraWidget *list, unsigned pos)
{
  ListExt *x = LVX(list);
  const long bi = bound_index(x, pos);
  if (bi >= 0) {
    return x->bound[bi];
  }
  AwraWidget *it = take_item(list);
  ITX(it)->position = pos;
  bound_insert(x, it);
  if (x->bind) {
    x->bind(list, it, x->data);
  }
  return it;
}

/* A bound row drawn `dy` raster px below its place (0: in place); the
 * conversation view raises arrivals into place so. */
void awra_list_view_item_set_offset_internal(AwraWidget *item, int dy);
void awra_list_view_item_set_offset_internal(AwraWidget *item, int dy)
{
  if (ITX(item)->dy != dy) {
    ITX(item)->dy = dy;
    if (ITX(item)->list) {
      awra_queue_allocate(ITX(item)->list);
      awra_invalidate(ITX(item)->list);
    }
  }
}

static void unbind_index(AwraWidget *list, unsigned bi)
{
  ListExt *x = LVX(list);
  AwraWidget *it = x->bound[bi];
  memmove(x->bound + bi, x->bound + bi + 1, sizeof(AwraWidget *) * (x->n_bound - bi - 1));
  x->n_bound--;
  drop_item(list, it);
}

/* ----------------------------------------------------------------------- */
/* Layout */

static AwraWidget *scroller_of(AwraWidget *w)
{
  for (BKNode *p = w->parent; p; p = p->parent) {
    if (awra_is_widget(p) && (AWRA_CLASS_OF(p)->flags & AWRA_CLASS_CLIP) &&
        awra_widget_is(p, "AwraScrolledWindow"))
    {
      return p;
    }
  }
  return NULL;
}

static void list_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  ListExt *x = LVX(w);
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    int mn = 0, nt = x->nat_width;
    for (unsigned i = 0; i < x->n_bound; i++) {
      int cm, cn;
      awra_measure(x->bound[i], o, -1, &cm, &cn);
      mn = BK_MAX(mn, cm);
      nt = BK_MAX(nt, cn);
    }
    x->nat_width = nt;
    *r_min = mn;
    *r_nat = BK_MAX(mn, nt);
    return;
  }
  if (for_size >= 0 && for_size != x->measured_width) {
    /* another width: what was measured is an estimate now */
    x->measured_width = for_size;
    forget_heights(x);
    for (unsigned i = 0; i < x->n_bound; i++) {
      int cm, cn;
      awra_measure(x->bound[i], AWRA_ORIENTATION_VERTICAL, for_size, &cm, &cn);
      set_height(x, ITX(x->bound[i])->position, cm);
    }
  }
  const long long h = total_height(x);
  *r_min = *r_nat = (int)BK_MIN(h, (long long)INT_MAX / 2);
}

void awra_scrolled_shift_internal(AwraWidget *scrolled, double delta);

static void list_allocate(AwraWidget *w)
{
  ListExt *x = LVX(w);
  if (x->in_allocate) {
    return;
  }
  x->in_allocate = true;
  const int width = w->w;
  if (width != x->measured_width) {
    x->measured_width = width;
    forget_heights(x);
  }
  /* the part of the list that shows */
  AwraWidget *scroller = scroller_of(w);
  int vis_top, vis_bottom;
  if (scroller) {
    vis_top = scroller->y - w->y;
    vis_bottom = vis_top + scroller->h;
  }
  else {
    vis_top = 0;
    vis_bottom = w->h;
  }
  const int page = BK_MAX(1, vis_bottom - vis_top);
  /* items or heights changed: the anchor stays where it was on screen */
  if (x->anchor_apply && scroller && x->anchor_valid && x->anchor < x->n_items) {
    long long want = offset_of(x, x->anchor) - x->anchor_delta;
    if (want < 0) {
      want = 0;
    }
    if (want != vis_top) {
      awra_scrolled_shift_internal(scroller, (double)(want - vis_top));
      vis_top = (int)want;
      vis_bottom = vis_top + page;
    }
  }
  x->anchor_apply = false;
  const int margin = (int)(page * OVERSCAN_PAGES);
  const long long want_top = (long long)vis_top - margin, want_bottom = (long long)vis_bottom + margin;

  const int g = gap(x);
  unsigned first = 0;
  long long y = awra_px(x->pad_top);
  while (first < x->n_items && y + height_of(x, first) <= want_top) {
    y += height_of(x, first) + g;
    first++;
  }
  /* bind and measure what shows, from the first */
  unsigned last = first;
  long long ly = y;
  bool changed = false;
  for (unsigned i = first; i < x->n_items; i++) {
    AwraWidget *it = bind_at(w, i);
    int mn, nt;
    awra_measure(it, AWRA_ORIENTATION_VERTICAL, width, &mn, &nt);
    if (x->heights[i] != mn) {
      set_height(x, i, mn);
      changed = true;
    }
    last = i;
    ly += mn + g;
    if (ly >= want_bottom) {
      break;
    }
  }
  /* unbind the rest */
  for (unsigned bi = 0; bi < x->n_bound;) {
    const unsigned p = ITX(x->bound[bi])->position;
    if (p < first || p > last || x->n_items == 0) {
      unbind_index(w, bi);
      continue;
    }
    bi++;
  }
  /* place them */
  long long py = offset_of(x, first);
  for (unsigned bi = 0; bi < x->n_bound; bi++) {
    AwraWidget *it = x->bound[bi];
    const unsigned p = ITX(it)->position;
    const int h = height_of(x, p);
    awra_allocate(it, w->x, w->y + (int)py + ITX(it)->dy, width, h);
    py += h + g;
  }
  /* the anchor: the first item whose bottom shows */
  if (x->n_items) {
    unsigned a = 0;
    long long ay = awra_px(x->pad_top);
    while (a + 1 < x->n_items && ay + height_of(x, a) <= vis_top) {
      ay += height_of(x, a) + g;
      a++;
    }
    x->anchor = a;
    x->anchor_delta = ay - vis_top;
    x->anchor_valid = true;
  }
  else {
    x->anchor_valid = false;
  }
  x->in_allocate = false;
  if (changed) {
    x->anchor_apply = true;
    awra_widget_queue_resize(w);
  }
  /* a pending scroll to an item */
  if (x->scroll_pos != INVALID_POSITION && scroller && x->scroll_pos < x->n_items) {
    const unsigned pos = x->scroll_pos;
    const long long top = offset_of(x, pos);
    const int h = height_of(x, pos);
    double v = -1;
    const double cur = (double)(top - vis_top); /* the item's top in the viewport */
    switch (x->scroll_align) {
      case 1: v = (double)top; break;
      case 2: v = (double)top - (page - h) / 2.0; break;
      case 3: v = (double)top + h - page; break;
      default:
        if (cur < 0) {
          v = (double)top;
        }
        else if (cur + h > page) {
          v = (double)top + h - page;
        }
    }
    /* once the item's height is known, the request is done */
    if (x->heights[pos] >= 0 && !changed) {
      x->scroll_pos = INVALID_POSITION;
    }
    if (v >= 0 || x->scroll_align) {
      x->anchor_valid = false;
      awra_scrolled_shift_internal(scroller, BK_MAX(0.0, v) - vis_top);
    }
  }
}

static void list_finalize(AwraWidget *w)
{
  ListExt *x = LVX(w);
  if (x->destroy) {
    x->destroy(x->data);
  }
  free(x->heights);
  free(x->bound);
}

static void list_dispose_items(AwraWidget *w)
{
  ListExt *x = LVX(w);
  while (x->n_bound) {
    unbind_index(w, x->n_bound - 1);
  }
}

static void list_map(AwraWidget *w, bool mapped)
{
  if (!mapped) {
    return;
  }
  awra_queue_allocate(w);
}

static void list_a11y(AwraWidget *w, AwraA11yInfo *info)
{
  info->selection = true;
}

static AwraClass list_class = {
    .a11y = list_a11y,
    .bk = AWRA_BK("AwraListView", 0),
    .type = "AwraListView",
    .flags = AWRA_CLASS_CONTAINER,
    .ext_size = sizeof(ListExt),
    .finalize = list_finalize,
    .measure = list_measure,
    .allocate = list_allocate,
    .map = list_map,
};

AwraWidget *awra_list_view_new(void)
{
  AwraWidget *w = awra_widget_new(&list_class);
  ListExt *x = LVX(w);
  x->measured_width = -1;
  x->scroll_pos = INVALID_POSITION;
  x->selected = INVALID_POSITION;
  return w;
}

void awra_list_view_set_factory(AwraWidget *w, AwraListItemFunc setup, AwraListItemFunc bind,
                                AwraListItemFunc unbind, AwraListItemFunc teardown, void *data,
                                AwraDestroyNotify destroy)
{
  ListExt *x = LVX(w);
  list_dispose_items(w);
  while (x->n_pool) {
    AwraWidget *it = x->pool[--x->n_pool];
    if (x->teardown) {
      x->teardown(w, it, x->data);
    }
    awra_child_remove(w, it);
  }
  if (x->destroy) {
    x->destroy(x->data);
  }
  x->setup = setup;
  x->bind = bind;
  x->unbind = unbind;
  x->teardown = teardown;
  x->data = data;
  x->destroy = destroy;
  forget_heights(x);
  awra_widget_queue_resize(w);
}

void awra_list_view_items_changed(AwraWidget *w, unsigned position, unsigned removed, unsigned added)
{
  ListExt *x = LVX(w);
  if (position > x->n_items) {
    position = x->n_items;
  }
  if (position + removed > x->n_items) {
    removed = x->n_items - position;
  }
  /* rows of removed items go; rows after them move */
  for (unsigned bi = 0; bi < x->n_bound;) {
    ItemExt *ix = ITX(x->bound[bi]);
    if (ix->position >= position && ix->position < position + removed) {
      unbind_index(w, bi);
      continue;
    }
    if (ix->position >= position + removed) {
      ix->position = ix->position - removed + added;
    }
    bi++;
  }
  /* heights follow their items */
  for (unsigned i = position; i < position + removed; i++) {
    set_height(x, i, -1);
  }
  const unsigned n = x->n_items - removed + added;
  if (n > x->cap) {
    x->cap = BK_MAX(n, x->cap * 2);
    x->heights = (int *)realloc(x->heights, sizeof(int) * x->cap);
  }
  memmove(x->heights + position + added, x->heights + position + removed,
          sizeof(int) * (x->n_items - position - removed));
  for (unsigned i = position; i < position + added; i++) {
    x->heights[i] = -1;
  }
  x->n_items = n;
  if (x->selected != INVALID_POSITION) {
    if (x->selected >= position + removed) {
      x->selected = x->selected - removed + added;
    }
    else if (x->selected >= position) {
      x->selected = n ? BK_MIN(position, n - 1) : INVALID_POSITION;
    }
  }
  /* the anchor follows its item; a removed one gives way to the next */
  if (x->anchor_valid) {
    if (x->anchor >= position + removed) {
      x->anchor = x->anchor - removed + added;
    }
    else if (x->anchor >= position) {
      x->anchor = position + added < n ? position + added : (n ? n - 1 : 0);
      x->anchor_delta = 0;
    }
    x->anchor_apply = true;
  }
  if (x->scroll_pos != INVALID_POSITION && x->scroll_pos >= n) {
    x->scroll_pos = INVALID_POSITION;
  }
  awra_widget_queue_resize(w);
  awra_queue_allocate(w);
}

unsigned awra_list_view_n_items(AwraWidget *w)
{
  return LVX(w)->n_items;
}

void awra_list_view_rebind(AwraWidget *w, unsigned position)
{
  ListExt *x = LVX(w);
  const long bi = bound_index(x, position);
  if (bi < 0) {
    return;
  }
  AwraWidget *it = x->bound[bi];
  if (x->unbind) {
    x->unbind(w, it, x->data);
  }
  if (x->bind) {
    x->bind(w, it, x->data);
  }
  awra_widget_queue_resize(it);
}

AwraWidget *awra_list_view_item(AwraWidget *w, unsigned position)
{
  ListExt *x = LVX(w);
  const long bi = bound_index(x, position);
  return bi >= 0 ? x->bound[bi] : NULL;
}

int awra_list_view_bound_count(AwraWidget *w)
{
  return (int)LVX(w)->n_bound;
}

AwraWidget *awra_list_view_bound_item(AwraWidget *w, int index)
{
  ListExt *x = LVX(w);
  return index >= 0 && (unsigned)index < x->n_bound ? x->bound[index] : NULL;
}

bool awra_list_view_visible_range(AwraWidget *w, unsigned *r_first, unsigned *r_last)
{
  ListExt *x = LVX(w);
  AwraWidget *scroller = scroller_of(w);
  const int top = scroller ? scroller->y : w->y;
  const int bottom = scroller ? scroller->y + scroller->h : w->y + w->h;
  bool any = false;
  for (unsigned bi = 0; bi < x->n_bound; bi++) {
    AwraWidget *it = x->bound[bi];
    if (it->y + it->h <= top || it->y >= bottom) {
      continue;
    }
    if (!any) {
      *r_first = ITX(it)->position;
    }
    *r_last = ITX(it)->position;
    any = true;
  }
  return any;
}

void awra_list_view_scroll_to(AwraWidget *w, unsigned position, int align)
{
  ListExt *x = LVX(w);
  x->scroll_pos = position;
  x->scroll_align = align;
  awra_queue_allocate(w);
}

void awra_list_view_set_single_click_activate(AwraWidget *w, bool single)
{
  LVX(w)->single_click = single;
}

void awra_list_view_set_spacing(AwraWidget *w, int spacing)
{
  LVX(w)->spacing = spacing;
  awra_widget_queue_resize(w);
}

long long awra_list_view_offset_internal(AwraWidget *w, unsigned position);
long long awra_list_view_offset_internal(AwraWidget *w, unsigned position)
{
  return offset_of(LVX(w), position);
}

int awra_list_view_height_internal(AwraWidget *w, unsigned position);
int awra_list_view_height_internal(AwraWidget *w, unsigned position)
{
  return position < LVX(w)->n_items ? height_of(LVX(w), position) : 0;
}

void awra_list_view_set_padding(AwraWidget *w, int top, int bottom)
{
  LVX(w)->pad_top = top;
  LVX(w)->pad_bottom = bottom;
  awra_widget_queue_resize(w);
}

/* Forget the anchor: the next allocation keeps the scroll position as it is
 * (a view that pins its end itself). */
void awra_list_view_drop_anchor_internal(AwraWidget *w);
void awra_list_view_drop_anchor_internal(AwraWidget *w)
{
  LVX(w)->anchor_apply = false;
}

void awra_list_view_set_row_style(AwraWidget *w, int style)
{
  LVX(w)->row_style = style;
  LVX(w)->hover_selects = style == 1;
  awra_widget_queue_resize(w);
}

void awra_list_view_set_selected(AwraWidget *w, unsigned position)
{
  ListExt *x = LVX(w);
  if (position != INVALID_POSITION && position >= x->n_items) {
    position = INVALID_POSITION;
  }
  if (x->selected == position) {
    return;
  }
  x->selected = position;
  for (unsigned bi = 0; bi < x->n_bound; bi++) {
    awra_invalidate(x->bound[bi]);
  }
  if (position != INVALID_POSITION) {
    awra_list_view_scroll_to(w, position, 0);
  }
  awra_emit_index(w, AWRA_SIGNAL_SELECTED, position == INVALID_POSITION ? -1 : (int)position);
}

unsigned awra_list_view_selected(AwraWidget *w)
{
  return LVX(w)->selected;
}

/* ======================================================================= */
/* Data view (Awra 1's AwraDataView over a GtkColumnView): a header of
 * column titles over a list view whose rows hold a cell per column; each
 * column has its own factory. Its states: content, empty, loading, error. */

typedef struct Column {
  char *title;
  AwraListItemFunc setup, bind, unbind, teardown;
  void *data;
  AwraDestroyNotify destroy;
  bool visible, expand;
  AwraWidget *heading;
} Column;

typedef struct DataExt {
  AwraWidget *header, *stack, *scroller, *list, *empty, *loading, *error;
  AwraWidget *more, *more_spinner; /* "Loading more…" under the rows */
  Column *columns;
  int n_columns;
  int state;
} DataExt;

static DataExt *dvx(AwraWidget *w)
{
  return (DataExt *)awra_widget_get_data(w, "awra-data-view");
}

static void row_setup(AwraWidget *list, AwraWidget *item, void *data)
{
  AwraWidget *view = (AwraWidget *)data;
  DataExt *x = dvx(view);
  AwraWidget *row = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 12);
  awra_box_set_homogeneous(row, true);
  awra_widget_set_margins(row, 0, 12, 0, 12);
  for (int c = 0; c < x->n_columns; c++) {
    AwraWidget *cell = awra_widget_new(&item_class);
    ItemExt *ix = ITX(cell);
    ix->list = list;
    ix->position = INVALID_POSITION;
    awra_widget_set_hexpand(cell, true);
    awra_widget_set_visible(cell, x->columns[c].visible);
    awra_box_append(row, cell);
    awra_widget_unref(cell);
    if (x->columns[c].setup) {
      x->columns[c].setup(list, cell, x->columns[c].data);
    }
  }
  awra_list_item_set_child(item, row);
  awra_widget_unref(row);
}

static AwraWidget *row_cells(AwraWidget *item)
{
  return ITX(item)->child;
}

static void row_bind(AwraWidget *list, AwraWidget *item, void *data)
{
  AwraWidget *view = (AwraWidget *)data;
  DataExt *x = dvx(view);
  AwraWidget *row = row_cells(item);
  for (int c = 0; row && c < x->n_columns && c < row->nkids; c++) {
    AwraWidget *cell = row->kids[c];
    ITX(cell)->position = ITX(item)->position;
    awra_widget_set_visible(cell, x->columns[c].visible);
    if (x->columns[c].bind) {
      x->columns[c].bind(list, cell, x->columns[c].data);
    }
  }
}

static void row_unbind(AwraWidget *list, AwraWidget *item, void *data)
{
  AwraWidget *view = (AwraWidget *)data;
  DataExt *x = dvx(view);
  AwraWidget *row = row_cells(item);
  for (int c = 0; row && c < x->n_columns && c < row->nkids; c++) {
    if (x->columns[c].unbind) {
      x->columns[c].unbind(list, row->kids[c], x->columns[c].data);
    }
    ITX(row->kids[c])->position = INVALID_POSITION;
  }
}

static void data_free(void *p)
{
  DataExt *x = (DataExt *)p;
  for (int c = 0; c < x->n_columns; c++) {
    if (x->columns[c].destroy) {
      x->columns[c].destroy(x->columns[c].data);
    }
    free(x->columns[c].title);
  }
  free(x->columns);
  free(x);
}

static void data_sync_state(AwraWidget *w)
{
  DataExt *x = dvx(w);
  const char *name = x->state == AWRA_DATA_VIEW_EMPTY && x->empty     ? "empty" :
                     x->state == AWRA_DATA_VIEW_LOADING && x->loading ? "loading" :
                     x->state == AWRA_DATA_VIEW_ERROR && x->error     ? "error" :
                                                                        "content";
  awra_stack_set_visible_child_name(x->stack, name);
  awra_widget_set_visible(x->header, !strcmp(name, "content"));
}

AwraWidget *awra_data_view_new(void)
{
  AwraWidget *w = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  awra_widget_set_name(w, "AwraDataView");
  DataExt *x = (DataExt *)calloc(1, sizeof(DataExt));
  awra_widget_set_data(w, "awra-data-view", x, data_free);
  x->header = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 12);
  awra_box_set_homogeneous(x->header, true);
  awra_widget_set_margins(x->header, 6, 12, 6, 12);
  awra_box_append(w, x->header);
  awra_widget_unref(x->header);
  x->stack = awra_stack_new();
  awra_widget_set_vexpand(x->stack, true);
  x->scroller = awra_scrolled_new();
  awra_scrolled_set_policy(x->scroller, AWRA_POLICY_NEVER, AWRA_POLICY_AUTOMATIC);
  x->list = awra_list_view_new();
  awra_list_view_set_factory(x->list, row_setup, row_bind, row_unbind, NULL, w, NULL);
  awra_scrolled_set_child(x->scroller, x->list);
  awra_widget_unref(x->list);
  awra_stack_add_named(x->stack, x->scroller, "content");
  awra_widget_unref(x->scroller);
  awra_box_append(w, x->stack);
  awra_widget_unref(x->stack);
  /* Awra 1's loading-more row */
  x->more = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, awra_spacing(AWRA_SPACING_SM));
  awra_widget_set_halign(x->more, AWRA_ALIGN_CENTER);
  x->more_spinner = awra_spinner_new();
  awra_box_append(x->more, x->more_spinner);
  awra_widget_unref(x->more_spinner);
  AwraWidget *ml = awra_label_new("Loading more\xE2\x80\xA6");
  awra_widget_set_typography(ml, AWRA_TYPOGRAPHY_MUTED);
  awra_box_append(x->more, ml);
  awra_widget_unref(ml);
  awra_widget_set_visible(x->more, false);
  awra_box_append(w, x->more);
  awra_widget_unref(x->more);
  return w;
}

void awra_data_view_set_loading_more(AwraWidget *w, bool loading)
{
  DataExt *x = dvx(w);
  awra_widget_set_visible(x->more, loading);
  awra_spinner_set_spinning(x->more_spinner, loading);
}

AwraWidget *awra_data_view_list(AwraWidget *w)
{
  return dvx(w)->list;
}

int awra_data_view_add_column(AwraWidget *w, const char *title, AwraListItemFunc setup, AwraListItemFunc bind,
                              AwraListItemFunc unbind, AwraListItemFunc teardown, void *data,
                              AwraDestroyNotify destroy)
{
  DataExt *x = dvx(w);
  x->columns = (Column *)realloc(x->columns, sizeof(Column) * (size_t)(x->n_columns + 1));
  Column *c = &x->columns[x->n_columns];
  memset(c, 0, sizeof(*c));
  c->title = awra_strdup(title ? title : "");
  c->setup = setup;
  c->bind = bind;
  c->unbind = unbind;
  c->teardown = teardown;
  c->data = data;
  c->destroy = destroy;
  c->visible = true;
  c->heading = awra_label_new(c->title);
  awra_label_set_xalign(c->heading, 0.0f);
  awra_label_set_ellipsize(c->heading, AWRA_ELLIPSIZE_END);
  awra_widget_set_typography(c->heading, AWRA_TYPOGRAPHY_MUTED);
  awra_label_set_weight(c->heading, 600);
  awra_widget_set_hexpand(c->heading, true);
  awra_box_append(x->header, c->heading);
  awra_widget_unref(c->heading);
  /* rows made before this column go: they're made again */
  awra_list_view_set_factory(x->list, row_setup, row_bind, row_unbind, NULL, w, NULL);
  return x->n_columns++;
}

void awra_data_view_set_column_visible(AwraWidget *w, int column, bool visible)
{
  DataExt *x = dvx(w);
  if (column < 0 || column >= x->n_columns || x->columns[column].visible == visible) {
    return;
  }
  x->columns[column].visible = visible;
  awra_widget_set_visible(x->columns[column].heading, visible);
  ListExt *lx = LVX(x->list);
  for (unsigned b = 0; b < lx->n_bound; b++) {
    AwraWidget *row = row_cells(lx->bound[b]);
    if (row && column < row->nkids) {
      awra_widget_set_visible(row->kids[column], visible);
    }
  }
}

void awra_data_view_set_state(AwraWidget *w, AwraDataViewState state)
{
  dvx(w)->state = state;
  data_sync_state(w);
}

static void set_state_widget(AwraWidget *w, AwraWidget **slot, const char *name, AwraWidget *child)
{
  DataExt *x = dvx(w);
  if (*slot) {
    awra_container_remove(x->stack, *slot);
  }
  *slot = child;
  if (child) {
    awra_stack_add_named(x->stack, child, name);
  }
  data_sync_state(w);
}

void awra_data_view_set_empty_widget(AwraWidget *w, AwraWidget *child)
{
  set_state_widget(w, &dvx(w)->empty, "empty", child);
}

void awra_data_view_set_loading_widget(AwraWidget *w, AwraWidget *child)
{
  set_state_widget(w, &dvx(w)->loading, "loading", child);
}

void awra_data_view_set_error_widget(AwraWidget *w, AwraWidget *child)
{
  set_state_widget(w, &dvx(w)->error, "error", child);
}
