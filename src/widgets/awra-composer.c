/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <math.h>

#include <awra/awra-composer.h>

#include <awra/awra-button.h>
#include <awra/awra-context.h>
#include <awra/awra-motion.h>
#include <awra/awra-token-set.h>

#include "awra-i18n-private.h"
#include "overlays/awra-completion-popover-private.h"

/* Calibration, not ABI: space between the parts of the well. */
#define GAP 6
/* Text typed within this window lets the well follow its new height with
 * a glide; any other change of height (a resize) applies at once. */
#define TEXT_CHANGE_WINDOW_US 200000
#define TEXT_MARGIN_Y 7
#define TEXT_MARGIN_X 6

typedef struct {
  gboolean shown;
  double progress;
  double from;
  gint64 started;
} Reveal;

struct _AwraComposer {
  GtkWidget parent_instance;

  GtkWidget *banner;
  GtkWidget *banner_leaving;
  GtkWidget *attachments;
  GtkWidget *attachments_leaving;
  GtkWidget *start_widget;
  GtkWidget *end_widget;
  GtkWidget *field;
  GtkScrolledWindow *scroller;
  GtkTextView *view;
  GtkLabel *placeholder;
  GtkWidget *send;

  char *text_cache;
  guint max_lines;
  int line_height;
  gboolean can_send;
  gboolean send_on_enter;
  gboolean busy;
  gboolean preedit;
  gboolean has_text;

  Reveal banner_reveal;
  Reveal attachments_reveal;
  Reveal send_reveal;

  double row_displayed;
  double row_from;
  double row_to;
  gint64 row_started;
  gboolean row_animating;
  gint64 text_changed_until;

  double busy_angle;
  gint64 busy_last;
  guint tick;
};

enum {
  PROP_0,
  PROP_TEXT,
  PROP_PLACEHOLDER_TEXT,
  PROP_MAX_LINES,
  PROP_CAN_SEND,
  PROP_SEND_ON_ENTER,
  PROP_BUSY,
  PROP_BANNER,
  PROP_ATTACHMENTS,
  PROP_START_WIDGET,
  PROP_END_WIDGET,
  N_PROPS,
};

enum {
  SIGNAL_SUBMIT,
  SIGNAL_BANNER_CLOSED,
  N_SIGNALS,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (AwraComposer, awra_composer, GTK_TYPE_WIDGET)

static guint
motion_duration (AwraComposer     *self,
                 AwraMotionPreset  preset)
{
  AwraContext *context = awra_context_get_for_display (
    gtk_widget_get_display (GTK_WIDGET (self)));

  return awra_motion_get_duration (awra_context_get_style_manager (context), preset);
}

static double
eased (const Reveal *reveal)
{
  return awra_motion_ease_out_cubic (reveal->progress);
}

static GtkWidget *
shown_banner (AwraComposer *self)
{
  return self->banner != NULL ? self->banner : self->banner_leaving;
}

static GtkWidget *
shown_attachments (AwraComposer *self)
{
  return self->attachments != NULL ? self->attachments : self->attachments_leaving;
}

/* Motion: one tick drives every reveal, the height of the text and the busy
 * indicator, and stops when none of them moves. */

static gboolean
reveal_step (Reveal *reveal,
             gint64  now,
             guint   duration)
{
  double target = reveal->shown ? 1.0 : 0.0;
  double t;

  if (reveal->progress == target)
    return FALSE;
  if (reveal->started == 0)
    reveal->started = now;
  t = duration == 0 ? 1.0 : (double) (now - reveal->started) / (duration * 1000.0);
  if (t >= 1.0) {
    reveal->progress = target;
    return TRUE;
  }
  reveal->progress = reveal->from + (target - reveal->from) * t;
  return TRUE;
}

static void
drop_leaving (AwraComposer *self)
{
  if (self->banner_reveal.progress == 0.0 && self->banner_leaving != NULL)
    g_clear_pointer (&self->banner_leaving, gtk_widget_unparent);
  if (self->attachments_reveal.progress == 0.0 && self->attachments_leaving != NULL)
    g_clear_pointer (&self->attachments_leaving, gtk_widget_unparent);
}

static gboolean
motion_tick_cb (GtkWidget     *widget,
                GdkFrameClock *clock,
                gpointer       user_data)
{
  AwraComposer *self = AWRA_COMPOSER (widget);
  gint64 now = gdk_frame_clock_get_frame_time (clock);
  guint normal = motion_duration (self, AWRA_MOTION_PRESET_NORMAL);
  guint fast = motion_duration (self, AWRA_MOTION_PRESET_FAST);
  gboolean moved = FALSE;

  (void) user_data;
  moved |= reveal_step (&self->banner_reveal, now, normal);
  moved |= reveal_step (&self->attachments_reveal, now, normal);
  moved |= reveal_step (&self->send_reveal, now, fast);
  if (self->row_animating) {
    double t;

    if (self->row_started == 0)
      self->row_started = now;
    t = normal == 0 ? 1.0 : (double) (now - self->row_started) / (normal * 1000.0);
    if (t >= 1.0) {
      self->row_displayed = self->row_to;
      self->row_animating = FALSE;
    } else {
      self->row_displayed = self->row_from +
                            (self->row_to - self->row_from) * awra_motion_ease_out_cubic (t);
    }
    moved = TRUE;
  }
  if (moved) {
    drop_leaving (self);
    gtk_widget_queue_resize (widget);
  }
  if (self->busy) {
    if (self->busy_last != 0)
      self->busy_angle = fmod (self->busy_angle + (now - self->busy_last) / 1000000.0 * 360.0, 360.0);
    self->busy_last = now;
    gtk_widget_queue_draw (widget);
    return G_SOURCE_CONTINUE;
  }
  if (moved)
    return G_SOURCE_CONTINUE;
  self->tick = 0;
  return G_SOURCE_REMOVE;
}

static void
ensure_tick (AwraComposer *self)
{
  if (self->tick == 0 && gtk_widget_get_mapped (GTK_WIDGET (self)))
    self->tick = gtk_widget_add_tick_callback (GTK_WIDGET (self), motion_tick_cb, NULL, NULL);
}

static void
reveal_set (AwraComposer *self,
            Reveal       *reveal,
            gboolean      shown)
{
  if (reveal->shown == shown)
    return;
  reveal->shown = shown;
  reveal->from = reveal->progress;
  reveal->started = 0;
  if (!gtk_widget_get_mapped (GTK_WIDGET (self)) ||
      motion_duration (self, AWRA_MOTION_PRESET_NORMAL) == 0) {
    reveal->progress = shown ? 1.0 : 0.0;
    drop_leaving (self);
  } else {
    ensure_tick (self);
  }
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/* Text. */

static gboolean
buffer_has_text (GtkTextBuffer *buffer)
{
  GtkTextIter iter;

  if (gtk_text_buffer_get_char_count (buffer) == 0)
    return FALSE;
  gtk_text_buffer_get_start_iter (buffer, &iter);
  do {
    gunichar c = gtk_text_iter_get_char (&iter);

    /* An embedded object, such as an inline image, counts as content. */
    if (c == 0xFFFC || !g_unichar_isspace (c))
      return TRUE;
  } while (gtk_text_iter_forward_char (&iter));
  return FALSE;
}

static gboolean
sendable (AwraComposer *self)
{
  return self->has_text || self->can_send;
}

static void
update_send (AwraComposer *self)
{
  reveal_set (self, &self->send_reveal, sendable (self));
  gtk_widget_set_can_target (self->send, sendable (self));
  gtk_widget_set_can_focus (self->send, sendable (self));
}

static void
update_placeholder (AwraComposer *self)
{
  gtk_widget_set_visible (GTK_WIDGET (self->placeholder),
                          gtk_text_buffer_get_char_count (gtk_text_view_get_buffer (self->view)) == 0 &&
                          !self->preedit);
}

static void
buffer_changed_cb (GtkTextBuffer *buffer,
                   AwraComposer  *self)
{
  g_clear_pointer (&self->text_cache, g_free);
  self->has_text = buffer_has_text (buffer);
  self->text_changed_until = g_get_monotonic_time () + TEXT_CHANGE_WINDOW_US;
  update_placeholder (self);
  update_send (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT]);
}

static void
preedit_changed_cb (GtkTextView  *view,
                    const char   *preedit,
                    AwraComposer *self)
{
  (void) view;
  self->preedit = preedit != NULL && *preedit != '\0';
  update_placeholder (self);
}

static void
submit (AwraComposer *self)
{
  if (!sendable (self) || !gtk_widget_is_sensitive (GTK_WIDGET (self)))
    return;
  g_signal_emit (self, signals[SIGNAL_SUBMIT], 0);
}

static gboolean
key_pressed_cb (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                AwraComposer          *self)
{
  GdkModifierType modifiers = state & gtk_accelerator_get_default_mod_mask ();

  (void) controller;
  (void) keycode;
  if (keyval != GDK_KEY_Return && keyval != GDK_KEY_KP_Enter && keyval != GDK_KEY_ISO_Enter)
    return FALSE;
  /* The input method is composing, or a completion takes Enter. */
  if (self->preedit || awra_completion_popover_is_active_for (GTK_WIDGET (self->view)))
    return FALSE;
  if (self->send_on_enter) {
    if (modifiers != 0)
      return FALSE;
  } else if (modifiers != GDK_CONTROL_MASK) {
    return FALSE;
  }
  submit (self);
  return TRUE;
}

static void
send_clicked_cb (GtkButton    *button,
                 AwraComposer *self)
{
  (void) button;
  submit (self);
  gtk_widget_grab_focus (GTK_WIDGET (self->view));
}

static void
update_line_height (AwraComposer *self)
{
  g_autoptr (PangoLayout) layout = gtk_widget_create_pango_layout (GTK_WIDGET (self->view), "Ag");
  int height = 0;

  pango_layout_get_pixel_size (layout, NULL, &height);
  self->line_height = MAX (height, 1);
  gtk_scrolled_window_set_max_content_height (
    self->scroller, self->line_height * (int) self->max_lines + 2 * TEXT_MARGIN_Y);
  /* The text view knows its height only after its first allocation: one
   * line is the floor from the start. */
  gtk_widget_set_size_request (GTK_WIDGET (self->scroller), -1,
                               self->line_height + 2 * TEXT_MARGIN_Y);
}

static int
single_line_height (AwraComposer *self)
{
  return self->line_height + 2 * TEXT_MARGIN_Y;
}

/* Layout. */

typedef struct {
  int start;
  int field;
  int end;
  int send;
} RowWidths;

static gboolean
child_shown (GtkWidget *child)
{
  return child != NULL && gtk_widget_get_visible (child);
}

static int
natural_width (GtkWidget *child)
{
  int natural = 0;

  if (!child_shown (child))
    return 0;
  gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &natural, NULL, NULL);
  return natural;
}

static int
height_for (GtkWidget *child,
            int        width)
{
  int natural = 0;

  if (!child_shown (child))
    return 0;
  gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, width, NULL, &natural, NULL, NULL);
  return natural;
}

static int
gaps (AwraComposer *self)
{
  int count = 0;

  count += child_shown (self->start_widget);
  count += child_shown (self->end_widget);
  return count * GAP + (int) round (eased (&self->send_reveal) * GAP);
}

static RowWidths
row_widths (AwraComposer *self,
            int           width)
{
  RowWidths widths;
  int field_min = 0;

  widths.start = natural_width (self->start_widget);
  widths.end = natural_width (self->end_widget);
  widths.send = (int) round (natural_width (self->send) * eased (&self->send_reveal));
  gtk_widget_measure (self->field, GTK_ORIENTATION_HORIZONTAL, -1, &field_min, NULL, NULL, NULL);
  widths.field = width < 0 ? -1
                           : MAX (field_min, width - widths.start - widths.end - widths.send - gaps (self));
  return widths;
}

static int
row_height_for (AwraComposer *self,
                int           width)
{
  RowWidths widths = row_widths (self, width);
  int height = height_for (self->field, widths.field);

  height = MAX (height, height_for (self->start_widget, widths.start));
  height = MAX (height, height_for (self->end_widget, widths.end));
  height = MAX (height, height_for (self->send, -1));
  return height;
}

static double
section_height (GtkWidget    *child,
                const Reveal *reveal,
                int           width)
{
  if (child == NULL || reveal->progress <= 0.0)
    return 0.0;
  return (height_for (child, width) + GAP) * awra_motion_ease_out_cubic (reveal->progress);
}

static void
awra_composer_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  AwraComposer *self = AWRA_COMPOSER (widget);

  (void) minimum_baseline;
  (void) natural_baseline;
  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    int field_min = 0;
    int field_nat = 0;
    int row_min;
    int row_nat;
    GtkWidget *sections[] = { shown_banner (self), shown_attachments (self) };

    gtk_widget_measure (self->field, orientation, -1, &field_min, &field_nat, NULL, NULL);
    row_min = row_nat = natural_width (self->start_widget) + natural_width (self->end_widget) +
                        (int) round (natural_width (self->send) * eased (&self->send_reveal)) +
                        gaps (self);
    row_min += field_min;
    row_nat += field_nat;
    for (guint i = 0; i < G_N_ELEMENTS (sections); i++) {
      int section_min = 0;
      int section_nat = 0;

      if (sections[i] == NULL)
        continue;
      gtk_widget_measure (sections[i], orientation, -1, &section_min, &section_nat, NULL, NULL);
      row_min = MAX (row_min, section_min);
      row_nat = MAX (row_nat, section_nat);
    }
    *minimum = row_min;
    *natural = row_nat;
  } else {
    double row;
    double height;

    if ((self->row_animating || g_get_monotonic_time () < self->text_changed_until) &&
        self->row_displayed > 0)
      row = self->row_displayed;
    else
      row = row_height_for (self, for_size);
    height = row + section_height (shown_banner (self), &self->banner_reveal, for_size) +
             section_height (shown_attachments (self), &self->attachments_reveal, for_size);
    *minimum = *natural = (int) ceil (height);
  }
}

static void
allocate_section (GtkWidget    *child,
                  const Reveal *reveal,
                  int           width,
                  double       *y)
{
  double region;
  int height;

  if (child == NULL)
    return;
  height = height_for (child, width);
  region = reveal->progress <= 0.0 ? 0.0 : (height + GAP) * awra_motion_ease_out_cubic (reveal->progress);
  /* The section slides out from under the part below it. */
  gtk_widget_allocate (child, width, height, -1,
                       gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (
                         0, (float) (*y + region - GAP - height))));
  *y += region;
}

static void
allocate_row_child (GtkWidget *child,
                    int        x,
                    int        width,
                    int        bottom,
                    int        band)
{
  int height;

  if (!child_shown (child) || width <= 0)
    return;
  height = height_for (child, width);
  /* Buttons stay level with the last line while the text grows. */
  gtk_widget_allocate (child, width, height, -1,
                       gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (
                         x, bottom - band + (band - height) / 2.0f)));
}

static void
awra_composer_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  AwraComposer *self = AWRA_COMPOSER (widget);
  double y = 0.0;
  double target = row_height_for (self, width);
  RowWidths widths = row_widths (self, width);
  gboolean rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  int band = MAX (single_line_height (self), height_for (self->send, -1));
  int x = 0;
  int field_height;

  (void) baseline;
  allocate_section (shown_banner (self), &self->banner_reveal, width, &y);
  allocate_section (shown_attachments (self), &self->attachments_reveal, width, &y);

  if (self->row_displayed <= 0 || fabs (target - self->row_to) >= 0.5 || !self->row_animating) {
    if (self->row_displayed > 0 && fabs (target - self->row_displayed) >= 0.5 &&
        g_get_monotonic_time () < self->text_changed_until &&
        gtk_widget_get_mapped (widget) && motion_duration (self, AWRA_MOTION_PRESET_NORMAL) > 0) {
      if (!self->row_animating || fabs (target - self->row_to) >= 0.5) {
        self->row_from = self->row_displayed;
        self->row_to = target;
        self->row_started = 0;
        self->row_animating = TRUE;
        ensure_tick (self);
      }
    } else if (!self->row_animating) {
      self->row_displayed = self->row_to = target;
    }
  }

  field_height = height_for (self->field, widths.field);
  band = MIN (band, MAX (field_height, band));
#define PLACE(child, child_width) \
  G_STMT_START { \
    int place_x = rtl ? width - x - (child_width) : x; \
    allocate_row_child ((child), place_x, (child_width), height, band); \
    if (child_shown (child) && (child_width) > 0) \
      x += (child_width) + GAP; \
  } G_STMT_END
  PLACE (self->start_widget, widths.start);
  {
    int place_x = rtl ? width - x - widths.field : x;

    gtk_widget_allocate (self->field, widths.field, field_height, -1,
                         gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (
                           place_x, height - field_height)));
    x += widths.field + GAP;
  }
  PLACE (self->end_widget, widths.end);
#undef PLACE
  {
    int send_width = natural_width (self->send);

    /* The button keeps its size at its final place; the snapshot grows it
     * from nothing inside the space that opens for it. */
    allocate_row_child (self->send, rtl ? 0 : width - send_width, send_width, height, band);
  }
}

static void
snapshot_section (GtkWidget    *widget,
                  GtkWidget    *child,
                  const Reveal *reveal,
                  GtkSnapshot  *snapshot,
                  double       *y)
{
  double region;
  int height;

  if (child == NULL || reveal->progress <= 0.0)
    return;
  height = gtk_widget_get_height (child);
  region = (height + GAP) * awra_motion_ease_out_cubic (reveal->progress);
  if (reveal->progress >= 1.0) {
    gtk_widget_snapshot_child (widget, child, snapshot);
  } else {
    gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (
      -GAP, (float) *y, gtk_widget_get_width (widget) + 2 * GAP, (float) region));
    gtk_snapshot_push_opacity (snapshot, awra_motion_ease_out_cubic (reveal->progress));
    gtk_widget_snapshot_child (widget, child, snapshot);
    gtk_snapshot_pop (snapshot);
    gtk_snapshot_pop (snapshot);
  }
  *y += region;
}

static void
snapshot_busy (AwraComposer *self,
               GtkSnapshot  *snapshot)
{
  AwraContext *context = awra_context_get_for_display (gtk_widget_get_display (GTK_WIDGET (self)));
  AwraTokenSet *tokens = awra_style_manager_get_token_set (awra_context_get_style_manager (context));
  g_autoptr (GskPathBuilder) builder = gsk_path_builder_new ();
  g_autoptr (GskPath) path = NULL;
  g_autoptr (GskStroke) stroke = gsk_stroke_new (2.0f);
  graphene_rect_t bounds;
  float radius;
  graphene_point_t center;
  double start = self->busy_angle * G_PI / 180.0;
  double sweep = G_PI * 0.6;

  if (!gtk_widget_compute_bounds (self->send, GTK_WIDGET (self), &bounds))
    return;
  graphene_rect_get_center (&bounds, &center);
  radius = MIN (bounds.size.width, bounds.size.height) / 2.0f - 3.0f;
  if (radius <= 2.0f)
    return;
  gsk_path_builder_move_to (builder, center.x + radius * cos (start), center.y + radius * sin (start));
  gsk_path_builder_svg_arc_to (builder, radius, radius, 0, FALSE, TRUE,
                               center.x + radius * cos (start + sweep),
                               center.y + radius * sin (start + sweep));
  path = gsk_path_builder_to_path (g_steal_pointer (&builder));
  gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_ROUND);
  gtk_snapshot_append_stroke (snapshot, path, stroke, awra_token_set_get_on_accent (tokens));
}

static void
awra_composer_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  AwraComposer *self = AWRA_COMPOSER (widget);
  int width = gtk_widget_get_width (widget);
  int height = gtk_widget_get_height (widget);
  double y = 0.0;
  double send = eased (&self->send_reveal);
  gboolean clip_row;

  snapshot_section (widget, shown_banner (self), &self->banner_reveal, snapshot, &y);
  snapshot_section (widget, shown_attachments (self), &self->attachments_reveal, snapshot, &y);

  /* While the well glides to a new height, the row is cut at its top. */
  clip_row = height - y < gtk_widget_get_height (self->field) - 0.5;
  if (clip_row)
    gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (-GAP, (float) y, width + 2 * GAP,
                                                           (float) (height - y) + GAP));
  if (child_shown (self->start_widget))
    gtk_widget_snapshot_child (widget, self->start_widget, snapshot);
  gtk_widget_snapshot_child (widget, self->field, snapshot);
  if (child_shown (self->end_widget))
    gtk_widget_snapshot_child (widget, self->end_widget, snapshot);
  if (send > 0.0) {
    graphene_rect_t bounds;

    if (send < 1.0 && gtk_widget_compute_bounds (self->send, widget, &bounds)) {
      graphene_point_t center;
      float scale = 0.55f + 0.45f * (float) send;

      graphene_rect_get_center (&bounds, &center);
      gtk_snapshot_save (snapshot);
      /* It grows from the end of the space that opens for it. */
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (
        (float) ((1.0 - send) * bounds.size.width / 2.0) *
          (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL ? -1 : 1) + center.x,
        center.y));
      gtk_snapshot_scale (snapshot, scale, scale);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-center.x, -center.y));
      gtk_snapshot_push_opacity (snapshot, send);
      gtk_widget_snapshot_child (widget, self->send, snapshot);
      gtk_snapshot_pop (snapshot);
      gtk_snapshot_restore (snapshot);
    } else {
      gtk_widget_snapshot_child (widget, self->send, snapshot);
      if (self->busy)
        snapshot_busy (self, snapshot);
    }
  }
  if (clip_row)
    gtk_snapshot_pop (snapshot);
}

static gboolean
awra_composer_grab_focus (GtkWidget *widget)
{
  return gtk_widget_grab_focus (GTK_WIDGET (AWRA_COMPOSER (widget)->view));
}

static void
awra_composer_css_changed (GtkWidget         *widget,
                           GtkCssStyleChange *change)
{
  GTK_WIDGET_CLASS (awra_composer_parent_class)->css_changed (widget, change);
  update_line_height (AWRA_COMPOSER (widget));
}

static void
awra_composer_map (GtkWidget *widget)
{
  AwraComposer *self = AWRA_COMPOSER (widget);

  GTK_WIDGET_CLASS (awra_composer_parent_class)->map (widget);
  if (self->busy)
    ensure_tick (self);
}

static void
awra_composer_unmap (GtkWidget *widget)
{
  AwraComposer *self = AWRA_COMPOSER (widget);

  if (self->tick != 0) {
    gtk_widget_remove_tick_callback (widget, self->tick);
    self->tick = 0;
  }
  /* Whatever was moving reaches its end. */
  self->banner_reveal.progress = self->banner_reveal.shown ? 1.0 : 0.0;
  self->attachments_reveal.progress = self->attachments_reveal.shown ? 1.0 : 0.0;
  self->send_reveal.progress = self->send_reveal.shown ? 1.0 : 0.0;
  if (self->row_animating) {
    self->row_animating = FALSE;
    self->row_displayed = self->row_to;
  }
  self->busy_last = 0;
  drop_leaving (self);
  GTK_WIDGET_CLASS (awra_composer_parent_class)->unmap (widget);
}

static void
awra_composer_dispose (GObject *object)
{
  AwraComposer *self = AWRA_COMPOSER (object);

  if (self->tick != 0) {
    gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick);
    self->tick = 0;
  }
  if (self->view != NULL)
    g_signal_handlers_disconnect_by_data (gtk_text_view_get_buffer (self->view), self);
  g_clear_pointer (&self->banner, gtk_widget_unparent);
  g_clear_pointer (&self->banner_leaving, gtk_widget_unparent);
  g_clear_pointer (&self->attachments, gtk_widget_unparent);
  g_clear_pointer (&self->attachments_leaving, gtk_widget_unparent);
  g_clear_pointer (&self->start_widget, gtk_widget_unparent);
  g_clear_pointer (&self->end_widget, gtk_widget_unparent);
  g_clear_pointer (&self->field, gtk_widget_unparent);
  g_clear_pointer (&self->send, gtk_widget_unparent);
  self->view = NULL;
  self->scroller = NULL;
  self->placeholder = NULL;
  G_OBJECT_CLASS (awra_composer_parent_class)->dispose (object);
}

static void
awra_composer_finalize (GObject *object)
{
  AwraComposer *self = AWRA_COMPOSER (object);

  g_free (self->text_cache);
  G_OBJECT_CLASS (awra_composer_parent_class)->finalize (object);
}

static void
awra_composer_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  AwraComposer *self = AWRA_COMPOSER (object);

  switch (prop_id) {
  case PROP_TEXT:
    g_value_set_string (value, awra_composer_get_text (self));
    break;
  case PROP_PLACEHOLDER_TEXT:
    g_value_set_string (value, awra_composer_get_placeholder_text (self));
    break;
  case PROP_MAX_LINES:
    g_value_set_uint (value, self->max_lines);
    break;
  case PROP_CAN_SEND:
    g_value_set_boolean (value, self->can_send);
    break;
  case PROP_SEND_ON_ENTER:
    g_value_set_boolean (value, self->send_on_enter);
    break;
  case PROP_BUSY:
    g_value_set_boolean (value, self->busy);
    break;
  case PROP_BANNER:
    g_value_set_object (value, self->banner);
    break;
  case PROP_ATTACHMENTS:
    g_value_set_object (value, self->attachments);
    break;
  case PROP_START_WIDGET:
    g_value_set_object (value, self->start_widget);
    break;
  case PROP_END_WIDGET:
    g_value_set_object (value, self->end_widget);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_composer_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  AwraComposer *self = AWRA_COMPOSER (object);

  switch (prop_id) {
  case PROP_TEXT:
    awra_composer_set_text (self, g_value_get_string (value));
    break;
  case PROP_PLACEHOLDER_TEXT:
    awra_composer_set_placeholder_text (self, g_value_get_string (value));
    break;
  case PROP_MAX_LINES:
    awra_composer_set_max_lines (self, g_value_get_uint (value));
    break;
  case PROP_CAN_SEND:
    awra_composer_set_can_send (self, g_value_get_boolean (value));
    break;
  case PROP_SEND_ON_ENTER:
    awra_composer_set_send_on_enter (self, g_value_get_boolean (value));
    break;
  case PROP_BUSY:
    awra_composer_set_busy (self, g_value_get_boolean (value));
    break;
  case PROP_BANNER:
    awra_composer_set_banner (self, g_value_get_object (value));
    break;
  case PROP_ATTACHMENTS:
    awra_composer_set_attachments (self, g_value_get_object (value));
    break;
  case PROP_START_WIDGET:
    awra_composer_set_start_widget (self, g_value_get_object (value));
    break;
  case PROP_END_WIDGET:
    awra_composer_set_end_widget (self, g_value_get_object (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
awra_composer_class_init (AwraComposerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_composer_dispose;
  object_class->finalize = awra_composer_finalize;
  object_class->get_property = awra_composer_get_property;
  object_class->set_property = awra_composer_set_property;
  widget_class->measure = awra_composer_measure;
  widget_class->size_allocate = awra_composer_size_allocate;
  widget_class->snapshot = awra_composer_snapshot;
  widget_class->grab_focus = awra_composer_grab_focus;
  widget_class->css_changed = awra_composer_css_changed;
  widget_class->map = awra_composer_map;
  widget_class->unmap = awra_composer_unmap;

  /**
   * AwraComposer:text:
   *
   * The whole text.
   *
   * Since: 1.1
   */
  properties[PROP_TEXT] =
    g_param_spec_string ("text", NULL, NULL, "",
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraComposer:placeholder-text:
   *
   * The text shown while the composer is empty.
   *
   * Since: 1.1
   */
  properties[PROP_PLACEHOLDER_TEXT] =
    g_param_spec_string ("placeholder-text", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraComposer:max-lines:
   *
   * The number of lines shown before the text scrolls.
   *
   * Since: 1.1
   */
  properties[PROP_MAX_LINES] =
    g_param_spec_uint ("max-lines", NULL, NULL, 1, G_MAXUINT16, 10,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraComposer:can-send:
   *
   * Whether sending is offered even without text, for example with
   * attachments only.
   *
   * Since: 1.1
   */
  properties[PROP_CAN_SEND] =
    g_param_spec_boolean ("can-send", NULL, NULL, FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraComposer:send-on-enter:
   *
   * Whether Enter submits; Shift+Enter then inserts a new line. When off,
   * Ctrl+Enter submits.
   *
   * Since: 1.1
   */
  properties[PROP_SEND_ON_ENTER] =
    g_param_spec_boolean ("send-on-enter", NULL, NULL, TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraComposer:busy:
   *
   * Whether something is being sent; the send button shows progress.
   *
   * Since: 1.1
   */
  properties[PROP_BUSY] =
    g_param_spec_boolean ("busy", NULL, NULL, FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraComposer:banner:
   *
   * A widget shown above the text, such as "Replying to Ada".
   *
   * Since: 1.1
   */
  properties[PROP_BANNER] =
    g_param_spec_object ("banner", NULL, NULL, GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraComposer:attachments:
   *
   * A widget shown above the text, such as thumbnails of attached files.
   *
   * Since: 1.1
   */
  properties[PROP_ATTACHMENTS] =
    g_param_spec_object ("attachments", NULL, NULL, GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraComposer:start-widget:
   *
   * A widget before the text, such as an add button.
   *
   * Since: 1.1
   */
  properties[PROP_START_WIDGET] =
    g_param_spec_object ("start-widget", NULL, NULL, GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  /**
   * AwraComposer:end-widget:
   *
   * A widget after the text, such as emoji and sticker buttons.
   *
   * Since: 1.1
   */
  properties[PROP_END_WIDGET] =
    g_param_spec_object ("end-widget", NULL, NULL, GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * AwraComposer::submit:
   * @self: the composer
   *
   * Emitted when the user sends, with Enter or the send button, and there is
   * something to send. The handler reads the text and clears it.
   *
   * Since: 1.1
   */
  signals[SIGNAL_SUBMIT] =
    g_signal_new ("submit", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
  /**
   * AwraComposer::banner-closed:
   * @self: the composer
   *
   * Emitted when the close button of a banner made by
   * awra_composer_show_banner() removes it.
   *
   * Since: 1.1
   */
  signals[SIGNAL_BANNER_CLOSED] =
    g_signal_new ("banner-closed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, "awracomposer");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
awra_composer_init (AwraComposer *self)
{
  GtkEventController *keys = gtk_event_controller_key_new ();
  GtkTextBuffer *buffer;

  self->max_lines = 10;
  self->send_on_enter = TRUE;

  self->view = GTK_TEXT_VIEW (gtk_text_view_new ());
  gtk_text_view_set_wrap_mode (self->view, GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_accepts_tab (self->view, FALSE);
  gtk_text_view_set_top_margin (self->view, TEXT_MARGIN_Y);
  gtk_text_view_set_bottom_margin (self->view, TEXT_MARGIN_Y);
  gtk_text_view_set_left_margin (self->view, TEXT_MARGIN_X);
  gtk_text_view_set_right_margin (self->view, TEXT_MARGIN_X);
  buffer = gtk_text_view_get_buffer (self->view);
  g_signal_connect (buffer, "changed", G_CALLBACK (buffer_changed_cb), self);
  g_signal_connect (self->view, "preedit-changed", G_CALLBACK (preedit_changed_cb), self);
  gtk_event_controller_set_propagation_phase (keys, GTK_PHASE_CAPTURE);
  g_signal_connect (keys, "key-pressed", G_CALLBACK (key_pressed_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self->view), keys);

  self->scroller = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new ());
  gtk_scrolled_window_set_policy (self->scroller, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_propagate_natural_height (self->scroller, TRUE);
  gtk_scrolled_window_set_overlay_scrolling (self->scroller, TRUE);
  gtk_scrolled_window_set_child (self->scroller, GTK_WIDGET (self->view));

  self->placeholder = GTK_LABEL (gtk_label_new (NULL));
  gtk_widget_add_css_class (GTK_WIDGET (self->placeholder), "awra-composer-placeholder");
  gtk_label_set_ellipsize (self->placeholder, PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (self->placeholder, 0.0f);
  gtk_widget_set_halign (GTK_WIDGET (self->placeholder), GTK_ALIGN_FILL);
  gtk_widget_set_valign (GTK_WIDGET (self->placeholder), GTK_ALIGN_START);
  gtk_widget_set_margin_start (GTK_WIDGET (self->placeholder), TEXT_MARGIN_X);
  gtk_widget_set_margin_end (GTK_WIDGET (self->placeholder), TEXT_MARGIN_X);
  gtk_widget_set_margin_top (GTK_WIDGET (self->placeholder), TEXT_MARGIN_Y);
  gtk_widget_set_can_target (GTK_WIDGET (self->placeholder), FALSE);

  self->field = gtk_overlay_new ();
  gtk_widget_add_css_class (self->field, "awra-composer-field");
  gtk_overlay_set_child (GTK_OVERLAY (self->field), GTK_WIDGET (self->scroller));
  gtk_overlay_add_overlay (GTK_OVERLAY (self->field), GTK_WIDGET (self->placeholder));
  gtk_widget_set_parent (self->field, GTK_WIDGET (self));

  self->send = awra_button_new_from_icon_name ("awra-send-symbolic");
  awra_button_set_appearance (AWRA_BUTTON (self->send), AWRA_BUTTON_APPEARANCE_PRIMARY);
  gtk_widget_add_css_class (self->send, "awra-composer-send");
  gtk_widget_set_tooltip_text (self->send, _("Send"));
  gtk_accessible_update_property (GTK_ACCESSIBLE (self->send),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, _("Send"), -1);
  gtk_widget_set_can_target (self->send, FALSE);
  gtk_widget_set_can_focus (self->send, FALSE);
  g_signal_connect (self->send, "clicked", G_CALLBACK (send_clicked_cb), self);
  gtk_widget_set_parent (self->send, GTK_WIDGET (self));

  update_line_height (self);
  update_placeholder (self);
}

GtkWidget *
awra_composer_new (void)
{
  return g_object_new (AWRA_TYPE_COMPOSER, NULL);
}

GtkTextView *
awra_composer_get_view (AwraComposer *self)
{
  g_return_val_if_fail (AWRA_IS_COMPOSER (self), NULL);
  return self->view;
}

GtkTextBuffer *
awra_composer_get_buffer (AwraComposer *self)
{
  g_return_val_if_fail (AWRA_IS_COMPOSER (self), NULL);
  return gtk_text_view_get_buffer (self->view);
}

const char *
awra_composer_get_text (AwraComposer *self)
{
  g_return_val_if_fail (AWRA_IS_COMPOSER (self), NULL);
  if (self->text_cache == NULL) {
    GtkTextIter start;
    GtkTextIter end;

    gtk_text_buffer_get_bounds (gtk_text_view_get_buffer (self->view), &start, &end);
    self->text_cache = gtk_text_iter_get_text (&start, &end);
  }
  return self->text_cache;
}

void
awra_composer_set_text (AwraComposer *self,
                        const char   *text)
{
  GtkTextBuffer *buffer;
  GtkTextIter end;

  g_return_if_fail (AWRA_IS_COMPOSER (self));
  buffer = gtk_text_view_get_buffer (self->view);
  gtk_text_buffer_set_text (buffer, text != NULL ? text : "", -1);
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_place_cursor (buffer, &end);
}

const char *
awra_composer_get_placeholder_text (AwraComposer *self)
{
  const char *text;

  g_return_val_if_fail (AWRA_IS_COMPOSER (self), NULL);
  text = gtk_label_get_text (self->placeholder);
  return text != NULL && *text != '\0' ? text : NULL;
}

void
awra_composer_set_placeholder_text (AwraComposer *self,
                                    const char   *text)
{
  g_return_if_fail (AWRA_IS_COMPOSER (self));
  if (g_strcmp0 (awra_composer_get_placeholder_text (self), text) == 0)
    return;
  gtk_label_set_text (self->placeholder, text != NULL ? text : "");
  gtk_accessible_update_property (GTK_ACCESSIBLE (self->view),
                                  GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER, text, -1);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PLACEHOLDER_TEXT]);
}

guint
awra_composer_get_max_lines (AwraComposer *self)
{
  g_return_val_if_fail (AWRA_IS_COMPOSER (self), 10);
  return self->max_lines;
}

void
awra_composer_set_max_lines (AwraComposer *self,
                             guint         lines)
{
  g_return_if_fail (AWRA_IS_COMPOSER (self));
  g_return_if_fail (lines >= 1);
  if (self->max_lines == lines)
    return;
  self->max_lines = lines;
  update_line_height (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_LINES]);
}

gboolean
awra_composer_get_can_send (AwraComposer *self)
{
  g_return_val_if_fail (AWRA_IS_COMPOSER (self), FALSE);
  return self->can_send;
}

void
awra_composer_set_can_send (AwraComposer *self,
                            gboolean      can_send)
{
  g_return_if_fail (AWRA_IS_COMPOSER (self));
  can_send = !!can_send;
  if (self->can_send == can_send)
    return;
  self->can_send = can_send;
  update_send (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CAN_SEND]);
}

gboolean
awra_composer_get_send_on_enter (AwraComposer *self)
{
  g_return_val_if_fail (AWRA_IS_COMPOSER (self), TRUE);
  return self->send_on_enter;
}

void
awra_composer_set_send_on_enter (AwraComposer *self,
                                 gboolean      send_on_enter)
{
  g_return_if_fail (AWRA_IS_COMPOSER (self));
  send_on_enter = !!send_on_enter;
  if (self->send_on_enter == send_on_enter)
    return;
  self->send_on_enter = send_on_enter;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEND_ON_ENTER]);
}

gboolean
awra_composer_get_busy (AwraComposer *self)
{
  g_return_val_if_fail (AWRA_IS_COMPOSER (self), FALSE);
  return self->busy;
}

void
awra_composer_set_busy (AwraComposer *self,
                        gboolean      busy)
{
  g_return_if_fail (AWRA_IS_COMPOSER (self));
  busy = !!busy;
  if (self->busy == busy)
    return;
  self->busy = busy;
  self->busy_last = 0;
  if (busy) {
    gtk_widget_add_css_class (self->send, "awra-composer-busy");
    ensure_tick (self);
  } else {
    gtk_widget_remove_css_class (self->send, "awra-composer-busy");
  }
  gtk_accessible_update_state (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_STATE_BUSY, busy, -1);
  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BUSY]);
}

/* Slots. The banner and the attachments slide; the row slots do not. */

static void
set_section (AwraComposer *self,
             GtkWidget   **slot,
             GtkWidget   **leaving,
             Reveal       *reveal,
             GtkWidget    *child,
             GtkWidget    *before,
             const char   *css_class)
{
  if (*slot != NULL) {
    gtk_widget_remove_css_class (*slot, css_class);
    if (child == NULL && gtk_widget_get_mapped (GTK_WIDGET (self)) &&
        motion_duration (self, AWRA_MOTION_PRESET_NORMAL) > 0) {
      /* The old one stays on screen while it slides out. */
      g_clear_pointer (leaving, gtk_widget_unparent);
      *leaving = g_steal_pointer (slot);
    } else {
      g_clear_pointer (slot, gtk_widget_unparent);
    }
  }
  if (child != NULL) {
    g_clear_pointer (leaving, gtk_widget_unparent);
    *slot = child;
    gtk_widget_add_css_class (child, css_class);
    gtk_widget_insert_before (child, GTK_WIDGET (self), before);
  }
  reveal_set (self, reveal, child != NULL);
  if (child == NULL && reveal->progress == 0.0)
    g_clear_pointer (leaving, gtk_widget_unparent);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static GtkWidget *
first_row_child (AwraComposer *self)
{
  return self->start_widget != NULL ? self->start_widget : self->field;
}

GtkWidget *
awra_composer_get_banner (AwraComposer *self)
{
  g_return_val_if_fail (AWRA_IS_COMPOSER (self), NULL);
  return self->banner;
}

void
awra_composer_set_banner (AwraComposer *self,
                          GtkWidget    *banner)
{
  g_return_if_fail (AWRA_IS_COMPOSER (self));
  g_return_if_fail (banner == NULL || GTK_IS_WIDGET (banner));
  if (self->banner == banner)
    return;
  g_return_if_fail (banner == NULL || gtk_widget_get_parent (banner) == NULL);
  set_section (self, &self->banner, &self->banner_leaving, &self->banner_reveal, banner,
               self->attachments != NULL ? self->attachments : first_row_child (self),
               "awra-composer-banner");
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BANNER]);
}

static void
banner_close_cb (GtkButton    *button,
                 AwraComposer *self)
{
  GtkWidget *banner = gtk_widget_get_parent (GTK_WIDGET (button));

  g_object_ref (self);
  if (self->banner == banner) {
    awra_composer_set_banner (self, NULL);
    g_signal_emit (self, signals[SIGNAL_BANNER_CLOSED], 0);
  }
  if (gtk_widget_get_mapped (GTK_WIDGET (self)))
    gtk_widget_grab_focus (GTK_WIDGET (self->view));
  g_object_unref (self);
}

void
awra_composer_show_banner (AwraComposer *self,
                           const char   *text,
                           gboolean      closable)
{
  GtkWidget *banner;
  GtkWidget *label;

  g_return_if_fail (AWRA_IS_COMPOSER (self));
  g_return_if_fail (text != NULL);
  banner = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GAP);
  gtk_widget_add_css_class (banner, "awra-composer-banner-text");
  label = gtk_label_new (text);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_box_append (GTK_BOX (banner), label);
  if (closable) {
    GtkWidget *close = awra_button_new_from_icon_name ("awra-close-symbolic");

    awra_button_set_appearance (AWRA_BUTTON (close), AWRA_BUTTON_APPEARANCE_GHOST);
    gtk_widget_add_css_class (close, "awra-composer-banner-close");
    gtk_widget_set_tooltip_text (close, _("Close"));
    gtk_accessible_update_property (GTK_ACCESSIBLE (close),
                                    GTK_ACCESSIBLE_PROPERTY_LABEL, _("Close"), -1);
    g_signal_connect (close, "clicked", G_CALLBACK (banner_close_cb), self);
    gtk_box_append (GTK_BOX (banner), close);
  }
  if (self->banner != NULL) {
    /* Replacing a banner swaps it in place. */
    gtk_widget_remove_css_class (self->banner, "awra-composer-banner");
    g_clear_pointer (&self->banner, gtk_widget_unparent);
  }
  awra_composer_set_banner (self, banner);
}

GtkWidget *
awra_composer_get_attachments (AwraComposer *self)
{
  g_return_val_if_fail (AWRA_IS_COMPOSER (self), NULL);
  return self->attachments;
}

void
awra_composer_set_attachments (AwraComposer *self,
                               GtkWidget    *attachments)
{
  g_return_if_fail (AWRA_IS_COMPOSER (self));
  g_return_if_fail (attachments == NULL || GTK_IS_WIDGET (attachments));
  if (self->attachments == attachments)
    return;
  g_return_if_fail (attachments == NULL || gtk_widget_get_parent (attachments) == NULL);
  set_section (self, &self->attachments, &self->attachments_leaving, &self->attachments_reveal,
               attachments, first_row_child (self), "awra-composer-attachments");
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ATTACHMENTS]);
}

static void
set_row_slot (AwraComposer *self,
              GtkWidget   **slot,
              GtkWidget    *child,
              GtkWidget    *before,
              GParamSpec   *pspec)
{
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));
  if (*slot == child)
    return;
  g_return_if_fail (child == NULL || gtk_widget_get_parent (child) == NULL);
  g_clear_pointer (slot, gtk_widget_unparent);
  *slot = child;
  if (child != NULL)
    gtk_widget_insert_before (child, GTK_WIDGET (self), before);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), pspec);
}

GtkWidget *
awra_composer_get_start_widget (AwraComposer *self)
{
  g_return_val_if_fail (AWRA_IS_COMPOSER (self), NULL);
  return self->start_widget;
}

void
awra_composer_set_start_widget (AwraComposer *self,
                                GtkWidget    *widget)
{
  g_return_if_fail (AWRA_IS_COMPOSER (self));
  set_row_slot (self, &self->start_widget, widget, self->field, properties[PROP_START_WIDGET]);
}

GtkWidget *
awra_composer_get_end_widget (AwraComposer *self)
{
  g_return_val_if_fail (AWRA_IS_COMPOSER (self), NULL);
  return self->end_widget;
}

void
awra_composer_set_end_widget (AwraComposer *self,
                              GtkWidget    *widget)
{
  g_return_if_fail (AWRA_IS_COMPOSER (self));
  set_row_slot (self, &self->end_widget, widget, self->send, properties[PROP_END_WIDGET]);
}
