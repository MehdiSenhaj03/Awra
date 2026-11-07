/* BUI-Awra: loop timers that belong to no window (BK_loop_timer), shared by
 * every native host: the host asks for the next due time to bound its wait
 * and runs what is due after each wake.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "kit_intern.h"
#include "kit_loop.h"

#include <stdlib.h>

typedef struct LoopTimer {
  int id;
  double due; /* bk_time() seconds */
  void (*fn)(void *data);
  void *data;
} LoopTimer;

static LoopTimer *g_timers = NULL;
static int g_count = 0, g_cap = 0, g_next_id = 1;

int BK_loop_timer(int ms, void (*fn)(void *data), void *data)
{
  if (!fn) {
    return 0;
  }
  if (g_count == g_cap) {
    const int cap = g_cap ? g_cap * 2 : 16;
    LoopTimer *grown = (LoopTimer *)realloc(g_timers, sizeof(LoopTimer) * (size_t)cap);
    if (!grown) {
      return 0;
    }
    g_timers = grown;
    g_cap = cap;
  }
  LoopTimer *t = &g_timers[g_count++];
  t->id = g_next_id++;
  if (g_next_id <= 0) {
    g_next_id = 1;
  }
  t->due = bk_time() + (ms > 0 ? ms : 0) / 1000.0;
  t->fn = fn;
  t->data = data;
  bk_loop_wake_host();
  return t->id;
}

void BK_loop_cancel_timer(int id)
{
  for (int i = 0; i < g_count; i++) {
    if (g_timers[i].id == id) {
      g_timers[i] = g_timers[--g_count];
      return;
    }
  }
}

double bk_loop_next_due(void)
{
  double due = 0.0;
  for (int i = 0; i < g_count; i++) {
    if (due == 0.0 || g_timers[i].due < due) {
      due = g_timers[i].due;
    }
  }
  return due;
}

int bk_loop_timeout_ms(int timeout_ms)
{
  const double due = bk_loop_next_due();
  if (due == 0.0) {
    return timeout_ms;
  }
  double ms = (due - bk_time()) * 1000.0;
  int t = ms <= 0.0 ? 0 : (int)(ms + 0.999);
  return timeout_ms < 0 || t < timeout_ms ? t : timeout_ms;
}

int bk_loop_run_due(void)
{
  int ran = 0;
  const double now = bk_time();
  /* a callback may add or cancel timers: take one due timer at a time */
  for (;;) {
    int found = -1;
    for (int i = 0; i < g_count; i++) {
      if (g_timers[i].due <= now && (found < 0 || g_timers[i].due < g_timers[found].due)) {
        found = i;
      }
    }
    if (found < 0) {
      break;
    }
    LoopTimer t = g_timers[found];
    g_timers[found] = g_timers[--g_count];
    t.fn(t.data);
    ran++;
  }
  return ran;
}

/* -- watched file descriptors ------------------------------------------------ */

typedef struct LoopFd {
  int fd;
  void (*fn)(int fd, void *data);
  void *data;
} LoopFd;

static LoopFd g_fds[8];
static int g_nfds = 0;

int BK_loop_watch_fd(int fd, void (*fn)(int fd, void *data), void *data)
{
  if (fd < 0 || !fn) {
    return 0;
  }
  for (int i = 0; i < g_nfds; i++) {
    if (g_fds[i].fd == fd) {
      g_fds[i].fn = fn;
      g_fds[i].data = data;
      return 1;
    }
  }
  if (g_nfds == (int)(sizeof(g_fds) / sizeof(g_fds[0]))) {
    return 0;
  }
  g_fds[g_nfds].fd = fd;
  g_fds[g_nfds].fn = fn;
  g_fds[g_nfds].data = data;
  g_nfds++;
  bk_loop_wake_host(); /* the host's wait starts watching it */
  return 1;
}

void BK_loop_unwatch_fd(int fd)
{
  for (int i = 0; i < g_nfds; i++) {
    if (g_fds[i].fd == fd) {
      g_fds[i] = g_fds[--g_nfds];
      return;
    }
  }
}

int bk_loop_fd_count(void)
{
  return g_nfds;
}

int bk_loop_fd(int index)
{
  return index >= 0 && index < g_nfds ? g_fds[index].fd : -1;
}

void bk_loop_fd_ready(int fd)
{
  for (int i = 0; i < g_nfds; i++) {
    if (g_fds[i].fd == fd) {
      LoopFd f = g_fds[i];
      f.fn(f.fd, f.data); /* may unwatch it */
      return;
    }
  }
}
