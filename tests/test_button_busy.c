/* Awra 1's AwraButton:busy: a spinner over the content, the button keeping
 * its size, clicks swallowed until it is no longer busy.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include <stdio.h>

static int failures, clicks;
#define CHECK(cond, ...)                                                   \
  do {                                                                     \
    if (!(cond)) {                                                         \
      failures++;                                                          \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);                 \
      fprintf(stderr, __VA_ARGS__);                                        \
      fputc('\n', stderr);                                                 \
    }                                                                      \
  } while (0)

static void clicked(AwraWidget *w, const AwraArgs *a, void *data)
{
  clicks++;
}

/* a click at the button's centre */
static void click(AwraWidget *win, AwraWidget *button)
{
  BKContext *ctx = awra_ctx(win);
  const int x = button->x + button->w / 2, y = button->y + button->h / 2;
  BK_motion(ctx, x, y, 0);
  BK_press(ctx, x, y, 0);
  BK_release(ctx, x, y, 0);
  awra_pump();
}

int main(void)
{
  awra_init("org.awra.TestButtonBusy");
  AwraWidget *win = awra_window_new_offscreen("busy", 300, 120, 1.0);
  AwraWidget *button = awra_button_new_with_label("Proposer des ambiances");
  awra_widget_set_halign(button, AWRA_ALIGN_START);
  awra_widget_set_valign(button, AWRA_ALIGN_START);
  awra_signal_connect(button, AWRA_SIGNAL_CLICKED, clicked, NULL, NULL);
  awra_window_set_child(win, button);
  awra_window_render_offscreen(win, true);
  const int w0 = button->w, h0 = button->h;

  awra_button_set_busy(button, true);
  awra_pump();
  awra_window_render_offscreen(win, true);
  CHECK(awra_button_busy(button), "not busy");
  CHECK(button->w == w0 && button->h == h0, "busy changed its size: %dx%d -> %dx%d", w0, h0, button->w, button->h);
  click(win, button);
  CHECK(clicks == 0, "a busy button reported a click");

  awra_button_set_busy(button, false);
  awra_pump();
  awra_window_render_offscreen(win, true);
  click(win, button);
  CHECK(clicks == 1, "the button didn't click once no longer busy (%d)", clicks);
  CHECK(button->w == w0 && button->h == h0, "its size didn't come back");

  awra_widget_destroy(win);
  awra_pump();
  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  printf("button busy: ok\n");
  return 0;
}
