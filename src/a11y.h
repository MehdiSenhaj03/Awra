/* Awra 2 accessibility: the model platform backends expose.
 *
 * Every widget has a role (its class's, or an explicit one), a name and a
 * description computed with GTK's rules (gtkatcontext.c: an explicit
 * label, the labelled-by relation, the widget's own text, its children's
 * for roles named from content, its tooltip), states and a value. Classes
 * describe their instances with AwraClass.a11y (role variants, checked,
 * selected, values, text) and may draw virtual children (tabs, cells) that
 * a11y_items / a11y_item describe.
 *
 * Nothing here runs while no assistive technology listens: awra_a11y_on is
 * 0, the hooks in core.c return at once and no widget holds accessibility
 * data except the attributes an application sets explicitly. A backend
 * (AT-SPI2 on Linux, UI Automation on Windows) turns it on when a client
 * appears. Widgets it hands to the client are "exposed": they get an id
 * and a snapshot of what the client saw; when a widget changes (it is
 * repainted, resized, re-parented, hidden, focused), it is marked dirty and
 * an idle flush compares it with its snapshot and tells the backend only
 * what changed.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "awra-private.h"

/* ----------------------------------------------------------------------- */
/* Flat states: what backends map to their own sets. */

enum {
  AWRA_A11Y_FOCUSABLE = 1u << 0,
  AWRA_A11Y_FOCUSED = 1u << 1,
  AWRA_A11Y_ACTIVE = 1u << 2, /* windows: the active one */
  AWRA_A11Y_SENSITIVE = 1u << 3,
  AWRA_A11Y_VISIBLE = 1u << 4,
  AWRA_A11Y_SHOWING = 1u << 5,
  AWRA_A11Y_CHECKABLE = 1u << 6,
  AWRA_A11Y_CHECKED = 1u << 7,
  AWRA_A11Y_MIXED = 1u << 8,
  AWRA_A11Y_PRESSED = 1u << 9,
  AWRA_A11Y_EXPANDABLE = 1u << 10,
  AWRA_A11Y_EXPANDED = 1u << 11,
  AWRA_A11Y_SELECTABLE = 1u << 12,
  AWRA_A11Y_SELECTED = 1u << 13,
  AWRA_A11Y_BUSY = 1u << 14,
  AWRA_A11Y_INVALID = 1u << 15,
  AWRA_A11Y_EDITABLE = 1u << 16,
  AWRA_A11Y_READ_ONLY = 1u << 17,
  AWRA_A11Y_MULTI_LINE = 1u << 18,
  AWRA_A11Y_SINGLE_LINE = 1u << 19,
  AWRA_A11Y_MODAL = 1u << 20,
  AWRA_A11Y_HORIZONTAL = 1u << 21,
  AWRA_A11Y_VERTICAL = 1u << 22,
  AWRA_A11Y_MULTISELECTABLE = 1u << 23,
  AWRA_A11Y_REQUIRED = 1u << 24,
  AWRA_A11Y_HAS_POPUP = 1u << 25,
  AWRA_A11Y_VISITED = 1u << 26,
  AWRA_A11Y_PASSWORD = 1u << 27, /* hidden text (role tweak, no text exposed) */
  AWRA_A11Y_DEFAULT = 1u << 28,  /* the default button */
};

/* ----------------------------------------------------------------------- */
/* What a widget (or a virtual item) is, filled by a11y.c from the class's
 * type, then by the class's a11y hook, then by the explicit attributes. */

typedef struct AwraA11yInfo {
  AwraAccessibleRole role;
  /* the widget's own name (a label's text, a window's title) and
   * description; NULL: computed from the content and the tooltip */
  const char *label, *description;
  const char *placeholder;
  /* AwraAccessibleState values: bit (1 << state) set in `defined` when the
   * widget has the state, its value in state[] */
  unsigned defined;
  signed char state[AWRA_ACCESSIBLE_STATE_VISITED + 1];
  /* AWRA_A11Y_FOCUSABLE, FOCUSED, ACTIVE, EDITABLE, MULTI_LINE, READ_ONLY,
   * MODAL, HORIZONTAL, VERTICAL, HAS_POPUP, MULTISELECTABLE, REQUIRED,
   * PASSWORD, DEFAULT */
  unsigned flags;
  /* range widgets */
  bool has_value;
  double value, min, max, step;
  const char *value_text;
  /* text widgets (the Text interface): UTF-8 text, caret and selection
   * in characters (-1 none) */
  const char *text;
  int caret, sel_start, sel_end;
  int level;           /* headings */
  bool selection;      /* its children (or items) are selected (Selection) */
  const char *action;  /* the default action ("click", "toggle", ...), NULL: none */
  const char *shortcut; /* key binding of the action ("Ctrl+K") */
  char buf[48];        /* scratch for the class hook (value text) */
} AwraA11yInfo;

static inline void awra_a11y_set_state(AwraA11yInfo *info, AwraAccessibleState s, int v)
{
  info->defined |= 1u << s;
  info->state[s] = (signed char)v;
}

/* Verbs of AwraClass.a11y_do (item -1: the widget itself). */
enum {
  AWRA_A11Y_DO_ACTIVATE = 0, /* the default action */
  AWRA_A11Y_DO_SET_VALUE,    /* value */
  AWRA_A11Y_DO_SELECT,       /* item / child selected */
  AWRA_A11Y_DO_DESELECT,
  AWRA_A11Y_DO_EXPAND,
  AWRA_A11Y_DO_COLLAPSE,
  AWRA_A11Y_DO_SCROLL_TO,    /* item into view */
  AWRA_A11Y_DO_FOCUS,        /* item gets the cursor */
};

/* ----------------------------------------------------------------------- */
/* Per-widget data: explicit attributes (always kept) and, while exposed,
 * the snapshot of what the client saw. */

typedef struct AwraA11yAttr AwraA11yAttr;

typedef struct AwraA11ySnap {
  uint32_t id;
  unsigned char role;
  unsigned states;
  char *name, *description;
  double value;
  char *text;
  int caret, sel_start, sel_end;
  int nitems, item_focus; /* virtual items: count, the focused one (-1) */
  void *platform;         /* the backend's object for it (UIA provider) */
} AwraA11ySnap;

struct AwraA11y {
  AwraA11yAttr *attrs;
  short role; /* explicit role, -1 none */
  unsigned targeted : 1; /* another widget's relation points here */
  unsigned holder : 1;   /* has widget relations (in the holders list) */
  unsigned dirty : 1;
  AwraA11ySnap *snap; /* NULL: not exposed */
};

/* ----------------------------------------------------------------------- */
/* Hooks from core.c and window.c: no-ops while awra_a11y_on is 0. */

extern int awra_a11y_on;
void awra_a11y_dirty_(AwraWidget *w);
void awra_a11y_child_changed_(AwraWidget *parent, AwraWidget *child, bool added);
void awra_a11y_window_changed_(AwraWidget *window, int what);
static inline void awra_a11y_dirty(AwraWidget *w)
{
  if (awra_a11y_on) {
    awra_a11y_dirty_(w);
  }
}
static inline void awra_a11y_child_changed(AwraWidget *parent, AwraWidget *child, bool added)
{
  if (awra_a11y_on) {
    awra_a11y_child_changed_(parent, child, added);
  }
}
enum { AWRA_A11Y_WINDOW_CREATE, AWRA_A11Y_WINDOW_DESTROY, AWRA_A11Y_WINDOW_ACTIVATE,
       AWRA_A11Y_WINDOW_DEACTIVATE };
static inline void awra_a11y_window_changed(AwraWidget *window, int what)
{
  if (awra_a11y_on) {
    awra_a11y_window_changed_(window, what);
  }
}
/* the exposed widgets of a subtree (states inherited: sensitivity) */
void awra_a11y_dirty_tree(AwraWidget *w);
/* the widget goes: its attributes, relations to it, its exposure */
void awra_a11y_free(AwraWidget *w);

/* ----------------------------------------------------------------------- */
/* The model, for backends */

/* The computed description of a widget, or of its virtual item (item >= 0;
 * rect: logical px in the window, x y w h). */
void awra_a11y_info(AwraWidget *w, AwraA11yInfo *info);
void awra_a11y_item_info(AwraWidget *w, int item, AwraA11yInfo *info, double rect[4]);
int awra_a11y_n_items(AwraWidget *w);
unsigned awra_a11y_states(AwraWidget *w, const AwraA11yInfo *info);
unsigned awra_a11y_item_states(AwraWidget *w, const AwraA11yInfo *info);
/* Name and description (malloc'd), GTK's computation. */
char *awra_a11y_name(AwraWidget *w);
char *awra_a11y_description(AwraWidget *w);
char *awra_a11y_item_name(AwraWidget *w, int item);
/* Tree: presented widgets only (visible, not role none / presentation);
 * others are skipped and their children lifted. Popovers open from a
 * widget are its children. */
bool awra_a11y_presented(AwraWidget *w);
AwraWidget *awra_a11y_parent(AwraWidget *w); /* NULL: a window (the application's child) */
/* Fills up to max children; returns how many there are. */
int awra_a11y_children(AwraWidget *w, AwraWidget **out, int max);
int awra_a11y_index_in_parent(AwraWidget *w);
/* The toplevels shown (presented windows); returns the count. */
int awra_a11y_windows(AwraWidget **out, int max);
/* Bounds in logical px relative to the window (x y w h). */
void awra_a11y_bounds(AwraWidget *w, double r[4]);
/* The deepest presented widget at a point (logical, window-relative). */
AwraWidget *awra_a11y_at_point(AwraWidget *window, double x, double y);
/* Relation targets (explicit relations); returns the count. */
int awra_a11y_relation(AwraWidget *w, AwraAccessibleRelation rel, AwraWidget **out, int max);
/* Widgets whose relation `rel` points at w (the reverse relations). */
int awra_a11y_relation_reverse(AwraWidget *w, AwraAccessibleRelation rel, AwraWidget **out,
                               int max);
/* Explicit string / number attributes (NULL / false when unset). */
const char *awra_a11y_property_string(AwraWidget *w, AwraAccessibleProperty p);
bool awra_a11y_property_number(AwraWidget *w, AwraAccessibleProperty p, double *r);
bool awra_a11y_relation_number(AwraWidget *w, AwraAccessibleRelation r, int *v);
/* Performs a verb (AWRA_A11Y_DO_*) on the widget or one of its items. */
bool awra_a11y_do(AwraWidget *w, int item, int verb, double value);
/* Text roles: the text's character extents (logical px, window) */
bool awra_a11y_char_rect(AwraWidget *w, int offset, double r[4]);
int awra_a11y_offset_at(AwraWidget *w, double x, double y);
/* Naming rules of a role (GTK's table). */
bool awra_a11y_role_named_from_content(AwraAccessibleRole role);
const char *awra_a11y_role_name(AwraAccessibleRole role); /* "push button", ... */

/* ----------------------------------------------------------------------- */
/* Exposure and events */

typedef struct AwraA11yBackend {
  /* a flat state changed (one bit of AWRA_A11Y_*) */
  void (*state)(AwraWidget *w, int item, unsigned flag, bool on);
  /* name, description or value changed */
  void (*name)(AwraWidget *w);
  void (*description)(AwraWidget *w);
  void (*value)(AwraWidget *w);
  void (*role)(AwraWidget *w);
  /* text inserted or removed at a character offset (text: the UTF-8 run) */
  void (*text)(AwraWidget *w, bool insert, int offset, int length, const char *text, int bytes);
  void (*caret)(AwraWidget *w, int offset);
  void (*text_selection)(AwraWidget *w);
  /* a selectable child's selection changed: on its container */
  void (*selection)(AwraWidget *container);
  /* a presented child came or went (index in the parent's children) */
  void (*children)(AwraWidget *parent, AwraWidget *child, int index, bool added);
  /* virtual items changed (count) */
  void (*items)(AwraWidget *w, int old_count, int new_count);
  void (*window)(AwraWidget *window, int what);
  void (*announce)(AwraWidget *w, const char *message, int priority);
  /* the widget goes: drop its object */
  void (*freed)(AwraWidget *w, AwraA11ySnap *snap);
} AwraA11yBackend;

/* Turns the model on (a client listens) or off (NULL: every snapshot is
 * dropped). */
void awra_a11y_set_backend(const AwraA11yBackend *backend);
/* Exposes a widget (id, snapshot of now); returns its id (> 0). */
uint32_t awra_a11y_expose(AwraWidget *w);
AwraA11ySnap *awra_a11y_snap(AwraWidget *w); /* NULL: not exposed */
AwraWidget *awra_a11y_lookup(uint32_t id);   /* an exposed widget by id */
/* Runs the pending change comparisons now (before answering a client). */
void awra_a11y_flush(void);

/* UTF-8 helpers */
int awra_utf8_chars(const char *s, int bytes); /* bytes -1: to the end */
int awra_utf8_offset(const char *s, int chars); /* byte index of char `chars` */

/* Platform backends (atspi.c, uia.c): started when the first window shows. */
void awra_a11y_platform_start(void);
void awra_a11y_platform_stop(void);
