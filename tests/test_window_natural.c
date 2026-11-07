/* A window left without a default height takes its contents' natural one
 * when it maps, as GtkWindow computes it at its first layout: what the
 * caller adds right after present counts (a dialog filled once shown).
 * Needs a display (X11 or Wayland); without one it passes untested.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

static void pump_for(int ms)
{
  struct timespec start, now;
  clock_gettime(CLOCK_MONOTONIC, &start);
  do {
    awra_pump();
    struct timespec nap = {0, 5 * 1000 * 1000};
    nanosleep(&nap, NULL);
    clock_gettime(CLOCK_MONOTONIC, &now);
  } while ((now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000 < ms);
}

static AwraWidget *rows(int n)
{
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 8);
  for (int i = 0; i < n; i++) {
    awra_box_append(box, awra_label_new("A line of the dialog's body"));
  }
  return box;
}

int main(void)
{
  if (!getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY")) {
    printf("test_window_natural: no display, skipped\n");
    return 0;
  }
  awra_init("org.awra.TestWindowNatural");

  /* filled after present: as tall as its twenty rows */
  AwraWidget *parent = awra_window_new("parent", 640, 480);
  awra_window_present(parent);
  AwraWidget *dialog = awra_dialog_new("dialog");
  awra_window_set_transient_for(dialog, parent);
  awra_window_set_default_size(dialog, 380, -1);
  awra_window_present(dialog);
  AwraWidget *body = rows(20);
  awra_dialog_set_child(dialog, body);
  pump_for(300);
  int nat = 0, min = 0;
  awra_measure(body, AWRA_ORIENTATION_VERTICAL, 380, &min, &nat);
  int w = 0, h = 0;
  awra_window_get_size(dialog, &w, &h);
  CHECK(w == 380, "the set width stays (%d)", w);
  CHECK(h > nat / awra_scale(), "the dialog holds the body added after present (%d, body %d)", h,
        nat);

  /* a width and height given: no natural size, whatever comes after */
  AwraWidget *fixed = awra_window_new("fixed", 300, 200);
  awra_window_present(fixed);
  awra_window_set_child(fixed, rows(40));
  pump_for(300);
  int fw = 0, fh = 0;
  awra_window_get_size(fixed, &fw, &fh);
  CHECK(fw == 300 && fh == 200, "a given size stays (%dx%d)", fw, fh);

  /* hidden before it maps, then shown: still measured, once */
  AwraWidget *later = awra_dialog_new("later");
  awra_window_set_transient_for(later, parent);
  awra_window_set_default_size(later, 320, -1);
  awra_window_present(later);
  awra_window_hide(later);
  awra_dialog_set_child(later, rows(10));
  pump_for(100);
  awra_window_present(later);
  pump_for(300);
  int h10 = 0;
  awra_window_get_size(later, &w, &h10);
  CHECK(h10 > 10 * 14 && h10 < h, "shown again, it measures its ten rows (%d, twenty %d)", h10, h);

  /* not resizable (a dialog, as Awra 1's AwraDialog): it follows its
   * contents, as GtkWindow recomputes such a window's size at each layout */
  AwraWidget *grows = awra_dialog_new("grows");
  awra_window_set_transient_for(grows, parent);
  awra_window_set_default_size(grows, 360, -1);
  AwraWidget *list = rows(4);
  awra_dialog_set_child(grows, list);
  awra_window_present(grows);
  pump_for(300);
  int h4 = 0, h12 = 0, gw = 0;
  awra_window_get_size(grows, &gw, &h4);
  for (int i = 0; i < 8; i++) {
    awra_box_append(list, awra_label_new("A line that came later"));
  }
  pump_for(300);
  awra_window_get_size(grows, &gw, &h12);
  CHECK(gw == 360, "its set width stays (%d)", gw);
  CHECK(h12 > h4 + 8 * 14, "it grows with its body (%d, then %d)", h4, h12);
  /* a resizable window keeps the size it took */
  AwraWidget *keeps = awra_window_new("keeps", 360, -1);
  AwraWidget *klist = rows(4);
  awra_window_set_child(keeps, klist);
  awra_window_present(keeps);
  pump_for(300);
  int k4 = 0, k12 = 0;
  awra_window_get_size(keeps, &gw, &k4);
  for (int i = 0; i < 8; i++) {
    awra_box_append(klist, awra_label_new("A line that came later"));
  }
  pump_for(300);
  awra_window_get_size(keeps, &gw, &k12);
  CHECK(k12 == k4, "a resizable window keeps its size (%d, then %d)", k4, k12);

  awra_window_close(keeps);
  awra_window_close(grows);
  awra_window_close(later);
  awra_window_close(fixed);
  awra_window_close(dialog);
  awra_window_close(parent);
  pump_for(50);
  printf("test_window_natural: %s\n", failures ? "FAILED" : "ok");
  return failures ? 1 : 0;
}
