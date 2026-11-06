/* The Awra showcase: every component, live.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include "pages.h"

int main(int argc, char **argv)
{
  awra_init("org.awra.Showcase");
  const char *page = getenv("AWRA_SHOWCASE_PAGE");
  AwraWidget *win = showcase_window(page ? page : "overview", false, 1100, 760, 1.0);
  awra_window_present(win);
  awra_run();
  return 0;
}
