/* Awra 2 pickers: the emoji picker (Awra 1's AwraEmojiPicker) and the
 * colour palette (Awra 1's AwraColorPalette).
 *
 * The emoji picker draws its grid itself (no widget per emoji): a search
 * field and section tabs on top, sections of 40 px cells, a preview of the
 * one under the pointer at the foot. picked: args->text = the item's id.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include <ctype.h>
#include <stdio.h>

#define CELL 40
#define GRID_INSET 12
#define NATURAL_COLUMNS 9
#define HEADER_H 28

typedef struct Emoji {
  char *id, *name, *text, *keywords; /* keywords: lowercase, space-separated */
  AwraTexture *texture;
} Emoji;

typedef struct Section {
  char *id, *title, *icon;
  Emoji *items;
  int n;
  /* layout (raster px, in the grid) */
  int y, h;
  int n_shown;
  int *shown; /* indexes matching the search */
} Section;

typedef struct GridExt {
  AwraWidget *picker;
} GridExt;

typedef struct PickerExt {
  AwraWidget *box, *search, *tabs, *scroller, *grid, *preview, *preview_glyph, *preview_image,
      *preview_name, *empty;
  Section *sections;
  int n_sections;
  char *query;
  int hover_s, hover_i;
  int columns;
} PickerExt;

static PickerExt *pk(AwraWidget *picker)
{
  return (PickerExt *)awra_widget_get_data(picker, "awra-emoji-picker");
}

static int grid_columns(int width)
{
  return BK_MAX(1, (width - awra_px(GRID_INSET)) / awra_px(CELL));
}

static bool matches(const Emoji *e, const char *q)
{
  if (!q || !q[0]) {
    return true;
  }
  char low[256];
  int n = 0;
  for (const char *p = e->name; p && *p && n < 255; p++) {
    low[n++] = (char)tolower((unsigned char)*p);
  }
  low[n] = 0;
  if (strstr(low, q)) {
    return true;
  }
  return e->keywords && strstr(e->keywords, q);
}

static void filter(AwraWidget *picker)
{
  PickerExt *x = pk(picker);
  bool any = false;
  for (int s = 0; s < x->n_sections; s++) {
    Section *sec = &x->sections[s];
    free(sec->shown);
    sec->shown = (int *)malloc(sizeof(int) * (size_t)BK_MAX(1, sec->n));
    sec->n_shown = 0;
    for (int i = 0; i < sec->n; i++) {
      if (matches(&sec->items[i], x->query)) {
        sec->shown[sec->n_shown++] = i;
      }
    }
    any |= sec->n_shown > 0;
  }
  awra_widget_set_visible(x->empty, !any);
  awra_widget_queue_resize(x->grid);
}

static void grid_layout(AwraWidget *picker, int width)
{
  PickerExt *x = pk(picker);
  x->columns = grid_columns(width);
  int y = 0;
  for (int s = 0; s < x->n_sections; s++) {
    Section *sec = &x->sections[s];
    sec->y = y;
    if (sec->n_shown == 0) {
      sec->h = 0;
      continue;
    }
    const int rows = (sec->n_shown + x->columns - 1) / x->columns;
    sec->h = awra_px(HEADER_H) + rows * awra_px(CELL);
    y += sec->h;
  }
}

static void grid_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  AwraWidget *picker = AWRA_EXT(w, GridExt)->picker;
  PickerExt *x = pk(picker);
  if (o == AWRA_ORIENTATION_HORIZONTAL) {
    *r_min = awra_px(CELL * 5 + GRID_INSET);
    *r_nat = awra_px(CELL * NATURAL_COLUMNS + GRID_INSET);
    return;
  }
  grid_layout(picker, for_size >= 0 ? for_size : awra_px(CELL * NATURAL_COLUMNS + GRID_INSET));
  int h = 0;
  for (int s = 0; s < x->n_sections; s++) {
    h += x->sections[s].h;
  }
  *r_min = *r_nat = h + awra_px(6);
}

static void grid_allocate(AwraWidget *w)
{
  grid_layout(AWRA_EXT(w, GridExt)->picker, w->w);
}

static void grid_draw(AwraWidget *w, AwraDraw *d)
{
  AwraWidget *picker = AWRA_EXT(w, GridExt)->picker;
  PickerExt *x = pk(picker);
  const AwraTokens *t = awra_tokens();
  const int cell = awra_px(CELL), ox = w->x + awra_px(6);
  AwraFont title = awra_font_for(AWRA_TYPOGRAPHY_EYEBROW);
  AwraFont glyph = awra_font_for(AWRA_TYPOGRAPHY_BODY);
  glyph.size = awra_pxf(22);
  glyph.weight = 400;
  glyph.letter_spacing = 0;
  for (int s = 0; s < x->n_sections; s++) {
    Section *sec = &x->sections[s];
    if (!sec->n_shown) {
      continue;
    }
    const int sy = w->y + sec->y;
    BKRect band = bk_rect(w->x, sy, w->w, sec->h);
    if (!bk_clip_visible(&band)) {
      continue;
    }
    /* the section title: muted, 10 6 4 */
    awra_draw_text(d, &title, (float)ox + awra_px(6), sy + awra_px(10) + awra_font_ascent(&title), sec->title, -1,
                   &t->muted);
    const int gy = sy + awra_px(HEADER_H);
    for (int k = 0; k < sec->n_shown; k++) {
      const Emoji *e = &sec->items[sec->shown[k]];
      const int cx = ox + (k % x->columns) * cell, cy = gy + (k / x->columns) * cell;
      BKRect r = bk_rect(cx, cy, cell, cell);
      if (!bk_clip_visible(&r)) {
        continue;
      }
      const AwraBox cb = {(float)cx + awra_pxf(2), (float)cy + awra_pxf(2), (float)cell - awra_pxf(4),
                          (float)cell - awra_pxf(4)};
      if (x->hover_s == s && x->hover_i == k) {
        awra_paint_rbox(d, cb, awra_pxf(9), &t->control_cap_hover, 0, NULL);
      }
      if (e->texture) {
        const float m = awra_pxf(5);
        awra_paint_texture(d, e->texture, (AwraBox){cb.x + m, cb.y + m, cb.w - 2 * m, cb.h - 2 * m}, 0, 1);
      }
      else if (e->text) {
        const float tw = awra_text_width(&glyph, e->text, -1);
        const float lh = awra_font_line_height(&glyph), asc = awra_font_ascent(&glyph);
        awra_draw_text(d, &glyph, roundf(cb.x + (cb.w - tw) / 2), roundf(cb.y + (cb.h - lh) / 2 + asc), e->text, -1,
                       &t->foreground);
      }
    }
  }
}

static bool hit(AwraWidget *w, int px, int py, int *r_s, int *r_i)
{
  AwraWidget *picker = AWRA_EXT(w, GridExt)->picker;
  PickerExt *x = pk(picker);
  const int cell = awra_px(CELL), ox = w->x + awra_px(6);
  for (int s = 0; s < x->n_sections; s++) {
    Section *sec = &x->sections[s];
    const int gy = w->y + sec->y + awra_px(HEADER_H);
    if (!sec->n_shown || py < gy || py >= w->y + sec->y + sec->h) {
      continue;
    }
    const int col = (px - ox) / cell, row = (py - gy) / cell;
    if (px < ox || col >= x->columns) {
      return false;
    }
    const int k = row * x->columns + col;
    if (k < sec->n_shown) {
      *r_s = s;
      *r_i = k;
      return true;
    }
  }
  return false;
}

static void show_preview(AwraWidget *picker, const Emoji *e)
{
  PickerExt *x = pk(picker);
  if (!e) {
    awra_label_set_text(x->preview_glyph, "");
    awra_image_set_texture(x->preview_image, NULL);
    awra_widget_set_visible(x->preview_image, false);
    awra_label_set_text(x->preview_name, "");
    return;
  }
  awra_widget_set_visible(x->preview_image, e->texture != NULL);
  awra_widget_set_visible(x->preview_glyph, e->texture == NULL);
  awra_image_set_texture(x->preview_image, e->texture);
  awra_label_set_text(x->preview_glyph, e->texture ? "" : (e->text ? e->text : ""));
  awra_label_set_text(x->preview_name, e->name ? e->name : "");
}

static int grid_motion(AwraWidget *w, BKEvent *e)
{
  AwraWidget *picker = AWRA_EXT(w, GridExt)->picker;
  PickerExt *x = pk(picker);
  int s = -1, i = -1;
  hit(w, e->x, e->y, &s, &i);
  if (s != x->hover_s || i != x->hover_i) {
    x->hover_s = s;
    x->hover_i = i;
    awra_invalidate(w);
    show_preview(picker, s >= 0 ? &x->sections[s].items[x->sections[s].shown[i]] : NULL);
  }
  return 1;
}

static void grid_hover(AwraWidget *w, bool on)
{
  if (!on) {
    AwraWidget *picker = AWRA_EXT(w, GridExt)->picker;
    pk(picker)->hover_s = pk(picker)->hover_i = -1;
    awra_invalidate(w);
  }
}

static int grid_release(AwraWidget *w, BKEvent *e)
{
  AwraWidget *picker = AWRA_EXT(w, GridExt)->picker;
  PickerExt *x = pk(picker);
  int s, i;
  if (e->inside && hit(w, e->x, e->y, &s, &i)) {
    const Emoji *em = &x->sections[s].items[x->sections[s].shown[i]];
    char *id = awra_strdup(em->id);
    awra_widget_ref(picker);
    awra_emit_text(picker, AWRA_SIGNAL_ACTIVATE, id);
    awra_widget_unref(picker);
    free(id);
  }
  return 1;
}

static int grid_cursor(AwraWidget *w, int px, int py)
{
  int s, i;
  return hit(w, px, py, &s, &i) ? BK_CURSOR_HAND : -1;
}

static AwraClass grid_class = {
    .bk = AWRA_BK("AwraEmojiGrid", 0),
    .type = "AwraEmojiGrid",
    .ext_size = sizeof(GridExt),
    .measure = grid_measure,
    .allocate = grid_allocate,
    .draw = grid_draw,
    .motion = grid_motion,
    .hover = grid_hover,
    .release = grid_release,
    .cursor = grid_cursor,
};

static void free_section(Section *sec)
{
  for (int i = 0; i < sec->n; i++) {
    free(sec->items[i].id);
    free(sec->items[i].name);
    free(sec->items[i].text);
    free(sec->items[i].keywords);
    awra_texture_unref(sec->items[i].texture);
  }
  free(sec->items);
  free(sec->shown);
  free(sec->id);
  free(sec->title);
  free(sec->icon);
}

static void picker_free(void *p)
{
  PickerExt *x = (PickerExt *)p;
  for (int s = 0; s < x->n_sections; s++) {
    free_section(&x->sections[s]);
  }
  free(x->sections);
  free(x->query);
  free(x);
}

static void search_changed(AwraWidget *entry, const AwraArgs *a, void *data)
{
  (void)entry;
  AwraWidget *picker = (AwraWidget *)data;
  PickerExt *x = pk(picker);
  char *q = awra_strdup(a->text ? a->text : "");
  for (char *p = q; *p; p++) {
    *p = (char)tolower((unsigned char)*p);
  }
  free(x->query);
  x->query = q;
  filter(picker);
  awra_scrolled_set_vvalue(x->scroller, 0);
}

static void tab_clicked(AwraWidget *button, const AwraArgs *a, void *data)
{
  (void)a;
  AwraWidget *picker = (AwraWidget *)data;
  PickerExt *x = pk(picker);
  const int s = (int)(intptr_t)awra_widget_get_data(button, "awra-section");
  if (s >= 0 && s < x->n_sections) {
    awra_scrolled_set_vvalue(x->scroller, x->sections[s].y / awra_scale());
  }
}

/* the picker is a vertical box: header (search, tabs), grid, preview */
AwraWidget *awra_emoji_picker_new(void)
{
  AwraWidget *w = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  awra_widget_set_name(w, "AwraEmojiPicker");
  PickerExt *x = (PickerExt *)calloc(1, sizeof(PickerExt));
  x->hover_s = x->hover_i = -1;
  awra_widget_set_data(w, "awra-emoji-picker", x, picker_free);
  AwraWidget *header = awra_box_new(AWRA_ORIENTATION_VERTICAL, 6);
  awra_widget_set_margins(header, 10, 10, 4, 10);
  x->search = awra_search_entry_new();
  awra_entry_set_placeholder(x->search, "Search emoji");
  awra_search_entry_set_delay(x->search, 60);
  awra_signal_connect(x->search, AWRA_SIGNAL_SEARCH_CHANGED, search_changed, w, NULL);
  x->tabs = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 2);
  /* the tabs scroll sideways rather than widen the picker (Awra 1's
   * tabs_scroller, EXTERNAL / NEVER) */
  AwraWidget *tabs_scroller = awra_scrolled_new();
  awra_scrolled_set_policy(tabs_scroller, AWRA_POLICY_EXTERNAL, AWRA_POLICY_NEVER);
  awra_scrolled_set_child(tabs_scroller, x->tabs);
  awra_box_append(header, x->search);
  awra_box_append(header, tabs_scroller);
  awra_widget_unref(x->search);
  awra_widget_unref(x->tabs);
  awra_widget_unref(tabs_scroller);
  awra_box_append(w, header);
  awra_widget_unref(header);
  x->scroller = awra_scrolled_new();
  awra_scrolled_set_policy(x->scroller, AWRA_POLICY_NEVER, AWRA_POLICY_AUTOMATIC);
  awra_widget_set_vexpand(x->scroller, true);
  AwraWidget *overlay = awra_overlay_new();
  x->grid = awra_widget_new(&grid_class);
  AWRA_EXT(x->grid, GridExt)->picker = w;
  x->grid->flags |= NF_EVENTS;
  awra_scrolled_set_child(x->scroller, x->grid);
  awra_widget_unref(x->grid);
  awra_overlay_set_child(overlay, x->scroller);
  awra_widget_unref(x->scroller);
  x->empty = awra_label_new("No emoji found");
  awra_widget_set_typography(x->empty, AWRA_TYPOGRAPHY_MUTED);
  awra_widget_set_valign(x->empty, AWRA_ALIGN_CENTER);
  awra_widget_set_visible(x->empty, false);
  awra_overlay_add_overlay(overlay, x->empty);
  awra_widget_unref(x->empty);
  awra_box_append(w, overlay);
  awra_widget_unref(overlay);
  /* the preview: 44 px, 6 12, a line above */
  x->preview = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 10);
  awra_widget_set_size_request(x->preview, -1, 44);
  awra_widget_set_margins(x->preview, 6, 12, 6, 12);
  x->preview_glyph = awra_label_new("");
  awra_label_set_scale(x->preview_glyph, 30.0 / 15.0);
  x->preview_image = awra_image_new();
  awra_widget_set_size_request(x->preview_image, 32, 32);
  awra_widget_set_visible(x->preview_image, false);
  x->preview_name = awra_label_new("");
  awra_label_set_color(x->preview_name, &awra_tokens()->muted);
  awra_label_set_weight(x->preview_name, 600);
  awra_label_set_ellipsize(x->preview_name, AWRA_ELLIPSIZE_END);
  awra_box_append(x->preview, x->preview_glyph);
  awra_box_append(x->preview, x->preview_image);
  awra_box_append(x->preview, x->preview_name);
  awra_widget_unref(x->preview_glyph);
  awra_widget_unref(x->preview_image);
  awra_widget_unref(x->preview_name);
  AwraWidget *line = awra_separator_new(AWRA_ORIENTATION_HORIZONTAL);
  awra_box_append(w, line);
  awra_widget_unref(line);
  awra_box_append(w, x->preview);
  awra_widget_unref(x->preview);
  return w;
}

void awra_emoji_picker_clear(AwraWidget *w)
{
  PickerExt *x = pk(w);
  for (int s = 0; s < x->n_sections; s++) {
    free_section(&x->sections[s]);
  }
  free(x->sections);
  x->sections = NULL;
  x->n_sections = 0;
  x->hover_s = x->hover_i = -1;
  awra_container_remove_all(x->tabs);
  filter(w);
}

void awra_emoji_picker_add_section(AwraWidget *w, const char *id, const char *title, const char *icon,
                                   const AwraEmoji *items, int n)
{
  PickerExt *x = pk(w);
  x->sections = (Section *)realloc(x->sections, sizeof(Section) * (size_t)(x->n_sections + 1));
  Section *sec = &x->sections[x->n_sections];
  memset(sec, 0, sizeof(*sec));
  sec->id = awra_strdup(id);
  sec->title = awra_strdup(title);
  sec->icon = icon ? awra_strdup(icon) : NULL;
  sec->items = (Emoji *)calloc((size_t)BK_MAX(1, n), sizeof(Emoji));
  sec->n = n;
  for (int i = 0; i < n; i++) {
    Emoji *e = &sec->items[i];
    e->id = awra_strdup(items[i].id);
    e->name = awra_strdup(items[i].name);
    e->text = items[i].text ? awra_strdup(items[i].text) : NULL;
    e->texture = awra_texture_ref(items[i].texture);
    if (items[i].keywords) {
      e->keywords = awra_strdup(items[i].keywords);
      for (char *p = e->keywords; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
      }
    }
  }
  /* a tab: the section's icon, else its first emoji */
  AwraWidget *tab;
  if (icon) {
    tab = awra_button_new_from_icon_name(icon);
  }
  else {
    tab = awra_button_new_with_label(n > 0 && items[0].text ? items[0].text : "•");
  }
  awra_button_set_appearance(tab, AWRA_BUTTON_GHOST);
  awra_widget_set_size_request(tab, 26, 26);
  awra_widget_set_tooltip(tab, title);
  awra_widget_set_data(tab, "awra-section", (void *)(intptr_t)x->n_sections, NULL);
  awra_signal_connect(tab, AWRA_SIGNAL_CLICKED, tab_clicked, w, NULL);
  awra_box_append(x->tabs, tab);
  awra_widget_unref(tab);
  x->n_sections++;
  filter(w);
}

void awra_emoji_picker_set_search_text(AwraWidget *w, const char *text)
{
  awra_entry_set_text(pk(w)->search, text ? text : "");
}

void awra_emoji_picker_set_texture(AwraWidget *w, const char *id, AwraTexture *texture)
{
  PickerExt *x = pk(w);
  for (int s = 0; s < x->n_sections; s++) {
    for (int i = 0; i < x->sections[s].n; i++) {
      Emoji *e = &x->sections[s].items[i];
      if (!strcmp(e->id, id) && e->texture != texture) {
        awra_texture_ref(texture);
        awra_texture_unref(e->texture);
        e->texture = texture;
        awra_invalidate(x->grid);
      }
    }
  }
}

AwraWidget *awra_emoji_picker_search_entry(AwraWidget *w)
{
  return pk(w)->search;
}

/* ======================================================================= */
/* Colour palette: an automatic swatch, colours, and a custom one */

typedef struct Swatch {
  AwraRGBA color;
  char *name;
} Swatch;

typedef struct PaletteExt {
  AwraWidget *flow, *automatic, *custom, *popover, *entry;
  Swatch *colors;
  int n;
  bool has_selected, allows_custom;
  AwraRGBA selected;
} PaletteExt;

static PaletteExt *pl(AwraWidget *w)
{
  return (PaletteExt *)awra_widget_get_data(w, "awra-palette");
}

typedef struct DotExt {
  AwraWidget *palette;
  int index; /* -1 automatic, -2 custom */
} DotExt;

static void dot_measure(AwraWidget *w, int o, int f, int *mn, int *nt)
{
  (void)w;
  (void)o;
  (void)f;
  *mn = *nt = awra_px(24 + 6);
}

static bool same_color(const AwraRGBA *a, const AwraRGBA *b)
{
  return fabsf(a->r - b->r) < 0.004f && fabsf(a->g - b->g) < 0.004f && fabsf(a->b - b->b) < 0.004f;
}

static void dot_draw(AwraWidget *w, AwraDraw *d)
{
  DotExt *x = AWRA_EXT(w, DotExt);
  PaletteExt *p = pl(x->palette);
  const AwraTokens *t = awra_tokens();
  const AwraBox b = awra_box_of(w);
  if (awra_hovered(w)) {
    awra_paint_rbox(d, b, b.h / 2, &t->control_cap_hover, 0, NULL);
  }
  const float cx = b.x + b.w / 2, cy = b.y + b.h / 2, r = awra_pxf(12);
  bool selected = false;
  if (x->index >= 0) {
    awra_paint_circle(d, cx, cy, r, &p->colors[x->index].color);
    selected = p->has_selected && same_color(&p->selected, &p->colors[x->index].color);
  }
  else if (x->index == -1) {
    AwraRGBA c = t->accent;
    awra_paint_circle(d, cx, cy, r, &c);
    awra_paint_rbox(d, (AwraBox){cx - r, cy - r, 2 * r, 2 * r}, r, NULL, awra_pxf(1), &t->outline);
    selected = !p->has_selected;
  }
  else {
    awra_paint_rbox(d, (AwraBox){cx - r, cy - r, 2 * r, 2 * r}, r, NULL, awra_pxf(1.5), &t->muted);
    awra_paint_icon(d, "list-add-symbolic", cx - awra_pxf(8), cy - awra_pxf(8), awra_px(16), &t->muted);
    if (p->has_selected) {
      bool known = false;
      for (int i = 0; i < p->n; i++) {
        known |= same_color(&p->selected, &p->colors[i].color);
      }
      if (!known) {
        awra_paint_circle(d, cx, cy, r - awra_pxf(3), &p->selected);
        selected = true;
      }
    }
  }
  if (selected) {
    /* a ring in the foreground around the chosen one */
    awra_paint_rbox(d, (AwraBox){cx - r - awra_pxf(3), cy - r - awra_pxf(3), 2 * r + awra_pxf(6), 2 * r + awra_pxf(6)},
                    r + awra_pxf(3), NULL, awra_pxf(2), &t->foreground);
  }
  if (awra_focus_visible(w)) {
    awra_paint_rbox(d, b, b.h / 2, NULL, awra_pxf(2), &t->focus_ring);
  }
}

static void palette_chosen(AwraWidget *palette, const AwraRGBA *c)
{
  PaletteExt *p = pl(palette);
  p->has_selected = c != NULL;
  if (c) {
    p->selected = *c;
  }
  for (int i = 0; i < p->flow->nkids; i++) {
    awra_invalidate(p->flow->kids[i]);
  }
  AwraArgs a = {0};
  a.signal = AWRA_SIGNAL_SELECTED;
  a.index = c ? 1 : 0;
  char hex[16] = "";
  if (c) {
    snprintf(hex, sizeof(hex), "#%02x%02x%02x", (int)roundf(c->r * 255), (int)roundf(c->g * 255),
             (int)roundf(c->b * 255));
  }
  a.text = hex;
  awra_emit(palette, &a);
}

static void custom_activate(AwraWidget *entry, const AwraArgs *a, void *data)
{
  (void)entry;
  AwraWidget *palette = (AwraWidget *)data;
  AwraRGBA c;
  if (a->text && awra_parse_color(a->text, &c)) {
    awra_popover_popdown(pl(palette)->popover);
    palette_chosen(palette, &c);
  }
}

static int dot_release(AwraWidget *w, BKEvent *e)
{
  DotExt *x = AWRA_EXT(w, DotExt);
  if (!e->inside) {
    return 1;
  }
  PaletteExt *p = pl(x->palette);
  if (x->index >= 0) {
    AwraRGBA c = p->colors[x->index].color;
    palette_chosen(x->palette, &c);
  }
  else if (x->index == -1) {
    palette_chosen(x->palette, NULL);
  }
  else {
    if (!p->popover) {
      p->popover = awra_popover_new();
      AwraWidget *box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 8);
      AwraWidget *l = awra_label_new("A colour, as #rrggbb");
      awra_widget_set_typography(l, AWRA_TYPOGRAPHY_MUTED);
      p->entry = awra_entry_new();
      awra_entry_set_placeholder(p->entry, "#5865f2");
      awra_signal_connect(p->entry, AWRA_SIGNAL_ACTIVATE, custom_activate, x->palette, NULL);
      awra_box_append(box, l);
      awra_box_append(box, p->entry);
      awra_widget_unref(l);
      awra_widget_unref(p->entry);
      awra_popover_set_child(p->popover, box);
      awra_widget_unref(box);
      awra_popover_set_anchor(p->popover, w);
    }
    awra_popover_popup(p->popover);
  }
  return 1;
}

static void dot_activate(AwraWidget *w)
{
  BKEvent e;
  memset(&e, 0, sizeof(e));
  e.inside = true;
  dot_release(w, &e);
}

static AwraClass dot_class = {
    .bk = AWRA_BK("AwraColorSwatch", CLS_INTERACTIVE),
    .type = "AwraColorSwatch",
    .flags = AWRA_CLASS_HOVER | AWRA_CLASS_FOCUSABLE | AWRA_CLASS_ACTIVATABLE,
    .ext_size = sizeof(DotExt),
    .measure = dot_measure,
    .draw = dot_draw,
    .release = dot_release,
    .activate = dot_activate,
};

static AwraWidget *dot_new(AwraWidget *palette, int index, const char *tooltip)
{
  AwraWidget *d = awra_widget_new(&dot_class);
  AWRA_EXT(d, DotExt)->palette = palette;
  AWRA_EXT(d, DotExt)->index = index;
  awra_widget_set_tooltip(d, tooltip);
  return d;
}

static void palette_free(void *data)
{
  PaletteExt *p = (PaletteExt *)data;
  for (int i = 0; i < p->n; i++) {
    free(p->colors[i].name);
  }
  free(p->colors);
  if (p->popover) {
    awra_widget_unref(p->popover);
  }
  free(p);
}

static void palette_rebuild(AwraWidget *w)
{
  PaletteExt *p = pl(w);
  awra_container_remove_all(p->flow);
  if (p->automatic) {
    AwraWidget *a = dot_new(w, -1, awra_widget_get_data(w, "awra-automatic-label"));
    awra_flow_box_append(p->flow, a);
    awra_widget_unref(a);
  }
  for (int i = 0; i < p->n; i++) {
    AwraWidget *d = dot_new(w, i, p->colors[i].name);
    awra_flow_box_append(p->flow, d);
    awra_widget_unref(d);
  }
  if (p->allows_custom) {
    AwraWidget *c = dot_new(w, -2, "Another colour");
    awra_flow_box_append(p->flow, c);
    awra_widget_unref(c);
  }
}

AwraWidget *awra_color_palette_new(void)
{
  AwraWidget *w = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 0);
  awra_widget_set_name(w, "AwraColorPalette");
  PaletteExt *p = (PaletteExt *)calloc(1, sizeof(PaletteExt));
  awra_widget_set_data(w, "awra-palette", p, palette_free);
  p->flow = awra_flow_box_new();
  awra_flow_box_set_selection_mode(p->flow, AWRA_SELECTION_NONE);
  awra_flow_box_set_spacing(p->flow, 4, 4);
  awra_flow_box_set_columns(p->flow, 1, 16);
  awra_widget_set_hexpand(p->flow, true);
  awra_box_append(w, p->flow);
  awra_widget_unref(p->flow);
  return w;
}

void awra_color_palette_add_color(AwraWidget *w, const AwraRGBA *color, const char *name)
{
  PaletteExt *p = pl(w);
  p->colors = (Swatch *)realloc(p->colors, sizeof(Swatch) * (size_t)(p->n + 1));
  p->colors[p->n].color = *color;
  p->colors[p->n].name = name ? awra_strdup(name) : NULL;
  p->n++;
  palette_rebuild(w);
}

void awra_color_palette_clear(AwraWidget *w)
{
  PaletteExt *p = pl(w);
  for (int i = 0; i < p->n; i++) {
    free(p->colors[i].name);
  }
  p->n = 0;
  palette_rebuild(w);
}

void awra_color_palette_set_automatic_label(AwraWidget *w, const char *label)
{
  PaletteExt *p = pl(w);
  awra_widget_set_data(w, "awra-automatic-label", label ? awra_strdup(label) : NULL, free);
  p->automatic = label ? w : NULL;
  palette_rebuild(w);
}

void awra_color_palette_set_allows_custom(AwraWidget *w, bool allows)
{
  pl(w)->allows_custom = allows;
  palette_rebuild(w);
}

void awra_color_palette_set_selected(AwraWidget *w, const AwraRGBA *color)
{
  PaletteExt *p = pl(w);
  p->has_selected = color != NULL;
  if (color) {
    p->selected = *color;
  }
  for (int i = 0; i < p->flow->nkids; i++) {
    awra_invalidate(p->flow->kids[i]);
  }
}

bool awra_color_palette_selected(AwraWidget *w, AwraRGBA *r_color)
{
  PaletteExt *p = pl(w);
  if (p->has_selected && r_color) {
    *r_color = p->selected;
  }
  return p->has_selected;
}
