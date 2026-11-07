/* A window smaller than its contents need, as GTK lays it out: GtkWindow
 * gives its child its own size, through bins (overlays) to a stack, which
 * gives its page at least the page's minimum, centred (GtkStack's
 * adjust_child_allocation); a split view's minimum is GtkPaned's (the
 * shown sidebar's, the handle and the content's), and its content gets at
 * least its own; materials draw their start and end edges mirrored right to
 * left; Latin text takes the fonts of the language Pango derives for it
 * when the user's language doesn't write Latin.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include "GPU_raster.h"

#include <stdio.h>
#include <stdlib.h>

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

static AwraWidget *block(int w, int h)
{
  AwraWidget *b = awra_box_new(AWRA_ORIENTATION_VERTICAL, 0);
  awra_widget_set_size_request(b, w, h);
  return b;
}

/* overlay > stack > page of `page_w` in a window 400 wide */
static void check_stack(bool rtl, int page_w, int want_x)
{
  AwraWidget *win = awra_window_new_offscreen("undersize", 400, 300, 1.0);
  AwraWidget *overlay = awra_overlay_new();
  AwraWidget *stack = awra_stack_new();
  AwraWidget *page = block(page_w, 100);
  awra_stack_add_named(stack, page, "page");
  awra_overlay_set_child(overlay, stack);
  awra_window_set_child(win, overlay);
  awra_widget_set_direction(overlay, rtl ? AWRA_TEXT_DIR_RTL : AWRA_TEXT_DIR_LTR);
  awra_window_render_offscreen(win, true);
  CHECK(stack->w == 400, "%s: the stack has the window's width (%d)", rtl ? "rtl" : "ltr", stack->w);
  const int want_w = page_w > 400 ? page_w : 400;
  CHECK(page->w == want_w, "%s: the page has at least its minimum (%d, not %d)", rtl ? "rtl" : "ltr",
        page->w, want_w);
  CHECK(page->x == want_x, "%s: the page is centred as GtkStack centres it (%d, not %d)",
        rtl ? "rtl" : "ltr", page->x, want_x);
  awra_widget_unref(page);
  awra_widget_unref(stack);
  awra_widget_unref(overlay);
  awra_widget_destroy(win);
}

static void check_split(void)
{
  AwraWidget *split = awra_split_view_new();
  AwraWidget *side = block(300, 50), *content = block(500, 50);
  awra_split_view_set_sidebar(split, side);
  awra_split_view_set_content(split, content);
  int mn = 0, nt = 0;
  awra_measure(split, AWRA_ORIENTATION_HORIZONTAL, -1, &mn, &nt);
  CHECK(mn == 300 + 1 + 500, "a split view's minimum is GtkPaned's (%d)", mn);

  /* in a stack of a narrower window: the split at its minimum, centred */
  /* (wide enough for the medium layout: the sidebar shows) */
  AwraWidget *win = awra_window_new_offscreen("split", 761, 200, 1.0);
  AwraWidget *stack = awra_stack_new();
  awra_stack_add_named(stack, split, "split");
  awra_window_set_child(win, stack);
  awra_window_render_offscreen(win, true);
  CHECK(split->w == 801 && split->x == -20, "the split view at its minimum, centred (%d at %d)",
        split->w, split->x);
  CHECK(side->x == -20 && side->w == 300, "the sidebar at the split's start (%d, %d)", side->x,
        side->w);
  CHECK(content->x == 281 && content->w == 500, "the content after the handle (%d, %d)",
        content->x, content->w);
  awra_widget_unref(side);
  awra_widget_unref(content);
  awra_widget_unref(split);
  awra_widget_unref(stack);
  awra_widget_destroy(win);
}

int main(void)
{
  /* the user reads Arabic: Latin letters are English to Pango */
  setenv("LANG", "ar_EG.UTF-8", 1);
  unsetenv("LC_ALL");
  unsetenv("LC_CTYPE");
  unsetenv("LANGUAGE");
  unsetenv("PANGO_LANGUAGE");
  awra_init("org.awra.TestUndersize");
  awra_widget_set_default_direction(AWRA_TEXT_DIR_LTR); /* (the locale's is right to left) */
#if defined(_WIN32) || defined(__APPLE__)
  CHECK(BUI_text_latin_own_font() == 1, "fonts aren't sorted by language here");
#else
  CHECK(BUI_text_latin_own_font() == 0, "Arabic doesn't write Latin: English fonts for it");
#endif
  check_stack(false, 460, -30);
  check_stack(true, 460, -30);
  check_stack(false, 461, -30); /* half of an odd excess rounds toward zero */
  check_stack(true, 461, -30);
  check_stack(false, 300, 0); /* room enough: the stack's own width */
  check_split();
  if (failures) {
    fprintf(stderr, "test_undersize: %d failure(s)\n", failures);
    return 1;
  }
  printf("test_undersize: ok\n");
  return 0;
}
