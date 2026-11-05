/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-avatar-group.h>

/* Each avatar overlaps the one before by a third of its width. Drawn in
 * order, every avatar but the last is masked by a circle a little larger
 * than its follower, so the gap between them is cut out of the picture
 * rather than painted in a colour guessed from the background. */

#define OVERLAP 0.34
#define GAP 2.0f

struct _AwraAvatarGroup {
  GtkWidget parent_instance;
};

G_DEFINE_FINAL_TYPE (AwraAvatarGroup, awra_avatar_group, GTK_TYPE_WIDGET)

static int
step_for (int size)
{
  return (int) (size * (1.0 - OVERLAP) + 0.5);
}

static void
awra_avatar_group_measure (GtkWidget      *widget,
                           GtkOrientation  orientation,
                           int             for_size,
                           int            *minimum,
                           int            *natural,
                           int            *minimum_baseline,
                           int            *natural_baseline)
{
  int across = 0;
  int along = 0;
  guint count = 0;

  (void) for_size;
  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    int width = 0;
    int height = 0;

    if (!gtk_widget_should_layout (child))
      continue;
    gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &width, NULL, NULL);
    gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, -1, NULL, &height, NULL, NULL);
    along = count == 0 ? width : along + step_for (width);
    across = MAX (across, height);
    count++;
  }
  *minimum = *natural = orientation == GTK_ORIENTATION_HORIZONTAL ? along : across;
  *minimum_baseline = *natural_baseline = -1;
}

static void
awra_avatar_group_size_allocate (GtkWidget *widget,
                                 int        width,
                                 int        height,
                                 int        baseline)
{
  int x = 0;
  gboolean rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  (void) baseline;
  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    int child_width = 0;
    int child_height = 0;
    int left;

    if (!gtk_widget_should_layout (child))
      continue;
    gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &child_width, NULL, NULL);
    gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, -1, NULL, &child_height, NULL, NULL);
    left = rtl ? width - x - child_width : x;
    gtk_widget_allocate (child, child_width, child_height, -1,
                         gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (
                           left, (height - child_height) / 2.0f)));
    x += step_for (child_width);
  }
}

static GtkWidget *
next_shown (GtkWidget *child)
{
  for (child = gtk_widget_get_next_sibling (child); child != NULL;
       child = gtk_widget_get_next_sibling (child))
    if (gtk_widget_should_layout (child))
      return child;
  return NULL;
}

static void
awra_avatar_group_snapshot (GtkWidget   *widget,
                            GtkSnapshot *snapshot)
{
  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    GtkWidget *next;
    graphene_rect_t bounds;
    GskRoundedRect cut;

    if (!gtk_widget_should_layout (child))
      continue;
    next = next_shown (child);
    if (next == NULL || !gtk_widget_compute_bounds (next, widget, &bounds)) {
      gtk_widget_snapshot_child (widget, child, snapshot);
      continue;
    }
    /* Everything of this avatar but where the next one, and its gap, go. */
    graphene_rect_inset (&bounds, -GAP, -GAP);
    gsk_rounded_rect_init_from_rect (&cut, &bounds, bounds.size.width / 2.0f);
    gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_INVERTED_ALPHA);
    gtk_snapshot_push_rounded_clip (snapshot, &cut);
    gtk_snapshot_append_color (snapshot, &(GdkRGBA) { 0, 0, 0, 1 }, &bounds);
    gtk_snapshot_pop (snapshot);
    gtk_snapshot_pop (snapshot);
    gtk_widget_snapshot_child (widget, child, snapshot);
    gtk_snapshot_pop (snapshot);
  }
}

static void
awra_avatar_group_dispose (GObject *object)
{
  awra_avatar_group_remove_all (AWRA_AVATAR_GROUP (object));
  G_OBJECT_CLASS (awra_avatar_group_parent_class)->dispose (object);
}

static void
awra_avatar_group_class_init (AwraAvatarGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_avatar_group_dispose;
  widget_class->measure = awra_avatar_group_measure;
  widget_class->size_allocate = awra_avatar_group_size_allocate;
  widget_class->snapshot = awra_avatar_group_snapshot;
  gtk_widget_class_set_css_name (widget_class, "awraavatargroup");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
awra_avatar_group_init (AwraAvatarGroup *self)
{
  (void) self;
}

GtkWidget *
awra_avatar_group_new (void)
{
  return g_object_new (AWRA_TYPE_AVATAR_GROUP, NULL);
}

void
awra_avatar_group_append (AwraAvatarGroup *self,
                          GtkWidget       *avatar)
{
  g_return_if_fail (AWRA_IS_AVATAR_GROUP (self));
  g_return_if_fail (GTK_IS_WIDGET (avatar) && gtk_widget_get_parent (avatar) == NULL);
  gtk_widget_set_parent (avatar, GTK_WIDGET (self));
}

void
awra_avatar_group_remove_all (AwraAvatarGroup *self)
{
  GtkWidget *child;

  g_return_if_fail (AWRA_IS_AVATAR_GROUP (self));
  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))) != NULL)
    gtk_widget_unparent (child);
}
