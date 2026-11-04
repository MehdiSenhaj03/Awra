/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-rail.h>

#include <awra/awra-spring-animation.h>
#include <awra/awra-token-set.h>
#include <math.h>

#include "awra-i18n-private.h"
#include "navigation/awra-rail-private.h"
#include "widgets/awra-widget-style-private.h"

/* How close to an edge of the visible column a drag scrolls, and how fast. */
#define AUTOSCROLL_EDGE 40.0
#define AUTOSCROLL_SPEED 720.0
/* The middle share of a neighbour that merges instead of reordering. */
#define MERGE_ZONE 0.5

/* Separator */

#define AWRA_TYPE_RAIL_SEPARATOR (awra_rail_separator_get_type ())
G_DECLARE_FINAL_TYPE (AwraRailSeparator, awra_rail_separator, AWRA, RAIL_SEPARATOR, GtkWidget)

struct _AwraRailSeparator {
  GtkWidget parent_instance;
};

G_DEFINE_FINAL_TYPE (AwraRailSeparator, awra_rail_separator, GTK_TYPE_WIDGET)

static void
awra_rail_separator_measure (GtkWidget      *widget,
                             GtkOrientation  orientation,
                             int             for_size,
                             int            *minimum,
                             int            *natural,
                             int            *minimum_baseline,
                             int            *natural_baseline)
{
  (void) for_size;
  (void) minimum_baseline;
  (void) natural_baseline;
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum = *natural = (int) (awra_rail_tile_size (widget) * 2.0 / 3.0);
  else
    *minimum = *natural = 2;
}

static void
awra_rail_separator_snapshot (GtkWidget   *widget,
                              GtkSnapshot *snapshot)
{
  AwraTokenSet *tokens = awra_widget_get_token_set (widget);
  float length = round (awra_rail_tile_size (widget) * 2.0 / 3.0);
  float width = gtk_widget_get_width (widget);
  GskRoundedRect line;

  gsk_rounded_rect_init_from_rect (&line,
                                   &GRAPHENE_RECT_INIT (round ((width - length) / 2), 0, length, 2),
                                   1);
  gtk_snapshot_push_rounded_clip (snapshot, &line);
  gtk_snapshot_append_color (snapshot, awra_token_set_get_separator (tokens), &line.bounds);
  gtk_snapshot_pop (snapshot);
}

static void
awra_rail_separator_class_init (AwraRailSeparatorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->measure = awra_rail_separator_measure;
  widget_class->snapshot = awra_rail_separator_snapshot;
  gtk_widget_class_set_css_name (widget_class, "awrarailseparator");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_SEPARATOR);
}

static void
awra_rail_separator_init (AwraRailSeparator *self)
{
  gtk_widget_set_can_target (GTK_WIDGET (self), FALSE);
}

/* The scrolling column of entries. Each entry carries an offset that moves
 * it out of the way of a dragged one; offsets only change allocation. */

typedef struct {
  double offset;
  guint old_index;
  AwraAnimation *animation;
  GtkWidget *list;
} EntryMotion;

#define AWRA_TYPE_RAIL_LIST (awra_rail_list_get_type ())
G_DECLARE_FINAL_TYPE (AwraRailList, awra_rail_list, AWRA, RAIL_LIST, GtkWidget)

struct _AwraRailList {
  GtkWidget parent_instance;
  GtkWidget *dragged;
  double dragged_y;
};

G_DEFINE_FINAL_TYPE (AwraRailList, awra_rail_list, GTK_TYPE_WIDGET)

static GQuark motion_quark;

static EntryMotion *
entry_motion (GtkWidget *entry)
{
  return g_object_get_qdata (G_OBJECT (entry), motion_quark);
}

static void
entry_motion_free (gpointer data)
{
  EntryMotion *motion = data;

  /* Stop without a last value: the list may already be going away. */
  if (motion->animation != NULL)
    awra_animation_pause (motion->animation);
  g_clear_object (&motion->animation);
  g_free (motion);
}

static void
set_entry_offset (double   value,
                  gpointer user_data)
{
  EntryMotion *motion = user_data;

  motion->offset = value;
  gtk_widget_queue_allocate (motion->list);
}

static void
move_entry_to (GtkWidget *list,
               GtkWidget *entry,
               double     offset)
{
  EntryMotion *motion = entry_motion (entry);

  if (motion->animation == NULL) {
    if (motion->offset == offset)
      return;
    motion->list = list;
    motion->animation =
      awra_spring_animation_new (entry, motion->offset, motion->offset, 0.86, 520.0,
                                 awra_callback_animation_target_new (set_entry_offset, motion,
                                                                     NULL));
    awra_spring_animation_set_epsilon (AWRA_SPRING_ANIMATION (motion->animation), 0.25);
  }
  if (awra_spring_animation_get_value_to (AWRA_SPRING_ANIMATION (motion->animation)) == offset &&
      awra_animation_get_state (motion->animation) == AWRA_ANIMATION_STATE_PLAYING)
    return;
  awra_spring_animation_retarget (AWRA_SPRING_ANIMATION (motion->animation), offset);
}

static void
jump_entry_to (GtkWidget *entry,
               double     offset)
{
  EntryMotion *motion = entry_motion (entry);

  if (motion->animation != NULL)
    awra_animation_reset (motion->animation);
  motion->offset = offset;
  if (motion->animation != NULL) {
    awra_spring_animation_set_value_from (AWRA_SPRING_ANIMATION (motion->animation), offset);
    awra_spring_animation_set_value_to (AWRA_SPRING_ANIMATION (motion->animation), offset);
    awra_spring_animation_set_initial_velocity (AWRA_SPRING_ANIMATION (motion->animation), 0);
  }
}

static int
entry_height (GtkWidget *entry)
{
  int height = 0;

  gtk_widget_measure (entry, GTK_ORIENTATION_VERTICAL, -1, &height, NULL, NULL, NULL);
  return height;
}

static void
awra_rail_list_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum,
                        int            *natural,
                        int            *minimum_baseline,
                        int            *natural_baseline)
{
  double gap = awra_rail_entry_gap (widget);
  int total = 0;

  (void) for_size;
  (void) minimum_baseline;
  (void) natural_baseline;
  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    int size = 0;

    gtk_widget_measure (child, orientation, -1, &size, NULL, NULL, NULL);
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
      total = MAX (total, size);
    else
      total += size + (int) gap;
  }
  if (orientation == GTK_ORIENTATION_VERTICAL)
    total += (int) gap;
  *minimum = *natural = total;
}

static void
awra_rail_list_size_allocate (GtkWidget *widget,
                              int        width,
                              int        height,
                              int        baseline)
{
  AwraRailList *self = AWRA_RAIL_LIST (widget);
  double gap = awra_rail_entry_gap (widget);
  double y = gap;

  (void) height;
  (void) baseline;
  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    int child_height = entry_height (child);
    double top = child == self->dragged ? self->dragged_y : y + entry_motion (child)->offset;

    gtk_widget_allocate (child, width, child_height, -1,
                         gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (0, round (top))));
    y += child_height + gap;
  }
}

static void
awra_rail_list_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  AwraRailList *self = AWRA_RAIL_LIST (widget);

  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    if (child != self->dragged)
      gtk_widget_snapshot_child (widget, child, snapshot);
  }
  /* The lifted entry passes over its neighbours. */
  if (self->dragged != NULL)
    gtk_widget_snapshot_child (widget, self->dragged, snapshot);
}

static void
awra_rail_list_dispose (GObject *object)
{
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (object))) != NULL) {
    /* An entry the application keeps must not keep motion towards this list. */
    g_object_set_qdata (G_OBJECT (child), motion_quark, NULL);
    gtk_widget_unparent (child);
  }
  G_OBJECT_CLASS (awra_rail_list_parent_class)->dispose (object);
}

static void
awra_rail_list_class_init (AwraRailListClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  G_OBJECT_CLASS (klass)->dispose = awra_rail_list_dispose;
  widget_class->measure = awra_rail_list_measure;
  widget_class->size_allocate = awra_rail_list_size_allocate;
  widget_class->snapshot = awra_rail_list_snapshot;
  gtk_widget_class_set_css_name (widget_class, "awraraillist");
  motion_quark = g_quark_from_static_string ("awra-rail-entry-motion");
}

static void
awra_rail_list_init (AwraRailList *self)
{
  (void) self;
}

/* The rail */

struct _AwraRail {
  GtkWidget parent_instance;
  GtkWidget *header;
  GtkWidget *footer;
  GtkWidget *scroller;
  AwraRailList *list;
  AwraRailItem *selected;
  gboolean reorderable;
  GtkGesture *drag;
  /* Drag state, in list coordinates unless noted. */
  GtkWidget *pressed_entry;
  GtkWidget *dragged;
  gboolean dragging;
  double press_x;
  double press_y;
  double grab_offset;
  double pointer_view_y;
  guint origin_index;
  guint drop_index;
  GtkWidget *merge_target;
  guint autoscroll_id;
  gint64 autoscroll_time;
};

enum {
  PROP_0,
  PROP_SELECTED_ITEM,
  PROP_HEADER,
  PROP_FOOTER,
  PROP_REORDERABLE,
  N_PROPS,
};

enum {
  SIGNAL_ITEM_MOVED,
  SIGNAL_ITEM_MERGED,
  N_SIGNALS,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (AwraRail, awra_rail, GTK_TYPE_WIDGET)

static gboolean
is_entry_type (GtkWidget *widget)
{
  return AWRA_IS_RAIL_ITEM (widget) || AWRA_IS_RAIL_GROUP (widget);
}

static GtkWidget *
list_widget (AwraRail *self)
{
  return GTK_WIDGET (self->list);
}

static guint
entry_index (AwraRail  *self,
             GtkWidget *entry)
{
  guint index = 0;

  for (GtkWidget *child = gtk_widget_get_first_child (list_widget (self)); child != NULL;
       child = gtk_widget_get_next_sibling (child), index++) {
    if (child == entry)
      return index;
  }
  return G_MAXUINT;
}

/* Selection */

void
awra_rail_item_activated (AwraRail     *self,
                          AwraRailItem *item)
{
  /* Footer actions, such as adding a workspace, are not destinations. */
  if (self->footer != NULL && gtk_widget_is_ancestor (GTK_WIDGET (item), self->footer))
    return;
  if (self->footer == GTK_WIDGET (item))
    return;
  awra_rail_set_selected_item (self, item);
}

void
awra_rail_item_selection_changed (AwraRail     *self,
                                  AwraRailItem *item)
{
  if (awra_rail_item_get_selected (item)) {
    AwraRailItem *previous = self->selected;

    if (previous == item)
      return;
    self->selected = item;
    if (previous != NULL)
      awra_rail_item_set_selected (previous, FALSE);
  } else if (self->selected == item) {
    self->selected = NULL;
  } else {
    return;
  }
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED_ITEM]);
}

static void
forget_selection_in (AwraRail  *self,
                     GtkWidget *subtree)
{
  if (self->selected != NULL &&
      (GTK_WIDGET (self->selected) == subtree ||
       gtk_widget_is_ancestor (GTK_WIDGET (self->selected), subtree))) {
    self->selected = NULL;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED_ITEM]);
  }
}

static void
adopt_selection_in (AwraRail  *self,
                    GtkWidget *subtree)
{
  /* An entry that arrives selected becomes the selection. */
  if (AWRA_IS_RAIL_ITEM (subtree) && awra_rail_item_get_selected (AWRA_RAIL_ITEM (subtree)))
    awra_rail_item_selection_changed (self, AWRA_RAIL_ITEM (subtree));
  if (AWRA_IS_RAIL_GROUP (subtree)) {
    AwraRailGroup *group = AWRA_RAIL_GROUP (subtree);

    for (guint i = 0; i < awra_rail_group_get_n_items (group); i++) {
      AwraRailItem *item = awra_rail_group_get_item (group, i);

      if (awra_rail_item_get_selected (item))
        awra_rail_item_selection_changed (self, item);
    }
  }
}

/* Keyboard */

static void
collect_focus_targets (GtkWidget *entry,
                       GPtrArray *targets)
{
  if (AWRA_IS_RAIL_ITEM (entry)) {
    g_ptr_array_add (targets, entry);
  } else if (AWRA_IS_RAIL_GROUP (entry)) {
    AwraRailGroup *group = AWRA_RAIL_GROUP (entry);

    g_ptr_array_add (targets, awra_rail_group_get_header (group));
    if (awra_rail_group_get_expanded (group)) {
      for (guint i = 0; i < awra_rail_group_get_n_items (group); i++)
        g_ptr_array_add (targets, awra_rail_group_get_item (group, i));
    }
  }
}

static gboolean
move_focus (AwraRail *self,
            int       direction)
{
  g_autoptr (GPtrArray) targets = g_ptr_array_new ();
  GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));
  GtkWidget *focus = root != NULL ? gtk_root_get_focus (root) : NULL;
  int current = -1;

  if (self->header != NULL)
    collect_focus_targets (self->header, targets);
  for (GtkWidget *child = gtk_widget_get_first_child (list_widget (self)); child != NULL;
       child = gtk_widget_get_next_sibling (child))
    collect_focus_targets (child, targets);
  if (self->footer != NULL)
    collect_focus_targets (self->footer, targets);
  for (guint i = 0; focus != NULL && i < targets->len; i++) {
    GtkWidget *target = g_ptr_array_index (targets, i);

    if (focus == target || gtk_widget_is_ancestor (focus, target)) {
      current = (int) i;
      break;
    }
  }
  if (current < 0 || targets->len == 0)
    return FALSE;
  for (int i = current + direction; i >= 0 && i < (int) targets->len; i += direction) {
    GtkWidget *target = g_ptr_array_index (targets, i);

    if (gtk_widget_is_drawable (target) && gtk_widget_grab_focus (target))
      return TRUE;
  }
  return TRUE;
}

static gboolean
key_pressed_cb (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                AwraRail              *self)
{
  (void) controller;
  (void) keycode;
  if ((state & (GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SHIFT_MASK)) != 0)
    return FALSE;
  switch (keyval) {
    case GDK_KEY_Up:
    case GDK_KEY_KP_Up:
      return move_focus (self, -1);
    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
      return move_focus (self, 1);
    default:
      return FALSE;
  }
}

/* Reordering */

static GtkAdjustment *
vadjustment (AwraRail *self)
{
  return gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->scroller));
}

static double
pointer_list_y (AwraRail *self)
{
  return self->pointer_view_y + gtk_adjustment_get_value (vadjustment (self));
}

static GtkWidget *
entry_at (AwraRail *self,
          double    x,
          double    y)
{
  GtkWidget *picked = gtk_widget_pick (list_widget (self), x, y, GTK_PICK_DEFAULT);

  while (picked != NULL && gtk_widget_get_parent (picked) != list_widget (self))
    picked = gtk_widget_get_parent (picked);
  return picked;
}

static AwraRailItem *
drag_face (GtkWidget *entry)
{
  return AWRA_IS_RAIL_GROUP (entry) ? awra_rail_group_get_header (AWRA_RAIL_GROUP (entry))
                                    : AWRA_RAIL_ITEM (entry);
}

static void
set_merge_target (AwraRail  *self,
                  GtkWidget *target)
{
  if (self->merge_target == target)
    return;
  if (self->merge_target != NULL)
    awra_rail_item_set_drop_target (drag_face (self->merge_target), FALSE);
  self->merge_target = target;
  if (target != NULL)
    awra_rail_item_set_drop_target (drag_face (target), TRUE);
}

/* The top of each entry other than the dragged one, with room for the
 * dragged entry at @drop; returns the list of tops in child order. */
static GArray *
layout_with_gap (AwraRail *self,
                 guint     drop)
{
  GtkWidget *list = list_widget (self);
  double gap = awra_rail_entry_gap (list);
  int dragged_height = entry_height (self->dragged);
  GArray *tops = g_array_new (FALSE, FALSE, sizeof (double));
  double y = gap;
  guint index = 0;

  for (GtkWidget *child = gtk_widget_get_first_child (list); child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    if (child == self->dragged)
      continue;
    if (index == drop)
      y += dragged_height + gap;
    g_array_append_val (tops, y);
    y += entry_height (child) + gap;
    index++;
  }
  return tops;
}

/* Places the dragged entry under the pointer and moves the others to make
 * room at the drop position, or holds them still over a merge target. */
static void
update_drag (AwraRail *self)
{
  GtkWidget *list = list_widget (self);
  double gap = awra_rail_entry_gap (list);
  double tile = awra_rail_tile_size (list);
  int dragged_height = entry_height (self->dragged);
  int list_height = gtk_widget_get_height (list);
  double top = pointer_list_y (self) - self->grab_offset;
  double center;
  double natural;
  double compact;
  guint index;
  guint drop = 0;
  GtkWidget *merge = NULL;
  g_autoptr (GArray) tops = NULL;

  top = CLAMP (top, 0, MAX (0, list_height - dragged_height));
  self->list->dragged_y = top;
  center = top + MIN (dragged_height, tile) / 2.0;
  /* Over the middle of a tile where it stands now, an item merges. */
  if (AWRA_IS_RAIL_ITEM (self->dragged)) {
    tops = layout_with_gap (self, self->drop_index);
    index = 0;
    for (GtkWidget *child = gtk_widget_get_first_child (list); child != NULL;
         child = gtk_widget_get_next_sibling (child)) {
      double tile_center;

      if (child == self->dragged)
        continue;
      tile_center = g_array_index (tops, double, index) + tile / 2.0;
      if (is_entry_type (child) && fabs (center - tile_center) < tile * MERGE_ZONE / 2.0)
        merge = child;
      index++;
    }
    g_clear_pointer (&tops, g_array_unref);
  }
  if (merge != NULL) {
    drop = self->drop_index;
  } else {
    /* Otherwise it slots in after every entry whose middle it passed. */
    compact = gap;
    for (GtkWidget *child = gtk_widget_get_first_child (list); child != NULL;
         child = gtk_widget_get_next_sibling (child)) {
      int height;

      if (child == self->dragged)
        continue;
      height = entry_height (child);
      if (center > compact + MIN (height, tile) / 2.0)
        drop++;
      compact += height + gap;
    }
  }
  self->drop_index = drop;
  set_merge_target (self, merge);
  tops = layout_with_gap (self, drop);
  natural = gap;
  index = 0;
  for (GtkWidget *child = gtk_widget_get_first_child (list); child != NULL;
       child = gtk_widget_get_next_sibling (child)) {
    if (child != self->dragged) {
      move_entry_to (list, child, g_array_index (tops, double, index) - natural);
      index++;
    }
    natural += entry_height (child) + gap;
  }
  gtk_widget_queue_allocate (list);
}

static gboolean
autoscroll_tick (GtkWidget     *widget,
                 GdkFrameClock *clock,
                 gpointer       user_data)
{
  AwraRail *self = AWRA_RAIL (user_data);
  GtkAdjustment *adjustment = vadjustment (self);
  double page = gtk_adjustment_get_page_size (adjustment);
  gint64 now = gdk_frame_clock_get_frame_time (clock);
  double elapsed = self->autoscroll_time > 0 ? (now - self->autoscroll_time) / 1e6 : 0;
  double speed = 0;

  (void) widget;
  self->autoscroll_time = now;
  if (self->pointer_view_y < AUTOSCROLL_EDGE)
    speed = -AUTOSCROLL_SPEED * (1.0 - MAX (self->pointer_view_y, 0.0) / AUTOSCROLL_EDGE);
  else if (self->pointer_view_y > page - AUTOSCROLL_EDGE)
    speed = AUTOSCROLL_SPEED *
            (1.0 - MAX (page - self->pointer_view_y, 0.0) / AUTOSCROLL_EDGE);
  if (speed != 0 && elapsed > 0) {
    gtk_adjustment_set_value (adjustment, gtk_adjustment_get_value (adjustment) + speed * elapsed);
    update_drag (self);
  }
  return G_SOURCE_CONTINUE;
}

static void
stop_autoscroll (AwraRail *self)
{
  if (self->autoscroll_id != 0)
    gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->autoscroll_id);
  self->autoscroll_id = 0;
  self->autoscroll_time = 0;
}

gboolean
awra_rail_is_dragging (AwraRail *self)
{
  return self->dragging;
}

static void
begin_drag (AwraRail *self)
{
  GtkWidget *list = list_widget (self);
  graphene_point_t origin;

  if (!gtk_widget_compute_point (self->pressed_entry, list, &GRAPHENE_POINT_INIT (0, 0), &origin))
    return;
  self->dragged = self->pressed_entry;
  self->dragging = TRUE;
  self->grab_offset = self->press_y - origin.y;
  self->origin_index = entry_index (self, self->dragged);
  self->drop_index = self->origin_index;
  self->list->dragged = self->dragged;
  self->list->dragged_y = origin.y;
  gtk_gesture_set_state (self->drag, GTK_EVENT_SEQUENCE_CLAIMED);
  awra_rail_item_set_lifted (drag_face (self->dragged), TRUE);
  self->autoscroll_id = gtk_widget_add_tick_callback (GTK_WIDGET (self), autoscroll_tick,
                                                      self, NULL);
}

static void
end_drag (AwraRail *self,
          gboolean  commit)
{
  GtkWidget *list = list_widget (self);
  GtkWidget *dragged = self->dragged;
  GtkWidget *merge = self->merge_target;
  guint drop = self->drop_index;
  double visual = self->list->dragged_y;
  gboolean moved = FALSE;
  g_autoptr (GArray) visuals = g_array_new (FALSE, FALSE, sizeof (double));

  stop_autoscroll (self);
  self->dragging = FALSE;
  self->dragged = NULL;
  self->pressed_entry = NULL;
  if (dragged == NULL)
    return;
  g_object_ref (dragged);
  if (merge != NULL)
    g_object_ref (merge);
  set_merge_target (self, NULL);
  awra_rail_item_set_lifted (drag_face (dragged), FALSE);
  self->list->dragged = NULL;
  {
    double gap = awra_rail_entry_gap (list);
    double y = gap;
    guint index = 0;

    for (GtkWidget *child = gtk_widget_get_first_child (list); child != NULL;
         child = gtk_widget_get_next_sibling (child), index++) {
      double top = child == dragged ? visual : y + entry_motion (child)->offset;

      g_array_append_val (visuals, top);
      entry_motion (child)->old_index = index;
      y += entry_height (child) + gap;
    }
  }
  if (commit && merge == NULL && drop != self->origin_index) {
    GtkWidget *sibling = NULL;
    guint index = 0;

    for (GtkWidget *child = gtk_widget_get_first_child (list); child != NULL;
         child = gtk_widget_get_next_sibling (child)) {
      if (child == dragged)
        continue;
      if (index == drop) {
        sibling = child;
        break;
      }
      index++;
    }
    gtk_widget_insert_before (dragged, list, sibling);
    moved = TRUE;
  }
  {
    double gap = awra_rail_entry_gap (list);
    double y = gap;

    /* Keep every entry where it is on screen; the springs then settle it
     * into its slot in the new order. */
    for (GtkWidget *child = gtk_widget_get_first_child (list); child != NULL;
         child = gtk_widget_get_next_sibling (child)) {
      double visual = g_array_index (visuals, double, entry_motion (child)->old_index);

      jump_entry_to (child, visual - y);
      move_entry_to (list, child, 0);
      y += entry_height (child) + gap;
    }
  }
  gtk_widget_queue_allocate (list);
  if (commit && merge != NULL)
    g_signal_emit (self, signals[SIGNAL_ITEM_MERGED], 0, dragged, merge);
  else if (moved)
    g_signal_emit (self, signals[SIGNAL_ITEM_MOVED], 0, dragged, entry_index (self, dragged));
  g_object_unref (dragged);
  g_clear_object (&merge);
}

static void
drag_begin_cb (GtkGestureDrag *gesture,
               double          x,
               double          y,
               AwraRail       *self)
{
  GtkWidget *entry;

  self->pressed_entry = NULL;
  if (!self->reorderable) {
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
    return;
  }
  entry = entry_at (self, x, y);
  if (entry == NULL || !is_entry_type (entry)) {
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
    return;
  }
  if (AWRA_IS_RAIL_GROUP (entry)) {
    graphene_point_t point;
    GtkWidget *header = GTK_WIDGET (awra_rail_group_get_header (AWRA_RAIL_GROUP (entry)));

    /* A group is dragged by its own tile, not by one of its items. */
    if (!gtk_widget_compute_point (list_widget (self), header, &GRAPHENE_POINT_INIT (x, y),
                                   &point) ||
        !gtk_widget_contains (header, point.x, point.y)) {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }
  }
  self->pressed_entry = entry;
  self->press_x = x;
  self->press_y = y;
  self->pointer_view_y = y - gtk_adjustment_get_value (vadjustment (self));
}

static void
drag_update_cb (GtkGestureDrag *gesture,
                double          offset_x,
                double          offset_y,
                AwraRail       *self)
{
  (void) gesture;
  if (self->pressed_entry == NULL)
    return;
  /* The pointer, kept in viewport coordinates so autoscroll can move the
   * list under it. */
  self->pointer_view_y = self->press_y + offset_y -
                         gtk_adjustment_get_value (vadjustment (self));
  if (!self->dragging) {
    if (!gtk_drag_check_threshold (list_widget (self), 0, 0, (int) offset_x, (int) offset_y))
      return;
    begin_drag (self);
  }
  if (self->dragging) {
    /* Offsets are relative to the press, in list coordinates that move as
     * the list scrolls: recompute the viewport position from the press. */
    update_drag (self);
  }
}

static void
drag_end_cb (GtkGestureDrag *gesture,
             double          offset_x,
             double          offset_y,
             AwraRail       *self)
{
  (void) gesture;
  (void) offset_x;
  (void) offset_y;
  if (self->dragging)
    end_drag (self, TRUE);
  self->pressed_entry = NULL;
}

static void
drag_cancel_cb (GtkGesture       *gesture,
                GdkEventSequence *sequence,
                AwraRail         *self)
{
  (void) gesture;
  (void) sequence;
  if (self->dragging)
    end_drag (self, FALSE);
  self->pressed_entry = NULL;
}

/* Lifecycle */

static void
awra_rail_unmap (GtkWidget *widget)
{
  AwraRail *self = AWRA_RAIL (widget);

  if (self->dragging)
    end_drag (self, FALSE);
  GTK_WIDGET_CLASS (awra_rail_parent_class)->unmap (widget);
}

static void
awra_rail_dispose (GObject *object)
{
  AwraRail *self = AWRA_RAIL (object);

  stop_autoscroll (self);
  self->dragged = NULL;
  self->pressed_entry = NULL;
  self->merge_target = NULL;
  self->selected = NULL;
  g_clear_pointer (&self->header, gtk_widget_unparent);
  g_clear_pointer (&self->footer, gtk_widget_unparent);
  g_clear_pointer (&self->scroller, gtk_widget_unparent);
  self->list = NULL;
  G_OBJECT_CLASS (awra_rail_parent_class)->dispose (object);
}

static void
awra_rail_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  AwraRail *self = AWRA_RAIL (object);

  switch (prop_id) {
    case PROP_SELECTED_ITEM: g_value_set_object (value, self->selected); break;
    case PROP_HEADER: g_value_set_object (value, self->header); break;
    case PROP_FOOTER: g_value_set_object (value, self->footer); break;
    case PROP_REORDERABLE: g_value_set_boolean (value, self->reorderable); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_rail_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  AwraRail *self = AWRA_RAIL (object);

  switch (prop_id) {
    case PROP_SELECTED_ITEM: awra_rail_set_selected_item (self, g_value_get_object (value)); break;
    case PROP_HEADER: awra_rail_set_header (self, g_value_get_object (value)); break;
    case PROP_FOOTER: awra_rail_set_footer (self, g_value_get_object (value)); break;
    case PROP_REORDERABLE: awra_rail_set_reorderable (self, g_value_get_boolean (value)); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_rail_class_init (AwraRailClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  const GParamFlags flags = G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY |
                            G_PARAM_STATIC_STRINGS;

  object_class->dispose = awra_rail_dispose;
  object_class->get_property = awra_rail_get_property;
  object_class->set_property = awra_rail_set_property;
  widget_class->unmap = awra_rail_unmap;

  /**
   * AwraRail:selected-item:
   *
   * The selected item, in the rail or in one of its groups.
   *
   * Since: 1.1
   */
  properties[PROP_SELECTED_ITEM] =
    g_param_spec_object ("selected-item", NULL, NULL, AWRA_TYPE_RAIL_ITEM, flags);
  /**
   * AwraRail:header:
   *
   * A widget that stays above the entries.
   *
   * Since: 1.1
   */
  properties[PROP_HEADER] = g_param_spec_object ("header", NULL, NULL, GTK_TYPE_WIDGET, flags);
  /**
   * AwraRail:footer:
   *
   * A widget that stays below the entries.
   *
   * Since: 1.1
   */
  properties[PROP_FOOTER] = g_param_spec_object ("footer", NULL, NULL, GTK_TYPE_WIDGET, flags);
  /**
   * AwraRail:reorderable:
   *
   * Whether the user can drag entries to reorder them.
   *
   * Since: 1.1
   */
  properties[PROP_REORDERABLE] =
    g_param_spec_boolean ("reorderable", NULL, NULL, FALSE, flags);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * AwraRail::item-moved:
   * @self: the rail
   * @item: the item or group the user moved
   * @position: its new position among the entries
   *
   * Emitted after the user dragged an entry to another place. The rail has
   * already moved it.
   *
   * Since: 1.1
   */
  signals[SIGNAL_ITEM_MOVED] =
    g_signal_new ("item-moved", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL, G_TYPE_NONE, 2, GTK_TYPE_WIDGET, G_TYPE_UINT);
  /**
   * AwraRail::item-merged:
   * @self: the rail
   * @item: the item the user dropped
   * @onto: the item or group it was dropped onto
   *
   * Emitted when the user dropped an item onto another item or onto a group.
   * The rail changes nothing: the application decides, for instance, to make
   * a folder of the two items or to add the item to the group.
   *
   * Since: 1.1
   */
  signals[SIGNAL_ITEM_MERGED] =
    g_signal_new ("item-merged", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, NULL, G_TYPE_NONE, 2, GTK_TYPE_WIDGET, GTK_TYPE_WIDGET);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awrarail");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TAB_LIST);
}

static void
awra_rail_init (AwraRail *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkEventController *keys = gtk_event_controller_key_new ();

  gtk_orientable_set_orientation (GTK_ORIENTABLE (gtk_widget_get_layout_manager (widget)),
                                  GTK_ORIENTATION_VERTICAL);
  self->scroller = gtk_scrolled_window_new ();
  /* A rail scrolls by wheel, touch and focus; a scrollbar would only
   * crowd the tiles. */
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scroller), GTK_POLICY_NEVER,
                                  GTK_POLICY_EXTERNAL);
  gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (self->scroller), TRUE);
  gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (self->scroller), TRUE);
  gtk_widget_set_vexpand (self->scroller, TRUE);
  self->list = g_object_new (AWRA_TYPE_RAIL_LIST, NULL);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->scroller), GTK_WIDGET (self->list));
  gtk_widget_set_parent (self->scroller, widget);
  self->drag = gtk_gesture_drag_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->drag), GDK_BUTTON_PRIMARY);
  g_signal_connect (self->drag, "drag-begin", G_CALLBACK (drag_begin_cb), self);
  g_signal_connect (self->drag, "drag-update", G_CALLBACK (drag_update_cb), self);
  g_signal_connect (self->drag, "drag-end", G_CALLBACK (drag_end_cb), self);
  g_signal_connect (self->drag, "cancel", G_CALLBACK (drag_cancel_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self->list), GTK_EVENT_CONTROLLER (self->drag));
  g_signal_connect (keys, "key-pressed", G_CALLBACK (key_pressed_cb), self);
  gtk_widget_add_controller (widget, keys);
  gtk_accessible_update_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_ORIENTATION,
                                  GTK_ORIENTATION_VERTICAL, -1);
}

GtkWidget *
awra_rail_new (void)
{
  return g_object_new (AWRA_TYPE_RAIL, NULL);
}

void
awra_rail_append (AwraRail  *self,
                  GtkWidget *widget)
{
  awra_rail_insert (self, widget, -1);
}

void
awra_rail_prepend (AwraRail  *self,
                   GtkWidget *widget)
{
  awra_rail_insert (self, widget, 0);
}

static void
insert_entry (AwraRail  *self,
              GtkWidget *widget,
              int        position)
{
  GtkWidget *sibling = NULL;

  if (position >= 0) {
    int index = 0;

    for (sibling = gtk_widget_get_first_child (list_widget (self));
         sibling != NULL && index < position;
         sibling = gtk_widget_get_next_sibling (sibling))
      index++;
  }
  g_object_set_qdata_full (G_OBJECT (widget), motion_quark, g_new0 (EntryMotion, 1),
                           entry_motion_free);
  gtk_widget_insert_before (widget, list_widget (self), sibling);
}

void
awra_rail_insert (AwraRail  *self,
                  GtkWidget *widget,
                  int        position)
{
  g_return_if_fail (AWRA_IS_RAIL (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (is_entry_type (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);
  insert_entry (self, widget, position);
  adopt_selection_in (self, widget);
}

void
awra_rail_remove (AwraRail  *self,
                  GtkWidget *widget)
{
  g_return_if_fail (AWRA_IS_RAIL (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  if (gtk_widget_get_parent (widget) != list_widget (self)) {
    g_critical ("%s: the widget is not an entry of this rail", G_STRFUNC);
    return;
  }
  if (self->dragging && (self->dragged == widget || self->merge_target == widget))
    end_drag (self, FALSE);
  forget_selection_in (self, widget);
  g_object_set_qdata (G_OBJECT (widget), motion_quark, NULL);
  gtk_widget_unparent (widget);
}

GtkWidget *
awra_rail_add_separator (AwraRail *self)
{
  GtkWidget *separator;

  g_return_val_if_fail (AWRA_IS_RAIL (self), NULL);
  separator = g_object_new (AWRA_TYPE_RAIL_SEPARATOR, NULL);
  insert_entry (self, separator, -1);
  return separator;
}

guint
awra_rail_get_n_entries (AwraRail *self)
{
  guint n = 0;

  g_return_val_if_fail (AWRA_IS_RAIL (self), 0);
  for (GtkWidget *child = gtk_widget_get_first_child (list_widget (self)); child != NULL;
       child = gtk_widget_get_next_sibling (child))
    n++;
  return n;
}

GtkWidget *
awra_rail_get_entry (AwraRail *self,
                     guint     position)
{
  guint index = 0;

  g_return_val_if_fail (AWRA_IS_RAIL (self), NULL);
  for (GtkWidget *child = gtk_widget_get_first_child (list_widget (self)); child != NULL;
       child = gtk_widget_get_next_sibling (child), index++) {
    if (index == position)
      return child;
  }
  return NULL;
}

AwraRailItem *
awra_rail_get_selected_item (AwraRail *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL (self), NULL);
  return self->selected;
}

void
awra_rail_set_selected_item (AwraRail     *self,
                             AwraRailItem *item)
{
  g_return_if_fail (AWRA_IS_RAIL (self));
  g_return_if_fail (item == NULL || AWRA_IS_RAIL_ITEM (item));
  g_return_if_fail (item == NULL || gtk_widget_is_ancestor (GTK_WIDGET (item), GTK_WIDGET (self)));
  if (self->selected == item)
    return;
  if (item != NULL) {
    /* The item tells the rail, which deselects the previous one. */
    awra_rail_item_set_selected (item, TRUE);
    return;
  }
  awra_rail_item_set_selected (self->selected, FALSE);
}

GtkWidget *
awra_rail_get_header (AwraRail *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL (self), NULL);
  return self->header;
}

static void
set_fixed_child (AwraRail   *self,
                 GtkWidget **slot,
                 GtkWidget  *widget,
                 gboolean    before_scroller,
                 GParamSpec *pspec)
{
  if (*slot == widget)
    return;
  if (*slot != NULL) {
    forget_selection_in (self, *slot);
    gtk_widget_unparent (*slot);
  }
  *slot = widget;
  if (widget != NULL) {
    if (before_scroller)
      gtk_widget_insert_before (widget, GTK_WIDGET (self), self->scroller);
    else
      gtk_widget_insert_after (widget, GTK_WIDGET (self), self->scroller);
    adopt_selection_in (self, widget);
  }
  g_object_notify_by_pspec (G_OBJECT (self), pspec);
}

void
awra_rail_set_header (AwraRail  *self,
                      GtkWidget *header)
{
  g_return_if_fail (AWRA_IS_RAIL (self));
  g_return_if_fail (header == NULL || GTK_IS_WIDGET (header));
  set_fixed_child (self, &self->header, header, TRUE, properties[PROP_HEADER]);
}

GtkWidget *
awra_rail_get_footer (AwraRail *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL (self), NULL);
  return self->footer;
}

void
awra_rail_set_footer (AwraRail  *self,
                      GtkWidget *footer)
{
  g_return_if_fail (AWRA_IS_RAIL (self));
  g_return_if_fail (footer == NULL || GTK_IS_WIDGET (footer));
  set_fixed_child (self, &self->footer, footer, FALSE, properties[PROP_FOOTER]);
}

gboolean
awra_rail_get_reorderable (AwraRail *self)
{
  g_return_val_if_fail (AWRA_IS_RAIL (self), FALSE);
  return self->reorderable;
}

void
awra_rail_set_reorderable (AwraRail *self,
                           gboolean  reorderable)
{
  g_return_if_fail (AWRA_IS_RAIL (self));
  reorderable = !!reorderable;
  if (self->reorderable == reorderable)
    return;
  self->reorderable = reorderable;
  if (!reorderable && self->dragging)
    end_drag (self, FALSE);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REORDERABLE]);
}
