/* Awra 2 core: the loop, widget lifetime, signals, layout, input dispatch
 * and state animations, on BUI's kit.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"
#include "a11y.h"
#include "kit_loop.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
typedef CRITICAL_SECTION AwraMutex;
static void mutex_init(AwraMutex *m)
{
  InitializeCriticalSection(m);
}
static void mutex_lock(AwraMutex *m)
{
  EnterCriticalSection(m);
}
static void mutex_unlock(AwraMutex *m)
{
  LeaveCriticalSection(m);
}
#else
#  include <pthread.h>
typedef pthread_mutex_t AwraMutex;
static void mutex_init(AwraMutex *m)
{
  pthread_mutex_init(m, NULL);
}
static void mutex_lock(AwraMutex *m)
{
  pthread_mutex_lock(m);
}
static void mutex_unlock(AwraMutex *m)
{
  pthread_mutex_unlock(m);
}
#endif

/* ======================================================================= */
/* Small utilities */

char *awra_strdup(const char *s)
{
  if (!s) {
    return NULL;
  }
  const size_t n = strlen(s) + 1;
  char *d = (char *)malloc(n);
  if (d) {
    memcpy(d, s, n);
  }
  return d;
}

void awra_set_str(char **dst, const char *s)
{
  if (*dst == s) {
    return;
  }
  char *c = awra_strdup(s);
  free(*dst);
  *dst = c;
}

double awra_now_ms(void)
{
  return bk_time() * 1000.0;
}

/* ======================================================================= */
/* The loop */

static bool g_inited;
static char *g_app_id;
static AwraMutex g_invoke_lock;

typedef struct Invoke {
  void (*fn)(void *data);
  void *data;
  struct Invoke *next;
} Invoke;
static Invoke *g_invoke_head, *g_invoke_tail;

static void run_invokes(void)
{
  mutex_lock(&g_invoke_lock);
  Invoke *list = g_invoke_head;
  g_invoke_head = g_invoke_tail = NULL;
  mutex_unlock(&g_invoke_lock);
  while (list) {
    Invoke *next = list->next;
    list->fn(list->data);
    free(list);
    list = next;
  }
}

static void wake_cb(void)
{
  run_invokes();
}

static void ref_retain(void *ref);
static void ref_release(void *ref);

static void init_default_direction(void);

void awra_init(const char *app_id)
{
  if (app_id && (!g_app_id || strcmp(app_id, g_app_id) != 0)) {
    awra_set_str(&g_app_id, app_id);
  }
  if (g_inited) {
    return;
  }
  g_inited = true;
  mutex_init(&g_invoke_lock);
  BK_set_ref_hooks(ref_retain, ref_release);
  BK_text_init_images(); /* colour emoji */
  /* reduced motion turns the kit's own animations off too */
  awra_style_init();
  init_default_direction();
  BK_window_supported(); /* opens the display: the backend is known */
  BK_window_wake_init(wake_cb);
  awra_tooltips_install();
}

const char *awra_app_id(void)
{
  return g_app_id ? g_app_id : "org.awra.Application";
}

void awra_run(void)
{
  run_invokes();
  BK_window_run();
}

void awra_quit(void)
{
  BK_window_quit();
}

void awra_set_keep_alive(bool keep_alive)
{
  BK_window_set_keep_alive(keep_alive);
}

int awra_pump(void)
{
  run_invokes();
  const int open = BK_window_pump();
  /* without a display (offscreen windows, tests) the host runs no timers */
  bk_loop_run_due();
  run_invokes();
  return open;
}

void awra_invoke(void (*fn)(void *data), void *data)
{
  Invoke *i = (Invoke *)malloc(sizeof(Invoke));
  if (!i) {
    return;
  }
  i->fn = fn;
  i->data = data;
  i->next = NULL;
  mutex_lock(&g_invoke_lock);
  if (g_invoke_tail) {
    g_invoke_tail->next = i;
  }
  else {
    g_invoke_head = i;
  }
  g_invoke_tail = i;
  mutex_unlock(&g_invoke_lock);
  BK_window_wake();
}

const char *awra_backend(void)
{
  return BK_window_backend();
}

/* Timers: one-shot loop timers, re-armed while the callback returns true. */
typedef struct Source {
  unsigned id;
  int timer;
  unsigned ms;
  bool (*fn)(void *data);
  void *data;
  AwraDestroyNotify destroy;
  struct Source *next;
} Source;
static Source *g_sources;
static unsigned g_next_source = 1;

static void source_free(Source *s)
{
  for (Source **p = &g_sources; *p; p = &(*p)->next) {
    if (*p == s) {
      *p = s->next;
      break;
    }
  }
  if (s->destroy) {
    s->destroy(s->data);
  }
  free(s);
}

static void source_fire(void *data)
{
  Source *s = (Source *)data;
  s->timer = 0;
  const unsigned id = s->id;
  const bool again = s->fn(s->data);
  /* the callback may have removed itself */
  for (Source *it = g_sources; it; it = it->next) {
    if (it == s && it->id == id) {
      if (again) {
        s->timer = BK_loop_timer((int)s->ms, source_fire, s);
      }
      else {
        source_free(s);
      }
      return;
    }
  }
}

unsigned awra_timeout_add(unsigned ms, bool (*fn)(void *data), void *data,
                          AwraDestroyNotify destroy)
{
  Source *s = (Source *)calloc(1, sizeof(Source));
  if (!s) {
    return 0;
  }
  s->id = g_next_source++;
  s->ms = ms;
  s->fn = fn;
  s->data = data;
  s->destroy = destroy;
  s->next = g_sources;
  g_sources = s;
  s->timer = BK_loop_timer((int)ms, source_fire, s);
  return s->id;
}

/* g_timeout_add_seconds: due on the second, at a per-session mark (the
 * hash of the session bus's address, else the host's name), so that a
 * session's coarse timers wake together: between 3/4 s early and 3/4 s
 * late. Then every `seconds` (not re-aligned). */
unsigned awra_timeout_add_seconds(unsigned seconds, bool (*fn)(void *data), void *data,
                                  AwraDestroyNotify destroy)
{
  static long long perturb = -1;
  if (perturb < 0) {
    const char *a = getenv("DBUS_SESSION_BUS_ADDRESS");
    if (!a) {
      a = getenv("HOSTNAME");
    }
    perturb = 0;
    if (a) {
      unsigned h = 5381; /* g_str_hash */
      for (const signed char *p = (const signed char *)a; *p; p++) {
        h = (h << 5) + h + (unsigned)*p;
      }
      perturb = llabs((long long)(int)h) % 1000000;
    }
  }
  long long now;
#ifdef _WIN32
  now = (long long)(bk_time() * 1e6);
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts); /* g_get_monotonic_time */
  now = (long long)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
#endif
  long long due = now + (long long)seconds * 1000000 - perturb;
  const long long rem = due % 1000000;
  if (rem >= 1000000 / 4) {
    due += 1000000;
  }
  due = due - rem + perturb;
  const long long first = due > now ? (due - now + 999) / 1000 : 0;
  const unsigned id = awra_timeout_add((unsigned)first, fn, data, destroy);
  for (Source *s = g_sources; s; s = s->next) {
    if (s->id == id) {
      s->ms = seconds * 1000; /* its repeats */
    }
  }
  return id;
}

unsigned awra_idle_add(bool (*fn)(void *data), void *data, AwraDestroyNotify destroy)
{
  return awra_timeout_add(0, fn, data, destroy);
}

void awra_source_remove(unsigned id)
{
  for (Source *s = g_sources; s; s = s->next) {
    if (s->id == id) {
      if (s->timer) {
        BK_loop_cancel_timer(s->timer);
      }
      source_free(s);
      return;
    }
  }
}

/* ======================================================================= */
/* Style classes */

static const char *g_class_builtin[] = {
    "accent",  "muted",       "flat",     "circular", "dim-label",   "numeric",
    "success", "warning",     "error",    "pill",     "compact",     "card",
    "heading", "caption",     "destructive-action", "suggested-action", "osd"};
/* every class name ever used, interned for the process (the number is the
 * class's bit while it is under 64); a hash finds them */
static const char **g_class_names;
static int g_class_count, g_class_cap;
static int *g_class_hash; /* open addressing: class number + 1, 0 empty */
static int g_class_hash_cap;

static unsigned class_hash(const char *s)
{
  unsigned h = 2166136261u;
  for (; *s; s++) {
    h = (h ^ (unsigned char)*s) * 16777619u;
  }
  return h;
}

static bool class_intern(const char *name)
{
  if (g_class_count == 0xFFFF) {
    return false; /* uint16_t numbers in widgets */
  }
  if (g_class_count == g_class_cap) {
    const int cap = g_class_cap ? g_class_cap * 2 : 64;
    const char **n = (const char **)realloc(g_class_names, sizeof(char *) * (size_t)cap);
    if (!n) {
      return false;
    }
    g_class_names = n;
    g_class_cap = cap;
  }
  if ((g_class_count + 1) * 2 > g_class_hash_cap) {
    const int cap = g_class_hash_cap ? g_class_hash_cap * 2 : 128;
    int *h = (int *)calloc((size_t)cap, sizeof(int));
    if (!h) {
      return false;
    }
    for (int i = 0; i < g_class_count; i++) {
      unsigned k = class_hash(g_class_names[i]) & (unsigned)(cap - 1);
      while (h[k]) {
        k = (k + 1) & (unsigned)(cap - 1);
      }
      h[k] = i + 1;
    }
    free(g_class_hash);
    g_class_hash = h;
    g_class_hash_cap = cap;
  }
  g_class_names[g_class_count] = name;
  unsigned k = class_hash(name) & (unsigned)(g_class_hash_cap - 1);
  while (g_class_hash[k]) {
    k = (k + 1) & (unsigned)(g_class_hash_cap - 1);
  }
  g_class_hash[k] = ++g_class_count;
  return true;
}

int awra_class_bit(const char *name)
{
  if (!name) {
    return -1;
  }
  if (!g_class_count) {
    for (int i = 0; i < AWRA_CB_BUILTIN_COUNT; i++) {
      class_intern(g_class_builtin[i]);
    }
  }
  if (g_class_hash_cap) {
    unsigned k = class_hash(name) & (unsigned)(g_class_hash_cap - 1);
    for (; g_class_hash[k]; k = (k + 1) & (unsigned)(g_class_hash_cap - 1)) {
      if (strcmp(g_class_names[g_class_hash[k] - 1], name) == 0) {
        return g_class_hash[k] - 1;
      }
    }
  }
  char *copy = awra_strdup(name);
  if (!copy || !class_intern(copy)) {
    free(copy);
    return -1;
  }
  return g_class_count - 1;
}

bool awra_has_xclass(const AwraWidget *w, int bit)
{
  const uint16_t *x = awra_aux(w)->xclasses;
  if (!x || bit < 64) {
    return false;
  }
  int lo = 1, hi = x[0];
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    if (x[mid] == bit) {
      return true;
    }
    if (x[mid] < bit) {
      lo = mid + 1;
    }
    else {
      hi = mid - 1;
    }
  }
  return false;
}

/* Sets (on) or clears a class past the 64 bits. Returns whether it changed. */
static bool xclass_set(AwraAux *a, int bit, bool on)
{
  uint16_t *x = a->xclasses;
  const int n = x ? x[0] : 0;
  int at = 1;
  while (at <= n && x[at] < bit) {
    at++;
  }
  const bool has = at <= n && x[at] == bit;
  if (has == on) {
    return false;
  }
  if (on) {
    uint16_t *nx = (uint16_t *)realloc(x, sizeof(uint16_t) * (size_t)(n + 2));
    if (!nx) {
      return false;
    }
    memmove(nx + at + 1, nx + at, sizeof(uint16_t) * (size_t)(n + 1 - at));
    nx[at] = (uint16_t)bit;
    nx[0] = (uint16_t)(n + 1);
    a->xclasses = nx;
  }
  else if (n == 1) {
    free(x);
    a->xclasses = NULL;
  }
  else {
    memmove(x + at, x + at + 1, sizeof(uint16_t) * (size_t)(n - at));
    x[0] = (uint16_t)(n - 1);
  }
  return true;
}

/* ======================================================================= */
/* Widgets: creation and lifetime */

bool awra_is_widget(const BKNode *n)
{
  return n && n->aux && n->cls->draw_tree == awra_bk_draw_tree;
}

static void aux_free(void *p)
{
  free(((AwraAux *)p)->xclasses);
  free(p);
}

/* widgets made and not yet freed (awra_widgets_alive) */
static int g_alive;

int awra_widgets_alive(void)
{
  return g_alive;
}

AwraWidget *awra_widget_new(const AwraClass *cls)
{
  if (!g_inited) {
    awra_init(NULL);
  }
  AwraAux *aux = (AwraAux *)calloc(1, sizeof(AwraAux));
  if (!aux) {
    return NULL;
  }
  BKNode *n = bk_node_new_cls(&cls->bk);
  if (!n) {
    free(aux);
    return NULL;
  }
  aux->refs = 1;
  g_alive++;
  aux->req_w = aux->req_h = -1;
  aux->halign = aux->valign = AWRA_ALIGN_FILL;
  aux->can_target = 1;
  aux->opacity = 1.0f;
  n->aux = aux;
  n->aux_free = aux_free;
  n->ref = n; /* the kit retains / releases it through our hooks */
  n->overdraw = (short)awra_px(cls->overdraw);
  if (cls->ext_size) {
    n->ext = calloc(1, cls->ext_size);
  }
  /* class init chain: the parent class first */
  const AwraClass *chain[8];
  int depth = 0;
  for (const AwraClass *c = cls; c && depth < 8; c = c->parent_class) {
    chain[depth++] = c;
  }
  for (int i = depth - 1; i >= 0; i--) {
    if (chain[i]->init) {
      chain[i]->init(n);
    }
  }
  return n;
}

AwraWidget *awra_widget_ref(AwraWidget *w)
{
  if (w && w->aux) {
    awra_aux(w)->refs++;
  }
  return w;
}

static void widget_dispose(AwraWidget *w);

void awra_widget_unref(AwraWidget *w)
{
  if (!w || !w->aux) {
    return;
  }
  AwraAux *a = awra_aux(w);
  if (--a->refs > 0) {
    return;
  }
  widget_dispose(w);
}

static void ref_retain(void *ref)
{
  if (ref) {
    awra_widget_ref((AwraWidget *)ref);
  }
}

static void ref_release(void *ref)
{
  if (ref) {
    awra_widget_unref((AwraWidget *)ref);
  }
}

static void widget_dispose(AwraWidget *w)
{
  AwraAux *a = awra_aux(w);
  if (a->in_dispose) {
    return;
  }
  a->in_dispose = 1;
  a->refs = 1; /* handlers may ref / unref while it goes */
  awra_anim_stop_for(w);
  BKContext *ctx = bk_ctx(w);
  if (ctx) {
    bk_forget(ctx, w); /* hover, grab, focus pointers into it */
  }
  awra_emit_simple(w, AWRA_SIGNAL_DESTROY);
  if (w->parent) {
    BKNode *p = w->parent;
    awra_a11y_child_changed(p, w, false);
    /* the parent's reference is the one going: detach without releasing */
    for (int i = 0; i < p->nkids; i++) {
      if (p->kids[i] == w) {
        memmove(p->kids + i, p->kids + i + 1, sizeof(BKNode *) * (size_t)(p->nkids - i - 1));
        p->nkids--;
        break;
      }
    }
    w->parent = NULL;
  }
  BK_node_free(w); /* children released, free_ext (finalize), aux freed */
  g_alive--;
}

void awra_bk_free(BKNode *n)
{
  AwraAux *a = awra_aux(n);
  const AwraClass *chain[8];
  int depth = 0;
  for (const AwraClass *c = AWRA_CLASS_OF(n); c && depth < 8; c = c->parent_class) {
    chain[depth++] = c;
  }
  for (int i = 0; i < depth; i++) {
    if (chain[i]->finalize) {
      chain[i]->finalize(n);
    }
  }
  free(n->ext);
  n->ext = NULL;
  awra_release_handlers_and_data(n);
  free(a->name);
  a->name = NULL;
  if (a->a11y) {
    awra_a11y_free(n);
  }
}

/* A widget's signal handlers and data go (their destroy notifies run): what
 * an application's closures hold is released. */
void awra_release_handlers_and_data(AwraWidget *n)
{
  AwraAux *a = awra_aux(n);
  for (AwraHandlerEntry *h = a->handlers; h;) {
    AwraHandlerEntry *next = h->next;
    if (h->destroy) {
      h->destroy(h->data);
    }
    free(h);
    h = next;
  }
  a->handlers = NULL;
  /* detached first: a destroy notify may look up or clear this widget's data
   * (a popover its menu button drops unlinks itself from it) */
  AwraDataEntry *data = a->data;
  a->data = NULL;
  for (AwraDataEntry *e = data; e;) {
    AwraDataEntry *next = e->next;
    if (e->destroy) {
      e->destroy(e->data);
    }
    free(e->key);
    free(e);
    e = next;
  }
  a->handler_mask = 0;
}

static void collect_tree(AwraWidget *w, AwraWidget ***list, int *n, int *cap)
{
  if (!awra_is_widget(w)) {
    return;
  }
  for (int i = 0; i < *n; i++) {
    if ((*list)[i] == w) {
      return;
    }
  }
  if (*n == *cap) {
    *cap = *cap ? *cap * 2 : 256;
    AwraWidget **grown = (AwraWidget **)realloc(*list, sizeof(AwraWidget *) * (size_t)*cap);
    if (!grown) {
      return;
    }
    *list = grown;
  }
  (*list)[(*n)++] = awra_widget_ref(w);
  for (int i = 0; i < w->nkids; i++) {
    collect_tree(w->kids[i], list, n, cap);
  }
  AwraWidget *pop = awra_attached_popover(w); /* popover.c: a menu button's, a dropdown's */
  if (pop) {
    collect_tree(pop, list, n, cap);
  }
}

/* GTK's dispose of a destroyed window's widgets: every widget of the tree
 * (and the popovers its menu buttons own) lets its handlers and data go, so
 * closures that hold widgets of the window (a dialog's button holding the
 * dialog) no longer keep it alive. */
void awra_dispose_tree(AwraWidget *root)
{
  AwraWidget **list = NULL;
  int n = 0, cap = 0;
  collect_tree(root, &list, &n, &cap);
  for (int i = 0; i < n; i++) {
    awra_release_handlers_and_data(list[i]);
  }
  for (int i = 0; i < n; i++) {
    awra_widget_unref(list[i]);
  }
  free(list);
}

void awra_bk_finish_edit(BKNode *n, bool confirm)
{
  (void)n;
  (void)confirm;
}

/* ----------------------------------------------------------------------- */
/* Tree */

static void set_mapped_tree(AwraWidget *w, bool mapped);
void awra_window_focus_unmapped(AwraWidget *w); /* window.c */
void awra_window_hover_unmapped(AwraWidget *w); /* window.c */

void awra_child_insert(AwraWidget *parent, AwraWidget *child, int index)
{
  if (!parent || !child || child == parent) {
    return;
  }
  BK_node_add(parent, child, index); /* retains: the parent's reference */
  awra_widget_queue_resize(parent);
  if (awra_is_widget(child)) {
    awra_aux(child)->needs_allocate = 1;
    if (awra_widget_mapped(parent)) {
      set_mapped_tree(child, true);
    }
  }
  awra_a11y_child_changed(parent, child, true);
}

void awra_child_remove(AwraWidget *parent, AwraWidget *child)
{
  if (!parent || !child || child->parent != parent) {
    return;
  }
  BKContext *ctx = bk_ctx(parent);
  if (ctx) {
    bk_invalidate(child);
    bk_forget(ctx, child);
  }
  awra_a11y_child_changed(parent, child, false);
  if (awra_is_widget(child)) {
    set_mapped_tree(child, false);
  }
  awra_widget_queue_resize(parent);
  BK_node_remove(parent, child); /* releases: may free it */
}

void awra_set_slot(AwraWidget *parent, AwraWidget **slot, AwraWidget *child)
{
  if (*slot == child) {
    return;
  }
  AwraWidget *old = *slot;
  *slot = child;
  if (old) {
    awra_child_remove(parent, old);
  }
  if (child) {
    if (child->parent) {
      awra_widget_ref(child);
      awra_container_remove(child->parent, child);
      awra_child_insert(parent, child, -1);
      awra_widget_unref(child);
    }
    else {
      awra_child_insert(parent, child, -1);
    }
  }
}

int awra_child_index(const AwraWidget *parent, const AwraWidget *child)
{
  for (int i = 0; parent && i < parent->nkids; i++) {
    if (parent->kids[i] == child) {
      return i;
    }
  }
  return -1;
}

void awra_container_add(AwraWidget *container, AwraWidget *child)
{
  if (!container || !child) {
    return;
  }
  const AwraClass *c = AWRA_CLASS_OF(container);
  if (c->add) {
    c->add(container, child);
  }
  else {
    awra_child_insert(container, child, -1);
  }
}

void awra_container_remove(AwraWidget *container, AwraWidget *child)
{
  if (!container || !child || child->parent != container) {
    return;
  }
  const AwraClass *c = AWRA_CLASS_OF(container);
  if (c->remove) {
    c->remove(container, child);
  }
  else {
    awra_child_remove(container, child);
  }
}

void awra_container_remove_all(AwraWidget *container)
{
  while (container && container->nkids > 0) {
    awra_container_remove(container, container->kids[container->nkids - 1]);
  }
}

void awra_widget_destroy(AwraWidget *w)
{
  if (!w) {
    return;
  }
  if (awra_widget_is(w, "AwraWindow")) {
    awra_window_close(w);
    return;
  }
  if (w->parent) {
    awra_container_remove(w->parent, w);
  }
}

const char *awra_widget_type(const AwraWidget *w)
{
  return awra_is_widget(w) ? AWRA_CLASS_OF(w)->type : (w ? w->cls->name : "");
}

bool awra_widget_is(const AwraWidget *w, const char *type)
{
  if (!awra_is_widget(w)) {
    return false;
  }
  for (const AwraClass *c = AWRA_CLASS_OF(w); c; c = c->parent_class) {
    if (strcmp(c->type, type) == 0) {
      return true;
    }
  }
  /* variants of a class (search entry, toggle button, ...) and composites
   * (dropdown: a button with data) */
  const char *sub = (const char *)awra_widget_get_data(w, "awra-type");
  if (sub && strcmp(sub, type) == 0) {
    return true;
  }
  static const char *const composites[][2] = {
      {"AwraDropdown", "awra-dropdown"},          {"AwraMenuButton", "awra-menu-button"},
      {"AwraVolumeControl", "awra-volume"},       {"AwraCommandPalette", "awra-palette"},
      {"AwraDialog", "awra-dialog"},
  };
  for (size_t i = 0; i < sizeof(composites) / sizeof(composites[0]); i++) {
    if (strcmp(composites[i][0], type) == 0) {
      return awra_widget_get_data(w, composites[i][1]) != NULL;
    }
  }
  return false;
}

AwraWidget *awra_widget_parent(const AwraWidget *w)
{
  return w ? w->parent : NULL;
}

int awra_widget_child_count(const AwraWidget *w)
{
  return w ? w->nkids : 0;
}

AwraWidget *awra_widget_child(const AwraWidget *w, int i)
{
  return (w && i >= 0 && i < w->nkids) ? w->kids[i] : NULL;
}

AwraWidget *awra_widget_root(const AwraWidget *w)
{
  return awra_window_of(w);
}

/* ======================================================================= */
/* Signals */

static unsigned long g_next_handler = 1;

unsigned long awra_signal_connect(AwraWidget *w, AwraSignal signal, AwraHandler fn, void *data,
                                  AwraDestroyNotify destroy)
{
  if (!awra_is_widget(w) || !fn || signal <= 0 || signal >= AWRA_SIGNAL_COUNT) {
    if (destroy) {
      destroy(data);
    }
    return 0;
  }
  AwraHandlerEntry *h = (AwraHandlerEntry *)calloc(1, sizeof(AwraHandlerEntry));
  if (!h) {
    return 0;
  }
  AwraAux *a = awra_aux(w);
  h->id = g_next_handler++;
  h->signal = signal;
  h->fn = fn;
  h->data = data;
  h->destroy = destroy;
  /* append: handlers run in connection order */
  AwraHandlerEntry **p = &a->handlers;
  while (*p) {
    p = &(*p)->next;
  }
  *p = h;
  a->handler_mask |= 1ul << signal;
  /* raw pointer signals make any widget a target of the pointer */
  if (signal == AWRA_SIGNAL_PRESSED || signal == AWRA_SIGNAL_RELEASED ||
      signal == AWRA_SIGNAL_CLICKED)
  {
    w->flags |= NF_EVENTS;
  }
  return h->id;
}

static void handlers_recompute_mask(AwraAux *a)
{
  a->handler_mask = 0;
  for (AwraHandlerEntry *h = a->handlers; h; h = h->next) {
    if (h->fn) {
      a->handler_mask |= 1ul << h->signal;
    }
  }
}

static int g_emitting; /* emissions in progress: disconnection only marks */

static void handlers_sweep(AwraAux *a)
{
  for (AwraHandlerEntry **p = &a->handlers; *p;) {
    AwraHandlerEntry *h = *p;
    if (!h->fn) {
      *p = h->next;
      if (h->destroy) {
        h->destroy(h->data);
      }
      free(h);
    }
    else {
      p = &h->next;
    }
  }
}

void awra_signal_disconnect(AwraWidget *w, unsigned long id)
{
  if (!awra_is_widget(w)) {
    return;
  }
  AwraAux *a = awra_aux(w);
  for (AwraHandlerEntry *h = a->handlers; h; h = h->next) {
    if (h->id == id) {
      h->fn = NULL;
    }
  }
  if (!g_emitting) {
    handlers_sweep(a);
  }
  handlers_recompute_mask(a);
}

void awra_signal_block(AwraWidget *w, unsigned long id, bool blocked)
{
  if (!awra_is_widget(w)) {
    return;
  }
  for (AwraHandlerEntry *h = awra_aux(w)->handlers; h; h = h->next) {
    if (h->id == id) {
      h->blocked = blocked;
    }
  }
}

bool awra_has_handlers(const AwraWidget *w, AwraSignal signal)
{
  return awra_is_widget(w) && (awra_aux(w)->handler_mask & (1ul << signal)) != 0;
}

bool awra_emit(AwraWidget *w, AwraArgs *args)
{
  if (!awra_has_handlers(w, args->signal)) {
    return false;
  }
  bool handled = false;
  if (!args->handled) {
    args->handled = &handled;
  }
  AwraAux *a = awra_aux(w);
  awra_widget_ref(w);
  g_emitting++;
  for (AwraHandlerEntry *h = a->handlers; h; h = h->next) {
    if (h->signal == args->signal && h->fn && !h->blocked) {
      h->fn(w, args, h->data);
      if (*args->handled) {
        break;
      }
    }
  }
  g_emitting--;
  if (!g_emitting) {
    handlers_sweep(a);
  }
  const bool r = *args->handled;
  awra_widget_unref(w);
  return r;
}

bool awra_signal_emit(AwraWidget *w, const AwraArgs *args)
{
  AwraArgs copy = *args; /* the caller's handled flag, if any, is set too */
  return awra_emit(w, &copy);
}

bool awra_emit_simple(AwraWidget *w, AwraSignal signal)
{
  AwraArgs a = {0};
  a.signal = signal;
  return awra_emit(w, &a);
}

bool awra_emit_index(AwraWidget *w, AwraSignal signal, int index)
{
  AwraArgs a = {0};
  a.signal = signal;
  a.index = index;
  return awra_emit(w, &a);
}

bool awra_emit_value(AwraWidget *w, AwraSignal signal, double value)
{
  AwraArgs a = {0};
  a.signal = signal;
  a.value = value;
  return awra_emit(w, &a);
}

bool awra_emit_text(AwraWidget *w, AwraSignal signal, const char *text)
{
  AwraArgs a = {0};
  a.signal = signal;
  a.text = text;
  return awra_emit(w, &a);
}

bool awra_emit_active(AwraWidget *w, AwraSignal signal, bool active)
{
  AwraArgs a = {0};
  a.signal = signal;
  a.active = active;
  return awra_emit(w, &a);
}

/* ======================================================================= */
/* Generic widget properties */

float awra_scale(void)
{
  return bk_scale_fac > 0.0f ? bk_scale_fac : 1.0f;
}

AwraWidget *awra_window_of(const AwraWidget *w)
{
  const BKNode *n = w;
  while (n && n->parent) {
    n = n->parent;
  }
  if (n && awra_is_widget(n) && strcmp(AWRA_CLASS_OF(n)->type, "AwraWindow") == 0) {
    return (AwraWidget *)n;
  }
  return NULL;
}

BKContext *awra_ctx(const AwraWidget *w)
{
  return w ? bk_ctx(w) : NULL;
}

void awra_invalidate(AwraWidget *w)
{
  awra_a11y_dirty(w);
  if (w && w->w > 0 && bk_ctx(w)) {
    bk_invalidate(w);
  }
}

void awra_widget_queue_draw(AwraWidget *w)
{
  awra_invalidate(w);
}

static void cache_clear_up(AwraWidget *w)
{
  for (BKNode *n = w; n; n = n->parent) {
    if (!awra_is_widget(n)) {
      continue;
    }
    AwraAux *a = awra_aux(n);
    a->cache_valid = 0;
    a->expand_valid = 0;
    a->mode_valid = 0;
    a->needs_allocate = 1;
  }
}

void awra_widget_queue_resize(AwraWidget *w)
{
  if (!w) {
    return;
  }
  awra_a11y_dirty(w);
  cache_clear_up(w);
  BKContext *ctx = bk_ctx(w);
  if (ctx) {
    ctx->layout_dirty = true;
    bk_ctx_request(ctx);
  }
}

void awra_queue_allocate(AwraWidget *w)
{
  for (BKNode *n = w; n; n = n->parent) {
    if (awra_is_widget(n)) {
      awra_aux(n)->needs_allocate = 1;
    }
  }
  BKContext *ctx = bk_ctx(w);
  if (ctx) {
    ctx->layout_dirty = true;
    bk_ctx_request(ctx);
  }
}

static void set_mapped_tree(AwraWidget *w, bool mapped)
{
  if (!awra_is_widget(w)) {
    return;
  }
  AwraAux *a = awra_aux(w);
  const bool now = mapped && (w->flags & NF_VISIBLE);
  if ((bool)a->mapped == now) {
    return;
  }
  if (!now && (AWRA_CLASS_OF(w)->flags & AWRA_CLASS_POPUP) && w->parent) {
    awra_a11y_child_changed(w->parent, w, false); /* a popover closes */
  }
  a->mapped = now;
  if (!now) {
    awra_window_focus_unmapped(w);
    if (a->hovered) {
      awra_window_hover_unmapped(w);
    }
  }
  if (AWRA_CLASS_OF(w)->map) {
    AWRA_CLASS_OF(w)->map(w, now);
  }
  awra_emit_simple(w, now ? AWRA_SIGNAL_MAP : AWRA_SIGNAL_UNMAP);
  for (int i = 0; i < w->nkids; i++) {
    set_mapped_tree(w->kids[i], now);
  }
  awra_a11y_dirty(w);
  if (now && (AWRA_CLASS_OF(w)->flags & AWRA_CLASS_POPUP) && w->parent) {
    awra_a11y_child_changed(w->parent, w, true); /* a popover opens */
  }
}

void awra_map_tree(AwraWidget *w, bool mapped); /* window.c */
void awra_map_tree(AwraWidget *w, bool mapped)
{
  set_mapped_tree(w, mapped);
}

static void set_effective_visible(AwraWidget *w, bool visible)
{
  if (((w->flags & NF_VISIBLE) != 0) == visible) {
    return;
  }
  if (!visible) {
    if (w->parent) {
      awra_a11y_child_changed(w->parent, w, false);
    }
    awra_invalidate(w);
    BKContext *ctx = bk_ctx(w);
    if (ctx) {
      bk_forget(ctx, w);
    }
    w->flags &= ~NF_VISIBLE;
  }
  else {
    w->flags |= NF_VISIBLE;
    if (awra_is_widget(w)) {
      awra_aux(w)->needs_allocate = 1;
    }
    /* shown again where it was: nothing else damages its area */
    awra_invalidate(w);
    if (w->parent) {
      awra_a11y_child_changed(w->parent, w, true);
    }
  }
  if (w->parent && awra_is_widget(w->parent)) {
    set_mapped_tree(w, visible && awra_aux(w->parent)->mapped);
  }
  awra_widget_queue_resize(w->parent ? w->parent : w);
}

void awra_widget_set_visible(AwraWidget *w, bool visible)
{
  if (!w) {
    return;
  }
  if (!awra_is_widget(w)) {
    set_effective_visible(w, visible);
    return;
  }
  AwraAux *a = awra_aux(w);
  a->user_hidden = !visible;
  set_effective_visible(w, visible && !a->child_hidden);
}

bool awra_widget_visible(const AwraWidget *w)
{
  if (!w) {
    return false;
  }
  if (awra_is_widget(w)) {
    return !awra_aux(w)->user_hidden;
  }
  return (w->flags & NF_VISIBLE) != 0;
}

void awra_widget_set_child_visible(AwraWidget *w, bool visible)
{
  if (!w || !awra_is_widget(w)) {
    return;
  }
  AwraAux *a = awra_aux(w);
  if ((bool)a->child_hidden == !visible) {
    return;
  }
  a->child_hidden = !visible;
  set_effective_visible(w, visible && !a->user_hidden);
}

bool awra_widget_child_visible(const AwraWidget *w)
{
  return w && awra_is_widget(w) && !awra_aux(w)->child_hidden;
}

bool awra_widget_mapped(const AwraWidget *w)
{
  return awra_is_widget(w) && awra_aux(w)->mapped;
}

void awra_widget_set_sensitive(AwraWidget *w, bool sensitive)
{
  if (!w || ((w->flags & NF_ENABLED) != 0) == sensitive) {
    return;
  }
  if (sensitive) {
    w->flags |= NF_ENABLED;
  }
  else {
    w->flags &= ~NF_ENABLED;
    BKContext *ctx = bk_ctx(w);
    if (ctx && ctx->focus && (ctx->focus == w)) {
      BK_focus_node(ctx, NULL);
    }
  }
  awra_invalidate(w);
  if (awra_a11y_on) {
    awra_a11y_dirty_tree(w); /* descendants are disabled with it */
  }
}

bool awra_widget_sensitive(const AwraWidget *w)
{
  return w && (w->flags & NF_ENABLED);
}

bool awra_sensitive(const AwraWidget *w)
{
  for (const BKNode *n = w; n; n = n->parent) {
    if (!(n->flags & NF_ENABLED)) {
      return false;
    }
  }
  return true;
}

#define AUX_OR_RETURN(w)        \
  if (!awra_is_widget(w)) {     \
    return;                     \
  }                             \
  AwraAux *a = awra_aux(w)

void awra_widget_set_hexpand(AwraWidget *w, bool expand)
{
  AUX_OR_RETURN(w);
  a->hexpand = expand;
  a->hexpand_set = 1;
  awra_widget_queue_resize(w);
}

void awra_widget_set_vexpand(AwraWidget *w, bool expand)
{
  AUX_OR_RETURN(w);
  a->vexpand = expand;
  a->vexpand_set = 1;
  awra_widget_queue_resize(w);
}

bool awra_widget_hexpand(const AwraWidget *w)
{
  return awra_is_widget(w) && awra_aux(w)->hexpand;
}

bool awra_widget_vexpand(const AwraWidget *w)
{
  return awra_is_widget(w) && awra_aux(w)->vexpand;
}

void awra_widget_set_halign(AwraWidget *w, AwraAlign align)
{
  AUX_OR_RETURN(w);
  a->halign = (unsigned char)align;
  awra_widget_queue_resize(w);
}

void awra_widget_set_valign(AwraWidget *w, AwraAlign align)
{
  AUX_OR_RETURN(w);
  a->valign = (unsigned char)align;
  awra_widget_queue_resize(w);
}

void awra_widget_set_margins(AwraWidget *w, int top, int end, int bottom, int start)
{
  AUX_OR_RETURN(w);
  a->margin[0] = (short)top;
  a->margin[1] = (short)end;
  a->margin[2] = (short)bottom;
  a->margin[3] = (short)start;
  awra_widget_queue_resize(w);
}

int awra_widget_margin(AwraWidget *w, int side)
{
  return awra_is_widget(w) && side >= 0 && side < 4 ? awra_aux(w)->margin[side] : 0;
}

bool awra_widget_focus_within(AwraWidget *w)
{
  AwraWidget *win = awra_window_of(w);
  for (AwraWidget *f = win ? awra_window_focus(win) : NULL; f; f = (AwraWidget *)f->parent) {
    if (f == w) {
      return true;
    }
    if (!awra_is_widget(f->parent)) {
      break;
    }
  }
  return false;
}

void awra_widget_set_margin_top(AwraWidget *w, int m)
{
  AUX_OR_RETURN(w);
  a->margin[0] = (short)m;
  awra_widget_queue_resize(w);
}

void awra_widget_set_margin_end(AwraWidget *w, int m)
{
  AUX_OR_RETURN(w);
  a->margin[1] = (short)m;
  awra_widget_queue_resize(w);
}

void awra_widget_set_margin_bottom(AwraWidget *w, int m)
{
  AUX_OR_RETURN(w);
  a->margin[2] = (short)m;
  awra_widget_queue_resize(w);
}

void awra_widget_set_margin_start(AwraWidget *w, int m)
{
  AUX_OR_RETURN(w);
  a->margin[3] = (short)m;
  awra_widget_queue_resize(w);
}

void awra_widget_set_margin(AwraWidget *w, AwraSpacing s)
{
  const int m = s == AWRA_SPACING_NONE ? 0 : awra_spacing(s);
  awra_widget_set_margins(w, m, m, m, m);
}

void awra_widget_measure(AwraWidget *w, AwraOrientation o, int for_size, int *mn, int *nt)
{
  int a = 0, b = 0;
  if (w && awra_is_widget(w)) {
    awra_measure(w, o, for_size >= 0 ? awra_px(for_size) : -1, &a, &b);
  }
  const double s = awra_scale();
  if (mn) {
    *mn = (int)(a / s + 0.5);
  }
  if (nt) {
    *nt = (int)(b / s + 0.5);
  }
}

void awra_widget_get_size_request(const AwraWidget *w, int *width, int *height)
{
  const AwraAux *a = w && awra_is_widget(w) ? awra_aux(w) : NULL;
  if (width) {
    *width = a ? a->req_w : -1;
  }
  if (height) {
    *height = a ? a->req_h : -1;
  }
}

void awra_widget_set_size_request(AwraWidget *w, int width, int height)
{
  AUX_OR_RETURN(w);
  a->req_w = (short)width;
  a->req_h = (short)height;
  awra_widget_queue_resize(w);
}

bool awra_widget_get_monitor_geometry(AwraWidget *w, int *x, int *y, int *width, int *height)
{
  BKContext *ctx = w ? bk_ctx(w) : NULL;
  BKRect r;
  if (!ctx || !bk_window_monitor(ctx, &r)) {
    return false;
  }
  const float s = awra_scale();
  if (x) {
    *x = (int)floorf(r.xmin / s);
  }
  if (y) {
    *y = (int)floorf(r.ymin / s);
  }
  if (width) {
    *width = (int)floorf(bk_rw(&r) / s);
  }
  if (height) {
    *height = (int)floorf(bk_rh(&r) / s);
  }
  return true;
}

void awra_widget_set_tooltip(AwraWidget *w, const char *text)
{
  if (!w) {
    return;
  }
  bk_set_text(&w->desc, text && text[0] ? text : NULL);
  awra_a11y_dirty(w);
}

void awra_widget_set_typography(AwraWidget *w, AwraTypography t)
{
  AUX_OR_RETURN(w);
  a->typography = (unsigned char)t;
  a->typography_set = 1;
  if (AWRA_CLASS_OF(w)->style_changed) {
    AWRA_CLASS_OF(w)->style_changed(w);
  }
  /* children inherit it */
  for (int i = 0; i < w->nkids; i++) {
    BKNode *c = w->kids[i];
    if (awra_is_widget(c) && !awra_aux(c)->typography_set && AWRA_CLASS_OF(c)->style_changed) {
      AWRA_CLASS_OF(c)->style_changed(c);
    }
  }
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

/* ----------------------------------------------------------------------- */
/* Reading direction */

static AwraTextDirection g_default_dir = AWRA_TEXT_DIR_LTR;

/* The languages GTK's translations say read right to left ("default:RTL"
 * in gtk40's ar, az_IR, fa, he, ps, ug, ur, yi). */
static AwraTextDirection dir_of_language(const char *lang)
{
  static const char *const rtl[] = {"ar", "fa", "he", "ps", "ug", "ur", "yi", "az_IR"};
  if (!lang || !lang[0]) {
    return AWRA_TEXT_DIR_LTR;
  }
  for (size_t i = 0; i < sizeof(rtl) / sizeof(rtl[0]); i++) {
    const size_t n = strlen(rtl[i]);
    if (strncmp(lang, rtl[i], n) == 0 && (lang[n] == 0 || lang[n] == '_' || lang[n] == '.' ||
                                          lang[n] == '@' || lang[n] == ':' || lang[n] == '-'))
    {
      return AWRA_TEXT_DIR_RTL;
    }
  }
  return AWRA_TEXT_DIR_LTR;
}

AwraTextDirection awra_get_locale_direction(void)
{
#ifdef _WIN32
  wchar_t wname[LOCALE_NAME_MAX_LENGTH];
  char name[LOCALE_NAME_MAX_LENGTH];
  const LANGID id = GetUserDefaultUILanguage();
  if (LCIDToLocaleName(MAKELCID(id, SORT_DEFAULT), wname, LOCALE_NAME_MAX_LENGTH, 0) > 0 &&
      WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, (int)sizeof(name), NULL, NULL) > 0)
  {
    return dir_of_language(name);
  }
  return AWRA_TEXT_DIR_LTR;
#else
  /* gettext's order for messages: LC_ALL, LC_MESSAGES, LANG decide the
   * locale; LANGUAGE's list goes first unless that locale is C */
  const char *loc = NULL;
  static const char *const vars[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
  for (int i = 0; i < 3 && !loc; i++) {
    const char *v = getenv(vars[i]);
    if (v && v[0]) {
      loc = v;
    }
  }
  if (!loc || !strcmp(loc, "C") || !strncmp(loc, "C.", 2) || !strcmp(loc, "POSIX")) {
    return AWRA_TEXT_DIR_LTR;
  }
  const char *language = getenv("LANGUAGE");
  if (language && language[0]) {
    return dir_of_language(language); /* its first entry */
  }
  return dir_of_language(loc);
#endif
}

static void init_default_direction(void)
{
  g_default_dir = awra_get_locale_direction();
  const char *dbg = getenv("GTK_DEBUG");
  if (dbg && strstr(dbg, "invert-text-dir")) {
    g_default_dir = g_default_dir == AWRA_TEXT_DIR_RTL ? AWRA_TEXT_DIR_LTR : AWRA_TEXT_DIR_RTL;
  }
}

bool awra_widget_rtl(const AwraWidget *w)
{
  const unsigned d = w && awra_is_widget(w) ? awra_aux(w)->dir : AWRA_TEXT_DIR_NONE;
  return (d == AWRA_TEXT_DIR_NONE ? (unsigned)g_default_dir : d) == AWRA_TEXT_DIR_RTL;
}

float awra_mirror_x(const AwraWidget *w, float x, float width)
{
  return awra_widget_rtl(w) ? 2.0f * (float)w->x + (float)w->w - x - width : x;
}

AwraTextDirection awra_widget_get_direction(const AwraWidget *w)
{
  return awra_widget_rtl(w) ? AWRA_TEXT_DIR_RTL : AWRA_TEXT_DIR_LTR;
}

/* the direction changed for `w` and the descendants that follow it: they
 * measure, place and draw themselves again */
static void direction_changed(AwraWidget *w, bool all)
{
  if (!awra_is_widget(w)) {
    return;
  }
  if (AWRA_CLASS_OF(w)->style_changed) {
    AWRA_CLASS_OF(w)->style_changed(w);
  }
  for (int i = 0; i < w->nkids; i++) {
    BKNode *c = w->kids[i];
    if (awra_is_widget(c) && (all || awra_aux(c)->dir == AWRA_TEXT_DIR_NONE)) {
      direction_changed(c, all);
    }
  }
  awra_aux(w)->needs_allocate = 1;
  awra_widget_queue_resize(w);
  awra_invalidate(w);
  awra_a11y_dirty(w);
}

void awra_widget_set_direction(AwraWidget *w, AwraTextDirection dir)
{
  AUX_OR_RETURN(w);
  if (dir < AWRA_TEXT_DIR_NONE || dir > AWRA_TEXT_DIR_RTL || a->dir == (unsigned)dir) {
    return;
  }
  const bool was = awra_widget_rtl(w);
  a->dir = (unsigned)dir;
  if (awra_widget_rtl(w) != was) {
    direction_changed(w, false);
  }
}

void awra_windows_direction_changed(void (*fn)(AwraWidget *)); /* window.c */

static void default_direction_changed(AwraWidget *win)
{
  direction_changed(win, false);
}

void awra_widget_set_default_direction(AwraTextDirection dir)
{
  if ((dir != AWRA_TEXT_DIR_LTR && dir != AWRA_TEXT_DIR_RTL) || dir == g_default_dir) {
    return;
  }
  g_default_dir = dir;
  awra_windows_direction_changed(default_direction_changed);
}

AwraTextDirection awra_widget_get_default_direction(void)
{
  return g_default_dir;
}

/* Under Awra 1's prose-metadata role, monospace values read as prose:
 * their font is inherited (awra.css L3923). */
static bool in_prose_metadata(const BKNode *n)
{
  static int bit = -2;
  if (bit == -2) {
    bit = awra_class_bit("prose-metadata");
  }
  for (; bit >= 0 && n; n = n->parent) {
    if (awra_is_widget(n) && awra_has_class_bit(n, bit)) {
      return true;
    }
  }
  return false;
}

AwraTypography awra_widget_typography(const AwraWidget *w)
{
  for (const BKNode *n = w; n; n = n->parent) {
    if (awra_is_widget(n) && awra_aux(n)->typography_set) {
      if (awra_aux(n)->typography == AWRA_TYPOGRAPHY_MONOSPACE && in_prose_metadata(n)) {
        continue;
      }
      return (AwraTypography)awra_aux(n)->typography;
    }
  }
  return AWRA_TYPOGRAPHY_BODY;
}

double awra_widget_opacity(AwraWidget *w)
{
  return w && awra_is_widget(w) ? awra_aux(w)->opacity : 1.0;
}

void awra_widget_set_opacity(AwraWidget *w, double opacity)
{
  AUX_OR_RETURN(w);
  a->opacity = (float)(opacity < 0 ? 0 : opacity > 1 ? 1 : opacity);
  awra_invalidate(w);
}

void awra_widget_set_can_target(AwraWidget *w, bool can_target)
{
  AUX_OR_RETURN(w);
  a->can_target = can_target;
}

void awra_widget_set_focusable(AwraWidget *w, bool focusable)
{
  AUX_OR_RETURN(w);
  a->focusable_set = 1;
  a->focusable = focusable;
}

void awra_widget_grab_focus(AwraWidget *w)
{
  AwraWidget *win = awra_window_of(w);
  if (win) {
    awra_set_focus(win, w, false);
  }
}

bool awra_widget_has_focus(const AwraWidget *w)
{
  AwraWidget *win = awra_window_of(w);
  return win && awra_focus_of(win) == w;
}

void awra_widget_set_cursor(AwraWidget *w, const char *name)
{
  AUX_OR_RETURN(w);
  static const char *names[] = {"default", "ew-resize", "ns-resize", "text",
                                "crosshair", "pointer", "move", "none",
                                "nesw-resize", "nwse-resize", "not-allowed", "wait",
                                "progress"};
  a->cursor = 0;
  for (int i = 0; name && i < (int)(sizeof(names) / sizeof(names[0])); i++) {
    if (strcmp(names[i], name) == 0) {
      a->cursor = i + 1;
    }
  }
  if (name && !strcmp(name, "grab")) {
    a->cursor = BK_CURSOR_MOVE + 1;
  }
}

void awra_widget_add_class(AwraWidget *w, const char *name)
{
  AUX_OR_RETURN(w);
  const int bit = awra_class_bit(name);
  if (bit < 0) {
    return;
  }
  if (bit < 64) {
    if (a->classes & (1ull << bit)) {
      return;
    }
    a->classes |= 1ull << bit;
  }
  else if (!xclass_set(a, bit, true)) {
    return;
  }
  if (AWRA_CLASS_OF(w)->style_changed) {
    AWRA_CLASS_OF(w)->style_changed(w);
  }
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

void awra_widget_remove_class(AwraWidget *w, const char *name)
{
  AUX_OR_RETURN(w);
  const int bit = awra_class_bit(name);
  if (bit < 0) {
    return;
  }
  if (bit < 64) {
    if (!(a->classes & (1ull << bit))) {
      return;
    }
    a->classes &= ~(1ull << bit);
  }
  else if (!xclass_set(a, bit, false)) {
    return;
  }
  if (AWRA_CLASS_OF(w)->style_changed) {
    AWRA_CLASS_OF(w)->style_changed(w);
  }
  awra_widget_queue_resize(w);
  awra_invalidate(w);
}

bool awra_widget_has_class(const AwraWidget *w, const char *name)
{
  return awra_is_widget(w) && awra_has_class_bit(w, awra_class_bit(name));
}

int awra_widget_width(const AwraWidget *w)
{
  return w ? (int)floor(w->w / awra_scale() + 0.5) : 0;
}

int awra_widget_height(const AwraWidget *w)
{
  return w ? (int)floor(w->h / awra_scale() + 0.5) : 0;
}

bool awra_widget_position(const AwraWidget *w, double *x, double *y)
{
  if (!awra_widget_mapped(w)) {
    return false;
  }
  if (x) {
    *x = w->x / awra_scale();
  }
  if (y) {
    *y = w->y / awra_scale();
  }
  return true;
}

void awra_widget_set_data(AwraWidget *w, const char *key, void *data, AwraDestroyNotify destroy)
{
  AUX_OR_RETURN(w);
  for (AwraDataEntry **p = &a->data; *p; p = &(*p)->next) {
    AwraDataEntry *e = *p;
    if (strcmp(e->key, key) == 0) {
      if (e->destroy) {
        e->destroy(e->data);
      }
      if (!data) {
        *p = e->next;
        free(e->key);
        free(e);
        return;
      }
      e->data = data;
      e->destroy = destroy;
      return;
    }
  }
  if (!data) {
    return;
  }
  AwraDataEntry *e = (AwraDataEntry *)calloc(1, sizeof(AwraDataEntry));
  if (!e) {
    return;
  }
  e->key = awra_strdup(key);
  e->data = data;
  e->destroy = destroy;
  e->next = a->data;
  a->data = e;
}

void *awra_widget_get_data(const AwraWidget *w, const char *key)
{
  if (!awra_is_widget(w)) {
    return NULL;
  }
  for (AwraDataEntry *e = awra_aux(w)->data; e; e = e->next) {
    if (strcmp(e->key, key) == 0) {
      return e->data;
    }
  }
  return NULL;
}

void awra_widget_set_name(AwraWidget *w, const char *name)
{
  AUX_OR_RETURN(w);
  awra_set_str(&a->name, name);
}

const char *awra_widget_name(const AwraWidget *w)
{
  return awra_is_widget(w) ? awra_aux(w)->name : NULL;
}

static AwraWidget *pick(AwraWidget *n, int x, int y)
{
  if (!(n->flags & NF_VISIBLE) || !bk_contains(n, x, y) ||
      (awra_is_widget(n) && !awra_aux(n)->can_target))
  {
    return NULL;
  }
  for (int i = n->nkids - 1; i >= 0; i--) {
    AwraWidget *f = pick(n->kids[i], x, y);
    if (f) {
      return f;
    }
  }
  return n;
}

AwraWidget *awra_widget_pick(AwraWidget *root, double x, double y)
{
  if (!root) {
    return NULL;
  }
  return pick(root, awra_px(x), awra_px(y));
}

bool awra_widget_set_property_string(AwraWidget *w, const char *name, const char *value)
{
  if (!awra_is_widget(w)) {
    return false;
  }
  for (const AwraClass *c = AWRA_CLASS_OF(w); c; c = c->parent_class) {
    if (c->set_string && c->set_string(w, name, value)) {
      return true;
    }
  }
  if (!strcmp(name, "tooltip")) {
    awra_widget_set_tooltip(w, value);
    return true;
  }
  if (!strcmp(name, "name")) {
    awra_widget_set_name(w, value);
    return true;
  }
  return false;
}

bool awra_widget_set_property_double(AwraWidget *w, const char *name, double value)
{
  if (!awra_is_widget(w)) {
    return false;
  }
  for (const AwraClass *c = AWRA_CLASS_OF(w); c; c = c->parent_class) {
    if (c->set_double && c->set_double(w, name, value)) {
      return true;
    }
  }
  return false;
}

const char *awra_widget_get_property_string(AwraWidget *w, const char *name)
{
  if (!awra_is_widget(w)) {
    return NULL;
  }
  for (const AwraClass *c = AWRA_CLASS_OF(w); c; c = c->parent_class) {
    const char *v = c->get_string ? c->get_string(w, name) : NULL;
    if (v) {
      return v;
    }
  }
  return NULL;
}

double awra_widget_get_property_double(AwraWidget *w, const char *name)
{
  double v = 0.0;
  if (!awra_is_widget(w)) {
    return v;
  }
  for (const AwraClass *c = AWRA_CLASS_OF(w); c; c = c->parent_class) {
    if (c->get_double && c->get_double(w, name, &v)) {
      return v;
    }
  }
  return v;
}

/* ======================================================================= */
/* Layout */

void awra_widget_isolate_expand(AwraWidget *w)
{
  if (awra_is_widget(w)) {
    awra_aux(w)->expand_isolated = 1;
    awra_aux(w)->expand_valid = 0;
  }
}

bool awra_height_for_width(AwraWidget *w)
{
  if (!awra_is_widget(w) || !(w->flags & NF_VISIBLE)) {
    return false;
  }
  AwraAux *a = awra_aux(w);
  if (!a->mode_valid) {
    const AwraClass *c = AWRA_CLASS_OF(w);
    int mode = c->request_mode ? c->request_mode(w) : -1;
    if (mode < 0) {
      mode = 0;
      for (int i = 0; i < w->nkids && !mode; i++) {
        mode = awra_child_visible(w->kids[i]) && awra_height_for_width(w->kids[i]);
      }
    }
    a->mode_hfw = mode > 0;
    a->mode_valid = 1;
  }
  return a->mode_hfw;
}

bool awra_compute_expand(AwraWidget *w, int orientation)
{
  if (!awra_is_widget(w)) {
    return false;
  }
  AwraAux *a = awra_aux(w);
  if (!(w->flags & NF_VISIBLE)) {
    return false;
  }
  if (orientation == AWRA_ORIENTATION_HORIZONTAL && a->hexpand_set) {
    return a->hexpand;
  }
  if (orientation == AWRA_ORIENTATION_VERTICAL && a->vexpand_set) {
    return a->vexpand;
  }
  if (!a->expand_valid) {
    bool h = false, v = false;
    const bool isolated = a->expand_isolated ||
                          (AWRA_CLASS_OF(w)->flags & AWRA_CLASS_EXPAND_ISOLATED);
    for (int i = 0; !isolated && i < w->nkids; i++) {
      h |= awra_compute_expand(w->kids[i], AWRA_ORIENTATION_HORIZONTAL);
      v |= awra_compute_expand(w->kids[i], AWRA_ORIENTATION_VERTICAL);
    }
    a->computed_hexpand = h;
    a->computed_vexpand = v;
    a->expand_valid = 1;
  }
  return orientation == AWRA_ORIENTATION_HORIZONTAL ? a->computed_hexpand : a->computed_vexpand;
}

bool awra_takes_undersize(AwraWidget *w)
{
  while (w && awra_is_widget(w) && (w->flags & NF_VISIBLE)) {
    const AwraClass *c = AWRA_CLASS_OF(w);
    if (c->flags & AWRA_CLASS_TAKES_UNDERSIZE) {
      return true;
    }
    w = c->bin_child ? c->bin_child(w) : NULL;
  }
  return false;
}

void awra_measure(AwraWidget *w, int o, int for_size, int *r_min, int *r_nat)
{
  int mn = 0, nt = 0;
  if (!w || !(w->flags & NF_VISIBLE)) {
    *r_min = *r_nat = 0;
    return;
  }
  if (!awra_is_widget(w)) {
    int nw, nh;
    bk_natural(w, &nw, &nh);
    *r_min = *r_nat = o == AWRA_ORIENTATION_HORIZONTAL ? nw : nh;
    return;
  }
  AwraAux *a = awra_aux(w);
  const int m_main = o == AWRA_ORIENTATION_HORIZONTAL ? awra_px(a->margin[1]) + awra_px(a->margin[3]) :
                                                         awra_px(a->margin[0]) + awra_px(a->margin[2]);
  const int m_other = o == AWRA_ORIENTATION_HORIZONTAL ? awra_px(a->margin[0]) + awra_px(a->margin[2]) :
                                                          awra_px(a->margin[1]) + awra_px(a->margin[3]);
  if ((a->cache_valid & (1u << o)) && a->cache_for[o] == for_size) {
    *r_min = a->cache_min[o];
    *r_nat = a->cache_nat[o];
    return;
  }
  int inner_for = for_size;
  if (for_size >= 0) {
    inner_for = BK_MAX(0, for_size - m_other);
    /* the other axis' size request is a floor for the size we measure for */
  }
  const AwraClass *c = AWRA_CLASS_OF(w);
  if (c->measure) {
    c->measure(w, o, inner_for, &mn, &nt);
  }
  const int req = o == AWRA_ORIENTATION_HORIZONTAL ? a->req_w : a->req_h;
  if (req >= 0) {
    mn = BK_MAX(mn, awra_px(req));
  }
  nt = BK_MAX(nt, mn);
  mn += m_main;
  nt += m_main;
  a->cache_for[o] = for_size;
  a->cache_min[o] = mn;
  a->cache_nat[o] = nt;
  a->cache_valid |= 1u << o;
  *r_min = mn;
  *r_nat = nt;
}

void awra_bk_measure(BKNode *n, int *r_w, int *r_h)
{
  int mn, nw, nh;
  awra_measure(n, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nw);
  awra_measure(n, AWRA_ORIENTATION_VERTICAL, nw, &mn, &nh);
  *r_w = nw;
  *r_h = nh;
}

/* GTK's gtk_distribute_natural_allocation: grow the children that are
 * furthest from their natural size first. Returns the space left. */
int awra_distribute(int extra, int n, int *sizes, const int *naturals)
{
  if (extra <= 0 || n <= 0) {
    return extra;
  }
  /* sort indices by the gap (natural - size), smallest first */
  int stack_idx[64];
  int *idx = n <= 64 ? stack_idx : (int *)malloc(sizeof(int) * (size_t)n);
  if (!idx) {
    return extra;
  }
  for (int i = 0; i < n; i++) {
    idx[i] = i;
  }
  for (int i = 1; i < n; i++) {
    const int k = idx[i];
    const int gk = naturals[k] - sizes[k];
    int j = i - 1;
    while (j >= 0 && naturals[idx[j]] - sizes[idx[j]] > gk) {
      idx[j + 1] = idx[j];
      j--;
    }
    idx[j + 1] = k;
  }
  for (int i = n - 1; extra > 0 && i >= 0; i--) {
    /* the remaining children share what is left; each takes at most its gap */
    const int k = idx[n - 1 - i];
    const int gap = BK_MAX(0, naturals[k] - sizes[k]);
    const int share = BK_MIN(gap, extra / (i + 1) + (extra % (i + 1) ? 1 : 0));
    const int give = BK_MIN(share, extra);
    sizes[k] += give;
    extra -= give;
  }
  if (idx != stack_idx) {
    free(idx);
  }
  return extra;
}

static int g_suppress_damage; /* an ancestor damaged its whole new and old rect */

static void damage_node_area(AwraWidget *w)
{
  BKContext *ctx = bk_ctx(w);
  if (!ctx || w->w <= 0 || w->h <= 0) {
    return;
  }
  const int m = 3 + w->overdraw_tree;
  bk_node_dirty(w, bk_rect(w->x - m, w->y - m, w->w + 2 * m, w->h + 2 * m));
}

static void update_overdraw(AwraWidget *w)
{
  short spill = w->overdraw;
  for (int i = 0; i < w->nkids; i++) {
    BKNode *k = w->kids[i];
    if (k->flags & NF_VISIBLE) {
      spill = BK_MAX(spill, k->overdraw_tree);
    }
  }
  w->overdraw_tree = spill;
}

static void allocate_rect(AwraWidget *w, int x, int y, int width, int height)
{
  AwraAux *a = awra_aux(w);
  const bool moved = w->x != x || w->y != y || w->w != width || w->h != height;
  if (!moved && !a->needs_allocate) {
    return;
  }
  int suppressed = 0;
  if (moved && !g_suppress_damage) {
    damage_node_area(w); /* where it was */
  }
  w->x = x;
  w->y = y;
  w->w = width;
  w->h = height;
  a->needs_allocate = 0;
  if (moved) {
    /* its new area is damaged below, children included */
    g_suppress_damage++;
    suppressed = 1;
  }
  if (AWRA_CLASS_OF(w)->allocate) {
    AWRA_CLASS_OF(w)->allocate(w);
  }
  if (suppressed) {
    g_suppress_damage--;
  }
  update_overdraw(w);
  if (moved && !g_suppress_damage) {
    damage_node_area(w);
  }
  if ((a->alloc_w != width || a->alloc_h != height)) {
    a->alloc_w = width;
    a->alloc_h = height;
    if (awra_has_handlers(w, AWRA_SIGNAL_RESIZE)) {
      AwraArgs args = {0};
      args.signal = AWRA_SIGNAL_RESIZE;
      args.width = awra_widget_width(w);
      args.height = awra_widget_height(w);
      awra_emit(w, &args);
    }
  }
}

/* adjust_for_align (gtkwidget.c) */
static void align_span(int align, int natural, int *pos, int *size)
{
  natural = BK_MAX(0, natural);
  switch (align) {
    case AWRA_ALIGN_CENTER:
    case AWRA_ALIGN_BASELINE:
      if (*size > natural) {
        *pos += (*size - natural) / 2;
        *size = natural;
      }
      break;
    case AWRA_ALIGN_START:
      *size = BK_MIN(*size, natural);
      break;
    case AWRA_ALIGN_END:
      if (*size > natural) {
        *pos += *size - natural;
        *size = natural;
      }
      break;
    default:
      break;
  }
}

void awra_allocate(AwraWidget *w, int x, int y, int width, int height)
{
  if (!w || !(w->flags & NF_VISIBLE)) {
    return;
  }
  if (!awra_is_widget(w)) {
    bk_arrange(w, x, y, width, height);
    return;
  }
  AwraAux *a = awra_aux(w);
  const int mt = awra_px(a->margin[0]), me = awra_px(a->margin[1]);
  const int mb = awra_px(a->margin[2]), ms = awra_px(a->margin[3]);
  x += ms;
  y += mt;
  width = BK_MAX(0, width - ms - me);
  height = BK_MAX(0, height - mt - mb);
  /* gtk_widget_adjust_size_allocation (widgets are height-for-width) */
  const bool hfill = a->halign == AWRA_ALIGN_FILL;
  const bool vfill = a->valign == AWRA_ALIGN_FILL;
  if (!hfill || !vfill) {
    const int outer_w = width + ms + me, outer_h = height + mt + mb;
    int mn, nat_w = -1, nat_h = -1;
    if (hfill) {
      awra_measure(w, AWRA_ORIENTATION_VERTICAL, outer_w, &mn, &nat_h);
      align_span(a->valign, nat_h - mt - mb, &y, &height);
    }
    else if (vfill) {
      awra_measure(w, AWRA_ORIENTATION_HORIZONTAL, outer_h, &mn, &nat_w);
      align_span(a->halign, nat_w - ms - me, &x, &width);
    }
    else {
      awra_measure(w, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nat_w);
      if (nat_w < outer_w) {
        int min_h;
        awra_measure(w, AWRA_ORIENTATION_VERTICAL, nat_w, &min_h, &nat_h);
        if (min_h > outer_h) {
          /* not tall enough for its natural width: the width for the height */
          awra_measure(w, AWRA_ORIENTATION_HORIZONTAL, outer_h, &mn, &nat_w);
          nat_h = -1;
        }
        align_span(a->halign, nat_w - ms - me, &x, &width);
      }
      if (nat_h == -1) {
        awra_measure(w, AWRA_ORIENTATION_VERTICAL, width + ms + me, &mn, &nat_h);
      }
      align_span(a->valign, nat_h - mt - mb, &y, &height);
    }
  }
  /* a right-to-left parent's children are mirrored in it, margins and
   * alignment with them (start is right), as GTK's containers place them */
  const BKNode *p = w->parent;
  if (p && awra_is_widget(p) && awra_widget_rtl(p) &&
      !(AWRA_CLASS_OF(p)->flags & AWRA_CLASS_NO_MIRROR) && !(AWRA_CLASS_OF(w)->flags & AWRA_CLASS_POPUP))
  {
    x = 2 * p->x + p->w - x - width;
  }
  allocate_rect(w, x, y, width, height);
}

void awra_bk_arrange(BKNode *n)
{
  /* the kit placed a root (window or popup) itself: lay out its contents */
  AwraAux *a = awra_aux(n);
  const bool resized = a->alloc_w != n->w || a->alloc_h != n->h;
  if (!resized && !a->needs_allocate) {
    return;
  }
  a->needs_allocate = 0;
  if (resized) {
    g_suppress_damage++;
  }
  if (AWRA_CLASS_OF(n)->allocate) {
    AWRA_CLASS_OF(n)->allocate(n);
  }
  if (resized) {
    g_suppress_damage--;
    damage_node_area(n);
  }
  update_overdraw(n);
  if (resized) {
    a->alloc_w = n->w;
    a->alloc_h = n->h;
    if (awra_has_handlers(n, AWRA_SIGNAL_RESIZE)) {
      AwraArgs args = {0};
      args.signal = AWRA_SIGNAL_RESIZE;
      args.width = awra_widget_width(n);
      args.height = awra_widget_height(n);
      awra_emit(n, &args);
    }
  }
}

/* ======================================================================= */
/* Drawing */

static AwraDraw g_draw;
void awra_paint_drop_active(AwraDraw *d, AwraWidget *w); /* dnd.c */

void awra_bk_draw_tree(BKNode *n)
{
  if (!(n->flags & NF_VISIBLE)) {
    return;
  }
  const int spill = 3 + n->overdraw_tree;
  BKRect r = bk_rect(n->x - spill, n->y - spill, n->w + 2 * spill, n->h + 2 * spill);
  if (!bk_clip_visible(&r)) {
    return;
  }
  AwraAux *a = awra_aux(n);
  const AwraClass *c = AWRA_CLASS_OF(n);
  const float saved_opacity = g_draw.opacity;
  if (g_draw.opacity <= 0.0f || bk_draw.r != g_draw.r) {
    if (bk_draw.r != g_draw.r) {
      g_draw.opacity = 1.0f;
    }
    g_draw.r = bk_draw.r;
  }
  const float base = g_draw.opacity > 0.0f ? g_draw.opacity : 1.0f;
  g_draw.opacity = base * a->opacity;
  if (g_draw.opacity > 0.003f) {
    /* rows that answer the pointer with a faint tint (awra.css's
     * .awra-role-conversation-entry:hover) */
    static int entry_bit = -2;
    if (entry_bit == -2) {
      entry_bit = awra_class_bit("conversation-entry");
    }
    if (a->hovered && awra_has_class_bit(n, entry_bit)) {
      const AwraRGBA tint = awra_with_alpha(awra_tokens()->foreground, 0.035f);
      awra_paint_rect(&g_draw, awra_box_of(n), &tint);
    }
    /* any widget can show it takes a drop ("drop-target", awra.css's
     * .awra-role-drop-zone:drop(active)) or opens under a held drag
     * ("about-to-open": the accent pulses .10 → .32 every 300 ms; the
     * application redraws it while it waits) */
    static int drop_bit = -2, open_bit = -2;
    if (drop_bit == -2) {
      drop_bit = awra_class_bit("drop-target");
      open_bit = awra_class_bit("about-to-open");
    }
    const bool opening = awra_has_class_bit(n, open_bit);
    const bool dropping = !opening && awra_has_class_bit(n, drop_bit);
    if (opening) {
      const double ph = awra_style_reduced_motion() ? 0.5 : fmod(awra_now_ms(), 300.0) / 300.0;
      const float k = (float)(1.0 - fabs(ph * 2.0 - 1.0));
      const AwraRGBA pulse = awra_with_alpha(awra_tokens()->accent, 0.10f + 0.22f * k);
      awra_paint_rbox(&g_draw, awra_box_of(n), awra_pxf(8), &pulse, 0, NULL);
    }
    else if (dropping) {
      awra_paint_rbox(&g_draw, awra_box_of(n), awra_pxf(8), &awra_tokens()->selection, 0, NULL);
    }
    const bool saved_rtl = g_draw.rtl;
    const float saved_sx = awra_shift_x, saved_sy = awra_shift_y;
    awra_shift_x += a->shift_x;
    awra_shift_y += a->shift_y;
    g_draw.rtl = awra_widget_rtl(n);
    if (c->draw) {
      c->draw(n, &g_draw);
    }
    awra_paint_drop_active(&g_draw, n);
    if (n->nkids) {
      const bool clip = (c->flags & AWRA_CLASS_CLIP) != 0 || a->overflow_hidden;
      if (clip) {
        bk_clip_push(n->x, n->y, n->w, n->h);
      }
      for (int i = 0; i < n->nkids; i++) {
        bk_draw_tree(n->kids[i]);
      }
      if (clip) {
        bk_clip_pop();
      }
    }
    g_draw.rtl = awra_widget_rtl(n);
    if (c->draw_over) {
      c->draw_over(n, &g_draw);
    }
    g_draw.rtl = saved_rtl;
    awra_shift_x = saved_sx;
    awra_shift_y = saved_sy;
    if (dropping) {
      const AwraBox b = awra_box_inset(awra_box_of(n), awra_pxf(1));
      awra_paint_rbox(&g_draw, b, awra_pxf(7), NULL, awra_pxf(2), &awra_tokens()->focus_ring);
    }
  }
  g_draw.opacity = saved_opacity > 0.0f ? saved_opacity : 1.0f;
}

/* ======================================================================= */
/* Input dispatch */

static bool targetable(const BKNode *n)
{
  if (!awra_is_widget(n)) {
    return true;
  }
  const AwraAux *a = awra_aux(n);
  return a->can_target && (n->flags & NF_ENABLED);
}

BKNode *awra_bk_hit(BKNode *n, int x, int y)
{
  if (awra_is_widget(n) && !awra_aux(n)->can_target) {
    return NULL;
  }
  for (int i = n->nkids - 1; i >= 0; i--) {
    BKNode *f = bk_hit(n->kids[i], x, y);
    if (f) {
      return f;
    }
  }
  if (!targetable(n)) {
    return NULL;
  }
  return ((n->cls->flags & CLS_INTERACTIVE) || (n->flags & NF_EVENTS)) ? n : NULL;
}

static void fill_pointer_args(AwraWidget *w, AwraArgs *args, const BKEvent *e, int button)
{
  const float s = awra_scale();
  args->x = (e->x - w->x) / s;
  args->y = (e->y - w->y) / s;
  args->button = button;
  args->mods = e->mods & (BK_SHIFT | BK_CTRL | BK_ALT);
  args->n_press = 1;
}

static int g_press_count = 1; /* 2 inside a double click */

int awra_bk_press(BKNode *n, BKEvent *e)
{
  if (!awra_sensitive(n)) {
    return 0;
  }
  AwraAux *a = awra_aux(n);
  const AwraClass *c = AWRA_CLASS_OF(n);
  BKContext *ctx = bk_ctx(n);
  /* a double click reaches here through the kit's dbl hook */
  const bool dbl = ctx && ctx->last_click_node == NULL;
  g_press_count = dbl ? 2 : 1;
  a->pressed = 1;
  if (c->flags & AWRA_CLASS_HOVER) {
    awra_animate_state(n);
  }
  if ((c->flags & AWRA_CLASS_FOCUSABLE) || (a->focusable_set && a->focusable)) {
    AwraWidget *win = awra_window_of(n);
    if (win) {
      awra_set_focus(win, n, false);
    }
  }
  int used = 0;
  if (awra_has_handlers(n, AWRA_SIGNAL_PRESSED)) {
    AwraArgs args = {0};
    args.signal = AWRA_SIGNAL_PRESSED;
    fill_pointer_args(n, &args, e, 1);
    args.n_press = g_press_count;
    used |= awra_emit(n, &args);
  }
  if (!used && c->press) {
    used = c->press(n, e);
  }
  return 1;
}

int awra_bk_drag(BKNode *n, BKEvent *e)
{
  const AwraClass *c = AWRA_CLASS_OF(n);
  int r = 0;
  if (c->drag) {
    r = c->drag(n, e);
  }
  if (awra_has_handlers(n, AWRA_SIGNAL_MOTION)) {
    AwraArgs args = {0};
    args.signal = AWRA_SIGNAL_MOTION;
    fill_pointer_args(n, &args, e, 1);
    args.dx = e->dx / awra_scale();
    args.dy = e->dy / awra_scale();
    awra_emit(n, &args);
  }
  return r;
}

int awra_bk_release(BKNode *n, BKEvent *e)
{
  AwraAux *a = awra_aux(n);
  const AwraClass *c = AWRA_CLASS_OF(n);
  a->pressed = 0;
  if (c->flags & AWRA_CLASS_HOVER) {
    awra_animate_state(n);
  }
  awra_widget_ref(n);
  int used = 0;
  if (awra_has_handlers(n, AWRA_SIGNAL_RELEASED)) {
    AwraArgs args = {0};
    args.signal = AWRA_SIGNAL_RELEASED;
    fill_pointer_args(n, &args, e, 1);
    args.n_press = g_press_count;
    used |= awra_emit(n, &args);
  }
  if (!used && c->release) {
    used = c->release(n, e);
  }
  /* a plain widget with clicked handlers clicks on release inside */
  if (!used && !c->release && e->inside && awra_sensitive(n) &&
      awra_has_handlers(n, AWRA_SIGNAL_CLICKED))
  {
    awra_emit_simple(n, AWRA_SIGNAL_CLICKED);
  }
  awra_widget_unref(n);
  return 1;
}

int awra_bk_move(BKNode *n, BKEvent *e)
{
  const AwraClass *c = AWRA_CLASS_OF(n);
  return c->motion ? c->motion(n, e) : 0;
}

int awra_bk_leave(BKNode *n)
{
  return 0;
}

int awra_bk_key(BKNode *n, BKEvent *e)
{
  const AwraClass *c = AWRA_CLASS_OF(n);
  return c->key ? c->key(n, e) : 0;
}

int awra_bk_scroll(BKNode *n, BKEvent *e)
{
  if (!awra_sensitive(n)) {
    return 0;
  }
  if (awra_has_handlers(n, AWRA_SIGNAL_SCROLL)) {
    AwraArgs args = {0};
    args.signal = AWRA_SIGNAL_SCROLL;
    fill_pointer_args(n, &args, e, 0);
    args.dx = e->wheel_x;
    args.dy = -e->wheel_y; /* GTK: positive dy scrolls down */
    if (awra_emit(n, &args)) {
      return 1;
    }
  }
  const AwraClass *c = AWRA_CLASS_OF(n);
  return c->scroll ? c->scroll(n, e) : 0;
}

int awra_bk_cursor_at(BKNode *n, int x, int y)
{
  const AwraAux *a = awra_aux(n);
  if (a->cursor) {
    return a->cursor - 1;
  }
  const AwraClass *c = AWRA_CLASS_OF(n);
  if (c->cursor) {
    const int r = c->cursor(n, x, y);
    if (r >= 0) {
      return r;
    }
  }
  return BK_CURSOR_DEFAULT;
}

/* ======================================================================= */
/* State: hover / press and their animations */

bool awra_hovered(const AwraWidget *w)
{
  return awra_is_widget(w) && awra_aux(w)->hovered && awra_sensitive(w);
}

bool awra_pressed(const AwraWidget *w)
{
  return awra_is_widget(w) && awra_aux(w)->pressed && awra_aux(w)->hovered;
}

bool awra_focus_visible(const AwraWidget *w)
{
  return awra_is_widget(w) && awra_aux(w)->focus_visible;
}

float awra_hover_t(const AwraWidget *w)
{
  return awra_is_widget(w) ? awra_aux(w)->hover_t : 0.0f;
}

float awra_press_t(const AwraWidget *w)
{
  return awra_is_widget(w) ? awra_aux(w)->press_t : 0.0f;
}

typedef struct Anim {
  AwraWidget *w;
  double start, ms;
  AwraAnimFunc fn;
  void *data;
  AwraDestroyNotify done;
  int state; /* 1: the hover / press state animation */
  float from_h, to_h, from_p, to_p;
  struct Anim *next;
} Anim;
static Anim *g_anims;

double awra_ease_out_cubic(double t)
{
  t = 1.0 - t;
  return 1.0 - t * t * t;
}

bool awra_animating(void)
{
  return g_anims != NULL;
}

static void anim_free(Anim *a, bool finished)
{
  if (a->done) {
    a->done(a->data);
  }
  (void)finished;
  free(a);
}

/* Advances every animation to now; returns whether any is still running. */
bool awra_anims_tick(void);
bool awra_anims_tick(void)
{
  const double now = awra_now_ms();
  Anim **p = &g_anims;
  while (*p) {
    Anim *a = *p;
    double t = a->ms > 0 ? (now - a->start) / a->ms : 1.0;
    const bool end = t >= 1.0;
    if (t > 1.0) {
      t = 1.0;
    }
    if (a->state) {
      AwraAux *x = awra_aux(a->w);
      const float e = (float)awra_ease_out_cubic(t);
      x->hover_t = a->from_h + (a->to_h - a->from_h) * e;
      x->press_t = a->from_p + (a->to_p - a->from_p) * e;
      awra_invalidate(a->w);
    }
    else if (a->fn) {
      a->fn(a->w, awra_ease_out_cubic(t), a->data);
    }
    if (end) {
      *p = a->next;
      anim_free(a, true);
    }
    else {
      p = &a->next;
    }
  }
  return g_anims != NULL;
}

AwraAnim *awra_anim_start(AwraWidget *w, unsigned ms, AwraAnimFunc fn, void *data,
                          AwraDestroyNotify done)
{
  if (awra_style_reduced_motion() || ms == 0) {
    if (fn) {
      fn(w, 1.0, data);
    }
    if (done) {
      done(data);
    }
    return NULL;
  }
  Anim *a = (Anim *)calloc(1, sizeof(Anim));
  if (!a) {
    return NULL;
  }
  a->w = w;
  a->start = awra_now_ms();
  a->ms = ms;
  a->fn = fn;
  a->data = data;
  a->done = done;
  a->next = g_anims;
  g_anims = a;
  if (fn) {
    fn(w, 0.0, data);
  }
  awra_windows_request_frame();
  return (AwraAnim *)a;
}

void awra_anim_stop_for(AwraWidget *w)
{
  Anim **p = &g_anims;
  while (*p) {
    Anim *a = *p;
    if (a->w == w) {
      *p = a->next;
      anim_free(a, false);
    }
    else {
      p = &a->next;
    }
  }
}

void awra_animate_state(AwraWidget *w)
{
  AwraAux *x = awra_aux(w);
  const float th = (x->hovered && awra_sensitive(w)) ? 1.0f : 0.0f;
  const float tp = (x->pressed && x->hovered) ? 1.0f : 0.0f;
  /* drop a running state animation of this widget */
  for (Anim **p = &g_anims; *p;) {
    Anim *a = *p;
    if (a->w == w && a->state) {
      *p = a->next;
      free(a);
    }
    else {
      p = &a->next;
    }
  }
  const unsigned ms = awra_motion_duration(AWRA_MOTION_FAST);
  if (ms == 0 || !x->mapped) {
    x->hover_t = th;
    x->press_t = tp;
    awra_invalidate(w);
    return;
  }
  Anim *a = (Anim *)calloc(1, sizeof(Anim));
  if (!a) {
    return;
  }
  a->w = w;
  a->start = awra_now_ms();
  /* the press goes down at once (CSS :active applies immediately) */
  a->ms = ms;
  a->state = 1;
  a->from_h = x->hover_t;
  a->to_h = th;
  a->from_p = tp > x->press_t ? tp : x->press_t;
  a->to_p = tp;
  x->press_t = a->from_p;
  a->next = g_anims;
  g_anims = a;
  awra_invalidate(w);
  awra_windows_request_frame();
}

void awra_widget_set_overflow_hidden(AwraWidget *w, bool hidden)
{
  if (awra_is_widget(w)) {
    awra_aux(w)->overflow_hidden = hidden;
    awra_invalidate(w);
  }
}

void awra_widget_set_draw_shift(AwraWidget *w, float dx, float dy)
{
  if (!awra_is_widget(w)) {
    return;
  }
  AwraAux *a = awra_aux(w);
  if (a->shift_x != dx || a->shift_y != dy) {
    a->shift_x = dx;
    a->shift_y = dy;
    awra_invalidate(w);
  }
}
