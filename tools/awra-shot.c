/* Renders a showcase page offscreen to a PNG (visual review, CI, benchmarks).
 * usage: awra-shot <page> <out.png> [width height scale]
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include "../showcase/pages.h"

int main(int argc, char **argv)
{
  if (argc < 3) {
    fprintf(stderr, "usage: %s page out.png [w h scale]\n", argv[0]);
    return 2;
  }
  const int w = argc > 3 ? atoi(argv[3]) : 1100, h = argc > 4 ? atoi(argv[4]) : 760;
  const double s = argc > 5 ? atof(argv[5]) : 1.0;
  awra_init("org.awra.Shot");
  AwraWidget *win = showcase_window(argv[1], true, w, h, s);
  const double t0 = awra_now_ms();
  awra_window_render_offscreen(win, true);
  const double t1 = awra_now_ms();
  if (!awra_window_snapshot_png(win, argv[2])) {
    return 1;
  }
  fprintf(stderr, "first frame %.2f ms\n", t1 - t0);
  return 0;
}
