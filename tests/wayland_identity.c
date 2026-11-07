/* Window identity on Wayland, against a compositor of our own: a minimal
 * Wayland server (libwayland-server) in a thread that offers what Awra
 * needs plus xdg-activation-v1 and xdg-toplevel-icon-v1, answers with
 * configures, frame callbacks and activation tokens, and records every
 * request. Awra reaches it through WAYLAND_SOCKET. Checked:
 *
 *  - a themed icon name goes by its name alone (xdg_toplevel_icon_v1
 *    set_name, no buffers), set on the toplevel;
 *  - an icon only Awra has (a registered SVG) goes as images
 *    (add_buffer for each size);
 *  - awra_window_set_startup_id: xdg_activation_v1.activate with that
 *    token and the window's surface when it is presented;
 *  - XDG_ACTIVATION_TOKEN from the environment activates the first window
 *    presented, and is then removed from the environment;
 *  - unsetting the icon sets none.
 *
 * Built where libwayland-server is (Linux); no display needed.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#define _GNU_SOURCE
#include <awra/awra.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

/* the protocols' interfaces, from the kit's generated code */
extern const struct wl_interface xdg_wm_base_interface;
extern const struct wl_interface xdg_activation_v1_interface;
extern const struct wl_interface xdg_toplevel_icon_manager_v1_interface;

static int failures;
#define CHECK(cond, ...)                                                   \
  do {                                                                     \
    if (!(cond)) {                                                         \
      failures++;                                                          \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);                 \
      fprintf(stderr, __VA_ARGS__);                                        \
      fputc('\n', stderr);                                                 \
    }                                                                      \
  } while (0)

/* -- the compositor ------------------------------------------------------ */

static struct wl_display *g_server;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile int g_stop;
/* what it saw (under g_lock) */
static char g_log[16384];
static uint32_t g_serial = 1;

static void log_line(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void log_line(const char *fmt, ...)
{
  pthread_mutex_lock(&g_lock);
  const size_t n = strlen(g_log);
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(g_log + n, sizeof(g_log) - n, fmt, ap);
  va_end(ap);
  pthread_mutex_unlock(&g_lock);
}

static int dispatch(const void *impl, void *target, uint32_t opcode, const struct wl_message *msg,
                    union wl_argument *args);

static void resource_new(struct wl_client *client, const struct wl_interface *iface, int version,
                         uint32_t id);

static void on_destroy(struct wl_resource *r)
{
  (void)r;
}

/* Every request of every object: recorded; new objects made; the replies a
 * client waits for sent. */
static int dispatch(const void *impl, void *target, uint32_t opcode, const struct wl_message *msg,
                    union wl_argument *args)
{
  (void)impl;
  struct wl_resource *res = (struct wl_resource *)target;
  struct wl_client *client = wl_resource_get_client(res);
  const char *iface = wl_resource_get_class(res);
  char line[512];
  int k = snprintf(line, sizeof(line), "%s.%s", iface, msg->name);
  const char *sig = msg->signature;
  int arg = 0;
  struct wl_resource *made[4] = {NULL};
  int n_made = 0;
  for (const char *p = sig; *p; p++) {
    if ((*p >= '0' && *p <= '9') || *p == '?') {
      continue;
    }
    switch (*p) {
      case 's':
        k += snprintf(line + k, sizeof(line) - (size_t)k, " \"%s\"", args[arg].s ? args[arg].s : "");
        break;
      case 'i':
        k += snprintf(line + k, sizeof(line) - (size_t)k, " %d", args[arg].i);
        break;
      case 'u':
        k += snprintf(line + k, sizeof(line) - (size_t)k, " %u", args[arg].u);
        break;
      case 'o': {
        struct wl_resource *o = (struct wl_resource *)args[arg].o;
        k += snprintf(line + k, sizeof(line) - (size_t)k, " %s", o ? wl_resource_get_class(o) : "null");
        break;
      }
      case 'n': {
        const struct wl_interface *ni = msg->types[arg];
        if (ni) {
          resource_new(client, ni, wl_resource_get_version(res), args[arg].n);
          struct wl_resource *nr = wl_client_get_object(client, args[arg].n);
          if (nr && n_made < 4) {
            made[n_made++] = nr;
          }
        }
        break;
      }
      case 'h':
        close(args[arg].h); /* shm pool fds: not mapped here */
        break;
      default:
        break;
    }
    if (k >= (int)sizeof(line)) {
      k = (int)sizeof(line) - 1;
    }
    arg++;
  }
  log_line("%s\n", line);
  /* the replies Awra waits for */
  if (!strcmp(iface, "wl_surface") && !strcmp(msg->name, "frame") && n_made) {
    wl_callback_send_done(made[0], 0);
    wl_resource_destroy(made[0]);
  }
  else if (!strcmp(iface, "xdg_surface") && !strcmp(msg->name, "get_toplevel") && n_made) {
    struct wl_array states;
    wl_array_init(&states);
    wl_resource_post_event(made[0], 0 /* configure */, 400, 300, &states);
    wl_array_release(&states);
    wl_resource_post_event(res, 0 /* xdg_surface.configure */, g_serial++);
  }
  else if (!strcmp(iface, "xdg_activation_token_v1") && !strcmp(msg->name, "commit")) {
    wl_resource_post_event(res, 0 /* done */, "fake-token");
  }
  else if (!strcmp(iface, "xdg_wm_base") && !strcmp(msg->name, "pong")) {
    /* nothing */
  }
  if (!strcmp(msg->name, "destroy") || !strcmp(msg->name, "release")) {
    wl_resource_destroy(res);
  }
  return 0;
}

static void resource_new(struct wl_client *client, const struct wl_interface *iface, int version,
                         uint32_t id)
{
  if (version > iface->version) {
    version = iface->version;
  }
  struct wl_resource *r = wl_resource_create(client, iface, version, id);
  if (!r) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_dispatcher(r, dispatch, NULL, NULL, on_destroy);
}

static void bind_global(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
  const struct wl_interface *iface = (const struct wl_interface *)data;
  resource_new(client, iface, (int)version, id);
  struct wl_resource *r = wl_client_get_object(client, id);
  if (r && iface == &wl_shm_interface) {
    wl_shm_send_format(r, WL_SHM_FORMAT_ARGB8888);
    wl_shm_send_format(r, WL_SHM_FORMAT_XRGB8888);
  }
  else if (r && iface == &wl_seat_interface) {
    wl_seat_send_capabilities(r, 0);
  }
  else if (r && iface == &xdg_toplevel_icon_manager_v1_interface) {
    for (int s = 16; s <= 128; s *= 2) {
      wl_resource_post_event(r, 0 /* icon_size */, s);
    }
    wl_resource_post_event(r, 1 /* done */);
  }
}

static void *server_thread(void *data)
{
  (void)data;
  struct wl_event_loop *loop = wl_display_get_event_loop(g_server);
  while (!g_stop) {
    wl_event_loop_dispatch(loop, 20);
    wl_display_flush_clients(g_server);
  }
  return NULL;
}

/* -- the test ----------------------------------------------------------- */

static bool log_has(const char *needle)
{
  pthread_mutex_lock(&g_lock);
  const bool has = strstr(g_log, needle) != NULL;
  pthread_mutex_unlock(&g_lock);
  return has;
}

static int log_count(const char *needle)
{
  int n = 0;
  pthread_mutex_lock(&g_lock);
  for (const char *p = g_log; (p = strstr(p, needle)); p++) {
    n++;
  }
  pthread_mutex_unlock(&g_lock);
  return n;
}

static void log_clear(void)
{
  pthread_mutex_lock(&g_lock);
  g_log[0] = 0;
  pthread_mutex_unlock(&g_lock);
}

/* pumps Awra's loop until `needle` was seen (or 3 s) */
static bool wait_for(const char *needle)
{
  for (int i = 0; i < 300; i++) {
    awra_pump();
    if (log_has(needle)) {
      return true;
    }
    usleep(10000);
  }
  return false;
}

int main(void)
{
  /* a private icon theme: one themed icon (only its presence counts) */
  char tmpl[] = "/tmp/awra-wl-XXXXXX";
  char *root = mkdtemp(tmpl);
  if (!root) {
    return 1;
  }
  char path[512];
  snprintf(path, sizeof(path), "%s/icons/hicolor/48x48/apps", root);
  char cmd[1200];
  snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && : > '%s/org.awra.Themed.png'", path, path);
  if (system(cmd) != 0) {
    return 1;
  }
  setenv("XDG_DATA_DIRS", root, 1);
  setenv("XDG_DATA_HOME", root, 1);
  setenv("XDG_ACTIVATION_TOKEN", "env-token", 1);
  unsetenv("DISPLAY");
  unsetenv("WAYLAND_DISPLAY");
  setenv("BUI_BACKEND", "wayland", 1);

  /* the compositor, over a socket pair */
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0) {
    return 1;
  }
  g_server = wl_display_create();
  wl_global_create(g_server, &wl_compositor_interface, 6, (void *)&wl_compositor_interface, bind_global);
  wl_global_create(g_server, &wl_shm_interface, 1, (void *)&wl_shm_interface, bind_global);
  wl_global_create(g_server, &wl_seat_interface, 7, (void *)&wl_seat_interface, bind_global);
  wl_global_create(g_server, &xdg_wm_base_interface, 5, (void *)&xdg_wm_base_interface, bind_global);
  wl_global_create(g_server, &xdg_activation_v1_interface, 1, (void *)&xdg_activation_v1_interface,
                   bind_global);
  wl_global_create(g_server, &xdg_toplevel_icon_manager_v1_interface, 1,
                   (void *)&xdg_toplevel_icon_manager_v1_interface, bind_global);
  if (!wl_client_create(g_server, sv[0])) {
    return 1;
  }
  /* WAYLAND_SOCKET: the client's end, not close-on-exec */
  const int cfd = dup(sv[1]);
  close(sv[1]);
  char num[16];
  snprintf(num, sizeof(num), "%d", cfd);
  setenv("WAYLAND_SOCKET", num, 1);
  pthread_t th;
  pthread_create(&th, NULL, server_thread, NULL);

  awra_init("org.awra.TestWaylandIdentity");

  /* a themed name: by name, no images; the environment's token */
  AwraWidget *a = awra_window_new("A", 200, 100);
  awra_window_set_icon_name(a, "org.awra.Themed");
  awra_window_present(a);
  CHECK(wait_for("xdg_toplevel_icon_manager_v1.set_icon xdg_toplevel xdg_toplevel_icon_v1"),
        "no icon set on the toplevel");
  CHECK(log_has("xdg_toplevel_icon_v1.set_name \"org.awra.Themed\""), "themed icon not by name");
  CHECK(log_count("xdg_toplevel_icon_v1.add_buffer") == 0, "themed icon sent as images");
  CHECK(wait_for("xdg_activation_v1.activate \"env-token\" wl_surface"), "XDG_ACTIVATION_TOKEN not used");
  CHECK(getenv("XDG_ACTIVATION_TOKEN") == NULL, "XDG_ACTIVATION_TOKEN left in the environment");

  /* Awra's own icon (a registered SVG): images; a startup ID */
  log_clear();
  AwraWidget *b = awra_window_new("B", 200, 100);
  awra_window_set_icon_name(b, "audio-card-symbolic");
  awra_window_set_startup_id(b, "startup-1234");
  awra_window_present(b);
  CHECK(wait_for("xdg_activation_v1.activate \"startup-1234\" wl_surface"), "startup ID not activated");
  CHECK(wait_for("xdg_toplevel_icon_manager_v1.set_icon xdg_toplevel xdg_toplevel_icon_v1"),
        "no icon for B");
  CHECK(log_count("xdg_toplevel_icon_v1.add_buffer") >= 4, "images sent: %d",
        log_count("xdg_toplevel_icon_v1.add_buffer"));
  CHECK(!log_has("activate \"env-token\""), "the environment's token used twice");

  /* no icon: none set */
  log_clear();
  awra_window_set_icon_name(b, NULL);
  CHECK(wait_for("xdg_toplevel_icon_manager_v1.set_icon xdg_toplevel null"), "icon not unset");

  awra_widget_destroy(a);
  awra_widget_destroy(b);
  awra_pump();
  g_stop = 1;
  pthread_join(th, NULL);
  wl_display_destroy_clients(g_server);
  wl_display_destroy(g_server);
  snprintf(cmd, sizeof(cmd), "rm -rf '%s'", root);
  if (system(cmd) != 0) {
    fprintf(stderr, "could not remove %s\n", root);
  }
  if (failures) {
    fprintf(stderr, "%d failure(s); requests seen:\n%s", failures, g_log);
    return 1;
  }
  printf("wayland identity ok\n");
  return 0;
}
