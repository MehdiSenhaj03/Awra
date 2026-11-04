/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <math.h>

#include <awra/awra-emoji-picker.h>

#include <awra/awra-button.h>
#include <awra/awra-search-entry.h>
#include <awra/awra-typography.h>

#include "awra-i18n-private.h"

/* Calibration, not ABI. A cell is 36 px plus a 2 px margin on each side, and
 * the grid is inset by 6 px on each side (see awra.css). GtkGridView cannot
 * show section headers, so the grid is a list of rows of cells, rebuilt when
 * the number of columns changes. */
#define CELL 40
#define GRID_INSET 12
#define MIN_COLUMNS 5
#define NATURAL_COLUMNS 9
#define MAX_COLUMNS 16
#define NATURAL_ROWS 7
#define MIN_ROWS 3
#define FALLBACK_HEADER_HEIGHT 28

/* A row of the grid: up to one row's worth of items of one section. */

#define AWRA_TYPE_EMOJI_ROW (awra_emoji_row_get_type ())
G_DECLARE_FINAL_TYPE (AwraEmojiRow, awra_emoji_row, AWRA, EMOJI_ROW, GObject)

struct _AwraEmojiRow {
  GObject parent_instance;
  char *title;
  GPtrArray *items;
};

G_DEFINE_FINAL_TYPE (AwraEmojiRow, awra_emoji_row, G_TYPE_OBJECT)

static void
awra_emoji_row_finalize (GObject *object)
{
  AwraEmojiRow *self = AWRA_EMOJI_ROW (object);

  g_free (self->title);
  g_ptr_array_unref (self->items);
  G_OBJECT_CLASS (awra_emoji_row_parent_class)->finalize (object);
}

static void
awra_emoji_row_class_init (AwraEmojiRowClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = awra_emoji_row_finalize;
}

static void
awra_emoji_row_init (AwraEmojiRow *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
}

typedef struct {
  char *id;
  char *title;
  char *icon_name;
  GListModel *items;
  gulong changed_handler;
  GPtrArray *visible;
  guint first_row;
  guint n_rows;
  GtkWidget *tab;
  gboolean recent;
} Section;

/* The rows, sectioned: the model the list shows. */

#define AWRA_TYPE_EMOJI_ROWS (awra_emoji_rows_get_type ())
G_DECLARE_FINAL_TYPE (AwraEmojiRows, awra_emoji_rows, AWRA, EMOJI_ROWS, GObject)

struct _AwraEmojiRows {
  GObject parent_instance;
  GPtrArray *sections;
  guint columns;
  guint n_rows;
};

static GType
awra_emoji_rows_get_item_type (GListModel *model)
{
  (void) model;
  return AWRA_TYPE_EMOJI_ROW;
}

static guint
awra_emoji_rows_get_n_items (GListModel *model)
{
  return AWRA_EMOJI_ROWS (model)->n_rows;
}

static Section *
section_for_row (AwraEmojiRows *self,
                 guint          row)
{
  if (self->sections == NULL)
    return NULL;
  for (guint i = 0; i < self->sections->len; i++) {
    Section *section = g_ptr_array_index (self->sections, i);

    if (row >= section->first_row && row < section->first_row + section->n_rows)
      return section;
  }
  return NULL;
}

static gpointer
awra_emoji_rows_get_item (GListModel *model,
                          guint       position)
{
  AwraEmojiRows *self = AWRA_EMOJI_ROWS (model);
  Section *section = section_for_row (self, position);
  AwraEmojiRow *row;
  guint start;
  guint end;

  if (section == NULL)
    return NULL;
  row = g_object_new (AWRA_TYPE_EMOJI_ROW, NULL);
  row->title = g_strdup (section->title);
  start = (position - section->first_row) * self->columns;
  end = MIN (start + self->columns, section->visible->len);
  for (guint i = start; i < end; i++)
    g_ptr_array_add (row->items, g_object_ref (g_ptr_array_index (section->visible, i)));
  return row;
}

static void
awra_emoji_rows_list_init (GListModelInterface *iface)
{
  iface->get_item_type = awra_emoji_rows_get_item_type;
  iface->get_n_items = awra_emoji_rows_get_n_items;
  iface->get_item = awra_emoji_rows_get_item;
}

static void
awra_emoji_rows_get_section (GtkSectionModel *model,
                             guint            position,
                             guint           *out_start,
                             guint           *out_end)
{
  AwraEmojiRows *self = AWRA_EMOJI_ROWS (model);
  Section *section = section_for_row (self, position);

  if (section == NULL) {
    *out_start = self->n_rows;
    *out_end = G_MAXUINT;
    return;
  }
  *out_start = section->first_row;
  *out_end = section->first_row + section->n_rows;
}

static void
awra_emoji_rows_section_init (GtkSectionModelInterface *iface)
{
  iface->get_section = awra_emoji_rows_get_section;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (AwraEmojiRows, awra_emoji_rows, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, awra_emoji_rows_list_init)
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_SECTION_MODEL,
                                                      awra_emoji_rows_section_init))

static void
awra_emoji_rows_class_init (AwraEmojiRowsClass *klass)
{
  (void) klass;
}

static void
awra_emoji_rows_init (AwraEmojiRows *self)
{
  self->columns = NATURAL_COLUMNS;
}

/* The picker. */

struct _AwraEmojiPicker {
  GtkWidget parent_instance;

  GtkWidget *header;
  AwraSearchEntry *search;
  GtkWidget *tabs_scroller;
  GtkWidget *tabs;
  GtkWidget *grid_area;
  GtkScrolledWindow *scroller;
  GtkListView *grid;
  GtkWidget *empty;
  GtkWidget *footer;
  GtkLabel *preview_glyph;
  GtkPicture *preview_picture;
  GtkLabel *preview_name;

  AwraEmojiRows *rows;
  GPtrArray *sections;
  GPtrArray *bound;
  char *query;
  AwraEmojiItem *cursor;
  AwraEmojiItem *hovered;
  guint current_section;
  gboolean scrolling_to_section;
  /* The list anchors its first row at the top, which hides the title of the
   * first section: at the top, the view returns to 0 after the next layout. */
  gboolean show_top;
};

enum {
  SIGNAL_PICKED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (AwraEmojiPicker, awra_emoji_picker, GTK_TYPE_WIDGET)

static void refilter (AwraEmojiPicker *self);

static void
section_free (gpointer data)
{
  Section *section = data;

  if (section->items != NULL)
    g_clear_signal_handler (&section->changed_handler, section->items);
  g_clear_object (&section->items);
  g_ptr_array_unref (section->visible);
  g_free (section->id);
  g_free (section->title);
  g_free (section->icon_name);
  g_free (section);
}

static Section *
section_new (const char *id,
             const char *title,
             const char *icon_name)
{
  Section *section = g_new0 (Section, 1);

  section->id = g_strdup (id);
  section->title = g_strdup (title);
  section->icon_name = g_strdup (icon_name);
  section->visible = g_ptr_array_new_with_free_func (g_object_unref);
  return section;
}

static void
section_items_changed_cb (GListModel      *model,
                          guint            position,
                          guint            removed,
                          guint            added,
                          AwraEmojiPicker *self)
{
  (void) model;
  (void) position;
  (void) removed;
  (void) added;
  refilter (self);
}

static void
section_set_items (AwraEmojiPicker *self,
                   Section         *section,
                   GListModel      *items)
{
  if (section->items != NULL)
    g_clear_signal_handler (&section->changed_handler, section->items);
  g_set_object (&section->items, items);
  if (items != NULL)
    section->changed_handler = g_signal_connect (items, "items-changed",
                                                 G_CALLBACK (section_items_changed_cb), self);
}

/* Geometry of the list, for scrolling to a section and following one. */

static double
row_height (AwraEmojiPicker *self)
{
  for (guint i = 0; i < self->bound->len; i++) {
    GtkWidget *child = gtk_list_item_get_child (g_ptr_array_index (self->bound, i));
    GtkWidget *row = child != NULL ? gtk_widget_get_parent (child) : NULL;

    if (row != NULL && gtk_widget_get_height (row) > 0)
      return gtk_widget_get_height (row);
  }
  return CELL;
}

static double
header_height (AwraEmojiPicker *self)
{
  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->grid));
       child != NULL; child = gtk_widget_get_next_sibling (child)) {
    if (g_str_equal (gtk_widget_get_css_name (child), "header") &&
        gtk_widget_get_height (child) > 0)
      return gtk_widget_get_height (child);
  }
  return FALLBACK_HEADER_HEIGHT;
}

static double
section_offset (AwraEmojiPicker *self,
                guint            index)
{
  double rows = row_height (self);
  double header = header_height (self);
  double offset = 0;

  for (guint i = 0; i < index && i < self->sections->len; i++) {
    Section *section = g_ptr_array_index (self->sections, i);

    if (section->n_rows > 0)
      offset += header + section->n_rows * rows;
  }
  return offset;
}

static void
set_current_section (AwraEmojiPicker *self,
                     guint            index)
{
  if (self->current_section == index)
    return;
  if (self->current_section < self->sections->len) {
    Section *old = g_ptr_array_index (self->sections, self->current_section);

    gtk_widget_remove_css_class (old->tab, "awra-emoji-tab-current");
  }
  self->current_section = index;
  if (index < self->sections->len) {
    Section *section = g_ptr_array_index (self->sections, index);

    gtk_widget_add_css_class (section->tab, "awra-emoji-tab-current");
  }
}

static void
scrolled_cb (GtkAdjustment   *adjustment,
             AwraEmojiPicker *self)
{
  double value = gtk_adjustment_get_value (adjustment) + 1.0;
  guint current = G_MAXUINT;

  if (self->scrolling_to_section)
    return;
  for (guint i = 0; i < self->sections->len; i++) {
    Section *section = g_ptr_array_index (self->sections, i);

    if (section->n_rows == 0)
      continue;
    if (current == G_MAXUINT || section_offset (self, i) <= value)
      current = i;
    else
      break;
  }
  set_current_section (self, current);
}

static void
tab_clicked_cb (GtkButton       *button,
                AwraEmojiPicker *self)
{
  GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment (self->scroller);

  for (guint i = 0; i < self->sections->len; i++) {
    Section *section = g_ptr_array_index (self->sections, i);

    if (section->tab != GTK_WIDGET (button) || section->n_rows == 0)
      continue;
    self->scrolling_to_section = TRUE;
    gtk_adjustment_set_value (adjustment, section_offset (self, i));
    self->scrolling_to_section = FALSE;
    set_current_section (self, i);
    break;
  }
}

static void
update_tab (Section *section)
{
  GtkButton *button = GTK_BUTTON (section->tab);

  if (section->icon_name != NULL) {
    gtk_button_set_icon_name (button, section->icon_name);
  } else {
    AwraEmojiItem *first = section->items != NULL && g_list_model_get_n_items (section->items) > 0
                             ? g_list_model_get_item (section->items, 0) : NULL;

    if (first != NULL && awra_emoji_item_get_paintable (first) != NULL) {
      GtkWidget *picture = gtk_picture_new_for_paintable (awra_emoji_item_get_paintable (first));

      gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_CONTAIN);
      gtk_button_set_child (button, picture);
    } else {
      gtk_button_set_label (button, first != NULL && awra_emoji_item_get_text (first) != NULL
                                      ? awra_emoji_item_get_text (first) : section->title);
    }
    g_clear_object (&first);
  }
  gtk_widget_set_tooltip_text (section->tab, section->title);
  gtk_accessible_update_property (GTK_ACCESSIBLE (section->tab),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, section->title, -1);
}

static GtkWidget *
make_tab (AwraEmojiPicker *self)
{
  GtkWidget *tab = awra_button_new ();

  awra_button_set_appearance (AWRA_BUTTON (tab), AWRA_BUTTON_APPEARANCE_GHOST);
  gtk_widget_add_css_class (tab, "awra-emoji-tab");
  gtk_widget_set_focus_on_click (tab, FALSE);
  g_signal_connect (tab, "clicked", G_CALLBACK (tab_clicked_cb), self);
  return tab;
}

/* Cursor and preview. */

static void
update_preview (AwraEmojiPicker *self)
{
  AwraEmojiItem *item = self->hovered != NULL ? self->hovered : self->cursor;
  GdkPaintable *paintable = item != NULL ? awra_emoji_item_get_paintable (item) : NULL;
  g_autofree char *name = NULL;

  if (item == NULL) {
    gtk_widget_set_visible (GTK_WIDGET (self->preview_glyph), FALSE);
    gtk_widget_set_visible (GTK_WIDGET (self->preview_picture), FALSE);
    gtk_label_set_text (self->preview_name, "");
    return;
  }
  gtk_widget_set_visible (GTK_WIDGET (self->preview_picture), paintable != NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->preview_glyph), paintable == NULL);
  gtk_picture_set_paintable (self->preview_picture, paintable);
  gtk_label_set_text (self->preview_glyph, awra_emoji_item_get_text (item) != NULL
                                             ? awra_emoji_item_get_text (item) : "");
  name = g_strdup_printf (":%s:", awra_emoji_item_get_name (item));
  gtk_label_set_text (self->preview_name, name);
}

static void
mark_cells (AwraEmojiPicker *self)
{
  for (guint i = 0; i < self->bound->len; i++) {
    GtkWidget *row = gtk_list_item_get_child (g_ptr_array_index (self->bound, i));

    for (GtkWidget *cell = gtk_widget_get_first_child (row); cell != NULL;
         cell = gtk_widget_get_next_sibling (cell)) {
      if (gtk_widget_get_visible (cell) &&
          g_object_get_data (G_OBJECT (cell), "awra-emoji-item") == self->cursor)
        gtk_widget_add_css_class (cell, "awra-emoji-cursor");
      else
        gtk_widget_remove_css_class (cell, "awra-emoji-cursor");
    }
  }
}

static gboolean
locate (AwraEmojiPicker *self,
        AwraEmojiItem   *item,
        guint           *section_index,
        guint           *index)
{
  if (item == NULL)
    return FALSE;
  for (guint i = 0; i < self->sections->len; i++) {
    Section *section = g_ptr_array_index (self->sections, i);

    if (g_ptr_array_find (section->visible, item, index)) {
      *section_index = i;
      return TRUE;
    }
  }
  return FALSE;
}

static void
set_cursor (AwraEmojiPicker *self,
            AwraEmojiItem   *item,
            gboolean         reveal)
{
  guint section_index;
  guint index;

  g_set_object (&self->cursor, item);
  mark_cells (self);
  update_preview (self);
  if (reveal && locate (self, item, &section_index, &index)) {
    Section *section = g_ptr_array_index (self->sections, section_index);

    gtk_list_view_scroll_to (self->grid, section->first_row + index / self->rows->columns,
                             GTK_LIST_SCROLL_NONE, NULL);
  }
}

static Section *
neighbour_section (AwraEmojiPicker *self,
                   guint            from,
                   int              direction,
                   guint           *found)
{
  for (int i = (int) from + direction; i >= 0 && i < (int) self->sections->len; i += direction) {
    Section *section = g_ptr_array_index (self->sections, i);

    if (section->visible->len > 0) {
      *found = (guint) i;
      return section;
    }
  }
  return NULL;
}

static void
move_cursor (AwraEmojiPicker *self,
             guint            keyval)
{
  guint columns = self->rows->columns;
  guint section_index;
  guint index;
  guint other_index;
  Section *section;
  Section *other;
  guint column;

  if (!locate (self, self->cursor, &section_index, &index)) {
    other = neighbour_section (self, G_MAXUINT, 1, &other_index);
    if (other == NULL)
      other = neighbour_section (self, (guint) -1, 1, &other_index);
    if (other != NULL)
      set_cursor (self, g_ptr_array_index (other->visible, 0), TRUE);
    return;
  }
  section = g_ptr_array_index (self->sections, section_index);
  column = index % columns;
  switch (keyval) {
  case GDK_KEY_Right:
  case GDK_KEY_KP_Right:
    if (index + 1 < section->visible->len)
      index++;
    else if ((other = neighbour_section (self, section_index, 1, &other_index)) != NULL)
      section = other, index = 0;
    break;
  case GDK_KEY_Left:
  case GDK_KEY_KP_Left:
    if (index > 0)
      index--;
    else if ((other = neighbour_section (self, section_index, -1, &other_index)) != NULL)
      section = other, index = other->visible->len - 1;
    break;
  case GDK_KEY_Down:
  case GDK_KEY_KP_Down:
    if (index + columns < section->visible->len)
      index += columns;
    else if (index / columns + 1 < section->n_rows)
      index = section->visible->len - 1;
    else if ((other = neighbour_section (self, section_index, 1, &other_index)) != NULL)
      section = other, index = MIN (column, other->visible->len - 1);
    break;
  case GDK_KEY_Up:
  case GDK_KEY_KP_Up:
    if (index >= columns)
      index -= columns;
    else if ((other = neighbour_section (self, section_index, -1, &other_index)) != NULL)
      section = other,
      index = MIN ((other->n_rows - 1) * columns + column, other->visible->len - 1);
    break;
  default:
    return;
  }
  set_cursor (self, g_ptr_array_index (section->visible, index), TRUE);
}

static void
pick (AwraEmojiPicker *self,
      AwraEmojiItem   *item)
{
  if (item == NULL)
    return;
  g_object_ref (item);
  g_signal_emit (self, signals[SIGNAL_PICKED], 0, item);
  g_object_unref (item);
}

static gboolean
key_pressed_cb (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                AwraEmojiPicker       *self)
{
  gboolean searching = self->query != NULL && *self->query != '\0';

  (void) controller;
  (void) keycode;
  if ((state & gtk_accelerator_get_default_mod_mask ()) != 0)
    return FALSE;
  switch (keyval) {
  case GDK_KEY_Left:
  case GDK_KEY_KP_Left:
  case GDK_KEY_Right:
  case GDK_KEY_KP_Right:
    /* While a search is typed, the sideways arrows edit it. */
    if (searching && gtk_widget_has_focus (GTK_WIDGET (awra_search_entry_get_delegate (self->search))))
      return FALSE;
    G_GNUC_FALLTHROUGH;
  case GDK_KEY_Up:
  case GDK_KEY_KP_Up:
  case GDK_KEY_Down:
  case GDK_KEY_KP_Down:
    move_cursor (self, keyval);
    return TRUE;
  case GDK_KEY_Return:
  case GDK_KEY_KP_Enter:
  case GDK_KEY_ISO_Enter:
    if (self->cursor == NULL)
      return FALSE;
    pick (self, self->cursor);
    return TRUE;
  default:
    return FALSE;
  }
}

/* Cells. */

static void
cell_enter_cb (GtkEventControllerMotion *motion,
               double                    x,
               double                    y,
               AwraEmojiPicker          *self)
{
  GtkWidget *cell = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (motion));

  (void) x;
  (void) y;
  g_set_object (&self->hovered, g_object_get_data (G_OBJECT (cell), "awra-emoji-item"));
  update_preview (self);
}

static void
cell_leave_cb (GtkEventControllerMotion *motion,
               AwraEmojiPicker          *self)
{
  GtkWidget *cell = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (motion));

  if (self->hovered == g_object_get_data (G_OBJECT (cell), "awra-emoji-item")) {
    g_clear_object (&self->hovered);
    update_preview (self);
  }
}

static void
cell_pressed_cb (GtkGestureClick *gesture,
                 int              n_press,
                 double           x,
                 double           y,
                 AwraEmojiPicker *self)
{
  GtkWidget *cell = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  (void) n_press;
  (void) x;
  (void) y;
  (void) self;
  gtk_widget_set_state_flags (cell, GTK_STATE_FLAG_ACTIVE, FALSE);
}

static void
cell_released_cb (GtkGestureClick *gesture,
                  int              n_press,
                  double           x,
                  double           y,
                  AwraEmojiPicker *self)
{
  GtkWidget *cell = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  AwraEmojiItem *item = g_object_get_data (G_OBJECT (cell), "awra-emoji-item");

  (void) n_press;
  gtk_widget_unset_state_flags (cell, GTK_STATE_FLAG_ACTIVE);
  if (item == NULL || !gtk_widget_contains (cell, x, y))
    return;
  set_cursor (self, item, FALSE);
  pick (self, item);
}

static void
cell_cancel_cb (GtkGesture       *gesture,
                GdkEventSequence *sequence,
                gpointer          user_data)
{
  (void) sequence;
  (void) user_data;
  gtk_widget_unset_state_flags (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)),
                                GTK_STATE_FLAG_ACTIVE);
}

static void
row_setup_cb (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item,
              AwraEmojiPicker          *self)
{
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  (void) factory;
  gtk_widget_add_css_class (row, "awra-emoji-row");
  for (guint i = 0; i < MAX_COLUMNS; i++) {
    GtkWidget *cell = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *glyph = gtk_label_new (NULL);
    GtkWidget *picture = gtk_picture_new ();
    GtkEventController *motion = gtk_event_controller_motion_new ();
    GtkGesture *click = gtk_gesture_click_new ();

    gtk_widget_add_css_class (cell, "awra-emoji-cell");
    gtk_accessible_update_property (GTK_ACCESSIBLE (cell), GTK_ACCESSIBLE_PROPERTY_LABEL, "", -1);
    /* Cells keep their size: a short last row stays on the grid. */
    gtk_widget_set_hexpand (cell, FALSE);
    gtk_widget_add_css_class (glyph, "awra-emoji-glyph");
    gtk_widget_set_hexpand (glyph, TRUE);
    gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_hexpand (picture, TRUE);
    gtk_widget_add_css_class (picture, "awra-emoji-image");
    gtk_box_append (GTK_BOX (cell), glyph);
    gtk_box_append (GTK_BOX (cell), picture);
    g_signal_connect (motion, "enter", G_CALLBACK (cell_enter_cb), self);
    g_signal_connect (motion, "leave", G_CALLBACK (cell_leave_cb), self);
    gtk_widget_add_controller (cell, motion);
    g_signal_connect (click, "pressed", G_CALLBACK (cell_pressed_cb), self);
    g_signal_connect (click, "released", G_CALLBACK (cell_released_cb), self);
    g_signal_connect (click, "cancel", G_CALLBACK (cell_cancel_cb), NULL);
    gtk_widget_add_controller (cell, GTK_EVENT_CONTROLLER (click));
    gtk_box_append (GTK_BOX (row), cell);
  }
  gtk_list_item_set_child (list_item, row);
  gtk_list_item_set_activatable (list_item, FALSE);
  gtk_list_item_set_focusable (list_item, FALSE);
}

static void
row_bind_cb (GtkSignalListItemFactory *factory,
             GtkListItem              *list_item,
             AwraEmojiPicker          *self)
{
  AwraEmojiRow *row = gtk_list_item_get_item (list_item);
  GtkWidget *cell = gtk_widget_get_first_child (gtk_list_item_get_child (list_item));

  (void) factory;
  for (guint i = 0; cell != NULL; i++, cell = gtk_widget_get_next_sibling (cell)) {
    AwraEmojiItem *item = i < row->items->len ? g_ptr_array_index (row->items, i) : NULL;
    GtkWidget *glyph = gtk_widget_get_first_child (cell);
    GtkWidget *picture = gtk_widget_get_last_child (cell);
    GdkPaintable *paintable = item != NULL ? awra_emoji_item_get_paintable (item) : NULL;

    gtk_widget_set_visible (cell, item != NULL);
    g_object_set_data (G_OBJECT (cell), "awra-emoji-item", item);
    if (item == NULL)
      continue;
    gtk_widget_set_visible (picture, paintable != NULL);
    gtk_widget_set_visible (glyph, paintable == NULL);
    gtk_picture_set_paintable (GTK_PICTURE (picture), paintable);
    gtk_label_set_text (GTK_LABEL (glyph), awra_emoji_item_get_text (item) != NULL
                                             ? awra_emoji_item_get_text (item) : "");
    gtk_accessible_update_property (GTK_ACCESSIBLE (cell), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                    awra_emoji_item_get_name (item), -1);
    gtk_widget_set_tooltip_text (cell, NULL);
    if (item == self->cursor)
      gtk_widget_add_css_class (cell, "awra-emoji-cursor");
    else
      gtk_widget_remove_css_class (cell, "awra-emoji-cursor");
  }
  g_ptr_array_add (self->bound, list_item);
}

static void
row_unbind_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item,
               AwraEmojiPicker          *self)
{
  GtkWidget *cell = gtk_widget_get_first_child (gtk_list_item_get_child (list_item));

  (void) factory;
  g_ptr_array_remove_fast (self->bound, list_item);
  for (; cell != NULL; cell = gtk_widget_get_next_sibling (cell)) {
    if (self->hovered != NULL && g_object_get_data (G_OBJECT (cell), "awra-emoji-item") == self->hovered)
      g_clear_object (&self->hovered);
    g_object_set_data (G_OBJECT (cell), "awra-emoji-item", NULL);
    gtk_picture_set_paintable (GTK_PICTURE (gtk_widget_get_last_child (cell)), NULL);
  }
}

static void
header_setup_cb (GtkSignalListItemFactory *factory,
                 GtkListHeader            *header,
                 gpointer                  user_data)
{
  GtkWidget *label = gtk_label_new (NULL);

  (void) factory;
  (void) user_data;
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_widget_add_css_class (label, "awra-emoji-section-title");
  awra_widget_set_typography (label, AWRA_TYPOGRAPHY_EYEBROW);
  gtk_list_header_set_child (header, label);
}

static void
header_bind_cb (GtkSignalListItemFactory *factory,
                GtkListHeader            *header,
                gpointer                  user_data)
{
  AwraEmojiRow *row = gtk_list_header_get_item (header);

  (void) factory;
  (void) user_data;
  gtk_label_set_text (GTK_LABEL (gtk_list_header_get_child (header)),
                      row != NULL ? row->title : "");
}

/* Filtering and rows. */

static void
relayout (AwraEmojiPicker *self)
{
  guint old = self->rows->n_rows;
  guint rows = 0;
  guint total = 0;

  for (guint i = 0; i < self->sections->len; i++) {
    Section *section = g_ptr_array_index (self->sections, i);

    section->first_row = rows;
    section->n_rows = (section->visible->len + self->rows->columns - 1) / self->rows->columns;
    rows += section->n_rows;
    total += section->visible->len;
    gtk_widget_set_visible (section->tab, section->visible->len > 0);
  }
  self->rows->n_rows = rows;
  gtk_widget_set_visible (self->empty, total == 0 && self->sections->len > 0 &&
                                         self->query != NULL && *self->query != '\0');
  g_list_model_items_changed (G_LIST_MODEL (self->rows), 0, old, rows);
}

static void
refilter (AwraEmojiPicker *self)
{
  const char *query = self->query != NULL ? self->query : "";
  guint section_index;
  guint index;

  if (gtk_adjustment_get_value (gtk_scrolled_window_get_vadjustment (self->scroller)) <=
      header_height (self) + 1.0)
    self->show_top = TRUE;
  for (guint i = 0; i < self->sections->len; i++) {
    Section *section = g_ptr_array_index (self->sections, i);
    guint n = section->items != NULL ? g_list_model_get_n_items (section->items) : 0;

    g_ptr_array_set_size (section->visible, 0);
    /* A search lists each match once: recent items are found in their
     * own section. */
    if (section->recent && *query != '\0')
      continue;
    for (guint j = 0; j < n; j++) {
      g_autoptr (AwraEmojiItem) item = g_list_model_get_item (section->items, j);

      if (AWRA_IS_EMOJI_ITEM (item) && awra_emoji_item_matches (item, query))
        g_ptr_array_add (section->visible, g_steal_pointer (&item));
    }
  }
  relayout (self);
  if (!locate (self, self->cursor, &section_index, &index)) {
    AwraEmojiItem *first = NULL;

    for (guint i = 0; i < self->sections->len && first == NULL; i++) {
      Section *section = g_ptr_array_index (self->sections, i);

      if (section->visible->len > 0)
        first = g_ptr_array_index (section->visible, 0);
    }
    set_cursor (self, first, FALSE);
  } else {
    update_preview (self);
  }
}

static void
search_changed_cb (GtkSearchEntry  *entry,
                   AwraEmojiPicker *self)
{
  const char *text = gtk_editable_get_text (GTK_EDITABLE (entry));

  if (g_strcmp0 (self->query != NULL ? self->query : "", text) == 0)
    return;
  g_free (self->query);
  self->query = g_strdup (text);
  refilter (self);
  /* The first matches, with their section title. */
  self->show_top = TRUE;
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
search_activate_cb (GtkSearchEntry  *entry,
                    AwraEmojiPicker *self)
{
  (void) entry;
  pick (self, self->cursor);
}

/* Widget. */

static guint
columns_for (int width)
{
  return CLAMP ((width - GRID_INSET) / CELL, MIN_COLUMNS, MAX_COLUMNS);
}

static void
awra_emoji_picker_measure (GtkWidget      *widget,
                           GtkOrientation  orientation,
                           int             for_size,
                           int            *minimum,
                           int            *natural,
                           int            *minimum_baseline,
                           int            *natural_baseline)
{
  AwraEmojiPicker *self = AWRA_EMOJI_PICKER (widget);
  GtkWidget *parts[] = { self->header, self->footer };

  (void) minimum_baseline;
  (void) natural_baseline;
  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    *minimum = MIN_COLUMNS * CELL + GRID_INSET;
    *natural = NATURAL_COLUMNS * CELL + GRID_INSET;
    for (guint i = 0; i < G_N_ELEMENTS (parts); i++) {
      int part_min = 0;

      gtk_widget_measure (parts[i], orientation, -1, &part_min, NULL, NULL, NULL);
      *minimum = MAX (*minimum, part_min);
    }
    *natural = MAX (*natural, *minimum);
    return;
  }
  *minimum = MIN_ROWS * CELL;
  *natural = NATURAL_ROWS * CELL;
  for (guint i = 0; i < G_N_ELEMENTS (parts); i++) {
    int part_min = 0;
    int part_nat = 0;

    gtk_widget_measure (parts[i], orientation, for_size, &part_min, &part_nat, NULL, NULL);
    *minimum += part_min;
    *natural += part_nat;
  }
}

static void
awra_emoji_picker_size_allocate (GtkWidget *widget,
                                 int        width,
                                 int        height,
                                 int        baseline)
{
  AwraEmojiPicker *self = AWRA_EMOJI_PICKER (widget);
  guint columns = columns_for (width);
  int header = 0;
  int footer = 0;

  (void) baseline;
  /* Before the list is laid out: the rows depend on the columns. */
  if (columns != self->rows->columns) {
    self->rows->columns = columns;
    relayout (self);
  }
  gtk_widget_measure (self->header, GTK_ORIENTATION_VERTICAL, width, NULL, &header, NULL, NULL);
  gtk_widget_measure (self->footer, GTK_ORIENTATION_VERTICAL, width, NULL, &footer, NULL, NULL);
  gtk_widget_allocate (self->header, width, header, -1, NULL);
  gtk_widget_allocate (self->grid_area, width, MAX (0, height - header - footer), -1,
                       gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (0, header)));
  gtk_widget_allocate (self->footer, width, footer, -1,
                       gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (0, height - footer)));
  if (self->show_top && self->rows->n_rows > 0) {
    self->show_top = FALSE;
    gtk_adjustment_set_value (gtk_scrolled_window_get_vadjustment (self->scroller), 0);
  }
  scrolled_cb (gtk_scrolled_window_get_vadjustment (self->scroller), self);
}

static gboolean
awra_emoji_picker_grab_focus (GtkWidget *widget)
{
  return gtk_widget_grab_focus (GTK_WIDGET (AWRA_EMOJI_PICKER (widget)->search));
}

static void
awra_emoji_picker_dispose (GObject *object)
{
  AwraEmojiPicker *self = AWRA_EMOJI_PICKER (object);

  if (self->grid != NULL) {
    gtk_list_view_set_model (self->grid, NULL);
    gtk_list_view_set_factory (self->grid, NULL);
    gtk_list_view_set_header_factory (self->grid, NULL);
  }
  if (self->sections != NULL) {
    if (self->rows != NULL)
      self->rows->sections = NULL;
    g_clear_pointer (&self->sections, g_ptr_array_unref);
  }
  g_clear_object (&self->rows);
  g_clear_object (&self->cursor);
  g_clear_object (&self->hovered);
  g_clear_pointer (&self->header, gtk_widget_unparent);
  g_clear_pointer (&self->grid_area, gtk_widget_unparent);
  g_clear_pointer (&self->footer, gtk_widget_unparent);
  self->grid = NULL;
  self->search = NULL;
  G_OBJECT_CLASS (awra_emoji_picker_parent_class)->dispose (object);
}

static void
awra_emoji_picker_finalize (GObject *object)
{
  AwraEmojiPicker *self = AWRA_EMOJI_PICKER (object);

  g_ptr_array_unref (self->bound);
  g_free (self->query);
  G_OBJECT_CLASS (awra_emoji_picker_parent_class)->finalize (object);
}

static void
awra_emoji_picker_class_init (AwraEmojiPickerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = awra_emoji_picker_dispose;
  object_class->finalize = awra_emoji_picker_finalize;
  widget_class->measure = awra_emoji_picker_measure;
  widget_class->size_allocate = awra_emoji_picker_size_allocate;
  widget_class->grab_focus = awra_emoji_picker_grab_focus;

  /**
   * AwraEmojiPicker::picked:
   * @self: the emoji picker
   * @item: the item picked
   *
   * Emitted when an item is clicked, or picked with Enter.
   *
   * Since: 1.1
   */
  signals[SIGNAL_PICKED] =
    g_signal_new ("picked", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 1, AWRA_TYPE_EMOJI_ITEM);

  gtk_widget_class_set_css_name (widget_class, "awraemojipicker");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
awra_emoji_picker_init (AwraEmojiPicker *self)
{
  GtkListItemFactory *rows_factory = gtk_signal_list_item_factory_new ();
  GtkListItemFactory *headers_factory = gtk_signal_list_item_factory_new ();
  GtkEventController *keys = gtk_event_controller_key_new ();
  GtkWidget *overlay = gtk_overlay_new ();
  GtkWidget *search_delegate;
  Section *recent;

  self->bound = g_ptr_array_new ();
  self->sections = g_ptr_array_new_with_free_func (section_free);
  self->current_section = G_MAXUINT;
  self->rows = g_object_new (AWRA_TYPE_EMOJI_ROWS, NULL);
  self->rows->sections = self->sections;

  self->header = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_add_css_class (self->header, "awra-emoji-header");
  self->search = AWRA_SEARCH_ENTRY (awra_search_entry_new ());
  awra_search_entry_set_placeholder_text (self->search, _("Search emoji"));
  search_delegate = GTK_WIDGET (awra_search_entry_get_delegate (self->search));
  g_signal_connect (search_delegate, "search-changed", G_CALLBACK (search_changed_cb), self);
  g_signal_connect (search_delegate, "activate", G_CALLBACK (search_activate_cb), self);
  gtk_box_append (GTK_BOX (self->header), GTK_WIDGET (self->search));
  self->tabs = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_add_css_class (self->tabs, "awra-emoji-tabs");
  self->tabs_scroller = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->tabs_scroller),
                                  GTK_POLICY_EXTERNAL, GTK_POLICY_NEVER);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->tabs_scroller), self->tabs);
  gtk_box_append (GTK_BOX (self->header), self->tabs_scroller);
  gtk_widget_set_parent (self->header, GTK_WIDGET (self));

  g_signal_connect (rows_factory, "setup", G_CALLBACK (row_setup_cb), self);
  g_signal_connect (rows_factory, "bind", G_CALLBACK (row_bind_cb), self);
  g_signal_connect (rows_factory, "unbind", G_CALLBACK (row_unbind_cb), self);
  g_signal_connect (headers_factory, "setup", G_CALLBACK (header_setup_cb), NULL);
  g_signal_connect (headers_factory, "bind", G_CALLBACK (header_bind_cb), NULL);
  self->grid = GTK_LIST_VIEW (gtk_list_view_new (
    GTK_SELECTION_MODEL (gtk_no_selection_new (g_object_ref (G_LIST_MODEL (self->rows)))),
    rows_factory));
  gtk_list_view_set_header_factory (self->grid, headers_factory);
  g_object_unref (headers_factory);
  gtk_widget_add_css_class (GTK_WIDGET (self->grid), "awra-emoji-grid");
  gtk_widget_set_can_focus (GTK_WIDGET (self->grid), FALSE);
  gtk_accessible_update_property (GTK_ACCESSIBLE (self->grid),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, _("Emoji"), -1);
  self->scroller = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new ());
  gtk_scrolled_window_set_policy (self->scroller, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (self->scroller, GTK_WIDGET (self->grid));
  g_signal_connect (gtk_scrolled_window_get_vadjustment (self->scroller), "value-changed",
                    G_CALLBACK (scrolled_cb), self);
  self->empty = gtk_label_new (_("No emoji found"));
  gtk_widget_add_css_class (self->empty, "awra-emoji-empty");
  gtk_widget_set_valign (self->empty, GTK_ALIGN_CENTER);
  gtk_widget_set_visible (self->empty, FALSE);
  gtk_overlay_set_child (GTK_OVERLAY (overlay), GTK_WIDGET (self->scroller));
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), self->empty);
  self->grid_area = overlay;
  gtk_widget_set_parent (self->grid_area, GTK_WIDGET (self));

  self->footer = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_add_css_class (self->footer, "awra-emoji-preview");
  self->preview_glyph = GTK_LABEL (gtk_label_new (NULL));
  gtk_widget_add_css_class (GTK_WIDGET (self->preview_glyph), "awra-emoji-preview-glyph");
  self->preview_picture = GTK_PICTURE (gtk_picture_new ());
  gtk_widget_add_css_class (GTK_WIDGET (self->preview_picture), "awra-emoji-preview-image");
  gtk_picture_set_content_fit (self->preview_picture, GTK_CONTENT_FIT_CONTAIN);
  self->preview_name = GTK_LABEL (gtk_label_new (NULL));
  gtk_label_set_xalign (self->preview_name, 0.0f);
  gtk_label_set_ellipsize (self->preview_name, PANGO_ELLIPSIZE_END);
  gtk_widget_set_hexpand (GTK_WIDGET (self->preview_name), TRUE);
  gtk_widget_add_css_class (GTK_WIDGET (self->preview_name), "awra-emoji-preview-name");
  gtk_box_append (GTK_BOX (self->footer), GTK_WIDGET (self->preview_glyph));
  gtk_box_append (GTK_BOX (self->footer), GTK_WIDGET (self->preview_picture));
  gtk_box_append (GTK_BOX (self->footer), GTK_WIDGET (self->preview_name));
  gtk_widget_set_parent (self->footer, GTK_WIDGET (self));

  recent = section_new ("recent", _("Recent"), "document-open-recent-symbolic");
  recent->recent = TRUE;
  recent->tab = make_tab (self);
  update_tab (recent);
  gtk_box_append (GTK_BOX (self->tabs), recent->tab);
  g_ptr_array_add (self->sections, recent);

  gtk_event_controller_set_propagation_phase (keys, GTK_PHASE_CAPTURE);
  g_signal_connect (keys, "key-pressed", G_CALLBACK (key_pressed_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), keys);
  refilter (self);
}

GtkWidget *
awra_emoji_picker_new (void)
{
  return g_object_new (AWRA_TYPE_EMOJI_PICKER, NULL);
}

static guint
find_section (AwraEmojiPicker *self,
              const char      *id)
{
  for (guint i = 1; i < self->sections->len; i++) {
    Section *section = g_ptr_array_index (self->sections, i);

    if (g_str_equal (section->id, id))
      return i;
  }
  return G_MAXUINT;
}

void
awra_emoji_picker_add_section (AwraEmojiPicker *self,
                               const char      *id,
                               const char      *title,
                               const char      *icon_name,
                               GListModel      *items)
{
  guint index;
  Section *section;

  g_return_if_fail (AWRA_IS_EMOJI_PICKER (self));
  g_return_if_fail (id != NULL);
  g_return_if_fail (title != NULL);
  g_return_if_fail (G_IS_LIST_MODEL (items));
  index = find_section (self, id);
  if (index != G_MAXUINT) {
    section = g_ptr_array_index (self->sections, index);
    g_free (section->title);
    section->title = g_strdup (title);
    g_free (section->icon_name);
    section->icon_name = g_strdup (icon_name);
    gtk_button_set_child (GTK_BUTTON (section->tab), NULL);
  } else {
    section = section_new (id, title, icon_name);
    section->tab = make_tab (self);
    gtk_box_append (GTK_BOX (self->tabs), section->tab);
    g_ptr_array_add (self->sections, section);
  }
  section_set_items (self, section, items);
  update_tab (section);
  refilter (self);
}

static void
remove_section_at (AwraEmojiPicker *self,
                   guint            index)
{
  Section *section = g_ptr_array_index (self->sections, index);

  if (self->current_section == index)
    self->current_section = G_MAXUINT;
  else if (self->current_section != G_MAXUINT && self->current_section > index)
    self->current_section--;
  gtk_box_remove (GTK_BOX (self->tabs), section->tab);
  g_ptr_array_remove_index (self->sections, index);
}

void
awra_emoji_picker_remove_section (AwraEmojiPicker *self,
                                  const char      *id)
{
  guint index;

  g_return_if_fail (AWRA_IS_EMOJI_PICKER (self));
  g_return_if_fail (id != NULL);
  index = find_section (self, id);
  if (index == G_MAXUINT)
    return;
  remove_section_at (self, index);
  refilter (self);
}

void
awra_emoji_picker_clear (AwraEmojiPicker *self)
{
  g_return_if_fail (AWRA_IS_EMOJI_PICKER (self));
  while (self->sections->len > 1)
    remove_section_at (self, self->sections->len - 1);
  refilter (self);
}

void
awra_emoji_picker_set_recent (AwraEmojiPicker *self,
                              GListModel      *items)
{
  Section *recent;

  g_return_if_fail (AWRA_IS_EMOJI_PICKER (self));
  g_return_if_fail (items == NULL || G_IS_LIST_MODEL (items));
  recent = g_ptr_array_index (self->sections, 0);
  if (recent->items == items)
    return;
  section_set_items (self, recent, items);
  refilter (self);
}

const char *
awra_emoji_picker_get_search_text (AwraEmojiPicker *self)
{
  g_return_val_if_fail (AWRA_IS_EMOJI_PICKER (self), NULL);
  return self->query != NULL ? self->query : "";
}

void
awra_emoji_picker_set_search_text (AwraEmojiPicker *self,
                                   const char      *text)
{
  GtkSearchEntry *entry;

  g_return_if_fail (AWRA_IS_EMOJI_PICKER (self));
  entry = awra_search_entry_get_delegate (self->search);
  gtk_editable_set_text (GTK_EDITABLE (entry), text != NULL ? text : "");
  /* Searches are applied at once, not after the typing delay. */
  search_changed_cb (entry, self);
}
