/* Awra 2 on AT-SPI2 (Linux): the accessibility tree on the accessibility
 * bus, as GTK 4's AT-SPI context and root expose it.
 *
 * Costs nothing while no assistive technology runs: after the first window
 * shows, Awra asks the session bus (without starting anything) whether
 * org.a11y.Bus runs and whether its org.a11y.Status says IsEnabled or
 * ScreenReaderEnabled, and watches for that to change. Only then does it
 * connect to the accessibility bus, embed its root in the registry and turn
 * the model on (a11y.h); widgets are exposed as clients ask for them, and
 * events are sent only while a client listens for some.
 *
 * Interfaces: Accessible, Application (root), Component, Action, Text,
 * EditableText, Value, Selection, Cache (empty), Properties.
 *
 * AWRA_A11Y=1 forces the bus on (tests); NO_AT_BRIDGE=1, GTK_A11Y=none or
 * AWRA_A11Y=0 keep it off.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)

#include "a11y.h"
#include "dbus.h"

#include <ctype.h>
#include <locale.h>
#include <stdio.h>

#define ROOT_PATH "/org/a11y/atspi/accessible/root"
#define OBJ_PREFIX "/org/a11y/atspi/accessible/"
#define NULL_PATH "/org/a11y/atspi/null"
#define REGISTRY "org.a11y.atspi.Registry"
#define REGISTRY_PATH "/org/a11y/atspi/registry"

#define I_ACCESSIBLE "org.a11y.atspi.Accessible"
#define I_APPLICATION "org.a11y.atspi.Application"
#define I_COMPONENT "org.a11y.atspi.Component"
#define I_ACTION "org.a11y.atspi.Action"
#define I_TEXT "org.a11y.atspi.Text"
#define I_EDITABLE "org.a11y.atspi.EditableText"
#define I_VALUE "org.a11y.atspi.Value"
#define I_SELECTION "org.a11y.atspi.Selection"
#define I_PROPS "org.freedesktop.DBus.Properties"
#define I_EV_OBJECT "org.a11y.atspi.Event.Object"
#define I_EV_WINDOW "org.a11y.atspi.Event.Window"

static bool g_started;
static AwraDBus *g_session, *g_bus;
static bool g_enabled; /* the session says an AT runs (or forced) */
static bool g_forced;
static bool g_registered;
static char *g_desktop_name, *g_desktop_path;
static int g_listeners; /* event listeners registered with the registry */
static int32_t g_app_id;

/* ======================================================================= */
/* Objects */

typedef struct Obj {
  AwraWidget *w; /* NULL: the application root */
  int item;      /* -1: the widget itself */
} Obj;

static bool parse_path(const char *path, Obj *o)
{
  o->w = NULL;
  o->item = -1;
  if (!path) {
    return false;
  }
  if (!strcmp(path, ROOT_PATH)) {
    return true;
  }
  const size_t n = strlen(OBJ_PREFIX);
  if (strncmp(path, OBJ_PREFIX, n) != 0) {
    return false;
  }
  char *end;
  const unsigned long id = strtoul(path + n, &end, 10);
  if (end == path + n) {
    return false;
  }
  if (*end == '_') {
    o->item = (int)strtol(end + 1, &end, 10);
  }
  if (*end) {
    return false;
  }
  o->w = awra_a11y_lookup((uint32_t)id);
  return o->w != NULL;
}

static void obj_path(const Obj *o, char *out, size_t size)
{
  if (!o->w) {
    snprintf(out, size, ROOT_PATH);
    return;
  }
  const uint32_t id = awra_a11y_expose(o->w);
  if (o->item >= 0) {
    snprintf(out, size, OBJ_PREFIX "%u_%d", id, o->item);
  }
  else {
    snprintf(out, size, OBJ_PREFIX "%u", id);
  }
}

static void w_ref(AwraDBusW *w, AwraWidget *wd, int item)
{
  char path[96];
  Obj o = {wd, item};
  obj_path(&o, path, sizeof(path));
  awra_dbus_w_struct(w);
  awra_dbus_w_str(w, awra_dbus_unique_name(g_bus));
  awra_dbus_w_str(w, path);
}

static void w_null_ref(AwraDBusW *w)
{
  awra_dbus_w_struct(w);
  awra_dbus_w_str(w, "");
  awra_dbus_w_str(w, NULL_PATH);
}

static void w_root_ref(AwraDBusW *w)
{
  w_ref(w, NULL, -1);
}

static void w_desktop_ref(AwraDBusW *w)
{
  if (g_desktop_name && g_desktop_path) {
    awra_dbus_w_struct(w);
    awra_dbus_w_str(w, g_desktop_name);
    awra_dbus_w_str(w, g_desktop_path);
  }
  else {
    w_null_ref(w);
  }
}

/* ----------------------------------------------------------------------- */
/* Roles and states */

static unsigned atspi_role(AwraAccessibleRole role, const AwraA11yInfo *info, AwraWidget *w)
{
  if ((info->flags & AWRA_A11Y_PASSWORD) &&
      (role == AWRA_ACCESSIBLE_ROLE_TEXT_BOX || role == AWRA_ACCESSIBLE_ROLE_SEARCH_BOX))
  {
    return 40; /* PASSWORD_TEXT */
  }
  if (w && role == AWRA_ACCESSIBLE_ROLE_GENERIC && awra_widget_is(w, "AwraScrolledWindow")) {
    return 49; /* SCROLL_PANE */
  }
  switch (role) {
    case AWRA_ACCESSIBLE_ROLE_ALERT: return 2;
    case AWRA_ACCESSIBLE_ROLE_ALERT_DIALOG: return 2;
    case AWRA_ACCESSIBLE_ROLE_APPLICATION: return 23;
    case AWRA_ACCESSIBLE_ROLE_ARTICLE: return 109;
    case AWRA_ACCESSIBLE_ROLE_BANNER: return 110;
    case AWRA_ACCESSIBLE_ROLE_BLOCK_QUOTE: return 105;
    case AWRA_ACCESSIBLE_ROLE_BUTTON: return 43;
    case AWRA_ACCESSIBLE_ROLE_CAPTION: return 81;
    case AWRA_ACCESSIBLE_ROLE_CELL: return 56;
    case AWRA_ACCESSIBLE_ROLE_CHECKBOX: return 7;
    case AWRA_ACCESSIBLE_ROLE_COLUMN_HEADER: return 10;
    case AWRA_ACCESSIBLE_ROLE_COMBO_BOX: return 11;
    case AWRA_ACCESSIBLE_ROLE_COMMENT: return 97;
    case AWRA_ACCESSIBLE_ROLE_DIALOG: return 16;
    case AWRA_ACCESSIBLE_ROLE_DOCUMENT: return 94;
    case AWRA_ACCESSIBLE_ROLE_FEED: return 39;
    case AWRA_ACCESSIBLE_ROLE_FORM: return 87;
    case AWRA_ACCESSIBLE_ROLE_GENERIC: return 39;
    case AWRA_ACCESSIBLE_ROLE_GRID: return 55;
    case AWRA_ACCESSIBLE_ROLE_GRID_CELL: return 56;
    case AWRA_ACCESSIBLE_ROLE_GROUP: return 99;
    case AWRA_ACCESSIBLE_ROLE_HEADING: return 83;
    case AWRA_ACCESSIBLE_ROLE_IMG: return 27;
    case AWRA_ACCESSIBLE_ROLE_INPUT: return 79;
    case AWRA_ACCESSIBLE_ROLE_LABEL: return 29;
    case AWRA_ACCESSIBLE_ROLE_LANDMARK: return 110;
    case AWRA_ACCESSIBLE_ROLE_LEGEND: return 29;
    case AWRA_ACCESSIBLE_ROLE_LINK: return 88;
    case AWRA_ACCESSIBLE_ROLE_LIST: return 31;
    case AWRA_ACCESSIBLE_ROLE_LIST_BOX: return 98;
    case AWRA_ACCESSIBLE_ROLE_LIST_ITEM: return 32;
    case AWRA_ACCESSIBLE_ROLE_LOG: return 111;
    case AWRA_ACCESSIBLE_ROLE_MAIN: return 110;
    case AWRA_ACCESSIBLE_ROLE_MARQUEE: return 112;
    case AWRA_ACCESSIBLE_ROLE_MATH: return 113;
    case AWRA_ACCESSIBLE_ROLE_METER: return 103;
    case AWRA_ACCESSIBLE_ROLE_MENU: return 33;
    case AWRA_ACCESSIBLE_ROLE_MENU_BAR: return 34;
    case AWRA_ACCESSIBLE_ROLE_MENU_ITEM: return 35;
    case AWRA_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX: return 8;
    case AWRA_ACCESSIBLE_ROLE_MENU_ITEM_RADIO: return 45;
    case AWRA_ACCESSIBLE_ROLE_NAVIGATION: return 110;
    case AWRA_ACCESSIBLE_ROLE_NOTE: return 124;
    case AWRA_ACCESSIBLE_ROLE_OPTION: return 32;
    case AWRA_ACCESSIBLE_ROLE_PARAGRAPH: return 73;
    case AWRA_ACCESSIBLE_ROLE_PROGRESS_BAR: return 42;
    case AWRA_ACCESSIBLE_ROLE_RADIO: return 44;
    case AWRA_ACCESSIBLE_ROLE_RADIO_GROUP: return 99;
    case AWRA_ACCESSIBLE_ROLE_REGION: return 20;
    case AWRA_ACCESSIBLE_ROLE_ROW: return 90;
    case AWRA_ACCESSIBLE_ROLE_ROW_GROUP: return 99;
    case AWRA_ACCESSIBLE_ROLE_ROW_HEADER: return 47;
    case AWRA_ACCESSIBLE_ROLE_SCROLLBAR: return 48;
    case AWRA_ACCESSIBLE_ROLE_SEARCH: return 87;
    case AWRA_ACCESSIBLE_ROLE_SEARCH_BOX: return 79;
    case AWRA_ACCESSIBLE_ROLE_SECTION: return 85;
    case AWRA_ACCESSIBLE_ROLE_SEPARATOR: return 50;
    case AWRA_ACCESSIBLE_ROLE_SLIDER: return 51;
    case AWRA_ACCESSIBLE_ROLE_SPIN_BUTTON: return 52;
    case AWRA_ACCESSIBLE_ROLE_STATUS: return 54;
    case AWRA_ACCESSIBLE_ROLE_SWITCH: return 130;
    case AWRA_ACCESSIBLE_ROLE_TAB: return 37;
    case AWRA_ACCESSIBLE_ROLE_TABLE: return 55;
    case AWRA_ACCESSIBLE_ROLE_TAB_LIST: return 38;
    case AWRA_ACCESSIBLE_ROLE_TAB_PANEL: return 39;
    case AWRA_ACCESSIBLE_ROLE_TERMINAL: return 60;
    case AWRA_ACCESSIBLE_ROLE_TEXT_BOX: return 61;
    case AWRA_ACCESSIBLE_ROLE_TIME: return 61;
    case AWRA_ACCESSIBLE_ROLE_TIMER: return 115;
    case AWRA_ACCESSIBLE_ROLE_TOGGLE_BUTTON: return 62;
    case AWRA_ACCESSIBLE_ROLE_TOOLBAR: return 63;
    case AWRA_ACCESSIBLE_ROLE_TOOLTIP: return 64;
    case AWRA_ACCESSIBLE_ROLE_TREE: return 65;
    case AWRA_ACCESSIBLE_ROLE_TREE_GRID: return 66;
    case AWRA_ACCESSIBLE_ROLE_TREE_ITEM: return 91;
    case AWRA_ACCESSIBLE_ROLE_WINDOW: return 23; /* FRAME */
    case AWRA_ACCESSIBLE_ROLE_NONE:
    case AWRA_ACCESSIBLE_ROLE_PRESENTATION: return 0;
    default: return 20; /* FILLER */
  }
}

/* flat state -> AtspiStateType */
static const struct {
  unsigned flag;
  unsigned char atspi;
  const char *name;
} g_states[] = {
    {AWRA_A11Y_ACTIVE, 1, "active"},
    {AWRA_A11Y_BUSY, 3, "busy"},
    {AWRA_A11Y_CHECKED, 4, "checked"},
    {AWRA_A11Y_EDITABLE, 7, "editable"},
    {AWRA_A11Y_SENSITIVE, 8, "enabled"},
    {AWRA_A11Y_EXPANDABLE, 9, "expandable"},
    {AWRA_A11Y_EXPANDED, 10, "expanded"},
    {AWRA_A11Y_FOCUSABLE, 11, "focusable"},
    {AWRA_A11Y_FOCUSED, 12, "focused"},
    {AWRA_A11Y_HORIZONTAL, 14, "horizontal"},
    {AWRA_A11Y_MODAL, 16, "modal"},
    {AWRA_A11Y_MULTI_LINE, 17, "multi-line"},
    {AWRA_A11Y_MULTISELECTABLE, 18, "multiselectable"},
    {AWRA_A11Y_PRESSED, 20, "pressed"},
    {AWRA_A11Y_SELECTABLE, 22, "selectable"},
    {AWRA_A11Y_SELECTED, 23, "selected"},
    {AWRA_A11Y_SENSITIVE, 24, "sensitive"},
    {AWRA_A11Y_SHOWING, 25, "showing"},
    {AWRA_A11Y_SINGLE_LINE, 26, "single-line"},
    {AWRA_A11Y_VERTICAL, 29, "vertical"},
    {AWRA_A11Y_VISIBLE, 30, "visible"},
    {AWRA_A11Y_MIXED, 32, "indeterminate"},
    {AWRA_A11Y_REQUIRED, 33, "required"},
    {AWRA_A11Y_INVALID, 36, "invalid-entry"},
    {AWRA_A11Y_DEFAULT, 39, "is-default"},
    {AWRA_A11Y_VISITED, 40, "visited"},
    {AWRA_A11Y_CHECKABLE, 41, "checkable"},
    {AWRA_A11Y_HAS_POPUP, 42, "has-popup"},
    {AWRA_A11Y_READ_ONLY, 43, "read-only"},
};

static void atspi_states(unsigned flat, const AwraA11yInfo *info, uint32_t out[2])
{
  uint64_t s = 0;
  for (size_t i = 0; i < sizeof(g_states) / sizeof(g_states[0]); i++) {
    if (flat & g_states[i].flag) {
      s |= 1ull << g_states[i].atspi;
    }
  }
  if ((flat & AWRA_A11Y_EXPANDABLE) && !(flat & AWRA_A11Y_EXPANDED)) {
    s |= 1ull << 5; /* COLLAPSED */
  }
  if (info->text && info->caret >= 0 && !(flat & AWRA_A11Y_EDITABLE)) {
    s |= 1ull << 38; /* SELECTABLE_TEXT: selectable labels */
  }
  out[0] = (uint32_t)s;
  out[1] = (uint32_t)(s >> 32);
}

/* the object's info; items through their container */
typedef struct Info {
  AwraA11yInfo i;
  double rect[4];
  unsigned flat;
} Info;

static void obj_info(const Obj *o, Info *in)
{
  if (o->item >= 0) {
    awra_a11y_item_info(o->w, o->item, &in->i, in->rect);
    in->flat = awra_a11y_item_states(o->w, &in->i);
  }
  else {
    awra_a11y_info(o->w, &in->i);
    awra_a11y_bounds(o->w, in->rect);
    in->flat = awra_a11y_states(o->w, &in->i);
  }
}

static char *obj_name(const Obj *o)
{
  return o->item >= 0 ? awra_a11y_item_name(o->w, o->item) : awra_a11y_name(o->w);
}

static char *obj_description(const Obj *o)
{
  if (o->item >= 0) {
    AwraA11yInfo i;
    awra_a11y_item_info(o->w, o->item, &i, NULL);
    return awra_strdup(i.description ? i.description : "");
  }
  return awra_a11y_description(o->w);
}

/* children: presented widgets, then virtual items */
static int obj_children(const Obj *o, AwraWidget ***out, int *nitems)
{
  *out = NULL;
  *nitems = 0;
  if (o->item >= 0) {
    return 0;
  }
  if (!o->w) {
    const int n = awra_a11y_windows(NULL, 0);
    AwraWidget **v = n ? (AwraWidget **)malloc(sizeof(*v) * (size_t)n) : NULL;
    if (v) {
      awra_a11y_windows(v, n);
    }
    *out = v;
    return v ? n : 0;
  }
  const int n = awra_a11y_children(o->w, NULL, 0);
  AwraWidget **v = n ? (AwraWidget **)malloc(sizeof(*v) * (size_t)n) : NULL;
  if (v) {
    awra_a11y_children(o->w, v, n);
  }
  *out = v;
  *nitems = awra_a11y_n_items(o->w);
  return v ? n : 0;
}

static int obj_child_count(const Obj *o)
{
  if (o->item >= 0) {
    return 0;
  }
  if (!o->w) {
    return awra_a11y_windows(NULL, 0);
  }
  return awra_a11y_children(o->w, NULL, 0) + awra_a11y_n_items(o->w);
}

static void w_parent_ref(AwraDBusW *w, const Obj *o)
{
  if (!o->w) {
    w_desktop_ref(w);
  }
  else if (o->item >= 0) {
    w_ref(w, o->w, -1);
  }
  else {
    AwraWidget *p = awra_a11y_parent(o->w);
    if (p) {
      w_ref(w, p, -1);
    }
    else {
      w_root_ref(w);
    }
  }
}

static int obj_index_in_parent(const Obj *o)
{
  if (!o->w) {
    return -1;
  }
  if (o->item >= 0) {
    return awra_a11y_children(o->w, NULL, 0) + o->item;
  }
  return awra_a11y_index_in_parent(o->w);
}

/* selection containers: their children or items carry the selected state */
static bool obj_selection(const Obj *o, const Info *in)
{
  if (!o->w || o->item >= 0) {
    return false;
  }
  if (in->i.selection) {
    return true;
  }
  switch (in->i.role) {
    case AWRA_ACCESSIBLE_ROLE_LIST:
    case AWRA_ACCESSIBLE_ROLE_LIST_BOX:
    case AWRA_ACCESSIBLE_ROLE_TAB_LIST:
    case AWRA_ACCESSIBLE_ROLE_GRID:
    case AWRA_ACCESSIBLE_ROLE_TREE:
    case AWRA_ACCESSIBLE_ROLE_MENU:
    case AWRA_ACCESSIBLE_ROLE_RADIO_GROUP:
      return true;
    default:
      return false;
  }
}

static bool obj_text(const Info *in)
{
  return in->i.text != NULL;
}

static bool obj_editable(const Obj *o, const Info *in)
{
  return o->item < 0 && in->i.text && (in->flat & AWRA_A11Y_EDITABLE) &&
         (in->i.role == AWRA_ACCESSIBLE_ROLE_TEXT_BOX || in->i.role == AWRA_ACCESSIBLE_ROLE_SEARCH_BOX ||
          in->i.role == AWRA_ACCESSIBLE_ROLE_SPIN_BUTTON);
}

static bool obj_action(const Info *in)
{
  return in->i.action != NULL;
}

static bool obj_value(const Info *in)
{
  return in->i.has_value;
}

/* ----------------------------------------------------------------------- */
/* Coordinates */

static AwraWidget *obj_window(const Obj *o)
{
  return o->w ? awra_window_of(o->w) : NULL;
}

/* rect in the window (logical) -> the coord type asked */
static void to_coords(const Obj *o, double r[4], unsigned coord)
{
  if (coord == 0) { /* screen */
    double x, y;
    AwraWidget *win = obj_window(o);
    if (win && awra_window_screen_origin(win, &x, &y)) {
      r[0] += x;
      r[1] += y;
    }
  }
  else if (coord == 2 && o->w) { /* parent */
    double pr[4] = {0, 0, 0, 0};
    if (o->item >= 0) {
      awra_a11y_bounds(o->w, pr);
    }
    else {
      AwraWidget *p = awra_a11y_parent(o->w);
      if (p) {
        awra_a11y_bounds(p, pr);
      }
    }
    r[0] -= pr[0];
    r[1] -= pr[1];
  }
}

static void from_coords(const Obj *o, double *x, double *y, unsigned coord)
{
  double r[4] = {0, 0, 0, 0};
  to_coords(o, r, coord);
  *x -= r[0];
  *y -= r[1];
}

/* ======================================================================= */
/* Text boundaries */

typedef struct U32 {
  uint32_t *u;
  int n;
} U32;

static U32 decode(const char *s)
{
  U32 r = {NULL, 0};
  const int n = awra_utf8_chars(s, -1);
  r.u = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)(n + 1));
  if (!r.u) {
    return r;
  }
  const unsigned char *p = (const unsigned char *)s;
  while (*p && r.n < n) {
    uint32_t c = *p;
    int k = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
    if (k > 1) {
      c &= k == 2 ? 0x1F : k == 3 ? 0x0F : 0x07;
      for (int i = 1; i < k && p[i]; i++) {
        c = (c << 6) | (p[i] & 0x3F);
      }
    }
    r.u[r.n++] = c;
    for (int i = 0; i < k && *p; i++) {
      p++;
    }
  }
  return r;
}

static bool is_space(uint32_t c)
{
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0xA0 || c == 0x2028 ||
         c == 0x2029 || c == 0x3000;
}

static bool is_word(uint32_t c)
{
  return !is_space(c) && !(c < 128 && ispunct((int)c));
}

static bool is_end_punct(uint32_t c)
{
  return c == '.' || c == '!' || c == '?' || c == 0x2026 || c == 0x3002;
}

/* boundary predicates (AtspiTextBoundaryType, granularity) */
static bool at_boundary(const U32 *t, int i, int type)
{
  const uint32_t *u = t->u;
  const int n = t->n;
  if (i <= 0) {
    return true;
  }
  if (i >= n) {
    return true;
  }
  switch (type) {
    case 0: /* char */
      return true;
    case 1: /* word start */
      return is_word(u[i]) && !is_word(u[i - 1]);
    case 2: /* word end */
      return is_word(u[i - 1]) && !is_word(u[i]);
    case 3: /* sentence start */
      if (is_space(u[i])) {
        return false;
      }
      for (int k = i - 1; k >= 0; k--) {
        if (u[k] == '\n') {
          return true;
        }
        if (!is_space(u[k])) {
          return is_end_punct(u[k]) && k < i - 1;
        }
      }
      return true;
    case 4: /* sentence end */
      return is_end_punct(u[i - 1]) && (is_space(u[i]));
    case 5: /* line start */
      return u[i - 1] == '\n';
    case 6: /* line end */
      return u[i] == '\n';
    default:
      return true;
  }
}

/* the range of boundary `type` containing offset */
static void boundary_at(const U32 *t, int offset, int type, int *s, int *e)
{
  if (offset < 0) {
    offset = 0;
  }
  if (offset > t->n) {
    offset = t->n;
  }
  int a = offset;
  while (a > 0 && !at_boundary(t, a, type)) {
    a--;
  }
  int b = offset + 1;
  while (b < t->n && !at_boundary(t, b, type)) {
    b++;
  }
  if (b > t->n) {
    b = t->n;
  }
  *s = a;
  *e = b;
}

static void put_range(AwraDBusW *w, const char *text, int s, int e)
{
  const int bs = awra_utf8_offset(text, s), be = awra_utf8_offset(text, e);
  awra_dbus_w_str_len(w, text + bs, (size_t)(be > bs ? be - bs : 0));
}

/* ======================================================================= */
/* Replies */

static void reply_empty(const AwraDBusMsg *m)
{
  awra_dbus_reply(g_bus, m, NULL, NULL);
}

static void reply_bool(const AwraDBusMsg *m, bool v)
{
  AwraDBusW w;
  awra_dbus_w_init(&w);
  awra_dbus_w_bool(&w, v);
  awra_dbus_reply(g_bus, m, "b", &w);
  awra_dbus_w_free(&w);
}

static void reply_int(const AwraDBusMsg *m, int32_t v)
{
  AwraDBusW w;
  awra_dbus_w_init(&w);
  awra_dbus_w_i32(&w, v);
  awra_dbus_reply(g_bus, m, "i", &w);
  awra_dbus_w_free(&w);
}

static void reply_uint(const AwraDBusMsg *m, uint32_t v)
{
  AwraDBusW w;
  awra_dbus_w_init(&w);
  awra_dbus_w_u32(&w, v);
  awra_dbus_reply(g_bus, m, "u", &w);
  awra_dbus_w_free(&w);
}

static void reply_str(const AwraDBusMsg *m, const char *v)
{
  AwraDBusW w;
  awra_dbus_w_init(&w);
  awra_dbus_w_str(&w, v);
  awra_dbus_reply(g_bus, m, "s", &w);
  awra_dbus_w_free(&w);
}

static void reply_ref(const AwraDBusMsg *m, AwraWidget *wd, int item, bool null)
{
  AwraDBusW w;
  awra_dbus_w_init(&w);
  if (null) {
    w_null_ref(&w);
  }
  else {
    w_ref(&w, wd, item);
  }
  awra_dbus_reply(g_bus, m, "(so)", &w);
  awra_dbus_w_free(&w);
}

static void reply_unknown(const AwraDBusMsg *m)
{
  awra_dbus_error(g_bus, m, "org.freedesktop.DBus.Error.UnknownMethod", m->member);
}

/* ----------------------------------------------------------------------- */
/* Accessible */

static void w_interfaces(AwraDBusW *w, const Obj *o, const Info *in)
{
  const AwraDBusArray a = awra_dbus_w_array_open(w, 4);
  awra_dbus_w_str(w, I_ACCESSIBLE);
  if (!o->w) {
    awra_dbus_w_str(w, I_APPLICATION);
  }
  else {
    awra_dbus_w_str(w, I_COMPONENT);
    if (obj_action(in)) {
      awra_dbus_w_str(w, I_ACTION);
    }
    if (obj_text(in)) {
      awra_dbus_w_str(w, I_TEXT);
    }
    if (obj_editable(o, in)) {
      awra_dbus_w_str(w, I_EDITABLE);
    }
    if (obj_value(in)) {
      awra_dbus_w_str(w, I_VALUE);
    }
    if (obj_selection(o, in)) {
      awra_dbus_w_str(w, I_SELECTION);
    }
  }
  awra_dbus_w_array_close(w, a);
}

static const char *app_name(void)
{
  static char name[128];
  const char *id = awra_app_id();
  const char *dot = strrchr(id, '.');
  snprintf(name, sizeof(name), "%s", dot && dot[1] ? dot + 1 : id);
  return name;
}

static void w_attributes(AwraDBusW *w, const Obj *o, const Info *in)
{
  const AwraDBusArray a = awra_dbus_w_array_open(w, 8);
  awra_dbus_w_struct(w);
  awra_dbus_w_str(w, "toolkit");
  awra_dbus_w_str(w, "Awra");
  if (o->w && in->i.placeholder && in->i.placeholder[0]) {
    awra_dbus_w_struct(w);
    awra_dbus_w_str(w, "placeholder-text");
    awra_dbus_w_str(w, in->i.placeholder);
  }
  if (o->w && in->i.level > 0) {
    char lv[16];
    snprintf(lv, sizeof(lv), "%d", in->i.level);
    awra_dbus_w_struct(w);
    awra_dbus_w_str(w, "level");
    awra_dbus_w_str(w, lv);
  }
  if (o->w && in->i.shortcut) {
    awra_dbus_w_struct(w);
    awra_dbus_w_str(w, "keyshortcuts");
    awra_dbus_w_str(w, in->i.shortcut);
  }
  if (o->w && o->item < 0) {
    const char *rd = awra_a11y_property_string(o->w, AWRA_ACCESSIBLE_PROPERTY_ROLE_DESCRIPTION);
    if (rd) {
      awra_dbus_w_struct(w);
      awra_dbus_w_str(w, "roledescription");
      awra_dbus_w_str(w, rd);
    }
    int v;
    static const struct {
      AwraAccessibleRelation r;
      const char *name;
    } nums[] = {{AWRA_ACCESSIBLE_RELATION_POS_IN_SET, "posinset"},
                {AWRA_ACCESSIBLE_RELATION_SET_SIZE, "setsize"},
                {AWRA_ACCESSIBLE_RELATION_ROW_INDEX, "rowindex"},
                {AWRA_ACCESSIBLE_RELATION_COL_INDEX, "colindex"},
                {AWRA_ACCESSIBLE_RELATION_ROW_COUNT, "rowcount"},
                {AWRA_ACCESSIBLE_RELATION_COL_COUNT, "colcount"}};
    for (size_t k = 0; k < sizeof(nums) / sizeof(nums[0]); k++) {
      if (awra_a11y_relation_number(o->w, nums[k].r, &v)) {
        char b[16];
        snprintf(b, sizeof(b), "%d", v);
        awra_dbus_w_struct(w);
        awra_dbus_w_str(w, nums[k].name);
        awra_dbus_w_str(w, b);
      }
    }
  }
  awra_dbus_w_array_close(w, a);
}

static void w_relation_set(AwraDBusW *w, const Obj *o)
{
  static const struct {
    AwraAccessibleRelation rel;
    unsigned atspi;
    AwraAccessibleRelation reverse; /* also reported from the other side */
    unsigned reverse_atspi;
  } map[] = {
      {AWRA_ACCESSIBLE_RELATION_LABELLED_BY, 2, AWRA_ACCESSIBLE_RELATION_LABELLED_BY, 1},
      {AWRA_ACCESSIBLE_RELATION_LABEL_FOR, 1, AWRA_ACCESSIBLE_RELATION_LABEL_FOR, 2},
      {AWRA_ACCESSIBLE_RELATION_CONTROLS, 3, AWRA_ACCESSIBLE_RELATION_CONTROLS, 4},
      {AWRA_ACCESSIBLE_RELATION_CONTROLLED_BY, 4, AWRA_ACCESSIBLE_RELATION_CONTROLLED_BY, 3},
      {AWRA_ACCESSIBLE_RELATION_DESCRIBED_BY, 18, AWRA_ACCESSIBLE_RELATION_DESCRIBED_BY, 17},
      {AWRA_ACCESSIBLE_RELATION_DESCRIPTION_FOR, 17, AWRA_ACCESSIBLE_RELATION_DESCRIPTION_FOR, 18},
      {AWRA_ACCESSIBLE_RELATION_DETAILS, 19, AWRA_ACCESSIBLE_RELATION_DETAILS, 20},
      {AWRA_ACCESSIBLE_RELATION_DETAILS_FOR, 20, AWRA_ACCESSIBLE_RELATION_DETAILS_FOR, 19},
      {AWRA_ACCESSIBLE_RELATION_ERROR_MESSAGE, 21, AWRA_ACCESSIBLE_RELATION_ERROR_MESSAGE, 22},
      {AWRA_ACCESSIBLE_RELATION_ERROR_MESSAGE_FOR, 22, AWRA_ACCESSIBLE_RELATION_ERROR_MESSAGE_FOR, 21},
      {AWRA_ACCESSIBLE_RELATION_FLOW_TO, 10, AWRA_ACCESSIBLE_RELATION_FLOW_TO, 11},
      {AWRA_ACCESSIBLE_RELATION_FLOW_FROM, 11, AWRA_ACCESSIBLE_RELATION_FLOW_FROM, 10},
  };
  const AwraDBusArray a = awra_dbus_w_array_open(w, 8);
  if (o->w && o->item < 0) {
    AwraWidget *t[32];
    for (size_t k = 0; k < sizeof(map) / sizeof(map[0]); k++) {
      for (int pass = 0; pass < 2; pass++) {
        const int n = pass == 0 ? awra_a11y_relation(o->w, map[k].rel, t, 32) :
                                  awra_a11y_relation_reverse(o->w, map[k].reverse, t, 32);
        if (n <= 0) {
          continue;
        }
        awra_dbus_w_struct(w);
        awra_dbus_w_u32(w, pass == 0 ? map[k].atspi : map[k].reverse_atspi);
        const AwraDBusArray b = awra_dbus_w_array_open(w, 8);
        for (int i = 0; i < n && i < 32; i++) {
          w_ref(w, t[i], -1);
        }
        awra_dbus_w_array_close(w, b);
      }
    }
  }
  awra_dbus_w_array_close(w, a);
}

static bool accessible_prop(AwraDBusW *w, const Obj *o, const char *prop, bool variant)
{
#define V(sig)                     \
  if (variant) {                   \
    awra_dbus_w_variant(w, sig);   \
  }
  if (!strcmp(prop, "Name")) {
    V("s");
    if (!o->w) {
      awra_dbus_w_str(w, app_name());
    }
    else {
      char *n = obj_name(o);
      awra_dbus_w_str(w, n);
      free(n);
    }
  }
  else if (!strcmp(prop, "Description")) {
    V("s");
    if (!o->w) {
      awra_dbus_w_str(w, "");
    }
    else {
      char *d = obj_description(o);
      awra_dbus_w_str(w, d);
      free(d);
    }
  }
  else if (!strcmp(prop, "Parent")) {
    V("(so)");
    w_parent_ref(w, o);
  }
  else if (!strcmp(prop, "ChildCount")) {
    V("i");
    awra_dbus_w_i32(w, obj_child_count(o));
  }
  else if (!strcmp(prop, "Locale")) {
    V("s");
    const char *l = setlocale(LC_MESSAGES, NULL);
    awra_dbus_w_str(w, l ? l : "C");
  }
  else if (!strcmp(prop, "AccessibleId")) {
    V("s");
    const char *id = o->w && o->item < 0 ? awra_widget_name(o->w) : NULL;
    awra_dbus_w_str(w, id ? id : "");
  }
  else if (!strcmp(prop, "HelpText")) {
    V("s");
    const char *h = o->w && o->item < 0 ?
                        awra_a11y_property_string(o->w, AWRA_ACCESSIBLE_PROPERTY_HELP_TEXT) :
                        NULL;
    awra_dbus_w_str(w, h ? h : "");
  }
  else {
    return false;
  }
  return true;
#undef V
}

static void accessible_method(const AwraDBusMsg *m, const Obj *o)
{
  const char *mb = m->member;
  AwraDBusR r;
  awra_dbus_r_init(&r, m);
  Info in;
  memset(&in, 0, sizeof(in));
  if (o->w) {
    obj_info(o, &in);
  }
  AwraDBusW w;
  awra_dbus_w_init(&w);
  if (!strcmp(mb, "GetChildAtIndex")) {
    const int idx = awra_dbus_r_i32(&r);
    AwraWidget **kids;
    int nitems;
    const int n = obj_children(o, &kids, &nitems);
    if (idx >= 0 && idx < n) {
      reply_ref(m, kids[idx], -1, false);
    }
    else if (idx >= n && idx < n + nitems) {
      reply_ref(m, o->w, idx - n, false);
    }
    else {
      reply_ref(m, NULL, -1, true);
    }
    free(kids);
  }
  else if (!strcmp(mb, "GetChildren")) {
    AwraWidget **kids;
    int nitems;
    const int n = obj_children(o, &kids, &nitems);
    const AwraDBusArray a = awra_dbus_w_array_open(&w, 8);
    for (int i = 0; i < n; i++) {
      w_ref(&w, kids[i], -1);
    }
    for (int i = 0; i < nitems; i++) {
      w_ref(&w, o->w, i);
    }
    awra_dbus_w_array_close(&w, a);
    awra_dbus_reply(g_bus, m, "a(so)", &w);
    free(kids);
  }
  else if (!strcmp(mb, "GetIndexInParent")) {
    reply_int(m, obj_index_in_parent(o));
  }
  else if (!strcmp(mb, "GetRelationSet")) {
    w_relation_set(&w, o);
    awra_dbus_reply(g_bus, m, "a(ua(so))", &w);
  }
  else if (!strcmp(mb, "GetRole")) {
    reply_uint(m, o->w ? atspi_role(in.i.role, &in.i, o->item < 0 ? o->w : NULL) : 75);
  }
  else if (!strcmp(mb, "GetRoleName") || !strcmp(mb, "GetLocalizedRoleName")) {
    const char *rd = o->w && o->item < 0 && !strcmp(mb, "GetLocalizedRoleName") ?
                         awra_a11y_property_string(o->w, AWRA_ACCESSIBLE_PROPERTY_ROLE_DESCRIPTION) :
                         NULL;
    reply_str(m, rd ? rd : o->w ? awra_a11y_role_name(in.i.role) : "application");
  }
  else if (!strcmp(mb, "GetState")) {
    uint32_t st[2] = {0, 0};
    if (o->w) {
      atspi_states(in.flat, &in.i, st);
    }
    const AwraDBusArray a = awra_dbus_w_array_open(&w, 4);
    awra_dbus_w_u32(&w, st[0]);
    awra_dbus_w_u32(&w, st[1]);
    awra_dbus_w_array_close(&w, a);
    awra_dbus_reply(g_bus, m, "au", &w);
  }
  else if (!strcmp(mb, "GetAttributes")) {
    w_attributes(&w, o, &in);
    awra_dbus_reply(g_bus, m, "a{ss}", &w);
  }
  else if (!strcmp(mb, "GetApplication")) {
    reply_ref(m, NULL, -1, false);
  }
  else if (!strcmp(mb, "GetInterfaces")) {
    w_interfaces(&w, o, &in);
    awra_dbus_reply(g_bus, m, "as", &w);
  }
  else {
    reply_unknown(m);
  }
  awra_dbus_w_free(&w);
}

/* ----------------------------------------------------------------------- */
/* Application (the root) */

static bool application_prop(AwraDBusW *w, const char *prop, bool variant)
{
  const char *s = NULL;
  if (!strcmp(prop, "ToolkitName")) {
    s = "Awra";
  }
  else if (!strcmp(prop, "Version")) {
    s = "2.0";
  }
  else if (!strcmp(prop, "AtspiVersion")) {
    s = "2.1";
  }
  else if (!strcmp(prop, "Id")) {
    if (variant) {
      awra_dbus_w_variant(w, "i");
    }
    awra_dbus_w_i32(w, g_app_id);
    return true;
  }
  else {
    return false;
  }
  if (variant) {
    awra_dbus_w_variant(w, "s");
  }
  awra_dbus_w_str(w, s);
  return true;
}

static void application_method(const AwraDBusMsg *m)
{
  if (!strcmp(m->member, "GetLocale")) {
    const char *l = setlocale(LC_MESSAGES, NULL);
    reply_str(m, l ? l : "C");
  }
  else if (!strcmp(m->member, "RegisterEventListener") ||
           !strcmp(m->member, "DeregisterEventListener"))
  {
    reply_empty(m);
  }
  else {
    reply_unknown(m);
  }
}

/* ----------------------------------------------------------------------- */
/* Component */

static void component_method(const AwraDBusMsg *m, const Obj *o)
{
  AwraDBusR r;
  awra_dbus_r_init(&r, m);
  Info in;
  obj_info(o, &in);
  const char *mb = m->member;
  AwraDBusW w;
  awra_dbus_w_init(&w);
  if (!strcmp(mb, "Contains") || !strcmp(mb, "GetAccessibleAtPoint")) {
    double x = awra_dbus_r_i32(&r), y = awra_dbus_r_i32(&r);
    const unsigned coord = awra_dbus_r_u32(&r);
    from_coords(o, &x, &y, coord);
    const bool inside = x >= in.rect[0] && y >= in.rect[1] && x < in.rect[0] + in.rect[2] &&
                        y < in.rect[1] + in.rect[3];
    if (!strcmp(mb, "Contains")) {
      reply_bool(m, inside);
    }
    else if (!inside || o->item >= 0) {
      reply_ref(m, NULL, -1, true);
    }
    else {
      AwraWidget *win = obj_window(o);
      AwraWidget *hit = win ? awra_a11y_at_point(win, x, y) : NULL;
      /* a descendant of this object */
      bool within = false;
      for (BKNode *n = hit; n; n = n->parent) {
        within |= n == o->w;
      }
      if (hit && within && hit != o->w) {
        /* the child of o on the way down */
        AwraWidget *c = hit;
        while (c && awra_a11y_parent(c) != o->w) {
          c = awra_a11y_parent(c);
        }
        reply_ref(m, c ? c : hit, -1, false);
      }
      else {
        int nitems = awra_a11y_n_items(o->w), found = -1;
        for (int i = 0; i < nitems && found < 0; i++) {
          AwraA11yInfo ii;
          double ir[4];
          awra_a11y_item_info(o->w, i, &ii, ir);
          if (x >= ir[0] && y >= ir[1] && x < ir[0] + ir[2] && y < ir[1] + ir[3]) {
            found = i;
          }
        }
        if (found >= 0) {
          reply_ref(m, o->w, found, false);
        }
        else {
          reply_ref(m, NULL, -1, true);
        }
      }
    }
  }
  else if (!strcmp(mb, "GetExtents") || !strcmp(mb, "GetPosition")) {
    const unsigned coord = awra_dbus_r_u32(&r);
    double rc[4] = {in.rect[0], in.rect[1], in.rect[2], in.rect[3]};
    to_coords(o, rc, coord);
    if (!strcmp(mb, "GetExtents")) {
      awra_dbus_w_struct(&w);
      for (int i = 0; i < 4; i++) {
        awra_dbus_w_i32(&w, (int32_t)floor(rc[i] + 0.5));
      }
      awra_dbus_reply(g_bus, m, "(iiii)", &w);
    }
    else {
      awra_dbus_w_i32(&w, (int32_t)floor(rc[0] + 0.5));
      awra_dbus_w_i32(&w, (int32_t)floor(rc[1] + 0.5));
      awra_dbus_reply(g_bus, m, "ii", &w);
    }
  }
  else if (!strcmp(mb, "GetSize")) {
    awra_dbus_w_i32(&w, (int32_t)floor(in.rect[2] + 0.5));
    awra_dbus_w_i32(&w, (int32_t)floor(in.rect[3] + 0.5));
    awra_dbus_reply(g_bus, m, "ii", &w);
  }
  else if (!strcmp(mb, "GetLayer")) {
    reply_uint(m, o->item < 0 && in.i.role == AWRA_ACCESSIBLE_ROLE_WINDOW ? 7 : 3);
  }
  else if (!strcmp(mb, "GetMDIZOrder")) {
    awra_dbus_w_i16(&w, 0);
    awra_dbus_reply(g_bus, m, "n", &w);
  }
  else if (!strcmp(mb, "GrabFocus")) {
    reply_bool(m, awra_a11y_do(o->w, o->item, AWRA_A11Y_DO_FOCUS, 0));
  }
  else if (!strcmp(mb, "GetAlpha")) {
    awra_dbus_w_double(&w, 1.0);
    awra_dbus_reply(g_bus, m, "d", &w);
  }
  else if (!strcmp(mb, "ScrollTo") || !strcmp(mb, "ScrollToPoint")) {
    bool ok = awra_a11y_do(o->w, o->item, AWRA_A11Y_DO_SCROLL_TO, 0);
    if (!ok && o->item < 0) {
      /* scrolled windows bring a widget into view */
      for (BKNode *n = o->w->parent; n; n = n->parent) {
        if (awra_widget_is(n, "AwraScrolledWindow")) {
          awra_scrolled_scroll_to(n, o->w);
          ok = true;
          break;
        }
      }
    }
    reply_bool(m, ok);
  }
  else if (!strcmp(mb, "SetExtents") || !strcmp(mb, "SetPosition") || !strcmp(mb, "SetSize")) {
    reply_bool(m, false);
  }
  else {
    reply_unknown(m);
  }
  awra_dbus_w_free(&w);
}

/* ----------------------------------------------------------------------- */
/* Action */

static const char *action_description(const char *name)
{
  if (!strcmp(name, "click")) {
    return "Clicks the button";
  }
  if (!strcmp(name, "activate")) {
    return "Activates the widget";
  }
  return "";
}

static void action_method(const AwraDBusMsg *m, const Obj *o)
{
  AwraDBusR r;
  awra_dbus_r_init(&r, m);
  Info in;
  obj_info(o, &in);
  const char *act = in.i.action;
  const char *mb = m->member;
  AwraDBusW w;
  awra_dbus_w_init(&w);
  if (!strcmp(mb, "GetActions")) {
    const AwraDBusArray a = awra_dbus_w_array_open(&w, 8);
    if (act) {
      awra_dbus_w_struct(&w);
      awra_dbus_w_str(&w, act);
      awra_dbus_w_str(&w, action_description(act));
      awra_dbus_w_str(&w, in.i.shortcut ? in.i.shortcut : "");
    }
    awra_dbus_w_array_close(&w, a);
    awra_dbus_reply(g_bus, m, "a(sss)", &w);
  }
  else if (!strcmp(mb, "DoAction")) {
    const int i = awra_dbus_r_i32(&r);
    /* answer first: the action may open a modal loop or close the window */
    reply_bool(m, act && i == 0);
    if (act && i == 0) {
      awra_widget_ref(o->w);
      awra_a11y_do(o->w, o->item, AWRA_A11Y_DO_ACTIVATE, 0);
      awra_widget_unref(o->w);
    }
  }
  else {
    const int i = awra_dbus_r_i32(&r);
    const bool ok = act && i == 0;
    if (!strcmp(mb, "GetName") || !strcmp(mb, "GetLocalizedName")) {
      reply_str(m, ok ? act : "");
    }
    else if (!strcmp(mb, "GetDescription")) {
      reply_str(m, ok ? action_description(act) : "");
    }
    else if (!strcmp(mb, "GetKeyBinding")) {
      reply_str(m, ok && in.i.shortcut ? in.i.shortcut : "");
    }
    else {
      reply_unknown(m);
    }
  }
  awra_dbus_w_free(&w);
}

/* ----------------------------------------------------------------------- */
/* Text */

static void text_range_reply(const AwraDBusMsg *m, const char *text, int s, int e)
{
  AwraDBusW w;
  awra_dbus_w_init(&w);
  put_range(&w, text, s, e);
  awra_dbus_w_i32(&w, s);
  awra_dbus_w_i32(&w, e);
  awra_dbus_reply(g_bus, m, "sii", &w);
  awra_dbus_w_free(&w);
}

static void text_method(const AwraDBusMsg *m, const Obj *o)
{
  AwraDBusR r;
  awra_dbus_r_init(&r, m);
  Info in;
  obj_info(o, &in);
  const char *text = (in.flat & AWRA_A11Y_PASSWORD) ? "" : in.i.text ? in.i.text : "";
  const char *mb = m->member;
  U32 t = decode(text);
  AwraDBusW w;
  awra_dbus_w_init(&w);
  if (!strcmp(mb, "GetText")) {
    int s = awra_dbus_r_i32(&r), e = awra_dbus_r_i32(&r);
    if (e < 0 || e > t.n) {
      e = t.n;
    }
    s = s < 0 ? 0 : s > e ? e : s;
    awra_dbus_w_init(&w);
    put_range(&w, text, s, e);
    awra_dbus_reply(g_bus, m, "s", &w);
  }
  else if (!strcmp(mb, "GetCharacterAtOffset")) {
    const int i = awra_dbus_r_i32(&r);
    reply_int(m, i >= 0 && i < t.n ? (int32_t)t.u[i] : 0);
  }
  else if (!strcmp(mb, "GetStringAtOffset")) {
    const int off = awra_dbus_r_i32(&r);
    const unsigned gran = awra_dbus_r_u32(&r);
    int s = 0, e = 0;
    if (off >= 0 && off < t.n) {
      switch (gran) {
        case 0: /* char */
          s = off;
          e = off + 1;
          break;
        case 1: /* word: the word at (or after) the offset */
          s = off;
          while (s < t.n && !is_word(t.u[s])) {
            s++;
          }
          while (s > 0 && is_word(t.u[s - 1])) {
            s--;
          }
          e = s;
          while (e < t.n && is_word(t.u[e])) {
            e++;
          }
          break;
        case 2: /* sentence */
          boundary_at(&t, off, 3, &s, &e);
          while (e > s && is_space(t.u[e - 1])) {
            e--;
          }
          break;
        default: /* line, paragraph */
          boundary_at(&t, off, 5, &s, &e);
          if (e > s && t.u[e - 1] == '\n') {
            e--;
          }
          break;
      }
    }
    text_range_reply(m, text, s, e);
  }
  else if (!strcmp(mb, "GetTextAtOffset") || !strcmp(mb, "GetTextBeforeOffset") ||
           !strcmp(mb, "GetTextAfterOffset"))
  {
    const int off = awra_dbus_r_i32(&r);
    const unsigned type = awra_dbus_r_u32(&r);
    int s, e;
    boundary_at(&t, off, (int)type, &s, &e);
    if (!strcmp(mb, "GetTextBeforeOffset")) {
      if (s <= 0) {
        s = e = 0;
      }
      else {
        e = s;
        boundary_at(&t, s - 1, (int)type, &s, &e);
        if (e > off) {
          e = off;
        }
      }
    }
    else if (!strcmp(mb, "GetTextAfterOffset")) {
      if (e >= t.n) {
        s = e = t.n;
      }
      else {
        boundary_at(&t, e, (int)type, &s, &e);
      }
    }
    text_range_reply(m, text, s, e);
  }
  else if (!strcmp(mb, "SetCaretOffset")) {
    const int off = awra_dbus_r_i32(&r);
    if (obj_editable(o, &in)) {
      awra_editable_set_cursor(o->w, off);
      reply_bool(m, true);
    }
    else {
      reply_bool(m, false);
    }
  }
  else if (!strcmp(mb, "GetNSelections")) {
    reply_int(m, in.i.sel_start >= 0 && in.i.sel_end > in.i.sel_start ? 1 : 0);
  }
  else if (!strcmp(mb, "GetSelection")) {
    const int i = awra_dbus_r_i32(&r);
    const bool has = i == 0 && in.i.sel_start >= 0 && in.i.sel_end > in.i.sel_start;
    awra_dbus_w_i32(&w, has ? in.i.sel_start : 0);
    awra_dbus_w_i32(&w, has ? in.i.sel_end : 0);
    awra_dbus_reply(g_bus, m, "ii", &w);
  }
  else if (!strcmp(mb, "AddSelection") || !strcmp(mb, "SetSelection")) {
    if (!strcmp(mb, "SetSelection")) {
      awra_dbus_r_i32(&r);
    }
    const int s = awra_dbus_r_i32(&r), e = awra_dbus_r_i32(&r);
    if (obj_editable(o, &in) || in.i.role == AWRA_ACCESSIBLE_ROLE_TEXT_BOX) {
      awra_editable_select(o->w, s, e);
      reply_bool(m, true);
    }
    else {
      reply_bool(m, false);
    }
  }
  else if (!strcmp(mb, "RemoveSelection")) {
    awra_dbus_r_i32(&r);
    if (obj_editable(o, &in) && in.i.caret >= 0) {
      awra_editable_set_cursor(o->w, in.i.caret);
      reply_bool(m, true);
    }
    else {
      reply_bool(m, false);
    }
  }
  else if (!strcmp(mb, "GetCharacterExtents") || !strcmp(mb, "GetRangeExtents")) {
    const int s = awra_dbus_r_i32(&r);
    const int e = !strcmp(mb, "GetRangeExtents") ? awra_dbus_r_i32(&r) : s + 1;
    const unsigned coord = awra_dbus_r_u32(&r);
    double a[4] = {0, 0, 0, 0}, b[4] = {0, 0, 0, 0};
    if (o->item < 0 && awra_a11y_char_rect(o->w, s, a)) {
      if (e - 1 > s && awra_a11y_char_rect(o->w, e - 1, b)) {
        const double x2 = fmax(a[0] + a[2], b[0] + b[2]), y2 = fmax(a[1] + a[3], b[1] + b[3]);
        a[0] = fmin(a[0], b[0]);
        a[1] = fmin(a[1], b[1]);
        a[2] = x2 - a[0];
        a[3] = y2 - a[1];
      }
      to_coords(o, a, coord);
    }
    for (int i = 0; i < 4; i++) {
      awra_dbus_w_i32(&w, (int32_t)floor(a[i] + 0.5));
    }
    awra_dbus_reply(g_bus, m, "iiii", &w);
  }
  else if (!strcmp(mb, "GetOffsetAtPoint")) {
    double x = awra_dbus_r_i32(&r), y = awra_dbus_r_i32(&r);
    from_coords(o, &x, &y, awra_dbus_r_u32(&r));
    reply_int(m, o->item < 0 ? awra_a11y_offset_at(o->w, x, y) : -1);
  }
  else if (!strcmp(mb, "GetAttributes") || !strcmp(mb, "GetAttributeRun")) {
    const AwraDBusArray a = awra_dbus_w_array_open(&w, 8);
    awra_dbus_w_array_close(&w, a);
    awra_dbus_w_i32(&w, 0);
    awra_dbus_w_i32(&w, t.n);
    awra_dbus_reply(g_bus, m, "a{ss}ii", &w);
  }
  else if (!strcmp(mb, "GetDefaultAttributes") || !strcmp(mb, "GetDefaultAttributeSet")) {
    const AwraDBusArray a = awra_dbus_w_array_open(&w, 8);
    awra_dbus_w_array_close(&w, a);
    awra_dbus_reply(g_bus, m, "a{ss}", &w);
  }
  else if (!strcmp(mb, "GetAttributeValue")) {
    reply_str(m, "");
  }
  else if (!strcmp(mb, "GetBoundedRanges")) {
    const AwraDBusArray a = awra_dbus_w_array_open(&w, 8);
    awra_dbus_w_array_close(&w, a);
    awra_dbus_reply(g_bus, m, "a(iisv)", &w);
  }
  else if (!strcmp(mb, "ScrollSubstringTo") || !strcmp(mb, "ScrollSubstringToPoint")) {
    reply_bool(m, false);
  }
  else {
    reply_unknown(m);
  }
  awra_dbus_w_free(&w);
  free(t.u);
}

static void editable_method(const AwraDBusMsg *m, const Obj *o)
{
  AwraDBusR r;
  awra_dbus_r_init(&r, m);
  Info in;
  obj_info(o, &in);
  if (!obj_editable(o, &in)) {
    reply_bool(m, false);
    return;
  }
  const char *mb = m->member;
  if (!strcmp(mb, "SetTextContents")) {
    const char *s = awra_dbus_r_str(&r);
    const int n = awra_editable_length(o->w);
    awra_editable_delete(o->w, 0, n);
    awra_editable_insert(o->w, 0, s);
    reply_bool(m, !r.err);
  }
  else if (!strcmp(mb, "InsertText")) {
    const int pos = awra_dbus_r_i32(&r);
    const char *s = awra_dbus_r_str(&r);
    const int len = awra_dbus_r_i32(&r);
    if (r.err) {
      reply_bool(m, false);
      return;
    }
    const int bytes = awra_utf8_offset(s, len < 0 ? awra_utf8_chars(s, -1) : len);
    char *part = (char *)malloc((size_t)bytes + 1);
    if (part) {
      memcpy(part, s, (size_t)bytes);
      part[bytes] = 0;
      awra_editable_insert(o->w, pos, part);
      free(part);
    }
    reply_bool(m, part != NULL);
  }
  else if (!strcmp(mb, "DeleteText") || !strcmp(mb, "CutText") || !strcmp(mb, "CopyText")) {
    const int s = awra_dbus_r_i32(&r), e = awra_dbus_r_i32(&r);
    if (strcmp(mb, "DeleteText") != 0 && in.i.text && !(in.flat & AWRA_A11Y_PASSWORD)) {
      const int bs = awra_utf8_offset(in.i.text, s), be = awra_utf8_offset(in.i.text, e);
      char *part = (char *)malloc((size_t)(be > bs ? be - bs : 0) + 1);
      if (part) {
        memcpy(part, in.i.text + bs, (size_t)(be > bs ? be - bs : 0));
        part[be > bs ? be - bs : 0] = 0;
        awra_clipboard_set_text(o->w, part);
        free(part);
      }
    }
    if (strcmp(mb, "CopyText") != 0) {
      awra_editable_delete(o->w, s, e);
      reply_bool(m, true);
    }
    else {
      reply_empty(m);
    }
  }
  else if (!strcmp(mb, "PasteText")) {
    const int pos = awra_dbus_r_i32(&r);
    char *clip = awra_clipboard_get_text(o->w);
    if (clip) {
      awra_editable_insert(o->w, pos, clip);
      awra_free(clip);
    }
    reply_bool(m, clip != NULL);
  }
  else {
    reply_unknown(m);
  }
}

/* ----------------------------------------------------------------------- */
/* Value */

static bool value_prop(AwraDBusW *w, const Obj *o, const char *prop, bool variant)
{
  Info in;
  obj_info(o, &in);
  double v;
  if (!strcmp(prop, "MinimumValue")) {
    v = in.i.min;
  }
  else if (!strcmp(prop, "MaximumValue")) {
    v = in.i.max;
  }
  else if (!strcmp(prop, "MinimumIncrement")) {
    v = in.i.step;
  }
  else if (!strcmp(prop, "CurrentValue")) {
    v = in.i.value;
  }
  else if (!strcmp(prop, "Text")) {
    if (variant) {
      awra_dbus_w_variant(w, "s");
    }
    char num[32];
    if (!in.i.value_text) {
      snprintf(num, sizeof(num), "%g", in.i.value);
    }
    awra_dbus_w_str(w, in.i.value_text ? in.i.value_text : num);
    return true;
  }
  else {
    return false;
  }
  if (variant) {
    awra_dbus_w_variant(w, "d");
  }
  awra_dbus_w_double(w, v);
  return true;
}

/* ----------------------------------------------------------------------- */
/* Selection: over the children (widgets, then items) */

typedef struct Sel {
  AwraWidget **kids;
  int n, nitems;
} Sel;

static bool child_selected(const Sel *s, const Obj *o, int i)
{
  AwraA11yInfo info;
  if (i < s->n) {
    awra_a11y_info(s->kids[i], &info);
  }
  else {
    awra_a11y_item_info(o->w, i - s->n, &info, NULL);
  }
  return (info.defined & (1u << AWRA_ACCESSIBLE_STATE_SELECTED)) ?
             info.state[AWRA_ACCESSIBLE_STATE_SELECTED] != 0 :
         (info.defined & (1u << AWRA_ACCESSIBLE_STATE_CHECKED)) ?
             info.state[AWRA_ACCESSIBLE_STATE_CHECKED] == AWRA_ACCESSIBLE_TRISTATE_TRUE :
             false;
}

static bool select_child(const Sel *s, const Obj *o, int i, bool on)
{
  if (i < 0 || i >= s->n + s->nitems) {
    return false;
  }
  if (i < s->n) {
    if (awra_a11y_do(s->kids[i], -1, on ? AWRA_A11Y_DO_SELECT : AWRA_A11Y_DO_DESELECT, 0)) {
      return true;
    }
    return on && awra_a11y_do(s->kids[i], -1, AWRA_A11Y_DO_ACTIVATE, 0);
  }
  return awra_a11y_do(o->w, i - s->n, on ? AWRA_A11Y_DO_SELECT : AWRA_A11Y_DO_DESELECT, 0);
}

static void selection_method(const AwraDBusMsg *m, const Obj *o, bool prop_only,
                             AwraDBusW *pw, bool variant)
{
  Sel s;
  s.n = obj_children(o, &s.kids, &s.nitems);
  const int total = s.n + s.nitems;
  if (prop_only) {
    int count = 0;
    for (int i = 0; i < total; i++) {
      count += child_selected(&s, o, i);
    }
    if (variant) {
      awra_dbus_w_variant(pw, "i");
    }
    awra_dbus_w_i32(pw, count);
    free(s.kids);
    return;
  }
  AwraDBusR r;
  awra_dbus_r_init(&r, m);
  const char *mb = m->member;
  if (!strcmp(mb, "GetSelectedChild")) {
    int k = awra_dbus_r_i32(&r), found = -1;
    for (int i = 0; i < total && found < 0; i++) {
      if (child_selected(&s, o, i) && k-- == 0) {
        found = i;
      }
    }
    if (found < 0) {
      reply_ref(m, NULL, -1, true);
    }
    else if (found < s.n) {
      reply_ref(m, s.kids[found], -1, false);
    }
    else {
      reply_ref(m, o->w, found - s.n, false);
    }
  }
  else if (!strcmp(mb, "SelectChild")) {
    reply_bool(m, select_child(&s, o, awra_dbus_r_i32(&r), true));
  }
  else if (!strcmp(mb, "DeselectChild")) {
    reply_bool(m, select_child(&s, o, awra_dbus_r_i32(&r), false));
  }
  else if (!strcmp(mb, "DeselectSelectedChild")) {
    int k = awra_dbus_r_i32(&r), found = -1;
    for (int i = 0; i < total && found < 0; i++) {
      if (child_selected(&s, o, i) && k-- == 0) {
        found = i;
      }
    }
    reply_bool(m, found >= 0 && select_child(&s, o, found, false));
  }
  else if (!strcmp(mb, "IsChildSelected")) {
    const int i = awra_dbus_r_i32(&r);
    reply_bool(m, i >= 0 && i < total && child_selected(&s, o, i));
  }
  else if (!strcmp(mb, "SelectAll") || !strcmp(mb, "ClearSelection")) {
    reply_bool(m, false);
  }
  else {
    reply_unknown(m);
  }
  free(s.kids);
}

/* ----------------------------------------------------------------------- */
/* Properties */

static bool get_prop(AwraDBusW *w, const Obj *o, const char *iface, const char *prop,
                     bool variant)
{
  if (!strcmp(iface, I_ACCESSIBLE)) {
    return accessible_prop(w, o, prop, variant);
  }
  if (!strcmp(iface, I_APPLICATION) && !o->w) {
    return application_prop(w, prop, variant);
  }
  if (!o->w) {
    return false;
  }
  Info in;
  obj_info(o, &in);
  if (!strcmp(iface, I_ACTION) && !strcmp(prop, "NActions")) {
    if (variant) {
      awra_dbus_w_variant(w, "i");
    }
    awra_dbus_w_i32(w, in.i.action ? 1 : 0);
    return true;
  }
  if (!strcmp(iface, I_TEXT)) {
    if (!strcmp(prop, "CharacterCount")) {
      if (variant) {
        awra_dbus_w_variant(w, "i");
      }
      awra_dbus_w_i32(w, (in.flat & AWRA_A11Y_PASSWORD) ? 0 : awra_utf8_chars(in.i.text, -1));
      return true;
    }
    if (!strcmp(prop, "CaretOffset")) {
      if (variant) {
        awra_dbus_w_variant(w, "i");
      }
      awra_dbus_w_i32(w, in.i.caret);
      return true;
    }
    return false;
  }
  if (!strcmp(iface, I_VALUE)) {
    return value_prop(w, o, prop, variant);
  }
  if (!strcmp(iface, I_SELECTION) && !strcmp(prop, "NSelectedChildren")) {
    selection_method(NULL, o, true, w, variant);
    return true;
  }
  return false;
}

static const char *const g_props[][8] = {
    {I_ACCESSIBLE, "Name", "Description", "Parent", "ChildCount", "Locale", "AccessibleId", "HelpText"},
    {I_APPLICATION, "ToolkitName", "Version", "AtspiVersion", "Id", NULL},
    {I_ACTION, "NActions", NULL},
    {I_TEXT, "CharacterCount", "CaretOffset", NULL},
    {I_VALUE, "MinimumValue", "MaximumValue", "MinimumIncrement", "CurrentValue", "Text", NULL},
    {I_SELECTION, "NSelectedChildren", NULL},
};

static void properties_method(const AwraDBusMsg *m, const Obj *o)
{
  AwraDBusR r;
  awra_dbus_r_init(&r, m);
  const char *iface = awra_dbus_r_str(&r);
  AwraDBusW w;
  awra_dbus_w_init(&w);
  if (!strcmp(m->member, "Get")) {
    const char *prop = awra_dbus_r_str(&r);
    if (!r.err && get_prop(&w, o, iface, prop, true)) {
      awra_dbus_reply(g_bus, m, "v", &w);
    }
    else {
      awra_dbus_error(g_bus, m, "org.freedesktop.DBus.Error.UnknownProperty", prop);
    }
  }
  else if (!strcmp(m->member, "GetAll")) {
    const AwraDBusArray a = awra_dbus_w_array_open(&w, 8);
    for (size_t i = 0; i < sizeof(g_props) / sizeof(g_props[0]); i++) {
      if (strcmp(g_props[i][0], iface) != 0) {
        continue;
      }
      for (int k = 1; k < 8 && g_props[i][k]; k++) {
        /* the value aligns as in the whole message: written in place */
        const size_t mark = w.n;
        awra_dbus_w_struct(&w);
        awra_dbus_w_str(&w, g_props[i][k]);
        if (!get_prop(&w, o, iface, g_props[i][k], true)) {
          w.n = mark; /* not for this object */
        }
      }
    }
    awra_dbus_w_array_close(&w, a);
    awra_dbus_reply(g_bus, m, "a{sv}", &w);
  }
  else if (!strcmp(m->member, "Set")) {
    const char *prop = awra_dbus_r_str(&r);
    const char *sig = awra_dbus_r_sig(&r);
    if (!o->w && !strcmp(iface, I_APPLICATION) && !strcmp(prop, "Id") && !strcmp(sig, "i")) {
      g_app_id = awra_dbus_r_i32(&r);
      reply_empty(m);
    }
    else if (o->w && !strcmp(iface, I_VALUE) && !strcmp(prop, "CurrentValue") &&
             !strcmp(sig, "d"))
    {
      const double v = awra_dbus_r_double(&r);
      reply_empty(m);
      awra_a11y_do(o->w, o->item, AWRA_A11Y_DO_SET_VALUE, v);
    }
    else {
      awra_dbus_error(g_bus, m, "org.freedesktop.DBus.Error.PropertyReadOnly", prop);
    }
  }
  else {
    reply_unknown(m);
  }
  awra_dbus_w_free(&w);
}

/* ----------------------------------------------------------------------- */
/* Dispatch */

static void introspect(const AwraDBusMsg *m)
{
  reply_str(m, "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" "
               "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
               "<node><interface name=\"org.a11y.atspi.Accessible\"/></node>\n");
}

static void handle_call(const AwraDBusMsg *m)
{
  const char *iface = m->iface ? m->iface : "";
  if (!strcmp(iface, "org.freedesktop.DBus.Peer")) {
    if (!strcmp(m->member, "Ping")) {
      reply_empty(m);
    }
    else {
      reply_str(m, "0");
    }
    return;
  }
  if (!strcmp(iface, "org.freedesktop.DBus.Introspectable")) {
    introspect(m);
    return;
  }
  if (m->path && !strcmp(m->path, "/org/a11y/atspi/cache")) {
    AwraDBusW w;
    awra_dbus_w_init(&w);
    const AwraDBusArray a = awra_dbus_w_array_open(&w, 8);
    awra_dbus_w_array_close(&w, a);
    awra_dbus_reply(g_bus, m, "a((so)(so)(so)iiassusau)", &w);
    awra_dbus_w_free(&w);
    return;
  }
  Obj o;
  if (!parse_path(m->path, &o)) {
    awra_dbus_error(g_bus, m, "org.freedesktop.DBus.Error.UnknownObject", m->path);
    return;
  }
  if (o.item >= 0 && o.item >= awra_a11y_n_items(o.w)) {
    awra_dbus_error(g_bus, m, "org.freedesktop.DBus.Error.UnknownObject", m->path);
    return;
  }
  if (o.w) {
    awra_widget_ref(o.w);
  }
  if (!strcmp(iface, I_PROPS)) {
    properties_method(m, &o);
  }
  else if (!strcmp(iface, I_ACCESSIBLE)) {
    accessible_method(m, &o);
  }
  else if (!o.w && !strcmp(iface, I_APPLICATION)) {
    application_method(m);
  }
  else if (o.w && !strcmp(iface, I_COMPONENT)) {
    component_method(m, &o);
  }
  else if (o.w && !strcmp(iface, I_ACTION)) {
    action_method(m, &o);
  }
  else if (o.w && !strcmp(iface, I_TEXT)) {
    text_method(m, &o);
  }
  else if (o.w && !strcmp(iface, I_EDITABLE)) {
    editable_method(m, &o);
  }
  else if (o.w && !strcmp(iface, I_SELECTION)) {
    selection_method(m, &o, false, NULL, false);
  }
  else if (o.w && !strcmp(iface, I_VALUE)) {
    reply_unknown(m); /* properties only */
  }
  else {
    reply_unknown(m);
  }
  if (o.w) {
    awra_widget_unref(o.w);
  }
}

/* ======================================================================= */
/* Events */

static bool events_on(void)
{
  return g_bus && g_registered && g_listeners > 0;
}

/* (siiva{sv}): the detail, two numbers, a variant the caller writes */
static void ev_begin(AwraDBusW *w, const char *detail, int a, int b)
{
  awra_dbus_w_init(w);
  awra_dbus_w_str(w, detail);
  awra_dbus_w_i32(w, a);
  awra_dbus_w_i32(w, b);
}

static void ev_send(AwraDBusW *w, AwraWidget *wd, int item, const char *iface, const char *member)
{
  const AwraDBusArray a = awra_dbus_w_array_open(w, 8);
  awra_dbus_w_array_close(w, a);
  char path[96];
  Obj o = {wd, item};
  obj_path(&o, path, sizeof(path));
  awra_dbus_signal(g_bus, path, iface, member, "siiva{sv}", w);
  awra_dbus_w_free(w);
}

static void ev_state(AwraWidget *wd, int item, unsigned flag, bool on)
{
  if (!events_on()) {
    return;
  }
  for (size_t i = 0; i < sizeof(g_states) / sizeof(g_states[0]); i++) {
    if (g_states[i].flag == flag) {
      AwraDBusW w;
      ev_begin(&w, g_states[i].name, on, 0);
      awra_dbus_w_variant(&w, "s");
      awra_dbus_w_str(&w, "0");
      ev_send(&w, wd, item, I_EV_OBJECT, "StateChanged");
    }
  }
  if (flag == AWRA_A11Y_EXPANDED) {
    AwraDBusW w;
    ev_begin(&w, "collapsed", !on, 0);
    awra_dbus_w_variant(&w, "s");
    awra_dbus_w_str(&w, "0");
    ev_send(&w, wd, item, I_EV_OBJECT, "StateChanged");
  }
}

static void ev_property(AwraWidget *wd, const char *prop)
{
  if (!events_on()) {
    return;
  }
  AwraDBusW w;
  ev_begin(&w, prop, 0, 0);
  if (!strcmp(prop, "accessible-value")) {
    AwraA11yInfo info;
    awra_a11y_info(wd, &info);
    awra_dbus_w_variant(&w, "d");
    awra_dbus_w_double(&w, info.value);
  }
  else if (!strcmp(prop, "accessible-role")) {
    AwraA11yInfo info;
    awra_a11y_info(wd, &info);
    awra_dbus_w_variant(&w, "u");
    awra_dbus_w_u32(&w, atspi_role(info.role, &info, wd));
  }
  else {
    const AwraA11ySnap *s = awra_a11y_snap(wd);
    awra_dbus_w_variant(&w, "s");
    awra_dbus_w_str(&w, !s ? "" : !strcmp(prop, "accessible-name") ? s->name : s->description);
  }
  ev_send(&w, wd, -1, I_EV_OBJECT, "PropertyChange");
}

static void ev_name(AwraWidget *wd)
{
  ev_property(wd, "accessible-name");
}

static void ev_description(AwraWidget *wd)
{
  ev_property(wd, "accessible-description");
}

static void ev_value(AwraWidget *wd)
{
  ev_property(wd, "accessible-value");
}

static void ev_role(AwraWidget *wd)
{
  ev_property(wd, "accessible-role");
}

static void ev_text(AwraWidget *wd, bool insert, int offset, int length, const char *text, int bytes)
{
  if (!events_on()) {
    return;
  }
  AwraDBusW w;
  ev_begin(&w, insert ? "insert" : "delete", offset, length);
  awra_dbus_w_variant(&w, "s");
  awra_dbus_w_str_len(&w, text, (size_t)bytes);
  ev_send(&w, wd, -1, I_EV_OBJECT, "TextChanged");
}

static void ev_caret(AwraWidget *wd, int offset)
{
  if (!events_on()) {
    return;
  }
  AwraDBusW w;
  ev_begin(&w, "", offset, 0);
  awra_dbus_w_variant(&w, "i");
  awra_dbus_w_i32(&w, 0);
  ev_send(&w, wd, -1, I_EV_OBJECT, "TextCaretMoved");
}

static void ev_simple(AwraWidget *wd, const char *member)
{
  if (!events_on()) {
    return;
  }
  AwraDBusW w;
  ev_begin(&w, "", 0, 0);
  awra_dbus_w_variant(&w, "s");
  awra_dbus_w_str(&w, "");
  ev_send(&w, wd, -1, I_EV_OBJECT, member);
}

static void ev_text_selection(AwraWidget *wd)
{
  ev_simple(wd, "TextSelectionChanged");
}

static void ev_selection(AwraWidget *wd)
{
  if (awra_a11y_snap(wd)) {
    ev_simple(wd, "SelectionChanged");
  }
}

static void ev_children_of(AwraWidget *parent, AwraWidget *child, int item, int index, bool added)
{
  if (!events_on()) {
    return;
  }
  AwraDBusW w;
  ev_begin(&w, added ? "add" : "remove", index, 0);
  awra_dbus_w_variant(&w, "(so)");
  w_ref(&w, child, item);
  ev_send(&w, parent, -1, I_EV_OBJECT, "ChildrenChanged");
}

static void ev_children(AwraWidget *parent, AwraWidget *child, int index, bool added)
{
  ev_children_of(parent, child, -1, index, added);
}

static void ev_items(AwraWidget *wd, int old_count, int new_count)
{
  if (!events_on()) {
    return;
  }
  const int base = awra_a11y_children(wd, NULL, 0);
  /* one event for the first item that came or went (lists change a lot) */
  if (new_count > old_count) {
    ev_children_of(wd, wd, old_count, base + old_count, true);
  }
  else if (old_count > new_count) {
    AwraDBusW w;
    ev_begin(&w, "remove", base + new_count, 0);
    awra_dbus_w_variant(&w, "(so)");
    w_null_ref(&w);
    ev_send(&w, wd, -1, I_EV_OBJECT, "ChildrenChanged");
  }
}

static void ev_window(AwraWidget *win, int what)
{
  if (!events_on()) {
    return;
  }
  static const char *const members[] = {"Create", "Destroy", "Activate", "Deactivate"};
  if (what == AWRA_A11Y_WINDOW_CREATE || what == AWRA_A11Y_WINDOW_DESTROY) {
    /* the root's children */
    AwraDBusW w;
    ev_begin(&w, what == AWRA_A11Y_WINDOW_CREATE ? "add" : "remove",
             awra_a11y_index_in_parent(win), 0);
    awra_dbus_w_variant(&w, "(so)");
    w_ref(&w, win, -1);
    ev_send(&w, NULL, -1, I_EV_OBJECT, "ChildrenChanged");
  }
  AwraDBusW w;
  ev_begin(&w, "", 0, 0);
  awra_dbus_w_variant(&w, "s");
  awra_dbus_w_str(&w, awra_window_title(win));
  ev_send(&w, win, -1, I_EV_WINDOW, members[what]);
}

static void ev_announce(AwraWidget *wd, const char *message, int priority)
{
  if (!events_on()) {
    return;
  }
  AwraWidget *src = wd;
  while (src && !awra_a11y_presented(src)) {
    src = src->parent;
  }
  AwraDBusW w;
  ev_begin(&w, "", priority >= AWRA_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_HIGH ? 2 : 1, 0);
  awra_dbus_w_variant(&w, "s");
  awra_dbus_w_str(&w, message);
  ev_send(&w, src, -1, I_EV_OBJECT, "Announcement");
}

static void ev_freed(AwraWidget *wd, AwraA11ySnap *snap)
{
  if (!events_on()) {
    return;
  }
  /* the object is gone: the defunct state, from its path */
  char path[96];
  snprintf(path, sizeof(path), OBJ_PREFIX "%u", snap->id);
  AwraDBusW w;
  ev_begin(&w, "defunct", 1, 0);
  awra_dbus_w_variant(&w, "s");
  awra_dbus_w_str(&w, "0");
  const AwraDBusArray a = awra_dbus_w_array_open(&w, 8);
  awra_dbus_w_array_close(&w, a);
  awra_dbus_signal(g_bus, path, I_EV_OBJECT, "StateChanged", "siiva{sv}", &w);
  awra_dbus_w_free(&w);
}

static const AwraA11yBackend g_backend = {
    .state = ev_state,
    .name = ev_name,
    .description = ev_description,
    .value = ev_value,
    .role = ev_role,
    .text = ev_text,
    .caret = ev_caret,
    .text_selection = ev_text_selection,
    .selection = ev_selection,
    .children = ev_children,
    .items = ev_items,
    .window = ev_window,
    .announce = ev_announce,
    .freed = ev_freed,
};

/* ======================================================================= */
/* The accessibility bus */

static void bus_off(void)
{
  awra_a11y_set_backend(NULL);
  g_registered = false;
  g_listeners = 0;
  free(g_desktop_name);
  free(g_desktop_path);
  g_desktop_name = g_desktop_path = NULL;
}

static void events_reply(AwraDBus *bus, const AwraDBusMsg *m, void *data)
{
  if (!m || m->type != AWRA_DBUS_RETURN || bus != g_bus) {
    return;
  }
  AwraDBusR r;
  awra_dbus_r_init(&r, m);
  size_t end;
  int n = 0;
  if (awra_dbus_r_array(&r, 8, &end)) {
    while (r.pos < end && !r.err) {
      awra_dbus_r_struct(&r);
      awra_dbus_r_str(&r);
      awra_dbus_r_str(&r);
      n++;
    }
  }
  g_listeners = n;
}

static void embed_reply(AwraDBus *bus, const AwraDBusMsg *m, void *data)
{
  if (!m || bus != g_bus) {
    return;
  }
  if (m->type == AWRA_DBUS_RETURN) {
    AwraDBusR r;
    awra_dbus_r_init(&r, m);
    awra_dbus_r_struct(&r);
    const char *name = awra_dbus_r_str(&r);
    const char *path = awra_dbus_r_str(&r);
    if (!r.err) {
      awra_set_str(&g_desktop_name, name);
      awra_set_str(&g_desktop_path, path);
    }
  }
  g_registered = true;
  awra_dbus_add_match(g_bus, "type='signal',interface='org.a11y.atspi.Registry',"
                             "member='EventListenerRegistered'");
  awra_dbus_add_match(g_bus, "type='signal',interface='org.a11y.atspi.Registry',"
                             "member='EventListenerDeregistered'");
  awra_dbus_call(g_bus, REGISTRY, REGISTRY_PATH, REGISTRY, "GetRegisteredEvents", NULL, NULL,
                 events_reply, NULL);
}

static void bus_ready(AwraDBus *bus, void *data)
{
  awra_a11y_set_backend(&g_backend);
  AwraDBusW w;
  awra_dbus_w_init(&w);
  awra_dbus_w_struct(&w);
  awra_dbus_w_str(&w, awra_dbus_unique_name(bus));
  awra_dbus_w_str(&w, ROOT_PATH);
  awra_dbus_call(bus, REGISTRY, ROOT_PATH, "org.a11y.atspi.Socket", "Embed", "(so)", &w,
                 embed_reply, NULL);
  awra_dbus_w_free(&w);
}

static void bus_closed(AwraDBus *bus, void *data)
{
  if (bus == g_bus) {
    g_bus = NULL;
    bus_off();
  }
}

static void bus_message(AwraDBus *bus, const AwraDBusMsg *m, void *data)
{
  if (m->type == AWRA_DBUS_CALL) {
    handle_call(m);
    return;
  }
  if (m->type == AWRA_DBUS_SIGNAL && m->iface && !strcmp(m->iface, REGISTRY) && m->member) {
    if (!strcmp(m->member, "EventListenerRegistered")) {
      g_listeners++;
    }
    else if (!strcmp(m->member, "EventListenerDeregistered") && g_listeners > 0) {
      g_listeners--;
    }
  }
}

static void bus_connect(const char *address)
{
  if (g_bus || !address || !address[0]) {
    return;
  }
  const AwraDBusCallbacks cb = {bus_ready, bus_closed, bus_message, NULL};
  g_bus = awra_dbus_connect(address, &cb);
}

static void bus_disconnect(void)
{
  if (g_bus) {
    AwraDBus *b = g_bus;
    g_bus = NULL;
    bus_off();
    awra_dbus_close(b);
  }
}

/* ======================================================================= */
/* The session bus: is an assistive technology running? */

static void address_reply(AwraDBus *bus, const AwraDBusMsg *m, void *data)
{
  if (!m || m->type != AWRA_DBUS_RETURN || !g_enabled) {
    return;
  }
  AwraDBusR r;
  awra_dbus_r_init(&r, m);
  const char *address = awra_dbus_r_str(&r);
  if (!r.err) {
    bus_connect(address);
  }
}

static void evaluate(bool enabled)
{
  g_enabled = enabled || g_forced;
  if (g_enabled && !g_bus && g_session) {
    awra_dbus_call(g_session, "org.a11y.Bus", "/org/a11y/bus", "org.a11y.Bus", "GetAddress", NULL,
                   NULL, address_reply, NULL);
  }
  else if (!g_enabled && g_bus) {
    bus_disconnect();
  }
}

/* a{sv} of org.a11y.Status: IsEnabled / ScreenReaderEnabled */
static void read_status(AwraDBusR *r, bool *is_enabled, bool *sr_enabled, bool *seen)
{
  size_t end;
  if (!awra_dbus_r_array(r, 8, &end)) {
    return;
  }
  while (r->pos < end && !r->err) {
    awra_dbus_r_struct(r);
    const char *key = awra_dbus_r_str(r);
    const char *sig = awra_dbus_r_sig(r);
    if (!strcmp(sig, "b") && (!strcmp(key, "IsEnabled") || !strcmp(key, "ScreenReaderEnabled"))) {
      const bool v = awra_dbus_r_bool(r);
      *(!strcmp(key, "IsEnabled") ? is_enabled : sr_enabled) = v;
      *seen = true;
    }
    else {
      const char *s = sig;
      awra_dbus_r_skip(r, &s);
    }
  }
}

static bool g_is_enabled, g_sr_enabled;

static void status_reply(AwraDBus *bus, const AwraDBusMsg *m, void *data)
{
  if (!m || m->type != AWRA_DBUS_RETURN) {
    /* no accessibility bus (org.a11y.Bus not running): wait for it */
    evaluate(false);
    return;
  }
  AwraDBusR r;
  awra_dbus_r_init(&r, m);
  bool seen = false;
  read_status(&r, &g_is_enabled, &g_sr_enabled, &seen);
  evaluate(g_is_enabled || g_sr_enabled);
}

static void query_status(void)
{
  AwraDBusW w;
  awra_dbus_w_init(&w);
  awra_dbus_w_str(&w, "org.a11y.Status");
  /* NO_AUTO_START: asking must not launch the accessibility bus */
  awra_dbus_call_flags(g_session, g_forced ? 0 : 2, "org.a11y.Bus", "/org/a11y/bus", I_PROPS,
                       "GetAll", "s", &w, status_reply, NULL);
  awra_dbus_w_free(&w);
}

static void session_ready(AwraDBus *bus, void *data)
{
  awra_dbus_add_match(bus, "type='signal',sender='org.freedesktop.DBus',"
                           "interface='org.freedesktop.DBus',member='NameOwnerChanged',"
                           "arg0='org.a11y.Bus'");
  awra_dbus_add_match(bus, "type='signal',interface='org.freedesktop.DBus.Properties',"
                           "member='PropertiesChanged',path='/org/a11y/bus',"
                           "arg0='org.a11y.Status'");
  query_status();
}

static void session_closed(AwraDBus *bus, void *data)
{
  if (bus == g_session) {
    g_session = NULL;
  }
}

static void session_message(AwraDBus *bus, const AwraDBusMsg *m, void *data)
{
  if (m->type == AWRA_DBUS_CALL) {
    awra_dbus_error(bus, m, "org.freedesktop.DBus.Error.UnknownMethod", m->member);
    return;
  }
  if (m->type != AWRA_DBUS_SIGNAL || !m->member) {
    return;
  }
  AwraDBusR r;
  awra_dbus_r_init(&r, m);
  if (!strcmp(m->member, "NameOwnerChanged")) {
    const char *name = awra_dbus_r_str(&r);
    awra_dbus_r_str(&r);
    const char *owner = awra_dbus_r_str(&r);
    if (!r.err && !strcmp(name, "org.a11y.Bus")) {
      if (owner[0]) {
        query_status();
      }
      else {
        g_is_enabled = g_sr_enabled = false;
        evaluate(false);
      }
    }
  }
  else if (!strcmp(m->member, "PropertiesChanged") && m->path && !strcmp(m->path, "/org/a11y/bus")) {
    const char *iface = awra_dbus_r_str(&r);
    if (!r.err && !strcmp(iface, "org.a11y.Status")) {
      bool seen = false;
      read_status(&r, &g_is_enabled, &g_sr_enabled, &seen);
      if (seen) {
        evaluate(g_is_enabled || g_sr_enabled);
      }
    }
  }
}

void awra_a11y_platform_start(void)
{
  if (g_started) {
    return;
  }
  g_started = true;
  const char *v = getenv("AWRA_A11Y");
  const char *gtk = getenv("GTK_A11Y");
  const char *no = getenv("NO_AT_BRIDGE");
  if ((v && !strcmp(v, "0")) || (gtk && !strcmp(gtk, "none")) || (no && !strcmp(no, "1"))) {
    return;
  }
  g_forced = v && (!strcmp(v, "1") || !strcmp(v, "atspi"));
  const char *direct = getenv("AT_SPI_BUS_ADDRESS");
  if (direct && direct[0] && g_forced) {
    g_enabled = true;
    bus_connect(direct);
    return;
  }
  const AwraDBusCallbacks cb = {session_ready, session_closed, session_message, NULL};
  g_session = awra_dbus_connect(awra_dbus_session_address(), &cb);
}

void awra_a11y_platform_stop(void)
{
  bus_disconnect();
  if (g_session) {
    AwraDBus *s = g_session;
    g_session = NULL;
    awra_dbus_close(s);
  }
  g_started = false;
}

#endif
