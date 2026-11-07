/* Style classes are unlimited (the first 64 are bits, the others a sorted
 * array per widget).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <awra/awra.h>

#include <stdio.h>

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

int main(void)
{
  awra_init("org.awra.TestStyleClasses");
  AwraWidget *w = awra_label_new("x"), *other = awra_label_new("y");
  char name[32];
  for (int i = 0; i < 300; i++) {
    snprintf(name, sizeof name, "cls-%d", i);
    awra_widget_add_class(w, name);
    awra_widget_add_class(w, name); /* twice: no change */
  }
  for (int i = 0; i < 300; i++) {
    snprintf(name, sizeof name, "cls-%d", i);
    CHECK(awra_widget_has_class(w, name), "%s missing", name);
    CHECK(!awra_widget_has_class(other, name), "%s on the other widget", name);
  }
  for (int i = 0; i < 300; i += 2) {
    snprintf(name, sizeof name, "cls-%d", i);
    awra_widget_remove_class(w, name);
  }
  for (int i = 0; i < 300; i++) {
    snprintf(name, sizeof name, "cls-%d", i);
    CHECK(awra_widget_has_class(w, name) == (i % 2 == 1), "%s after removal", name);
  }
  awra_widget_add_class(w, "accent");
  CHECK(awra_widget_has_class(w, "accent"), "built-in class");
  CHECK(!awra_widget_has_class(w, "never-added"), "unknown class");
  awra_widget_unref(w);
  awra_widget_unref(other);
  if (failures) {
    return 1;
  }
  printf("style classes ok\n");
  return 0;
}
