/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <math.h>

#include <awra/awra-context.h>
#include <awra/awra-motion.h>
#include <awra/awra-tile-view.h>

/* Thickness of what the pointer can grab, however thin the visible line. */
#define DIVIDER_REACH 9
/* An arrangement is written by the application, but read back from a file. */
#define ARRANGEMENT_DEPTH 32

typedef struct _Tile Tile;

struct _Tile {
  Tile *parent;
  double share;
  GdkRectangle bounds;

  /* A leaf holds a child... */
  GtkWidget *child;
  GdkRectangle from;
  GdkRectangle shown;
  double fade_from;
  gboolean arriving;
  gboolean lifted;

  /* ...and a branch divides its space along its orientation. */
  GtkOrientation orientation;
  GPtrArray *tiles;
  GPtrArray *dividers;
};

typedef struct {
  GdkPaintable *frame;
  GdkRectangle bounds;
  double fade_from;
} Ghost;

struct _AwraTileView {
  GtkWidget parent_instance;

  Tile *root;
  int column_spacing;
  int row_spacing;

  /* The change in progress. */
  guint tick;
  gint64 started;
  double eased;
  GPtrArray *ghosts;

  /* The divider in the pointer's hand. */
  Tile *grip;
  guint grip_index;
  double grip_before;
  double grip_after;
  double grip_length;
};

enum {
  PROP_0,
  PROP_COLUMN_SPACING,
  PROP_ROW_SPACING,
  N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (AwraTileView, awra_tile_view, GTK_TYPE_WIDGET)

static gboolean
is_horizontal (GtkPositionType side)
{
  return side == GTK_POS_LEFT || side == GTK_POS_RIGHT;
}

static gboolean
comes_first (GtkPositionType side)
{
  return side == GTK_POS_LEFT || side == GTK_POS_TOP;
}

static int
spacing_of (AwraTileView *self,
            Tile         *branch)
{
  return branch->orientation == GTK_ORIENTATION_HORIZONTAL
           ? self->column_spacing
           : self->row_spacing;
}

static int
extent (const GdkRectangle *area,
        GtkOrientation      orientation)
{
  return orientation == GTK_ORIENTATION_HORIZONTAL ? area->width : area->height;
}

static Tile *
tile_at (Tile *branch,
         guint index)
{
  return g_ptr_array_index (branch->tiles, index);
}

static Tile *
leaf_new (GtkWidget *child)
{
  Tile *leaf = g_new0 (Tile, 1);

  leaf->child = child;
  leaf->share = 1.0;
  return leaf;
}

static GtkWidget *
divider_new (AwraTileView  *self,
             GtkOrientation orientation);

static Tile *
branch_new (GtkOrientation orientation)
{
  Tile *branch = g_new0 (Tile, 1);

  branch->orientation = orientation;
  branch->tiles = g_ptr_array_new ();
  branch->dividers = g_ptr_array_new ();
  return branch;
}

static void
add_tile (AwraTileView *self,
          Tile         *branch,
          Tile         *tile,
          guint         index)
{
  tile->parent = branch;
  g_ptr_array_insert (branch->tiles, (int) index, tile);
  if (branch->tiles->len > 1)
    g_ptr_array_add (branch->dividers, divider_new (self, branch->orientation));
}

static void
drop_divider (Tile *branch)
{
  GtkWidget *divider = g_ptr_array_steal_index (branch->dividers,
                                                branch->dividers->len - 1);

  gtk_widget_unparent (divider);
}

/* Frees a branch and what it holds, but neither the children nor their leaves. */
static void
free_branches (Tile *tile)
{
  if (tile->child != NULL)
    return;
  for (guint i = 0; i < tile->tiles->len; i++)
    free_branches (tile_at (tile, i));
  while (tile->dividers->len > 0)
    drop_divider (tile);
  g_ptr_array_unref (tile->tiles);
  g_ptr_array_unref (tile->dividers);
  g_free (tile);
}

static void
replace_tile (AwraTileView *self,
              Tile         *old,
              Tile         *tile)
{
  Tile *parent = old->parent;

  tile->parent = parent;
  tile->share = old->share;
  if (parent == NULL)
    {
      self->root = tile;
      return;
    }
  for (guint i = 0; i < parent->tiles->len; i++)
    if (tile_at (parent, i) == old)
      parent->tiles->pdata[i] = tile;
}

static guint
index_in_parent (Tile *tile)
{
  guint index = 0;

  g_ptr_array_find (tile->parent->tiles, tile, &index);
  return index;
}

static void
collect_leaves (Tile       *tile,
                GPtrArray  *leaves)
{
  if (tile == NULL)
    return;
  if (tile->child != NULL)
    {
      g_ptr_array_add (leaves, tile);
      return;
    }
  for (guint i = 0; i < tile->tiles->len; i++)
    collect_leaves (tile_at (tile, i), leaves);
}

static GPtrArray *
leaves_of (AwraTileView *self)
{
  GPtrArray *leaves = g_ptr_array_new ();

  collect_leaves (self->root, leaves);
  return leaves;
}

static Tile *
find_leaf (AwraTileView *self,
           GtkWidget    *child)
{
  g_autoptr (GPtrArray) leaves = leaves_of (self);

  for (guint i = 0; i < leaves->len; i++)
    if (((Tile *) g_ptr_array_index (leaves, i))->child == child)
      return g_ptr_array_index (leaves, i);
  return NULL;
}

static gboolean
find_divider (Tile      *tile,
              GtkWidget *divider,
              Tile     **branch,
              guint     *index)
{
  if (tile == NULL || tile->child != NULL)
    return FALSE;
  if (g_ptr_array_find (tile->dividers, divider, index))
    {
      *branch = tile;
      return TRUE;
    }
  for (guint i = 0; i < tile->tiles->len; i++)
    if (find_divider (tile_at (tile, i), divider, branch, index))
      return TRUE;
  return FALSE;
}

static void
measure_tile (AwraTileView  *self,
              Tile          *tile,
              GtkOrientation orientation,
              int           *minimum,
              int           *natural)
{
  gboolean along;

  if (tile->child != NULL)
    {
      gtk_widget_measure (tile->child, orientation, -1, minimum, natural,
                          NULL, NULL);
      return;
    }
  along = tile->orientation == orientation;
  *minimum = *natural = along
                          ? spacing_of (self, tile) * ((int) tile->tiles->len - 1)
                          : 0;
  for (guint i = 0; i < tile->tiles->len; i++)
    {
      int part_minimum = 0;
      int part_natural = 0;

      measure_tile (self, tile_at (tile, i), orientation,
                    &part_minimum, &part_natural);
      *minimum = along ? *minimum + part_minimum : MAX (*minimum, part_minimum);
      *natural = along ? *natural + part_natural : MAX (*natural, part_natural);
    }
}

static int
minimum_of (AwraTileView  *self,
            Tile          *tile,
            GtkOrientation orientation)
{
  int minimum = 0;
  int natural = 0;

  measure_tile (self, tile, orientation, &minimum, &natural);
  return minimum;
}

/* Each tile gets its share, and those too small for it their minimum. */
static void
fit_sizes (AwraTileView *self,
           Tile         *branch,
           double        length,
           double       *sizes)
{
  guint count = branch->tiles->len;
  g_autofree gboolean *held = g_new0 (gboolean, count);
  gboolean changed = TRUE;

  while (changed)
    {
      double rest = length;
      double weight = 0.0;

      changed = FALSE;
      for (guint i = 0; i < count; i++)
        {
          if (held[i])
            rest -= sizes[i];
          else
            weight += tile_at (branch, i)->share;
        }
      for (guint i = 0; i < count && !changed; i++)
        {
          int minimum;

          if (held[i])
            continue;
          sizes[i] = rest * tile_at (branch, i)->share / weight;
          minimum = minimum_of (self, tile_at (branch, i), branch->orientation);
          if (sizes[i] < minimum)
            {
              sizes[i] = minimum;
              held[i] = changed = TRUE;
            }
        }
    }
}

static void
arrange_tile (AwraTileView       *self,
              Tile               *tile,
              const GdkRectangle *area)
{
  gboolean columns;
  int spacing;
  int length;
  double edge = 0.0;
  g_autofree double *sizes = NULL;

  tile->bounds = *area;
  if (tile->child != NULL)
    return;
  columns = tile->orientation == GTK_ORIENTATION_HORIZONTAL;
  spacing = spacing_of (self, tile);
  length = extent (area, tile->orientation)
           - spacing * ((int) tile->tiles->len - 1);
  sizes = g_new0 (double, tile->tiles->len);
  fit_sizes (self, tile, length, sizes);
  for (guint i = 0; i < tile->tiles->len; i++)
    {
      GdkRectangle part = *area;
      int start = (int) lround (edge);
      int end = i + 1 == tile->tiles->len ? length
                                          : (int) lround (edge + sizes[i]);

      edge += sizes[i];
      if (columns)
        {
          part.x += start + spacing * (int) i;
          part.width = end - start;
        }
      else
        {
          part.y += start + spacing * (int) i;
          part.height = end - start;
        }
      arrange_tile (self, tile_at (tile, i), &part);
    }
}

static void
arrange (AwraTileView *self)
{
  GdkRectangle area = { 0, 0,
                        gtk_widget_get_width (GTK_WIDGET (self)),
                        gtk_widget_get_height (GTK_WIDGET (self)) };

  if (self->root != NULL)
    arrange_tile (self, self->root, &area);
}

static void
ghost_free (gpointer data)
{
  Ghost *ghost = data;

  g_object_unref (ghost->frame);
  g_free (ghost);
}

static void
settle (AwraTileView *self)
{
  g_autoptr (GPtrArray) leaves = leaves_of (self);

  if (self->tick != 0)
    gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick);
  self->tick = 0;
  self->started = 0;
  self->eased = 1.0;
  g_ptr_array_set_size (self->ghosts, 0);
  for (guint i = 0; i < leaves->len; i++)
    {
      Tile *leaf = g_ptr_array_index (leaves, i);

      leaf->arriving = leaf->lifted = FALSE;
    }
  gtk_widget_queue_allocate (GTK_WIDGET (self));
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static AwraStyleManager *
style_of (AwraTileView *self)
{
  GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (self));

  return awra_context_get_style_manager (awra_context_get_for_display (display));
}

static gboolean
can_animate (AwraTileView *self)
{
  return gtk_widget_get_mapped (GTK_WIDGET (self)) &&
         awra_motion_get_duration (style_of (self), AWRA_MOTION_PRESET_NORMAL) > 0;
}

static gboolean
tick_cb (GtkWidget     *widget,
         GdkFrameClock *clock,
         gpointer       user_data)
{
  AwraTileView *self = AWRA_TILE_VIEW (widget);
  gint64 now = gdk_frame_clock_get_frame_time (clock);
  double duration = awra_motion_get_duration (style_of (self),
                                              AWRA_MOTION_PRESET_NORMAL);
  double progress;

  (void) user_data;
  /* From the first frame: building what arrives takes no time off the motion. */
  if (self->started == 0)
    self->started = now;
  progress = duration > 0 ? (now - self->started) / (duration * 1000.0) : 1.0;
  if (progress >= 1.0)
    {
      self->tick = 0;
      settle (self);
      return G_SOURCE_REMOVE;
    }
  self->eased = awra_motion_ease_out_cubic (progress);
  gtk_widget_queue_allocate (widget);
  gtk_widget_queue_draw (widget);
  return G_SOURCE_CONTINUE;
}

/* What is on screen now is where the next change starts from. */
static void
hold_still (AwraTileView *self)
{
  g_autoptr (GPtrArray) leaves = leaves_of (self);

  self->grip = NULL;
  for (guint i = 0; i < leaves->len; i++)
    {
      Tile *leaf = g_ptr_array_index (leaves, i);

      leaf->from = leaf->shown;
      if (leaf->arriving)
        leaf->fade_from += (1.0 - leaf->fade_from) * self->eased;
    }
  for (guint i = 0; i < self->ghosts->len; i++)
    {
      Ghost *ghost = g_ptr_array_index (self->ghosts, i);

      ghost->fade_from *= 1.0 - self->eased;
    }
  self->eased = 0.0;
  self->started = 0;
}

static void
apply_change (AwraTileView *self)
{
  gtk_widget_queue_resize (GTK_WIDGET (self));
  arrange (self);
  if (!can_animate (self))
    {
      settle (self);
      return;
    }
  if (self->tick == 0)
    self->tick = gtk_widget_add_tick_callback (GTK_WIDGET (self), tick_cb,
                                               NULL, NULL);
}

static void
attach_tile (AwraTileView    *self,
             Tile            *leaf,
             GtkWidget       *beside,
             GtkPositionType  side)
{
  GtkOrientation orientation = is_horizontal (side) ? GTK_ORIENTATION_HORIZONTAL
                                                    : GTK_ORIENTATION_VERTICAL;
  Tile *anchor = beside != NULL ? find_leaf (self, beside) : self->root;
  Tile *branch;

  leaf->share = 1.0;
  if (anchor == NULL)
    {
      leaf->parent = NULL;
      self->root = leaf;
      return;
    }
  if (beside == NULL && anchor->child == NULL && anchor->orientation == orientation)
    {
      /* A new tile along the whole row: everyone makes some room. */
      double kept = anchor->tiles->len / (anchor->tiles->len + 1.0);

      for (guint i = 0; i < anchor->tiles->len; i++)
        tile_at (anchor, i)->share *= kept;
      leaf->share = 1.0 - kept;
      add_tile (self, anchor, leaf, comes_first (side) ? 0 : anchor->tiles->len);
      return;
    }
  if (anchor->parent != NULL && anchor->parent->orientation == orientation)
    {
      guint index = index_in_parent (anchor);

      anchor->share /= 2.0;
      leaf->share = anchor->share;
      add_tile (self, anchor->parent, leaf, comes_first (side) ? index : index + 1);
      return;
    }
  branch = branch_new (orientation);
  replace_tile (self, anchor, branch);
  anchor->share = leaf->share = 0.5;
  add_tile (self, branch, comes_first (side) ? leaf : anchor, 0);
  add_tile (self, branch, comes_first (side) ? anchor : leaf, 1);
}

static void
collapse_branch (AwraTileView *self,
                 Tile         *branch)
{
  Tile *only = tile_at (branch, 0);
  Tile *parent = branch->parent;

  replace_tile (self, branch, only);
  g_ptr_array_set_size (branch->tiles, 0);
  free_branches (branch);
  if (parent == NULL || only->child != NULL ||
      only->orientation != parent->orientation)
    return;

  /* A row inside a row is one row: its tiles join the parent's. */
  {
    guint index = index_in_parent (only);

    g_ptr_array_remove_index (parent->tiles, index);
    for (guint i = 0; i < only->tiles->len; i++)
      {
        Tile *tile = tile_at (only, i);

        tile->share *= only->share;
        tile->parent = parent;
        g_ptr_array_insert (parent->tiles, (int) (index + i), tile);
      }
    while (only->dividers->len > 0)
      g_ptr_array_add (parent->dividers,
                       g_ptr_array_steal_index (only->dividers, 0));
    g_ptr_array_set_size (only->tiles, 0);
    free_branches (only);
  }
}

/* Takes a leaf out of the tree, without touching its child. */
static void
detach_tile (AwraTileView *self,
             Tile         *leaf)
{
  Tile *parent = leaf->parent;

  if (parent == NULL)
    {
      self->root = NULL;
      return;
    }
  g_ptr_array_remove (parent->tiles, leaf);
  drop_divider (parent);
  for (guint i = 0; i < parent->tiles->len; i++)
    tile_at (parent, i)->share /= 1.0 - leaf->share;
  leaf->parent = NULL;
  if (parent->tiles->len == 1)
    collapse_branch (self, parent);
}

static void
divider_pressed_cb (GtkGestureClick *gesture,
                    int              count,
                    double           x,
                    double           y,
                    AwraTileView    *self)
{
  GtkWidget *divider = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  Tile *branch = NULL;
  guint index = 0;

  (void) x;
  (void) y;
  if (count != 2 || !find_divider (self->root, divider, &branch, &index))
    return;
  hold_still (self);
  for (guint i = 0; i < branch->tiles->len; i++)
    tile_at (branch, i)->share = 1.0 / branch->tiles->len;
  apply_change (self);
}

static void
divider_drag_begin_cb (GtkGestureDrag *gesture,
                       double          x,
                       double          y,
                       AwraTileView   *self)
{
  GtkWidget *divider = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  Tile *branch = NULL;
  guint index = 0;
  double length = 0.0;

  (void) x;
  (void) y;
  if (!find_divider (self->root, divider, &branch, &index))
    return;
  settle (self);
  arrange (self);
  /* Shares say what is on screen, or the others would move with these two. */
  for (guint i = 0; i < branch->tiles->len; i++)
    length += extent (&tile_at (branch, i)->bounds, branch->orientation);
  for (guint i = 0; i < branch->tiles->len; i++)
    tile_at (branch, i)->share =
      extent (&tile_at (branch, i)->bounds, branch->orientation) / length;
  self->grip = branch;
  self->grip_index = index;
  self->grip_length = length;
  self->grip_before = extent (&tile_at (branch, index)->bounds, branch->orientation);
  self->grip_after = extent (&tile_at (branch, index + 1)->bounds, branch->orientation);
}

static void
divider_drag_update_cb (GtkGestureDrag *gesture,
                        double          offset_x,
                        double          offset_y,
                        AwraTileView   *self)
{
  Tile *branch = self->grip;
  double offset;
  double pair;
  double before;
  double lowest;
  double highest;

  (void) gesture;
  if (branch == NULL)
    return;
  offset = branch->orientation == GTK_ORIENTATION_HORIZONTAL ? offset_x : offset_y;
  pair = self->grip_before + self->grip_after;
  lowest = minimum_of (self, tile_at (branch, self->grip_index), branch->orientation);
  highest = pair - minimum_of (self, tile_at (branch, self->grip_index + 1),
                               branch->orientation);
  before = MAX (lowest, MIN (self->grip_before + offset, highest));
  tile_at (branch, self->grip_index)->share = before / self->grip_length;
  tile_at (branch, self->grip_index + 1)->share = (pair - before) / self->grip_length;
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
divider_drag_end_cb (GtkGestureDrag *gesture,
                     double          offset_x,
                     double          offset_y,
                     AwraTileView   *self)
{
  (void) gesture;
  (void) offset_x;
  (void) offset_y;
  self->grip = NULL;
}

static GtkWidget *
divider_new (AwraTileView  *self,
             GtkOrientation orientation)
{
  gboolean columns = orientation == GTK_ORIENTATION_HORIZONTAL;
  GtkWidget *divider = gtk_box_new (orientation, 0);
  GtkWidget *line = gtk_separator_new (columns ? GTK_ORIENTATION_VERTICAL
                                               : GTK_ORIENTATION_HORIZONTAL);
  GtkGesture *click = gtk_gesture_click_new ();
  GtkGesture *drag = gtk_gesture_drag_new ();

  gtk_widget_add_css_class (divider, "awra-tile-divider");
  gtk_widget_set_cursor_from_name (divider, columns ? "col-resize" : "row-resize");
  gtk_widget_set_halign (line, columns ? GTK_ALIGN_CENTER : GTK_ALIGN_FILL);
  gtk_widget_set_valign (line, columns ? GTK_ALIGN_FILL : GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (line, TRUE);
  gtk_widget_set_vexpand (line, TRUE);
  gtk_box_append (GTK_BOX (divider), line);
  g_signal_connect (click, "pressed", G_CALLBACK (divider_pressed_cb), self);
  gtk_widget_add_controller (divider, GTK_EVENT_CONTROLLER (click));
  g_signal_connect (drag, "drag-begin", G_CALLBACK (divider_drag_begin_cb), self);
  g_signal_connect (drag, "drag-update", G_CALLBACK (divider_drag_update_cb), self);
  g_signal_connect (drag, "drag-end", G_CALLBACK (divider_drag_end_cb), self);
  gtk_widget_add_controller (divider, GTK_EVENT_CONTROLLER (drag));
  /* Last in line, so that dividers are above the tiles they reach over. */
  gtk_widget_set_parent (divider, GTK_WIDGET (self));
  return divider;
}

static int
mix (int    from,
     int    to,
     double progress)
{
  return (int) lround (from + (to - from) * progress);
}

static void
shown_bounds (Tile         *tile,
              GdkRectangle *bounds)
{
  if (tile->child != NULL)
    {
      *bounds = tile->shown;
      return;
    }
  shown_bounds (tile_at (tile, 0), bounds);
  for (guint i = 1; i < tile->tiles->len; i++)
    {
      GdkRectangle part;

      shown_bounds (tile_at (tile, i), &part);
      gdk_rectangle_union (bounds, &part, bounds);
    }
}

static void
allocate_divider (AwraTileView *self,
                  Tile         *branch,
                  guint         index)
{
  GtkWidget *divider = g_ptr_array_index (branch->dividers, index);
  int reach = MAX (DIVIDER_REACH, spacing_of (self, branch));
  GdkRectangle before;
  GdkRectangle after;
  GdkRectangle both;
  GtkAllocation box;

  shown_bounds (tile_at (branch, index), &before);
  shown_bounds (tile_at (branch, index + 1), &after);
  gdk_rectangle_union (&before, &after, &both);
  box = both;
  if (branch->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      box.x = (before.x + before.width + after.x - reach) / 2;
      box.width = reach;
    }
  else
    {
      box.y = (before.y + before.height + after.y - reach) / 2;
      box.height = reach;
    }
  box.width = MAX (box.width, 1);
  box.height = MAX (box.height, 1);
  gtk_widget_size_allocate (divider, &box, -1);
}

static void
allocate_tile (AwraTileView *self,
               Tile         *tile)
{
  if (tile->child != NULL)
    {
      GtkAllocation box;
      int minimum = 0;

      tile->shown = tile->bounds;
      if (self->tick != 0)
        {
          tile->shown.x = mix (tile->from.x, tile->bounds.x, self->eased);
          tile->shown.y = mix (tile->from.y, tile->bounds.y, self->eased);
          tile->shown.width = mix (tile->from.width, tile->bounds.width, self->eased);
          tile->shown.height = mix (tile->from.height, tile->bounds.height, self->eased);
        }
      /* What arrives is uncovered at its final size; nothing goes below its
       * minimum, what does not fit is clipped until it does. */
      box = tile->arriving ? tile->bounds : tile->shown;
      gtk_widget_measure (tile->child, GTK_ORIENTATION_HORIZONTAL, -1,
                          &minimum, NULL, NULL, NULL);
      box.width = MAX (box.width, minimum);
      gtk_widget_measure (tile->child, GTK_ORIENTATION_VERTICAL, -1,
                          &minimum, NULL, NULL, NULL);
      box.height = MAX (box.height, minimum);
      gtk_widget_size_allocate (tile->child, &box, -1);
      return;
    }
  for (guint i = 0; i < tile->tiles->len; i++)
    allocate_tile (self, tile_at (tile, i));
  for (guint i = 0; i < tile->dividers->len; i++)
    allocate_divider (self, tile, i);
}

static void
awra_tile_view_size_allocate (GtkWidget *widget,
                              int        width,
                              int        height,
                              int        baseline)
{
  AwraTileView *self = AWRA_TILE_VIEW (widget);

  (void) width;
  (void) height;
  (void) baseline;
  if (self->root == NULL)
    return;
  arrange (self);
  allocate_tile (self, self->root);
}

static void
awra_tile_view_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum,
                        int            *natural,
                        int            *minimum_baseline,
                        int            *natural_baseline)
{
  AwraTileView *self = AWRA_TILE_VIEW (widget);

  (void) for_size;
  (void) minimum_baseline;
  (void) natural_baseline;
  *minimum = *natural = 0;
  if (self->root != NULL)
    measure_tile (self, self->root, orientation, minimum, natural);
}

static void
snapshot_leaf (AwraTileView *self,
               Tile         *leaf,
               GtkSnapshot  *snapshot)
{
  GtkWidget *widget = GTK_WIDGET (self);
  graphene_rect_t clip;

  if (self->tick == 0)
    {
      gtk_widget_snapshot_child (widget, leaf->child, snapshot);
      return;
    }
  graphene_rect_init (&clip, leaf->shown.x, leaf->shown.y,
                      leaf->shown.width, leaf->shown.height);
  gtk_snapshot_push_clip (snapshot, &clip);
  if (leaf->arriving)
    gtk_snapshot_push_opacity (snapshot,
                               leaf->fade_from + (1.0 - leaf->fade_from) * self->eased);
  gtk_widget_snapshot_child (widget, leaf->child, snapshot);
  if (leaf->arriving)
    gtk_snapshot_pop (snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
snapshot_dividers (AwraTileView *self,
                   Tile         *tile,
                   GtkSnapshot  *snapshot)
{
  if (tile == NULL || tile->child != NULL)
    return;
  for (guint i = 0; i < tile->tiles->len; i++)
    snapshot_dividers (self, tile_at (tile, i), snapshot);
  for (guint i = 0; i < tile->dividers->len; i++)
    gtk_widget_snapshot_child (GTK_WIDGET (self),
                               g_ptr_array_index (tile->dividers, i), snapshot);
}

static void
awra_tile_view_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  AwraTileView *self = AWRA_TILE_VIEW (widget);
  g_autoptr (GPtrArray) leaves = leaves_of (self);

  for (guint i = 0; i < self->ghosts->len; i++)
    {
      Ghost *ghost = g_ptr_array_index (self->ghosts, i);
      graphene_point_t corner;

      graphene_point_init (&corner, ghost->bounds.x, ghost->bounds.y);
      gtk_snapshot_push_opacity (snapshot, ghost->fade_from * (1.0 - self->eased));
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &corner);
      gdk_paintable_snapshot (ghost->frame, snapshot,
                              ghost->bounds.width, ghost->bounds.height);
      gtk_snapshot_restore (snapshot);
      gtk_snapshot_pop (snapshot);
    }
  for (guint i = 0; i < leaves->len; i++)
    if (!((Tile *) g_ptr_array_index (leaves, i))->lifted)
      snapshot_leaf (self, g_ptr_array_index (leaves, i), snapshot);
  snapshot_dividers (self, self->root, snapshot);
  for (guint i = 0; i < leaves->len; i++)
    if (((Tile *) g_ptr_array_index (leaves, i))->lifted)
      snapshot_leaf (self, g_ptr_array_index (leaves, i), snapshot);
}

static void
awra_tile_view_unmap (GtkWidget *widget)
{
  settle (AWRA_TILE_VIEW (widget));
  GTK_WIDGET_CLASS (awra_tile_view_parent_class)->unmap (widget);
}

static void
awra_tile_view_dispose (GObject *object)
{
  AwraTileView *self = AWRA_TILE_VIEW (object);
  GtkWidget *child;

  if (self->tick != 0)
    gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick);
  self->tick = 0;
  if (self->root != NULL)
    {
      g_autoptr (GPtrArray) leaves = leaves_of (self);

      free_branches (self->root);
      for (guint i = 0; i < leaves->len; i++)
        g_free (g_ptr_array_index (leaves, i));
    }
  self->root = NULL;
  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))) != NULL)
    gtk_widget_unparent (child);
  g_clear_pointer (&self->ghosts, g_ptr_array_unref);
  G_OBJECT_CLASS (awra_tile_view_parent_class)->dispose (object);
}

static void
awra_tile_view_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  AwraTileView *self = AWRA_TILE_VIEW (object);

  switch (prop_id)
    {
    case PROP_COLUMN_SPACING:
      g_value_set_int (value, self->column_spacing);
      break;
    case PROP_ROW_SPACING:
      g_value_set_int (value, self->row_spacing);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
awra_tile_view_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  AwraTileView *self = AWRA_TILE_VIEW (object);

  switch (prop_id)
    {
    case PROP_COLUMN_SPACING:
      awra_tile_view_set_column_spacing (self, g_value_get_int (value));
      break;
    case PROP_ROW_SPACING:
      awra_tile_view_set_row_spacing (self, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
awra_tile_view_class_init (AwraTileViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_tile_view_dispose;
  object_class->get_property = awra_tile_view_get_property;
  object_class->set_property = awra_tile_view_set_property;
  widget_class->measure = awra_tile_view_measure;
  widget_class->size_allocate = awra_tile_view_size_allocate;
  widget_class->snapshot = awra_tile_view_snapshot;
  widget_class->unmap = awra_tile_view_unmap;

  /**
   * AwraTileView:column-spacing:
   *
   * The gap between columns.
   *
   * Since: 1.1
   */
  properties[PROP_COLUMN_SPACING] =
    g_param_spec_int ("column-spacing", NULL, NULL, 0, G_MAXINT, 1,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraTileView:row-spacing:
   *
   * The gap between rows.
   *
   * Since: 1.1
   */
  properties[PROP_ROW_SPACING] =
    g_param_spec_int ("row-spacing", NULL, NULL, 0, G_MAXINT, 1,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "awratileview");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
awra_tile_view_init (AwraTileView *self)
{
  self->column_spacing = self->row_spacing = 1;
  self->eased = 1.0;
  self->ghosts = g_ptr_array_new_with_free_func (ghost_free);
  gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
}

GtkWidget *
awra_tile_view_new (void)
{
  return g_object_new (AWRA_TYPE_TILE_VIEW, NULL);
}

static GdkRectangle
far_edge (const GdkRectangle *bounds,
          GtkPositionType     side)
{
  GdkRectangle edge = *bounds;

  if (is_horizontal (side))
    {
      edge.width = 0;
      edge.x += comes_first (side) ? 0 : bounds->width;
    }
  else
    {
      edge.height = 0;
      edge.y += comes_first (side) ? 0 : bounds->height;
    }
  return edge;
}

void
awra_tile_view_insert (AwraTileView    *self,
                       GtkWidget       *child,
                       GtkWidget       *beside,
                       GtkPositionType  side)
{
  gboolean first;
  Tile *leaf;

  g_return_if_fail (AWRA_IS_TILE_VIEW (self));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == NULL);
  g_return_if_fail (beside == NULL || find_leaf (self, beside) != NULL);

  first = self->root == NULL;
  hold_still (self);
  leaf = leaf_new (child);
  /* First in line, so that it stays below every divider. */
  gtk_widget_insert_after (child, GTK_WIDGET (self), NULL);
  attach_tile (self, leaf, beside, side);
  arrange (self);
  leaf->arriving = !first;
  leaf->from = leaf->shown = first ? leaf->bounds : far_edge (&leaf->bounds, side);
  apply_change (self);
}

void
awra_tile_view_remove (AwraTileView *self,
                       GtkWidget    *child)
{
  Tile *leaf;

  g_return_if_fail (AWRA_IS_TILE_VIEW (self));
  leaf = find_leaf (self, child);
  g_return_if_fail (leaf != NULL);

  hold_still (self);
  if (can_animate (self) && gtk_widget_get_mapped (child))
    {
      g_autoptr (GdkPaintable) live = gtk_widget_paintable_new (child);
      Ghost *ghost = g_new0 (Ghost, 1);

      ghost->frame = gdk_paintable_get_current_image (live);
      ghost->bounds = leaf->shown;
      ghost->fade_from = 1.0;
      g_ptr_array_add (self->ghosts, ghost);
    }
  detach_tile (self, leaf);
  g_free (leaf);
  gtk_widget_unparent (child);
  apply_change (self);
}

void
awra_tile_view_move (AwraTileView    *self,
                     GtkWidget       *child,
                     GtkWidget       *beside,
                     GtkPositionType  side)
{
  Tile *leaf;

  g_return_if_fail (AWRA_IS_TILE_VIEW (self));
  leaf = find_leaf (self, child);
  g_return_if_fail (leaf != NULL);
  g_return_if_fail (beside != child);
  g_return_if_fail (beside == NULL || find_leaf (self, beside) != NULL);

  hold_still (self);
  detach_tile (self, leaf);
  attach_tile (self, leaf, beside, side);
  leaf->lifted = TRUE;
  apply_change (self);
}

GList *
awra_tile_view_get_children (AwraTileView *self)
{
  g_autoptr (GPtrArray) leaves = NULL;
  GList *children = NULL;

  g_return_val_if_fail (AWRA_IS_TILE_VIEW (self), NULL);
  leaves = leaves_of (self);
  for (guint i = leaves->len; i > 0; i--)
    children = g_list_prepend (children,
                               ((Tile *) g_ptr_array_index (leaves, i - 1))->child);
  return children;
}

gboolean
awra_tile_view_get_child_bounds (AwraTileView *self,
                                 GtkWidget    *child,
                                 GdkRectangle *bounds)
{
  Tile *leaf;

  g_return_val_if_fail (AWRA_IS_TILE_VIEW (self), FALSE);
  g_return_val_if_fail (bounds != NULL, FALSE);
  leaf = find_leaf (self, child);
  g_return_val_if_fail (leaf != NULL, FALSE);
  if (gtk_widget_get_width (GTK_WIDGET (self)) <= 0)
    return FALSE;
  arrange (self);
  *bounds = leaf->bounds;
  return TRUE;
}

gboolean
awra_tile_view_touches_edge (AwraTileView    *self,
                             GtkWidget       *child,
                             GtkPositionType  side)
{
  GtkOrientation orientation = is_horizontal (side) ? GTK_ORIENTATION_HORIZONTAL
                                                    : GTK_ORIENTATION_VERTICAL;
  Tile *tile;

  g_return_val_if_fail (AWRA_IS_TILE_VIEW (self), FALSE);
  tile = find_leaf (self, child);
  g_return_val_if_fail (tile != NULL, FALSE);
  for (; tile->parent != NULL; tile = tile->parent)
    {
      guint index = index_in_parent (tile);

      if (tile->parent->orientation == orientation &&
          index != (comes_first (side) ? 0 : tile->parent->tiles->len - 1))
        return FALSE;
    }
  return TRUE;
}

gboolean
awra_tile_view_compute_insertion (AwraTileView    *self,
                                  GtkWidget       *beside,
                                  GtkPositionType  side,
                                  GdkRectangle    *bounds)
{
  GtkOrientation orientation = is_horizontal (side) ? GTK_ORIENTATION_HORIZONTAL
                                                    : GTK_ORIENTATION_VERTICAL;
  int spacing = is_horizontal (side) ? self->column_spacing : self->row_spacing;
  Tile *anchor;
  int length;
  int part;
  int minimum;
  gboolean fits;

  g_return_val_if_fail (AWRA_IS_TILE_VIEW (self), FALSE);
  g_return_val_if_fail (bounds != NULL, FALSE);
  anchor = beside != NULL ? find_leaf (self, beside) : self->root;
  g_return_val_if_fail (beside == NULL || anchor != NULL, FALSE);

  if (gtk_widget_get_width (GTK_WIDGET (self)) <= 0)
    return FALSE;
  arrange (self);
  if (anchor == NULL)
    {
      *bounds = (GdkRectangle) { 0, 0, gtk_widget_get_width (GTK_WIDGET (self)),
                                 gtk_widget_get_height (GTK_WIDGET (self)) };
      return TRUE;
    }
  length = extent (&anchor->bounds, orientation);
  minimum = minimum_of (self, anchor, orientation);
  if (beside == NULL && anchor->child == NULL && anchor->orientation == orientation)
    {
      g_autoptr (GPtrArray) leaves = leaves_of (self);
      int count = (int) anchor->tiles->len;
      int largest = 0;

      for (guint i = 0; i < leaves->len; i++)
        largest = MAX (largest, minimum_of (self, g_ptr_array_index (leaves, i), orientation));
      part = (length - spacing * count) / (count + 1);
      fits = part >= largest && length - part - spacing >= minimum;
    }
  else
    {
      part = (length - spacing) / 2;
      fits = part >= minimum;
    }
  *bounds = anchor->bounds;
  if (is_horizontal (side))
    {
      bounds->x += comes_first (side) ? 0 : length - part;
      bounds->width = part;
    }
  else
    {
      bounds->y += comes_first (side) ? 0 : length - part;
      bounds->height = part;
    }
  return fits;
}

static void
equalize_tile (Tile *tile)
{
  if (tile == NULL || tile->child != NULL)
    return;
  for (guint i = 0; i < tile->tiles->len; i++)
    {
      tile_at (tile, i)->share = 1.0 / tile->tiles->len;
      equalize_tile (tile_at (tile, i));
    }
}

void
awra_tile_view_equalize (AwraTileView *self)
{
  g_return_if_fail (AWRA_IS_TILE_VIEW (self));
  hold_still (self);
  equalize_tile (self->root);
  apply_change (self);
}

static GVariant *
arrangement_of (Tile *tile)
{
  GVariantBuilder shares;
  GVariantBuilder tiles;

  if (tile == NULL || tile->child != NULL)
    return g_variant_new_variant (g_variant_new_tuple (NULL, 0));
  g_variant_builder_init (&shares, G_VARIANT_TYPE ("ad"));
  g_variant_builder_init (&tiles, G_VARIANT_TYPE ("av"));
  for (guint i = 0; i < tile->tiles->len; i++)
    {
      g_variant_builder_add (&shares, "d", tile_at (tile, i)->share);
      g_variant_builder_add_value (&tiles, arrangement_of (tile_at (tile, i)));
    }
  return g_variant_new_variant (
    g_variant_new ("(b@ad@av)", tile->orientation == GTK_ORIENTATION_HORIZONTAL,
                   g_variant_builder_end (&shares), g_variant_builder_end (&tiles)));
}

GVariant *
awra_tile_view_get_arrangement (AwraTileView *self)
{
  g_return_val_if_fail (AWRA_IS_TILE_VIEW (self), NULL);
  return arrangement_of (self->root);
}

/* The number of tiles, or -1 for what no view can have written. */
static int
count_tiles (GVariant *arrangement,
             int       depth)
{
  g_autoptr (GVariant) inner = NULL;
  g_autoptr (GVariant) shares = NULL;
  g_autoptr (GVariant) tiles = NULL;
  const double *values;
  gsize count = 0;
  int total = 0;

  if (depth > ARRANGEMENT_DEPTH ||
      !g_variant_is_of_type (arrangement, G_VARIANT_TYPE_VARIANT))
    return -1;
  inner = g_variant_get_variant (arrangement);
  if (g_variant_is_of_type (inner, G_VARIANT_TYPE_UNIT))
    return 1;
  if (!g_variant_is_of_type (inner, G_VARIANT_TYPE ("(badav)")))
    return -1;
  shares = g_variant_get_child_value (inner, 1);
  tiles = g_variant_get_child_value (inner, 2);
  values = g_variant_get_fixed_array (shares, &count, sizeof (double));
  if (count < 2 || count != g_variant_n_children (tiles))
    return -1;
  for (gsize i = 0; i < count; i++)
    {
      g_autoptr (GVariant) tile = g_variant_get_child_value (tiles, i);
      int part = count_tiles (tile, depth + 1);

      if (part < 0 || !isfinite (values[i]) || values[i] <= 0.0)
        return -1;
      total += part;
    }
  return total;
}

static Tile *
tile_from (AwraTileView *self,
           GVariant     *arrangement,
           GPtrArray    *leaves,
           guint        *next)
{
  g_autoptr (GVariant) inner = g_variant_get_variant (arrangement);
  g_autoptr (GVariant) shares = NULL;
  g_autoptr (GVariant) tiles = NULL;
  const double *values;
  gsize count = 0;
  double sum = 0.0;
  gboolean columns = FALSE;
  Tile *branch;

  if (g_variant_is_of_type (inner, G_VARIANT_TYPE_UNIT))
    return g_ptr_array_index (leaves, (*next)++);
  g_variant_get_child (inner, 0, "b", &columns);
  shares = g_variant_get_child_value (inner, 1);
  tiles = g_variant_get_child_value (inner, 2);
  values = g_variant_get_fixed_array (shares, &count, sizeof (double));
  branch = branch_new (columns ? GTK_ORIENTATION_HORIZONTAL
                               : GTK_ORIENTATION_VERTICAL);
  for (gsize i = 0; i < count; i++)
    sum += values[i];
  for (gsize i = 0; i < count; i++)
    {
      g_autoptr (GVariant) part = g_variant_get_child_value (tiles, i);
      Tile *tile = tile_from (self, part, leaves, next);

      tile->share = values[i] / sum;
      add_tile (self, branch, tile, (guint) i);
    }
  return branch;
}

gboolean
awra_tile_view_set_arrangement (AwraTileView *self,
                                GVariant     *arrangement)
{
  g_autoptr (GPtrArray) leaves = NULL;
  guint next = 0;

  g_return_val_if_fail (AWRA_IS_TILE_VIEW (self), FALSE);
  g_return_val_if_fail (arrangement != NULL, FALSE);
  leaves = leaves_of (self);
  if (leaves->len == 0 || count_tiles (arrangement, 0) != (int) leaves->len)
    return FALSE;
  hold_still (self);
  free_branches (self->root);
  self->root = tile_from (self, arrangement, leaves, &next);
  self->root->parent = NULL;
  self->root->share = 1.0;
  settle (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  return TRUE;
}

int
awra_tile_view_get_column_spacing (AwraTileView *self)
{
  g_return_val_if_fail (AWRA_IS_TILE_VIEW (self), 0);
  return self->column_spacing;
}

void
awra_tile_view_set_column_spacing (AwraTileView *self,
                                   int           spacing)
{
  g_return_if_fail (AWRA_IS_TILE_VIEW (self));
  g_return_if_fail (spacing >= 0);
  if (self->column_spacing == spacing)
    return;
  self->column_spacing = spacing;
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLUMN_SPACING]);
}

int
awra_tile_view_get_row_spacing (AwraTileView *self)
{
  g_return_val_if_fail (AWRA_IS_TILE_VIEW (self), 0);
  return self->row_spacing;
}

void
awra_tile_view_set_row_spacing (AwraTileView *self,
                                int           spacing)
{
  g_return_if_fail (AWRA_IS_TILE_VIEW (self));
  g_return_if_fail (spacing >= 0);
  if (self->row_spacing == spacing)
    return;
  self->row_spacing = spacing;
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ROW_SPACING]);
}
