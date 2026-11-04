/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <math.h>

#include "widgets/awra-widget-style-private.h"
#include <awra/awra-conversation-view.h>

#include <awra/awra-button.h>
#include <awra/awra-context.h>
#include <awra/awra-motion.h>

#include "awra-i18n-private.h"

/* Within this distance of the end, the view counts as being at the end. */
#define AT_BOTTOM_SLACK 24.0
/* Beyond this many pages, moving to an item or to the end jumps instead of
 * gliding: a long glide only blurs what it passes. */
#define GLIDE_PAGES 2.0
/* A larger batch of appended items is a load, not arrivals worth showing. */
#define MAX_ARRIVING 8
/* Classes stay a little longer than their CSS animation, then leave so a
 * restyled row never replays them. */
#define ARRIVING_CLASS_MS 420
#define HIGHLIGHT_CLASS_MS 1500
#define PILL_MARGIN 14
#define PILL_RISE 10.0
#define TOP_FADE 16.0f
#define ALIGN_ATTEMPTS 12

typedef struct {
  GtkWidget *child;
  const char *css_class;
  gint64 until;
} Mark;

struct _AwraConversationView {
  GtkWidget parent_instance;

  GtkScrolledWindow *scroller;
  GtkListView *delegate;
  GtkAdjustment *vadjustment;
  GtkWidget *pill;
  GtkLabel *pill_label;
  /* What is happening at the top and at the bottom: a revealer each, over
   * the list, with a spinner or a warning, a label and a retry. */
  GtkWidget *edges[2];
  GtkWidget *edge_spinners[2];
  GtkWidget *edge_icons[2];
  GtkLabel *edge_labels[2];
  GtkWidget *edge_retries[2];
  AwraEdgeStatus edge_status[2];

  GListModel *model;
  GtkNoSelection *selection;
  GtkListItemFactory *factory;
  GtkListItemFactory *wrapper;
  GtkListItemFactory *header_factory;
  guint n_items;

  /* GtkListItems currently bound, so rows can be found by position. */
  GPtrArray *bound;
  guint bound_generation;
  /* Model items appended while following, until their row is bound. */
  GPtrArray *arriving;
  GArray *marks;
  guint marks_source;
  GObject *highlighted;
  gint64 highlight_until;

  char *jump_label;
  guint unread_count;
  int edge_threshold;
  gboolean follow;
  gboolean at_bottom;
  gboolean top_armed;
  gboolean bottom_armed;

  /* Telling the user's scrolling apart from the view's own and the list's. */
  GtkWidget *port;
  gboolean setting_value;
  gboolean user_moved;
  gboolean append_pending;
  gboolean reach_end;
  gboolean snapped_with_scroll_to;
  double set_value_last;
  double frame_value;
  double frame_upper;
  double frame_page;

  /* Glide. */
  guint glide_tick;
  gint64 glide_started;
  guint glide_duration;
  double glide_from;
  double glide_target;
  /* A glide to an item follows the item, not a scroll position: the list may
   * re-anchor under it as estimated heights become real. */
  guint glide_item;
  AwraConversationAlign glide_align;
  gboolean glide_to_end;

  /* Pending scroll_to_item() alignment, once the row exists. */
  guint align_position;
  AwraConversationAlign align;
  guint align_attempts;

  /* Jump pill. */
  gboolean pill_shown;
  double pill_progress;
  double pill_from;
  gint64 pill_started;
  guint pill_tick;

  GdkFrameClock *clock;
  gulong layout_handler;
  gulong paint_handler;
  gboolean range_valid;
  guint range_first;
  guint range_last;
  double range_value;
  double range_upper;
  double range_page;
  guint range_generation;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_FACTORY,
  PROP_HEADER_FACTORY,
  PROP_AT_BOTTOM,
  PROP_FOLLOW,
  PROP_UNREAD_COUNT,
  PROP_JUMP_LABEL,
  PROP_EDGE_THRESHOLD,
  N_PROPS,
};

enum {
  SIGNAL_EDGE_REACHED,
  SIGNAL_VISIBLE_RANGE_CHANGED,
  SIGNAL_JUMP_ACTIVATED,
  SIGNAL_RETRY,
  N_SIGNALS,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (AwraConversationView, awra_conversation_view, GTK_TYPE_WIDGET)

/* The scrollable between the scrolled window and the list. It only forwards,
 * but it knows when the list is being allocated: scroll positions the list
 * sets then come from its anchor, never from the user. */

typedef struct {
  GtkWidget parent_instance;
  GtkWidget *child;
  gboolean allocating;
} AwraConversationPort;

typedef struct {
  GtkWidgetClass parent_class;
} AwraConversationPortClass;

enum {
  PORT_PROP_0,
  PORT_PROP_HADJUSTMENT,
  PORT_PROP_VADJUSTMENT,
  PORT_PROP_HSCROLL_POLICY,
  PORT_PROP_VSCROLL_POLICY,
};

static GType awra_conversation_port_get_type (void);
static void awra_conversation_port_scrollable_init (GtkScrollableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (AwraConversationPort, awra_conversation_port, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE,
                                                      awra_conversation_port_scrollable_init))

#define AWRA_CONVERSATION_PORT(object) \
  (G_TYPE_CHECK_INSTANCE_CAST ((object), awra_conversation_port_get_type (), AwraConversationPort))

static const char *
port_property_name (guint prop_id)
{
  switch (prop_id) {
  case PORT_PROP_HADJUSTMENT:
    return "hadjustment";
  case PORT_PROP_VADJUSTMENT:
    return "vadjustment";
  case PORT_PROP_HSCROLL_POLICY:
    return "hscroll-policy";
  case PORT_PROP_VSCROLL_POLICY:
    return "vscroll-policy";
  default:
    return NULL;
  }
}

static void
awra_conversation_port_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  AwraConversationPort *self = AWRA_CONVERSATION_PORT (object);
  const char *name = port_property_name (prop_id);

  if (name == NULL) {
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    return;
  }
  if (self->child != NULL)
    g_object_get_property (G_OBJECT (self->child), name, value);
}

static void
awra_conversation_port_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  AwraConversationPort *self = AWRA_CONVERSATION_PORT (object);
  const char *name = port_property_name (prop_id);

  if (name == NULL) {
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    return;
  }
  /* Construct-time defaults arrive before the list does. */
  if (self->child != NULL)
    g_object_set_property (G_OBJECT (self->child), name, value);
}

static gboolean
awra_conversation_port_get_border (GtkScrollable *scrollable,
                                   GtkBorder     *border)
{
  return gtk_scrollable_get_border (GTK_SCROLLABLE (AWRA_CONVERSATION_PORT (scrollable)->child),
                                    border);
}

static void
awra_conversation_port_scrollable_init (GtkScrollableInterface *iface)
{
  iface->get_border = awra_conversation_port_get_border;
}

static void
awra_conversation_port_measure (GtkWidget      *widget,
                                GtkOrientation  orientation,
                                int             for_size,
                                int            *minimum,
                                int            *natural,
                                int            *minimum_baseline,
                                int            *natural_baseline)
{
  gtk_widget_measure (AWRA_CONVERSATION_PORT (widget)->child, orientation, for_size,
                      minimum, natural, minimum_baseline, natural_baseline);
}

static void
awra_conversation_port_size_allocate (GtkWidget *widget,
                                      int        width,
                                      int        height,
                                      int        baseline)
{
  AwraConversationPort *self = AWRA_CONVERSATION_PORT (widget);
  int natural = 0;

  /* A short conversation sits at the bottom, next to where the next message
   * will appear, like the end of a longer one. */
  gtk_widget_measure (self->child, GTK_ORIENTATION_VERTICAL, width, NULL, &natural, NULL, NULL);
  natural = MIN (natural, height);
  self->allocating = TRUE;
  gtk_widget_allocate (self->child, width, natural, baseline,
                       gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (0, height - natural)));
  self->allocating = FALSE;
}

static void
awra_conversation_port_dispose (GObject *object)
{
  AwraConversationPort *self = AWRA_CONVERSATION_PORT (object);

  g_clear_pointer (&self->child, gtk_widget_unparent);
  G_OBJECT_CLASS (awra_conversation_port_parent_class)->dispose (object);
}

static void
awra_conversation_port_class_init (AwraConversationPortClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_conversation_port_dispose;
  object_class->get_property = awra_conversation_port_get_property;
  object_class->set_property = awra_conversation_port_set_property;
  widget_class->measure = awra_conversation_port_measure;
  widget_class->size_allocate = awra_conversation_port_size_allocate;
  g_object_class_override_property (object_class, PORT_PROP_HADJUSTMENT, "hadjustment");
  g_object_class_override_property (object_class, PORT_PROP_VADJUSTMENT, "vadjustment");
  g_object_class_override_property (object_class, PORT_PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (object_class, PORT_PROP_VSCROLL_POLICY, "vscroll-policy");
  gtk_widget_class_set_css_name (widget_class, "awraconversationport");
}

static void
awra_conversation_port_init (AwraConversationPort *self)
{
  (void) self;
}

static GtkWidget *
awra_conversation_port_new (GtkWidget *child)
{
  AwraConversationPort *self = g_object_new (awra_conversation_port_get_type (), NULL);

  self->child = child;
  gtk_widget_set_parent (child, GTK_WIDGET (self));
  return GTK_WIDGET (self);
}

static guint
motion_duration (AwraConversationView *self,
                 AwraMotionPreset      preset)
{
  AwraContext *context = awra_context_get_for_display (
    gtk_widget_get_display (GTK_WIDGET (self)));

  return awra_motion_get_duration (awra_context_get_style_manager (context), preset);
}

static double
end_value (AwraConversationView *self)
{
  return MAX (0.0, gtk_adjustment_get_upper (self->vadjustment) -
                   gtk_adjustment_get_page_size (self->vadjustment));
}

static double
distance_to_end (AwraConversationView *self)
{
  return end_value (self) - gtk_adjustment_get_value (self->vadjustment);
}

static void
set_value (AwraConversationView *self,
           double                value)
{
  self->setting_value = TRUE;
  gtk_adjustment_set_value (self->vadjustment, CLAMP (value, 0.0, end_value (self)));
  self->setting_value = FALSE;
  self->set_value_last = gtk_adjustment_get_value (self->vadjustment);
}

static void
set_follow_internal (AwraConversationView *self,
                     gboolean              follow)
{
  follow = !!follow;
  if (self->follow == follow)
    return;
  self->follow = follow;
  if (!follow)
    g_ptr_array_set_size (self->arriving, 0);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FOLLOW]);
}

/* Marks: CSS classes that leave on their own. */

static void
mark_clear (Mark *mark)
{
  gtk_widget_remove_css_class (mark->child, mark->css_class);
  g_object_unref (mark->child);
}

static gboolean marks_timeout_cb (gpointer user_data);

static void
marks_schedule (AwraConversationView *self)
{
  gint64 now = g_get_monotonic_time ();
  gint64 earliest = G_MAXINT64;

  g_clear_handle_id (&self->marks_source, g_source_remove);
  for (guint i = 0; i < self->marks->len; i++)
    earliest = MIN (earliest, g_array_index (self->marks, Mark, i).until);
  if (earliest == G_MAXINT64)
    return;
  self->marks_source = g_timeout_add (MAX (0, (earliest - now) / 1000) + 1,
                                      marks_timeout_cb, self);
}

static gboolean
marks_timeout_cb (gpointer user_data)
{
  AwraConversationView *self = user_data;
  gint64 now = g_get_monotonic_time ();

  self->marks_source = 0;
  for (guint i = self->marks->len; i > 0; i--) {
    Mark *mark = &g_array_index (self->marks, Mark, i - 1);

    if (mark->until <= now) {
      mark_clear (mark);
      g_array_remove_index_fast (self->marks, i - 1);
    }
  }
  marks_schedule (self);
  return G_SOURCE_REMOVE;
}

static void
mark_add (AwraConversationView *self,
          GtkWidget            *child,
          const char           *css_class,
          guint                 duration_ms)
{
  Mark mark = { g_object_ref (child), css_class,
                g_get_monotonic_time () + (gint64) duration_ms * 1000 };

  gtk_widget_add_css_class (child, css_class);
  g_array_append_val (self->marks, mark);
  marks_schedule (self);
}

static void
marks_drop_child (AwraConversationView *self,
                  GtkWidget            *child)
{
  for (guint i = self->marks->len; i > 0; i--) {
    Mark *mark = &g_array_index (self->marks, Mark, i - 1);

    if (mark->child == child) {
      mark_clear (mark);
      g_array_remove_index_fast (self->marks, i - 1);
    }
  }
}

static void
marks_clear (AwraConversationView *self)
{
  g_clear_handle_id (&self->marks_source, g_source_remove);
  for (guint i = 0; i < self->marks->len; i++)
    mark_clear (&g_array_index (self->marks, Mark, i));
  g_array_set_size (self->marks, 0);
}

static void
highlight_child (AwraConversationView *self,
                 GtkWidget            *child)
{
  gint64 now = g_get_monotonic_time ();
  /* Two identical animations: a class that is already there would not
   * replay, so a second highlight alternates. */
  const char *css_class = gtk_widget_has_css_class (child, "awra-conversation-highlight")
                            ? "awra-conversation-highlight-again"
                            : "awra-conversation-highlight";

  marks_drop_child (self, child);
  if (self->highlight_until <= now)
    return;
  mark_add (self, child, css_class, (guint) ((self->highlight_until - now) / 1000));
}

/* Rows. */

static GtkListItem *
find_bound (AwraConversationView *self,
            guint                 position)
{
  for (guint i = 0; i < self->bound->len; i++) {
    GtkListItem *item = g_ptr_array_index (self->bound, i);

    if (gtk_list_item_get_position (item) == position)
      return item;
  }
  return NULL;
}

/* Marks go on the row that holds the application's child, so a highlight
 * spans the whole width whatever margins the child has. */
static GtkWidget *
row_widget (GtkListItem *list_item)
{
  GtkWidget *child = gtk_list_item_get_child (list_item);

  if (child == NULL)
    return NULL;
  return gtk_widget_get_parent (child) != NULL ? gtk_widget_get_parent (child) : child;
}

static void
row_bound (AwraConversationView *self,
           GtkListItem          *list_item)
{
  GtkWidget *child = row_widget (list_item);
  gpointer item = gtk_list_item_get_item (list_item);
  guint index;

  g_ptr_array_add (self->bound, list_item);
  self->bound_generation++;
  if (child == NULL || item == NULL)
    return;
  if (g_ptr_array_find (self->arriving, item, &index)) {
    g_ptr_array_remove_index (self->arriving, index);
    mark_add (self, child, "awra-conversation-arriving", ARRIVING_CLASS_MS);
  }
  if (self->highlighted == item)
    highlight_child (self, child);
}

static void
row_unbound (AwraConversationView *self,
             GtkListItem          *list_item)
{
  GtkWidget *child = row_widget (list_item);

  g_ptr_array_remove_fast (self->bound, list_item);
  self->bound_generation++;
  if (child != NULL)
    marks_drop_child (self, child);
}

/* The view sits between the list and the application's factory, so it knows
 * which row shows which position without reaching into GTK's private rows. */

static void
forward (AwraConversationView *self,
         const char           *signal_name,
         GObject              *object)
{
  if (GTK_IS_SIGNAL_LIST_ITEM_FACTORY (self->factory))
    g_signal_emit_by_name (self->factory, signal_name, object);
}

static void
wrapper_setup_cb (GtkSignalListItemFactory *wrapper,
                  GObject                  *object,
                  AwraConversationView     *self)
{
  (void) wrapper;
  if (GTK_IS_BUILDER_LIST_ITEM_FACTORY (self->factory)) {
    GtkBuilderListItemFactory *factory = GTK_BUILDER_LIST_ITEM_FACTORY (self->factory);
    GBytes *bytes = gtk_builder_list_item_factory_get_bytes (factory);
    GtkBuilderScope *scope = gtk_builder_list_item_factory_get_scope (factory);
    g_autoptr (GtkBuilder) builder = gtk_builder_new ();
    g_autoptr (GError) error = NULL;
    gsize size = 0;
    const char *data = g_bytes_get_data (bytes, &size);

    gtk_builder_set_current_object (builder, object);
    if (scope != NULL)
      gtk_builder_set_scope (builder, scope);
    if (!gtk_builder_extend_with_template (builder, object, G_OBJECT_TYPE (object),
                                           data, (gssize) size, &error))
      g_critical ("Error building template for list item: %s", error->message);
    return;
  }
  forward (self, "setup", object);
}

static void
wrapper_bind_cb (GtkSignalListItemFactory *wrapper,
                 GObject                  *object,
                 AwraConversationView     *self)
{
  (void) wrapper;
  forward (self, "bind", object);
  row_bound (self, GTK_LIST_ITEM (object));
}

static void
wrapper_unbind_cb (GtkSignalListItemFactory *wrapper,
                   GObject                  *object,
                   AwraConversationView     *self)
{
  (void) wrapper;
  row_unbound (self, GTK_LIST_ITEM (object));
  forward (self, "unbind", object);
}

static void
wrapper_teardown_cb (GtkSignalListItemFactory *wrapper,
                     GObject                  *object,
                     AwraConversationView     *self)
{
  (void) wrapper;
  forward (self, "teardown", object);
}

static void
install_factory (AwraConversationView *self)
{
  /* Rebuild every row: those that exist came from the previous factory. */
  gtk_list_view_set_factory (self->delegate, NULL);
  if (self->factory != NULL)
    gtk_list_view_set_factory (self->delegate, self->wrapper);
}

/* How far the view must scroll for the row at @position to sit at @align. */
static gboolean
item_offset (AwraConversationView  *self,
             guint                  position,
             AwraConversationAlign  align,
             double                *offset)
{
  GtkListItem *item = find_bound (self, position);
  GtkWidget *row = item != NULL ? row_widget (item) : NULL;
  double page = gtk_adjustment_get_page_size (self->vadjustment);
  double value = gtk_adjustment_get_value (self->vadjustment);
  graphene_rect_t bounds;
  double target;

  /* A row the list scrolled away keeps a stale allocation, hidden. */
  if (row == NULL || !gtk_widget_get_mapped (row) ||
      !gtk_widget_compute_bounds (row, GTK_WIDGET (self->delegate), &bounds))
    return FALSE;
  switch (align) {
  case AWRA_CONVERSATION_ALIGN_CENTER:
    target = value + bounds.origin.y + bounds.size.height / 2.0 - page / 2.0;
    break;
  case AWRA_CONVERSATION_ALIGN_END:
    target = value + bounds.origin.y + bounds.size.height - page;
    break;
  case AWRA_CONVERSATION_ALIGN_TOP:
  default:
    target = value + bounds.origin.y;
    break;
  }
  *offset = CLAMP (target, 0.0, end_value (self)) - value;
  return TRUE;
}

/* Glide. */

static void
glide_stop (AwraConversationView *self)
{
  if (self->glide_tick != 0) {
    gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->glide_tick);
    self->glide_tick = 0;
  }
}

static gboolean
glide_tick_cb (GtkWidget     *widget,
               GdkFrameClock *clock,
               gpointer       user_data)
{
  AwraConversationView *self = AWRA_CONVERSATION_VIEW (widget);
  gint64 now = gdk_frame_clock_get_frame_time (clock);
  double value = gtk_adjustment_get_value (self->vadjustment);
  double progress;
  double eased;
  double offset;

  (void) user_data;
  if (self->glide_started == 0)
    self->glide_started = now;
  progress = (double) (now - self->glide_started) / (self->glide_duration * 1000.0);
  eased = awra_motion_ease_out_cubic (progress);
  if (self->glide_item != GTK_INVALID_LIST_POSITION &&
      item_offset (self, self->glide_item, self->glide_align, &offset)) {
    /* glide_from holds the distance left at the start. */
    if (progress >= 1.0) {
      self->glide_tick = 0;
      set_value (self, value + offset);
      /* Settle on the item once the rows around it are real. */
      self->align_position = self->glide_item;
      self->align = self->glide_align;
      self->align_attempts = 0;
      return G_SOURCE_REMOVE;
    }
    set_value (self, value + offset - self->glide_from * (1.0 - eased));
    return G_SOURCE_CONTINUE;
  } else {
    double target = self->glide_to_end ? end_value (self)
                                       : CLAMP (self->glide_target, 0.0, end_value (self));

    if (progress >= 1.0) {
      self->glide_tick = 0;
      set_value (self, target);
      if (self->glide_to_end)
        g_ptr_array_set_size (self->arriving, 0);
      return G_SOURCE_REMOVE;
    }
    set_value (self, self->glide_from + (target - self->glide_from) * eased);
    return G_SOURCE_CONTINUE;
  }
}

static void
glide_start (AwraConversationView *self,
             gboolean              to_end,
             double               target,
             guint                 duration)
{
  self->glide_from = gtk_adjustment_get_value (self->vadjustment);
  self->glide_target = target;
  self->glide_to_end = to_end;
  self->glide_item = GTK_INVALID_LIST_POSITION;
  self->glide_duration = MAX (duration, 1);
  self->glide_started = 0;
  if (self->glide_tick == 0)
    self->glide_tick = gtk_widget_add_tick_callback (GTK_WIDGET (self), glide_tick_cb,
                                                     NULL, NULL);
}

static void
glide_start_item (AwraConversationView  *self,
                  guint                  position,
                  AwraConversationAlign  align,
                  double                 offset,
                  guint                  duration)
{
  glide_start (self, FALSE, gtk_adjustment_get_value (self->vadjustment) + offset, duration);
  self->glide_item = position;
  self->glide_align = align;
  self->glide_from = offset;
}

static void
snap_to_end (AwraConversationView *self)
{
  double page = gtk_adjustment_get_page_size (self->vadjustment);

  if (self->n_items == 0)
    return;
  /* From far away, the list itself is asked: the heights between here and
   * the end are only estimates, and it anchors on the last item. */
  self->reach_end = TRUE;
  if (distance_to_end (self) > page * GLIDE_PAGES && !self->snapped_with_scroll_to) {
    self->snapped_with_scroll_to = TRUE;
    gtk_list_view_scroll_to (self->delegate, self->n_items - 1, GTK_LIST_SCROLL_NONE, NULL);
    return;
  }
  set_value (self, end_value (self));
}

/* Jump pill. */

static void
update_pill_label (AwraConversationView *self)
{
  g_autofree char *text = NULL;

  if (self->jump_label != NULL)
    text = g_strdup (self->jump_label);
  else if (self->unread_count > 0)
    text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%u new message",
                                         "%u new messages", self->unread_count),
                            self->unread_count);
  else
    text = g_strdup (_("Jump to present"));
  gtk_label_set_text (self->pill_label, text);
  if (self->unread_count > 0)
    gtk_widget_add_css_class (self->pill, "awra-conversation-jump-unread");
  else
    gtk_widget_remove_css_class (self->pill, "awra-conversation-jump-unread");
}

static gboolean
pill_tick_cb (GtkWidget     *widget,
              GdkFrameClock *clock,
              gpointer       user_data)
{
  AwraConversationView *self = AWRA_CONVERSATION_VIEW (widget);
  gint64 now = gdk_frame_clock_get_frame_time (clock);
  double target = self->pill_shown ? 1.0 : 0.0;
  guint duration = motion_duration (self, AWRA_MOTION_PRESET_NORMAL);
  double progress;

  (void) user_data;
  if (self->pill_started == 0)
    self->pill_started = now;
  progress = duration == 0 ? 1.0
                           : (double) (now - self->pill_started) / (duration * 1000.0);
  if (progress >= 1.0) {
    self->pill_progress = target;
    self->pill_tick = 0;
    if (!self->pill_shown)
      gtk_widget_set_child_visible (self->pill, FALSE);
    gtk_widget_queue_draw (widget);
    return G_SOURCE_REMOVE;
  }
  self->pill_progress = self->pill_from + (target - self->pill_from) * progress;
  gtk_widget_queue_draw (widget);
  return G_SOURCE_CONTINUE;
}

static void
set_pill_shown (AwraConversationView *self,
                gboolean              shown)
{
  if (self->pill_shown == shown)
    return;
  self->pill_shown = shown;
  if (shown)
    gtk_widget_set_child_visible (self->pill, TRUE);
  if (!gtk_widget_get_mapped (GTK_WIDGET (self)) ||
      motion_duration (self, AWRA_MOTION_PRESET_NORMAL) == 0) {
    if (self->pill_tick != 0) {
      gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->pill_tick);
      self->pill_tick = 0;
    }
    self->pill_progress = shown ? 1.0 : 0.0;
    gtk_widget_set_child_visible (self->pill, shown);
    gtk_widget_queue_draw (GTK_WIDGET (self));
    return;
  }
  self->pill_from = self->pill_progress;
  self->pill_started = 0;
  if (self->pill_tick == 0)
    self->pill_tick = gtk_widget_add_tick_callback (GTK_WIDGET (self), pill_tick_cb,
                                                    NULL, NULL);
}

static void
update_at_bottom (AwraConversationView *self)
{
  gboolean at_bottom = self->n_items == 0 ||
                       distance_to_end (self) <= AT_BOTTOM_SLACK ||
                       (self->glide_tick != 0 && self->glide_to_end);

  set_pill_shown (self, !at_bottom);
  if (self->at_bottom == at_bottom)
    return;
  self->at_bottom = at_bottom;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AT_BOTTOM]);
}

/* Adjustment. */

static void
adjustment_value_changed_cb (GtkAdjustment        *adjustment,
                             AwraConversationView *self)
{
  (void) adjustment;
  /* The view's own glides and the list's anchoring are not the user. */
  if (self->setting_value || ((AwraConversationPort *) self->port)->allocating)
    return;
  glide_stop (self);
  self->user_moved = TRUE;
}

/* Model. */

static void
items_changed_cb (GListModel           *model,
                  guint                 position,
                  guint                 removed,
                  guint                 added,
                  AwraConversationView *self)
{
  guint old_n = self->n_items;

  self->n_items = g_list_model_get_n_items (model);
  self->bound_generation++;
  if (position == 0 && added > 0 && old_n > 0)
    self->top_armed = TRUE;
  if (removed == 0 && added > 0 && position == old_n && old_n > 0) {
    self->append_pending = TRUE;
    if (!self->follow)
      self->bottom_armed = TRUE;
    else if (added <= MAX_ARRIVING && gtk_widget_get_mapped (GTK_WIDGET (self)) &&
             motion_duration (self, AWRA_MOTION_PRESET_NORMAL) > 0) {
      while (self->arriving->len + added > MAX_ARRIVING && self->arriving->len > 0)
        g_ptr_array_remove_index (self->arriving, 0);
      for (guint i = position; i < position + added; i++)
        g_ptr_array_add (self->arriving, g_list_model_get_item (model, i));
    }
  }
  if (self->n_items == 0)
    update_at_bottom (self);
}

/* Frame hooks: the list lays itself out first, then the view corrects the
 * scroll position in the same frame, before anything is painted. */

static void
apply_alignment (AwraConversationView *self)
{
  double offset;

  if (!item_offset (self, self->align_position, self->align, &offset)) {
    if (++self->align_attempts >= ALIGN_ATTEMPTS)
      self->align_position = GTK_INVALID_LIST_POSITION;
    return;
  }
  /* Rows brought in around the item replace estimated heights and the list
   * keeps its own anchor in place, so check again until the item stays. */
  if (fabs (offset) < 1.0 || ++self->align_attempts >= ALIGN_ATTEMPTS)
    self->align_position = GTK_INVALID_LIST_POSITION;
  if (fabs (offset) >= 0.5)
    set_value (self, gtk_adjustment_get_value (self->vadjustment) + offset);
}

static void
frame_layout_cb (GdkFrameClock        *clock,
                 AwraConversationView *self)
{
  double value = gtk_adjustment_get_value (self->vadjustment);
  double upper = gtk_adjustment_get_upper (self->vadjustment);
  double page = gtk_adjustment_get_page_size (self->vadjustment);
  double distance = distance_to_end (self);
  gboolean gliding_to_end = self->glide_tick != 0 && self->glide_to_end;
  gboolean content_changed = upper != self->frame_upper || page != self->frame_page;
  /* The list scrolled itself, following the keyboard focus: */
  gboolean list_moved = value != self->frame_value && !content_changed &&
                        fabs (value - self->set_value_last) >= 0.5;

  (void) clock;
  if (self->align_position != GTK_INVALID_LIST_POSITION) {
    apply_alignment (self);
  } else if (self->user_moved) {
    /* Scrolling away stops following; reaching the end resumes it. */
    set_follow_internal (self, distance <= AT_BOTTOM_SLACK);
    if (!self->follow)
      self->reach_end = FALSE;
  } else if (self->follow && self->n_items > 0 && !gliding_to_end) {
    if (distance <= 0.5) {
      self->reach_end = FALSE;
      self->snapped_with_scroll_to = FALSE;
    } else if (list_moved && distance > AT_BOTTOM_SLACK && !self->reach_end) {
      set_follow_internal (self, FALSE);
    } else {
      guint duration = motion_duration (self, AWRA_MOTION_PRESET_NORMAL);

      if (self->append_pending && duration > 0 && distance <= page * GLIDE_PAGES)
        glide_start (self, TRUE, 0.0, duration);
      else
        snap_to_end (self);
    }
  } else if (!self->follow && list_moved && self->glide_tick == 0 &&
             distance <= AT_BOTTOM_SLACK) {
    set_follow_internal (self, TRUE);
  }
  self->user_moved = FALSE;
  self->append_pending = FALSE;
  self->frame_value = gtk_adjustment_get_value (self->vadjustment);
  self->frame_upper = gtk_adjustment_get_upper (self->vadjustment);
  self->frame_page = gtk_adjustment_get_page_size (self->vadjustment);
  update_at_bottom (self);
}

static gboolean
compute_visible_range (AwraConversationView *self,
                       guint                *first,
                       guint                *last)
{
  double height = gtk_widget_get_height (GTK_WIDGET (self->delegate));
  guint low = G_MAXUINT;
  guint high = 0;

  for (guint i = 0; i < self->bound->len; i++) {
    GtkListItem *item = g_ptr_array_index (self->bound, i);
    GtkWidget *child = row_widget (item);
    graphene_rect_t bounds;
    guint position;

    /* The list hides rows it scrolled away without moving them. */
    if (child == NULL || !gtk_widget_get_mapped (child) ||
        !gtk_widget_compute_bounds (child, GTK_WIDGET (self->delegate), &bounds) ||
        bounds.size.height <= 0 ||
        bounds.origin.y + bounds.size.height <= 0 || bounds.origin.y >= height)
      continue;
    position = gtk_list_item_get_position (item);
    if (position == GTK_INVALID_LIST_POSITION)
      continue;
    low = MIN (low, position);
    high = MAX (high, position);
  }
  if (low == G_MAXUINT)
    return FALSE;
  *first = low;
  *last = high;
  return TRUE;
}

static void
frame_paint_cb (GdkFrameClock        *clock,
                AwraConversationView *self)
{
  double value = gtk_adjustment_get_value (self->vadjustment);
  double upper = gtk_adjustment_get_upper (self->vadjustment);
  double page = gtk_adjustment_get_page_size (self->vadjustment);
  double threshold = self->edge_threshold < 0 ? page : self->edge_threshold;
  gboolean emit_top = FALSE;
  gboolean emit_bottom = FALSE;
  gboolean emit_range = FALSE;

  (void) clock;
  if (value == self->range_value && upper == self->range_upper &&
      page == self->range_page && self->bound_generation == self->range_generation)
    return;
  self->range_value = value;
  self->range_upper = upper;
  self->range_page = page;
  self->range_generation = self->bound_generation;

  {
    guint first = 0;
    guint last = 0;
    gboolean valid = compute_visible_range (self, &first, &last);

    if (valid != self->range_valid ||
        (valid && (first != self->range_first || last != self->range_last))) {
      self->range_valid = valid;
      self->range_first = first;
      self->range_last = last;
      emit_range = TRUE;
    }
  }
  if (self->n_items > 0 && page > 0) {
    if (value <= threshold) {
      emit_top = self->top_armed;
      self->top_armed = FALSE;
    } else {
      self->top_armed = TRUE;
    }
    if (upper - page - value <= threshold) {
      emit_bottom = self->bottom_armed;
      self->bottom_armed = FALSE;
    } else {
      self->bottom_armed = TRUE;
    }
  }

  g_object_ref (self);
  if (emit_range)
    g_signal_emit (self, signals[SIGNAL_VISIBLE_RANGE_CHANGED], 0);
  /* A loading or failed edge waits: no second load, nor one per scroll. */
  if (emit_top && self->edge_status[0] == AWRA_EDGE_STATUS_NONE)
    g_signal_emit (self, signals[SIGNAL_EDGE_REACHED], 0, GTK_POS_TOP);
  if (emit_bottom && self->edge_status[1] == AWRA_EDGE_STATUS_NONE)
    g_signal_emit (self, signals[SIGNAL_EDGE_REACHED], 0, GTK_POS_BOTTOM);
  g_object_unref (self);
}

/* Keyboard. */

static gboolean
key_pressed_cb (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                AwraConversationView  *self)
{
  double page = gtk_adjustment_get_page_size (self->vadjustment);
  guint duration = motion_duration (self, AWRA_MOTION_PRESET_NORMAL);
  double target;

  (void) controller;
  (void) keycode;
  if ((state & gtk_accelerator_get_default_mod_mask ()) != 0 || self->n_items == 0)
    return FALSE;
  switch (keyval) {
  case GDK_KEY_Page_Up:
  case GDK_KEY_KP_Page_Up:
  case GDK_KEY_Page_Down:
  case GDK_KEY_KP_Page_Down:
    target = (self->glide_tick != 0 && !self->glide_to_end
                ? self->glide_target
                : gtk_adjustment_get_value (self->vadjustment));
    target += (keyval == GDK_KEY_Page_Up || keyval == GDK_KEY_KP_Page_Up ? -1 : 1) * page * 0.9;
    target = CLAMP (target, 0.0, end_value (self));
    self->align_position = GTK_INVALID_LIST_POSITION;
    if (target >= end_value (self) - AT_BOTTOM_SLACK) {
      awra_conversation_view_scroll_to_end (self, TRUE);
      return TRUE;
    }
    set_follow_internal (self, FALSE);
    if (duration > 0)
      glide_start (self, FALSE, target, duration);
    else
      set_value (self, target);
    return TRUE;
  case GDK_KEY_Home:
  case GDK_KEY_KP_Home:
    awra_conversation_view_scroll_to_item (self, 0, AWRA_CONVERSATION_ALIGN_TOP, FALSE);
    return TRUE;
  case GDK_KEY_End:
  case GDK_KEY_KP_End:
    awra_conversation_view_scroll_to_end (self, TRUE);
    return TRUE;
  default:
    return FALSE;
  }
}

static void
pill_clicked_cb (GtkButton            *button,
                 AwraConversationView *self)
{
  (void) button;
  g_signal_emit (self, signals[SIGNAL_JUMP_ACTIVATED], 0);
}

static void
jump_activated_default (AwraConversationView *self)
{
  awra_conversation_view_scroll_to_end (self, TRUE);
}

/* Widget. */

static void
edge_retry_cb (GtkButton            *button,
               AwraConversationView *self)
{
  GtkPositionType edge = button == GTK_BUTTON (self->edge_retries[0]) ? GTK_POS_TOP : GTK_POS_BOTTOM;

  g_signal_emit (self, signals[SIGNAL_RETRY], 0, edge);
}

static void
make_edge (AwraConversationView *self,
           guint                 index)
{
  GtkWidget *line = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *revealer = gtk_revealer_new ();

  self->edge_spinners[index] = gtk_spinner_new ();
  self->edge_icons[index] = gtk_image_new_from_icon_name ("dialog-warning-symbolic");
  self->edge_labels[index] = GTK_LABEL (gtk_label_new (NULL));
  gtk_label_set_ellipsize (self->edge_labels[index], PANGO_ELLIPSIZE_END);
  self->edge_retries[index] = awra_button_new_with_label (_("Try again"));
  awra_button_set_appearance (AWRA_BUTTON (self->edge_retries[index]), AWRA_BUTTON_APPEARANCE_GHOST);
  gtk_widget_add_css_class (self->edge_retries[index], "awra-conversation-edge-retry");
  g_signal_connect (self->edge_retries[index], "clicked", G_CALLBACK (edge_retry_cb), self);
  gtk_box_append (GTK_BOX (line), self->edge_spinners[index]);
  gtk_box_append (GTK_BOX (line), self->edge_icons[index]);
  gtk_box_append (GTK_BOX (line), GTK_WIDGET (self->edge_labels[index]));
  gtk_box_append (GTK_BOX (line), self->edge_retries[index]);
  gtk_widget_add_css_class (line, "awra-conversation-edge");
  gtk_accessible_update_property (GTK_ACCESSIBLE (line), GTK_ACCESSIBLE_PROPERTY_LABEL, "", -1);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer),
                                    index == 0 ? GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN
                                               : GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 180);
  gtk_revealer_set_child (GTK_REVEALER (revealer), line);
  gtk_widget_set_parent (revealer, GTK_WIDGET (self));
  self->edges[index] = revealer;
}

static void
awra_conversation_view_measure (GtkWidget      *widget,
                                GtkOrientation  orientation,
                                int             for_size,
                                int            *minimum,
                                int            *natural,
                                int            *minimum_baseline,
                                int            *natural_baseline)
{
  AwraConversationView *self = AWRA_CONVERSATION_VIEW (widget);

  /* The pill floats: it never widens the view. */
  gtk_widget_measure (GTK_WIDGET (self->scroller), orientation, for_size,
                      minimum, natural, minimum_baseline, natural_baseline);
}

static void
awra_conversation_view_size_allocate (GtkWidget *widget,
                                      int        width,
                                      int        height,
                                      int        baseline)
{
  AwraConversationView *self = AWRA_CONVERSATION_VIEW (widget);
  int pill_min = 0;
  int pill_width = 0;
  int pill_height = 0;

  gtk_widget_allocate (GTK_WIDGET (self->scroller), width, height, baseline, NULL);
  gtk_widget_measure (self->pill, GTK_ORIENTATION_HORIZONTAL, -1, &pill_min, &pill_width, NULL, NULL);
  pill_width = CLAMP (pill_width, pill_min, MAX (pill_min, width - 2 * PILL_MARGIN));
  gtk_widget_measure (self->pill, GTK_ORIENTATION_VERTICAL, pill_width, NULL, &pill_height, NULL, NULL);
  gtk_widget_allocate (self->pill, pill_width, pill_height, -1,
                       gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (
                         (width - pill_width) / 2.0f,
                         MAX (0, height - pill_height - PILL_MARGIN))));
  for (guint i = 0; i < 2; i++) {
    int edge_min = 0;
    int edge_width = 0;
    int edge_height = 0;
    float y;

    gtk_widget_measure (self->edges[i], GTK_ORIENTATION_HORIZONTAL, -1, &edge_min, &edge_width, NULL, NULL);
    edge_width = CLAMP (edge_width, edge_min, MAX (edge_min, width - 2 * PILL_MARGIN));
    gtk_widget_measure (self->edges[i], GTK_ORIENTATION_VERTICAL, edge_width, NULL, &edge_height, NULL, NULL);
    /* The bottom one sits above the jump pill's place, clear of it. */
    y = i == 0 ? PILL_MARGIN : MAX (0, height - edge_height - pill_height - 2 * PILL_MARGIN);
    gtk_widget_allocate (self->edges[i], edge_width, edge_height, -1,
                         gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT ((width - edge_width) / 2.0f, y)));
  }
  awra_widget_present_popovers (widget);
}

static void
awra_conversation_view_snapshot (GtkWidget   *widget,
                                 GtkSnapshot *snapshot)
{
  AwraConversationView *self = AWRA_CONVERSATION_VIEW (widget);
  float width = gtk_widget_get_width (widget);
  float height = gtk_widget_get_height (widget);
  double shown;

  /* Content scrolled past the top edge fades out instead of being cut. */
  if (gtk_adjustment_get_value (self->vadjustment) > 0.5 && height > 2 * TOP_FADE) {
    static const GskColorStop stops[] = {
      { 0.0f, { 0, 0, 0, 0 } },
      { 1.0f, { 0, 0, 0, 1 } },
    };

    gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_ALPHA);
    gtk_snapshot_append_linear_gradient (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, TOP_FADE),
                                         &GRAPHENE_POINT_INIT (0, 0),
                                         &GRAPHENE_POINT_INIT (0, TOP_FADE),
                                         stops, G_N_ELEMENTS (stops));
    gtk_snapshot_append_color (snapshot, &(GdkRGBA) { 0, 0, 0, 1 },
                               &GRAPHENE_RECT_INIT (0, TOP_FADE, width, height - TOP_FADE));
    gtk_snapshot_pop (snapshot);
    gtk_widget_snapshot_child (widget, GTK_WIDGET (self->scroller), snapshot);
    gtk_snapshot_pop (snapshot);
  } else {
    gtk_widget_snapshot_child (widget, GTK_WIDGET (self->scroller), snapshot);
  }
  gtk_widget_snapshot_child (widget, self->edges[0], snapshot);
  gtk_widget_snapshot_child (widget, self->edges[1], snapshot);
  if (self->pill_progress <= 0.0 || !gtk_widget_get_child_visible (self->pill))
    return;
  shown = awra_motion_ease_out_cubic (self->pill_progress);
  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0, (1.0 - shown) * PILL_RISE));
  gtk_snapshot_push_opacity (snapshot, shown);
  gtk_widget_snapshot_child (widget, self->pill, snapshot);
  gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);
}

static void
awra_conversation_view_map (GtkWidget *widget)
{
  AwraConversationView *self = AWRA_CONVERSATION_VIEW (widget);

  GTK_WIDGET_CLASS (awra_conversation_view_parent_class)->map (widget);
  self->clock = g_object_ref (gtk_widget_get_frame_clock (widget));
  /* Connected after the surface's own layout handler, which allocates. */
  self->layout_handler = g_signal_connect (self->clock, "layout",
                                           G_CALLBACK (frame_layout_cb), self);
  self->paint_handler = g_signal_connect (self->clock, "after-paint",
                                          G_CALLBACK (frame_paint_cb), self);
  self->range_generation = self->bound_generation - 1;
  gtk_widget_queue_allocate (GTK_WIDGET (self->delegate));
}

static void
disconnect_clock (AwraConversationView *self)
{
  if (self->clock == NULL)
    return;
  g_clear_signal_handler (&self->layout_handler, self->clock);
  g_clear_signal_handler (&self->paint_handler, self->clock);
  g_clear_object (&self->clock);
}

static void
awra_conversation_view_unmap (GtkWidget *widget)
{
  AwraConversationView *self = AWRA_CONVERSATION_VIEW (widget);

  disconnect_clock (self);
  glide_stop (self);
  if (self->pill_tick != 0) {
    gtk_widget_remove_tick_callback (widget, self->pill_tick);
    self->pill_tick = 0;
    self->pill_progress = self->pill_shown ? 1.0 : 0.0;
    gtk_widget_set_child_visible (self->pill, self->pill_shown);
  }
  g_ptr_array_set_size (self->arriving, 0);
  GTK_WIDGET_CLASS (awra_conversation_view_parent_class)->unmap (widget);
}

static void
awra_conversation_view_dispose (GObject *object)
{
  AwraConversationView *self = AWRA_CONVERSATION_VIEW (object);

  disconnect_clock (self);
  glide_stop (self);
  if (self->pill_tick != 0) {
    gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->pill_tick);
    self->pill_tick = 0;
  }
  marks_clear (self);
  if (self->delegate != NULL) {
    /* Rows are torn down while the application's factory is still known. */
    gtk_list_view_set_factory (self->delegate, NULL);
    gtk_list_view_set_model (self->delegate, NULL);
  }
  if (self->vadjustment != NULL) {
    g_signal_handlers_disconnect_by_data (self->vadjustment, self);
    g_clear_object (&self->vadjustment);
  }
  if (self->selection != NULL) {
    g_signal_handlers_disconnect_by_data (self->selection, self);
    g_clear_object (&self->selection);
  }
  if (self->wrapper != NULL) {
    g_signal_handlers_disconnect_by_data (self->wrapper, self);
    g_clear_object (&self->wrapper);
  }
  g_clear_object (&self->model);
  g_clear_object (&self->factory);
  g_clear_object (&self->header_factory);
  g_clear_object (&self->highlighted);
  if (self->arriving != NULL)
    g_ptr_array_set_size (self->arriving, 0);
  if (self->scroller != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->scroller));
    self->scroller = NULL;
    self->port = NULL;
    self->delegate = NULL;
  }
  g_clear_pointer (&self->pill, gtk_widget_unparent);
  g_clear_pointer (&self->edges[0], gtk_widget_unparent);
  g_clear_pointer (&self->edges[1], gtk_widget_unparent);
  G_OBJECT_CLASS (awra_conversation_view_parent_class)->dispose (object);
}

static void
awra_conversation_view_finalize (GObject *object)
{
  AwraConversationView *self = AWRA_CONVERSATION_VIEW (object);

  g_ptr_array_unref (self->bound);
  g_ptr_array_unref (self->arriving);
  g_array_unref (self->marks);
  g_free (self->jump_label);
  G_OBJECT_CLASS (awra_conversation_view_parent_class)->finalize (object);
}

static void
awra_conversation_view_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  AwraConversationView *self = AWRA_CONVERSATION_VIEW (object);

  switch (prop_id) {
  case PROP_MODEL:
    g_value_set_object (value, self->model);
    break;
  case PROP_FACTORY:
    g_value_set_object (value, self->factory);
    break;
  case PROP_HEADER_FACTORY:
    g_value_set_object (value, self->header_factory);
    break;
  case PROP_AT_BOTTOM:
    g_value_set_boolean (value, self->at_bottom);
    break;
  case PROP_FOLLOW:
    g_value_set_boolean (value, self->follow);
    break;
  case PROP_UNREAD_COUNT:
    g_value_set_uint (value, self->unread_count);
    break;
  case PROP_JUMP_LABEL:
    g_value_set_string (value, self->jump_label);
    break;
  case PROP_EDGE_THRESHOLD:
    g_value_set_int (value, self->edge_threshold);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_conversation_view_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  AwraConversationView *self = AWRA_CONVERSATION_VIEW (object);

  switch (prop_id) {
  case PROP_MODEL:
    awra_conversation_view_set_model (self, g_value_get_object (value));
    break;
  case PROP_FACTORY:
    awra_conversation_view_set_factory (self, g_value_get_object (value));
    break;
  case PROP_HEADER_FACTORY:
    awra_conversation_view_set_header_factory (self, g_value_get_object (value));
    break;
  case PROP_FOLLOW:
    awra_conversation_view_set_follow (self, g_value_get_boolean (value));
    break;
  case PROP_UNREAD_COUNT:
    awra_conversation_view_set_unread_count (self, g_value_get_uint (value));
    break;
  case PROP_JUMP_LABEL:
    awra_conversation_view_set_jump_label (self, g_value_get_string (value));
    break;
  case PROP_EDGE_THRESHOLD:
    awra_conversation_view_set_edge_threshold (self, g_value_get_int (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_conversation_view_class_init (AwraConversationViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_conversation_view_dispose;
  object_class->finalize = awra_conversation_view_finalize;
  object_class->get_property = awra_conversation_view_get_property;
  object_class->set_property = awra_conversation_view_set_property;
  widget_class->measure = awra_conversation_view_measure;
  widget_class->size_allocate = awra_conversation_view_size_allocate;
  widget_class->snapshot = awra_conversation_view_snapshot;
  widget_class->map = awra_conversation_view_map;
  widget_class->unmap = awra_conversation_view_unmap;

  /**
   * AwraConversationView:model:
   *
   * The items, oldest first.
   *
   * Since: 1.1
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL, G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraConversationView:factory:
   *
   * The factory that makes rows: a #GtkSignalListItemFactory or a
   * #GtkBuilderListItemFactory.
   *
   * Since: 1.1
   */
  properties[PROP_FACTORY] =
    g_param_spec_object ("factory", NULL, NULL, GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraConversationView:header-factory:
   *
   * The factory that makes section headers, for a model that implements
   * #GtkSectionModel.
   *
   * Since: 1.1
   */
  properties[PROP_HEADER_FACTORY] =
    g_param_spec_object ("header-factory", NULL, NULL, GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraConversationView:at-bottom:
   *
   * Whether the end of the timeline is in view, or the view is gliding to it.
   *
   * Since: 1.1
   */
  properties[PROP_AT_BOTTOM] =
    g_param_spec_boolean ("at-bottom", NULL, NULL, TRUE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraConversationView:follow:
   *
   * Whether the view keeps the newest items in view as they are appended.
   *
   * Since: 1.1
   */
  properties[PROP_FOLLOW] =
    g_param_spec_boolean ("follow", NULL, NULL, TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraConversationView:unread-count:
   *
   * The number of unread items the jump pill announces.
   *
   * Since: 1.1
   */
  properties[PROP_UNREAD_COUNT] =
    g_param_spec_uint ("unread-count", NULL, NULL, 0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraConversationView:jump-label:
   *
   * A label that replaces the jump pill's own, or %NULL.
   *
   * Since: 1.1
   */
  properties[PROP_JUMP_LABEL] =
    g_param_spec_string ("jump-label", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraConversationView:edge-threshold:
   *
   * The distance from an end at which #AwraConversationView::edge-reached is
   * emitted, in pixels, or -1 for the height of the view.
   *
   * Since: 1.1
   */
  properties[PROP_EDGE_THRESHOLD] =
    g_param_spec_int ("edge-threshold", NULL, NULL, -1, G_MAXINT, -1,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * AwraConversationView::edge-reached:
   * @self: the conversation view
   * @edge: %GTK_POS_TOP or %GTK_POS_BOTTOM
   *
   * Emitted once when scrolling comes within
   * #AwraConversationView:edge-threshold of an end. At the top, it is the time
   * to load older items; at the bottom, newer ones when the model holds a
   * window of history that does not reach the present. It is emitted again
   * after leaving the edge, or when items were inserted at that end.
   *
   * Since: 1.1
   */
  signals[SIGNAL_EDGE_REACHED] =
    g_signal_new ("edge-reached", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 1, GTK_TYPE_POSITION_TYPE);
  /**
   * AwraConversationView::retry:
   * @self: the conversation view
   * @edge: %GTK_POS_TOP or %GTK_POS_BOTTOM
   *
   * The person asked to load the failed edge again.
   *
   * Since: 1.1
   */
  signals[SIGNAL_RETRY] =
    g_signal_new ("retry", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 1, GTK_TYPE_POSITION_TYPE);
  /**
   * AwraConversationView::visible-range-changed:
   * @self: the conversation view
   *
   * Emitted at most once per frame when the visible items change; read them
   * with awra_conversation_view_get_visible_range(), for example to mark them
   * as read.
   *
   * Since: 1.1
   */
  signals[SIGNAL_VISIBLE_RANGE_CHANGED] =
    g_signal_new ("visible-range-changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
  /**
   * AwraConversationView::jump-activated:
   * @self: the conversation view
   *
   * Emitted when the jump pill is clicked. The default handler scrolls to the
   * end; an application showing a window of history that does not reach the
   * present loads it first and stops the emission.
   *
   * Since: 1.1
   */
  signals[SIGNAL_JUMP_ACTIVATED] =
    g_signal_new_class_handler ("jump-activated", G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (jump_activated_default),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, "awraconversationview");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
awra_conversation_view_init (AwraConversationView *self)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  GtkEventController *keys = gtk_event_controller_key_new ();

  self->follow = TRUE;
  self->at_bottom = TRUE;
  self->top_armed = TRUE;
  self->bottom_armed = TRUE;
  self->edge_threshold = -1;
  self->align_position = GTK_INVALID_LIST_POSITION;
  self->glide_item = GTK_INVALID_LIST_POSITION;
  self->bound = g_ptr_array_new ();
  self->arriving = g_ptr_array_new_with_free_func (g_object_unref);
  self->marks = g_array_new (FALSE, FALSE, sizeof (Mark));

  self->wrapper = gtk_signal_list_item_factory_new ();
  g_signal_connect (self->wrapper, "setup", G_CALLBACK (wrapper_setup_cb), self);
  g_signal_connect (self->wrapper, "bind", G_CALLBACK (wrapper_bind_cb), self);
  g_signal_connect (self->wrapper, "unbind", G_CALLBACK (wrapper_unbind_cb), self);
  g_signal_connect (self->wrapper, "teardown", G_CALLBACK (wrapper_teardown_cb), self);

  self->delegate = GTK_LIST_VIEW (gtk_list_view_new (NULL, NULL));
  gtk_widget_add_css_class (GTK_WIDGET (self->delegate), "awra-conversation-list");
  self->port = awra_conversation_port_new (GTK_WIDGET (self->delegate));
  self->scroller = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new ());
  gtk_scrolled_window_set_policy (self->scroller, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_overlay_scrolling (self->scroller, TRUE);
  gtk_scrolled_window_set_child (self->scroller, self->port);
  gtk_widget_set_parent (GTK_WIDGET (self->scroller), GTK_WIDGET (self));
  self->vadjustment = g_object_ref (gtk_scrolled_window_get_vadjustment (self->scroller));
  g_signal_connect (self->vadjustment, "value-changed",
                    G_CALLBACK (adjustment_value_changed_cb), self);

  self->pill = awra_button_new ();
  gtk_widget_add_css_class (self->pill, "awra-conversation-jump");
  gtk_box_append (GTK_BOX (box), gtk_image_new_from_icon_name ("go-down-symbolic"));
  self->pill_label = GTK_LABEL (gtk_label_new (NULL));
  gtk_label_set_ellipsize (self->pill_label, PANGO_ELLIPSIZE_END);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (self->pill_label));
  gtk_button_set_child (GTK_BUTTON (self->pill), box);
  gtk_widget_set_parent (self->pill, GTK_WIDGET (self));
  gtk_widget_set_child_visible (self->pill, FALSE);
  g_signal_connect (self->pill, "clicked", G_CALLBACK (pill_clicked_cb), self);
  update_pill_label (self);
  for (guint i = 0; i < 2; i++)
    make_edge (self, i);

  gtk_event_controller_set_propagation_phase (keys, GTK_PHASE_CAPTURE);
  g_signal_connect (keys, "key-pressed", G_CALLBACK (key_pressed_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), keys);
}

GtkWidget *
awra_conversation_view_new (GListModel         *model,
                            GtkListItemFactory *factory)
{
  GtkWidget *self;

  g_return_val_if_fail (model == NULL || G_IS_LIST_MODEL (model), NULL);
  g_return_val_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory), NULL);
  self = g_object_new (AWRA_TYPE_CONVERSATION_VIEW,
                       "model", model,
                       "factory", factory,
                       NULL);
  g_clear_object (&model);
  g_clear_object (&factory);
  return self;
}

GListModel *
awra_conversation_view_get_model (AwraConversationView *self)
{
  g_return_val_if_fail (AWRA_IS_CONVERSATION_VIEW (self), NULL);
  return self->model;
}

void
awra_conversation_view_set_model (AwraConversationView *self,
                                  GListModel           *model)
{
  g_return_if_fail (AWRA_IS_CONVERSATION_VIEW (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));
  if (self->model == model)
    return;
  if (self->selection != NULL) {
    g_signal_handlers_disconnect_by_data (self->selection, self);
    g_clear_object (&self->selection);
  }
  g_set_object (&self->model, model);
  g_ptr_array_set_size (self->arriving, 0);
  g_clear_object (&self->highlighted);
  glide_stop (self);
  self->align_position = GTK_INVALID_LIST_POSITION;
  self->n_items = model != NULL ? g_list_model_get_n_items (model) : 0;
  self->top_armed = TRUE;
  self->bottom_armed = TRUE;
  self->snapped_with_scroll_to = FALSE;
  self->reach_end = TRUE;
  if (model != NULL) {
    self->selection = gtk_no_selection_new (g_object_ref (model));
    g_signal_connect (self->selection, "items-changed",
                      G_CALLBACK (items_changed_cb), self);
  }
  gtk_list_view_set_model (self->delegate, GTK_SELECTION_MODEL (self->selection));
  set_follow_internal (self, TRUE);
  update_at_bottom (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

GtkListItemFactory *
awra_conversation_view_get_factory (AwraConversationView *self)
{
  g_return_val_if_fail (AWRA_IS_CONVERSATION_VIEW (self), NULL);
  return self->factory;
}

void
awra_conversation_view_set_factory (AwraConversationView *self,
                                    GtkListItemFactory   *factory)
{
  g_return_if_fail (AWRA_IS_CONVERSATION_VIEW (self));
  g_return_if_fail (factory == NULL ||
                    GTK_IS_SIGNAL_LIST_ITEM_FACTORY (factory) ||
                    GTK_IS_BUILDER_LIST_ITEM_FACTORY (factory));
  if (self->factory == factory)
    return;
  /* The old rows are torn down by the factory that made them. */
  gtk_list_view_set_factory (self->delegate, NULL);
  g_set_object (&self->factory, factory);
  install_factory (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

GtkListItemFactory *
awra_conversation_view_get_header_factory (AwraConversationView *self)
{
  g_return_val_if_fail (AWRA_IS_CONVERSATION_VIEW (self), NULL);
  return self->header_factory;
}

void
awra_conversation_view_set_header_factory (AwraConversationView *self,
                                           GtkListItemFactory   *factory)
{
  g_return_if_fail (AWRA_IS_CONVERSATION_VIEW (self));
  g_return_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory));
  if (!g_set_object (&self->header_factory, factory))
    return;
  gtk_list_view_set_header_factory (self->delegate, factory);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HEADER_FACTORY]);
}

GtkListView *
awra_conversation_view_get_delegate (AwraConversationView *self)
{
  g_return_val_if_fail (AWRA_IS_CONVERSATION_VIEW (self), NULL);
  return self->delegate;
}

gboolean
awra_conversation_view_get_at_bottom (AwraConversationView *self)
{
  g_return_val_if_fail (AWRA_IS_CONVERSATION_VIEW (self), TRUE);
  return self->at_bottom;
}

gboolean
awra_conversation_view_get_follow (AwraConversationView *self)
{
  g_return_val_if_fail (AWRA_IS_CONVERSATION_VIEW (self), TRUE);
  return self->follow;
}

void
awra_conversation_view_set_follow (AwraConversationView *self,
                                   gboolean              follow)
{
  g_return_if_fail (AWRA_IS_CONVERSATION_VIEW (self));
  set_follow_internal (self, follow);
  self->reach_end = follow;
  if (follow && gtk_widget_get_mapped (GTK_WIDGET (self)))
    gtk_widget_queue_allocate (GTK_WIDGET (self->delegate));
}

guint
awra_conversation_view_get_unread_count (AwraConversationView *self)
{
  g_return_val_if_fail (AWRA_IS_CONVERSATION_VIEW (self), 0);
  return self->unread_count;
}

void
awra_conversation_view_set_unread_count (AwraConversationView *self,
                                         guint                 count)
{
  g_return_if_fail (AWRA_IS_CONVERSATION_VIEW (self));
  if (self->unread_count == count)
    return;
  self->unread_count = count;
  update_pill_label (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UNREAD_COUNT]);
}

const char *
awra_conversation_view_get_jump_label (AwraConversationView *self)
{
  g_return_val_if_fail (AWRA_IS_CONVERSATION_VIEW (self), NULL);
  return self->jump_label;
}

void
awra_conversation_view_set_jump_label (AwraConversationView *self,
                                       const char           *label)
{
  g_return_if_fail (AWRA_IS_CONVERSATION_VIEW (self));
  if (!g_set_str (&self->jump_label, label))
    return;
  update_pill_label (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_JUMP_LABEL]);
}

int
awra_conversation_view_get_edge_threshold (AwraConversationView *self)
{
  g_return_val_if_fail (AWRA_IS_CONVERSATION_VIEW (self), -1);
  return self->edge_threshold;
}

void
awra_conversation_view_set_edge_threshold (AwraConversationView *self,
                                           int                   threshold)
{
  g_return_if_fail (AWRA_IS_CONVERSATION_VIEW (self));
  g_return_if_fail (threshold >= -1);
  if (self->edge_threshold == threshold)
    return;
  self->edge_threshold = threshold;
  self->range_generation = self->bound_generation - 1;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EDGE_THRESHOLD]);
}

void
awra_conversation_view_scroll_to_end (AwraConversationView *self,
                                      gboolean              animate)
{
  double page;
  guint duration;

  g_return_if_fail (AWRA_IS_CONVERSATION_VIEW (self));
  self->align_position = GTK_INVALID_LIST_POSITION;
  set_follow_internal (self, TRUE);
  if (self->n_items == 0 || !gtk_widget_get_mapped (GTK_WIDGET (self))) {
    glide_stop (self);
    return;
  }
  page = gtk_adjustment_get_page_size (self->vadjustment);
  duration = animate ? motion_duration (self, AWRA_MOTION_PRESET_SLOW) : 0;
  if (duration == 0) {
    glide_stop (self);
    self->snapped_with_scroll_to = FALSE;
    snap_to_end (self);
    update_at_bottom (self);
    return;
  }
  /* Close the distance at once and glide the last stretch. */
  if (distance_to_end (self) > page * GLIDE_PAGES)
    set_value (self, end_value (self) - page * GLIDE_PAGES / 2.0);
  glide_start (self, TRUE, 0.0, duration);
  update_at_bottom (self);
}

void
awra_conversation_view_scroll_to_item (AwraConversationView  *self,
                                       guint                  position,
                                       AwraConversationAlign  align,
                                       gboolean               highlight)
{
  GtkListItem *item;
  GtkWidget *child;
  double offset;

  g_return_if_fail (AWRA_IS_CONVERSATION_VIEW (self));
  g_return_if_fail (align <= AWRA_CONVERSATION_ALIGN_END);
  g_return_if_fail (position < self->n_items);

  glide_stop (self);
  set_follow_internal (self, FALSE);
  if (highlight) {
    g_clear_object (&self->highlighted);
    self->highlighted = g_list_model_get_item (self->model, position);
    self->highlight_until = g_get_monotonic_time () + (gint64) HIGHLIGHT_CLASS_MS * 1000;
  }
  item = find_bound (self, position);
  child = item != NULL ? row_widget (item) : NULL;
  if (highlight && child != NULL)
    highlight_child (self, child);
  self->align = align;
  self->align_attempts = 0;
  self->align_position = GTK_INVALID_LIST_POSITION;
  if (item_offset (self, position, align, &offset)) {
    double page = gtk_adjustment_get_page_size (self->vadjustment);
    guint duration = motion_duration (self, AWRA_MOTION_PRESET_SLOW);

    if (duration > 0 && fabs (offset) <= page * GLIDE_PAGES && fabs (offset) >= 1.0) {
      glide_start_item (self, position, align, offset, duration);
      return;
    }
    self->align_position = position;
    set_value (self, gtk_adjustment_get_value (self->vadjustment) + offset);
    return;
  }
  /* Not built yet: let the list bring it in, then align it before the frame
   * is painted. */
  self->align_position = position;
  gtk_list_view_scroll_to (self->delegate, position, GTK_LIST_SCROLL_NONE, NULL);
}

gboolean
awra_conversation_view_get_visible_range (AwraConversationView *self,
                                          guint                *first,
                                          guint                *last)
{
  guint low = 0;
  guint high = 0;
  gboolean valid;

  g_return_val_if_fail (AWRA_IS_CONVERSATION_VIEW (self), FALSE);
  valid = compute_visible_range (self, &low, &high);
  if (first != NULL)
    *first = valid ? low : GTK_INVALID_LIST_POSITION;
  if (last != NULL)
    *last = valid ? high : GTK_INVALID_LIST_POSITION;
  return valid;
}

AwraEdgeStatus
awra_conversation_view_get_edge_status (AwraConversationView *self,
                                        GtkPositionType       edge)
{
  g_return_val_if_fail (AWRA_IS_CONVERSATION_VIEW (self), AWRA_EDGE_STATUS_NONE);
  g_return_val_if_fail (edge == GTK_POS_TOP || edge == GTK_POS_BOTTOM, AWRA_EDGE_STATUS_NONE);
  return self->edge_status[edge == GTK_POS_TOP ? 0 : 1];
}

void
awra_conversation_view_set_edge_status (AwraConversationView *self,
                                        GtkPositionType       edge,
                                        AwraEdgeStatus        status,
                                        const char           *label)
{
  guint i;
  gboolean failed;

  g_return_if_fail (AWRA_IS_CONVERSATION_VIEW (self));
  g_return_if_fail (edge == GTK_POS_TOP || edge == GTK_POS_BOTTOM);
  g_return_if_fail (status <= AWRA_EDGE_STATUS_FAILED);
  i = edge == GTK_POS_TOP ? 0 : 1;
  failed = status == AWRA_EDGE_STATUS_FAILED;
  if (status != AWRA_EDGE_STATUS_NONE) {
    gtk_label_set_text (self->edge_labels[i], label != NULL ? label : "");
    gtk_widget_set_visible (GTK_WIDGET (self->edge_labels[i]), label != NULL && *label != '\0');
    gtk_widget_set_visible (self->edge_spinners[i], !failed);
    gtk_spinner_set_spinning (GTK_SPINNER (self->edge_spinners[i]), !failed);
    gtk_widget_set_visible (self->edge_icons[i], failed);
    gtk_widget_set_visible (self->edge_retries[i], failed);
    gtk_accessible_update_property (GTK_ACCESSIBLE (gtk_revealer_get_child (GTK_REVEALER (self->edges[i]))),
                                    GTK_ACCESSIBLE_PROPERTY_LABEL, label != NULL ? label : "", -1);
  } else {
    gtk_spinner_set_spinning (GTK_SPINNER (self->edge_spinners[i]), FALSE);
  }
  if (failed)
    gtk_widget_add_css_class (self->edges[i], "awra-failed");
  else
    gtk_widget_remove_css_class (self->edges[i], "awra-failed");
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->edges[i]), status != AWRA_EDGE_STATUS_NONE);
  /* Idle again: reaching the edge from here loads, as after leaving it. */
  if (self->edge_status[i] != AWRA_EDGE_STATUS_NONE && status == AWRA_EDGE_STATUS_NONE) {
    if (i == 0)
      self->top_armed = TRUE;
    else
      self->bottom_armed = TRUE;
  }
  self->edge_status[i] = status;
}
