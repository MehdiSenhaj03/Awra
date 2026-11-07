//! More of gtk-rs's and Awra 1's API over Awra 2, for applications ported
//! from them: editables, label and tooltip notifications, in-application
//! drag and drop, the application's lifecycle and assorted getters.

use std::any::Any;
use std::cell::{Cell, RefCell};
use std::rc::Rc;

use crate::compat::{Paintable, Texture};
use crate::enums::*;
use crate::ffi;
use crate::object::{cstring, from_cstr_opt, IntoOptWidget, IsWidget, StaticType, Widget};
use crate::signal::{sig, SignalHandlerId};

// ===========================================================================
// Spacing

/// What sets an inset: a spacing token, or pixels (0: none).
pub trait IntoSpacing {
    fn into_spacing(self) -> Spacing;
}
impl IntoSpacing for Spacing {
    fn into_spacing(self) -> Spacing {
        self
    }
}
impl IntoSpacing for i32 {
    fn into_spacing(self) -> Spacing {
        match self {
            i32::MIN..=0 => Spacing::None,
            1..=5 => Spacing::Xs,
            6..=9 => Spacing::Sm,
            10..=13 => Spacing::Md,
            14..=19 => Spacing::Lg,
            20..=27 => Spacing::Xl,
            _ => Spacing::Xxl,
        }
    }
}

impl Spacing {
    /// Its size in logical pixels for `widget` (awra_spacing_resolve).
    pub fn resolve(_widget: &impl IsWidget, s: Spacing) -> i32 {
        crate::spacing(s).max(0)
    }
}

// ===========================================================================
// Editables: every entry

crate::awra_wrapper!(
    /// GtkEditable: any entry.
    Editable, ["AwraEntry"]
);

impl Editable {
    pub fn text(&self) -> String {
        unsafe { crate::object::from_cstr(ffi::awra_entry_text(self.as_ptr())) }
    }
    pub fn set_text(&self, text: &str) {
        let t = cstring(text);
        unsafe { ffi::awra_entry_set_text(self.as_ptr(), t.as_ptr()) }
    }
    pub fn set_position(&self, p: i32) {
        unsafe { ffi::awra_entry_set_position(self.as_ptr(), p) }
    }
    pub fn position(&self) -> i32 {
        unsafe { ffi::awra_editable_cursor(self.as_ptr()) }
    }
    pub fn connect_changed<F: Fn(&Editable) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed::<Editable, _>(sig::CHANGED, move |w, _| f(w))
    }
    pub fn connect_text_notify<F: Fn(&Editable) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_changed(f)
    }
}

macro_rules! is_editable {
    ($($t:ty),*) => {$(
        impl AsRef<Editable> for $t {
            fn as_ref(&self) -> &Editable {
                // both are a Widget, transparently
                unsafe { &*(self as *const $t as *const Editable) }
            }
        }
    )*};
}
is_editable!(crate::Entry, crate::SearchEntry, crate::SpinButton);

// ===========================================================================
// Labels: change notifications, Ctrl+C, links

type LabelFn = Rc<dyn Fn(&crate::Label)>;

pub(crate) fn label_changed(label: &crate::Label) {
    let hs: Vec<LabelFn> = label.rust_data("label-notify").unwrap_or_default();
    for h in hs {
        h(label);
    }
}

thread_local! {
    static STOPPED: Cell<bool> = const { Cell::new(false) };
}

impl Widget {
    /// Stops the signal being handled now (gtk's
    /// g_signal_stop_emission_by_name), for the handlers that support it:
    /// copy-clipboard.
    pub fn stop_signal_emission_by_name(&self, _name: &str) {
        STOPPED.with(|s| s.set(true));
    }
}

impl crate::Label {
    /// Its text changed (gtk-rs notify::label).
    pub fn connect_label_notify<F: Fn(&crate::Label) + 'static>(&self, f: F) {
        let mut hs: Vec<LabelFn> = self.rust_data("label-notify").unwrap_or_default();
        hs.push(Rc::new(f));
        self.set_rust_data("label-notify", hs);
    }
    /// Ctrl+C on its selection; `stop_signal_emission_by_name` keeps the
    /// label from copying it itself.
    pub fn connect_copy_clipboard<F: Fn(&crate::Label) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed::<crate::Label, _>(sig::COPY_CLIPBOARD, move |w, a| {
            STOPPED.with(|s| s.set(false));
            f(w);
            if STOPPED.with(|s| s.replace(false)) {
                a.stop();
            }
        })
    }
    /// The link under the pointer.
    pub fn current_uri(&self) -> Option<String> {
        unsafe { from_cstr_opt(ffi::awra_label_current_uri(self.as_ptr())) }
    }
}

// ===========================================================================
// Tooltips computed when shown

/// What a query-tooltip handler fills.
#[derive(Default)]
pub struct Tooltip(RefCell<Option<String>>);
impl Tooltip {
    pub fn set_text(&self, text: Option<&str>) {
        *self.0.borrow_mut() = text.map(str::to_owned);
    }
    pub fn set_markup(&self, markup: Option<&str>) {
        self.set_text(markup)
    }
}

type QueryFn = Rc<dyn Fn(&Widget, i32, i32, bool, &Tooltip) -> bool>;

fn query_tooltip(w: &Widget) {
    let Some(f) = w.rust_data::<QueryFn>("query-tooltip") else {
        return;
    };
    let tip = Tooltip::default();
    let shown = f(w, 0, 0, false, &tip);
    let text = if shown { tip.0.borrow().clone() } else { None };
    if w.tooltip_text() != text {
        w.set_tooltip_text(text.as_deref());
    }
}

/// gtk-rs connect_query_tooltip, and trigger_tooltip_query.
pub trait TooltipExt: IsWidget {
    /// The tooltip is computed as the pointer comes over the widget.
    fn connect_query_tooltip<F: Fn(&Widget, i32, i32, bool, &Tooltip) -> bool + 'static>(&self, f: F) {
        let w = self.upcast_ref();
        w.set_rust_data::<QueryFn>("query-tooltip", Rc::new(f));
        if w.rust_data::<bool>("query-tooltip-hooked").is_none() {
            w.set_rust_data("query-tooltip-hooked", true);
            w.connect_raw(sig::ENTER, |w, _| query_tooltip(w));
        }
        query_tooltip(w);
    }
    /// Computes the tooltip again (it may have changed).
    fn trigger_tooltip_query(&self) {
        query_tooltip(self.upcast_ref())
    }
}
impl<T: IsWidget> TooltipExt for T {}

// ===========================================================================
// Text buffers, typed notifications, state

impl crate::TextBuffer {
    /// gtk-rs notify: "cursor-position" (and any change for `None`).
    pub fn connect_notify_local<F: Fn(&crate::TextBuffer, &str) + 'static>(&self, name: Option<&str>, f: F) -> SignalHandlerId {
        match name {
            Some("cursor-position") | None => self.connect_cursor_position_notify(move |b| f(b, "cursor-position")),
            Some(_) => self.connect_changed(move |b| f(b, "text")),
        }
    }
}

/// GtkStateFlags (those Awra knows).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct StateFlags(u32);
impl StateFlags {
    pub const NORMAL: StateFlags = StateFlags(0);
    pub const FOCUSED: StateFlags = StateFlags(1);
    pub const FOCUS_WITHIN: StateFlags = StateFlags(2);
    pub const INSENSITIVE: StateFlags = StateFlags(4);
    pub fn contains(&self, o: StateFlags) -> bool {
        self.0 & o.0 == o.0
    }
}

/// Typed notifications and what every widget reports, on anything that
/// is one.
pub trait WidgetExt3: IsWidget {
    /// gtk-rs connect_notify_local, the widget typed.
    fn connect_notify_local<F: Fn(&Self, &str) + 'static>(&self, name: Option<&str>, f: F) -> SignalHandlerId
    where
        Self: StaticType,
    {
        self.upcast_ref().connect_notify_local(name, move |w, n| {
            if let Ok(t) = w.clone().downcast::<Self>() {
                f(&t, n)
            }
        })
    }
    fn state_flags(&self) -> StateFlags {
        let w = self.upcast_ref();
        let mut f = 0;
        if unsafe { ffi::awra_widget_has_focus(w.as_ptr()) } {
            f |= 1 | 2;
        }
        if unsafe { ffi::awra_widget_focus_within(w.as_ptr()) } {
            f |= 2;
        }
        if !w.is_sensitive() {
            f |= 4;
        }
        StateFlags(f)
    }
    fn margin_top(&self) -> i32 {
        unsafe { ffi::awra_widget_margin(self.upcast_ref().as_ptr(), 0) }
    }
    fn margin_end(&self) -> i32 {
        unsafe { ffi::awra_widget_margin(self.upcast_ref().as_ptr(), 1) }
    }
    fn margin_bottom(&self) -> i32 {
        unsafe { ffi::awra_widget_margin(self.upcast_ref().as_ptr(), 2) }
    }
    fn margin_start(&self) -> i32 {
        unsafe { ffi::awra_widget_margin(self.upcast_ref().as_ptr(), 3) }
    }
    /// The clipboard of its window.
    fn clipboard(&self) -> crate::widget::Clipboard {
        self.upcast_ref().display().clipboard()
    }
    fn insert_action_group(&self, name: &str, group: Option<&crate::SimpleActionGroup>) {
        self.upcast_ref().insert_action_group(name, group)
    }
    /// A point of this widget in `target`'s coordinates.
    fn compute_point(&self, target: &impl IsWidget, p: &crate::graphene::Point) -> Option<crate::graphene::Point> {
        let (x, y) = self.upcast_ref().window_position()?;
        let (tx, ty) = target.upcast_ref().window_position()?;
        Some(crate::graphene::Point::new(p.x() + (x - tx) as f32, p.y() + (y - ty) as f32))
    }
}
impl<T: IsWidget> WidgetExt3 for T {}

pub mod graphene {
    //! graphene's point.
    #[derive(Debug, Clone, Copy, PartialEq, Default)]
    pub struct Point(f32, f32);
    impl Point {
        pub fn new(x: f32, y: f32) -> Point {
            Point(x, y)
        }
        pub fn x(&self) -> f32 {
            self.0
        }
        pub fn y(&self) -> f32 {
            self.1
        }
    }
}

// ===========================================================================
// Paintables: textures are paintables

/// Anything shown as a picture: a texture or a paintable (gtk-rs's
/// `IsA<gdk::Paintable>`).
pub trait IsPaintable {
    fn to_paintable(&self) -> Paintable;
}
impl IsPaintable for Paintable {
    fn to_paintable(&self) -> Paintable {
        self.clone()
    }
}
impl IsPaintable for Texture {
    fn to_paintable(&self) -> Paintable {
        Paintable::new(Some(self.clone()))
    }
}

impl Texture {
    /// gtk-rs `upcast`: the texture as a paintable.
    pub fn upcast<T: From<Texture>>(self) -> T {
        T::from(self)
    }
    /// Planar I420 frames (Y, U, V planes, rows packed).
    pub fn from_i420(width: i32, height: i32, data: &[u8], bt709: bool) -> Option<Texture> {
        let (w, h) = (width as usize, height as usize);
        if width <= 0 || height <= 0 || data.len() < w * h + 2 * (w / 2) * (h / 2) {
            return None;
        }
        unsafe { Texture::from_raw(ffi::awra_texture_new_i420(data.as_ptr(), width, height, bt709)) }
    }
}

impl Paintable {
    pub fn intrinsic_aspect_ratio(&self) -> f64 {
        let (w, h) = (self.intrinsic_width(), self.intrinsic_height());
        if w > 0 && h > 0 {
            w as f64 / h as f64
        } else {
            0.0
        }
    }
}

/// A widget's picture now (GtkWidgetPaintable): Awra draws no drag
/// images, so it shows nothing.
pub struct WidgetPaintable;
impl WidgetPaintable {
    pub fn new(_w: Option<&impl IsWidget>) -> WidgetPaintable {
        WidgetPaintable
    }
    pub fn current_image(&self) -> Paintable {
        Paintable::new(None)
    }
}

// ===========================================================================
// Values, and drag and drop within the application

/// A glib::Value: any value (in-application drags carry these).
#[derive(Clone)]
pub struct Value(Rc<dyn Any>);
impl Value {
    pub fn new<T: Any>(v: T) -> Value {
        Value(Rc::new(v))
    }
    pub fn get<T: Any + Clone>(&self) -> Result<T, ()> {
        self.0.downcast_ref::<T>().cloned().ok_or(())
    }
}
impl std::fmt::Debug for Value {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("Value")
    }
}

/// gtk-rs ToValue.
pub trait ToValue {
    fn to_value(&self) -> Value;
}
impl ToValue for String {
    fn to_value(&self) -> Value {
        Value::new(self.clone())
    }
}
impl ToValue for &str {
    fn to_value(&self) -> Value {
        Value::new(self.to_string())
    }
}

/// What a drag carries (GdkContentProvider).
#[derive(Clone, Debug)]
pub struct ContentProvider(pub(crate) Value);
impl ContentProvider {
    pub fn for_value(v: &Value) -> ContentProvider {
        ContentProvider(v.clone())
    }
}

/// A drag in progress (GdkDrag).
#[derive(Clone, Debug, Default)]
pub struct Drag;

struct Dragging {
    value: Value,
    source: DragSource,
    widget: Widget,
    /// The destination under the pointer, and whether it takes the drag.
    over: Option<(Widget, crate::DropTarget, bool)>,
    /// Escape on the window cancels the drag (GTK).
    escape: Option<(Widget, crate::SignalHandlerId)>,
}

thread_local! {
    static DRAGGING: RefCell<Option<Dragging>> = const { RefCell::new(None) };
}

type PrepareFn = Rc<dyn Fn(&DragSource, f64, f64) -> Option<ContentProvider>>;
type BeginFn = Rc<dyn Fn(&DragSource, &Drag)>;
type EndFn = Rc<dyn Fn(&DragSource, &Drag, bool)>;

#[derive(Default)]
struct SourceInner {
    prepare: RefCell<Option<PrepareFn>>,
    begin: RefCell<Vec<BeginFn>>,
    end: RefCell<Vec<EndFn>>,
    widget: RefCell<Option<crate::WeakRef<Widget>>>,
    press: Cell<Option<(f64, f64)>>,
    /// A drag cancelled with Escape: its release clicks nothing.
    swallow_release: Cell<bool>,
}

/// GtkDragSource, within the application: a press and a move beyond a
/// few pixels start a drag; drop targets under the pointer take it. The
/// destination under the pointer shows GTK's :drop(active) ring, the
/// pointer says when nothing there takes the drag, and Escape cancels it.
#[derive(Clone, Default)]
pub struct DragSource(Rc<SourceInner>);

impl DragSource {
    pub fn new() -> DragSource {
        DragSource::default()
    }
    pub fn set_actions(&self, _a: crate::DragAction) {}
    pub fn set_icon(&self, _p: Option<&Paintable>, _x: i32, _y: i32) {}
    pub fn widget(&self) -> Option<Widget> {
        self.0.widget.borrow().as_ref().and_then(|w| w.upgrade())
    }
    pub fn connect_prepare<F: Fn(&DragSource, f64, f64) -> Option<ContentProvider> + 'static>(&self, f: F) {
        *self.0.prepare.borrow_mut() = Some(Rc::new(f));
    }
    pub fn connect_drag_begin<F: Fn(&DragSource, &Drag) + 'static>(&self, f: F) {
        self.0.begin.borrow_mut().push(Rc::new(f));
    }
    pub fn connect_drag_end<F: Fn(&DragSource, &Drag, bool) + 'static>(&self, f: F) {
        self.0.end.borrow_mut().push(Rc::new(f));
    }
    /// Whether a drag from this source is under way.
    pub fn is_dragging(&self) -> bool {
        DRAGGING.with(|d| d.borrow().as_ref().is_some_and(|d| Rc::ptr_eq(&d.source.0, &self.0)))
    }
}

/// The drop target nearest the point (window coordinates) under the pointer.
fn target_at(source: &Widget, wx: f64, wy: f64) -> Option<(Widget, crate::DropTarget)> {
    let root = source.root_widget()?;
    let mut w = root.pick(wx, wy, crate::PickFlags::DEFAULT);
    while let Some(cur) = w {
        if let Some(t) = cur.rust_data::<crate::DropTarget>("drop-target") {
            return Some((cur, t));
        }
        w = cur.parent();
    }
    None
}

/// The drag left `over`'s destination.
fn leave_target(over: Option<(Widget, crate::DropTarget, bool)>) {
    if let Some((w, t, _)) = over {
        w.set_drop_active(false);
        t.leave();
    }
}

fn drag_motion(source: &Widget, x: f64, y: f64) {
    let Some((sx, sy)) = source.window_position() else { return };
    let (wx, wy) = (sx + x, sy + y);
    let found = target_at(source, wx, wy);
    let old = DRAGGING.with(|d| d.borrow_mut().as_mut().and_then(|d| d.over.take()));
    if old.as_ref().is_some_and(|(ow, _, _)| found.as_ref().is_none_or(|(w, _)| w != ow)) {
        leave_target(old);
    }
    let over = found.map(|(w, t)| {
        let (tx, ty) = w.window_position().unwrap_or((0.0, 0.0));
        let accepted = t.motion(wx - tx, wy - ty);
        w.set_drop_active(accepted);
        (w, t, accepted)
    });
    // GTK's drag cursor without a destination ("no-drop")
    let accepted = over.as_ref().is_some_and(|o| o.2);
    source.set_cursor_from_name(if accepted { None } else { Some("not-allowed") });
    motion_controllers(source, wx, wy);
    DRAGGING.with(|d| {
        if let Some(d) = d.borrow_mut().as_mut() {
            d.over = over;
        }
    });
}

/// Ends the drag in progress: dropped at (x, y) of `source`, or cancelled.
fn drag_end(drop_at: Option<(f64, f64)>) {
    let Some(mut d) = DRAGGING.with(|d| d.borrow_mut().take()) else { return };
    let mut dropped = false;
    let over = d.over.take();
    if let (Some((x, y)), Some((sx, sy))) = (drop_at, d.widget.window_position()) {
        if let Some((w, t)) = target_at(&d.widget, sx + x, sy + y) {
            if over.as_ref().is_none_or(|(ow, _, ok)| ow != &w || *ok) {
                let (tx, ty) = w.window_position().unwrap_or((0.0, 0.0));
                dropped = t.drop_value(&d.value, sx + x - tx, sy + y - ty);
            }
        }
    }
    leave_target(over);
    d.widget.set_cursor_from_name(None);
    if let Some((win, id)) = d.escape.take() {
        win.disconnect(id);
    }
    end_motion_controllers();
    let ends: Vec<EndFn> = d.source.0.end.borrow().clone();
    for e in ends {
        e(&d.source, &Drag, dropped);
    }
}

impl crate::widget::Controller for DragSource {
    fn attach(&self, widget: &Widget) {
        *self.0.widget.borrow_mut() = Some(widget.downgrade());
        let me = self.clone();
        widget.connect_raw(sig::PRESSED, move |_, a| {
            me.0.swallow_release.set(false);
            me.0.press.set((a.button() == 1).then(|| (a.x(), a.y())));
        });
        let me = self.clone();
        widget.connect_raw(sig::MOTION, move |w, a| {
            let Some((px, py)) = me.0.press.get() else { return };
            if me.is_dragging() {
                drag_motion(w, a.x(), a.y());
                return;
            }
            if (a.x() - px).hypot(a.y() - py) < 8.0 {
                return;
            }
            me.0.press.set(None);
            let prepare = me.0.prepare.borrow().clone();
            let Some(content) = prepare.and_then(|p| p(&me, px, py)) else { return };
            // a drag already under way (another source) ends first
            drag_end(None);
            let escape = w.root_widget().map(|win| {
                let id = win.connect_raw(sig::KEY_CAPTURE, |_, a| {
                    if a.keysym().as_deref() == Some("Escape") {
                        let source = DRAGGING.with(|d| d.borrow().as_ref().map(|d| d.source.clone()));
                        if let Some(s) = source {
                            s.0.swallow_release.set(true);
                            s.0.press.set(None);
                            drag_end(None);
                            a.stop();
                        }
                    }
                });
                (win, id)
            });
            DRAGGING.with(|d| {
                *d.borrow_mut() =
                    Some(Dragging { value: content.0, source: me.clone(), widget: w.clone(), over: None, escape })
            });
            me.0.press.set(Some((px, py)));
            let begins: Vec<BeginFn> = me.0.begin.borrow().clone();
            for b in begins {
                b(&me, &Drag);
            }
            drag_motion(w, a.x(), a.y());
        });
        let me = self.clone();
        widget.connect_raw(sig::RELEASED, move |_, a| {
            me.0.press.set(None);
            if me.is_dragging() {
                drag_end(Some((a.x(), a.y())));
                a.stop();
            } else if me.0.swallow_release.replace(false) {
                a.stop();
            }
        });
    }
}

type MotionFn = Rc<dyn Fn(&DropControllerMotion, f64, f64)>;
type LeaveFn = Rc<dyn Fn(&DropControllerMotion)>;

#[derive(Default)]
struct DropMotionInner {
    motion: RefCell<Vec<MotionFn>>,
    leave: RefCell<Vec<LeaveFn>>,
    widget: RefCell<Option<crate::WeakRef<Widget>>>,
    inside: Cell<bool>,
}

/// GtkDropControllerMotion: a drag moving over a widget.
#[derive(Clone, Default)]
pub struct DropControllerMotion(Rc<DropMotionInner>);

thread_local! {
    static DROP_MOTIONS: RefCell<Vec<DropControllerMotion>> = const { RefCell::new(Vec::new()) };
}

impl DropControllerMotion {
    pub fn new() -> DropControllerMotion {
        DropControllerMotion::default()
    }
    pub fn connect_motion<F: Fn(&DropControllerMotion, f64, f64) + 'static>(&self, f: F) {
        self.0.motion.borrow_mut().push(Rc::new(f));
    }
    pub fn connect_leave<F: Fn(&DropControllerMotion) + 'static>(&self, f: F) {
        self.0.leave.borrow_mut().push(Rc::new(f));
    }
    fn left(&self) {
        if self.0.inside.replace(false) {
            let hs: Vec<LeaveFn> = self.0.leave.borrow().clone();
            for h in hs {
                h(self);
            }
        }
    }
}

impl crate::widget::Controller for DropControllerMotion {
    fn attach(&self, widget: &Widget) {
        *self.0.widget.borrow_mut() = Some(widget.downgrade());
        DROP_MOTIONS.with(|m| m.borrow_mut().push(self.clone()));
    }
}

/// The drag moved to (wx, wy) in the window: motion controllers under it
/// hear of it, those it left hear that.
fn motion_controllers(source: &Widget, wx: f64, wy: f64) {
    let ms: Vec<DropControllerMotion> = DROP_MOTIONS.with(|m| {
        let mut m = m.borrow_mut();
        m.retain(|c| c.0.widget.borrow().as_ref().is_some_and(|w| w.upgrade().is_some()));
        m.clone()
    });
    let root = source.root_widget();
    for c in ms {
        let Some(w) = c.0.widget.borrow().as_ref().and_then(|w| w.upgrade()) else { continue };
        if w.root_widget() != root || !w.is_mapped() {
            continue;
        }
        let Some((x, y)) = w.window_position() else { continue };
        let (lx, ly) = (wx - x, wy - y);
        if lx >= 0.0 && ly >= 0.0 && lx < w.width() as f64 && ly < w.height() as f64 {
            c.0.inside.set(true);
            let hs: Vec<MotionFn> = c.0.motion.borrow().clone();
            for h in hs {
                h(&c, lx, ly);
            }
        } else {
            c.left();
        }
    }
}

fn end_motion_controllers() {
    let ms: Vec<DropControllerMotion> = DROP_MOTIONS.with(|m| m.borrow().clone());
    for c in ms {
        c.left();
    }
}

// ===========================================================================
// Chrome

impl crate::Header {
    /// The material it paints when it doesn't blend with the window.
    pub fn set_material(&self, m: impl std::borrow::Borrow<Material>) {
        unsafe { ffi::awra_header_set_material(self.as_ptr(), *m.borrow() as _) }
    }
}

impl crate::Window {
    /// The desktop names the window's icon from the application id.
    pub fn set_icon_name(&self, _name: Option<&str>) {}
    /// Awra shows focus rings for keyboard use only, by itself.
    pub fn set_focus_visible(&self, _v: bool) {}
    pub fn set_startup_id(&self, _id: &str) {}
}

/// gtk-rs GtkWindowExt (for the calls that name it).
pub trait GtkWindowExt {
    fn is_active(&self) -> bool;
}
impl GtkWindowExt for crate::Window {
    fn is_active(&self) -> bool {
        crate::Window::is_active(self)
    }
}

impl crate::Surface {
    pub fn set_elevation_level(&self, e: Elevation) {
        self.set_elevation(e)
    }
    /// Its children are clipped to its shape.
    pub fn set_clip_child(&self, clip: bool) {
        unsafe { ffi::awra_widget_set_overflow_hidden(self.as_ptr(), clip) }
    }
}

impl crate::OverlayBar {
    pub fn connect_map<F: Fn(&crate::OverlayBar) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed::<crate::OverlayBar, _>(sig::MAP, move |w, _| f(w))
    }
}

impl crate::Section {
    /// Widgets at the end of its heading.
    pub fn set_actions(&self, w: impl IntoOptWidget) {
        self.set_header_suffix(w)
    }
}

impl crate::DataView {
    /// A "Loading more…" row under the rows.
    pub fn set_loading_more(&self, loading: bool) {
        unsafe { ffi::awra_data_view_set_loading_more(self.as_ptr(), loading) }
    }
}

impl crate::Button {
    /// An icon then a label.
    pub fn with_icon_and_label(icon: &str, label: &str) -> crate::Button {
        let b = crate::Button::with_label(label);
        b.set_icon_name(icon);
        b
    }
}

impl crate::Slider {
    /// A tick under the trough at `value` (its label is not shown).
    pub fn add_mark(&self, value: f64, _position: PositionType, _markup: Option<&str>) {
        unsafe { ffi::awra_slider_add_mark(self.as_ptr(), value) }
    }
    pub fn clear_marks(&self) {
        unsafe { ffi::awra_slider_clear_marks(self.as_ptr()) }
    }
    /// The step rounds values already.
    pub fn set_round_digits(&self, _digits: i32) {}
}

impl crate::VolumeControl {
    pub fn connect_muted_notify<F: Fn(&crate::VolumeControl) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed::<crate::VolumeControl, _>(sig::TOGGLED, move |w, _| f(w))
    }
}

pub mod accessible {
    //! GtkAccessible properties (Awra 2 has no accessibility tree yet).
    pub enum Property<'a> {
        Label(&'a str),
        Description(&'a str),
    }
}

impl Widget {
    /// Accessible properties (no accessibility tree yet: kept nowhere).
    pub fn update_property(&self, _props: &[accessible::Property]) {}
}

// ===========================================================================
// Shortcut captures by keys

impl crate::ShortcutCapture {
    /// Shows these keys (GDK hardware keycodes) as the shortcut.
    pub fn set_shortcut(&self, keycodes: &[u32], _mods: crate::gdk::ModifierType) {
        let names: Vec<String> = keycodes.iter().filter_map(|&k| crate::keycode_name(k)).collect();
        let cs: Vec<_> = names.iter().map(|n| cstring(n)).collect();
        let ps: Vec<*const std::ffi::c_char> = cs.iter().map(|c| c.as_ptr()).collect();
        unsafe { ffi::awra_shortcut_capture_set_codes(self.as_ptr(), ps.as_ptr(), ps.len() as i32) }
    }
    /// The keys the last capture typed.
    pub fn keyvals(&self) -> Vec<crate::gdk::Key> {
        let mut p: *const *const std::ffi::c_char = std::ptr::null();
        let n = unsafe { ffi::awra_shortcut_capture_keysyms(self.as_ptr(), &mut p) };
        (0..n.max(0) as usize)
            .map(|i| crate::gdk::Key::from_name(&unsafe { from_cstr_opt(*p.add(i)) }.unwrap_or_default()))
            .collect()
    }
    /// The physical keys of the last capture, as GDK hardware keycodes.
    pub fn keycodes(&self) -> Vec<u32> {
        let mut p: *const *const std::ffi::c_char = std::ptr::null();
        let n = unsafe { ffi::awra_shortcut_capture_codes(self.as_ptr(), &mut p) };
        (0..n.max(0) as usize)
            .map(|i| crate::hardware_keycode(&unsafe { from_cstr_opt(*p.add(i)) }.unwrap_or_default()))
            .collect()
    }
}

// ===========================================================================
// Dropdowns of string lists

/// GtkExpression (dropdowns of strings need none).
pub struct Expression;

use crate::compat::StringObject;

impl crate::StringList {
    pub fn item(&self, i: u32) -> Option<StringObject> {
        self.string(i).map(StringObject)
    }
}

/// gtk-rs `and_downcast` on a string list's items.
pub trait StringObjectDowncast {
    fn and_downcast<T: From<StringObject>>(self) -> Option<T>;
}
impl StringObjectDowncast for Option<StringObject> {
    fn and_downcast<T: From<StringObject>>(self) -> Option<T> {
        self.map(T::from)
    }
}

impl crate::Dropdown {
    /// A dropdown of a string list's strings.
    pub fn new(model: Option<&crate::StringList>, _expression: Option<Expression>) -> crate::Dropdown {
        let d = crate::Dropdown::from_strings(&[]);
        d.set_model(model);
        d
    }
    /// The strings it offers.
    pub fn model(&self) -> Option<crate::StringList> {
        self.rust_data::<crate::StringList>("p:model")
    }
}

// ===========================================================================
// Command palettes: commands by name

type CommandFn = Rc<dyn Fn(&crate::CommandPalette, &str, &crate::gio::Variant) -> bool>;

impl crate::CommandPalette {
    /// A command chosen: `f` gets its action's name and target, and
    /// returns true when it handled it (else the action is activated).
    pub fn connect_activate_command<F: Fn(&crate::CommandPalette, &str, &crate::gio::Variant) -> bool + 'static>(&self, f: F) {
        self.set_rust_data::<CommandFn>("palette-command", Rc::new(f));
    }
    pub fn set_query(&self, query: &str) {
        let q = cstring(query);
        unsafe { ffi::awra_command_palette_set_query(self.as_ptr(), q.as_ptr()) }
    }
}

pub(crate) fn palette_command(w: &Widget, action: &str, target: Option<&crate::gio::Variant>) -> bool {
    let Some(f) = w.rust_data::<CommandFn>("palette-command") else {
        return false;
    };
    let palette: crate::CommandPalette = unsafe { crate::CommandPalette::unchecked_from(w.clone()) };
    let name = action.split_once('.').map_or(action, |(_, n)| n);
    let empty = crate::gio::Variant::default();
    f(&palette, name, target.unwrap_or(&empty))
}

// ===========================================================================
// The application's lifecycle

type ActivateFn = Rc<dyn Fn(&crate::Application)>;

thread_local! {
    static ACTIVATE: RefCell<Vec<ActivateFn>> = const { RefCell::new(Vec::new()) };
}

impl crate::Application {
    /// The application runs `f` once its loop starts (GApplication's
    /// activate).
    pub fn connect_activate<F: Fn(&crate::Application) + 'static>(&self, f: F) {
        ACTIVATE.with(|a| a.borrow_mut().push(Rc::new(f)));
    }
    /// Activates, then runs the loop until the last window closes or
    /// `quit` (the arguments are the application's own to read).
    pub fn run_with_args<S: AsRef<str>>(&self, _args: &[S]) -> i32 {
        let hs: Vec<ActivateFn> = ACTIVATE.with(|a| a.borrow().clone());
        for h in hs {
            h(self);
        }
        crate::run();
        0
    }
    /// gtk-rs downcast_ref: the application is an Awra one already.
    pub fn downcast_ref<T: Any>(&self) -> Option<&T> {
        (self as &dyn Any).downcast_ref::<T>()
    }
    /// The window that last had the focus, or any.
    pub fn active_window(&self) -> Option<crate::Window> {
        let ws = self.windows();
        ws.iter().find(|w| w.is_active()).cloned().or_else(|| ws.into_iter().next())
    }
}

impl crate::gio::AppInfo {
    /// Opens `uri` with the desktop's handler; `done` hears whether it could.
    pub fn launch_default_for_uri_async<F: FnOnce(Result<(), String>) + 'static>(
        uri: &str,
        ctx: Option<&crate::gio::AppLaunchContext>,
        _cancellable: Option<&crate::gio::Cancellable>,
        done: F,
    ) {
        done(crate::gio::AppInfo::launch_default_for_uri(uri, ctx))
    }
}

impl crate::StyleManager {
    /// A custom accent.
    pub fn set_accent(&self, c: &crate::gdk::RGBA) {
        let v = c.to_ffi();
        unsafe { ffi::awra_style_set_accent(&v) }
    }
    /// The colours of the appearance in use.
    pub fn token_set(&self) -> TokenSet {
        TokenSet(crate::StyleManager)
    }
}

/// Awra 1's AwraTokenSet: the appearance's colours.
pub struct TokenSet(crate::StyleManager);
impl TokenSet {
    fn named(&self, n: &str) -> crate::gdk::RGBA {
        self.0.color(n).unwrap_or_default()
    }
    pub fn danger(&self) -> crate::gdk::RGBA {
        self.named("danger")
    }
    pub fn success(&self) -> crate::gdk::RGBA {
        self.named("success")
    }
    pub fn warning(&self) -> crate::gdk::RGBA {
        self.named("warning")
    }
    pub fn accent(&self) -> crate::gdk::RGBA {
        self.named("accent")
    }
    pub fn muted(&self) -> crate::gdk::RGBA {
        self.named("muted")
    }
    pub fn foreground(&self) -> crate::gdk::RGBA {
        self.named("foreground")
    }
}

// ===========================================================================
// Drawing areas, with the bit of cairo applications draw them with

/// What a drawing function draws with: a few of cairo's calls over Awra's
/// canvas (a source colour, rectangles and full circles, fill and paint).
pub struct CairoContext {
    canvas: *mut ffi::AwraCanvas,
    width: f64,
    height: f64,
    source: Cell<ffi::AwraRGBA>,
    path: RefCell<Vec<PathPart>>,
}

#[derive(Clone, Copy)]
enum PathPart {
    Rect(f64, f64, f64, f64),
    Circle(f64, f64, f64),
}

impl CairoContext {
    pub fn set_source_rgb(&self, r: f64, g: f64, b: f64) {
        self.set_source_rgba(r, g, b, 1.0)
    }
    pub fn set_source_rgba(&self, r: f64, g: f64, b: f64, a: f64) {
        self.source.set(ffi::AwraRGBA { r: r as f32, g: g as f32, b: b as f32, a: a as f32 })
    }
    pub fn rectangle(&self, x: f64, y: f64, w: f64, h: f64) {
        self.path.borrow_mut().push(PathPart::Rect(x, y, w, h))
    }
    /// Arcs are drawn as whole circles.
    pub fn arc(&self, cx: f64, cy: f64, r: f64, _a0: f64, _a1: f64) {
        self.path.borrow_mut().push(PathPart::Circle(cx, cy, r))
    }
    pub fn fill(&self) -> Result<(), ()> {
        let c = self.source.get();
        for p in self.path.borrow_mut().drain(..) {
            match p {
                PathPart::Rect(x, y, w, h) => unsafe { ffi::awra_canvas_fill_rect(self.canvas, x, y, w, h, &c) },
                PathPart::Circle(x, y, r) => unsafe { ffi::awra_canvas_circle(self.canvas, x, y, r, &c) },
            }
        }
        Ok(())
    }
    pub fn paint(&self) -> Result<(), ()> {
        let c = self.source.get();
        unsafe { ffi::awra_canvas_fill_rect(self.canvas, 0.0, 0.0, self.width, self.height, &c) };
        Ok(())
    }
}

type DrawFn = Rc<dyn Fn(&DrawingArea, &CairoContext, i32, i32)>;
type DrawCell = RefCell<Option<DrawFn>>;

unsafe extern "C" fn area_draw(w: *mut ffi::AwraWidget, canvas: *mut ffi::AwraCanvas, width: i32, height: i32, data: *mut std::ffi::c_void) {
    let cell = &*(data as *const DrawCell);
    let Some(f) = cell.borrow().clone() else { return };
    let area = DrawingArea(Widget::from_none(w));
    let cr = CairoContext {
        canvas,
        width: width as f64,
        height: height as f64,
        source: Cell::new(ffi::AwraRGBA { r: 0.0, g: 0.0, b: 0.0, a: 1.0 }),
        path: RefCell::new(Vec::new()),
    };
    f(&area, &cr, width, height);
}

unsafe extern "C" fn area_free(data: *mut std::ffi::c_void) {
    drop(Box::from_raw(data as *mut DrawCell));
}

crate::awra_wrapper!(
    /// GtkDrawingArea.
    DrawingArea, ["AwraDrawingArea"]
);

impl DrawingArea {
    pub fn new() -> DrawingArea {
        let cell: Box<DrawCell> = Box::new(RefCell::new(None));
        let raw = Box::into_raw(cell);
        let w = unsafe { DrawingArea::from_full(ffi::awra_drawing_area_new(Some(area_draw), raw as *mut _, Some(area_free))) };
        w.set_rust_data("draw-cell", raw as usize);
        w
    }
    pub fn set_draw_func<F: Fn(&DrawingArea, &CairoContext, i32, i32) + 'static>(&self, f: F) {
        if let Some(raw) = self.rust_data::<usize>("draw-cell") {
            let cell = unsafe { &*(raw as *const DrawCell) };
            *cell.borrow_mut() = Some(Rc::new(f));
        }
        self.queue_draw();
    }
    fn content_size(&self) -> (i32, i32) {
        self.rust_data::<(i32, i32)>("p:content-size").unwrap_or((0, 0))
    }
    pub fn set_content_width(&self, width: i32) {
        let (_, h) = self.content_size();
        self.set_rust_data("p:content-size", (width, h));
        unsafe { ffi::awra_drawing_area_set_content_size(self.as_ptr(), width, h) }
    }
    pub fn set_content_height(&self, height: i32) {
        let (w, _) = self.content_size();
        self.set_rust_data("p:content-size", (w, height));
        unsafe { ffi::awra_drawing_area_set_content_size(self.as_ptr(), w, height) }
    }
}
impl Default for DrawingArea {
    fn default() -> Self {
        Self::new()
    }
}
