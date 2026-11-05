/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>

/* The surface's scroller holds the box of items. */
static GtkWidget *
menu_box (AwraPopover *popover)
{
  GtkWidget *scroller = awra_popover_get_child (popover);
  GtkWidget *viewport = gtk_scrolled_window_get_child (GTK_SCROLLED_WINDOW (scroller));

  if (GTK_IS_VIEWPORT (viewport))
    return gtk_viewport_get_child (GTK_VIEWPORT (viewport));
  return viewport;
}

static GMenu *
menu_with_custom (const char *id)
{
  GMenu *menu = g_menu_new ();
  g_autoptr (GMenuItem) custom = g_menu_item_new (NULL, NULL);

  g_menu_item_set_attribute (custom, "custom", "s", id);
  g_menu_append_item (menu, custom);
  g_menu_append (menu, "Copy", "app.copy");
  return menu;
}

static void
test_custom_child_stands_in_for_its_item (void)
{
  AwraPopover *popover = AWRA_POPOVER (g_object_ref_sink (awra_popover_new ()));
  g_autoptr (GMenu) menu = menu_with_custom ("volume");
  GtkWidget *scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 200, 5);

  g_object_ref_sink (scale);
  g_assert_true (awra_popover_add_menu_widget (popover, scale, "volume"));
  g_assert_false (awra_popover_add_menu_widget (popover, gtk_label_new ("again"), "volume"));
  awra_popover_set_menu_model (popover, G_MENU_MODEL (menu));
  g_assert_true (gtk_widget_get_parent (scale) == menu_box (popover));
  g_assert_true (gtk_widget_get_first_child (menu_box (popover)) == scale);
  g_assert_true (gtk_widget_has_css_class (scale, "awra-menu-custom"));

  /* A rebuilt menu, as each time it opens, takes the widget along. */
  g_menu_append (menu, "Paste", "app.paste");
  awra_popover_set_menu_model (popover, NULL);
  awra_popover_set_menu_model (popover, G_MENU_MODEL (menu));
  g_assert_true (gtk_widget_get_parent (scale) == menu_box (popover));

  g_assert_true (awra_popover_remove_menu_widget (popover, scale));
  g_assert_null (gtk_widget_get_parent (scale));
  g_assert_false (gtk_widget_has_css_class (scale, "awra-menu-custom"));
  g_assert_false (awra_popover_remove_menu_widget (popover, scale));
  g_object_unref (scale);
  g_object_unref (popover);
}

static void
test_unknown_custom_item_shows_nothing (void)
{
  AwraPopover *popover = AWRA_POPOVER (g_object_ref_sink (awra_popover_new ()));
  g_autoptr (GMenu) menu = menu_with_custom ("missing");
  GtkWidget *first;

  awra_popover_set_menu_model (popover, G_MENU_MODEL (menu));
  first = gtk_widget_get_first_child (menu_box (popover));
  g_assert_nonnull (first);
  g_assert_true (gtk_widget_has_css_class (first, "awra-menu-item"));
  g_assert_null (gtk_widget_get_next_sibling (first));
  g_object_unref (popover);
}

int
main (int argc, char **argv)
{
  gtk_test_init (&argc, &argv, NULL);
  awra_init ();
  g_test_add_func ("/popover-menu/custom-child", test_custom_child_stands_in_for_its_item);
  g_test_add_func ("/popover-menu/unknown-custom", test_unknown_custom_item_shows_nothing);
  return g_test_run ();
}
