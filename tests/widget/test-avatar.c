/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra.h>

static gboolean
quit_loop (gpointer user_data)
{
  g_main_loop_quit (user_data);
  return G_SOURCE_REMOVE;
}

static void
run_for (guint ms)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);

  g_timeout_add (ms, quit_loop, loop);
  g_main_loop_run (loop);
}

static GdkTexture *
make_texture (int width,
              int height)
{
  g_autoptr (GBytes) bytes = NULL;
  guchar *data = g_malloc (width * height * 4);

  for (int i = 0; i < width * height; i++) {
    data[i * 4] = 200;
    data[i * 4 + 1] = (guchar) (i % 255);
    data[i * 4 + 2] = 90;
    data[i * 4 + 3] = 255;
  }
  bytes = g_bytes_new_take (data, width * height * 4);
  return gdk_memory_texture_new (width, height, GDK_MEMORY_R8G8B8A8, bytes, width * 4);
}

static void
render (GtkWidget *widget)
{
  g_autoptr (GtkSnapshot) snapshot = gtk_snapshot_new ();
  g_autoptr (GskRenderNode) node = NULL;

  GTK_WIDGET_GET_CLASS (widget)->snapshot (widget, snapshot);
  node = gtk_snapshot_to_node (snapshot);
  g_assert_nonnull (node);
}

static void
test_properties (void)
{
  GtkWidget *avatar = g_object_ref_sink (awra_avatar_new (40, "Ada Lovelace"));
  AwraAvatar *self = AWRA_AVATAR (avatar);
  g_autoptr (GdkTexture) texture = make_texture (8, 4);
  int minimum = 0;
  int natural = 0;

  g_assert_cmpint (awra_avatar_get_size (self), ==, 40);
  g_assert_cmpstr (awra_avatar_get_text (self), ==, "Ada Lovelace");
  g_assert_true (awra_avatar_get_show_initials (self));
  g_assert_null (awra_avatar_get_custom_image (self));
  g_assert_null (awra_avatar_get_icon_name (self));
  g_assert_cmpint (awra_avatar_get_shape (self), ==, AWRA_AVATAR_SHAPE_CIRCLE);
  g_assert_cmpint (awra_avatar_get_presence (self), ==, AWRA_PRESENCE_NONE);
  g_assert_false (awra_avatar_get_speaking (self));
  gtk_widget_measure (avatar, GTK_ORIENTATION_HORIZONTAL, -1, &minimum, &natural, NULL, NULL);
  g_assert_cmpint (minimum, ==, 40);
  g_assert_cmpint (natural, ==, 40);
  awra_avatar_set_size (self, 24);
  gtk_widget_measure (avatar, GTK_ORIENTATION_VERTICAL, -1, &minimum, NULL, NULL, NULL);
  g_assert_cmpint (minimum, ==, 24);
  awra_avatar_set_shape (self, AWRA_AVATAR_SHAPE_ROUNDED);
  awra_avatar_set_presence (self, AWRA_PRESENCE_BUSY);
  awra_avatar_set_speaking (self, TRUE);
  awra_avatar_set_icon_name (self, "folder-symbolic");
  awra_avatar_set_show_initials (self, FALSE);
  awra_avatar_set_custom_image (self, GDK_PAINTABLE (texture));
  g_assert_true (awra_avatar_get_custom_image (self) == GDK_PAINTABLE (texture));
  g_assert_cmpint (awra_avatar_get_presence (self), ==, AWRA_PRESENCE_BUSY);
  g_assert_true (awra_avatar_get_speaking (self));
  g_assert_cmpstr (awra_avatar_get_icon_name (self), ==, "folder-symbolic");
  g_object_set (avatar, "text", NULL, "custom-image", NULL, "presence", AWRA_PRESENCE_NONE, NULL);
  g_assert_null (awra_avatar_get_text (self));
  g_object_unref (avatar);
}

static void
test_draws_every_face (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  g_autoptr (GdkTexture) wide = make_texture (64, 16);
  g_autoptr (GdkTexture) tall = make_texture (16, 64);
  const char *names[] = { "Mehdi Senhaj", "grace", "jean-luc picard", "🦊", "   ", "Ørjan", NULL };
  GtkWidget *avatars[10];
  guint n = 0;

  for (guint i = 0; names[i] != NULL; i++)
    avatars[n++] = awra_avatar_new (32, names[i]);
  avatars[n++] = awra_avatar_new (32, NULL);
  avatars[n] = awra_avatar_new (48, "Wide");
  awra_avatar_set_custom_image (AWRA_AVATAR (avatars[n++]), GDK_PAINTABLE (wide));
  avatars[n] = awra_avatar_new (48, "Tall");
  awra_avatar_set_custom_image (AWRA_AVATAR (avatars[n++]), GDK_PAINTABLE (tall));
  for (guint i = 0; i < n; i++) {
    awra_avatar_set_presence (AWRA_AVATAR (avatars[i]), (AwraPresence) (i % 5));
    gtk_box_append (GTK_BOX (box), avatars[i]);
  }
  gtk_widget_set_direction (avatars[0], GTK_TEXT_DIR_RTL);
  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_window_present (GTK_WINDOW (window));
  run_for (150);
  for (guint i = 0; i < n; i++)
    render (avatars[i]);
  /* Changing everything while mapped animates, then settles. */
  awra_avatar_set_speaking (AWRA_AVATAR (avatars[1]), TRUE);
  awra_avatar_set_presence (AWRA_AVATAR (avatars[1]), AWRA_PRESENCE_IDLE);
  awra_avatar_set_presence (AWRA_AVATAR (avatars[2]), AWRA_PRESENCE_NONE);
  awra_avatar_set_custom_image (AWRA_AVATAR (avatars[3]), GDK_PAINTABLE (wide));
  run_for (60);
  render (avatars[1]);
  render (avatars[3]);
  /* An image replaced mid-fade fades from what is on screen. */
  awra_avatar_set_custom_image (AWRA_AVATAR (avatars[3]), GDK_PAINTABLE (tall));
  run_for (400);
  render (avatars[3]);
  awra_avatar_set_speaking (AWRA_AVATAR (avatars[1]), FALSE);
  run_for (400);
  render (avatars[1]);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_destroy_while_animating (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *avatar = awra_avatar_new (40, "Linus");
  GtkWidget *weak = avatar;
  g_autoptr (GdkTexture) texture = make_texture (4, 4);

  g_object_add_weak_pointer (G_OBJECT (avatar), (gpointer *) &weak);
  gtk_window_set_child (GTK_WINDOW (window), avatar);
  gtk_window_present (GTK_WINDOW (window));
  run_for (100);
  awra_avatar_set_speaking (AWRA_AVATAR (avatar), TRUE);
  awra_avatar_set_presence (AWRA_AVATAR (avatar), AWRA_PRESENCE_ONLINE);
  awra_avatar_set_custom_image (AWRA_AVATAR (avatar), GDK_PAINTABLE (texture));
  run_for (30);
  gtk_window_destroy (GTK_WINDOW (window));
  run_for (50);
  g_assert_null (weak);
}

static void
test_reduced_motion_is_still (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *avatar = awra_avatar_new (40, "Grace Hopper");
  AwraStyleManager *style = awra_context_get_style_manager (
    awra_context_get_for_display (gdk_display_get_default ()));

  awra_style_manager_set_reduced_motion (style, TRUE);
  gtk_window_set_child (GTK_WINDOW (window), avatar);
  gtk_window_present (GTK_WINDOW (window));
  run_for (100);
  awra_avatar_set_speaking (AWRA_AVATAR (avatar), TRUE);
  render (avatar);
  awra_style_manager_set_reduced_motion (style, FALSE);
  run_for (50);
  awra_style_manager_set_reduced_motion (style, TRUE);
  render (avatar);
  awra_style_manager_set_reduced_motion (style, FALSE);
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_accessible_name (void)
{
  GtkWidget *avatar = g_object_ref_sink (awra_avatar_new (32, "Alan Turing"));

  g_assert_cmpint (gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (avatar)), ==,
                   GTK_ACCESSIBLE_ROLE_IMG);
  awra_avatar_set_presence (AWRA_AVATAR (avatar), AWRA_PRESENCE_ONLINE);
  awra_avatar_set_speaking (AWRA_AVATAR (avatar), TRUE);
  awra_avatar_set_text (AWRA_AVATAR (avatar), NULL);
  awra_avatar_set_presence (AWRA_AVATAR (avatar), AWRA_PRESENCE_NONE);
  awra_avatar_set_speaking (AWRA_AVATAR (avatar), FALSE);
  g_object_unref (avatar);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check ())
    return 77;
  awra_init ();
  g_test_add_func ("/avatar/properties", test_properties);
  g_test_add_func ("/avatar/draws-every-face", test_draws_every_face);
  g_test_add_func ("/avatar/destroy-while-animating", test_destroy_while_animating);
  g_test_add_func ("/avatar/reduced-motion", test_reduced_motion_is_still);
  g_test_add_func ("/avatar/accessible", test_accessible_name);
  return g_test_run ();
}
