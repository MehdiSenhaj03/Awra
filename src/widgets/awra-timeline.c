/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "awra-i18n-private.h"
#include "widgets/awra-media-time-private.h"
#include <awra/awra-context.h>
#include <awra/awra-timeline.h>
#include <math.h>

typedef struct
{
  double start, end;
  char *label;
} Interval;
struct _AwraTimeline
{
  GtkWidget parent_instance;
  double step_increment, page_increment;
  GArray *intervals;
  double duration, position, view_start, view_duration, offset, hover_time;
  double drag_x, drag_offset, drag_target;
  gboolean editable, dragging, inside;
  double hover_x;
  AwraPopover *preview;
  GtkWidget *preview_content;
  AwraStyleManager *style;
  gulong style_handler;
};
enum
{
  P_0,
  P_DURATION,
  P_POSITION,
  P_VIEW_START,
  P_VIEW_DURATION,
  P_OFFSET,
  P_EDITABLE,
  P_HOVER_TIME,
  P_HOVER_X,
  P_PREVIEW,
  P_STEP_INCREMENT,
  P_PAGE_INCREMENT,
  N_PROPS
};
enum
{
  SEEK,
  COMMIT,
  N_SIGNALS
};
static GParamSpec *props[N_PROPS];
static guint signals[N_SIGNALS];
static void range_init (GtkAccessibleRangeInterface *iface);
static void hide_preview (AwraTimeline *self);
G_DEFINE_FINAL_TYPE_WITH_CODE (AwraTimeline, awra_timeline, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE_RANGE,
                                                      range_init))

static double
span (AwraTimeline *s)
{
  return s->view_duration > 0 ? s->view_duration : MAX (s->duration, 1.0);
}
static double
time_x (AwraTimeline *s, double time)
{
  double f = (time - s->view_start) / span (s);
  if (gtk_widget_get_direction (GTK_WIDGET (s)) == GTK_TEXT_DIR_RTL)
    f = 1 - f;
  return 8 + f * MAX (gtk_widget_get_width (GTK_WIDGET (s)) - 16, 1);
}
static double
x_time (AwraTimeline *s, double x)
{
  double f = CLAMP ((x - 8) / MAX (gtk_widget_get_width (GTK_WIDGET (s)) - 16, 1), 0, 1);
  if (gtk_widget_get_direction (GTK_WIDGET (s)) == GTK_TEXT_DIR_RTL)
    f = 1 - f;
  return CLAMP (s->view_start + f * span (s), 0, s->duration);
}
static void
accessible (AwraTimeline *s)
{
  g_autofree char *text = s->editable ? g_strdup_printf ("%+.2f s", s->offset)
                                      : awra_media_format_time (s->position);
  gtk_accessible_update_property (
      GTK_ACCESSIBLE (s), GTK_ACCESSIBLE_PROPERTY_VALUE_MIN,
      s->editable ? -MAX (s->duration, fabs (s->offset)) : 0.0,
      GTK_ACCESSIBLE_PROPERTY_VALUE_MAX,
      s->editable ? MAX (s->duration, fabs (s->offset)) : s->duration,
      GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, s->editable ? s->offset : s->position,
      GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT, text, -1);
}
static gboolean
request (AwraTimeline *s, double time)
{
  if (s->duration <= 0 || !isfinite (time) || !gtk_widget_is_sensitive (GTK_WIDGET (s)))
    return FALSE;
  g_signal_emit (s, signals[SEEK], 0, CLAMP (time, 0, s->duration));
  return TRUE;
}
static gboolean
range_set (GtkAccessibleRange *r, double value)
{
  AwraTimeline *s = AWRA_TIMELINE (r);
  if (s->editable && isfinite (value) && gtk_widget_is_sensitive (GTK_WIDGET (s)))
    {
      awra_timeline_set_offset (s, value);
      g_signal_emit (s, signals[COMMIT], 0, s->offset);
      return TRUE;
    }
  return request (s, value);
}
static void
range_init (GtkAccessibleRangeInterface *i)
{
  i->set_current_value = range_set;
}
static void
invalidate (AwraTimeline *s)
{
  gtk_widget_queue_draw (GTK_WIDGET (s));
}
static void
style_changed (GObject *o, GParamSpec *p, AwraTimeline *s)
{
  (void)o;
  (void)p;
  invalidate (s);
}
static void
map (GtkWidget *w)
{
  AwraTimeline *s = AWRA_TIMELINE (w);
  GTK_WIDGET_CLASS (awra_timeline_parent_class)->map (w);
  s->style = awra_context_get_style_manager (
      awra_context_get_for_display (gtk_widget_get_display (w)));
  s->style_handler = g_signal_connect (s->style, "notify", G_CALLBACK (style_changed), s);
}
static void
unmap (GtkWidget *w)
{
  AwraTimeline *s = AWRA_TIMELINE (w);
  s->inside = s->dragging = FALSE;
  hide_preview (s);
  if (s->style)
    g_clear_signal_handler (&s->style_handler, s->style);
  s->style = NULL;
  GTK_WIDGET_CLASS (awra_timeline_parent_class)->unmap (w);
}
static void
measure (GtkWidget *w, GtkOrientation o, int f, int *min, int *nat, int *mb, int *nb)
{
  (void)w;
  (void)f;
  *min = o == GTK_ORIENTATION_HORIZONTAL ? 64 : 40;
  *nat = o == GTK_ORIENTATION_HORIZONTAL ? 320 : 56;
  *mb = *nb = -1;
}
static void
rect (GtkSnapshot *sn, const GdkRGBA *color, float x, float y, float w, float h,
      float radius)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (x, y, w, h);
  GskRoundedRect r;
  if (w <= 0 || h <= 0)
    return;
  gsk_rounded_rect_init_from_rect (&r, &bounds, radius);
  gtk_snapshot_push_rounded_clip (sn, &r);
  gtk_snapshot_append_color (sn, color, &bounds);
  gtk_snapshot_pop (sn);
}
static void
text_at (GtkWidget *w, GtkSnapshot *sn, const char *str, const GdkRGBA *color, float x,
         float y, int width)
{
  g_autoptr (PangoLayout) l = gtk_widget_create_pango_layout (w, str);
  graphene_point_t p = GRAPHENE_POINT_INIT (x, y);
  pango_layout_set_width (l, MAX (width, 1) * PANGO_SCALE);
  pango_layout_set_ellipsize (l, PANGO_ELLIPSIZE_END);
  pango_layout_set_single_paragraph_mode (l, TRUE);
  gtk_snapshot_save (sn);
  gtk_snapshot_translate (sn, &p);
  gtk_snapshot_append_layout (sn, l, color);
  gtk_snapshot_restore (sn);
}
static void
snapshot (GtkWidget *w, GtkSnapshot *sn)
{
  AwraTimeline *s = AWRA_TIMELINE (w);
  AwraStyleManager *style
      = s->style ? s->style
                 : awra_context_get_style_manager (
                       awra_context_get_for_display (gtk_widget_get_display (w)));
  AwraTokenSet *t = awra_style_manager_get_token_set (style);
  float width = gtk_widget_get_width (w), h = gtk_widget_get_height (w);
  float radius = awra_token_set_get_radius_for_size (t, AWRA_RADIUS_SMALL);
  const GdkRGBA *fg = gtk_widget_is_sensitive (w)
                          ? awra_token_set_get_foreground (t)
                          : awra_token_set_get_disabled_foreground (t);
  const GdkRGBA *accent
      = gtk_widget_is_sensitive (w) ? awra_token_set_get_accent (t) : fg;
  gboolean rtl = gtk_widget_get_direction (w) == GTK_TEXT_DIR_RTL;
  double pos
      = CLAMP (time_x (s, s->dragging && !s->editable ? s->drag_target : s->position), 8,
               MAX (width - 8, 8));
  rect (sn, awra_token_set_get_control_well (t), 4, 4, width - 8, h - 8, radius);
  rect (sn, awra_token_set_get_control_track (t), 8, h / 2 - 4, width - 16, 8, 4);
  rect (sn, accent, rtl ? pos : 8, h / 2 - 4, rtl ? width - 8 - pos : pos - 8, 8, 4);
  if (s->intervals->len)
    {
      for (guint n = 0; n < s->intervals->len; n++)
        {
          Interval *i = &g_array_index (s->intervals, Interval, n);
          double a = time_x (s, i->start + s->offset), b = time_x (s, i->end + s->offset);
          double left = MIN (a, b), right = MAX (a, b);
          gboolean point = i->start == i->end;
          if (right < 8 || left > width - 8)
            continue;
          left = MAX (left, 8);
          right = MIN (right, width - 8);
          gboolean active
              = s->position >= i->start + s->offset && s->position <= i->end + s->offset;
          rect (sn,
                active ? awra_token_set_get_accent_soft (t)
                       : awra_token_set_get_control_cap (t),
                left, 8, MAX (right - left, point ? 3 : 1), h - 16, radius / 2);
          if (point)
            rect (sn, accent, left, 8, 3, h - 16, 1);
          else if (i->label && right - left > 40)
            text_at (w, sn, i->label, fg, left + 6, h / 2 - 8, right - left - 12);
        }
    }
  if (s->hover_time >= 0)
    rect (sn, awra_token_set_get_muted_foreground (t), time_x (s, s->hover_time), 6, 1,
          h - 12, 0);
  rect (sn, fg, pos - 1, 4, 2, h - 8, 1);
  rect (sn, accent, pos - 4, h / 2 - 6, 8, 12, 4);
  if (gtk_widget_has_visible_focus (w) || awra_token_set_get_high_contrast (t))
    {
      graphene_rect_t bounds
          = GRAPHENE_RECT_INIT (1, 1, MAX (width - 2, 0), MAX (h - 2, 0));
      GskRoundedRect rr;
      float widths[4] = { 2, 2, 2, 2 };
      const GdkRGBA *c = awra_token_set_get_focus_ring (t);
      GdkRGBA colors[4] = { *c, *c, *c, *c };
      gsk_rounded_rect_init_from_rect (&rr, &bounds, radius);
      gtk_snapshot_append_border (sn, &rr, widths, colors);
    }
}
static void
update_preview (AwraTimeline *s, double x)
{
  s->hover_x = CLAMP (x, 0, MAX (0, gtk_widget_get_width (GTK_WIDGET (s))));
  s->hover_time = x_time (s, x);
  if (s->preview_content && s->duration > 0 && gtk_widget_get_mapped (GTK_WIDGET (s)))
    {
      GdkRectangle rect = { (int)s->hover_x, 0, 1, 1 };
      gtk_popover_set_pointing_to (GTK_POPOVER (s->preview), &rect);
      if (!gtk_widget_get_visible (GTK_WIDGET (s->preview)))
        gtk_popover_popup (GTK_POPOVER (s->preview));
      else
        gtk_popover_present (GTK_POPOVER (s->preview));
    }
  g_object_notify_by_pspec (G_OBJECT (s), props[P_HOVER_X]);
  g_object_notify_by_pspec (G_OBJECT (s), props[P_HOVER_TIME]);
  invalidate (s);
}
static void
hide_preview (AwraTimeline *s)
{
  if (s->preview)
    gtk_popover_popdown (GTK_POPOVER (s->preview));
  s->hover_x = s->hover_time = -1;
  g_object_notify_by_pspec (G_OBJECT (s), props[P_HOVER_X]);
  g_object_notify_by_pspec (G_OBJECT (s), props[P_HOVER_TIME]);
  invalidate (s);
}
static void
hover (GtkEventControllerMotion *c, double x, double y, AwraTimeline *s)
{
  (void)c;
  (void)y;
  s->inside = TRUE;
  update_preview (s, x);
}
static void
leave (GtkEventControllerMotion *c, AwraTimeline *s)
{
  (void)c;
  s->inside = FALSE;
  if (!s->dragging)
    hide_preview (s);
}
static void
begin (GtkGestureDrag *g, double x, double y, AwraTimeline *s)
{
  (void)y;
  if (s->duration <= 0)
    {
      gtk_gesture_set_state (GTK_GESTURE (g), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }
  gtk_widget_grab_focus (GTK_WIDGET (s));
  s->dragging = TRUE;
  s->drag_x = x;
  s->drag_offset = s->offset;
  s->drag_target = x_time (s, x);
  gtk_gesture_set_state (GTK_GESTURE (g), GTK_EVENT_SEQUENCE_CLAIMED);
  invalidate (s);
}
static void
update (GtkGestureDrag *g, double dx, double dy, AwraTimeline *s)
{
  (void)g;
  (void)dy;
  if (!s->dragging)
    return;
  if (s->editable)
    {
      double direction
          = gtk_widget_get_direction (GTK_WIDGET (s)) == GTK_TEXT_DIR_RTL ? -1 : 1;
      awra_timeline_set_offset (
          s, s->drag_offset
                 + direction * dx * span (s)
                       / MAX (gtk_widget_get_width (GTK_WIDGET (s)) - 16, 1));
    }
  else
    {
      s->drag_target = x_time (s, s->drag_x + dx);
      update_preview (s, s->drag_x + dx);
    }
  invalidate (s);
}
static void
end (GtkGestureDrag *g, double dx, double dy, AwraTimeline *s)
{
  if (!s->dragging)
    return;
  update (g, dx, dy, s);
  s->dragging = FALSE;
  if (s->editable)
    g_signal_emit (s, signals[COMMIT], 0, s->offset);
  else
    request (s, s->drag_target);
  if (!s->inside)
    hide_preview (s);
  invalidate (s);
}
static void
cancel (GtkGesture *g, GdkEventSequence *seq, AwraTimeline *s)
{
  (void)g;
  (void)seq;
  if (s->dragging && s->editable)
    awra_timeline_set_offset (s, s->drag_offset);
  s->dragging = FALSE;
  if (!s->inside)
    hide_preview (s);
  invalidate (s);
}
static gboolean
key (GtkEventControllerKey *c, guint k, guint code, GdkModifierType mods, AwraTimeline *s)
{
  double delta = (mods & GDK_SHIFT_MASK) ? s->page_increment : s->step_increment;
  double target = s->position;
  double direction
      = gtk_widget_get_direction (GTK_WIDGET (s)) == GTK_TEXT_DIR_RTL ? -1 : 1;
  (void)c;
  (void)code;
  if (s->duration <= 0 || !gtk_widget_is_sensitive (GTK_WIDGET (s)))
    return FALSE;
  if (k == GDK_KEY_Left)
    target -= delta * direction;
  else if (k == GDK_KEY_Right)
    target += delta * direction;
  else if (k == GDK_KEY_Home)
    target = 0;
  else if (k == GDK_KEY_End)
    target = s->duration;
  else
    return FALSE;
  if (s->editable && (k == GDK_KEY_Left || k == GDK_KEY_Right))
    {
      awra_timeline_set_offset (s, s->offset + (target - s->position) * 0.1);
      g_signal_emit (s, signals[COMMIT], 0, s->offset);
      return TRUE;
    }
  return request (s, target);
}
static void
dispose (GObject *o)
{
  AwraTimeline *s = AWRA_TIMELINE (o);
  if (s->style)
    g_clear_signal_handler (&s->style_handler, s->style);
  s->style = NULL;
  if (s->preview)
    {
      gtk_widget_unparent (GTK_WIDGET (s->preview));
      s->preview = NULL;
      s->preview_content = NULL;
    }
  G_OBJECT_CLASS (awra_timeline_parent_class)->dispose (o);
}
static void
interval_clear (gpointer data)
{
  g_free (((Interval *)data)->label);
}
static void
finalize (GObject *o)
{
  g_array_unref (AWRA_TIMELINE (o)->intervals);
  G_OBJECT_CLASS (awra_timeline_parent_class)->finalize (o);
}
static void
get_property (GObject *o, guint id, GValue *v, GParamSpec *p)
{
  AwraTimeline *s = AWRA_TIMELINE (o);
  switch (id)
    {
    case P_DURATION:
      g_value_set_double (v, s->duration);
      break;
    case P_POSITION:
      g_value_set_double (v, s->position);
      break;
    case P_STEP_INCREMENT:
      g_value_set_double (v, s->step_increment);
      break;
    case P_PAGE_INCREMENT:
      g_value_set_double (v, s->page_increment);
      break;
    case P_VIEW_START:
      g_value_set_double (v, s->view_start);
      break;
    case P_VIEW_DURATION:
      g_value_set_double (v, s->view_duration);
      break;
    case P_OFFSET:
      g_value_set_double (v, s->offset);
      break;
    case P_EDITABLE:
      g_value_set_boolean (v, s->editable);
      break;
    case P_HOVER_TIME:
      g_value_set_double (v, s->hover_time);
      break;
    case P_HOVER_X:
      g_value_set_double (v, s->hover_x);
      break;
    case P_PREVIEW:
      g_value_set_object (v, s->preview_content);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (o, id, p);
    }
}
static void
set_property (GObject *o, guint id, const GValue *v, GParamSpec *p)
{
  AwraTimeline *s = AWRA_TIMELINE (o);
  switch (id)
    {
    case P_DURATION:
      awra_timeline_set_duration (s, g_value_get_double (v));
      break;
    case P_POSITION:
      awra_timeline_set_position (s, g_value_get_double (v));
      break;
    case P_STEP_INCREMENT:
      awra_timeline_set_step_increment (s, g_value_get_double (v));
      break;
    case P_PAGE_INCREMENT:
      awra_timeline_set_page_increment (s, g_value_get_double (v));
      break;
    case P_VIEW_START:
      awra_timeline_set_view_start (s, g_value_get_double (v));
      break;
    case P_VIEW_DURATION:
      awra_timeline_set_view_duration (s, g_value_get_double (v));
      break;
    case P_OFFSET:
      awra_timeline_set_offset (s, g_value_get_double (v));
      break;
    case P_EDITABLE:
      awra_timeline_set_editable (s, g_value_get_boolean (v));
      break;
    case P_PREVIEW:
      awra_timeline_set_preview (s, g_value_get_object (v));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (o, id, p);
    }
}
static void
awra_timeline_class_init (AwraTimelineClass *klass)
{
  GObjectClass *o = G_OBJECT_CLASS (klass);
  GtkWidgetClass *w = GTK_WIDGET_CLASS (klass);
  GParamFlags f = G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS;
  o->dispose = dispose;
  o->finalize = finalize;
  o->get_property = get_property;
  o->set_property = set_property;
  w->map = map;
  w->unmap = unmap;
  w->measure = measure;
  w->snapshot = snapshot;
  props[P_DURATION]
      = g_param_spec_double ("duration", NULL, "Total seconds", 0, G_MAXDOUBLE, 0, f);
  props[P_POSITION]
      = g_param_spec_double ("position", NULL, "Playback seconds", 0, G_MAXDOUBLE, 0, f);
  props[P_STEP_INCREMENT] = g_param_spec_double (
      "step-increment", NULL, "Arrow-key step in seconds", 0, G_MAXDOUBLE, 1, f);
  props[P_PAGE_INCREMENT] = g_param_spec_double (
      "page-increment", NULL, "Shift+arrow step in seconds", 0, G_MAXDOUBLE, 10, f);
  props[P_VIEW_START]
      = g_param_spec_double ("view-start", NULL, "Viewport start", 0, G_MAXDOUBLE, 0, f);
  props[P_VIEW_DURATION] = g_param_spec_double ("view-duration", NULL,
                                                "Viewport seconds; zero fits duration", 0,
                                                G_MAXDOUBLE, 0, f);
  props[P_OFFSET]
      = g_param_spec_double ("offset", NULL, "Interval displacement in seconds",
                             -G_MAXDOUBLE, G_MAXDOUBLE, 0, f);
  props[P_EDITABLE] = g_param_spec_boolean (
      "editable", NULL, "Drag shifts intervals instead of seeking", FALSE, f);
  props[P_HOVER_TIME] = g_param_spec_double (
      "hover-time", NULL, "Hovered second, or -1 outside", -1, G_MAXDOUBLE, -1,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  props[P_HOVER_X] = g_param_spec_double (
      "hover-x", NULL, "Pointer coordinate in logical pixels; -1 outside", -1,
      G_MAXDOUBLE, -1,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  props[P_PREVIEW] = g_param_spec_object (
      "preview", NULL, "Backend-provided preview content", GTK_TYPE_WIDGET, f);
  g_object_class_install_properties (o, N_PROPS, props);
  /**
   * AwraTimeline::seek-requested:
   * @self: the timeline
   * @position: requested seconds
   *
   * Emitted after pointer, keyboard or assistive technology activation.
   * Applications own playback; position is never implicitly changed.
   */
  signals[SEEK]
      = g_signal_new ("seek-requested", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
                      NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_DOUBLE);
  /**
   * AwraTimeline::offset-committed:
   * @self: the timeline
   * @offset: committed interval displacement in seconds
   *
   * Emitted after an edit. Observe notify::offset for live preview.
   */
  signals[COMMIT]
      = g_signal_new ("offset-committed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
                      NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_DOUBLE);
  gtk_widget_class_set_css_name (w, "awratimeline");
  gtk_widget_class_set_accessible_role (w, GTK_ACCESSIBLE_ROLE_SLIDER);
}
static void
awra_timeline_init (AwraTimeline *s)
{
  s->step_increment = 1;
  s->page_increment = 10;
  s->intervals = g_array_new (FALSE, FALSE, sizeof (Interval));
  g_array_set_clear_func (s->intervals, interval_clear);
  s->hover_x = s->hover_time = -1;
  s->preview = AWRA_POPOVER (awra_popover_new ());
  gtk_popover_set_autohide (GTK_POPOVER (s->preview), FALSE);
  gtk_popover_set_has_arrow (GTK_POPOVER (s->preview), FALSE);
  gtk_popover_set_position (GTK_POPOVER (s->preview), GTK_POS_TOP);
  gtk_widget_set_can_focus (GTK_WIDGET (s->preview), FALSE);
  gtk_widget_set_can_target (GTK_WIDGET (s->preview), FALSE);
  gtk_widget_set_parent (GTK_WIDGET (s->preview), GTK_WIDGET (s));
  gtk_widget_set_focusable (GTK_WIDGET (s), TRUE);
  gtk_widget_set_cursor_from_name (GTK_WIDGET (s), "pointer");
  GtkGesture *g = gtk_gesture_drag_new ();
  g_signal_connect (g, "drag-begin", G_CALLBACK (begin), s);
  g_signal_connect (g, "drag-update", G_CALLBACK (update), s);
  g_signal_connect (g, "drag-end", G_CALLBACK (end), s);
  g_signal_connect (g, "cancel", G_CALLBACK (cancel), s);
  gtk_widget_add_controller (GTK_WIDGET (s), GTK_EVENT_CONTROLLER (g));
  GtkEventController *m = gtk_event_controller_motion_new ();
  g_signal_connect (m, "motion", G_CALLBACK (hover), s);
  g_signal_connect (m, "leave", G_CALLBACK (leave), s);
  gtk_widget_add_controller (GTK_WIDGET (s), m);
  GtkEventController *k = gtk_event_controller_key_new ();
  g_signal_connect (k, "key-pressed", G_CALLBACK (key), s);
  gtk_widget_add_controller (GTK_WIDGET (s), k);
  gtk_accessible_update_property (GTK_ACCESSIBLE (s), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  _ ("Timeline"), -1);
  accessible (s);
}
GtkWidget *
awra_timeline_new (void)
{
  return g_object_new (AWRA_TYPE_TIMELINE, NULL);
}
#define DOUBLE_ACCESSOR(name, field, prop, normalize)                                    \
  double awra_timeline_get_##name (AwraTimeline *s)                                      \
  {                                                                                      \
    g_return_val_if_fail (AWRA_IS_TIMELINE (s), 0);                                      \
    return s->field;                                                                     \
  }                                                                                      \
  void awra_timeline_set_##name (AwraTimeline *s, double value)                          \
  {                                                                                      \
    g_return_if_fail (AWRA_IS_TIMELINE (s));                                             \
    if (!isfinite (value))                                                               \
      value = 0;                                                                         \
    normalize;                                                                           \
    if (s->field == value)                                                               \
      {                                                                                  \
        return;                                                                          \
      }                                                                                  \
    s->field = value;                                                                    \
    accessible (s);                                                                      \
    invalidate (s);                                                                      \
    g_object_notify_by_pspec (G_OBJECT (s), props[prop]);                                \
  }
double
awra_timeline_get_duration (AwraTimeline *s)
{
  g_return_val_if_fail (AWRA_IS_TIMELINE (s), 0);
  return s->duration;
}
void
awra_timeline_set_duration (AwraTimeline *s, double value)
{
  g_return_if_fail (AWRA_IS_TIMELINE (s));
  value = isfinite (value) ? MAX (value, 0) : 0;
  if (s->duration == value)
    return;
  s->duration = value;
  if (value == 0)
    hide_preview (s);
  if (s->position > value)
    awra_timeline_set_position (s, value);
  accessible (s);
  invalidate (s);
  g_object_notify_by_pspec (G_OBJECT (s), props[P_DURATION]);
}
DOUBLE_ACCESSOR (position, position, P_POSITION, value = CLAMP (value, 0, s->duration))
DOUBLE_ACCESSOR (step_increment, step_increment, P_STEP_INCREMENT,
                 value = value > 0 ? value : 1)
DOUBLE_ACCESSOR (page_increment, page_increment, P_PAGE_INCREMENT,
                 value = value > 0 ? value : 10)
DOUBLE_ACCESSOR (view_start, view_start, P_VIEW_START, value = MAX (value, 0))
DOUBLE_ACCESSOR (view_duration, view_duration, P_VIEW_DURATION, value = MAX (value, 0))
DOUBLE_ACCESSOR (offset, offset, P_OFFSET, (void)0)
#undef DOUBLE_ACCESSOR

gboolean
awra_timeline_get_editable (AwraTimeline *s)
{
  g_return_val_if_fail (AWRA_IS_TIMELINE (s), FALSE);
  return s->editable;
}
void
awra_timeline_set_editable (AwraTimeline *s, gboolean e)
{
  g_return_if_fail (AWRA_IS_TIMELINE (s));
  e = !!e;
  if (s->editable == e)
    return;
  s->editable = e;
  accessible (s);
  gtk_widget_set_cursor_from_name (GTK_WIDGET (s), e ? "ew-resize" : "pointer");
  g_object_notify_by_pspec (G_OBJECT (s), props[P_EDITABLE]);
}
double
awra_timeline_get_hover_time (AwraTimeline *s)
{
  g_return_val_if_fail (AWRA_IS_TIMELINE (s), -1);
  return s->hover_time;
}
void
awra_timeline_add_interval (AwraTimeline *s, double start, double finish,
                            const char *label)
{
  g_return_if_fail (AWRA_IS_TIMELINE (s));
  if (!isfinite (start) || !isfinite (finish) || finish < start)
    return;
  Interval i = { start, finish, g_strdup (label) };
  g_array_append_val (s->intervals, i);
  invalidate (s);
}
void
awra_timeline_clear_intervals (AwraTimeline *s)
{
  g_return_if_fail (AWRA_IS_TIMELINE (s));
  g_array_set_size (s->intervals, 0);
  invalidate (s);
}
guint
awra_timeline_get_n_intervals (AwraTimeline *s)
{
  g_return_val_if_fail (AWRA_IS_TIMELINE (s), 0);
  return s->intervals->len;
}

double
awra_timeline_get_hover_x (AwraTimeline *s)
{
  g_return_val_if_fail (AWRA_IS_TIMELINE (s), -1);
  return s->hover_x;
}
GtkWidget *
awra_timeline_get_preview (AwraTimeline *s)
{
  g_return_val_if_fail (AWRA_IS_TIMELINE (s), NULL);
  return s->preview_content;
}
void
awra_timeline_set_preview (AwraTimeline *s, GtkWidget *content)
{
  g_return_if_fail (AWRA_IS_TIMELINE (s));
  g_return_if_fail (content == NULL || GTK_IS_WIDGET (content));
  if (s->preview_content == content)
    return;
  awra_popover_set_child (s->preview, content);
  s->preview_content = content;
  if (!content)
    gtk_popover_popdown (GTK_POPOVER (s->preview));
  g_object_notify_by_pspec (G_OBJECT (s), props[P_PREVIEW]);
}
AwraPopover *
awra_timeline_get_preview_popover (AwraTimeline *s)
{
  g_return_val_if_fail (AWRA_IS_TIMELINE (s), NULL);
  return s->preview;
}
