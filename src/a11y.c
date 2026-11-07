/* Awra 2 accessibility: roles, names, states and values of widgets (GTK's
 * GtkAccessible / GtkATContext), the explicit attributes applications set,
 * and the change tracking platform backends turn on (see a11y.h).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "a11y.h"

#include <stdio.h>

int awra_a11y_on = 0;
static const AwraA11yBackend *g_backend;

/* ======================================================================= */
/* UTF-8 */

int awra_utf8_chars(const char *s, int bytes)
{
  if (!s) {
    return 0;
  }
  int n = 0;
  for (int i = 0; bytes < 0 ? s[i] != 0 : i < bytes; i++) {
    if (((unsigned char)s[i] & 0xC0) != 0x80) {
      n++;
    }
  }
  return n;
}

int awra_utf8_offset(const char *s, int chars)
{
  if (!s || chars <= 0) {
    return 0;
  }
  int i = 0;
  while (s[i]) {
    if (((unsigned char)s[i] & 0xC0) != 0x80) {
      if (chars-- == 0) {
        return i;
      }
    }
    i++;
  }
  return i;
}

/* ======================================================================= */
/* Explicit attributes */

enum { KIND_PROP, KIND_STATE, KIND_REL };
enum { TYPE_STRING, TYPE_NUMBER, TYPE_REFS };

struct AwraA11yAttr {
  unsigned char kind, id, type;
  union {
    char *s;
    double d;
    struct {
      AwraWidget **v;
      int n;
    } refs;
  } u;
  AwraA11yAttr *next;
};

/* widgets holding widget relations (to drop targets that go) */
static AwraWidget **g_holders;
static int g_nholders, g_capholders;

static struct AwraA11y *a11y_of(const AwraWidget *w)
{
  return awra_is_widget(w) ? awra_aux(w)->a11y : NULL;
}

static struct AwraA11y *a11y_ensure(AwraWidget *w)
{
  if (!awra_is_widget(w)) {
    return NULL;
  }
  AwraAux *a = awra_aux(w);
  if (!a->a11y) {
    a->a11y = (struct AwraA11y *)calloc(1, sizeof(struct AwraA11y));
    if (a->a11y) {
      a->a11y->role = -1;
    }
  }
  return a->a11y;
}

static AwraA11yAttr *attr_find(const AwraWidget *w, int kind, int id)
{
  const struct AwraA11y *x = a11y_of(w);
  for (AwraA11yAttr *t = x ? x->attrs : NULL; t; t = t->next) {
    if (t->kind == kind && t->id == id) {
      return t;
    }
  }
  return NULL;
}

static void attr_clear_value(AwraA11yAttr *t)
{
  if (t->type == TYPE_STRING) {
    free(t->u.s);
  }
  else if (t->type == TYPE_REFS) {
    free(t->u.refs.v);
  }
  memset(&t->u, 0, sizeof(t->u));
}

static AwraA11yAttr *attr_set(AwraWidget *w, int kind, int id, int type)
{
  struct AwraA11y *x = a11y_ensure(w);
  if (!x) {
    return NULL;
  }
  AwraA11yAttr *t = attr_find(w, kind, id);
  if (t) {
    attr_clear_value(t);
  }
  else {
    t = (AwraA11yAttr *)calloc(1, sizeof(AwraA11yAttr));
    if (!t) {
      return NULL;
    }
    t->kind = (unsigned char)kind;
    t->id = (unsigned char)id;
    t->next = x->attrs;
    x->attrs = t;
  }
  t->type = (unsigned char)type;
  return t;
}

static void attr_remove(AwraWidget *w, int kind, int id)
{
  struct AwraA11y *x = a11y_of(w);
  if (!x) {
    return;
  }
  for (AwraA11yAttr **p = &x->attrs; *p; p = &(*p)->next) {
    if ((*p)->kind == kind && (*p)->id == id) {
      AwraA11yAttr *t = *p;
      *p = t->next;
      attr_clear_value(t);
      free(t);
      return;
    }
  }
}

static void holders_add(AwraWidget *w)
{
  struct AwraA11y *x = a11y_of(w);
  if (!x || x->holder) {
    return;
  }
  if (g_nholders == g_capholders) {
    const int cap = g_capholders ? g_capholders * 2 : 8;
    AwraWidget **v = (AwraWidget **)realloc(g_holders, sizeof(*v) * (size_t)cap);
    if (!v) {
      return;
    }
    g_holders = v;
    g_capholders = cap;
  }
  g_holders[g_nholders++] = w;
  x->holder = 1;
}

static void holders_remove(AwraWidget *w)
{
  for (int i = 0; i < g_nholders; i++) {
    if (g_holders[i] == w) {
      g_holders[i] = g_holders[--g_nholders];
      return;
    }
  }
}

static void changed(AwraWidget *w)
{
  awra_a11y_dirty(w);
}

void awra_accessible_set_role(AwraWidget *w, AwraAccessibleRole role)
{
  struct AwraA11y *x = a11y_ensure(w);
  if (x && x->role != (short)role) {
    x->role = (short)role;
    changed(w);
  }
}

void awra_accessible_update_property_string(AwraWidget *w, AwraAccessibleProperty p,
                                            const char *value)
{
  AwraA11yAttr *t = attr_find(w, KIND_PROP, p);
  if (t && t->type == TYPE_STRING && t->u.s && value && !strcmp(t->u.s, value)) {
    return;
  }
  t = attr_set(w, KIND_PROP, p, TYPE_STRING);
  if (t) {
    t->u.s = awra_strdup(value ? value : "");
    changed(w);
  }
}

void awra_accessible_update_property_number(AwraWidget *w, AwraAccessibleProperty p, double value)
{
  AwraA11yAttr *t = attr_find(w, KIND_PROP, p);
  if (t && t->type == TYPE_NUMBER && t->u.d == value) {
    return;
  }
  t = attr_set(w, KIND_PROP, p, TYPE_NUMBER);
  if (t) {
    t->u.d = value;
    changed(w);
  }
}

void awra_accessible_reset_property(AwraWidget *w, AwraAccessibleProperty p)
{
  if (attr_find(w, KIND_PROP, p)) {
    attr_remove(w, KIND_PROP, p);
    changed(w);
  }
}

void awra_accessible_update_state(AwraWidget *w, AwraAccessibleState s, int value)
{
  if ((unsigned)s > AWRA_ACCESSIBLE_STATE_VISITED) {
    return;
  }
  AwraA11yAttr *t = attr_find(w, KIND_STATE, s);
  if (t && t->u.d == value) {
    return;
  }
  t = attr_set(w, KIND_STATE, s, TYPE_NUMBER);
  if (t) {
    t->u.d = value;
    changed(w);
    if (s == AWRA_ACCESSIBLE_STATE_HIDDEN && w->parent) {
      /* hidden widgets leave the tree */
      awra_a11y_child_changed(w->parent, w, !value);
    }
  }
}

void awra_accessible_reset_state(AwraWidget *w, AwraAccessibleState s)
{
  if (attr_find(w, KIND_STATE, s)) {
    attr_remove(w, KIND_STATE, s);
    changed(w);
  }
}

void awra_accessible_update_relation(AwraWidget *w, AwraAccessibleRelation r,
                                     AwraWidget *const *targets, int n)
{
  AwraA11yAttr *t = attr_set(w, KIND_REL, r, TYPE_REFS);
  if (!t) {
    return;
  }
  if (n > 0 && targets) {
    t->u.refs.v = (AwraWidget **)malloc(sizeof(AwraWidget *) * (size_t)n);
    if (t->u.refs.v) {
      for (int i = 0; i < n; i++) {
        if (targets[i] && awra_is_widget(targets[i])) {
          t->u.refs.v[t->u.refs.n++] = targets[i];
          struct AwraA11y *tx = a11y_ensure(targets[i]);
          if (tx) {
            tx->targeted = 1;
          }
        }
      }
    }
    holders_add(w);
  }
  changed(w);
}

void awra_accessible_update_relation_number(AwraWidget *w, AwraAccessibleRelation r, int value)
{
  AwraA11yAttr *t = attr_set(w, KIND_REL, r, TYPE_NUMBER);
  if (t) {
    t->u.d = value;
    changed(w);
  }
}

void awra_accessible_update_relation_string(AwraWidget *w, AwraAccessibleRelation r,
                                            const char *value)
{
  AwraA11yAttr *t = attr_set(w, KIND_REL, r, TYPE_STRING);
  if (t) {
    t->u.s = awra_strdup(value ? value : "");
    changed(w);
  }
}

void awra_accessible_reset_relation(AwraWidget *w, AwraAccessibleRelation r)
{
  if (attr_find(w, KIND_REL, r)) {
    attr_remove(w, KIND_REL, r);
    changed(w);
  }
}

const char *awra_a11y_property_string(AwraWidget *w, AwraAccessibleProperty p)
{
  const AwraA11yAttr *t = attr_find(w, KIND_PROP, p);
  return t && t->type == TYPE_STRING ? t->u.s : NULL;
}

bool awra_a11y_property_number(AwraWidget *w, AwraAccessibleProperty p, double *r)
{
  const AwraA11yAttr *t = attr_find(w, KIND_PROP, p);
  if (t && t->type == TYPE_NUMBER) {
    *r = t->u.d;
    return true;
  }
  return false;
}

bool awra_a11y_relation_number(AwraWidget *w, AwraAccessibleRelation r, int *v)
{
  const AwraA11yAttr *t = attr_find(w, KIND_REL, r);
  if (t && t->type == TYPE_NUMBER) {
    *v = (int)t->u.d;
    return true;
  }
  return false;
}

int awra_a11y_relation(AwraWidget *w, AwraAccessibleRelation rel, AwraWidget **out, int max)
{
  const AwraA11yAttr *t = attr_find(w, KIND_REL, rel);
  if (!t || t->type != TYPE_REFS) {
    return 0;
  }
  for (int i = 0; i < t->u.refs.n && i < max; i++) {
    out[i] = t->u.refs.v[i];
  }
  return t->u.refs.n;
}

int awra_a11y_relation_reverse(AwraWidget *w, AwraAccessibleRelation rel, AwraWidget **out,
                               int max)
{
  const struct AwraA11y *x = a11y_of(w);
  if (!x || !x->targeted) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < g_nholders; i++) {
    const AwraA11yAttr *t = attr_find(g_holders[i], KIND_REL, rel);
    if (!t || t->type != TYPE_REFS) {
      continue;
    }
    for (int k = 0; k < t->u.refs.n; k++) {
      if (t->u.refs.v[k] == w) {
        if (n < max) {
          out[n] = g_holders[i];
        }
        n++;
        break;
      }
    }
  }
  return n;
}

/* ======================================================================= */
/* Roles */

#define FROM_AUTHOR 1
#define FROM_CONTENT 2
#define PROHIBITED 4

static const unsigned char g_naming[] = {
    [AWRA_ACCESSIBLE_ROLE_ALERT] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_ALERT_DIALOG] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_APPLICATION] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_ARTICLE] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_BANNER] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_BLOCK_QUOTE] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_BUTTON] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_CAPTION] = PROHIBITED,
    [AWRA_ACCESSIBLE_ROLE_CELL] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_CHECKBOX] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_COLUMN_HEADER] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_COMBO_BOX] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_COMMAND] = PROHIBITED,
    [AWRA_ACCESSIBLE_ROLE_COMMENT] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_COMPOSITE] = PROHIBITED,
    [AWRA_ACCESSIBLE_ROLE_DIALOG] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_DOCUMENT] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_FEED] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_FORM] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_GENERIC] = PROHIBITED,
    [AWRA_ACCESSIBLE_ROLE_GRID] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_GRID_CELL] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_GROUP] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_HEADING] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_IMG] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_INPUT] = PROHIBITED,
    [AWRA_ACCESSIBLE_ROLE_LABEL] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_LANDMARK] = PROHIBITED,
    [AWRA_ACCESSIBLE_ROLE_LEGEND] = PROHIBITED,
    [AWRA_ACCESSIBLE_ROLE_LINK] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_LIST] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_LIST_BOX] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_LIST_ITEM] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_LOG] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_MAIN] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_MARQUEE] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_MATH] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_METER] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_MENU] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_MENU_BAR] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_MENU_ITEM] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_MENU_ITEM_RADIO] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_NAVIGATION] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_NONE] = PROHIBITED,
    [AWRA_ACCESSIBLE_ROLE_NOTE] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_OPTION] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_PARAGRAPH] = PROHIBITED,
    [AWRA_ACCESSIBLE_ROLE_PRESENTATION] = PROHIBITED,
    [AWRA_ACCESSIBLE_ROLE_PROGRESS_BAR] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_RADIO] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_RADIO_GROUP] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_RANGE] = PROHIBITED,
    [AWRA_ACCESSIBLE_ROLE_REGION] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_ROW] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_ROW_GROUP] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_ROW_HEADER] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_SCROLLBAR] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_SEARCH] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_SEARCH_BOX] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_SECTION] = PROHIBITED,
    [AWRA_ACCESSIBLE_ROLE_SECTION_HEAD] = PROHIBITED,
    [AWRA_ACCESSIBLE_ROLE_SELECT] = PROHIBITED,
    [AWRA_ACCESSIBLE_ROLE_SEPARATOR] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_SLIDER] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_SPIN_BUTTON] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_STATUS] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_STRUCTURE] = PROHIBITED,
    [AWRA_ACCESSIBLE_ROLE_SWITCH] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_TAB] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_TABLE] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_TAB_LIST] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_TAB_PANEL] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_TERMINAL] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_TEXT_BOX] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_TIME] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_TIMER] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_TOGGLE_BUTTON] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_TOOLBAR] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_TOOLTIP] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_TREE] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_TREE_GRID] = FROM_AUTHOR,
    [AWRA_ACCESSIBLE_ROLE_TREE_ITEM] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_WIDGET] = FROM_AUTHOR | FROM_CONTENT,
    [AWRA_ACCESSIBLE_ROLE_WINDOW] = FROM_AUTHOR,
};

static unsigned naming(int role)
{
  return role >= 0 && role < (int)sizeof(g_naming) ? g_naming[role] : 0;
}

bool awra_a11y_role_named_from_content(AwraAccessibleRole role)
{
  return (naming(role) & FROM_CONTENT) != 0;
}

static bool role_is_range(int role)
{
  return role == AWRA_ACCESSIBLE_ROLE_METER || role == AWRA_ACCESSIBLE_ROLE_PROGRESS_BAR ||
         role == AWRA_ACCESSIBLE_ROLE_SCROLLBAR || role == AWRA_ACCESSIBLE_ROLE_SLIDER ||
         role == AWRA_ACCESSIBLE_ROLE_SPIN_BUTTON;
}

static bool role_is_text(int role)
{
  return role == AWRA_ACCESSIBLE_ROLE_TEXT_BOX || role == AWRA_ACCESSIBLE_ROLE_SEARCH_BOX ||
         role == AWRA_ACCESSIBLE_ROLE_SPIN_BUTTON;
}

const char *awra_a11y_role_name(AwraAccessibleRole role)
{
  static const char *const names[] = {
      [AWRA_ACCESSIBLE_ROLE_ALERT] = "alert", [AWRA_ACCESSIBLE_ROLE_ALERT_DIALOG] = "alert",
      [AWRA_ACCESSIBLE_ROLE_BANNER] = "landmark", [AWRA_ACCESSIBLE_ROLE_BUTTON] = "push button",
      [AWRA_ACCESSIBLE_ROLE_CAPTION] = "caption", [AWRA_ACCESSIBLE_ROLE_CELL] = "table cell",
      [AWRA_ACCESSIBLE_ROLE_CHECKBOX] = "check box",
      [AWRA_ACCESSIBLE_ROLE_COLUMN_HEADER] = "column header",
      [AWRA_ACCESSIBLE_ROLE_COMBO_BOX] = "combo box", [AWRA_ACCESSIBLE_ROLE_COMMAND] = "filler",
      [AWRA_ACCESSIBLE_ROLE_COMPOSITE] = "filler", [AWRA_ACCESSIBLE_ROLE_DIALOG] = "dialog",
      [AWRA_ACCESSIBLE_ROLE_DOCUMENT] = "document text", [AWRA_ACCESSIBLE_ROLE_FEED] = "panel",
      [AWRA_ACCESSIBLE_ROLE_FORM] = "form", [AWRA_ACCESSIBLE_ROLE_GENERIC] = "panel",
      [AWRA_ACCESSIBLE_ROLE_GRID] = "table", [AWRA_ACCESSIBLE_ROLE_GRID_CELL] = "table cell",
      [AWRA_ACCESSIBLE_ROLE_GROUP] = "grouping", [AWRA_ACCESSIBLE_ROLE_HEADING] = "heading",
      [AWRA_ACCESSIBLE_ROLE_IMG] = "image", [AWRA_ACCESSIBLE_ROLE_INPUT] = "entry",
      [AWRA_ACCESSIBLE_ROLE_LABEL] = "label", [AWRA_ACCESSIBLE_ROLE_LANDMARK] = "landmark",
      [AWRA_ACCESSIBLE_ROLE_LEGEND] = "label", [AWRA_ACCESSIBLE_ROLE_LINK] = "link",
      [AWRA_ACCESSIBLE_ROLE_LIST] = "list", [AWRA_ACCESSIBLE_ROLE_LIST_BOX] = "list box",
      [AWRA_ACCESSIBLE_ROLE_LIST_ITEM] = "list item", [AWRA_ACCESSIBLE_ROLE_LOG] = "log",
      [AWRA_ACCESSIBLE_ROLE_MAIN] = "landmark", [AWRA_ACCESSIBLE_ROLE_MARQUEE] = "marquee",
      [AWRA_ACCESSIBLE_ROLE_MATH] = "math", [AWRA_ACCESSIBLE_ROLE_METER] = "level bar",
      [AWRA_ACCESSIBLE_ROLE_MENU] = "menu", [AWRA_ACCESSIBLE_ROLE_MENU_BAR] = "menu bar",
      [AWRA_ACCESSIBLE_ROLE_MENU_ITEM] = "menu item",
      [AWRA_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX] = "check menu item",
      [AWRA_ACCESSIBLE_ROLE_MENU_ITEM_RADIO] = "radio menu item",
      [AWRA_ACCESSIBLE_ROLE_NAVIGATION] = "landmark", [AWRA_ACCESSIBLE_ROLE_NONE] = "invalid",
      [AWRA_ACCESSIBLE_ROLE_NOTE] = "footnote", [AWRA_ACCESSIBLE_ROLE_OPTION] = "list item",
      [AWRA_ACCESSIBLE_ROLE_PRESENTATION] = "invalid",
      [AWRA_ACCESSIBLE_ROLE_PROGRESS_BAR] = "progress bar",
      [AWRA_ACCESSIBLE_ROLE_RADIO] = "radio button",
      [AWRA_ACCESSIBLE_ROLE_RADIO_GROUP] = "grouping", [AWRA_ACCESSIBLE_ROLE_RANGE] = "filler",
      [AWRA_ACCESSIBLE_ROLE_REGION] = "filler", [AWRA_ACCESSIBLE_ROLE_ROW] = "table row",
      [AWRA_ACCESSIBLE_ROLE_ROW_GROUP] = "grouping",
      [AWRA_ACCESSIBLE_ROLE_ROW_HEADER] = "row header",
      [AWRA_ACCESSIBLE_ROLE_SCROLLBAR] = "scroll bar", [AWRA_ACCESSIBLE_ROLE_SEARCH] = "form",
      [AWRA_ACCESSIBLE_ROLE_SEARCH_BOX] = "entry", [AWRA_ACCESSIBLE_ROLE_SECTION] = "section",
      [AWRA_ACCESSIBLE_ROLE_SECTION_HEAD] = "filler", [AWRA_ACCESSIBLE_ROLE_SELECT] = "filler",
      [AWRA_ACCESSIBLE_ROLE_SEPARATOR] = "separator", [AWRA_ACCESSIBLE_ROLE_SLIDER] = "slider",
      [AWRA_ACCESSIBLE_ROLE_SPIN_BUTTON] = "spin button",
      [AWRA_ACCESSIBLE_ROLE_STATUS] = "status bar", [AWRA_ACCESSIBLE_ROLE_STRUCTURE] = "filler",
      [AWRA_ACCESSIBLE_ROLE_SWITCH] = "switch", [AWRA_ACCESSIBLE_ROLE_TAB] = "page tab",
      [AWRA_ACCESSIBLE_ROLE_TABLE] = "table", [AWRA_ACCESSIBLE_ROLE_TAB_LIST] = "page tab list",
      [AWRA_ACCESSIBLE_ROLE_TAB_PANEL] = "panel", [AWRA_ACCESSIBLE_ROLE_TEXT_BOX] = "text",
      [AWRA_ACCESSIBLE_ROLE_TIME] = "text", [AWRA_ACCESSIBLE_ROLE_TIMER] = "timer",
      [AWRA_ACCESSIBLE_ROLE_TOOLBAR] = "tool bar", [AWRA_ACCESSIBLE_ROLE_TOOLTIP] = "tool tip",
      [AWRA_ACCESSIBLE_ROLE_TREE] = "tree", [AWRA_ACCESSIBLE_ROLE_TREE_GRID] = "tree table",
      [AWRA_ACCESSIBLE_ROLE_TREE_ITEM] = "tree item", [AWRA_ACCESSIBLE_ROLE_WIDGET] = "filler",
      [AWRA_ACCESSIBLE_ROLE_WINDOW] = "frame",
      [AWRA_ACCESSIBLE_ROLE_TOGGLE_BUTTON] = "toggle button",
      [AWRA_ACCESSIBLE_ROLE_APPLICATION] = "frame", [AWRA_ACCESSIBLE_ROLE_PARAGRAPH] = "paragraph",
      [AWRA_ACCESSIBLE_ROLE_BLOCK_QUOTE] = "block quote", [AWRA_ACCESSIBLE_ROLE_ARTICLE] = "article",
      [AWRA_ACCESSIBLE_ROLE_COMMENT] = "comment", [AWRA_ACCESSIBLE_ROLE_TERMINAL] = "terminal",
  };
  return (unsigned)role < sizeof(names) / sizeof(names[0]) && names[role] ? names[role] : "filler";
}

/* The role of an Awra class (Awra 1's components and GTK's widgets). */
static const struct {
  const char *type;
  unsigned char role;
} g_class_roles[] = {
    {"AwraWindow", AWRA_ACCESSIBLE_ROLE_WINDOW},
    {"AwraHeader", AWRA_ACCESSIBLE_ROLE_GROUP},
    {"AwraLabel", AWRA_ACCESSIBLE_ROLE_LABEL},
    {"AwraRichLabel", AWRA_ACCESSIBLE_ROLE_PRESENTATION}, /* its label speaks */
    {"AwraImage", AWRA_ACCESSIBLE_ROLE_IMG},
    {"AwraButton", AWRA_ACCESSIBLE_ROLE_BUTTON},
    {"AwraCheckButton", AWRA_ACCESSIBLE_ROLE_CHECKBOX},
    {"AwraSwitch", AWRA_ACCESSIBLE_ROLE_SWITCH},
    {"AwraSlider", AWRA_ACCESSIBLE_ROLE_SLIDER},
    {"AwraSegmentedControl", AWRA_ACCESSIBLE_ROLE_RADIO_GROUP},
    {"AwraProgressBar", AWRA_ACCESSIBLE_ROLE_PROGRESS_BAR},
    {"AwraSpinner", AWRA_ACCESSIBLE_ROLE_PROGRESS_BAR},
    {"AwraEntry", AWRA_ACCESSIBLE_ROLE_TEXT_BOX},
    {"AwraShortcutCapture", AWRA_ACCESSIBLE_ROLE_BUTTON},
    {"AwraBox", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraSpacer", AWRA_ACCESSIBLE_ROLE_PRESENTATION},
    {"AwraSeparator", AWRA_ACCESSIBLE_ROLE_SEPARATOR},
    {"AwraDivider", AWRA_ACCESSIBLE_ROLE_SEPARATOR},
    {"AwraRailSeparator", AWRA_ACCESSIBLE_ROLE_SEPARATOR},
    {"AwraGrid", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraFlowBox", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraStack", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraOverlay", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraScrolledWindow", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraSurface", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraCard", AWRA_ACCESSIBLE_ROLE_GROUP},
    {"AwraPageViewport", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraPage", AWRA_ACCESSIBLE_ROLE_MAIN},
    {"AwraFormRow", AWRA_ACCESSIBLE_ROLE_GROUP},
    {"AwraActionRow", AWRA_ACCESSIBLE_ROLE_BUTTON},
    {"AwraBadge", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraStatusBanner", AWRA_ACCESSIBLE_ROLE_STATUS},
    {"AwraNavigationItem", AWRA_ACCESSIBLE_ROLE_BUTTON},
    {"AwraSplitView", AWRA_ACCESSIBLE_ROLE_GROUP},
    {"AwraComposer", AWRA_ACCESSIBLE_ROLE_GROUP},
    {"AwraComposerBanner", AWRA_ACCESSIBLE_ROLE_STATUS},
    {"AwraChip", AWRA_ACCESSIBLE_ROLE_TOGGLE_BUTTON},
    {"AwraSkeleton", AWRA_ACCESSIBLE_ROLE_GROUP},
    {"AwraTypingIndicator", AWRA_ACCESSIBLE_ROLE_STATUS},
    {"AwraCollectionView", AWRA_ACCESSIBLE_ROLE_LIST},
    {"AwraListView", AWRA_ACCESSIBLE_ROLE_LIST},
    {"AwraListItem", AWRA_ACCESSIBLE_ROLE_LIST_ITEM},
    {"AwraPopover", AWRA_ACCESSIBLE_ROLE_DIALOG},
    {"AwraMenuRow", AWRA_ACCESSIBLE_ROLE_MENU_ITEM},
    {"AwraChevron", AWRA_ACCESSIBLE_ROLE_PRESENTATION},
    {"AwraToastOverlay", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraToast", AWRA_ACCESSIBLE_ROLE_STATUS},
    {"AwraRailItem", AWRA_ACCESSIBLE_ROLE_TAB},
    {"AwraRailGroup", AWRA_ACCESSIBLE_ROLE_GROUP},
    {"AwraRailList", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraRail", AWRA_ACCESSIBLE_ROLE_TAB_LIST},
    {"AwraTabBar", AWRA_ACCESSIBLE_ROLE_TAB_LIST},
    {"AwraPathBar", AWRA_ACCESSIBLE_ROLE_NAVIGATION},
    {"AwraEmojiGrid", AWRA_ACCESSIBLE_ROLE_GRID},
    {"AwraColorSwatch", AWRA_ACCESSIBLE_ROLE_RADIO},
    {"AwraKeyCap", AWRA_ACCESSIBLE_ROLE_BUTTON},
    {"AwraKeyboardView", AWRA_ACCESSIBLE_ROLE_GROUP},
    {"AwraWaveformView", AWRA_ACCESSIBLE_ROLE_SLIDER},
    {"AwraMediaRow", AWRA_ACCESSIBLE_ROLE_GROUP},
    {"AwraDropZone", AWRA_ACCESSIBLE_ROLE_BUTTON},
    {"AwraDrawingArea", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraVideoView", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraTimeline", AWRA_ACCESSIBLE_ROLE_SLIDER},
    {"AwraOverlayBar", AWRA_ACCESSIBLE_ROLE_GROUP},
    {"AwraEdgePanel", AWRA_ACCESSIBLE_ROLE_GROUP},
    {"AwraConversationView", AWRA_ACCESSIBLE_ROLE_GROUP},
    {"AwraConversationPill", AWRA_ACCESSIBLE_ROLE_BUTTON},
    {"AwraAvatar", AWRA_ACCESSIBLE_ROLE_IMG},
    {"AwraAvatarGroup", AWRA_ACCESSIBLE_ROLE_GROUP},
    {"AwraPaned", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraRevealer", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraLayout", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraAspectFrame", AWRA_ACCESSIBLE_ROLE_GENERIC},
    {"AwraWindowHandle", AWRA_ACCESSIBLE_ROLE_GENERIC},
};

/* class -> role cache (class pointers hashed) */
typedef struct {
  const AwraClass *cls;
  int role;
} RoleSlot;
static RoleSlot g_role_cache[256];

static int class_role(const AwraClass *cls)
{
  unsigned h = (unsigned)(((uintptr_t)cls >> 4) & 255u);
  for (unsigned i = 0; i < 256; i++) {
    RoleSlot *s = &g_role_cache[(h + i) & 255u];
    if (s->cls == cls) {
      return s->role;
    }
    if (!s->cls) {
      int role = AWRA_ACCESSIBLE_ROLE_GENERIC;
      bool found = false;
      for (const AwraClass *c = cls; c && !found; c = c->parent_class) {
        for (size_t k = 0; k < sizeof(g_class_roles) / sizeof(g_class_roles[0]); k++) {
          if (c->type && !strcmp(c->type, g_class_roles[k].type)) {
            role = g_class_roles[k].role;
            found = true;
            break;
          }
        }
      }
      s->cls = cls;
      s->role = role;
      return role;
    }
  }
  return AWRA_ACCESSIBLE_ROLE_GENERIC;
}

AwraAccessibleRole awra_accessible_get_role(AwraWidget *w)
{
  if (!awra_is_widget(w)) {
    return AWRA_ACCESSIBLE_ROLE_NONE;
  }
  AwraA11yInfo info;
  awra_a11y_info(w, &info);
  return info.role;
}

/* ======================================================================= */
/* Info */

static bool widget_focusable(const AwraWidget *n)
{
  if (!awra_is_widget(n) || !awra_sensitive(n)) {
    return false;
  }
  const AwraAux *a = awra_aux(n);
  if (a->focusable_set) {
    return a->focusable;
  }
  return (AWRA_CLASS_OF(n)->flags & (AWRA_CLASS_FOCUSABLE | AWRA_CLASS_TEXT_INPUT)) != 0;
}

static bool window_active(AwraWidget *win)
{
  /* offscreen windows (tests) have no activation: count them active */
  return win && (awra_window_is_active(win) || !awra_window_native_handle(win));
}

static void apply_explicit(AwraWidget *w, AwraA11yInfo *info)
{
  const struct AwraA11y *x = a11y_of(w);
  if (!x) {
    return;
  }
  if (x->role >= 0) {
    info->role = (AwraAccessibleRole)x->role;
  }
  for (const AwraA11yAttr *t = x->attrs; t; t = t->next) {
    if (t->kind == KIND_STATE) {
      awra_a11y_set_state(info, (AwraAccessibleState)t->id, (int)t->u.d);
      continue;
    }
    if (t->kind != KIND_PROP) {
      continue;
    }
    const double d = t->type == TYPE_NUMBER ? t->u.d : 0.0;
    const char *s = t->type == TYPE_STRING ? t->u.s : NULL;
    switch (t->id) {
      case AWRA_ACCESSIBLE_PROPERTY_PLACEHOLDER:
        info->placeholder = s;
        break;
      case AWRA_ACCESSIBLE_PROPERTY_VALUE_MAX:
        info->max = d;
        info->has_value = true;
        break;
      case AWRA_ACCESSIBLE_PROPERTY_VALUE_MIN:
        info->min = d;
        info->has_value = true;
        break;
      case AWRA_ACCESSIBLE_PROPERTY_VALUE_NOW:
        info->value = d;
        info->has_value = true;
        break;
      case AWRA_ACCESSIBLE_PROPERTY_VALUE_TEXT:
        info->value_text = s;
        break;
      case AWRA_ACCESSIBLE_PROPERTY_LEVEL:
        info->level = (int)d;
        break;
      case AWRA_ACCESSIBLE_PROPERTY_KEY_SHORTCUTS:
        info->shortcut = s;
        break;
#define FLAG(prop, bit)            \
  case prop:                       \
    if (d != 0.0) {                \
      info->flags |= (bit);        \
    }                              \
    else {                         \
      info->flags &= ~(unsigned)(bit); \
    }                              \
    break;
      FLAG(AWRA_ACCESSIBLE_PROPERTY_HAS_POPUP, AWRA_A11Y_HAS_POPUP)
      FLAG(AWRA_ACCESSIBLE_PROPERTY_MODAL, AWRA_A11Y_MODAL)
      FLAG(AWRA_ACCESSIBLE_PROPERTY_MULTI_LINE, AWRA_A11Y_MULTI_LINE)
      FLAG(AWRA_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE, AWRA_A11Y_MULTISELECTABLE)
      FLAG(AWRA_ACCESSIBLE_PROPERTY_READ_ONLY, AWRA_A11Y_READ_ONLY)
      FLAG(AWRA_ACCESSIBLE_PROPERTY_REQUIRED, AWRA_A11Y_REQUIRED)
#undef FLAG
      case AWRA_ACCESSIBLE_PROPERTY_ORIENTATION:
        info->flags &= ~(unsigned)(AWRA_A11Y_HORIZONTAL | AWRA_A11Y_VERTICAL);
        info->flags |= d == AWRA_ORIENTATION_HORIZONTAL ? AWRA_A11Y_HORIZONTAL : AWRA_A11Y_VERTICAL;
        break;
      default:
        break;
    }
  }
}

void awra_a11y_info(AwraWidget *w, AwraA11yInfo *info)
{
  memset(info, 0, sizeof(*info));
  info->caret = info->sel_start = info->sel_end = -1;
  if (!awra_is_widget(w)) {
    info->role = AWRA_ACCESSIBLE_ROLE_NONE;
    return;
  }
  const AwraClass *cls = AWRA_CLASS_OF(w);
  info->role = (AwraAccessibleRole)class_role(cls);
  if (widget_focusable(w)) {
    info->flags |= AWRA_A11Y_FOCUSABLE;
  }
  AwraWidget *win = awra_window_of(w);
  if (win && awra_focus_of(win) == w && window_active(win)) {
    info->flags |= AWRA_A11Y_FOCUSED;
  }
  if (!awra_sensitive(w)) {
    awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_DISABLED, 1);
  }
  if ((cls->flags & AWRA_CLASS_ACTIVATABLE) && cls->activate) {
    info->action = "click";
  }
  else if (awra_has_handlers(w, AWRA_SIGNAL_CLICKED)) {
    info->action = "click";
  }
  if (info->role == AWRA_ACCESSIBLE_ROLE_WINDOW) {
    info->label = awra_window_title(w);
    if (window_active(w)) {
      info->flags |= AWRA_A11Y_ACTIVE;
    }
    if (awra_widget_get_data(w, "awra-dialog")) {
      info->role = AWRA_ACCESSIBLE_ROLE_DIALOG;
      info->flags |= AWRA_A11Y_MODAL;
    }
  }
  if (cls->a11y) {
    cls->a11y(w, info);
  }
  apply_explicit(w, info);
}

int awra_a11y_n_items(AwraWidget *w)
{
  return awra_is_widget(w) && AWRA_CLASS_OF(w)->a11y_items ? AWRA_CLASS_OF(w)->a11y_items(w) : 0;
}

void awra_a11y_item_info(AwraWidget *w, int item, AwraA11yInfo *info, double rect[4])
{
  memset(info, 0, sizeof(*info));
  info->caret = info->sel_start = info->sel_end = -1;
  info->role = AWRA_ACCESSIBLE_ROLE_LIST_ITEM;
  double r[4] = {0, 0, 0, 0};
  if (awra_is_widget(w) && AWRA_CLASS_OF(w)->a11y_item) {
    AWRA_CLASS_OF(w)->a11y_item(w, item, info, r);
  }
  if (!awra_sensitive(w)) {
    awra_a11y_set_state(info, AWRA_ACCESSIBLE_STATE_DISABLED, 1);
  }
  if (rect) {
    memcpy(rect, r, sizeof(r));
  }
}

static unsigned flat_states(AwraWidget *w, const AwraA11yInfo *info, bool item)
{
  unsigned s = info->flags & (AWRA_A11Y_FOCUSABLE | AWRA_A11Y_FOCUSED | AWRA_A11Y_ACTIVE |
                              AWRA_A11Y_EDITABLE | AWRA_A11Y_MULTI_LINE | AWRA_A11Y_READ_ONLY |
                              AWRA_A11Y_MODAL | AWRA_A11Y_HORIZONTAL | AWRA_A11Y_VERTICAL |
                              AWRA_A11Y_HAS_POPUP | AWRA_A11Y_MULTISELECTABLE |
                              AWRA_A11Y_REQUIRED | AWRA_A11Y_PASSWORD | AWRA_A11Y_DEFAULT);
  const unsigned d = info->defined;
#define HAS(st) (d & (1u << (st)))
#define VAL(st) (info->state[st])
  if (!(HAS(AWRA_ACCESSIBLE_STATE_DISABLED) && VAL(AWRA_ACCESSIBLE_STATE_DISABLED))) {
    s |= AWRA_A11Y_SENSITIVE;
  }
  bool hidden = HAS(AWRA_ACCESSIBLE_STATE_HIDDEN) && VAL(AWRA_ACCESSIBLE_STATE_HIDDEN);
  if (!item) {
    hidden |= !(w->flags & NF_VISIBLE);
  }
  if (!hidden) {
    s |= AWRA_A11Y_VISIBLE;
    if (item ? awra_widget_mapped(w) : awra_widget_mapped(w)) {
      s |= AWRA_A11Y_SHOWING;
    }
  }
  if (role_is_text(info->role) && !(s & AWRA_A11Y_READ_ONLY)) {
    s |= AWRA_A11Y_EDITABLE;
  }
  if (s & AWRA_A11Y_READ_ONLY) {
    s &= ~(unsigned)AWRA_A11Y_EDITABLE;
  }
  if ((s & AWRA_A11Y_EDITABLE) && !(s & AWRA_A11Y_MULTI_LINE)) {
    s |= AWRA_A11Y_SINGLE_LINE;
  }
  if (HAS(AWRA_ACCESSIBLE_STATE_BUSY) && VAL(AWRA_ACCESSIBLE_STATE_BUSY)) {
    s |= AWRA_A11Y_BUSY;
  }
  if (HAS(AWRA_ACCESSIBLE_STATE_CHECKED)) {
    s |= AWRA_A11Y_CHECKABLE;
    if (VAL(AWRA_ACCESSIBLE_STATE_CHECKED) == AWRA_ACCESSIBLE_TRISTATE_TRUE) {
      s |= AWRA_A11Y_CHECKED;
    }
    else if (VAL(AWRA_ACCESSIBLE_STATE_CHECKED) == AWRA_ACCESSIBLE_TRISTATE_MIXED) {
      s |= AWRA_A11Y_MIXED;
    }
  }
  if (HAS(AWRA_ACCESSIBLE_STATE_PRESSED)) {
    if (VAL(AWRA_ACCESSIBLE_STATE_PRESSED) == AWRA_ACCESSIBLE_TRISTATE_TRUE) {
      s |= AWRA_A11Y_PRESSED;
    }
    else if (VAL(AWRA_ACCESSIBLE_STATE_PRESSED) == AWRA_ACCESSIBLE_TRISTATE_MIXED) {
      s |= AWRA_A11Y_MIXED;
    }
  }
  if (HAS(AWRA_ACCESSIBLE_STATE_EXPANDED)) {
    s |= AWRA_A11Y_EXPANDABLE;
    if (VAL(AWRA_ACCESSIBLE_STATE_EXPANDED)) {
      s |= AWRA_A11Y_EXPANDED;
    }
  }
  if (HAS(AWRA_ACCESSIBLE_STATE_SELECTED)) {
    s |= AWRA_A11Y_SELECTABLE;
    if (VAL(AWRA_ACCESSIBLE_STATE_SELECTED)) {
      s |= AWRA_A11Y_SELECTED;
    }
  }
  if (HAS(AWRA_ACCESSIBLE_STATE_INVALID) && VAL(AWRA_ACCESSIBLE_STATE_INVALID)) {
    s |= AWRA_A11Y_INVALID;
  }
  if (HAS(AWRA_ACCESSIBLE_STATE_VISITED) && VAL(AWRA_ACCESSIBLE_STATE_VISITED)) {
    s |= AWRA_A11Y_VISITED;
  }
#undef HAS
#undef VAL
  return s;
}

unsigned awra_a11y_states(AwraWidget *w, const AwraA11yInfo *info)
{
  return flat_states(w, info, false);
}

unsigned awra_a11y_item_states(AwraWidget *w, const AwraA11yInfo *info)
{
  return flat_states(w, info, true);
}

int awra_accessible_get_state(AwraWidget *w, AwraAccessibleState s)
{
  if ((unsigned)s > AWRA_ACCESSIBLE_STATE_VISITED || !awra_is_widget(w)) {
    return -1;
  }
  AwraA11yInfo info;
  awra_a11y_info(w, &info);
  if (s == AWRA_ACCESSIBLE_STATE_HIDDEN && !(info.defined & (1u << s))) {
    return !(w->flags & NF_VISIBLE);
  }
  return (info.defined & (1u << s)) ? info.state[s] : -1;
}

/* ======================================================================= */
/* Names and descriptions (gtk_at_context_get_text_accumulate) */

typedef struct Str {
  char *s;
  size_t n, cap;
} Str;

static void str_append(Str *b, const char *t, size_t len)
{
  if (b->n + len + 2 > b->cap) {
    size_t cap = b->cap ? b->cap * 2 : 64;
    while (cap < b->n + len + 2) {
      cap *= 2;
    }
    char *s = (char *)realloc(b->s, cap);
    if (!s) {
      return;
    }
    b->s = s;
    b->cap = cap;
  }
  memcpy(b->s + b->n, t, len);
  b->n += len;
  b->s[b->n] = 0;
}

static void str_append_word(Str *b, const char *t)
{
  if (b->n > 0) {
    str_append(b, " ", 1);
  }
  str_append(b, t, strlen(t));
}

static bool not_just_space(const char *t)
{
  for (; t && *t; t++) {
    if (*t != ' ' && *t != '\t' && *t != '\n' && *t != '\r') {
      return true;
    }
  }
  return false;
}

typedef struct Visit {
  AwraWidget *v[32];
  int n;
} Visit;

static void accumulate(AwraWidget *w, Visit *nodes, Str *res, bool label, bool is_ref,
                       bool is_child, bool check_dup, int depth);
static char *text_of(AwraWidget *w, bool label, bool check_dup);

static bool hidden_widget(AwraWidget *w, const AwraA11yInfo *info)
{
  if (info->defined & (1u << AWRA_ACCESSIBLE_STATE_HIDDEN)) {
    return info->state[AWRA_ACCESSIBLE_STATE_HIDDEN] != 0;
  }
  return !(w->flags & NF_VISIBLE);
}

static void accumulate(AwraWidget *w, Visit *nodes, Str *res, bool label, bool is_ref,
                       bool is_child, bool check_dup, int depth)
{
  if (!awra_is_widget(w) || depth > 24) {
    /* non-widget nodes: their widget children speak for them */
    if (w && depth <= 24) {
      for (int i = 0; i < w->nkids; i++) {
        if (w->kids[i]->flags & NF_VISIBLE) {
          accumulate(w->kids[i], nodes, res, label, is_ref, is_child, check_dup, depth + 1);
        }
      }
    }
    return;
  }
  AwraA11yInfo info;
  awra_a11y_info(w, &info);
  /* 2A */
  if (!is_ref && hidden_widget(w, &info)) {
    return;
  }
  const AwraAccessibleProperty prop =
      label ? AWRA_ACCESSIBLE_PROPERTY_LABEL : AWRA_ACCESSIBLE_PROPERTY_DESCRIPTION;
  const AwraAccessibleRelation rel =
      label ? AWRA_ACCESSIBLE_RELATION_LABELLED_BY : AWRA_ACCESSIBLE_RELATION_DESCRIBED_BY;
  if (naming(info.role) & FROM_AUTHOR) {
    /* 2B */
    AwraWidget *targets[16];
    const int n = is_ref ? 0 : awra_a11y_relation(w, rel, targets, 16);
    if (n > 0) {
      for (int i = 0; i < n && i < 16; i++) {
        bool seen = false;
        for (int k = 0; k < nodes->n; k++) {
          seen |= nodes->v[k] == targets[i];
        }
        if (!seen && nodes->n < 32) {
          nodes->v[nodes->n++] = targets[i];
          accumulate(targets[i], nodes, res, label, true, false, check_dup, depth + 1);
        }
      }
      return;
    }
    /* 2C: the explicit property, then the widget's own (a label's text) */
    const char *s = awra_a11y_property_string(w, prop);
    if (!s || !s[0]) {
      s = label ? info.label : info.description;
    }
    if (s && s[0]) {
      str_append_word(res, s);
      return;
    }
  }
  /* 2E */
  if (label && (is_child || is_ref)) {
    if (role_is_text(info.role) && info.role != AWRA_ACCESSIBLE_ROLE_SPIN_BUTTON) {
      if (info.text && !(info.flags & AWRA_A11Y_PASSWORD) && not_just_space(info.text)) {
        str_append_word(res, info.text);
      }
      return;
    }
    if (role_is_range(info.role)) {
      if (info.value_text) {
        str_append_word(res, info.value_text);
      }
      else if (info.has_value) {
        char num[32];
        snprintf(num, sizeof(num), "%g", info.value);
        str_append_word(res, num);
      }
      return;
    }
  }
  /* 2F */
  if ((naming(info.role) & FROM_CONTENT) || is_ref || is_child) {
    Str s = {0};
    for (int i = 0; i < w->nkids; i++) {
      accumulate(w->kids[i], nodes, &s, label, false, true, check_dup, depth + 1);
    }
    if (s.n > 0) {
      str_append_word(res, s.s);
      free(s.s);
      return;
    }
    free(s.s);
  }
  /* 2I */
  const char *tip = w->desc;
  if (tip && not_just_space(tip)) {
    bool append = !check_dup;
    if (!append) {
      char *other = text_of(w, !label, false);
      append = !other || strcmp(tip, other) != 0;
      free(other);
    }
    if (append) {
      str_append_word(res, tip);
    }
  }
}

static char *text_of(AwraWidget *w, bool label, bool check_dup)
{
  if (!awra_is_widget(w)) {
    return awra_strdup("");
  }
  AwraA11yInfo info;
  awra_a11y_info(w, &info);
  if (naming(info.role) & PROHIBITED) {
    return awra_strdup("");
  }
  Visit nodes;
  nodes.n = 0;
  Str res = {0};
  accumulate(w, &nodes, &res, label, false, false, check_dup, 0);
  return res.s ? res.s : awra_strdup("");
}

char *awra_a11y_name(AwraWidget *w)
{
  return text_of(w, true, false);
}

char *awra_a11y_description(AwraWidget *w)
{
  return text_of(w, false, true);
}

char *awra_accessible_get_name(AwraWidget *w)
{
  return awra_a11y_name(w);
}

char *awra_accessible_get_description(AwraWidget *w)
{
  return awra_a11y_description(w);
}

char *awra_a11y_item_name(AwraWidget *w, int item)
{
  AwraA11yInfo info;
  awra_a11y_item_info(w, item, &info, NULL);
  return awra_strdup(info.label ? info.label : info.text ? info.text : "");
}

/* ======================================================================= */
/* Tree */

static bool presented_info(AwraWidget *w, const AwraA11yInfo *info)
{
  if (!(w->flags & NF_VISIBLE)) {
    return false;
  }
  if (info->role == AWRA_ACCESSIBLE_ROLE_NONE || info->role == AWRA_ACCESSIBLE_ROLE_PRESENTATION) {
    return false;
  }
  if ((info->defined & (1u << AWRA_ACCESSIBLE_STATE_HIDDEN)) &&
      info->state[AWRA_ACCESSIBLE_STATE_HIDDEN])
  {
    return false;
  }
  return true;
}

bool awra_a11y_presented(AwraWidget *w)
{
  if (!awra_is_widget(w)) {
    return false;
  }
  AwraA11yInfo info;
  awra_a11y_info(w, &info);
  return presented_info(w, &info);
}

/* the kit context's popups whose owner is `w` */
static void owned_popups(AwraWidget *w, AwraWidget **out, int *n, int max, bool count_only)
{
  BKContext *ctx = bk_ctx(w);
  if (!ctx) {
    return;
  }
  for (int i = 0; i < ctx->npopups; i++) {
    BKNode *p = ctx->popups[i];
    if (p->parent == w && awra_is_widget(p) && awra_widget_mapped(p)) {
      if (awra_a11y_presented(p)) {
        if (!count_only && *n < max) {
          out[*n] = p;
        }
        (*n)++;
      }
      else {
        int k = *n < max ? *n : max;
        const int got = awra_a11y_children(p, count_only ? NULL : out + k,
                                           count_only ? 0 : max - k);
        *n += got;
      }
    }
  }
}

static void collect_children(BKNode *n, AwraWidget **out, int *count, int max, int depth)
{
  for (int i = 0; i < n->nkids; i++) {
    BKNode *k = n->kids[i];
    if (!(k->flags & NF_VISIBLE)) {
      continue;
    }
    if (awra_is_widget(k) && awra_a11y_presented(k)) {
      if (out && *count < max) {
        out[*count] = k;
      }
      (*count)++;
      continue;
    }
    if (depth < 48) {
      collect_children(k, out, count, max, depth + 1);
    }
    if (awra_is_widget(k)) {
      owned_popups(k, out, count, max, out == NULL);
    }
  }
}

int awra_a11y_children(AwraWidget *w, AwraWidget **out, int max)
{
  int n = 0;
  if (!w) {
    return 0;
  }
  collect_children(w, out, &n, max, 0);
  owned_popups(w, out, &n, max, out == NULL);
  return n;
}

AwraWidget *awra_a11y_parent(AwraWidget *w)
{
  for (BKNode *n = w ? w->parent : NULL; n; n = n->parent) {
    if (awra_is_widget(n) && awra_a11y_presented(n)) {
      return n;
    }
  }
  return NULL;
}

int awra_a11y_index_in_parent(AwraWidget *w)
{
  AwraWidget *p = awra_a11y_parent(w);
  if (!p) {
    AwraWidget *wins[64];
    const int n = awra_a11y_windows(wins, 64);
    for (int i = 0; i < n && i < 64; i++) {
      if (wins[i] == w) {
        return i;
      }
    }
    return -1;
  }
  int count = awra_a11y_children(p, NULL, 0);
  AwraWidget *stack[256];
  AwraWidget **kids = count <= 256 ? stack : (AwraWidget **)malloc(sizeof(*kids) * (size_t)count);
  if (!kids) {
    return -1;
  }
  count = awra_a11y_children(p, kids, count);
  int index = -1;
  for (int i = 0; i < count; i++) {
    if (kids[i] == w) {
      index = i;
      break;
    }
  }
  if (kids != stack) {
    free(kids);
  }
  return index;
}

int awra_a11y_windows(AwraWidget **out, int max)
{
  int n = 0;
  const int total = awra_window_count();
  for (int i = 0; i < total; i++) {
    AwraWidget *win = awra_window_nth(i);
    if (win && awra_widget_mapped(win)) {
      if (n < max) {
        out[n] = win;
      }
      n++;
    }
  }
  return n;
}

void awra_a11y_bounds(AwraWidget *w, double r[4])
{
  const double s = awra_scale();
  r[0] = w->x / s;
  r[1] = w->y / s;
  r[2] = w->w / s;
  r[3] = w->h / s;
}

static AwraWidget *at_point(BKNode *n, int x, int y, AwraWidget *best)
{
  if (!(n->flags & NF_VISIBLE)) {
    return best;
  }
  const bool inside = x >= n->x && y >= n->y && x < n->x + n->w && y < n->y + n->h;
  if (awra_is_widget(n) && inside && awra_a11y_presented(n)) {
    best = n;
  }
  /* children may overflow non-clipping parents: look at every child */
  for (int i = n->nkids - 1; i >= 0; i--) {
    AwraWidget *found = at_point(n->kids[i], x, y, NULL);
    if (found) {
      return found;
    }
  }
  return best;
}

AwraWidget *awra_a11y_at_point(AwraWidget *win, double lx, double ly)
{
  const double s = awra_scale();
  const int x = (int)(lx * s), y = (int)(ly * s);
  BKContext *ctx = bk_ctx(win);
  if (ctx) {
    for (int i = ctx->npopups - 1; i >= 0; i--) {
      BKNode *p = ctx->popups[i];
      if (x >= p->x && y >= p->y && x < p->x + p->w && y < p->y + p->h) {
        AwraWidget *f = at_point(p, x, y, NULL);
        if (f) {
          return f;
        }
      }
    }
  }
  return at_point(win, x, y, NULL);
}

bool awra_a11y_do(AwraWidget *w, int item, int verb, double value)
{
  if (!awra_is_widget(w)) {
    return false;
  }
  const AwraClass *cls = AWRA_CLASS_OF(w);
  if (cls->a11y_do && cls->a11y_do(w, item, verb, value)) {
    return true;
  }
  if (item >= 0 || !awra_sensitive(w)) {
    return false;
  }
  switch (verb) {
    case AWRA_A11Y_DO_ACTIVATE:
      if ((cls->flags & AWRA_CLASS_ACTIVATABLE) && cls->activate) {
        cls->activate(w);
        return true;
      }
      if (awra_has_handlers(w, AWRA_SIGNAL_CLICKED)) {
        awra_emit_simple(w, AWRA_SIGNAL_CLICKED);
        return true;
      }
      return false;
    case AWRA_A11Y_DO_SELECT:
    case AWRA_A11Y_DO_FOCUS:
      if (widget_focusable(w)) {
        awra_widget_grab_focus(w);
        return true;
      }
      return false;
    default:
      return false;
  }
}

bool awra_a11y_char_rect(AwraWidget *w, int offset, double r[4])
{
  /* the caret of editables, else the widget's box split evenly */
  AwraA11yInfo info;
  awra_a11y_info(w, &info);
  double b[4];
  awra_a11y_bounds(w, b);
  const int n = info.text ? awra_utf8_chars(info.text, -1) : 0;
  if (n <= 0 || offset < 0 || offset > n) {
    return false;
  }
  r[0] = b[0] + b[2] * offset / n;
  r[1] = b[1];
  r[2] = b[2] / n;
  r[3] = b[3];
  return true;
}

int awra_a11y_offset_at(AwraWidget *w, double x, double y)
{
  AwraA11yInfo info;
  awra_a11y_info(w, &info);
  double b[4];
  awra_a11y_bounds(w, b);
  const int n = info.text ? awra_utf8_chars(info.text, -1) : 0;
  if (n <= 0 || x < b[0] || y < b[1] || x >= b[0] + b[2] || y >= b[1] + b[3]) {
    return -1;
  }
  return (int)((x - b[0]) * n / b[2]);
}

/* ======================================================================= */
/* Exposure: ids and snapshots */

typedef struct IdSlot {
  uint32_t id; /* 0 empty, UINT32_MAX tombstone */
  AwraWidget *w;
} IdSlot;
static IdSlot *g_ids;
static uint32_t g_ids_cap, g_ids_used, g_next_id = 1;

static uint32_t hash_id(uint32_t id)
{
  id ^= id >> 16;
  id *= 0x45d9f3bu;
  id ^= id >> 16;
  return id;
}

static void ids_insert(uint32_t id, AwraWidget *w);

static void ids_grow(void)
{
  const uint32_t old_cap = g_ids_cap;
  IdSlot *old = g_ids;
  g_ids_cap = old_cap ? old_cap * 2 : 256;
  g_ids = (IdSlot *)calloc(g_ids_cap, sizeof(IdSlot));
  g_ids_used = 0;
  for (uint32_t i = 0; i < old_cap; i++) {
    if (old[i].id && old[i].id != UINT32_MAX) {
      ids_insert(old[i].id, old[i].w);
    }
  }
  free(old);
}

static void ids_insert(uint32_t id, AwraWidget *w)
{
  if ((g_ids_used + 1) * 4 >= g_ids_cap * 3) {
    ids_grow();
  }
  for (uint32_t i = hash_id(id);; i++) {
    IdSlot *s = &g_ids[i & (g_ids_cap - 1)];
    if (s->id == 0 || s->id == UINT32_MAX) {
      if (s->id == 0) {
        g_ids_used++;
      }
      s->id = id;
      s->w = w;
      return;
    }
  }
}

static IdSlot *ids_find(uint32_t id)
{
  if (!g_ids_cap || !id || id == UINT32_MAX) {
    return NULL;
  }
  for (uint32_t i = hash_id(id), k = 0; k < g_ids_cap; i++, k++) {
    IdSlot *s = &g_ids[i & (g_ids_cap - 1)];
    if (s->id == id) {
      return s;
    }
    if (s->id == 0) {
      return NULL;
    }
  }
  return NULL;
}

AwraWidget *awra_a11y_lookup(uint32_t id)
{
  IdSlot *s = ids_find(id);
  return s ? s->w : NULL;
}

AwraA11ySnap *awra_a11y_snap(AwraWidget *w)
{
  const struct AwraA11y *x = a11y_of(w);
  return x ? x->snap : NULL;
}

static char *text_dup(const AwraA11yInfo *info)
{
  if (!info->text) {
    return NULL;
  }
  if (info->flags & AWRA_A11Y_PASSWORD) {
    return awra_strdup("");
  }
  return awra_strdup(info->text);
}

static void snap_fill(AwraWidget *w, AwraA11ySnap *s)
{
  AwraA11yInfo info;
  awra_a11y_info(w, &info);
  s->role = (unsigned char)info.role;
  s->states = awra_a11y_states(w, &info);
  free(s->name);
  free(s->description);
  free(s->text);
  s->name = awra_a11y_name(w);
  s->description = awra_a11y_description(w);
  s->value = info.has_value ? info.value : 0.0;
  s->text = text_dup(&info);
  s->caret = info.caret;
  s->sel_start = info.sel_start;
  s->sel_end = info.sel_end;
  s->nitems = awra_a11y_n_items(w);
  s->item_focus = -1;
  for (int i = 0; i < s->nitems && (s->states & AWRA_A11Y_FOCUSED); i++) {
    /* only the focused container's cursor item counts */
    AwraA11yInfo ii;
    awra_a11y_item_info(w, i, &ii, NULL);
    if (ii.flags & AWRA_A11Y_FOCUSED) {
      s->item_focus = i;
      break;
    }
  }
}

uint32_t awra_a11y_expose(AwraWidget *w)
{
  struct AwraA11y *x = a11y_ensure(w);
  if (!x) {
    return 0;
  }
  if (!x->snap) {
    x->snap = (AwraA11ySnap *)calloc(1, sizeof(AwraA11ySnap));
    if (!x->snap) {
      return 0;
    }
    x->snap->id = g_next_id++;
    if (g_next_id == UINT32_MAX) {
      g_next_id = 1;
    }
    ids_insert(x->snap->id, w);
    snap_fill(w, x->snap);
  }
  return x->snap->id;
}

static void snap_free(AwraWidget *w, struct AwraA11y *x)
{
  if (!x->snap) {
    return;
  }
  if (g_backend && g_backend->freed) {
    g_backend->freed(w, x->snap);
  }
  IdSlot *s = ids_find(x->snap->id);
  if (s) {
    s->id = UINT32_MAX;
    s->w = NULL;
  }
  free(x->snap->name);
  free(x->snap->description);
  free(x->snap->text);
  free(x->snap);
  x->snap = NULL;
}

/* ----------------------------------------------------------------------- */
/* Dirty widgets and the flush */

static AwraWidget **g_dirty;
static int g_ndirty, g_capdirty;
static bool g_flush_queued;

static bool flush_idle(void *data)
{
  g_flush_queued = false;
  awra_a11y_flush();
  return false;
}

static void mark(AwraWidget *w)
{
  struct AwraA11y *x = a11y_of(w);
  if (!x || !x->snap || x->dirty) {
    return;
  }
  if (g_ndirty == g_capdirty) {
    const int cap = g_capdirty ? g_capdirty * 2 : 32;
    AwraWidget **v = (AwraWidget **)realloc(g_dirty, sizeof(*v) * (size_t)cap);
    if (!v) {
      return;
    }
    g_dirty = v;
    g_capdirty = cap;
  }
  x->dirty = 1;
  g_dirty[g_ndirty++] = w;
  if (!g_flush_queued) {
    g_flush_queued = true;
    awra_idle_add(flush_idle, NULL, NULL);
  }
}

void awra_a11y_dirty_(AwraWidget *w)
{
  if (!awra_is_widget(w)) {
    return;
  }
  const struct AwraA11y *x = a11y_of(w);
  if (x && x->snap) {
    mark(w);
    return;
  }
  /* a widget the client never saw: only its gaining the focus matters
   * (the client is told about it), and its name feeding an ancestor's */
  AwraWidget *win = awra_window_of(w);
  if (win && awra_focus_of(win) == w && g_backend) {
    awra_a11y_expose(w);
    struct AwraA11y *nx = a11y_of(w);
    if (nx && nx->snap) {
      nx->snap->states &= ~(unsigned)AWRA_A11Y_FOCUSED;
      mark(w);
    }
    return;
  }
  for (BKNode *n = w->parent; n; n = n->parent) {
    if (awra_is_widget(n) && awra_aux(n)->a11y && awra_aux(n)->a11y->snap) {
      mark(n);
      return;
    }
  }
}

void awra_a11y_dirty_tree(AwraWidget *w)
{
  if (awra_is_widget(w)) {
    mark(w);
  }
  for (int i = 0; i < w->nkids; i++) {
    awra_a11y_dirty_tree(w->kids[i]);
  }
}

static void text_diff(AwraWidget *w, const char *old, const char *now)
{
  if (!g_backend->text) {
    return;
  }
  old = old ? old : "";
  now = now ? now : "";
  const size_t lo = strlen(old), ln = strlen(now);
  size_t p = 0;
  while (p < lo && p < ln && old[p] == now[p]) {
    p++;
  }
  while (p > 0 && ((unsigned char)old[p] & 0xC0) == 0x80) {
    p--;
  }
  size_t so = lo, sn = ln;
  while (so > p && sn > p && old[so - 1] == now[sn - 1]) {
    so--;
    sn--;
  }
  while (so < lo && ((unsigned char)old[so] & 0xC0) == 0x80) {
    so++;
    sn++;
  }
  const int at = awra_utf8_chars(old, (int)p);
  if (so > p) {
    g_backend->text(w, false, at, awra_utf8_chars(old + p, (int)(so - p)), old + p, (int)(so - p));
  }
  if (sn > p) {
    g_backend->text(w, true, at, awra_utf8_chars(now + p, (int)(sn - p)), now + p, (int)(sn - p));
  }
}

static bool streq(const char *a, const char *b)
{
  return !strcmp(a ? a : "", b ? b : "");
}

static void compare(AwraWidget *w, struct AwraA11y *x)
{
  AwraA11ySnap *s = x->snap;
  AwraA11yInfo info;
  awra_a11y_info(w, &info);
  const unsigned states = awra_a11y_states(w, &info);
  if (info.role != s->role) {
    s->role = (unsigned char)info.role;
    if (g_backend->role) {
      g_backend->role(w);
    }
  }
  char *name = awra_a11y_name(w);
  if (!streq(name, s->name)) {
    free(s->name);
    s->name = name;
    if (g_backend->name) {
      g_backend->name(w);
    }
  }
  else {
    free(name);
  }
  char *desc = awra_a11y_description(w);
  if (!streq(desc, s->description)) {
    free(s->description);
    s->description = desc;
    if (g_backend->description) {
      g_backend->description(w);
    }
  }
  else {
    free(desc);
  }
  const double value = info.has_value ? info.value : 0.0;
  if (value != s->value) {
    s->value = value;
    if (g_backend->value) {
      g_backend->value(w);
    }
  }
  if (info.text || s->text) {
    char *text = text_dup(&info);
    if (!streq(text, s->text)) {
      char *old = s->text;
      s->text = text;
      text_diff(w, old, text);
      free(old);
    }
    else {
      free(text);
    }
    if (info.caret != s->caret) {
      s->caret = info.caret;
      if (g_backend->caret && info.caret >= 0) {
        g_backend->caret(w, info.caret);
      }
    }
    if (info.sel_start != s->sel_start || info.sel_end != s->sel_end) {
      s->sel_start = info.sel_start;
      s->sel_end = info.sel_end;
      if (g_backend->text_selection) {
        g_backend->text_selection(w);
      }
    }
  }
  const unsigned diff = states ^ s->states;
  /* focus last: what changed in a widget is told before it is focused */
  s->states = states;
  for (unsigned bit = 1; bit && diff; bit <<= 1) {
    if ((diff & bit) && bit != AWRA_A11Y_FOCUSED && g_backend->state) {
      g_backend->state(w, -1, bit, (states & bit) != 0);
    }
  }
  if ((diff & AWRA_A11Y_SELECTED) && g_backend->selection) {
    AwraWidget *p = awra_a11y_parent(w);
    if (p) {
      g_backend->selection(p);
    }
  }
  const int nitems = awra_a11y_n_items(w);
  if (nitems != s->nitems) {
    if (g_backend->items) {
      g_backend->items(w, s->nitems, nitems);
    }
    s->nitems = nitems;
    s->item_focus = -1;
  }
  int focus_item = -1;
  for (int i = 0; i < nitems && (states & AWRA_A11Y_FOCUSED); i++) {
    AwraA11yInfo ii;
    awra_a11y_item_info(w, i, &ii, NULL);
    if (ii.flags & AWRA_A11Y_FOCUSED) {
      focus_item = i;
      break;
    }
  }
  if ((diff & AWRA_A11Y_FOCUSED) && g_backend->state) {
    if (!(states & AWRA_A11Y_FOCUSED) && s->item_focus >= 0) {
      g_backend->state(w, s->item_focus, AWRA_A11Y_FOCUSED, false);
    }
    g_backend->state(w, -1, AWRA_A11Y_FOCUSED, (states & AWRA_A11Y_FOCUSED) != 0);
  }
  if (focus_item != s->item_focus) {
    if (g_backend->state) {
      if (s->item_focus >= 0 && s->item_focus < nitems && (states & AWRA_A11Y_FOCUSED)) {
        g_backend->state(w, s->item_focus, AWRA_A11Y_FOCUSED, false);
      }
      if (focus_item >= 0) {
        g_backend->state(w, focus_item, AWRA_A11Y_FOCUSED, true);
      }
    }
    s->item_focus = focus_item;
  }
}

void awra_a11y_flush(void)
{
  if (!g_backend) {
    return;
  }
  /* ancestors named from their content and widgets labelled by a dirty
   * one may have a new name: they are compared too */
  for (int i = 0; i < g_ndirty; i++) {
    AwraWidget *w = g_dirty[i];
    if (!w) {
      continue;
    }
    int depth = 0;
    for (BKNode *n = w->parent; n && depth < 6; n = n->parent) {
      if (!awra_is_widget(n)) {
        continue;
      }
      depth++;
      if (awra_aux(n)->a11y && awra_aux(n)->a11y->snap &&
          awra_a11y_role_named_from_content((AwraAccessibleRole)awra_aux(n)->a11y->snap->role))
      {
        mark(n);
      }
    }
    const struct AwraA11y *x = a11y_of(w);
    if (x && x->targeted) {
      for (int k = 0; k < g_nholders; k++) {
        mark(g_holders[k]);
      }
    }
  }
  for (int i = 0; i < g_ndirty; i++) {
    AwraWidget *w = g_dirty[i];
    if (!w) {
      continue;
    }
    struct AwraA11y *x = a11y_of(w);
    if (!x) {
      continue;
    }
    x->dirty = 0;
    if (x->snap) {
      awra_widget_ref(w);
      compare(w, x);
      g_dirty[i] = NULL;
      awra_widget_unref(w);
    }
    if (!g_backend) {
      break; /* a handler turned it off */
    }
  }
  g_ndirty = 0;
}

void awra_a11y_child_changed_(AwraWidget *parent, AwraWidget *child, bool added)
{
  if (!g_backend || !g_backend->children || !awra_is_widget(child)) {
    return;
  }
  /* the presented parent */
  AwraWidget *p = parent;
  while (p && !(awra_is_widget(p) && awra_a11y_presented(p))) {
    p = p->parent;
  }
  if (!p || !awra_a11y_snap(p)) {
    return;
  }
  if (awra_a11y_presented(child)) {
    const int index = awra_a11y_index_in_parent(child);
    g_backend->children(p, child, index, added);
  }
  else {
    AwraWidget *kids[64];
    const int n = awra_a11y_children(child, kids, 64);
    for (int i = 0; i < n && i < 64; i++) {
      g_backend->children(p, kids[i], awra_a11y_index_in_parent(kids[i]), added);
    }
  }
  if (!added) {
    /* what the client saw inside it no longer has pending changes there */
  }
}

void awra_a11y_window_changed_(AwraWidget *window, int what)
{
  if (g_backend && g_backend->window) {
    if (what == AWRA_A11Y_WINDOW_ACTIVATE || what == AWRA_A11Y_WINDOW_DEACTIVATE) {
      awra_a11y_dirty_(window);
      /* the focused widget's focused state follows the window's */
      AwraWidget *f = awra_focus_of(window);
      if (f) {
        awra_a11y_dirty_(f);
      }
    }
    g_backend->window(window, what);
  }
}

void awra_accessible_announce(AwraWidget *w, const char *message,
                              AwraAccessibleAnnouncementPriority priority)
{
  if (g_backend && g_backend->announce && message) {
    g_backend->announce(w, message, (int)priority);
  }
}

bool awra_accessible_active(void)
{
  return awra_a11y_on != 0;
}

void awra_a11y_set_backend(const AwraA11yBackend *backend)
{
  if (backend == g_backend) {
    return;
  }
  if (!backend) {
    const AwraA11yBackend *old = g_backend;
    g_backend = NULL;
    awra_a11y_on = 0;
    /* every exposed widget is in the id table: drop their snapshots */
    for (uint32_t i = 0; i < g_ids_cap; i++) {
      if (g_ids[i].id && g_ids[i].id != UINT32_MAX && g_ids[i].w) {
        struct AwraA11y *x = a11y_of(g_ids[i].w);
        if (x) {
          x->dirty = 0;
          if (old && old->freed && x->snap) {
            old->freed(g_ids[i].w, x->snap);
          }
          if (x->snap) {
            free(x->snap->name);
            free(x->snap->description);
            free(x->snap->text);
            free(x->snap);
            x->snap = NULL;
          }
        }
      }
    }
    free(g_ids);
    g_ids = NULL;
    g_ids_cap = g_ids_used = 0;
    free(g_dirty);
    g_dirty = NULL;
    g_ndirty = g_capdirty = 0;
    return;
  }
  g_backend = backend;
  awra_a11y_on = 1;
}

void awra_a11y_free(AwraWidget *w)
{
  struct AwraA11y *x = awra_aux(w)->a11y;
  if (!x) {
    return;
  }
  if (x->dirty) {
    for (int i = 0; i < g_ndirty; i++) {
      if (g_dirty[i] == w) {
        g_dirty[i] = NULL;
      }
    }
  }
  snap_free(w, x);
  if (x->holder) {
    holders_remove(w);
  }
  if (x->targeted) {
    /* weak relations: drop it from every relation that names it */
    for (int i = 0; i < g_nholders; i++) {
      const struct AwraA11y *hx = a11y_of(g_holders[i]);
      for (AwraA11yAttr *t = hx ? hx->attrs : NULL; t; t = t->next) {
        if (t->type != TYPE_REFS) {
          continue;
        }
        int k = 0;
        for (int j = 0; j < t->u.refs.n; j++) {
          if (t->u.refs.v[j] != w) {
            t->u.refs.v[k++] = t->u.refs.v[j];
          }
        }
        t->u.refs.n = k;
      }
    }
  }
  for (AwraA11yAttr *t = x->attrs; t;) {
    AwraA11yAttr *next = t->next;
    attr_clear_value(t);
    free(t);
    t = next;
  }
  free(x);
  awra_aux(w)->a11y = NULL;
  if (g_nholders == 0 && g_holders) {
    free(g_holders);
    g_holders = NULL;
    g_capholders = 0;
  }
}

#if !defined(__linux__) && !defined(_WIN32)
void awra_a11y_platform_start(void) {}
void awra_a11y_platform_stop(void) {}
#endif
