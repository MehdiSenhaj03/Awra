/* Accessibility model tests: roles, names (GTK's rules), states, values,
 * explicit attributes, weak relations, and the change events a backend
 * receives (only what changed). Offscreen, no display needed.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "a11y.h"

#include <stdio.h>

static int g_fail;
#define CHECK(c)                                                         \
  do {                                                                   \
    if (!(c)) {                                                          \
      fprintf(stderr, "%s:%d: FAILED: %s\n", __FILE__, __LINE__, #c);   \
      g_fail++;                                                          \
    }                                                                    \
  } while (0)

static void check_name(AwraWidget *w, const char *want)
{
  char *n = awra_accessible_get_name(w);
  if (strcmp(n, want) != 0) {
    fprintf(stderr, "name: got \"%s\", want \"%s\"\n", n, want);
    g_fail++;
  }
  awra_free(n);
}

static void check_desc(AwraWidget *w, const char *want)
{
  char *n = awra_accessible_get_description(w);
  if (strcmp(n, want) != 0) {
    fprintf(stderr, "description: got \"%s\", want \"%s\"\n", n, want);
    g_fail++;
  }
  awra_free(n);
}

/* a recording backend */
static char g_log[8192];
static void logf_(const char *fmt, const char *a, int b)
{
  char line[256];
  snprintf(line, sizeof(line), fmt, a, b);
  strncat(g_log, line, sizeof(g_log) - strlen(g_log) - 1);
}
static const char *type_of(AwraWidget *w)
{
  return awra_widget_type(w);
}
static void ev_state(AwraWidget *w, int item, unsigned flag, bool on)
{
  char b[64];
  snprintf(b, sizeof(b), "state:%s:%x:%d;", type_of(w), flag, on);
  strncat(g_log, b, sizeof(g_log) - strlen(g_log) - 1);
}
static void ev_name(AwraWidget *w)
{
  char *n = awra_a11y_name(w);
  logf_("name:%s;", n, 0);
  free(n);
}
static void ev_desc(AwraWidget *w)
{
  logf_("desc:%s;", type_of(w), 0);
}
static void ev_value(AwraWidget *w)
{
  logf_("value:%s;", type_of(w), 0);
}
static void ev_text(AwraWidget *w, bool insert, int offset, int length, const char *t, int bytes)
{
  char b[128];
  snprintf(b, sizeof(b), "text:%s:%d:%d:%.*s;", insert ? "ins" : "del", offset, length, bytes, t);
  strncat(g_log, b, sizeof(g_log) - strlen(g_log) - 1);
}
static void ev_caret(AwraWidget *w, int offset)
{
  logf_("caret:%s:%d;", type_of(w), offset);
}
static void ev_children(AwraWidget *p, AwraWidget *c, int index, bool added)
{
  char b[128];
  snprintf(b, sizeof(b), "children:%s:%s:%d:%d;", type_of(p), type_of(c), index, added);
  strncat(g_log, b, sizeof(g_log) - strlen(g_log) - 1);
}
static int g_freed;
static void ev_freed(AwraWidget *w, AwraA11ySnap *s)
{
  g_freed++;
}
static const AwraA11yBackend g_rec = {
    .state = ev_state, .name = ev_name, .description = ev_desc, .value = ev_value,
    .text = ev_text, .caret = ev_caret, .children = ev_children, .freed = ev_freed,
};

static void flush(void)
{
  awra_a11y_flush();
}

int main(void)
{
  awra_init("org.awra.A11yTest");
  AwraWidget *win = awra_window_new_offscreen("Settings", 400, 300, 1.0);
  AwraWidget *box = awra_box_new(AWRA_ORIENTATION_VERTICAL, 6);
  awra_window_set_child(win, box);

  /* roles */
  AwraWidget *button = awra_button_new_with_label("Save");
  AwraWidget *icon = awra_button_new_from_icon_name("edit-delete-symbolic");
  awra_widget_set_tooltip(icon, "Delete");
  AwraWidget *check = awra_check_button_new_with_label("Remember me");
  AwraWidget *sw = awra_switch_new();
  AwraWidget *slider = awra_slider_new(0, 10, 1);
  AwraWidget *entry = awra_entry_new();
  AwraWidget *label = awra_label_new("Name");
  AwraWidget *toggle = awra_toggle_button_new_with_label("Bold");
  AwraWidget *progress = awra_progress_bar_new();
  AwraWidget *widgets[] = {button, icon, check, sw, slider, label, entry, toggle, progress};
  for (size_t i = 0; i < sizeof(widgets) / sizeof(widgets[0]); i++) {
    awra_box_append(box, widgets[i]);
    awra_widget_unref(widgets[i]); /* the box holds them */
  }
  awra_window_render_offscreen(win, true);

  CHECK(awra_accessible_get_role(win) == AWRA_ACCESSIBLE_ROLE_WINDOW);
  CHECK(awra_accessible_get_role(box) == AWRA_ACCESSIBLE_ROLE_GENERIC);
  CHECK(awra_accessible_get_role(button) == AWRA_ACCESSIBLE_ROLE_BUTTON);
  CHECK(awra_accessible_get_role(check) == AWRA_ACCESSIBLE_ROLE_CHECKBOX);
  CHECK(awra_accessible_get_role(sw) == AWRA_ACCESSIBLE_ROLE_SWITCH);
  CHECK(awra_accessible_get_role(slider) == AWRA_ACCESSIBLE_ROLE_SLIDER);
  CHECK(awra_accessible_get_role(entry) == AWRA_ACCESSIBLE_ROLE_TEXT_BOX);
  CHECK(awra_accessible_get_role(label) == AWRA_ACCESSIBLE_ROLE_LABEL);
  CHECK(awra_accessible_get_role(toggle) == AWRA_ACCESSIBLE_ROLE_TOGGLE_BUTTON);
  CHECK(awra_accessible_get_role(progress) == AWRA_ACCESSIBLE_ROLE_PROGRESS_BAR);

  /* names: own text, content, tooltip, explicit label, labelled-by */
  check_name(win, "Settings");
  check_name(button, "Save");
  check_name(icon, "Delete");
  check_desc(icon, ""); /* the tooltip already names it */
  check_name(check, "Remember me");
  check_name(label, "Name");
  check_name(entry, "");
  awra_accessible_update_relation(entry, AWRA_ACCESSIBLE_RELATION_LABELLED_BY, &label, 1);
  check_name(entry, "Name");
  awra_widget_set_tooltip(button, "Writes the file");
  check_name(button, "Save");
  check_desc(button, "Writes the file");
  awra_accessible_update_property_string(sw, AWRA_ACCESSIBLE_PROPERTY_LABEL, "Wi-Fi");
  check_name(sw, "Wi-Fi");
  awra_accessible_update_property_string(sw, AWRA_ACCESSIBLE_PROPERTY_DESCRIPTION, "Radio");
  check_desc(sw, "Radio");
  /* a button of a box of labels: named from its content */
  AwraWidget *content = awra_box_new(AWRA_ORIENTATION_HORIZONTAL, 4);
  awra_box_append(content, awra_label_new("Open"));
  awra_box_append(content, awra_label_new("file"));
  AwraWidget *cbutton = awra_button_new();
  awra_button_set_child(cbutton, content);
  awra_box_append(box, cbutton);
  check_name(cbutton, "Open file");
  /* a hidden child does not speak */
  awra_widget_set_visible(content->kids[1], false);
  check_name(cbutton, "Open");

  /* states and values */
  CHECK(awra_accessible_get_state(check, AWRA_ACCESSIBLE_STATE_CHECKED) == 0);
  awra_check_button_set_active(check, true);
  CHECK(awra_accessible_get_state(check, AWRA_ACCESSIBLE_STATE_CHECKED) == 1);
  CHECK(awra_accessible_get_state(toggle, AWRA_ACCESSIBLE_STATE_PRESSED) == 0);
  CHECK(awra_accessible_get_state(button, AWRA_ACCESSIBLE_STATE_CHECKED) == -1);
  awra_widget_set_sensitive(box, false);
  CHECK(awra_accessible_get_state(button, AWRA_ACCESSIBLE_STATE_DISABLED) == 1);
  awra_widget_set_sensitive(box, true);
  CHECK(awra_accessible_get_state(button, AWRA_ACCESSIBLE_STATE_DISABLED) == -1);
  awra_accessible_update_state(button, AWRA_ACCESSIBLE_STATE_BUSY, 1);
  CHECK(awra_accessible_get_state(button, AWRA_ACCESSIBLE_STATE_BUSY) == 1);
  awra_accessible_reset_state(button, AWRA_ACCESSIBLE_STATE_BUSY);
  CHECK(awra_accessible_get_state(button, AWRA_ACCESSIBLE_STATE_BUSY) == -1);
  awra_slider_set_value(slider, 4);
  AwraA11yInfo info;
  awra_a11y_info(slider, &info);
  CHECK(info.has_value && info.value == 4 && info.min == 0 && info.max == 10);
  awra_accessible_set_role(box, AWRA_ACCESSIBLE_ROLE_GROUP);
  CHECK(awra_accessible_get_role(box) == AWRA_ACCESSIBLE_ROLE_GROUP);

  /* weak relations: the label goes, the entry loses its name */
  awra_widget_ref(label);
  awra_box_remove(box, label);
  awra_widget_unref(label);
  check_name(entry, "");

  /* tree: the decorative icon of a button is skipped, boxes are presented */
  AwraWidget *kids[32];
  const int n = awra_a11y_children(win, kids, 32);
  CHECK(n >= 1);
  CHECK(awra_a11y_parent(button) == box);
  CHECK(awra_a11y_index_in_parent(button) == 0);

  /* events: only what changed */
  awra_a11y_set_backend(&g_rec);
  CHECK(awra_accessible_active());
  awra_a11y_expose(win);
  awra_a11y_expose(box);
  awra_a11y_expose(check);
  awra_a11y_expose(entry);
  awra_a11y_expose(slider);
  awra_a11y_expose(button);
  g_log[0] = 0;
  awra_check_button_set_active(check, false);
  flush();
  CHECK(strstr(g_log, "state:AwraCheckButton:80:0;") != NULL); /* CHECKED off */
  CHECK(strstr(g_log, "name:") == NULL);
  g_log[0] = 0;
  awra_window_render_offscreen(win, false); /* repaint only */
  flush();
  CHECK(g_log[0] == 0);
  g_log[0] = 0;
  awra_entry_set_text(entry, "hello");
  flush();
  CHECK(strstr(g_log, "text:ins:0:5:hello;") != NULL);
  g_log[0] = 0;
  awra_entry_set_text(entry, "help");
  flush();
  CHECK(strstr(g_log, "text:del:3:2:lo;") != NULL);
  CHECK(strstr(g_log, "text:ins:3:1:p;") != NULL);
  g_log[0] = 0;
  awra_slider_set_value(slider, 7);
  flush();
  CHECK(strstr(g_log, "value:AwraSlider;") != NULL);
  g_log[0] = 0;
  awra_button_set_label(button, "Save as");
  flush();
  CHECK(strstr(g_log, "name:Save as;") != NULL);
  g_log[0] = 0;
  AwraWidget *extra = awra_label_new("New");
  awra_box_append(box, extra);
  CHECK(strstr(g_log, "children:AwraBox:AwraLabel:") != NULL && strstr(g_log, ":1;"));
  g_log[0] = 0;
  awra_widget_set_visible(slider, false);
  CHECK(strstr(g_log, "children:AwraBox:AwraSlider:") != NULL && strstr(g_log, ":0;"));
  g_log[0] = 0;
  awra_widget_grab_focus(check);
  flush();
  CHECK(strstr(g_log, "state:AwraCheckButton:2:1;") != NULL); /* FOCUSED */
  g_log[0] = 0;
  awra_widget_grab_focus(entry);
  flush();
  CHECK(strstr(g_log, "state:AwraCheckButton:2:0;") != NULL);
  CHECK(strstr(g_log, "state:AwraEntry:2:1;") != NULL);
  const int freed_before = g_freed;
  awra_box_remove(box, button); /* frees it */
  CHECK(g_freed == freed_before + 1);
  awra_a11y_set_backend(NULL);
  CHECK(!awra_accessible_active());

  awra_widget_destroy(win);
  awra_pump();

  /* scale 2: bounds are logical; a hidden password is never told */
  {
    AwraWidget *w3 = awra_window_new_offscreen("Scaled", 300, 200, 2.0);
    AwraWidget *pw = awra_password_entry_new();
    awra_window_set_child(w3, pw);
    awra_widget_unref(pw);
    AwraWidget *entry3 = awra_password_entry_entry(pw);
    awra_entry_set_text(entry3, "s3cr\xC3\xA9t");
    awra_window_render_offscreen(w3, true);
    double r[4];
    awra_a11y_bounds(entry3, r);
    CHECK(fabs(r[2] - awra_widget_width(entry3)) < 1.0);
    AwraA11yInfo pi;
    awra_a11y_info(entry3, &pi);
    CHECK((pi.flags & AWRA_A11Y_PASSWORD) != 0);
    awra_a11y_set_backend(&g_rec);
    awra_a11y_expose(entry3);
    CHECK(awra_a11y_snap(entry3)->text && !awra_a11y_snap(entry3)->text[0]);
    g_log[0] = 0;
    awra_entry_set_text(entry3, "other");
    awra_a11y_flush();
    CHECK(strstr(g_log, "text:") == NULL); /* nothing of it leaves */
    awra_a11y_set_backend(NULL);
    awra_widget_destroy(w3);
    awra_pump();
  }

  /* windows opened and closed while a client walks them: every exposed
   * widget is let go, the memory comes back flat */
  awra_a11y_set_backend(&g_rec);
  long rss_at[2] = {0, 0};
  int exposed = 0;
  const int freed0 = g_freed;
  for (int round = 0; round < 400; round++) {
    AwraWidget *w2 = awra_window_new_offscreen("Dialog", 360, 240, 1.0);
    AwraWidget *form = awra_form_new();
    AwraWidget *row = awra_form_row_new("Delay", "Between two sounds");
    AwraWidget *spin = awra_spin_button_new(0, 100, 1);
    awra_form_row_set_control(row, spin);
    awra_widget_unref(spin);
    awra_form_append(form, row);
    AwraWidget *ok = awra_button_new_with_label("OK");
    awra_accessible_update_relation(ok, AWRA_ACCESSIBLE_RELATION_DESCRIBED_BY, &row, 1);
    awra_box_append(form, ok);
    awra_widget_unref(ok);
    awra_widget_unref(row);
    awra_window_set_child(w2, form);
    awra_widget_unref(form);
    awra_window_render_offscreen(w2, true);
    /* the client reaches everything */
    AwraWidget *stack[64];
    int n = 0;
    stack[n++] = w2;
    while (n > 0) {
      AwraWidget *x = stack[--n];
      awra_a11y_expose(x);
      exposed++;
      AwraWidget *kids[32];
      const int k = awra_a11y_children(x, kids, 32);
      for (int i = 0; i < k && i < 32 && n < 64; i++) {
        stack[n++] = kids[i];
      }
    }
    awra_entry_set_text(spin, "12");
    awra_a11y_flush();
    awra_widget_destroy(w2);
    awra_pump();
#ifdef __linux__
    if (round == 49 || round == 399) {
      FILE *f = fopen("/proc/self/statm", "r");
      long pages = 0, res = 0;
      if (f && fscanf(f, "%ld %ld", &pages, &res) == 2) {
        rss_at[round == 399] = res * 4;
      }
      if (f) {
        fclose(f);
      }
    }
#endif
  }
  CHECK(g_freed - freed0 == exposed); /* every exposed widget was freed */
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) /* ASan keeps freed memory */
  printf("a11y: RSS after 50 rounds %ld KB, after 400 %ld KB\n", rss_at[0], rss_at[1]);
  CHECK(rss_at[1] - rss_at[0] < 256);
#endif
  awra_a11y_set_backend(NULL);
  if (g_fail) {
    fprintf(stderr, "%d failure(s)\n", g_fail);
    return 1;
  }
  printf("a11y: all passed\n");
  return 0;
}
