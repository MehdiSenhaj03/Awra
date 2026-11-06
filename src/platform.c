/* Awra 2 platform services: native file dialogs and opening URIs.
 *
 * Linux: the desktop's file chooser through xdg-desktop-portal
 * (org.freedesktop.portal.FileChooser), spoken over libdbus loaded at run
 * time (no build dependency), on a worker thread; zenity / kdialog when no
 * portal answers. Windows: the common dialogs.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include <stdio.h>

typedef struct DialogJob {
  char *title;
  bool multiple, folder, save;
  char *initial_name;
  AwraFileFilter *filters;
  int nfilters;
  AwraFileCallback done;
  void *data;
  char *result; /* '\n'-separated paths, NULL cancelled */
} DialogJob;

static void job_free(DialogJob *j)
{
  for (int i = 0; i < j->nfilters; i++) {
    free((char *)j->filters[i].name);
    free((char *)j->filters[i].patterns);
  }
  free(j->filters);
  free(j->title);
  free(j->initial_name);
  free(j->result);
  free(j);
}

static void job_deliver(void *data)
{
  DialogJob *j = (DialogJob *)data;
  if (j->done) {
    j->done(j->result, j->data);
  }
  job_free(j);
}

#ifdef _WIN32
/* ======================================================================= */
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <commdlg.h>
#  include <shellapi.h>
#  include <shlobj.h>

static char *to_utf8(const wchar_t *w)
{
  const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
  char *s = (char *)malloc((size_t)n);
  WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
  return s;
}

static wchar_t *to_wide(const char *s)
{
  const int n = MultiByteToWideChar(CP_UTF8, 0, s ? s : "", -1, NULL, 0);
  wchar_t *w = (wchar_t *)malloc(sizeof(wchar_t) * (size_t)n);
  MultiByteToWideChar(CP_UTF8, 0, s ? s : "", -1, w, n);
  return w;
}

static void run_dialog(DialogJob *j, HWND owner)
{
  if (j->folder) {
    BROWSEINFOW bi;
    memset(&bi, 0, sizeof(bi));
    wchar_t *title = to_wide(j->title);
    bi.hwndOwner = owner;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    free(title);
    if (pidl) {
      wchar_t path[MAX_PATH];
      if (SHGetPathFromIDListW(pidl, path)) {
        j->result = to_utf8(path);
      }
      CoTaskMemFree(pidl);
    }
    return;
  }
  /* filters: "Name\0*.a;*.b\0...\0\0" */
  size_t flen = 2;
  for (int i = 0; i < j->nfilters; i++) {
    flen += strlen(j->filters[i].name) + strlen(j->filters[i].patterns) + 2;
  }
  char *f = (char *)calloc(flen + 16, 1);
  size_t o = 0;
  for (int i = 0; i < j->nfilters; i++) {
    o += (size_t)sprintf(f + o, "%s", j->filters[i].name) + 1;
    o += (size_t)sprintf(f + o, "%s", j->filters[i].patterns) + 1;
  }
  wchar_t *wf = (wchar_t *)calloc(flen + 16, sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, f, (int)o + 1, wf, (int)flen + 16);
  free(f);
  wchar_t *title = to_wide(j->title);
  const DWORD cap = 32768;
  wchar_t *buf = (wchar_t *)calloc(cap, sizeof(wchar_t));
  OPENFILENAMEW ofn;
  memset(&ofn, 0, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;
  ofn.lpstrFilter = j->nfilters ? wf : NULL;
  ofn.lpstrFile = buf;
  ofn.nMaxFile = cap;
  ofn.lpstrTitle = title;
  if (j->save) {
    if (j->initial_name) {
      wchar_t *name = to_wide(j->initial_name);
      wcsncpy(buf, name, cap - 1);
      free(name);
    }
    ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (GetSaveFileNameW(&ofn)) {
      j->result = to_utf8(buf);
    }
    free(buf);
    free(title);
    free(wf);
    return;
  }
  ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
              (j->multiple ? OFN_ALLOWMULTISELECT : 0);
  if (GetOpenFileNameW(&ofn)) {
    /* one path, or a folder then names, NUL-separated */
    const wchar_t *first = buf;
    const wchar_t *next = first + wcslen(first) + 1;
    if (!*next) {
      j->result = to_utf8(first);
    }
    else {
      char *dir = to_utf8(first);
      size_t total = 1;
      char *out = (char *)calloc(1, 1);
      for (; *next; next += wcslen(next) + 1) {
        char *name = to_utf8(next);
        const size_t add = strlen(dir) + strlen(name) + 3;
        out = (char *)realloc(out, total + add);
        total += (size_t)sprintf(out + total - 1, "%s%s\\%s", total > 1 ? "\n" : "", dir, name);
        free(name);
      }
      free(dir);
      j->result = out;
    }
  }
  free(buf);
  free(title);
  free(wf);
}

bool awra_show_uri(const char *uri)
{
  wchar_t *w = to_wide(uri);
  const HINSTANCE r = ShellExecuteW(NULL, L"open", w, NULL, NULL, SW_SHOWNORMAL);
  free(w);
  return (INT_PTR)r > 32;
}

#else
/* ======================================================================= */
#  include <dlfcn.h>
#  include <pthread.h>
#  include <signal.h>
#  include <sys/wait.h>
#  include <unistd.h>

/* the part of libdbus' ABI used here */
typedef struct DBusError_ {
  const char *name, *message;
  unsigned d1 : 1, d2 : 1, d3 : 1, d4 : 1, d5 : 1;
  void *padding1;
} DBusError_;
typedef struct DBusMessageIter_ {
  void *d1, *d2;
  unsigned u3, u4, u5, u6;
  int u7, u8, u9, u10, u11, pad1;
  void *pad2, *pad3;
} DBusMessageIter_;
enum {
  T_STRING = 's', T_OBJECT_PATH = 'o', T_BOOLEAN = 'b', T_UINT32 = 'u', T_ARRAY = 'a',
  T_VARIANT = 'v', T_DICT_ENTRY = 'e', T_STRUCT = 'r', T_INVALID = 0
};

static struct {
  int state;
  void (*error_init)(DBusError_ *);
  void (*error_free)(DBusError_ *);
  void *(*bus_get_private)(int, DBusError_ *);
  const char *(*bus_get_unique_name)(void *);
  void (*bus_add_match)(void *, const char *, DBusError_ *);
  void *(*message_new_method_call)(const char *, const char *, const char *, const char *);
  void (*message_unref)(void *);
  void (*message_iter_init_append)(void *, DBusMessageIter_ *);
  int (*message_iter_append_basic)(DBusMessageIter_ *, int, const void *);
  int (*message_iter_open_container)(DBusMessageIter_ *, int, const char *, DBusMessageIter_ *);
  int (*message_iter_close_container)(DBusMessageIter_ *, DBusMessageIter_ *);
  void *(*connection_send_with_reply_and_block)(void *, void *, int, DBusError_ *);
  int (*connection_read_write)(void *, int);
  void *(*connection_pop_message)(void *);
  void (*connection_close)(void *);
  void (*connection_unref)(void *);
  int (*message_is_signal)(void *, const char *, const char *);
  const char *(*message_get_path)(void *);
  int (*message_iter_init)(void *, DBusMessageIter_ *);
  int (*message_iter_get_arg_type)(DBusMessageIter_ *);
  void (*message_iter_get_basic)(DBusMessageIter_ *, void *);
  int (*message_iter_next)(DBusMessageIter_ *);
  void (*message_iter_recurse)(DBusMessageIter_ *, DBusMessageIter_ *);
} D;

static int dbus_load(void)
{
  if (D.state) {
    return D.state > 0;
  }
  D.state = -1;
  void *lib = dlopen("libdbus-1.so.3", RTLD_NOW | RTLD_LOCAL);
  if (!lib) {
    return 0;
  }
#  define SYM(f, n)                              \
    if (!(*(void **)&D.f = dlsym(lib, n))) {     \
      return 0;                                  \
    }
  SYM(error_init, "dbus_error_init")
  SYM(error_free, "dbus_error_free")
  SYM(bus_get_private, "dbus_bus_get_private")
  SYM(bus_get_unique_name, "dbus_bus_get_unique_name")
  SYM(bus_add_match, "dbus_bus_add_match")
  SYM(message_new_method_call, "dbus_message_new_method_call")
  SYM(message_unref, "dbus_message_unref")
  SYM(message_iter_init_append, "dbus_message_iter_init_append")
  SYM(message_iter_append_basic, "dbus_message_iter_append_basic")
  SYM(message_iter_open_container, "dbus_message_iter_open_container")
  SYM(message_iter_close_container, "dbus_message_iter_close_container")
  SYM(connection_send_with_reply_and_block, "dbus_connection_send_with_reply_and_block")
  SYM(connection_read_write, "dbus_connection_read_write")
  SYM(connection_pop_message, "dbus_connection_pop_message")
  SYM(connection_close, "dbus_connection_close")
  SYM(connection_unref, "dbus_connection_unref")
  SYM(message_is_signal, "dbus_message_is_signal")
  SYM(message_get_path, "dbus_message_get_path")
  SYM(message_iter_init, "dbus_message_iter_init")
  SYM(message_iter_get_arg_type, "dbus_message_iter_get_arg_type")
  SYM(message_iter_get_basic, "dbus_message_iter_get_basic")
  SYM(message_iter_next, "dbus_message_iter_next")
  SYM(message_iter_recurse, "dbus_message_iter_recurse")
#  undef SYM
  D.state = 1;
  return 1;
}

static void dict_bool(DBusMessageIter_ *dict, const char *key, int value)
{
  DBusMessageIter_ e, v;
  D.message_iter_open_container(dict, T_DICT_ENTRY, NULL, &e);
  D.message_iter_append_basic(&e, T_STRING, &key);
  D.message_iter_open_container(&e, T_VARIANT, "b", &v);
  D.message_iter_append_basic(&v, T_BOOLEAN, &value);
  D.message_iter_close_container(&e, &v);
  D.message_iter_close_container(dict, &e);
}

static void dict_string(DBusMessageIter_ *dict, const char *key, const char *value)
{
  DBusMessageIter_ e, v;
  D.message_iter_open_container(dict, T_DICT_ENTRY, NULL, &e);
  D.message_iter_append_basic(&e, T_STRING, &key);
  D.message_iter_open_container(&e, T_VARIANT, "s", &v);
  D.message_iter_append_basic(&v, T_STRING, &value);
  D.message_iter_close_container(&e, &v);
  D.message_iter_close_container(dict, &e);
}

/* filters: a(sa(us)) — each pattern "*.mp3" as glob (0) */
static void dict_filters(DBusMessageIter_ *dict, const DialogJob *j)
{
  if (!j->nfilters) {
    return;
  }
  DBusMessageIter_ e, v, arr, st, pats, pat;
  const char *key = "filters";
  D.message_iter_open_container(dict, T_DICT_ENTRY, NULL, &e);
  D.message_iter_append_basic(&e, T_STRING, &key);
  D.message_iter_open_container(&e, T_VARIANT, "a(sa(us))", &v);
  D.message_iter_open_container(&v, T_ARRAY, "(sa(us))", &arr);
  for (int i = 0; i < j->nfilters; i++) {
    D.message_iter_open_container(&arr, T_STRUCT, NULL, &st);
    const char *name = j->filters[i].name;
    D.message_iter_append_basic(&st, T_STRING, &name);
    D.message_iter_open_container(&st, T_ARRAY, "(us)", &pats);
    char *copy = awra_strdup(j->filters[i].patterns);
    for (char *tok = strtok(copy, ";"); tok; tok = strtok(NULL, ";")) {
      while (*tok == ' ') {
        tok++;
      }
      D.message_iter_open_container(&pats, T_STRUCT, NULL, &pat);
      const unsigned zero = 0;
      const char *p = tok;
      D.message_iter_append_basic(&pat, T_UINT32, &zero);
      D.message_iter_append_basic(&pat, T_STRING, &p);
      D.message_iter_close_container(&pats, &pat);
    }
    free(copy);
    D.message_iter_close_container(&st, &pats);
    D.message_iter_close_container(&arr, &st);
  }
  D.message_iter_close_container(&v, &arr);
  D.message_iter_close_container(&e, &v);
  D.message_iter_close_container(dict, &e);
}

static char *uri_to_path(const char *uri)
{
  const char *s = !strncmp(uri, "file://", 7) ? uri + 7 : uri;
  char *out = (char *)malloc(strlen(s) + 1);
  size_t o = 0;
  for (; *s; s++) {
    if (*s == '%' && s[1] && s[2]) {
      char hex[3] = {s[1], s[2], 0};
      out[o++] = (char)strtol(hex, NULL, 16);
      s += 2;
    }
    else {
      out[o++] = *s;
    }
  }
  out[o] = 0;
  return out;
}

/* 1 answered (j->result set or cancelled), 0 no portal */
static int portal_dialog(DialogJob *j)
{
  if (!dbus_load()) {
    return 0;
  }
  DBusError_ err;
  D.error_init(&err);
  void *c = D.bus_get_private(0 /* DBUS_BUS_SESSION */, &err);
  if (!c) {
    D.error_free(&err);
    return 0;
  }
  char token[32], sender[128], handle[256];
  snprintf(token, sizeof(token), "awra%u", (unsigned)(awra_now_ms()));
  const char *unique = D.bus_get_unique_name(c);
  size_t k = 0;
  for (const char *p = unique + (unique[0] == ':' ? 1 : 0); *p && k < sizeof(sender) - 1; p++) {
    sender[k++] = *p == '.' ? '_' : *p;
  }
  sender[k] = 0;
  snprintf(handle, sizeof(handle), "/org/freedesktop/portal/desktop/request/%s/%s", sender, token);
  char match[512];
  snprintf(match, sizeof(match),
           "type='signal',interface='org.freedesktop.portal.Request',member='Response',path='%s'",
           handle);
  D.bus_add_match(c, match, &err);
  void *msg = D.message_new_method_call("org.freedesktop.portal.Desktop",
                                        "/org/freedesktop/portal/desktop",
                                        "org.freedesktop.portal.FileChooser",
                                        j->save ? "SaveFile" : "OpenFile");
  DBusMessageIter_ it, dict;
  D.message_iter_init_append(msg, &it);
  const char *parent = "", *title = j->title ? j->title : "";
  D.message_iter_append_basic(&it, T_STRING, &parent);
  D.message_iter_append_basic(&it, T_STRING, &title);
  D.message_iter_open_container(&it, T_ARRAY, "{sv}", &dict);
  dict_string(&dict, "handle_token", token);
  if (j->save) {
    if (j->initial_name) {
      dict_string(&dict, "current_name", j->initial_name);
    }
  }
  else {
    dict_bool(&dict, "multiple", j->multiple);
    dict_bool(&dict, "directory", j->folder);
  }
  dict_filters(&dict, j);
  D.message_iter_close_container(&it, &dict);
  void *reply = D.connection_send_with_reply_and_block(c, msg, 5000, &err);
  D.message_unref(msg);
  if (!reply) {
    D.error_free(&err);
    D.connection_close(c);
    D.connection_unref(c);
    return 0;
  }
  D.message_unref(reply);
  /* wait for the Response (the user may take long) */
  int answered = 0;
  while (!answered && D.connection_read_write(c, 500)) {
    void *m;
    while ((m = D.connection_pop_message(c)) != NULL) {
      if (D.message_is_signal(m, "org.freedesktop.portal.Request", "Response") &&
          !strcmp(D.message_get_path(m), handle))
      {
        answered = 1;
        DBusMessageIter_ a;
        unsigned response = 2;
        if (D.message_iter_init(m, &a)) {
          D.message_iter_get_basic(&a, &response);
          D.message_iter_next(&a);
        }
        if (response == 0 && D.message_iter_get_arg_type(&a) == T_ARRAY) {
          DBusMessageIter_ entries;
          D.message_iter_recurse(&a, &entries);
          while (D.message_iter_get_arg_type(&entries) == T_DICT_ENTRY) {
            DBusMessageIter_ e, v, uris;
            D.message_iter_recurse(&entries, &e);
            const char *key = NULL;
            D.message_iter_get_basic(&e, &key);
            D.message_iter_next(&e);
            if (key && !strcmp(key, "uris")) {
              D.message_iter_recurse(&e, &v);
              D.message_iter_recurse(&v, &uris);
              size_t total = 0;
              char *out = NULL;
              while (D.message_iter_get_arg_type(&uris) == T_STRING) {
                const char *u = NULL;
                D.message_iter_get_basic(&uris, &u);
                char *path = uri_to_path(u);
                const size_t add = strlen(path) + 2;
                out = (char *)realloc(out, total + add);
                total += (size_t)sprintf(out + total, "%s%s", total ? "\n" : "", path);
                free(path);
                D.message_iter_next(&uris);
              }
              j->result = out;
            }
            D.message_iter_next(&entries);
          }
        }
      }
      D.message_unref(m);
    }
  }
  D.connection_close(c);
  D.connection_unref(c);
  return answered;
}

/* zenity / kdialog, when no portal is there */
static void spawn_dialog(DialogJob *j)
{
  int fds[2];
  if (pipe(fds) != 0) {
    return;
  }
  const pid_t pid = fork();
  if (pid == 0) {
    dup2(fds[1], 1);
    close(fds[0]);
    close(fds[1]);
    if (j->save) {
      char arg[1024];
      snprintf(arg, sizeof(arg), "--filename=%s", j->initial_name ? j->initial_name : "");
      execlp("zenity", "zenity", "--file-selection", "--save", "--confirm-overwrite", arg, "--title", j->title ? j->title : "", (char *)NULL);
      execlp("kdialog", "kdialog", "--getsavefilename", j->initial_name ? j->initial_name : ".", (char *)NULL);
    }
    else if (j->folder) {
      execlp("zenity", "zenity", "--file-selection", "--directory", "--title", j->title ? j->title : "", (char *)NULL);
      execlp("kdialog", "kdialog", "--getexistingdirectory", (char *)NULL);
    }
    else if (j->multiple) {
      execlp("zenity", "zenity", "--file-selection", "--multiple", "--separator=\n", "--title", j->title ? j->title : "", (char *)NULL);
      execlp("kdialog", "kdialog", "--getopenfilename", "--multiple", "--separate-output", (char *)NULL);
    }
    else {
      execlp("zenity", "zenity", "--file-selection", "--title", j->title ? j->title : "", (char *)NULL);
      execlp("kdialog", "kdialog", "--getopenfilename", (char *)NULL);
    }
    _exit(127);
  }
  close(fds[1]);
  if (pid < 0) {
    close(fds[0]);
    return;
  }
  size_t len = 0, cap = 4096;
  char *buf = (char *)malloc(cap + 1);
  ssize_t n;
  while (buf && (n = read(fds[0], buf + len, cap - len)) > 0) {
    len += (size_t)n;
    if (len == cap) {
      cap *= 2;
      buf = (char *)realloc(buf, cap + 1);
    }
  }
  close(fds[0]);
  int status = 0;
  waitpid(pid, &status, 0);
  if (buf) {
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
      len--;
    }
    buf[len] = 0;
  }
  if (buf && len > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    j->result = buf;
  }
  else {
    free(buf);
  }
}

static void *dialog_thread(void *data)
{
  DialogJob *j = (DialogJob *)data;
  if (!portal_dialog(j)) {
    spawn_dialog(j);
  }
  awra_invoke(job_deliver, j);
  return NULL;
}

bool awra_show_uri(const char *uri)
{
  const pid_t pid = fork();
  if (pid == 0) {
    setsid();
    execlp("xdg-open", "xdg-open", uri, (char *)NULL);
    _exit(127);
  }
  return pid > 0;
}
#endif

static void dialog_run(AwraWidget *parent, DialogJob *j);

void awra_file_dialog_save(AwraWidget *parent, const char *title, const char *initial_name,
                           const AwraFileFilter *filters, int n, AwraFileCallback done, void *data)
{
  DialogJob *j = (DialogJob *)calloc(1, sizeof(DialogJob));
  j->title = awra_strdup(title ? title : "");
  j->save = true;
  j->initial_name = initial_name ? awra_strdup(initial_name) : NULL;
  j->done = done;
  j->data = data;
  j->filters = (AwraFileFilter *)calloc((size_t)n + 1, sizeof(AwraFileFilter));
  j->nfilters = n;
  for (int i = 0; i < n; i++) {
    j->filters[i].name = awra_strdup(filters[i].name);
    j->filters[i].patterns = awra_strdup(filters[i].patterns);
  }
  dialog_run(parent, j);
}

void awra_file_dialog_open(AwraWidget *parent, const char *title, bool multiple, bool folder,
                           const AwraFileFilter *filters, int n, AwraFileCallback done, void *data)
{
  DialogJob *j = (DialogJob *)calloc(1, sizeof(DialogJob));
  j->title = awra_strdup(title ? title : "");
  j->multiple = multiple;
  j->folder = folder;
  j->done = done;
  j->data = data;
  j->filters = (AwraFileFilter *)calloc((size_t)n + 1, sizeof(AwraFileFilter));
  j->nfilters = n;
  for (int i = 0; i < n; i++) {
    j->filters[i].name = awra_strdup(filters[i].name);
    j->filters[i].patterns = awra_strdup(filters[i].patterns);
  }
  dialog_run(parent, j);
}

static void dialog_run(AwraWidget *parent, DialogJob *j)
{
#ifdef _WIN32
  HWND owner = parent ? (HWND)awra_window_native_handle(awra_window_of(parent) ? awra_window_of(parent) : parent) : NULL;
  run_dialog(j, owner);
  job_deliver(j);
#else
  (void)parent;
  pthread_t t;
  if (pthread_create(&t, NULL, dialog_thread, j) == 0) {
    pthread_detach(t);
  }
  else {
    job_deliver(j);
  }
#endif
}

char *awra_clipboard_get_text(AwraWidget *w)
{
  BKContext *ctx = w ? bk_ctx(w) : NULL;
  return ctx ? BK_clipboard_get(ctx) : NULL;
}

void awra_clipboard_set_text(AwraWidget *w, const char *text)
{
  BKContext *ctx = w ? bk_ctx(w) : NULL;
  if (ctx) {
    BK_clipboard_set(ctx, text ? text : "");
  }
}

void awra_free(void *p)
{
  free(p);
}
