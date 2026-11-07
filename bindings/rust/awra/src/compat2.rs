//! More gtk-rs names: methods of every widget for generic code
//! (`impl IsA<Widget>`), adjustments, types, data, displays, drags and
//! drops, applications.
//!
//! SPDX-License-Identifier: GPL-3.0-or-later

use std::any::Any;
use std::cell::{Cell, RefCell};
use std::ffi::c_void;
use std::rc::Rc;

use crate::compat::Overflow;
use crate::enums::*;
use crate::ffi;
use crate::glib::Propagation;
use crate::object::{cstring, IsWidget, StaticType, Widget};
use crate::signal::{sig, SignalHandlerId};

// ===========================================================================
// Types (gtk-rs `T::static_type()` and `ancestor`)

/// A widget type, as a test of a widget.
#[derive(Clone, Copy)]
pub struct Type(pub(crate) fn(&Widget) -> bool);

fn no_widget(_: &Widget) -> bool {
    false
}
impl Type {
    /// G_TYPE_STRING: what application drags of text carry.
    pub const STRING: Type = Type(no_widget);
}

impl std::fmt::Debug for Type {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Type")
    }
}

/// `T::static_type()` for every widget type.
pub trait StaticTypeExt: StaticType {
    fn static_type() -> Type {
        Type(Self::matches)
    }
}
impl<T: StaticType> StaticTypeExt for T {}

impl Widget {
    /// The nearest ancestor of a type.
    pub fn ancestor(&self, t: Type) -> Option<Widget> {
        let mut w = self.parent();
        while let Some(cur) = w {
            if (t.0)(&cur) {
                return Some(cur);
            }
            w = cur.parent();
        }
        None
    }
    pub fn set_overflow(&self, o: Overflow) {
        unsafe { ffi::awra_widget_set_overflow_hidden(self.as_ptr(), o == Overflow::Hidden) }
    }
    pub fn tooltip_text(&self) -> Option<String> {
        self.rust_data::<Option<String>>("p:tooltip").flatten()
    }
    pub fn set_has_tooltip(&self, has: bool) {
        if !has {
            unsafe { ffi::awra_widget_set_tooltip(self.as_ptr(), std::ptr::null()) }
        } else if let Some(t) = self.tooltip_text() {
            let c = cstring(&t);
            unsafe { ffi::awra_widget_set_tooltip(self.as_ptr(), c.as_ptr()) }
        }
    }
    /// Pixels per logical pixel of the window.
    pub fn scale_factor(&self) -> i32 {
        1
    }
    /// The pointer's place in the widget, when it's over it.
    pub fn pointer_position(&self) -> Option<(f64, f64)> {
        let (mut x, mut y) = (0.0, 0.0);
        unsafe { ffi::awra_widget_pointer_position(self.as_ptr(), &mut x, &mut y) }.then_some((x, y))
    }
    /// GObject data (gtk-rs's unsafe set_data / data).
    ///
    /// # Safety
    /// As gtk-rs: the value is read back as the same type.
    pub unsafe fn set_data<T: Any>(&self, key: &str, value: T) {
        self.set_rust_data(&format!("data:{key}"), Rc::new(value));
    }
    /// # Safety
    /// As gtk-rs.
    pub unsafe fn data<T: Any>(&self, key: &str) -> Option<std::ptr::NonNull<T>> {
        self.rust_data::<Rc<T>>(&format!("data:{key}"))
            .map(|rc| std::ptr::NonNull::new_unchecked(Rc::as_ptr(&rc) as *mut T))
    }
    /// A property's change (gtk-rs connect_notify_local); the names Awra
    /// reports: "at-bottom", "child-revealed", "has-focus", "cursor-position"…
    pub fn connect_notify_local<F: Fn(&Widget, &str) + 'static>(&self, name: Option<&str>, f: F) -> SignalHandlerId {
        let name = name.map(str::to_owned);
        self.connect_raw(sig::NOTIFY, move |w, a| {
            let n = a.text().unwrap_or_default();
            if name.as_deref().is_none_or(|x| x == n) {
                f(w, &n)
            }
        })
    }
}

// ===========================================================================
// Methods of every widget, for generic code (`&impl IsA<Widget>`)

/// gtk-rs WidgetExt: what every widget does, on anything that is one.
pub trait WidgetExt: IsWidget {
    fn root(&self) -> Option<crate::Window> {
        self.upcast_ref().root()
    }
    fn parent(&self) -> Option<Widget> {
        self.upcast_ref().parent()
    }
    fn set_visible(&self, v: bool) {
        self.upcast_ref().set_visible(v)
    }
    fn is_visible(&self) -> bool {
        self.upcast_ref().is_visible()
    }
    fn is_mapped(&self) -> bool {
        self.upcast_ref().is_mapped()
    }
    fn set_sensitive(&self, s: bool) {
        self.upcast_ref().set_sensitive(s)
    }
    fn is_sensitive(&self) -> bool {
        self.upcast_ref().is_sensitive()
    }
    fn set_halign(&self, a: Align) {
        self.upcast_ref().set_halign(a)
    }
    fn set_valign(&self, a: Align) {
        self.upcast_ref().set_valign(a)
    }
    fn set_hexpand(&self, e: bool) {
        self.upcast_ref().set_hexpand(e)
    }
    fn set_vexpand(&self, e: bool) {
        self.upcast_ref().set_vexpand(e)
    }
    fn set_margin_top(&self, m: i32) {
        self.upcast_ref().set_margin_top(m)
    }
    fn set_margin_bottom(&self, m: i32) {
        self.upcast_ref().set_margin_bottom(m)
    }
    fn set_margin_start(&self, m: i32) {
        self.upcast_ref().set_margin_start(m)
    }
    fn set_margin_end(&self, m: i32) {
        self.upcast_ref().set_margin_end(m)
    }
    fn set_tooltip_text(&self, t: Option<&str>) {
        self.upcast_ref().set_tooltip_text(t)
    }
    fn grab_focus(&self) -> bool {
        self.upcast_ref().grab_focus()
    }
    fn ancestor(&self, t: Type) -> Option<Widget> {
        self.upcast_ref().ancestor(t)
    }
    fn width(&self) -> i32 {
        self.upcast_ref().width()
    }
    fn height(&self) -> i32 {
        self.upcast_ref().height()
    }
    fn add_controller(&self, c: impl crate::widget::Controller) {
        self.upcast_ref().add_controller(c)
    }
    fn activate_action(&self, name: &str, target: Option<&crate::gio::Variant>) -> bool {
        self.upcast_ref().activate_action(name, target)
    }
    fn measure(&self, o: Orientation, for_size: i32) -> (i32, i32, i32, i32) {
        self.upcast_ref().measure(o, for_size)
    }
    fn compute_bounds(&self, target: &impl IsWidget) -> Option<crate::Rectangle> {
        self.upcast_ref().compute_bounds(target)
    }
    fn display(&self) -> crate::widget::Display {
        self.upcast_ref().display()
    }
}
impl<T: IsWidget> WidgetExt for T {}

// ===========================================================================
// Adjustments (scrolled windows)

/// A scrolled window's range (GtkAdjustment).
#[derive(Clone, Debug)]
pub struct Adjustment {
    scrolled: Widget,
    vertical: bool,
}

impl Adjustment {
    fn get(&self) -> (f64, f64, f64) {
        if self.vertical {
            let (mut v, mut u, mut p) = (0.0, 0.0, 0.0);
            unsafe { ffi::awra_scrolled_get_vadjustment(self.scrolled.as_ptr(), &mut v, &mut u, &mut p) };
            (v, u, p)
        } else {
            let v = unsafe { ffi::awra_scrolled_hvalue(self.scrolled.as_ptr()) };
            (v, v + self.scrolled.width() as f64, self.scrolled.width() as f64)
        }
    }
    pub fn value(&self) -> f64 {
        self.get().0
    }
    pub fn upper(&self) -> f64 {
        self.get().1
    }
    pub fn lower(&self) -> f64 {
        0.0
    }
    pub fn page_size(&self) -> f64 {
        self.get().2
    }
    pub fn set_value(&self, v: f64) {
        unsafe {
            if self.vertical {
                ffi::awra_scrolled_set_vvalue(self.scrolled.as_ptr(), v)
            } else {
                ffi::awra_scrolled_set_hvalue(self.scrolled.as_ptr(), v)
            }
        }
    }
    pub fn connect_value_changed<F: Fn(&Adjustment) + 'static>(&self, f: F) -> SignalHandlerId {
        let me = self.clone();
        self.scrolled.connect_raw(sig::VALUE_CHANGED, move |_, _| f(&me))
    }
    /// The page (the scrolled window's size) changed.
    pub fn connect_page_size_notify<F: Fn(&Adjustment) + 'static>(&self, f: F) -> SignalHandlerId {
        let me = self.clone();
        let last = Cell::new(-1.0);
        self.scrolled.connect_raw(sig::RESIZE, move |_, _| {
            let p = me.page_size();
            if p != last.replace(p) {
                f(&me)
            }
        })
    }
    pub fn connect_changed<F: Fn(&Adjustment) + 'static>(&self, f: F) -> SignalHandlerId {
        let me = self.clone();
        self.scrolled.connect_raw(sig::RESIZE, move |_, _| f(&me))
    }
}

impl crate::ScrolledWindow {
    pub fn vadjustment(&self) -> Adjustment {
        Adjustment { scrolled: self.upcast(), vertical: true }
    }
    pub fn hadjustment(&self) -> Adjustment {
        Adjustment { scrolled: self.upcast(), vertical: false }
    }
    pub fn set_min_content_width(&self, w: i32) {
        let (_, h) = self.size_request();
        self.set_size_request(w, h);
    }
    pub fn set_max_content_width(&self, _w: i32) {}
    /// An end reached by scrolling (GtkScrolledWindow::edge-reached).
    pub fn connect_edge_reached<F: Fn(&crate::ScrolledWindow, PositionType) + 'static>(&self, f: F) -> SignalHandlerId {
        let weak = self.downgrade();
        let at = Rc::new(Cell::new((false, false)));
        self.connect_raw(sig::VALUE_CHANGED, move |_, _| {
            let Some(s) = weak.upgrade() else { return };
            let a = s.vadjustment();
            let (v, u, p) = a.get();
            let top = v <= 0.5;
            let bottom = u - p - v <= 0.5;
            let (was_top, was_bottom) = at.get();
            at.set((top, bottom));
            if top && !was_top {
                f(&s, PositionType::Top)
            }
            if bottom && !was_bottom {
                f(&s, PositionType::Bottom)
            }
        })
    }
}

impl Widget {
    /// The adjustment of a scrolled window or what's inside one.
    pub fn vadjustment(&self) -> Option<Adjustment> {
        let s = if self.is::<crate::ScrolledWindow>() { Some(self.clone()) } else { self.ancestor(crate::ScrolledWindow::static_type()) };
        s.map(|scrolled| Adjustment { scrolled, vertical: true })
    }
}

// ===========================================================================
// gdk: displays, clipboards, drags and drops

/// gdk::Display: the clipboard of the application's window.
#[derive(Clone, Debug)]
pub struct GdkDisplay;
impl GdkDisplay {
    pub fn default() -> Option<GdkDisplay> {
        Some(GdkDisplay)
    }
    pub fn clipboard(&self) -> crate::widget::Clipboard {
        crate::widget::Clipboard::of_any_window()
    }
}

/// gdk::DragAction.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct DragAction(u32);
impl DragAction {
    pub const COPY: DragAction = DragAction(1);
    pub const MOVE: DragAction = DragAction(2);
    pub const LINK: DragAction = DragAction(4);
    pub fn empty() -> DragAction {
        DragAction(0)
    }
    pub fn contains(&self, o: DragAction) -> bool {
        self.0 & o.0 == o.0
    }
}
impl std::ops::BitOr for DragAction {
    type Output = DragAction;
    fn bitor(self, o: DragAction) -> DragAction {
        DragAction(self.0 | o.0)
    }
}

/// gdk::FileList: dropped files.
#[derive(Debug, Clone, Default)]
pub struct FileList(pub Vec<std::path::PathBuf>);
impl FileList {
    pub fn files(&self) -> Vec<crate::gio::File> {
        self.0.iter().map(|p| crate::gio::File::for_path(p)).collect()
    }
}

type DropFn = Rc<dyn Fn(&DropTarget, &crate::Value, f64, f64) -> bool>;
type DropMotionFn = Rc<dyn Fn(&DropTarget, f64, f64) -> DragAction>;
type DropLeaveFn = Rc<dyn Fn(&DropTarget)>;

#[derive(Default)]
struct DropInner {
    drop: RefCell<Vec<DropFn>>,
    hover: RefCell<Vec<Rc<dyn Fn(&DropTarget, bool)>>>,
    motion: RefCell<Vec<DropMotionFn>>,
    leave: RefCell<Vec<DropLeaveFn>>,
    files: Cell<bool>,
}

/// GtkDropTarget: files dropped from the desktop arrive as a FileList;
/// values dragged within the application (a DragSource's) as themselves.
#[derive(Clone, Default)]
pub struct DropTarget(Rc<DropInner>);
impl DropTarget {
    /// gtk-rs: `DropTarget::new(FileList::static_type(), DragAction::COPY)`
    /// takes files; any other type, the application's own drags.
    pub fn new<T: 'static, A>(_t: T, _a: A) -> DropTarget {
        let t = DropTarget::default();
        t.0.files.set(std::any::TypeId::of::<T>() == std::any::TypeId::of::<FileTypeMarker>());
        t
    }
    pub fn connect_drop<F: Fn(&DropTarget, &crate::Value, f64, f64) -> bool + 'static>(&self, f: F) {
        self.0.drop.borrow_mut().push(Rc::new(f));
    }
    /// A file drag over the widget, or gone.
    pub fn connect_hover<F: Fn(&DropTarget, bool) + 'static>(&self, f: F) {
        self.0.hover.borrow_mut().push(Rc::new(f));
    }
    /// An application drag moving over the widget: the action it would take.
    pub fn connect_motion<F: Fn(&DropTarget, f64, f64) -> DragAction + 'static>(&self, f: F) {
        self.0.motion.borrow_mut().push(Rc::new(f));
    }
    pub fn connect_leave<F: Fn(&DropTarget) + 'static>(&self, f: F) {
        self.0.leave.borrow_mut().push(Rc::new(f));
    }
    pub(crate) fn motion(&self, x: f64, y: f64) {
        let hs: Vec<DropMotionFn> = self.0.motion.borrow().clone();
        for h in hs {
            h(self, x, y);
        }
    }
    pub(crate) fn leave(&self) {
        let hs: Vec<DropLeaveFn> = self.0.leave.borrow().clone();
        for h in hs {
            h(self);
        }
    }
    pub(crate) fn drop_value(&self, v: &crate::Value, x: f64, y: f64) -> bool {
        let hs: Vec<DropFn> = self.0.drop.borrow().clone();
        hs.iter().any(|h| h(self, v, x, y))
    }
}

/// The type `FileList::static_type()` names.
pub struct FileTypeMarker;
impl FileList {
    pub fn static_type() -> FileTypeMarker {
        FileTypeMarker
    }
}

impl crate::widget::Controller for DropTarget {
    fn attach(&self, widget: &Widget) {
        if !self.0.files.get() {
            widget.set_rust_data("drop-target", self.clone());
            return;
        }
        widget.set_accepts_file_drops(true);
        let me = self.clone();
        widget.connect_raw(sig::FILES_DROPPED, move |_, a| {
            let files = FileList(
                a.text().unwrap_or_default().lines().filter(|l| !l.is_empty()).map(std::path::PathBuf::from).collect(),
            );
            me.drop_value(&crate::Value::new(files), a.x(), a.y());
        });
        let me = self.clone();
        widget.connect_raw(sig::DROP_HOVER, move |_, a| {
            let hs: Vec<Rc<dyn Fn(&DropTarget, bool)>> = me.0.hover.borrow().clone();
            for h in hs {
                h(&me, a.active());
            }
        });
    }
}

// ===========================================================================
// gio: files, apps, cancellables

/// gio::File: a path.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct File(std::path::PathBuf);
impl File {
    pub fn for_path(p: impl AsRef<std::path::Path>) -> File {
        File(p.as_ref().to_path_buf())
    }
    pub fn path(&self) -> Option<std::path::PathBuf> {
        Some(self.0.clone())
    }
    pub fn uri(&self) -> String {
        format!("file://{}", self.0.display())
    }
}

/// gio::Cancellable (nothing to cancel: dialogs answer once).
#[derive(Debug, Clone, Default)]
pub struct Cancellable;
impl Cancellable {
    pub fn new() -> Cancellable {
        Cancellable
    }
    pub fn cancel(&self) {}
}

/// gio::AppInfo: opening URIs with the desktop's handler.
pub struct AppInfo;
impl AppInfo {
    pub fn launch_default_for_uri(uri: &str, _ctx: Option<&AppLaunchContext>) -> Result<(), String> {
        if crate::show_uri(uri) {
            Ok(())
        } else {
            Err(format!("could not open {uri}"))
        }
    }
}
/// gio::AppLaunchContext.
#[derive(Debug, Clone, Default)]
pub struct AppLaunchContext;

/// gio::ApplicationFlags.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct ApplicationFlags(u32);
impl ApplicationFlags {
    pub const FLAGS_NONE: ApplicationFlags = ApplicationFlags(0);
    pub const NON_UNIQUE: ApplicationFlags = ApplicationFlags(32);
}

/// Keeps the application running with no window (gio's hold).
#[derive(Debug)]
pub struct ApplicationHoldGuard(());
impl Drop for ApplicationHoldGuard {
    fn drop(&mut self) {
        HOLDS.with(|h| {
            h.set(h.get().saturating_sub(1));
            if h.get() == 0 {
                crate::set_keep_alive(false);
            }
        });
    }
}

thread_local! {
    static HOLDS: Cell<u32> = const { Cell::new(0) };
}

impl crate::gio::Application {
    pub fn hold(&self) -> ApplicationHoldGuard {
        HOLDS.with(|h| h.set(h.get() + 1));
        crate::set_keep_alive(true);
        ApplicationHoldGuard(())
    }
}

// ===========================================================================
// Overlays placing their children

type PositionFn = Box<dyn Fn(&crate::Overlay, &Widget) -> Option<crate::Rectangle>>;

unsafe extern "C" fn position_tramp(
    o: *mut ffi::AwraWidget,
    c: *mut ffi::AwraWidget,
    x: *mut i32,
    y: *mut i32,
    w: *mut i32,
    h: *mut i32,
    data: *mut c_void,
) -> bool {
    let f = &*(data as *const PositionFn);
    let overlay = std::mem::ManuallyDrop::new(Widget::from_full(o));
    let child = std::mem::ManuallyDrop::new(Widget::from_full(c));
    let overlay: crate::Overlay = (*overlay).clone().downcast().expect("an overlay");
    match f(&overlay, &child) {
        Some(r) => {
            *x = r.x;
            *y = r.y;
            *w = r.width;
            *h = r.height;
            true
        }
        None => false,
    }
}
unsafe extern "C" fn position_free(data: *mut c_void) {
    drop(Box::from_raw(data as *mut PositionFn));
}

impl crate::Overlay {
    /// Places overlay children itself (GtkOverlay::get-child-position).
    pub fn connect_get_child_position<F: Fn(&crate::Overlay, &Widget) -> Option<crate::Rectangle> + 'static>(&self, f: F) {
        let data: Box<PositionFn> = Box::new(Box::new(f));
        unsafe {
            ffi::awra_overlay_set_position_func(
                self.as_ptr(),
                Some(position_tramp),
                Box::into_raw(data) as *mut c_void,
                Some(position_free),
            )
        }
    }
}

impl crate::Rectangle {
    pub fn x(&self) -> i32 {
        self.x
    }
    pub fn y(&self) -> i32 {
        self.y
    }
    pub fn width(&self) -> i32 {
        self.width
    }
    pub fn height(&self) -> i32 {
        self.height
    }
}

/// gtk-rs's Propagation from a bool (stop).
pub fn propagation(stop: bool) -> Propagation {
    if stop {
        Propagation::Stop
    } else {
        Propagation::Proceed
    }
}
