/* Awra 2 internals: how components sit on the BUI kit.
 *
 * Every Awra widget is a BKNode whose class is an AwraClass (a BKClass as
 * its first member, so the kit dispatches input, drawing and layout to Awra's
 * generic hooks) and whose `aux` holds an AwraAux: reference count, layout
 * properties, the measurement cache, signal handlers, hover/press state.
 *
 * Layout is GTK's: measure(orientation, for_size) -> minimum and natural
 * sizes (height-for-width for wrapping text), then allocate(rect). It is
 * incremental: queue_resize clears the caches of a widget and its
 * ancestors only; allocation skips subtrees whose rect and contents didn't
 * change, and damages exactly what moved (the kit's root_fills mode).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "awra/awra.h"
#include "kit_intern.h"

/* ----------------------------------------------------------------------- */
/* Classes */

enum {
  AWRA_CLASS_CONTAINER = 1 << 0, /* children are Awra widgets it lays out */
  AWRA_CLASS_HOVER = 1 << 1,     /* repaints on hover / press changes */
  AWRA_CLASS_CLIP = 1 << 2,      /* clips its children to its rect */
  AWRA_CLASS_FOCUSABLE = 1 << 3, /* takes keyboard focus (Tab) */
  AWRA_CLASS_ACTIVATABLE = 1 << 4, /* Return / Space activate it (clicked) */
  AWRA_CLASS_TEXT_INPUT = 1 << 5,  /* has the text cursor and key focus when clicked */
  AWRA_CLASS_POPUP = 1 << 6,       /* shown above the window content (kit popups) */
  /* expand is not computed from the children (a GtkWidget subclass without
   * compute_expand, as Awra 1's components were) */
  AWRA_CLASS_EXPAND_ISOLATED = 1 << 7,
};

typedef struct AwraClass AwraClass;
typedef struct AwraDraw AwraDraw;

struct AwraClass {
  BKClass bk; /* first: node->cls points here */
  const char *type;
  unsigned flags;
  size_t ext_size; /* zeroed `ext` of this size, freed with the widget */
  const AwraClass *parent_class; /* for awra_widget_is() on subclasses */
  void (*init)(AwraWidget *w);
  void (*finalize)(AwraWidget *w);
  /* content size without margins, in raster pixels; for_size -1 = any */
  void (*measure)(AwraWidget *w, int orientation, int for_size, int *r_min, int *r_nat);
  /* place children inside w's rect (w->x, y, w, h already set) */
  void (*allocate)(AwraWidget *w);
  /* paint below the children / above them */
  void (*draw)(AwraWidget *w, AwraDraw *d);
  void (*draw_over)(AwraWidget *w, AwraDraw *d);
  void (*style_changed)(AwraWidget *w);
  /* containers */
  void (*add)(AwraWidget *w, AwraWidget *child);
  void (*remove)(AwraWidget *w, AwraWidget *child);
  /* input (logical event: positions in raster pixels, window-relative) */
  int (*press)(AwraWidget *w, BKEvent *e);
  int (*release)(AwraWidget *w, BKEvent *e);
  int (*drag)(AwraWidget *w, BKEvent *e);
  int (*motion)(AwraWidget *w, BKEvent *e);
  int (*key)(AwraWidget *w, BKEvent *e);
  int (*scroll)(AwraWidget *w, BKEvent *e);
  void (*activate)(AwraWidget *w); /* Return / Space / mnemonic */
  int (*cursor)(AwraWidget *w, int x, int y); /* BK_CURSOR_* or -1 */
  /* generic properties */
  bool (*set_string)(AwraWidget *w, const char *name, const char *value);
  bool (*set_double)(AwraWidget *w, const char *name, double value);
  const char *(*get_string)(AwraWidget *w, const char *name);
  bool (*get_double)(AwraWidget *w, const char *name, double *r_value);
  void (*map)(AwraWidget *w, bool mapped);
  /* text style for descendants (labels and icons inherit color and weight
   * like CSS `color: inherit`); false: ask further up */
  bool (*child_style)(AwraWidget *w, AwraRGBA *r_color, int *r_weight);
  int overdraw; /* logical px painted outside the rect (shadows, focus ring) */
  /* the pointer entered or left it */
  void (*hover)(AwraWidget *w, bool on);
  /* GtkSizeRequestMode: 1 height-for-width, 0 constant size; NULL: from
   * the children (height-for-width when one is) */
  int (*request_mode)(AwraWidget *w);
};

/* Declares the kit side of an Awra class: every hook goes to Awra's
 * generic dispatchers (core.c). */
extern const BKClass awra_bk_template;
#define AWRA_BK(name_, kit_flags_)                                                   \
  {                                                                                 \
    .name = (name_), .flags = (kit_flags_), .measure = awra_bk_measure,             \
    .arrange = awra_bk_arrange, .draw_tree = awra_bk_draw_tree, .hit = awra_bk_hit, \
    .press = awra_bk_press, .dbl = awra_bk_press, .drag = awra_bk_drag,             \
    .release = awra_bk_release, .move = awra_bk_move, .leave = awra_bk_leave,       \
    .key = awra_bk_key, .cursor_at = awra_bk_cursor_at, .free_ext = awra_bk_free,   \
    .finish_edit = awra_bk_finish_edit, .scroll = awra_bk_scroll,                   \
  }

void awra_bk_measure(BKNode *n, int *r_w, int *r_h);
void awra_bk_arrange(BKNode *n);
void awra_bk_draw_tree(BKNode *n);
BKNode *awra_bk_hit(BKNode *n, int x, int y);
int awra_bk_press(BKNode *n, BKEvent *e);
int awra_bk_drag(BKNode *n, BKEvent *e);
int awra_bk_release(BKNode *n, BKEvent *e);
int awra_bk_move(BKNode *n, BKEvent *e);
int awra_bk_leave(BKNode *n);
int awra_bk_key(BKNode *n, BKEvent *e);
int awra_bk_cursor_at(BKNode *n, int x, int y);
void awra_bk_free(BKNode *n);
void awra_bk_finish_edit(BKNode *n, bool confirm);
int awra_bk_scroll(BKNode *n, BKEvent *e);

#define AWRA_CLASS_OF(w) ((const AwraClass *)(w)->cls)
#define AWRA_EXT(w, type) ((type *)(w)->ext)

/* ----------------------------------------------------------------------- */
/* Per-widget data */

typedef struct AwraHandlerEntry {
  unsigned long id;
  AwraSignal signal;
  AwraHandler fn;
  void *data;
  AwraDestroyNotify destroy;
  bool blocked;
  struct AwraHandlerEntry *next;
} AwraHandlerEntry;

typedef struct AwraDataEntry {
  char *key;
  void *data;
  AwraDestroyNotify destroy;
  struct AwraDataEntry *next;
} AwraDataEntry;

typedef struct AwraAux {
  int refs;
  /* layout, logical pixels */
  short margin[4]; /* top end bottom start */
  short req_w, req_h; /* size request, -1 none */
  unsigned char halign, valign;
  unsigned hexpand : 1, vexpand : 1, hexpand_set : 1, vexpand_set : 1;
  unsigned can_target : 1, focusable_set : 1, focusable : 1;
  unsigned hovered : 1, pressed : 1, focus_visible : 1, mapped : 1, destroyed : 1;
  unsigned typography_set : 1, in_dispose : 1, expand_isolated : 1;
  /* GTK's visible (the application's) and child-visible (the parent's):
   * the node's NF_VISIBLE is set when both are */
  unsigned user_hidden : 1, child_hidden : 1, overflow_hidden : 1;
  unsigned char typography;
  float opacity;
  /* measurement cache: [orientation] for one for_size each */
  int cache_for[2], cache_min[2], cache_nat[2];
  unsigned cache_valid : 2;
  unsigned expand_valid : 1, computed_hexpand : 1, computed_vexpand : 1;
  unsigned mode_valid : 1, mode_hfw : 1; /* request mode cache */
  unsigned needs_allocate : 1;
  int alloc_w, alloc_h; /* last allocated size (resize signal) */
  /* state animations, 0..1 */
  float hover_t, press_t;
  /* style classes, small fixed set of interned names */
  uint64_t classes;
  int cursor; /* BK_CURSOR_* + 1, 0 = class default */
  char *name;
  AwraHandlerEntry *handlers;
  unsigned long handler_mask; /* bit per signal with handlers */
  AwraDataEntry *data;
} AwraAux;

static inline AwraAux *awra_aux(const AwraWidget *w)
{
  return (AwraAux *)w->aux;
}
bool awra_is_widget(const BKNode *n); /* an Awra widget (has the aux) */
/* Composites standing for Awra 1's GtkWidget subclasses: their expand is
 * not computed from their children. */
void awra_widget_isolate_expand(AwraWidget *w);

/* Creating widgets of a class. */
AwraWidget *awra_widget_new(const AwraClass *cls);
/* Adds `child` under `parent` at `index` (-1 end); the parent holds a ref. */
void awra_child_insert(AwraWidget *parent, AwraWidget *child, int index);
void awra_child_remove(AwraWidget *parent, AwraWidget *child);
/* Replaces a single-child slot: *slot gets `child` (old one removed). */
void awra_set_slot(AwraWidget *parent, AwraWidget **slot, AwraWidget *child);
int awra_child_index(const AwraWidget *parent, const AwraWidget *child);

/* ----------------------------------------------------------------------- */
/* Layout */

/* Size of `w` including margins and size request; for_size -1 = any. */
void awra_measure(AwraWidget *w, int orientation, int for_size, int *r_min, int *r_nat);
/* Places `w` in the slot x, y, width, height (margins and alignment inside). */
void awra_allocate(AwraWidget *w, int x, int y, int width, int height);
/* Ensures `w`'s children are placed again (contents changed, not size). */
void awra_queue_allocate(AwraWidget *w);
bool awra_compute_expand(AwraWidget *w, int orientation);
/* GtkSizeRequestMode: its height depends on its width (wrapping text,
 * pictures keeping their aspect, what holds them); else constant size. */
bool awra_height_for_width(AwraWidget *w);
/* a colour that reads against this appearance's background */
AwraRGBA awra_rgba_readable(AwraRGBA c);
/* Distributes `extra` among n children by GTK's natural-allocation rule:
 * sizes[] start at their minimums and grow towards their naturals. */
int awra_distribute(int extra, int n, int *sizes, const int *naturals);
/* Is this child laid out (visible)? */
static inline bool awra_child_visible(const BKNode *c)
{
  return (c->flags & NF_VISIBLE) != 0;
}

/* ----------------------------------------------------------------------- */
/* Metrics and the window scale */

float awra_scale(void);
static inline int awra_px(double logical)
{
  return (int)floor(logical * awra_scale() + 0.5);
}
static inline float awra_pxf(double logical)
{
  return (float)(logical * awra_scale());
}

/* ----------------------------------------------------------------------- */
/* Style */

typedef struct AwraTokens {
  bool dark, high_contrast;
  AwraRGBA background, foreground, muted, accent, accent_content, accent_soft, on_accent;
  AwraRGBA border, outline, separator, rim;
  AwraRGBA control_cap, control_cap_hover, control_pressed, control_well, control_thumb,
      control_track;
  AwraRGBA selection, focus_ring, disabled;
  AwraRGBA danger, success, warning, info;
  AwraRGBA material_tint[AWRA_MATERIAL_COUNT];
  float material_radius[AWRA_MATERIAL_COUNT];
  float material_grain[AWRA_MATERIAL_COUNT];
  bool material_blur[AWRA_MATERIAL_COUNT];
  float spacing[6];
  float radii[5];
  float border_width;
  float grain_scale;
} AwraTokens;

const AwraTokens *awra_tokens(void);
unsigned awra_style_generation(void);
void awra_style_init(void);
bool awra_reduced_transparency(void);
bool awra_native_blur_enabled(void);
float awra_font_px(void); /* base UI font size, logical px */
const char *awra_font_family(void);
const char *awra_mono_family(void);

/* colors */
static inline void awra_rgba_f(const AwraRGBA *c, float out[4])
{
  out[0] = c->r;
  out[1] = c->g;
  out[2] = c->b;
  out[3] = c->a;
}
AwraRGBA awra_rgba(float r, float g, float b, float a);
AwraRGBA awra_with_alpha(AwraRGBA c, float a);
AwraRGBA awra_mix(AwraRGBA a, AwraRGBA b, float t); /* CSS mix(a, b, t) */
AwraRGBA awra_shade(AwraRGBA c, float f);           /* CSS shade() */
bool awra_parse_color(const char *s, AwraRGBA *r_color);

/* ----------------------------------------------------------------------- */
/* Painting (paint.c) */

struct AwraDraw {
  BuiRaster *r;
  float opacity; /* inherited opacity of the subtree */
};

typedef struct AwraBox {
  float x, y, w, h;
} AwraBox;

static inline AwraBox awra_box_of(const BKNode *n)
{
  AwraBox b = {(float)n->x, (float)n->y, (float)n->w, (float)n->h};
  return b;
}
static inline AwraBox awra_box_inset(AwraBox b, float d)
{
  AwraBox o = {b.x + d, b.y + d, b.w - 2 * d, b.h - 2 * d};
  return o;
}

/* fills / borders; radii in raster px; NULL colors skip */
void awra_paint_rbox(AwraDraw *d, AwraBox b, float radius, const AwraRGBA *fill,
                     float border_width, const AwraRGBA *border);
void awra_paint_rbox4(AwraDraw *d, AwraBox b, const float radii[4], const AwraRGBA *fill,
                      const float widths[4], const AwraRGBA *border);
void awra_paint_shadow(AwraDraw *d, AwraBox b, float radius, float dx, float dy, float spread,
                       float blur, const AwraRGBA *color, bool clip_out);
void awra_paint_rect(AwraDraw *d, AwraBox b, const AwraRGBA *color);
void awra_paint_focus_ring(AwraDraw *d, AwraBox b, float radius);
void awra_paint_circle(AwraDraw *d, float cx, float cy, float r, const AwraRGBA *fill);
void awra_paint_line(AwraDraw *d, float x0, float y0, float x1, float y1, float width,
                     const AwraRGBA *color);

/* Materials (Awra 1's AwraSurface, AwraMaterialResolver). */
enum {
  AWRA_EDGE_TOP = 1,
  AWRA_EDGE_BOTTOM = 2,
  AWRA_EDGE_START = 4,
  AWRA_EDGE_END = 8,
  AWRA_EDGE_ALL = 15
};
typedef struct AwraResolvedMaterial {
  AwraRGBA fill, border, highlight, shadow;
  float radius, border_width, highlight_width, grain_opacity;
  int border_edges, highlight_edges;
  bool request_blur, fallback;
} AwraResolvedMaterial;
void awra_material_resolve(AwraMaterial material, int role, AwraElevation elevation,
                           bool blur_available, AwraResolvedMaterial *r);
/* Paints a resolved material in box b (corner mask: TL 1 TR 2 BR 4 BL 8). */
void awra_paint_material(AwraDraw *d, AwraBox b, const AwraResolvedMaterial *m, float radius,
                         int corner_mask, int edge_mask, AwraElevation elevation);
enum { AWRA_ROLE_GENERIC = 0, AWRA_ROLE_WINDOW, AWRA_ROLE_SIDEBAR, AWRA_ROLE_CONTENT,
       AWRA_ROLE_POPOVER, AWRA_ROLE_TOOLTIP };

/* ----------------------------------------------------------------------- */
/* Text (text.c) */

typedef struct AwraFont {
  const char *family;
  float size;  /* raster px */
  int weight;  /* 100..1000 */
  bool italic;
  float letter_spacing; /* raster px */
} AwraFont;

/* The font of a typography role at the current scale. */
AwraFont awra_font_for(AwraTypography t);
/* Color of a typography role. */
AwraRGBA awra_typography_color(AwraTypography t);
AwraTypography awra_widget_typography(const AwraWidget *w); /* inherited */
/* Line metrics */
float awra_font_ascent(const AwraFont *f);
float awra_font_line_height(const AwraFont *f); /* Pango's logical line height */
float awra_text_width(const AwraFont *f, const char *s, int len);
void awra_draw_text(AwraDraw *d, const AwraFont *f, float x, float baseline, const char *s,
                    int len, const AwraRGBA *color);

/* A laid-out paragraph (labels): runs of styled text, wrapped. */
typedef struct AwraTextLayout AwraTextLayout;
AwraTextLayout *awra_text_layout_new(void);
void awra_text_layout_free(AwraTextLayout *l);
void awra_text_layout_set_text(AwraTextLayout *l, const char *text, bool markup);
void awra_text_layout_set_font(AwraTextLayout *l, const AwraFont *f);
void awra_text_layout_set_wrap(AwraTextLayout *l, AwraWrap wrap, AwraEllipsize ellipsize,
                               int max_lines);
void awra_text_layout_set_justify(AwraTextLayout *l, AwraJustify justify);
/* Lays out for a width (-1 = unlimited); returns the size. */
void awra_text_layout_size(AwraTextLayout *l, int width, int *r_w, int *r_h);
/* Width of the widest unbreakable word (the minimum width of wrapping text). */
int awra_text_layout_min_width(AwraTextLayout *l);
int awra_text_layout_natural_width(AwraTextLayout *l);
void awra_text_layout_draw(AwraTextLayout *l, AwraDraw *d, float x, float y, int width,
                           const AwraRGBA *color);
int awra_text_layout_baseline(AwraTextLayout *l);
void awra_text_layout_index_to_pos(AwraTextLayout *l, int width, int index, float *r_x,
                                   float *r_y, float *r_h);
int awra_text_layout_pos_to_index(AwraTextLayout *l, int width, float px, float py);
const char *awra_text_layout_text(const AwraTextLayout *l);
/* links (<a href>): index under a point (layout coordinates), their target */
int awra_text_layout_n_links(const AwraTextLayout *l);
const char *awra_text_layout_link(const AwraTextLayout *l, int index);
int awra_text_layout_link_at(AwraTextLayout *l, int width, float px, float py);
void awra_text_layout_set_link_style(AwraTextLayout *l, int hover, const AwraRGBA *color,
                                     const AwraRGBA *hover_color);
/* inline objects: U+FFFC laid out as squares of `size` raster px */
void awra_text_layout_set_object_size(AwraTextLayout *l, float size);
void awra_text_layout_foreach_object(AwraTextLayout *l, int width,
                                     void (*fn)(int index, float x, float y, void *data), void *data);
void awra_text_layout_draw_selection(AwraTextLayout *l, AwraDraw *d, float x, float y, int width,
                                     int start, int end, const AwraRGBA *color);
void awra_text_layout_word_at(const AwraTextLayout *l, int index, int *r_start, int *r_end);
void awra_text_layout_index_to_pos(AwraTextLayout *l, int width, int index, float *r_x,
                                   float *r_y, float *r_h);
int awra_text_layout_pos_to_index(AwraTextLayout *l, int width, float px, float py);

/* ----------------------------------------------------------------------- */
/* Icons (icons.c) */

/* A symbolic icon's coverage mask at `size` raster px; NULL when unknown. */
typedef struct AwraIconMask {
  int w, h;
  uint8_t *a;
} AwraIconMask;
const AwraIconMask *awra_icon_mask(const char *name, int size);
void awra_paint_icon(AwraDraw *d, const char *name, float x, float y, int size,
                     const AwraRGBA *color);
/* Full-color icons (non-symbolic SVG / PNG) as a BuiPixmap; NULL: none. */
const BuiPixmap *awra_icon_pixmap(const char *name, int size);

/* ----------------------------------------------------------------------- */
/* Signals, state, animation (core.c) */

bool awra_has_handlers(const AwraWidget *w, AwraSignal signal);
/* Emits; returns whether a handler set *handled. */
bool awra_emit(AwraWidget *w, AwraArgs *args);
bool awra_emit_simple(AwraWidget *w, AwraSignal signal);
bool awra_emit_index(AwraWidget *w, AwraSignal signal, int index);
bool awra_emit_value(AwraWidget *w, AwraSignal signal, double value);
bool awra_emit_text(AwraWidget *w, AwraSignal signal, const char *text);
bool awra_emit_active(AwraWidget *w, AwraSignal signal, bool active);

/* State helpers for drawing */
bool awra_hovered(const AwraWidget *w);
bool awra_pressed(const AwraWidget *w);
bool awra_sensitive(const AwraWidget *w); /* itself and every ancestor */
bool awra_focus_visible(const AwraWidget *w);
/* Animated state fractions (Fast transitions; 0 / 1 with reduced motion). */
float awra_hover_t(const AwraWidget *w);
float awra_press_t(const AwraWidget *w);
void awra_animate_state(AwraWidget *w);

/* Value animations driven by the frame clock. */
typedef struct AwraAnim AwraAnim;
typedef void (*AwraAnimFunc)(AwraWidget *w, double t, void *data); /* t: 0..1 eased */
AwraAnim *awra_anim_start(AwraWidget *w, unsigned ms, AwraAnimFunc fn, void *data,
                          AwraDestroyNotify done);
void awra_anim_stop_for(AwraWidget *w);
bool awra_animating(void);
double awra_ease_out_cubic(double t);

/* The window a widget is in (NULL: not in one), and its kit context. */
AwraWidget *awra_window_of(const AwraWidget *w);
BKContext *awra_ctx(const AwraWidget *w);
void awra_invalidate(AwraWidget *w);
/* Keyboard focus */
void awra_set_focus(AwraWidget *window, AwraWidget *w, bool keyboard);
AwraWidget *awra_focus_of(AwraWidget *window);
/* Style classes: interned bits */
int awra_class_bit(const char *name);
static inline bool awra_has_class_bit(const AwraWidget *w, int bit)
{
  return bit >= 0 && (awra_aux(w)->classes & (1ull << bit)) != 0;
}
enum {
  AWRA_CB_ACCENT = 0, AWRA_CB_MUTED, AWRA_CB_FLAT, AWRA_CB_CIRCULAR, AWRA_CB_DIM,
  AWRA_CB_NUMERIC, AWRA_CB_SUCCESS, AWRA_CB_WARNING, AWRA_CB_ERROR, AWRA_CB_PILL,
  AWRA_CB_COMPACT, AWRA_CB_CARD, AWRA_CB_HEADING, AWRA_CB_CAPTION, AWRA_CB_DESTRUCTIVE,
  AWRA_CB_SUGGESTED, AWRA_CB_OSD, AWRA_CB_BUILTIN_COUNT
};

/* The inherited text color / weight for a label or icon inside `w` (its
 * ancestors' child_style); false when none applies. */
bool awra_inherited_style(const AwraWidget *w, AwraRGBA *r_color, int *r_weight);
float awra_typography_line_factor(AwraTypography t);
void awra_text_layout_set_line_factor(AwraTextLayout *l, float factor);
void awra_text_layout_set_line_extra(AwraTextLayout *l, float paragraph, float wrapped);

/* Button looks (button.c), shared by buttons, toggles, menu buttons,
 * segments and window controls. */
typedef enum {
  AWRA_LOOK_SECONDARY = 0, AWRA_LOOK_PRIMARY, AWRA_LOOK_GHOST, AWRA_LOOK_DESTRUCTIVE,
  AWRA_LOOK_TOOLBAR, AWRA_LOOK_SUCCESS, AWRA_LOOK_SEGMENT, AWRA_LOOK_WINDOW_CONTROL,
  AWRA_LOOK_NAV_ITEM, AWRA_LOOK_ACTION_ROW, AWRA_LOOK_MENU_ROW,
  /* style roles of Awra 1 on plain buttons (awra.css L4144-4430) */
  AWRA_LOOK_INLINE_ACTION, AWRA_LOOK_ROW_ACTION, AWRA_LOOK_SEARCH_LAUNCHER,
  AWRA_LOOK_COLLECTION_TILE, AWRA_LOOK_CARD_ACTION, AWRA_LOOK_SIDEBAR_HEADING
} AwraLook;
typedef struct AwraButtonColors {
  AwraRGBA fg, bg;
  AwraRGBA rim;       /* inset 0 1px rim (a = 0: none) */
  AwraRGBA ring;      /* inset 0 0 0 1px (primary hover) */
  float pressed_shadow; /* inset 0 1px 2px dark, alpha */
} AwraButtonColors;
void awra_button_colors(AwraLook look, bool hover, bool pressed, bool checked, bool sensitive,
                        AwraButtonColors *r);
/* Paints a button background (state, scale .96 while pressed, focus ring). */
void awra_paint_button_bg(AwraDraw *d, AwraBox b, float radius, const AwraButtonColors *c,
                          float press_t, bool focus_ring);

void awra_button_set_padding_internal(AwraWidget *w, int pad_x, int pad_y, int min_w);
void awra_header_set_drag_enabled(AwraWidget *w, bool drag);
void awra_header_set_dialog(AwraWidget *w, bool dialog);
void awra_header_set_compact(AwraWidget *w, bool compact);
bool awra_in_compact_header(const AwraWidget *w);
void awra_popover_set_padding_internal(AwraWidget *w, int pad, bool menu);
void awra_popover_set_centered_internal(AwraWidget *w, bool centered);
void awra_map_tree(AwraWidget *w, bool mapped);
void awra_tooltips_install(void);

/* Strings */
char *awra_strdup(const char *s);
void awra_set_str(char **dst, const char *s);

/* Popups (popovers, menus, dialogs, toasts): the window's overlay layer. */
void awra_popup_open(AwraWidget *popup, AwraWidget *owner);
void awra_popup_close(AwraWidget *popup);
bool awra_popup_is_open(const AwraWidget *popup);

/* The window's shortcut table and window internals (window.c). */
bool awra_window_dispatch_shortcut(AwraWidget *window, const char *keysym, int mods);
void awra_window_style_changed_all(void);
void awra_window_close_request(AwraWidget *window);
void awra_windows_request_frame(void);

/* Canvas used by drawing areas (canvas.c) */
struct AwraCanvas {
  AwraDraw *d;
  float ox, oy; /* widget origin, raster px */
  float scale;
};

/* Textures (texture.c): fit 0 fill, 1 contain, 2 cover, 3 scale-down */
void awra_paint_texture(AwraDraw *d, const AwraTexture *t, AwraBox b, float radius, int fit);
/* A texture of the (x, y, w, h) part of premultiplied ARGB pixels
 * (width x height); what lies outside them is transparent. */
AwraTexture *awra_texture_new_argb_region(const uint32_t *px, int width, int height, int x, int y,
                                          int w, int h);
/* The raster a window draws into (NULL: none yet); it keeps the last frame. */
struct BuiRaster *awra_window_raster(AwraWidget *window);

/* PNG writing (png.c), premultiplied ARGB in */
bool awra_write_png(const char *path, const uint32_t *argb, int w, int h);
const char *awra_key_code_name(int evdev);
