//! Collections and workspace pieces: the virtualized collection view, the
//! path bar, the tab bar and the paned (Awra 1's GridView / DataView,
//! PathBar, TabView strip, TileView).

use std::ffi::{c_char, c_int, c_void};

use crate::awra_wrapper;
use crate::enums::Orientation;
use crate::ffi;
use crate::object::{cstring, from_cstr, IsWidget, Widget};
use crate::signal::{sig, SignalHandlerId};

macro_rules! ptr {
    ($s:expr) => {
        $s.0.as_ptr()
    };
}

fn cstrings(items: &[&str]) -> (Vec<std::ffi::CString>, Vec<*const c_char>) {
    let owned: Vec<_> = items.iter().map(|s| cstring(s)).collect();
    let ptrs = owned.iter().map(|c| c.as_ptr()).collect();
    (owned, ptrs)
}

// ===========================================================================
// Collection view

/// How a collection view lays its items out.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CollectionLayout {
    Grid = 0,
    List = 1,
}

awra_wrapper!(
    /// A virtualized grid of tiles or list of rows with column headers.
    CollectionView, ["AwraCollectionView"]
);

struct Factory {
    setup: Box<dyn Fn(i32) -> Widget>,
    bind: Box<dyn Fn(&Widget, i32, usize)>,
    unbind: Box<dyn Fn(&Widget, i32, usize)>,
}

unsafe extern "C" fn factory_setup(_view: *mut ffi::AwraWidget, column: c_int, data: *mut c_void) -> *mut ffi::AwraWidget {
    let f = &*(data as *const Factory);
    let w = (f.setup)(column);
    let p = w.as_ptr();
    std::mem::forget(w); // the view takes this reference
    p
}

unsafe extern "C" fn factory_bind(_view: *mut ffi::AwraWidget, cell: *mut ffi::AwraWidget, column: c_int, index: c_int, data: *mut c_void) {
    let f = &*(data as *const Factory);
    let w = std::mem::ManuallyDrop::new(Widget::from_full(cell));
    (f.bind)(&w, column, index.max(0) as usize);
}

unsafe extern "C" fn factory_unbind(_view: *mut ffi::AwraWidget, cell: *mut ffi::AwraWidget, column: c_int, index: c_int, data: *mut c_void) {
    let f = &*(data as *const Factory);
    let w = std::mem::ManuallyDrop::new(Widget::from_full(cell));
    (f.unbind)(&w, column, index.max(0) as usize);
}

unsafe extern "C" fn factory_free(data: *mut c_void) {
    drop(Box::from_raw(data as *mut Factory));
}

impl CollectionView {
    pub fn new(layout: CollectionLayout) -> CollectionView {
        unsafe { CollectionView::from_full(ffi::awra_collection_view_new(layout as _)) }
    }
    pub fn set_layout(&self, layout: CollectionLayout) {
        unsafe { ffi::awra_collection_view_set_layout(ptr!(self), layout as _) }
    }
    pub fn layout(&self) -> CollectionLayout {
        if unsafe { ffi::awra_collection_view_layout(ptr!(self)) } == 1 {
            CollectionLayout::List
        } else {
            CollectionLayout::Grid
        }
    }
    /// setup(column) builds a cell (column -1 in a grid); bind(cell, column,
    /// index) fills it; unbind(cell, column, index) lets it go.
    pub fn set_factory(
        &self,
        setup: impl Fn(i32) -> Widget + 'static,
        bind: impl Fn(&Widget, i32, usize) + 'static,
        unbind: impl Fn(&Widget, i32, usize) + 'static,
    ) {
        let f = Box::new(Factory { setup: Box::new(setup), bind: Box::new(bind), unbind: Box::new(unbind) });
        unsafe {
            ffi::awra_collection_view_set_factory(
                ptr!(self),
                Some(factory_setup),
                Some(factory_bind),
                Some(factory_unbind),
                Box::into_raw(f) as *mut c_void,
                Some(factory_free),
            )
        }
    }
    pub fn set_item_count(&self, n: usize) {
        unsafe { ffi::awra_collection_view_set_item_count(ptr!(self), n as c_int) }
    }
    pub fn item_count(&self) -> usize {
        unsafe { ffi::awra_collection_view_item_count(ptr!(self)).max(0) as usize }
    }
    pub fn items_changed(&self, position: usize, removed: usize, added: usize) {
        unsafe { ffi::awra_collection_view_items_changed(ptr!(self), position as c_int, removed as c_int, added as c_int) }
    }
    /// Binds an item again (None: every item on screen).
    pub fn rebind(&self, index: Option<usize>) {
        unsafe { ffi::awra_collection_view_rebind(ptr!(self), index.map_or(-1, |i| i as c_int)) }
    }
    pub fn cell(&self, index: usize, column: i32) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_collection_view_cell(ptr!(self), index as c_int, column)) }
    }
    /// (first, count) of the items on screen.
    pub fn visible_range(&self) -> (usize, usize) {
        let (mut f, mut n) = (0, 0);
        unsafe { ffi::awra_collection_view_visible_range(ptr!(self), &mut f, &mut n) };
        (f.max(0) as usize, n.max(0) as usize)
    }
    pub fn set_item_size(&self, width: i32, height: i32) {
        unsafe { ffi::awra_collection_view_set_item_size(ptr!(self), width, height) }
    }
    pub fn set_row_height(&self, height: i32) {
        unsafe { ffi::awra_collection_view_set_row_height(ptr!(self), height) }
    }
    pub fn set_max_columns(&self, n: i32) {
        unsafe { ffi::awra_collection_view_set_max_columns(ptr!(self), n) }
    }
    pub fn add_column(&self, title: &str, width: i32, expand: bool) -> i32 {
        let t = cstring(title);
        unsafe { ffi::awra_collection_view_add_column(ptr!(self), t.as_ptr(), width, expand) }
    }
    pub fn set_column_title(&self, column: i32, title: &str) {
        let t = cstring(title);
        unsafe { ffi::awra_collection_view_set_column_title(ptr!(self), column, t.as_ptr()) }
    }
    pub fn set_column_visible(&self, column: i32, visible: bool) {
        unsafe { ffi::awra_collection_view_set_column_visible(ptr!(self), column, visible) }
    }
    pub fn column_visible(&self, column: i32) -> bool {
        unsafe { ffi::awra_collection_view_column_visible(ptr!(self), column) }
    }
    pub fn set_column_width(&self, column: i32, width: i32) {
        unsafe { ffi::awra_collection_view_set_column_width(ptr!(self), column, width) }
    }
    pub fn column_width(&self, column: i32) -> i32 {
        unsafe { ffi::awra_collection_view_column_width(ptr!(self), column) }
    }
    /// The column showing the order (None: none).
    pub fn set_sort_column(&self, column: Option<i32>, descending: bool) {
        unsafe { ffi::awra_collection_view_set_sort_column(ptr!(self), column.unwrap_or(-1), descending) }
    }
    pub fn set_show_header(&self, show: bool) {
        unsafe { ffi::awra_collection_view_set_show_header(ptr!(self), show) }
    }
    pub fn header_height(&self) -> i32 {
        unsafe { ffi::awra_collection_view_header_height(ptr!(self)) }
    }
    pub fn column_at(&self, x: f64, y: f64) -> Option<i32> {
        let c = unsafe { ffi::awra_collection_view_column_at(ptr!(self), x, y) };
        (c >= 0).then_some(c)
    }
    /// The item at a point (view coordinates) and whether the point is on its
    /// content (the tile, or the row's name).
    pub fn index_at(&self, x: f64, y: f64) -> Option<(usize, bool)> {
        let mut content = false;
        let i = unsafe { ffi::awra_collection_view_index_at(ptr!(self), x, y, &mut content) };
        (i >= 0).then_some((i as usize, content))
    }
    pub fn item_bounds(&self, index: usize) -> Option<(f64, f64, f64, f64)> {
        let (mut x, mut y, mut w, mut h) = (0.0, 0.0, 0.0, 0.0);
        unsafe { ffi::awra_collection_view_item_bounds(ptr!(self), index as c_int, &mut x, &mut y, &mut w, &mut h) }
            .then_some((x, y, w, h))
    }
    pub fn is_selected(&self, index: usize) -> bool {
        unsafe { ffi::awra_collection_view_is_selected(ptr!(self), index as c_int) }
    }
    pub fn selected_count(&self) -> usize {
        unsafe { ffi::awra_collection_view_selected_count(ptr!(self)).max(0) as usize }
    }
    /// The selected indices in order, at most `limit` of them.
    pub fn selected(&self, limit: Option<usize>) -> Vec<usize> {
        let count = self.selected_count();
        let max = limit.map_or(count, |l| l.min(count));
        let mut out = vec![0 as c_int; max];
        let n = unsafe { ffi::awra_collection_view_get_selected(ptr!(self), out.as_mut_ptr(), max as c_int) };
        out.truncate(n.max(0) as usize);
        out.into_iter().map(|i| i as usize).collect()
    }
    pub fn set_selected(&self, index: usize, selected: bool) {
        unsafe { ffi::awra_collection_view_set_selected(ptr!(self), index as c_int, selected) }
    }
    pub fn select_only(&self, indices: &[usize]) {
        let v: Vec<c_int> = indices.iter().map(|&i| i as c_int).collect();
        unsafe { ffi::awra_collection_view_select_only(ptr!(self), v.as_ptr(), v.len() as c_int) }
    }
    pub fn select_all(&self) {
        unsafe { ffi::awra_collection_view_select_all(ptr!(self)) }
    }
    pub fn unselect_all(&self) {
        unsafe { ffi::awra_collection_view_unselect_all(ptr!(self)) }
    }
    pub fn cursor(&self) -> Option<usize> {
        let c = unsafe { ffi::awra_collection_view_cursor(ptr!(self)) };
        (c >= 0).then_some(c as usize)
    }
    pub fn set_cursor(&self, index: Option<usize>) {
        unsafe { ffi::awra_collection_view_set_cursor(ptr!(self), index.map_or(-1, |i| i as c_int)) }
    }
    pub fn scroll_to(&self, index: usize) {
        unsafe { ffi::awra_collection_view_scroll_to(ptr!(self), index as c_int) }
    }
    pub fn vvalue(&self) -> f64 {
        unsafe { ffi::awra_collection_view_vvalue(ptr!(self)) }
    }
    pub fn set_vvalue(&self, v: f64) {
        unsafe { ffi::awra_collection_view_set_vvalue(ptr!(self), v) }
    }
    pub fn set_drop_index(&self, index: Option<usize>) {
        unsafe { ffi::awra_collection_view_set_drop_index(ptr!(self), index.map_or(-1, |i| i as c_int)) }
    }
    pub fn grid_columns(&self) -> i32 {
        unsafe { ffi::awra_collection_view_grid_columns(ptr!(self)) }
    }
    pub fn connect_selection_changed<F: Fn(&CollectionView) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed(sig::NOTIFY, move |v: &CollectionView, a| {
            if a.text().as_deref() == Some("selection") {
                f(v)
            }
        })
    }
    pub fn connect_activate<F: Fn(&CollectionView, usize) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed(sig::ACTIVATE, move |v: &CollectionView, a| f(v, a.index().max(0) as usize))
    }
    /// A drag starts from an item: (index, x, y at the press).
    pub fn connect_drag_begin<F: Fn(&CollectionView, usize, f64, f64) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed(sig::DRAG_BEGIN, move |v: &CollectionView, a| f(v, a.index().max(0) as usize, a.x(), a.y()))
    }
    /// A column header was clicked: (column, the descending order it asks for).
    pub fn connect_header_clicked<F: Fn(&CollectionView, i32, bool) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed(sig::SELECTED, move |v: &CollectionView, a| f(v, a.index(), a.active()))
    }
    pub fn connect_scrolled<F: Fn(&CollectionView) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed(sig::VALUE_CHANGED, move |v: &CollectionView, _| f(v))
    }
}

// ===========================================================================
// Path bar

awra_wrapper!(
    /// Breadcrumbs and a path field in one frame.
    PathBar, ["AwraPathBar"]
);

impl PathBar {
    pub fn new() -> PathBar {
        unsafe { PathBar::from_full(ffi::awra_path_bar_new()) }
    }
    pub fn set_items(&self, labels: &[&str]) {
        let (_o, p) = cstrings(labels);
        unsafe { ffi::awra_path_bar_set_items(ptr!(self), p.as_ptr(), p.len() as c_int) }
    }
    pub fn set_icon_name(&self, icon: Option<&str>) {
        let c = icon.map(cstring);
        unsafe { ffi::awra_path_bar_set_icon_name(ptr!(self), c.as_ref().map_or(std::ptr::null(), |c| c.as_ptr())) }
    }
    pub fn set_text(&self, text: &str) {
        let c = cstring(text);
        unsafe { ffi::awra_path_bar_set_text(ptr!(self), c.as_ptr()) }
    }
    pub fn set_placeholder_text(&self, text: &str) {
        let c = cstring(text);
        unsafe { ffi::awra_path_bar_set_placeholder(ptr!(self), c.as_ptr()) }
    }
    pub fn set_end_widget(&self, w: Option<&impl IsWidget>) {
        unsafe { ffi::awra_path_bar_set_end_widget(ptr!(self), w.map_or(std::ptr::null_mut(), |w| w.widget_ptr())) }
    }
    pub fn set_blank_tooltip(&self, text: &str) {
        let c = cstring(text);
        unsafe { ffi::awra_path_bar_set_blank_tooltip(ptr!(self), c.as_ptr()) }
    }
    pub fn begin_edit(&self, initial: Option<&str>) {
        let c = initial.map(cstring);
        unsafe { ffi::awra_path_bar_begin_edit(ptr!(self), c.as_ref().map_or(std::ptr::null(), |c| c.as_ptr())) }
    }
    pub fn end_edit(&self) {
        unsafe { ffi::awra_path_bar_end_edit(ptr!(self)) }
    }
    pub fn is_editing(&self) -> bool {
        unsafe { ffi::awra_path_bar_editing(ptr!(self)) }
    }
    pub fn edit_text(&self) -> String {
        unsafe { from_cstr(ffi::awra_path_bar_edit_text(ptr!(self))) }
    }
    pub fn entry(&self) -> crate::Entry {
        unsafe { <crate::Entry as crate::StaticType>::unchecked_from(Widget::from_none(ffi::awra_path_bar_entry(ptr!(self)))) }
    }
    /// (title, value inserted) pairs; empty hides them.
    pub fn set_suggestions(&self, items: &[(String, String)]) {
        let titles: Vec<&str> = items.iter().map(|(t, _)| t.as_str()).collect();
        let values: Vec<&str> = items.iter().map(|(_, v)| v.as_str()).collect();
        let (_a, tp) = cstrings(&titles);
        let (_b, vp) = cstrings(&values);
        unsafe { ffi::awra_path_bar_set_suggestions(ptr!(self), tp.as_ptr(), vp.as_ptr(), tp.len() as c_int) }
    }
    pub fn set_invalid(&self, invalid: bool) {
        unsafe { ffi::awra_path_bar_set_invalid(ptr!(self), invalid) }
    }
    pub fn item_at(&self, x: f64, y: f64) -> Option<usize> {
        let i = unsafe { ffi::awra_path_bar_item_at(ptr!(self), x, y) };
        (i >= 0).then_some(i as usize)
    }
    pub fn set_drop_item(&self, index: Option<usize>) {
        unsafe { ffi::awra_path_bar_set_drop_item(ptr!(self), index.map_or(-1, |i| i as c_int)) }
    }
    /// A breadcrumb was clicked.
    pub fn connect_activate_item<F: Fn(&PathBar, usize) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed(sig::ACTIVATE, move |b: &PathBar, a| {
            if a.index() >= 0 {
                f(b, a.index() as usize)
            }
        })
    }
    /// The text was submitted: return false to refuse it (the field stays open).
    pub fn connect_submit<F: Fn(&PathBar, &str) -> bool + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed(sig::ACTIVATE, move |b: &PathBar, a| {
            if a.index() < 0 && !f(b, &a.text().unwrap_or_default()) {
                a.stop();
            }
        })
    }
    pub fn connect_edit_changed<F: Fn(&PathBar, &str) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed(sig::CHANGED, move |b: &PathBar, a| f(b, &a.text().unwrap_or_default()))
    }
    pub fn connect_editing_notify<F: Fn(&PathBar, bool) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed(sig::NOTIFY, move |b: &PathBar, a| {
            if a.text().as_deref() == Some("editing") {
                f(b, b.is_editing())
            }
        })
    }
}

// ===========================================================================
// Tab bar

awra_wrapper!(
    /// The strip of a tab view.
    TabBar, ["AwraTabBar"]
);

impl TabBar {
    pub fn new() -> TabBar {
        unsafe { TabBar::from_full(ffi::awra_tab_bar_new()) }
    }
    pub fn insert(&self, position: Option<usize>, title: &str, icon: Option<&str>) -> usize {
        let t = cstring(title);
        let i = icon.map(cstring);
        unsafe {
            ffi::awra_tab_bar_insert(
                ptr!(self),
                position.map_or(-1, |p| p as c_int),
                t.as_ptr(),
                i.as_ref().map_or(std::ptr::null(), |c| c.as_ptr()),
            )
            .max(0) as usize
        }
    }
    pub fn remove(&self, index: usize) {
        unsafe { ffi::awra_tab_bar_remove(ptr!(self), index as c_int) }
    }
    pub fn move_tab(&self, from: usize, to: usize) {
        unsafe { ffi::awra_tab_bar_move(ptr!(self), from as c_int, to as c_int) }
    }
    pub fn set_title(&self, index: usize, title: &str) {
        let t = cstring(title);
        unsafe { ffi::awra_tab_bar_set_title(ptr!(self), index as c_int, t.as_ptr()) }
    }
    pub fn set_icon_name(&self, index: usize, icon: &str) {
        let t = cstring(icon);
        unsafe { ffi::awra_tab_bar_set_icon_name(ptr!(self), index as c_int, t.as_ptr()) }
    }
    pub fn set_tab_tooltip(&self, index: usize, text: &str) {
        let t = cstring(text);
        unsafe { ffi::awra_tab_bar_set_tab_tooltip(ptr!(self), index as c_int, t.as_ptr()) }
    }
    pub fn count(&self) -> usize {
        unsafe { ffi::awra_tab_bar_count(ptr!(self)).max(0) as usize }
    }
    pub fn selected(&self) -> Option<usize> {
        let s = unsafe { ffi::awra_tab_bar_selected(ptr!(self)) };
        (s >= 0).then_some(s as usize)
    }
    pub fn set_selected(&self, index: usize) {
        unsafe { ffi::awra_tab_bar_set_selected(ptr!(self), index as c_int) }
    }
    pub fn set_active(&self, active: bool) {
        unsafe { ffi::awra_tab_bar_set_active(ptr!(self), active) }
    }
    pub fn index_at(&self, x: f64, y: f64) -> Option<usize> {
        let i = unsafe { ffi::awra_tab_bar_index_at(ptr!(self), x, y) };
        (i >= 0).then_some(i as usize)
    }
    pub fn insertion_at(&self, x: f64) -> usize {
        unsafe { ffi::awra_tab_bar_insertion_at(ptr!(self), x).max(0) as usize }
    }
    pub fn set_insertion(&self, position: Option<usize>) {
        unsafe { ffi::awra_tab_bar_set_insertion(ptr!(self), position.map_or(-1, |p| p as c_int)) }
    }
    pub fn connect_selected<F: Fn(&TabBar, usize) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed(sig::SELECTED, move |b: &TabBar, a| f(b, a.index().max(0) as usize))
    }
    pub fn connect_close_tab<F: Fn(&TabBar, usize) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed(sig::CLOSE_REQUEST, move |b: &TabBar, a| f(b, a.index().max(0) as usize))
    }
    pub fn connect_moved<F: Fn(&TabBar, usize, usize) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed(sig::MOVED, move |b: &TabBar, a| {
            let t = a.text().unwrap_or_default();
            let mut it = t.split('\t').filter_map(|s| s.parse::<usize>().ok());
            if let (Some(from), Some(to)) = (it.next(), it.next()) {
                f(b, from, to)
            }
        })
    }
    /// A double or middle click on the blank of the bar.
    pub fn connect_new_tab<F: Fn(&TabBar) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed(sig::ACTIVATE, move |b: &TabBar, _| f(b))
    }
    pub fn connect_drag_begin<F: Fn(&TabBar, usize) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed(sig::DRAG_BEGIN, move |b: &TabBar, a| f(b, a.index().max(0) as usize))
    }
}

// ===========================================================================
// Paned

awra_wrapper!(
    /// Two children split by a draggable separator.
    Paned, ["AwraPaned"]
);

impl Paned {
    pub fn new(orientation: Orientation) -> Paned {
        unsafe { Paned::from_full(ffi::awra_paned_new(orientation as _)) }
    }
    pub fn set_start_child(&self, child: Option<&impl IsWidget>) {
        unsafe { ffi::awra_paned_set_start_child(ptr!(self), child.map_or(std::ptr::null_mut(), |c| c.widget_ptr())) }
    }
    pub fn set_end_child(&self, child: Option<&impl IsWidget>) {
        unsafe { ffi::awra_paned_set_end_child(ptr!(self), child.map_or(std::ptr::null_mut(), |c| c.widget_ptr())) }
    }
    pub fn start_child(&self) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_paned_start_child(ptr!(self))) }
    }
    pub fn end_child(&self) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_paned_end_child(ptr!(self))) }
    }
    pub fn set_orientation(&self, o: Orientation) {
        unsafe { ffi::awra_paned_set_orientation(ptr!(self), o as _) }
    }
    pub fn orientation(&self) -> Orientation {
        if unsafe { ffi::awra_paned_orientation(ptr!(self)) } == 1 {
            Orientation::Vertical
        } else {
            Orientation::Horizontal
        }
    }
    pub fn set_fraction(&self, f: f64) {
        unsafe { ffi::awra_paned_set_fraction(ptr!(self), f) }
    }
    pub fn fraction(&self) -> f64 {
        unsafe { ffi::awra_paned_fraction(ptr!(self)) }
    }
    pub fn connect_position_notify<F: Fn(&Paned) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed(sig::NOTIFY, move |p: &Paned, a| {
            if a.text().as_deref() == Some("position") {
                f(p)
            }
        })
    }
}

// ===========================================================================
// Entry additions

impl crate::Entry {
    /// false: a bare text field, its container draws the frame.
    pub fn set_has_frame(&self, frame: bool) {
        unsafe { ffi::awra_entry_set_has_frame(self.as_ptr(), frame) }
    }
}

impl crate::Header {
    /// The centre widget takes all the room between the start and end ones.
    pub fn set_expand_center(&self, expand: bool) {
        unsafe { ffi::awra_header_set_expand_center(self.as_ptr(), expand) }
    }
}

// ===========================================================================
// Icons

/// Registers a symbolic icon (SVG source) under a name.
pub fn icon_register_svg(name: &str, svg: &str) -> bool {
    let n = cstring(name);
    unsafe { ffi::awra_icon_register_svg(n.as_ptr(), svg.as_ptr() as *const c_char, svg.len()) }
}

/// Whether Awra knows a symbolic icon of that name.
pub fn icon_exists(name: &str) -> bool {
    let n = cstring(name);
    unsafe { ffi::awra_icon_exists(n.as_ptr()) }
}

// ===========================================================================
// Drawing area

awra_wrapper!(
    /// A widget the application draws itself (GtkDrawingArea).
    DrawingArea, ["AwraDrawingArea"]
);

/// Drawing in a drawing area, in its logical pixels.
pub struct Canvas(*mut ffi::AwraCanvas);

impl Canvas {
    pub fn fill_rect(&self, x: f64, y: f64, w: f64, h: f64, c: &crate::gdk::RGBA) {
        let c = c.to_ffi();
        unsafe { ffi::awra_canvas_fill_rect(self.0, x, y, w, h, &c) }
    }
    pub fn round_rect(&self, x: f64, y: f64, w: f64, h: f64, radius: f64, fill: Option<&crate::gdk::RGBA>, border_width: f64, border: Option<&crate::gdk::RGBA>) {
        let f = fill.map(|c| c.to_ffi());
        let b = border.map(|c| c.to_ffi());
        unsafe {
            ffi::awra_canvas_round_rect(
                self.0, x, y, w, h, radius,
                f.as_ref().map_or(std::ptr::null(), |c| c as *const _),
                border_width,
                b.as_ref().map_or(std::ptr::null(), |c| c as *const _),
            )
        }
    }
    pub fn line(&self, x0: f64, y0: f64, x1: f64, y1: f64, width: f64, c: &crate::gdk::RGBA) {
        let c = c.to_ffi();
        unsafe { ffi::awra_canvas_line(self.0, x0, y0, x1, y1, width, &c) }
    }
    pub fn circle(&self, cx: f64, cy: f64, r: f64, c: &crate::gdk::RGBA) {
        let c = c.to_ffi();
        unsafe { ffi::awra_canvas_circle(self.0, cx, cy, r, &c) }
    }
    pub fn icon(&self, name: &str, x: f64, y: f64, size: f64, c: &crate::gdk::RGBA) {
        let n = cstring(name);
        let c = c.to_ffi();
        unsafe { ffi::awra_canvas_icon(self.0, n.as_ptr(), x, y, size, &c) }
    }
}

type DrawFn = Box<dyn Fn(&DrawingArea, &Canvas, i32, i32)>;

unsafe extern "C" fn draw_tramp(w: *mut ffi::AwraWidget, c: *mut ffi::AwraCanvas, width: c_int, height: c_int, data: *mut c_void) {
    let f = &*(data as *const DrawFn);
    let area = std::mem::ManuallyDrop::new(DrawingArea(Widget::from_full(w)));
    f(&area, &Canvas(c), width, height);
}

unsafe extern "C" fn draw_free(data: *mut c_void) {
    drop(Box::from_raw(data as *mut DrawFn));
}

impl DrawingArea {
    pub fn new(draw: impl Fn(&DrawingArea, &Canvas, i32, i32) + 'static) -> DrawingArea {
        let f: Box<DrawFn> = Box::new(Box::new(draw));
        unsafe { DrawingArea::from_full(ffi::awra_drawing_area_new(Some(draw_tramp), Box::into_raw(f) as *mut c_void, Some(draw_free))) }
    }
    pub fn set_content_size(&self, w: i32, h: i32) {
        unsafe { ffi::awra_drawing_area_set_content_size(ptr!(self), w, h) }
    }
}
