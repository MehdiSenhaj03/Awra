/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <awra/awra-context.h>
#include <awra/awra-layout.h>
#include <awra/awra-motion.h>
#include <awra/awra-tab-view.h>

#include <math.h>

#include "awra-i18n-private.h"

typedef struct {
  GtkWidget parent_instance;
  GtkLabel *label;
  gboolean overflowing;
} AwraTabTitle;

typedef GtkWidgetClass AwraTabTitleClass;

#define AWRA_TYPE_TAB_TITLE (awra_tab_title_get_type ())
#define AWRA_TAB_TITLE(obj) ((AwraTabTitle *) (obj))

G_DEFINE_TYPE (AwraTabTitle, awra_tab_title, GTK_TYPE_WIDGET)

static void
awra_tab_title_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum,
                        int            *natural,
                        int            *minimum_baseline,
                        int            *natural_baseline)
{
  AwraTabTitle *self = AWRA_TAB_TITLE (widget);

  gtk_widget_measure (GTK_WIDGET (self->label), orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum = 0;
}

static void
awra_tab_title_size_allocate (GtkWidget *widget,
                              int        width,
                              int        height,
                              int        baseline)
{
  AwraTabTitle *self = AWRA_TAB_TITLE (widget);
  int natural_width;
  int child_width;
  float x;
  graphene_point_t offset;

  gtk_widget_measure (GTK_WIDGET (self->label),
                      GTK_ORIENTATION_HORIZONTAL,
                      height, NULL, &natural_width, NULL, NULL);
  self->overflowing = natural_width > width;
  child_width = MAX (width, natural_width);
  if (!self->overflowing)
    x = (width - child_width) / 2.0f;
  else if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    x = width - child_width;
  else
    x = 0.0f;
  offset = GRAPHENE_POINT_INIT (x, 0.0f);
  gtk_widget_allocate (GTK_WIDGET (self->label), child_width, height,
                       baseline, gsk_transform_translate (NULL, &offset));
}

static void
awra_tab_title_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  AwraTabTitle *self = AWRA_TAB_TITLE (widget);
  float width = gtk_widget_get_width (widget);
  float height = gtk_widget_get_height (widget);
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, width, height);

  gtk_snapshot_push_clip (snapshot, &bounds);
  if (self->overflowing && width > 0.0f) {
    const GdkRGBA opaque = { 1.0, 1.0, 1.0, 1.0 };
    const GdkRGBA transparent = { 1.0, 1.0, 1.0, 0.0 };
    float fade_start = MAX (0.0f, (width - 18.0f) / width);
    GskColorStop stops[] = {
      { 0.0f, opaque },
      { fade_start, opaque },
      { 1.0f, transparent },
    };
    graphene_point_t start;
    graphene_point_t end;

    if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) {
      start = GRAPHENE_POINT_INIT (width, height / 2.0f);
      end = GRAPHENE_POINT_INIT (0.0f, height / 2.0f);
    } else {
      start = GRAPHENE_POINT_INIT (0.0f, height / 2.0f);
      end = GRAPHENE_POINT_INIT (width, height / 2.0f);
    }
    gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_ALPHA);
    gtk_snapshot_append_linear_gradient (snapshot, &bounds, &start, &end,
                                         stops, G_N_ELEMENTS (stops));
    gtk_snapshot_pop (snapshot);
    gtk_widget_snapshot_child (widget, GTK_WIDGET (self->label), snapshot);
    gtk_snapshot_pop (snapshot);
  } else {
    gtk_widget_snapshot_child (widget, GTK_WIDGET (self->label), snapshot);
  }
  gtk_snapshot_pop (snapshot);
}

static void
awra_tab_title_dispose (GObject *object)
{
  AwraTabTitle *self = AWRA_TAB_TITLE (object);

  if (self->label != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->label));
    self->label = NULL;
  }
  G_OBJECT_CLASS (awra_tab_title_parent_class)->dispose (object);
}

static void
awra_tab_title_class_init (AwraTabTitleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_tab_title_dispose;
  widget_class->measure = awra_tab_title_measure;
  widget_class->size_allocate = awra_tab_title_size_allocate;
  widget_class->snapshot = awra_tab_title_snapshot;
  gtk_widget_class_set_css_name (widget_class, "awratabtitle");
}

static void
awra_tab_title_init (AwraTabTitle *self)
{
  self->label = GTK_LABEL (gtk_label_new (NULL));
  gtk_label_set_single_line_mode (self->label, TRUE);
  gtk_label_set_xalign (self->label, 0.0f);
  gtk_widget_set_parent (GTK_WIDGET (self->label), GTK_WIDGET (self));
}

static GtkWidget *
awra_tab_title_new (const char *text)
{
  AwraTabTitle *self = g_object_new (AWRA_TYPE_TAB_TITLE, NULL);

  gtk_label_set_text (self->label, text);
  return GTK_WIDGET (self);
}

/* GtkScrolledWindow clips its child at the viewport boundary. That is the
 * correct input behaviour, but a highlighted tab then ends on a hard vertical
 * cut when it is only partly visible. Keep the native scroller and place it in
 * a tiny rendering wrapper that softens only the overflowing physical edge.
 * The mask is private implementation detail: applications still interact with
 * AwraTabView and never need to coordinate scrolling or edge effects. */
typedef struct {
  GtkWidget parent_instance;
  GtkScrolledWindow *scroller;
} AwraTabViewportMask;

typedef GtkWidgetClass AwraTabViewportMaskClass;

#define AWRA_TYPE_TAB_VIEWPORT_MASK (awra_tab_viewport_mask_get_type ())
#define AWRA_TAB_VIEWPORT_MASK(obj) ((AwraTabViewportMask *) (obj))

G_DEFINE_TYPE (AwraTabViewportMask,
               awra_tab_viewport_mask,
               GTK_TYPE_WIDGET)

static void
tab_viewport_adjustment_changed_cb (GtkAdjustment       *adjustment,
                                    AwraTabViewportMask *self)
{
  (void) adjustment;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
awra_tab_viewport_mask_snapshot (GtkWidget   *widget,
                                 GtkSnapshot *snapshot)
{
  AwraTabViewportMask *self = AWRA_TAB_VIEWPORT_MASK (widget);
  GtkAdjustment *adjustment;
  double value;
  double lower;
  double upper;
  double page_size;
  gboolean start_hidden;
  gboolean end_hidden;
  gboolean left_hidden;
  gboolean right_hidden;
  float width = gtk_widget_get_width (widget);
  float height = gtk_widget_get_height (widget);

  if (self->scroller == NULL)
    return;

  adjustment = gtk_scrolled_window_get_hadjustment (self->scroller);
  value = gtk_adjustment_get_value (adjustment);
  lower = gtk_adjustment_get_lower (adjustment);
  upper = gtk_adjustment_get_upper (adjustment);
  page_size = gtk_adjustment_get_page_size (adjustment);
  start_hidden = value > lower + 0.5;
  end_hidden = value + page_size < upper - 0.5;
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) {
    left_hidden = end_hidden;
    right_hidden = start_hidden;
  } else {
    left_hidden = start_hidden;
    right_hidden = end_hidden;
  }

  if (width <= 0.0f || height <= 0.0f ||
      (!left_hidden && !right_hidden)) {
    gtk_widget_snapshot_child (widget, GTK_WIDGET (self->scroller), snapshot);
    return;
  }

  {
    const GdkRGBA opaque = { 1.0, 1.0, 1.0, 1.0 };
    const GdkRGBA transparent = { 1.0, 1.0, 1.0, 0.0 };
    GskColorStop stops[4];
    guint n_stops = 0;
    float fade = MIN (0.5f, 20.0f / width);
    graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, width, height);
    graphene_point_t from = GRAPHENE_POINT_INIT (0, height / 2.0f);
    graphene_point_t to = GRAPHENE_POINT_INIT (width, height / 2.0f);

    if (left_hidden) {
      stops[n_stops++] = (GskColorStop) { 0.0f, transparent };
      stops[n_stops++] = (GskColorStop) { fade, opaque };
    } else {
      stops[n_stops++] = (GskColorStop) { 0.0f, opaque };
    }
    if (right_hidden) {
      stops[n_stops++] = (GskColorStop) { 1.0f - fade, opaque };
      stops[n_stops++] = (GskColorStop) { 1.0f, transparent };
    } else {
      stops[n_stops++] = (GskColorStop) { 1.0f, opaque };
    }

    gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_ALPHA);
    gtk_snapshot_append_linear_gradient (snapshot, &bounds, &from, &to,
                                         stops, n_stops);
    gtk_snapshot_pop (snapshot);
    gtk_widget_snapshot_child (widget, GTK_WIDGET (self->scroller), snapshot);
    gtk_snapshot_pop (snapshot);
  }
}

static void
awra_tab_viewport_mask_dispose (GObject *object)
{
  AwraTabViewportMask *self = AWRA_TAB_VIEWPORT_MASK (object);

  if (self->scroller != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->scroller));
    self->scroller = NULL;
  }
  G_OBJECT_CLASS (awra_tab_viewport_mask_parent_class)->dispose (object);
}

static void
awra_tab_viewport_mask_class_init (AwraTabViewportMaskClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_tab_viewport_mask_dispose;
  widget_class->snapshot = awra_tab_viewport_mask_snapshot;
  gtk_widget_class_set_layout_manager_type (widget_class,
                                            GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "awratabviewportmask");
}

static void
awra_tab_viewport_mask_init (AwraTabViewportMask *self)
{
  gtk_widget_add_css_class (GTK_WIDGET (self), "awra-tab-viewport-mask");
}

static GtkWidget *
awra_tab_viewport_mask_new (GtkScrolledWindow *scroller)
{
  AwraTabViewportMask *self;
  GtkAdjustment *adjustment;

  self = g_object_new (AWRA_TYPE_TAB_VIEWPORT_MASK, NULL);
  self->scroller = scroller;
  gtk_widget_set_parent (GTK_WIDGET (scroller), GTK_WIDGET (self));
  adjustment = gtk_scrolled_window_get_hadjustment (scroller);
  g_signal_connect_object (adjustment, "value-changed",
                           G_CALLBACK (tab_viewport_adjustment_changed_cb),
                           self, 0);
  g_signal_connect_object (adjustment, "changed",
                           G_CALLBACK (tab_viewport_adjustment_changed_cb),
                           self, 0);
  return GTK_WIDGET (self);
}

typedef struct {
  AwraTabView *view;
  GtkWidget *page;
  GtkWidget *tab;
  GtkOverlay *body;
  GtkToggleButton *select_button;
  GtkBox *select_content;
  GtkImage *icon;
  AwraTabTitle *title;
  GtkLabel *label;
  GtkWidget *close_button;
  GtkSeparator *separator;
  char *name;
  gboolean pinned;
  gboolean hovered;
  gboolean title_visible;
} AwraTabPage;

struct _AwraTabView {
  GtkWidget parent_instance;
  GtkBox *box;
  GtkBox *tab_bar;
  GtkBox *pinned_tabs;
  GtkWidget *tab_viewport_mask;
  GtkScrolledWindow *tab_scroller;
  GtkWidget *tab_strip;
  GtkStack *stack;
  GPtrArray *pages;
  guint next_page_id;
  guint ensure_visible_tick;
  gboolean scroll_selects;
  double scroll_steps;
  AwraStyleManager *style_manager;
  gulong style_handler;
};

enum {
  /* Keep peer destinations calm and legible. 118 px let one extra tab leak
   * into a typical data window, which made the separators read as a picket
   * fence and forced ordinary names through the overflow mask. The rail is
   * deliberately scrollable, so preserving this visual rhythm is preferable
   * to compressing another destination into view. */
  TAB_MIN_WIDTH = 132,
  TAB_MAX_WIDTH = 220,
  TAB_PINNED_ICON_SIZE = 36,
  TAB_PINNED_ICON_IMAGE_SIZE = 14,
  TAB_PAGE_ICON_SIZE = 22,
  TAB_CLOSE_SIZE = 22,
};

/* Seconds; about 150 ms to settle, quick enough to keep up with the pointer. */
#define TAB_SLIDE_TIME_CONSTANT 0.05

/* Every tab eases towards its slot, so reordering, closing and making room for
 * a placeholder share one motion. */
typedef struct {
  double x;
  double target;
  gboolean placed;
} TabSlot;

typedef struct {
  GtkWidget parent_instance;
  GPtrArray *children;
  GHashTable *slots;
  GtkWidget *placeholder;
  TabSlot placeholder_slot;
  double placeholder_opacity;
  int placeholder_position;
  GtkWidget *dragged;
  gboolean animated;
  guint tick;
  gint64 last_frame;
  int tab_width;
} AwraTabStrip;

typedef GtkWidgetClass AwraTabStripClass;

#define AWRA_TYPE_TAB_STRIP (awra_tab_strip_get_type ())
#define AWRA_TAB_STRIP(obj) ((AwraTabStrip *) (obj))

G_DEFINE_TYPE (AwraTabStrip, awra_tab_strip, GTK_TYPE_WIDGET)

static void
awra_tab_strip_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum,
                        int            *natural,
                        int            *minimum_baseline,
                        int            *natural_baseline)
{
  AwraTabStrip *self = AWRA_TAB_STRIP (widget);

  *minimum = 0;
  *natural = 0;
  if (minimum_baseline != NULL)
    *minimum_baseline = -1;
  if (natural_baseline != NULL)
    *natural_baseline = -1;

  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    int count = (int) self->children->len +
                (self->placeholder_position >= 0 && self->dragged == NULL ? 1 : 0);

    *minimum = count * TAB_MIN_WIDTH;
    *natural = count * TAB_MAX_WIDTH;
    return;
  }

  for (guint i = 0; i < self->children->len; i++) {
    GtkWidget *child = g_ptr_array_index (self->children, i);
    int child_minimum;
    int child_natural;

    gtk_widget_measure (child, orientation, for_size,
                        &child_minimum, &child_natural, NULL, NULL);
    *minimum = MAX (*minimum, child_minimum);
    *natural = MAX (*natural, child_natural);
  }
}

static gboolean
tab_slot_step (TabSlot *slot,
               double   progress)
{
  slot->x += (slot->target - slot->x) * progress;
  if (fabs (slot->target - slot->x) >= 0.5)
    return TRUE;
  slot->x = slot->target;
  return FALSE;
}

static gboolean
awra_tab_strip_tick_cb (GtkWidget     *widget,
                        GdkFrameClock *clock,
                        gpointer       user_data)
{
  AwraTabStrip *self = AWRA_TAB_STRIP (widget);
  gint64 now = gdk_frame_clock_get_frame_time (clock);
  double elapsed = self->last_frame != 0 ? (now - self->last_frame) / 1000000.0 : 0.016;
  double progress = 1.0 - exp (-elapsed / TAB_SLIDE_TIME_CONSTANT);
  gboolean moving = FALSE;
  GHashTableIter iter;
  gpointer slot;

  (void) user_data;
  self->last_frame = now;
  g_hash_table_iter_init (&iter, self->slots);
  while (g_hash_table_iter_next (&iter, NULL, &slot))
    moving |= tab_slot_step (slot, progress);
  if (self->placeholder_position >= 0) {
    moving |= tab_slot_step (&self->placeholder_slot, progress);
    self->placeholder_opacity += (1.0 - self->placeholder_opacity) * progress;
    if (self->placeholder_opacity > 0.99)
      self->placeholder_opacity = 1.0;
    else
      moving = TRUE;
    gtk_widget_set_opacity (self->placeholder, self->placeholder_opacity);
  }
  gtk_widget_queue_allocate (widget);
  if (moving)
    return G_SOURCE_CONTINUE;
  self->tick = 0;
  self->last_frame = 0;
  return G_SOURCE_REMOVE;
}

static void
awra_tab_strip_place (AwraTabStrip *self,
                      TabSlot      *slot,
                      double        target)
{
  slot->target = target;
  if (!slot->placed || !self->animated ||
      !gtk_widget_get_mapped (GTK_WIDGET (self))) {
    slot->x = target;
    slot->placed = TRUE;
  }
}

static void
awra_tab_strip_size_allocate (GtkWidget *widget,
                              int        width,
                              int        height,
                              int        baseline)
{
  AwraTabStrip *self = AWRA_TAB_STRIP (widget);
  guint count = self->children->len;
  gboolean extra = self->placeholder_position >= 0 && self->dragged == NULL;
  gboolean moving = FALSE;
  int tab_width;
  int position = 0;

  if (count == 0 && !extra)
    return;
  tab_width = CLAMP (width / (int) (count + (extra ? 1 : 0)),
                     TAB_MIN_WIDTH, TAB_MAX_WIDTH);
  self->tab_width = tab_width;

  for (guint i = 0; i < count; i++) {
    GtkWidget *child = g_ptr_array_index (self->children, i);
    TabSlot *slot = g_hash_table_lookup (self->slots, child);
    graphene_point_t offset;

    if (child == self->dragged) {
      awra_tab_strip_place (self, slot, slot->x);
    } else {
      if (position == self->placeholder_position)
        position++;
      awra_tab_strip_place (self, slot, position * tab_width);
      position++;
    }
    moving |= slot->x != slot->target;
    offset = GRAPHENE_POINT_INIT ((float) round (slot->x), 0.0f);
    gtk_widget_allocate (child, tab_width, height, baseline,
                         gsk_transform_translate (NULL, &offset));
  }

  if (self->placeholder_position >= 0) {
    graphene_point_t offset;

    awra_tab_strip_place (self, &self->placeholder_slot,
                          self->placeholder_position * tab_width);
    moving |= self->placeholder_slot.x != self->placeholder_slot.target ||
              self->placeholder_opacity < 1.0;
    offset = GRAPHENE_POINT_INIT ((float) round (self->placeholder_slot.x), 0.0f);
    gtk_widget_allocate (self->placeholder, tab_width, height, baseline,
                         gsk_transform_translate (NULL, &offset));
  }

  if (moving && self->tick == 0 && self->animated &&
      gtk_widget_get_mapped (widget))
    self->tick = gtk_widget_add_tick_callback (widget, awra_tab_strip_tick_cb,
                                               NULL, NULL);
}

static void
awra_tab_strip_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  AwraTabStrip *self = AWRA_TAB_STRIP (widget);

  if (self->placeholder != NULL)
    gtk_widget_snapshot_child (widget, self->placeholder, snapshot);
  for (guint i = 0; i < self->children->len; i++)
    gtk_widget_snapshot_child (widget,
                               g_ptr_array_index (self->children, i),
                               snapshot);
}

static void
awra_tab_strip_dispose (GObject *object)
{
  AwraTabStrip *self = AWRA_TAB_STRIP (object);

  if (self->tick != 0) {
    gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick);
    self->tick = 0;
  }
  g_clear_pointer (&self->placeholder, gtk_widget_unparent);
  while (self->children != NULL && self->children->len > 0) {
    GtkWidget *child = g_ptr_array_index (self->children,
                                         self->children->len - 1);

    gtk_widget_unparent (child);
    g_ptr_array_remove_index (self->children, self->children->len - 1);
  }
  g_clear_pointer (&self->children, g_ptr_array_unref);
  g_clear_pointer (&self->slots, g_hash_table_unref);
  G_OBJECT_CLASS (awra_tab_strip_parent_class)->dispose (object);
}

static void
awra_tab_strip_class_init (AwraTabStripClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_tab_strip_dispose;
  widget_class->measure = awra_tab_strip_measure;
  widget_class->size_allocate = awra_tab_strip_size_allocate;
  widget_class->snapshot = awra_tab_strip_snapshot;
  gtk_widget_class_set_css_name (widget_class, "awratabstrip");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TAB_LIST);
}

static void
awra_tab_strip_init (AwraTabStrip *self)
{
  self->children = g_ptr_array_new ();
  self->slots = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  self->placeholder_position = -1;
  self->animated = TRUE;
}

static GtkWidget *
awra_tab_strip_new (void)
{
  return g_object_new (AWRA_TYPE_TAB_STRIP, NULL);
}

static void
awra_tab_strip_append (AwraTabStrip *self,
                       GtkWidget    *child)
{
  g_ptr_array_add (self->children, child);
  g_hash_table_insert (self->slots, child, g_new0 (TabSlot, 1));
  /* Tabs come first: the placeholder is not part of the tab order. */
  gtk_widget_insert_before (child, GTK_WIDGET (self), self->placeholder);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
awra_tab_strip_remove (AwraTabStrip *self,
                       GtkWidget    *child)
{
  for (guint i = 0; i < self->children->len; i++) {
    if (g_ptr_array_index (self->children, i) == child) {
      if (self->dragged == child)
        self->dragged = NULL;
      g_hash_table_remove (self->slots, child);
      gtk_widget_unparent (child);
      g_ptr_array_remove_index (self->children, i);
      gtk_widget_queue_resize (GTK_WIDGET (self));
      return;
    }
  }
}

enum {
  PROP_0,
  PROP_SELECTED_NAME,
  PROP_SELECTED_PAGE,
  PROP_CONTENT_VISIBLE,
  PROP_STACK,
  PROP_SCROLL_SELECTS,
  N_PROPS,
};

enum {
  SIGNAL_CLOSE_PAGE,
  N_SIGNALS,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (AwraTabView, awra_tab_view, GTK_TYPE_WIDGET)

static AwraTabPage *
find_page (AwraTabView *self,
           GtkWidget   *page,
           guint       *position)
{
  for (guint i = 0; i < self->pages->len; i++) {
    AwraTabPage *info = g_ptr_array_index (self->pages, i);

    if (info->page == page) {
      if (position != NULL)
        *position = i;
      return info;
    }
  }

  return NULL;
}

static void
tab_page_free (gpointer data)
{
  AwraTabPage *info = data;

  g_clear_pointer (&info->name, g_free);
  g_free (info);
}

static void
update_tab_geometry (AwraTabPage *info)
{
  gboolean icon_only = info->pinned && !info->title_visible;
  GtkWidget *select = GTK_WIDGET (info->select_button);

  if (icon_only) {
    gtk_widget_add_css_class (info->tab, "awra-tab-icon-only");
    gtk_widget_set_size_request (select,
                                 TAB_PINNED_ICON_SIZE,
                                 TAB_PINNED_ICON_SIZE);
    gtk_widget_set_hexpand (select, FALSE);
    gtk_widget_set_vexpand (select, FALSE);
    gtk_widget_set_halign (select, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (select, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (GTK_WIDGET (info->body), FALSE);
    gtk_widget_set_hexpand (GTK_WIDGET (info->select_content), TRUE);
    gtk_widget_set_halign (GTK_WIDGET (info->select_content), GTK_ALIGN_FILL);
    if (info->icon != NULL) {
      gtk_image_set_pixel_size (info->icon, TAB_PINNED_ICON_IMAGE_SIZE);
      gtk_widget_set_hexpand (GTK_WIDGET (info->icon), TRUE);
    }
  } else {
    gtk_widget_remove_css_class (info->tab, "awra-tab-icon-only");
    gtk_widget_set_size_request (select, -1, -1);
    gtk_widget_set_hexpand (select, TRUE);
    gtk_widget_set_vexpand (select, FALSE);
    gtk_widget_set_halign (select, GTK_ALIGN_FILL);
    gtk_widget_set_valign (select, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand (GTK_WIDGET (info->body), TRUE);
    gtk_widget_set_hexpand (GTK_WIDGET (info->select_content), TRUE);
    gtk_widget_set_halign (GTK_WIDGET (info->select_content), GTK_ALIGN_FILL);
    if (info->icon != NULL) {
      gtk_image_set_pixel_size (info->icon, TAB_PAGE_ICON_SIZE);
      gtk_widget_set_hexpand (GTK_WIDGET (info->icon), FALSE);
    }
  }
}

static void
set_page_icon (AwraTabPage *info,
               GtkImage    *icon)
{
  if (info->icon == icon)
    return;

  g_return_if_fail (icon == NULL ||
                    gtk_widget_get_parent (GTK_WIDGET (icon)) == NULL);

  if (info->icon != NULL) {
    GtkWidget *previous = GTK_WIDGET (info->icon);

    info->icon = NULL;
    gtk_box_remove (info->select_content, previous);
  }

  info->icon = icon;
  if (icon != NULL) {
    gtk_widget_add_css_class (GTK_WIDGET (icon), "awra-tab-icon");
    gtk_widget_set_halign (GTK_WIDGET (icon), GTK_ALIGN_CENTER);
    gtk_widget_set_valign (GTK_WIDGET (icon), GTK_ALIGN_CENTER);
    gtk_widget_set_can_target (GTK_WIDGET (icon), FALSE);
    gtk_widget_set_focusable (GTK_WIDGET (icon), FALSE);
    gtk_box_prepend (info->select_content, GTK_WIDGET (icon));
  }

  update_tab_geometry (info);
}

static void
update_motion (AwraTabView *self)
{
  guint duration = 0;

  if (self->style_manager != NULL)
    duration = awra_motion_get_duration (self->style_manager,
                                         AWRA_MOTION_PRESET_NORMAL);
  AWRA_TAB_STRIP (self->tab_strip)->animated = duration > 0;
  gtk_stack_set_transition_duration (self->stack, duration);
  gtk_stack_set_transition_type (
    self->stack,
    duration == 0 ? GTK_STACK_TRANSITION_TYPE_NONE
                  : GTK_STACK_TRANSITION_TYPE_CROSSFADE);
}

static void
motion_changed_cb (AwraStyleManager *manager,
                   GParamSpec       *pspec,
                   AwraTabView      *self)
{
  (void) manager;
  (void) pspec;
  update_motion (self);
}

static void
update_tab_separators (AwraTabView *self)
{
  GtkWidget *selected = gtk_stack_get_visible_child (self->stack);

  for (guint i = 0; i < self->pages->len; i++) {
    AwraTabPage *info = g_ptr_array_index (self->pages, i);
    gboolean current_emphasized = info->hovered || info->page == selected;
    gboolean next_emphasized = FALSE;

    /* A pinned destination is a fixed landmark rather than another peer tab.
     * Keep its boundary stable while contextual separators between dynamic
     * tabs react to hover and selection. */
    if (info->pinned) {
      gtk_widget_remove_css_class (GTK_WIDGET (info->separator),
                                   "awra-tab-separator-hidden");
      continue;
    }

    if (i + 1 < self->pages->len) {
      AwraTabPage *next = g_ptr_array_index (self->pages, i + 1);

      next_emphasized = next->hovered || next->page == selected;
    }

    if (i + 1 == self->pages->len ||
        current_emphasized || next_emphasized)
      gtk_widget_add_css_class (GTK_WIDGET (info->separator),
                                "awra-tab-separator-hidden");
    else
      gtk_widget_remove_css_class (GTK_WIDGET (info->separator),
                                   "awra-tab-separator-hidden");
  }
}

static void
ensure_selected_visible (AwraTabView *self)
{
  GtkWidget *selected = gtk_stack_get_visible_child (self->stack);
  AwraTabPage *info;
  GtkAdjustment *adjustment;
  graphene_rect_t bounds;
  double value;
  double page_size;
  double target;
  double upper;

  if (selected == NULL)
    return;
  info = find_page (self, selected, NULL);
  if (info == NULL || info->pinned)
    return;
  if (!gtk_widget_compute_bounds (info->tab,
                                  GTK_WIDGET (self->tab_strip),
                                  &bounds))
    return;

  adjustment = gtk_scrolled_window_get_hadjustment (self->tab_scroller);
  value = gtk_adjustment_get_value (adjustment);
  page_size = gtk_adjustment_get_page_size (adjustment);
  upper = gtk_adjustment_get_upper (adjustment);
  target = value;

  if (bounds.origin.x < value)
    target = bounds.origin.x;
  else if (bounds.origin.x + bounds.size.width > value + page_size)
    target = bounds.origin.x + bounds.size.width - page_size;

  target = CLAMP (target,
                  gtk_adjustment_get_lower (adjustment),
                  MAX (gtk_adjustment_get_lower (adjustment),
                       upper - page_size));
  if (fabs (target - value) > 0.5)
    gtk_adjustment_set_value (adjustment, target);
}

static gboolean
ensure_selected_visible_tick_cb (GtkWidget     *widget,
                                 GdkFrameClock *frame_clock,
                                 gpointer       user_data)
{
  AwraTabView *self = AWRA_TAB_VIEW (widget);

  (void) frame_clock;
  (void) user_data;
  self->ensure_visible_tick = 0;
  ensure_selected_visible (self);
  return G_SOURCE_REMOVE;
}

static void
schedule_selected_visible (AwraTabView *self)
{
  if (self->ensure_visible_tick == 0 &&
      gtk_widget_get_mapped (GTK_WIDGET (self)))
    self->ensure_visible_tick = gtk_widget_add_tick_callback (
      GTK_WIDGET (self), ensure_selected_visible_tick_cb, NULL, NULL);
}

static void
cancel_selected_visible (AwraTabView *self)
{
  if (self->ensure_visible_tick == 0)
    return;
  gtk_widget_remove_tick_callback (GTK_WIDGET (self),
                                   self->ensure_visible_tick);
  self->ensure_visible_tick = 0;
}

static void
update_selected_tab (AwraTabView *self)
{
  GtkWidget *selected = gtk_stack_get_visible_child (self->stack);

  for (guint i = 0; i < self->pages->len; i++) {
    AwraTabPage *info = g_ptr_array_index (self->pages, i);

    gboolean is_selected = info->page == selected;
    gboolean show_close = is_selected || info->hovered;

    if (is_selected)
      gtk_widget_add_css_class (info->tab, "awra-tab-selected");
    else
      gtk_widget_remove_css_class (info->tab, "awra-tab-selected");
    gtk_toggle_button_set_active (info->select_button,
                                  is_selected);
    gtk_accessible_update_state (GTK_ACCESSIBLE (info->select_button),
                                 GTK_ACCESSIBLE_STATE_SELECTED, is_selected, -1);
    if (info->close_button != NULL) {
      if (show_close)
        gtk_widget_add_css_class (info->close_button,
                                  "awra-tab-close-visible");
      else
        gtk_widget_remove_css_class (info->close_button,
                                     "awra-tab-close-visible");
      gtk_widget_set_can_target (info->close_button, show_close);
      gtk_widget_set_focusable (info->close_button, show_close);
    }
  }
  update_tab_separators (self);
}

static void
tab_enter_cb (GtkEventControllerMotion *controller,
              double                    x,
              double                    y,
              AwraTabPage              *info)
{
  (void) controller;
  (void) x;
  (void) y;
  info->hovered = TRUE;
  update_selected_tab (info->view);
}

static void
tab_leave_cb (GtkEventControllerMotion *controller,
              AwraTabPage              *info)
{
  (void) controller;
  info->hovered = FALSE;
  update_selected_tab (info->view);
}

static void
selected_changed_cb (GtkStack    *stack,
                     GParamSpec  *pspec,
                     AwraTabView *self)
{
  (void) stack;
  (void) pspec;
  update_selected_tab (self);
  schedule_selected_visible (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED_NAME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED_PAGE]);
}

static void
select_page (AwraTabPage *info)
{
  if (find_page (info->view, info->page, NULL) != NULL)
    awra_tab_view_set_selected_page (info->view, info->page);
}

static void
select_clicked_cb (GtkButton   *button,
                   AwraTabPage *info)
{
  (void) button;
  select_page (info);
}

static void
close_clicked_cb (GtkButton   *button,
                  AwraTabPage *info)
{
  AwraTabView *self = info->view;
  GtkWidget *page = info->page;

  (void) button;
  awra_tab_view_close_page (self, page);
}

static void
tab_middle_released_cb (GtkGestureClick *gesture,
                        int              n_press,
                        double           x,
                        double           y,
                        AwraTabPage     *info)
{
  (void) n_press;
  if (info->close_button == NULL ||
      !gtk_widget_contains (info->tab, x, y))
    return;
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  awra_tab_view_close_page (info->view, info->page);
}

static AwraTabPage *
append_page (AwraTabView *self,
             GtkWidget   *page,
             const char  *name,
             const char  *title,
             gboolean     pinned,
             gboolean     closable)
{
  AwraTabPage *info;
  GtkEventController *motion;
  GtkGesture *middle_click;

  g_return_val_if_fail (find_page (self, page, NULL) == NULL, NULL);

  info = g_new0 (AwraTabPage, 1);
  info->view = self;
  info->page = page;
  info->name = g_strdup (name);
  info->pinned = pinned;
  info->title_visible = TRUE;
  info->tab = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  info->body = GTK_OVERLAY (gtk_overlay_new ());
  /* Exposed as a tab of a tab list, controlling its page. */
  info->select_button = GTK_TOGGLE_BUTTON (
    g_object_new (GTK_TYPE_TOGGLE_BUTTON, "accessible-role", GTK_ACCESSIBLE_ROLE_TAB, NULL));
  gtk_accessible_update_relation (GTK_ACCESSIBLE (info->select_button),
                                  GTK_ACCESSIBLE_RELATION_CONTROLS, page, NULL,
                                  -1);
  info->select_content = GTK_BOX (
    gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6));
  info->title = AWRA_TAB_TITLE (awra_tab_title_new (title));
  info->label = info->title->label;
  info->separator = GTK_SEPARATOR (
    gtk_separator_new (GTK_ORIENTATION_VERTICAL));

  gtk_widget_add_css_class (GTK_WIDGET (info->label), "awra-tab-label");
  gtk_widget_add_css_class (GTK_WIDGET (info->title), "awra-tab-title");
  gtk_widget_add_css_class (info->tab, "awra-tab-item");
  gtk_widget_add_css_class (GTK_WIDGET (info->body), "awra-tab-pill");
  gtk_widget_set_hexpand (info->tab, FALSE);
  gtk_widget_set_hexpand (GTK_WIDGET (info->body), TRUE);
  gtk_widget_set_valign (GTK_WIDGET (info->body), GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (GTK_WIDGET (info->select_button),
                            "awra-tab-button");
  gtk_widget_set_hexpand (GTK_WIDGET (info->select_button), TRUE);
  gtk_accessible_update_property (GTK_ACCESSIBLE (info->select_button),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  title,
                                  -1);
  if (pinned) {
    gtk_widget_add_css_class (info->tab, "awra-tab-pinned");
  }
  update_tab_geometry (info);
  motion = gtk_event_controller_motion_new ();
  g_signal_connect (motion, "enter", G_CALLBACK (tab_enter_cb), info);
  g_signal_connect (motion, "leave", G_CALLBACK (tab_leave_cb), info);
  gtk_widget_add_controller (info->tab, motion);
  middle_click = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (middle_click), GDK_BUTTON_MIDDLE);
  g_signal_connect (middle_click, "released",
                    G_CALLBACK (tab_middle_released_cb), info);
  gtk_widget_add_controller (info->tab, GTK_EVENT_CONTROLLER (middle_click));
  gtk_widget_set_halign (GTK_WIDGET (info->select_content), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (info->select_content), GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (GTK_WIDGET (info->title), TRUE);
  gtk_widget_set_halign (GTK_WIDGET (info->title), GTK_ALIGN_FILL);
  gtk_box_append (info->select_content, GTK_WIDGET (info->title));
  gtk_button_set_child (GTK_BUTTON (info->select_button),
                        GTK_WIDGET (info->select_content));
  g_signal_connect (info->select_button, "clicked",
                    G_CALLBACK (select_clicked_cb), info);
  gtk_overlay_set_child (info->body, GTK_WIDGET (info->select_button));

  if (closable) {
    info->close_button = gtk_button_new_from_icon_name (
      "awra-close-symbolic");
    gtk_widget_set_tooltip_text (info->close_button, _("Close tab"));
    gtk_widget_add_css_class (info->close_button, "awra-tab-close");
    gtk_widget_set_size_request (info->close_button,
                                 TAB_CLOSE_SIZE,
                                 TAB_CLOSE_SIZE);
    gtk_widget_set_hexpand (info->close_button, FALSE);
    gtk_widget_set_vexpand (info->close_button, FALSE);
    gtk_widget_set_halign (info->close_button, GTK_ALIGN_END);
    gtk_widget_set_valign (info->close_button, GTK_ALIGN_CENTER);
    gtk_widget_set_can_target (info->close_button, FALSE);
    gtk_widget_set_focusable (info->close_button, FALSE);
    g_signal_connect (info->close_button, "clicked",
                      G_CALLBACK (close_clicked_cb), info);
    gtk_overlay_add_overlay (info->body, info->close_button);
  }
  gtk_box_append (GTK_BOX (info->tab), GTK_WIDGET (info->body));
  gtk_widget_add_css_class (GTK_WIDGET (info->separator),
                            "awra-tab-separator");
  gtk_box_append (GTK_BOX (info->tab), GTK_WIDGET (info->separator));

  g_ptr_array_add (self->pages, info);
  if (pinned)
    gtk_box_append (self->pinned_tabs, info->tab);
  else
    awra_tab_strip_append (AWRA_TAB_STRIP (self->tab_strip), info->tab);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  gtk_stack_add_named (self->stack, page, name);
  if (self->pages->len == 1)
    gtk_stack_set_visible_child (self->stack, page);
  update_selected_tab (self);

  return info;
}

static void
select_adjacent_page (AwraTabView *self,
                      int          step)
{
  GtkWidget *selected = gtk_stack_get_visible_child (self->stack);

  for (guint i = 0; i < self->pages->len; i++) {
    AwraTabPage *info = g_ptr_array_index (self->pages, i);

    if (info->page != selected)
      continue;
    /* The wheel stops at the ends: wrapping around hides where the user is. */
    if ((step > 0 && i + 1 < self->pages->len) || (step < 0 && i > 0))
      awra_tab_view_set_selected_page (
        self, ((AwraTabPage *) g_ptr_array_index (self->pages, i + step))->page);
    return;
  }
}

static gboolean
tab_bar_scroll_cb (GtkEventControllerScroll *controller,
                   double                    dx,
                   double                    dy,
                   AwraTabView              *self)
{
  if (!self->scroll_selects)
    return GDK_EVENT_PROPAGATE;
  /* High-resolution wheels and touchpads report fractions of a step. */
  self->scroll_steps += (fabs (dx) > fabs (dy) ? dx : dy) /
    (gtk_event_controller_scroll_get_unit (controller) == GDK_SCROLL_UNIT_WHEEL
       ? 1.0 : 48.0);
  if (fabs (self->scroll_steps) >= 1.0) {
    select_adjacent_page (self, self->scroll_steps > 0 ? 1 : -1);
    self->scroll_steps = 0.0;
  }
  return GDK_EVENT_STOP;
}

static gboolean
tab_scroll_cb (GtkEventControllerScroll *controller,
               double                    dx,
               double                    dy,
               AwraTabView              *self)
{
  GtkAdjustment *adjustment;
  GdkScrollUnit unit;
  double delta;
  double value;
  double upper;
  double page_size;

  if (fabs (dx) >= 0.01 || fabs (dy) >= 0.01)
    cancel_selected_visible (self);
  if (fabs (dx) > fabs (dy) || fabs (dy) < 0.01)
    return GDK_EVENT_PROPAGATE;

  adjustment = gtk_scrolled_window_get_hadjustment (self->tab_scroller);
  upper = gtk_adjustment_get_upper (adjustment);
  page_size = gtk_adjustment_get_page_size (adjustment);
  if (upper <= page_size + 0.5)
    return GDK_EVENT_PROPAGATE;

  unit = gtk_event_controller_scroll_get_unit (controller);
  delta = dy * (unit == GDK_SCROLL_UNIT_WHEEL ? 84.0 : 1.0);
  value = CLAMP (gtk_adjustment_get_value (adjustment) + delta,
                 gtk_adjustment_get_lower (adjustment),
                 MAX (gtk_adjustment_get_lower (adjustment),
                      upper - page_size));
  gtk_adjustment_set_value (adjustment, value);
  return GDK_EVENT_STOP;
}

static void
awra_tab_view_map (GtkWidget *widget)
{
  AwraTabView *self = AWRA_TAB_VIEW (widget);
  AwraContext *context;

  GTK_WIDGET_CLASS (awra_tab_view_parent_class)->map (widget);
  context = awra_context_get_for_display (gtk_widget_get_display (widget));
  self->style_manager = awra_context_get_style_manager (context);
  self->style_handler = g_signal_connect (self->style_manager,
                                          "notify::reduced-motion",
                                          G_CALLBACK (motion_changed_cb),
                                          self);
  update_motion (self);
  schedule_selected_visible (self);
}

static void
awra_tab_view_unmap (GtkWidget *widget)
{
  AwraTabView *self = AWRA_TAB_VIEW (widget);

  cancel_selected_visible (self);
  if (self->style_handler != 0) {
    g_signal_handler_disconnect (self->style_manager, self->style_handler);
    self->style_handler = 0;
    self->style_manager = NULL;
  }
  GTK_WIDGET_CLASS (awra_tab_view_parent_class)->unmap (widget);
}

static void
awra_tab_view_dispose (GObject *object)
{
  AwraTabView *self = AWRA_TAB_VIEW (object);

  if (self->style_handler != 0) {
    g_signal_handler_disconnect (self->style_manager, self->style_handler);
    self->style_handler = 0;
  }
  cancel_selected_visible (self);
  if (self->box != NULL) {
    gtk_widget_unparent (GTK_WIDGET (self->box));
    self->box = NULL;
    self->tab_bar = NULL;
    self->pinned_tabs = NULL;
    self->tab_viewport_mask = NULL;
    self->tab_scroller = NULL;
    self->tab_strip = NULL;
    self->stack = NULL;
  }
  g_clear_pointer (&self->pages, g_ptr_array_unref);
  G_OBJECT_CLASS (awra_tab_view_parent_class)->dispose (object);
}

static void
awra_tab_view_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  AwraTabView *self = AWRA_TAB_VIEW (widget);

  if (self->box != NULL)
    gtk_widget_snapshot_child (widget, GTK_WIDGET (self->box), snapshot);
}

static void
awra_tab_view_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  AwraTabView *self = AWRA_TAB_VIEW (object);

  if (property_id == PROP_SELECTED_NAME)
    g_value_set_string (value, awra_tab_view_get_selected_name (self));
  else if (property_id == PROP_SELECTED_PAGE)
    g_value_set_object (value, awra_tab_view_get_selected_page (self));
  else if (property_id == PROP_CONTENT_VISIBLE)
    g_value_set_boolean (value, awra_tab_view_get_content_visible (self));
  else if (property_id == PROP_SCROLL_SELECTS)
    g_value_set_boolean (value, awra_tab_view_get_scroll_selects (AWRA_TAB_VIEW (object)));
  else if (property_id == PROP_STACK)
    g_value_set_object (value, self->stack);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
awra_tab_view_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  if (property_id == PROP_SELECTED_NAME)
    awra_tab_view_set_selected_name (AWRA_TAB_VIEW (object),
                                     g_value_get_string (value));
  else if (property_id == PROP_SCROLL_SELECTS)
    awra_tab_view_set_scroll_selects (AWRA_TAB_VIEW (object), g_value_get_boolean (value));
  else if (property_id == PROP_CONTENT_VISIBLE)
    awra_tab_view_set_content_visible (AWRA_TAB_VIEW (object),
                                       g_value_get_boolean (value));
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

/* Ctrl+Tab and Ctrl+Page Down select the next tab, Ctrl+Shift+Tab and
 * Ctrl+Page Up the previous one, around the ends. */
static gboolean
select_relative_cb (GtkWidget *widget,
                    GVariant  *args,
                    gpointer   user_data)
{
  AwraTabView *self = AWRA_TAB_VIEW (widget);
  GtkWidget *selected = gtk_stack_get_visible_child (self->stack);
  int step = g_variant_get_int32 (args);
  guint count = self->pages->len;

  (void) user_data;
  if (count < 2)
    return FALSE;
  for (guint i = 0; i < count; i++) {
    AwraTabPage *info = g_ptr_array_index (self->pages, i);

    if (info->page == selected) {
      AwraTabPage *next = g_ptr_array_index (self->pages, (i + count + step) % count);

      awra_tab_view_set_selected_page (self, next->page);
      gtk_widget_grab_focus (GTK_WIDGET (next->select_button));
      return TRUE;
    }
  }
  return FALSE;
}

static void
awra_tab_view_class_init (AwraTabViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_layout_manager_type (widget_class,
                                            GTK_TYPE_BIN_LAYOUT);
  object_class->dispose = awra_tab_view_dispose;
  object_class->get_property = awra_tab_view_get_property;
  object_class->set_property = awra_tab_view_set_property;
  widget_class->map = awra_tab_view_map;
  widget_class->unmap = awra_tab_view_unmap;
  widget_class->snapshot = awra_tab_view_snapshot;
  properties[PROP_SELECTED_NAME] =
    g_param_spec_string ("selected-name", NULL, NULL, NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);
  properties[PROP_SELECTED_PAGE] =
    g_param_spec_object ("selected-page", NULL, NULL, GTK_TYPE_WIDGET,
                         G_PARAM_READABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);
  properties[PROP_CONTENT_VISIBLE] =
    g_param_spec_boolean ("content-visible", NULL, NULL, TRUE,
                          G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS);
  properties[PROP_STACK] =
    g_param_spec_object ("stack", NULL, NULL, GTK_TYPE_STACK,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_SCROLL_SELECTS] =
    g_param_spec_boolean ("scroll-selects", NULL, NULL, FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
  signals[SIGNAL_CLOSE_PAGE] =
    g_signal_new ("close-page",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_true_handled,
                  NULL,
                  NULL,
                  G_TYPE_BOOLEAN,
                  1,
                  GTK_TYPE_WIDGET);
  gtk_widget_class_set_css_name (widget_class, "awratabview");
  gtk_widget_class_add_binding (widget_class, GDK_KEY_Tab, GDK_CONTROL_MASK,
                                select_relative_cb, "i", 1);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_ISO_Left_Tab,
                                GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                select_relative_cb, "i", -1);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_Tab,
                                GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                select_relative_cb, "i", -1);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_Page_Down, GDK_CONTROL_MASK,
                                select_relative_cb, "i", 1);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_Page_Up, GDK_CONTROL_MASK,
                                select_relative_cb, "i", -1);
}

static void
awra_tab_view_init (AwraTabView *self)
{
  GtkEventController *scroll;

  self->pages = g_ptr_array_new_with_free_func (tab_page_free);
  self->box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  self->tab_bar = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  self->pinned_tabs = GTK_BOX (g_object_new (GTK_TYPE_BOX, "orientation",
                                             GTK_ORIENTATION_HORIZONTAL,
                                             "accessible-role",
                                             GTK_ACCESSIBLE_ROLE_TAB_LIST, NULL));
  self->tab_scroller = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new ());
  self->tab_viewport_mask = awra_tab_viewport_mask_new (self->tab_scroller);
  self->tab_strip = awra_tab_strip_new ();
  self->stack = GTK_STACK (gtk_stack_new ());
  gtk_widget_add_css_class (GTK_WIDGET (self->tab_bar), "awra-tab-bar");
  gtk_widget_add_css_class (GTK_WIDGET (self->pinned_tabs),
                            "awra-pinned-tabs");
  gtk_widget_add_css_class (GTK_WIDGET (self->tab_scroller),
                            "awra-tab-scroller");
  gtk_widget_add_css_class (self->tab_strip,
                            "awra-tab-strip");
  gtk_widget_set_halign (GTK_WIDGET (self->tab_bar), GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (GTK_WIDGET (self->tab_bar), TRUE);
  gtk_widget_set_hexpand (self->tab_viewport_mask, TRUE);
  gtk_widget_set_hexpand (GTK_WIDGET (self->tab_scroller), TRUE);
  gtk_widget_set_hexpand (self->tab_strip, TRUE);
  gtk_scrolled_window_set_policy (self->tab_scroller,
                                  GTK_POLICY_EXTERNAL,
                                  GTK_POLICY_NEVER);
  gtk_scrolled_window_set_propagate_natural_width (self->tab_scroller,
                                                    FALSE);
  gtk_scrolled_window_set_min_content_width (self->tab_scroller, 0);
  gtk_scrolled_window_set_has_frame (self->tab_scroller, FALSE);
  gtk_scrolled_window_set_child (self->tab_scroller, self->tab_strip);
  scroll = gtk_event_controller_scroll_new (
    GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES |
    GTK_EVENT_CONTROLLER_SCROLL_KINETIC);
  g_signal_connect (scroll, "scroll", G_CALLBACK (tab_scroll_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self->tab_scroller), scroll);
  scroll = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
  gtk_event_controller_set_propagation_phase (scroll, GTK_PHASE_CAPTURE);
  g_signal_connect (scroll, "scroll", G_CALLBACK (tab_bar_scroll_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self->tab_bar), scroll);
  gtk_box_append (self->tab_bar, GTK_WIDGET (self->pinned_tabs));
  gtk_box_append (self->tab_bar, self->tab_viewport_mask);
  gtk_widget_set_hexpand (GTK_WIDGET (self->stack), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self->stack), TRUE);
  gtk_box_append (self->box, GTK_WIDGET (self->tab_bar));
  gtk_box_append (self->box, GTK_WIDGET (self->stack));
  gtk_widget_set_parent (GTK_WIDGET (self->box), GTK_WIDGET (self));
  g_signal_connect (self->stack,
                    "notify::visible-child-name",
                    G_CALLBACK (selected_changed_cb),
                    self);
}

GtkWidget *
awra_tab_view_new (void)
{
  return g_object_new (AWRA_TYPE_TAB_VIEW, NULL);
}

void
awra_tab_view_add (AwraTabView *self,
                   GtkWidget   *page,
                   const char  *name,
                   const char  *title)
{
  g_return_if_fail (AWRA_IS_TAB_VIEW (self));
  g_return_if_fail (GTK_IS_WIDGET (page));
  g_return_if_fail (name != NULL && *name != '\0');

  append_page (self, page, name, title != NULL ? title : name, FALSE, FALSE);
}

GtkWidget *
awra_tab_view_append (AwraTabView *self,
                      GtkWidget   *page,
                      const char  *title,
                      gboolean     pinned)
{
  g_autofree char *name = NULL;

  g_return_val_if_fail (AWRA_IS_TAB_VIEW (self), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (page), NULL);

  name = g_strdup_printf ("awra-page-%u", self->next_page_id++);
  if (append_page (self, page, name,
                   title != NULL ? title : _("Untitled"),
                   pinned, !pinned) == NULL)
    return NULL;
  return page;
}

void
awra_tab_view_remove (AwraTabView *self,
                      GtkWidget   *page)
{
  guint position;
  AwraTabPage *info;
  GtkWidget *selected;
  GtkWidget *replacement = NULL;

  g_return_if_fail (AWRA_IS_TAB_VIEW (self));
  g_return_if_fail (GTK_IS_WIDGET (page));

  info = find_page (self, page, &position);
  if (info == NULL)
    return;

  selected = gtk_stack_get_visible_child (self->stack);
  if (selected == page && self->pages->len > 1) {
    guint replacement_position = position > 0 ? position - 1 : 1;
    AwraTabPage *replacement_info = g_ptr_array_index (self->pages,
                                                       replacement_position);
    replacement = replacement_info->page;
  }

  if (info->pinned)
    gtk_box_remove (self->pinned_tabs, info->tab);
  else
    awra_tab_strip_remove (AWRA_TAB_STRIP (self->tab_strip), info->tab);
  gtk_stack_remove (self->stack, page);
  g_ptr_array_remove_index (self->pages, position);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  update_tab_separators (self);

  if (replacement != NULL)
    gtk_stack_set_visible_child (self->stack, replacement);
  update_selected_tab (self);
}

GtkWidget *
awra_tab_view_get_page_tab (AwraTabView *self, GtkWidget *page)
{
  g_return_val_if_fail (AWRA_IS_TAB_VIEW (self), NULL);
  AwraTabPage *info = find_page (self, page, NULL);
  return info != NULL ? info->tab : NULL;
}

void
awra_tab_view_reorder_page (AwraTabView *self, GtkWidget *page, guint position)
{
  guint old;
  g_return_if_fail (AWRA_IS_TAB_VIEW (self));
  AwraTabPage *info = find_page (self, page, &old);
  if (info == NULL || info->pinned) return;
  AwraTabStrip *strip = AWRA_TAB_STRIP (self->tab_strip);
  position = MIN (position, strip->children->len - 1);
  for (guint i = 0; i < strip->children->len; i++) {
    if (g_ptr_array_index (strip->children, i) == info->tab) {
      g_ptr_array_steal_index (strip->children, i);
      break;
    }
  }
  g_ptr_array_insert (strip->children, position, info->tab);
  g_ptr_array_steal_index (self->pages, old);
  guint index = 0, count = 0;
  while (index < self->pages->len) {
    AwraTabPage *other = g_ptr_array_index (self->pages, index);
    if (!other->pinned && count++ == position) break;
    index++;
  }
  g_ptr_array_insert (self->pages, index, info);
  GtkWidget *next = position + 1 < strip->children->len ? g_ptr_array_index (strip->children, position + 1) : strip->placeholder;
  gtk_widget_insert_before (info->tab, GTK_WIDGET (strip), next);
  gtk_widget_queue_resize (GTK_WIDGET (strip));
  update_tab_separators (self);
  update_selected_tab (self);
}

gboolean
awra_tab_view_get_scroll_selects (AwraTabView *self)
{
  g_return_val_if_fail (AWRA_IS_TAB_VIEW (self), FALSE);
  return self->scroll_selects;
}

void
awra_tab_view_set_scroll_selects (AwraTabView *self,
                                  gboolean     selects)
{
  g_return_if_fail (AWRA_IS_TAB_VIEW (self));
  selects = !!selects;
  if (self->scroll_selects == selects)
    return;
  self->scroll_selects = selects;
  self->scroll_steps = 0.0;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SCROLL_SELECTS]);
}

void
awra_tab_view_set_page_dragged (AwraTabView *self,
                                GtkWidget   *page,
                                gboolean     dragged)
{
  AwraTabPage *info;
  AwraTabStrip *strip;

  g_return_if_fail (AWRA_IS_TAB_VIEW (self));
  g_return_if_fail (GTK_IS_WIDGET (page));
  info = find_page (self, page, NULL);
  g_return_if_fail (info != NULL);
  if (info->pinned)
    return;
  strip = AWRA_TAB_STRIP (self->tab_strip);
  if (dragged) {
    if (strip->dragged != NULL)
      gtk_widget_set_opacity (strip->dragged, 1.0);
    strip->dragged = info->tab;
    gtk_widget_set_opacity (info->tab, 0.0);
  } else if (strip->dragged == info->tab) {
    TabSlot *slot = g_hash_table_lookup (strip->slots, info->tab);

    strip->dragged = NULL;
    /* It lands where the placeholder was, without crossing the bar. */
    slot->placed = FALSE;
    gtk_widget_set_opacity (info->tab, 1.0);
  }
  gtk_widget_queue_resize (GTK_WIDGET (strip));
}

guint
awra_tab_view_get_insertion_position (AwraTabView *self,
                                      double       x)
{
  AwraTabStrip *strip;
  graphene_point_t point = GRAPHENE_POINT_INIT ((float) x, 0.0f);
  graphene_point_t mapped;
  guint count;

  g_return_val_if_fail (AWRA_IS_TAB_VIEW (self), 0);
  strip = AWRA_TAB_STRIP (self->tab_strip);
  count = strip->children->len - (strip->dragged != NULL ? 1 : 0);
  if (strip->tab_width <= 0 ||
      !gtk_widget_compute_point (GTK_WIDGET (self->tab_bar), GTK_WIDGET (strip),
                                 &point, &mapped))
    return count;
  return (guint) CLAMP ((mapped.x + strip->tab_width / 2.0) / strip->tab_width,
                        0.0, (double) count);
}

void
awra_tab_view_show_placeholder (AwraTabView *self,
                                guint        position)
{
  AwraTabStrip *strip;

  g_return_if_fail (AWRA_IS_TAB_VIEW (self));
  strip = AWRA_TAB_STRIP (self->tab_strip);
  if (strip->placeholder_position == (int) position)
    return;
  if (strip->placeholder_position < 0) {
    strip->placeholder_slot.placed = FALSE;
    strip->placeholder_opacity = strip->animated ? 0.0 : 1.0;
    /* It only exists during a drag: at rest, the strip holds nothing but tabs. */
    strip->placeholder = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class (strip->placeholder, "awra-tab-placeholder");
    gtk_widget_set_can_target (strip->placeholder, FALSE);
    gtk_widget_set_opacity (strip->placeholder, strip->placeholder_opacity);
    gtk_widget_set_parent (strip->placeholder, GTK_WIDGET (strip));
  }
  strip->placeholder_position = (int) position;
  gtk_widget_queue_resize (GTK_WIDGET (strip));
}

void
awra_tab_view_hide_placeholder (AwraTabView *self)
{
  AwraTabStrip *strip;

  g_return_if_fail (AWRA_IS_TAB_VIEW (self));
  strip = AWRA_TAB_STRIP (self->tab_strip);
  if (strip->placeholder_position < 0)
    return;
  strip->placeholder_position = -1;
  g_clear_pointer (&strip->placeholder, gtk_widget_unparent);
  gtk_widget_queue_resize (GTK_WIDGET (strip));
}

void
awra_tab_view_close_page (AwraTabView *self,
                          GtkWidget   *page)
{
  gboolean inhibit = FALSE;

  g_return_if_fail (AWRA_IS_TAB_VIEW (self));
  g_return_if_fail (GTK_IS_WIDGET (page));
  g_return_if_fail (find_page (self, page, NULL) != NULL);

  g_signal_emit (self, signals[SIGNAL_CLOSE_PAGE], 0, page, &inhibit);
  if (!inhibit)
    awra_tab_view_remove (self, page);
}

GtkWidget *
awra_tab_view_get_selected_page (AwraTabView *self)
{
  g_return_val_if_fail (AWRA_IS_TAB_VIEW (self), NULL);
  return gtk_stack_get_visible_child (self->stack);
}

void
awra_tab_view_set_selected_page (AwraTabView *self,
                                 GtkWidget   *page)
{
  g_return_if_fail (AWRA_IS_TAB_VIEW (self));
  g_return_if_fail (GTK_IS_WIDGET (page));
  g_return_if_fail (find_page (self, page, NULL) != NULL);

  gtk_stack_set_visible_child (self->stack, page);
}

void
awra_tab_view_set_page_title (AwraTabView *self,
                              GtkWidget   *page,
                              const char  *title)
{
  AwraTabPage *info;

  g_return_if_fail (AWRA_IS_TAB_VIEW (self));
  g_return_if_fail (GTK_IS_WIDGET (page));

  info = find_page (self, page, NULL);
  g_return_if_fail (info != NULL);
  gtk_label_set_text (info->label, title != NULL ? title : _("Untitled"));
  gtk_accessible_update_property (GTK_ACCESSIBLE (info->select_button),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  title != NULL ? title : _("Untitled"),
                                  -1);
}

void
awra_tab_view_set_page_icon_name (AwraTabView *self,
                                  GtkWidget   *page,
                                  const char  *icon_name)
{
  AwraTabPage *info;
  GtkImage *icon = NULL;

  g_return_if_fail (AWRA_IS_TAB_VIEW (self));
  g_return_if_fail (GTK_IS_WIDGET (page));

  info = find_page (self, page, NULL);
  g_return_if_fail (info != NULL);
  if (icon_name != NULL && *icon_name != '\0')
    icon = GTK_IMAGE (gtk_image_new_from_icon_name (icon_name));
  set_page_icon (info, icon);
}

void
awra_tab_view_set_page_icon (AwraTabView *self,
                             GtkWidget   *page,
                             GtkImage    *icon)
{
  AwraTabPage *info;

  g_return_if_fail (AWRA_IS_TAB_VIEW (self));
  g_return_if_fail (GTK_IS_WIDGET (page));
  g_return_if_fail (icon == NULL || GTK_IS_IMAGE (icon));

  info = find_page (self, page, NULL);
  g_return_if_fail (info != NULL);
  set_page_icon (info, icon);
}

void
awra_tab_view_set_page_title_visible (AwraTabView *self,
                                      GtkWidget   *page,
                                      gboolean     visible)
{
  AwraTabPage *info;

  g_return_if_fail (AWRA_IS_TAB_VIEW (self));
  g_return_if_fail (GTK_IS_WIDGET (page));

  info = find_page (self, page, NULL);
  g_return_if_fail (info != NULL);
  info->title_visible = !!visible;
  gtk_widget_set_visible (GTK_WIDGET (info->title), info->title_visible);
  update_tab_geometry (info);
}

void
awra_tab_view_set_page_tooltip (AwraTabView *self,
                                GtkWidget   *page,
                                const char  *tooltip)
{
  AwraTabPage *info;

  g_return_if_fail (AWRA_IS_TAB_VIEW (self));
  g_return_if_fail (GTK_IS_WIDGET (page));

  info = find_page (self, page, NULL);
  g_return_if_fail (info != NULL);
  gtk_widget_set_tooltip_text (info->tab, tooltip);
}

GtkWidget *
awra_tab_view_get_tab_bar (AwraTabView *self)
{
  g_return_val_if_fail (AWRA_IS_TAB_VIEW (self), NULL);
  return GTK_WIDGET (self->tab_bar);
}

gboolean
awra_tab_view_get_content_visible (AwraTabView *self)
{
  g_return_val_if_fail (AWRA_IS_TAB_VIEW (self), FALSE);
  return gtk_widget_get_visible (GTK_WIDGET (self->stack));
}

void
awra_tab_view_set_content_visible (AwraTabView *self,
                                   gboolean     visible)
{
  g_return_if_fail (AWRA_IS_TAB_VIEW (self));

  visible = !!visible;
  if (visible == awra_tab_view_get_content_visible (self))
    return;
  gtk_widget_set_visible (GTK_WIDGET (self->stack), visible);
  g_object_notify_by_pspec (G_OBJECT (self),
                            properties[PROP_CONTENT_VISIBLE]);
}

const char *
awra_tab_view_get_selected_name (AwraTabView *self)
{
  g_return_val_if_fail (AWRA_IS_TAB_VIEW (self), NULL);
  return gtk_stack_get_visible_child_name (self->stack);
}

void
awra_tab_view_set_selected_name (AwraTabView *self,
                                 const char  *name)
{
  g_return_if_fail (AWRA_IS_TAB_VIEW (self));
  g_return_if_fail (name != NULL);
  g_return_if_fail (gtk_stack_get_child_by_name (self->stack, name) != NULL);

  gtk_stack_set_visible_child_name (self->stack, name);
}

GtkStack *
awra_tab_view_get_stack (AwraTabView *self)
{
  g_return_val_if_fail (AWRA_IS_TAB_VIEW (self), NULL);
  return self->stack;
}
