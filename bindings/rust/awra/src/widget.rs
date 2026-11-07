//! Methods of every widget (GtkWidget's), event controllers.

use std::any::Any;
use std::cell::RefCell;
use std::ffi::c_void;
use std::path::PathBuf;
use std::rc::Rc;

use crate::enums::*;
use crate::ffi;
use crate::glib::Propagation;
use crate::object::{cstring, from_cstr, from_cstr_opt, IntoOptStr, IsWidget, OptC, StaticType, Widget};
use crate::signal::{sig, SignalHandlerId};

unsafe extern "C" fn any_free(data: *mut c_void) {
    drop(Box::from_raw(data as *mut Box<dyn Any>));
}

impl Widget {
    // --- tree ---------------------------------------------------------------
    pub fn parent(&self) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_widget_parent(self.as_ptr())) }
    }
    /// The window holding the widget.
    pub fn root(&self) -> Option<crate::Window> {
        self.root_widget().and_then(|w| w.downcast().ok())
    }
    pub(crate) fn root_widget(&self) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_widget_root(self.as_ptr())) }
    }
    pub fn children(&self) -> Vec<Widget> {
        let n = unsafe { ffi::awra_widget_child_count(self.as_ptr()) };
        (0..n).filter_map(|i| unsafe { Widget::from_none_opt(ffi::awra_widget_child(self.as_ptr(), i)) }).collect()
    }
    pub fn first_child(&self) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_widget_child(self.as_ptr(), 0)) }
    }
    pub fn last_child(&self) -> Option<Widget> {
        let n = unsafe { ffi::awra_widget_child_count(self.as_ptr()) };
        if n == 0 {
            None
        } else {
            unsafe { Widget::from_none_opt(ffi::awra_widget_child(self.as_ptr(), n - 1)) }
        }
    }
    fn sibling(&self, delta: i32) -> Option<Widget> {
        let parent = self.parent()?;
        let n = unsafe { ffi::awra_widget_child_count(parent.as_ptr()) };
        for i in 0..n {
            if unsafe { ffi::awra_widget_child(parent.as_ptr(), i) } == self.as_ptr() {
                let j = i + delta;
                if j < 0 || j >= n {
                    return None;
                }
                return unsafe { Widget::from_none_opt(ffi::awra_widget_child(parent.as_ptr(), j)) };
            }
        }
        None
    }
    pub fn next_sibling(&self) -> Option<Widget> {
        self.sibling(1)
    }
    pub fn prev_sibling(&self) -> Option<Widget> {
        self.sibling(-1)
    }
    pub fn is_ancestor(&self, ancestor: &impl IsWidget) -> bool {
        let a = ancestor.widget_ptr();
        let mut p = unsafe { ffi::awra_widget_parent(self.as_ptr()) };
        while !p.is_null() {
            if p == a {
                return true;
            }
            p = unsafe { ffi::awra_widget_parent(p) };
        }
        false
    }
    /// Takes the widget out of its parent.
    pub fn unparent(&self) {
        if let Some(p) = self.parent() {
            unsafe { ffi::awra_container_remove(p.as_ptr(), self.as_ptr()) }
        }
    }
    /// Removes the widget from its parent; closes it when it is a window.
    pub fn destroy(&self) {
        unsafe { ffi::awra_widget_destroy(self.as_ptr()) }
    }
    /// The deepest widget at (x, y) of this widget's window.
    /// The deepest widget at a point of this one (gtk_widget_pick).
    pub fn pick(&self, x: f64, y: f64, _flags: crate::PickFlags) -> Option<Widget> {
        let root = self.root_widget().unwrap_or_else(|| self.clone());
        let (ox, oy) = self.window_position().unwrap_or((0.0, 0.0));
        unsafe { Widget::from_none_opt(ffi::awra_widget_pick(root.as_ptr(), x + ox, y + oy)) }
    }
    pub fn window_position(&self) -> Option<(f64, f64)> {
        let (mut x, mut y) = (0.0, 0.0);
        unsafe { ffi::awra_widget_position(self.as_ptr(), &mut x, &mut y) }.then_some((x, y))
    }

    // --- state --------------------------------------------------------------
    pub fn set_visible(&self, visible: bool) {
        unsafe { ffi::awra_widget_set_visible(self.as_ptr(), visible) }
    }
    pub fn is_visible(&self) -> bool {
        unsafe { ffi::awra_widget_visible(self.as_ptr()) }
    }
    pub fn is_mapped(&self) -> bool {
        unsafe { ffi::awra_widget_mapped(self.as_ptr()) }
    }
    pub fn is_drawable(&self) -> bool {
        self.is_mapped()
    }
    pub fn is_realized(&self) -> bool {
        self.root_widget().is_some()
    }
    pub fn realize(&self) {}
    pub fn set_sensitive(&self, sensitive: bool) {
        unsafe { ffi::awra_widget_set_sensitive(self.as_ptr(), sensitive) }
    }
    pub fn is_sensitive(&self) -> bool {
        unsafe { ffi::awra_widget_sensitive(self.as_ptr()) }
    }
    pub fn set_hexpand(&self, expand: bool) {
        unsafe { ffi::awra_widget_set_hexpand(self.as_ptr(), expand) }
    }
    pub fn set_vexpand(&self, expand: bool) {
        unsafe { ffi::awra_widget_set_vexpand(self.as_ptr(), expand) }
    }
    pub fn hexpands(&self) -> bool {
        unsafe { ffi::awra_widget_hexpand(self.as_ptr()) }
    }
    pub fn vexpands(&self) -> bool {
        unsafe { ffi::awra_widget_vexpand(self.as_ptr()) }
    }
    pub fn set_halign(&self, align: Align) {
        unsafe { ffi::awra_widget_set_halign(self.as_ptr(), align as _) }
    }
    pub fn set_valign(&self, align: Align) {
        unsafe { ffi::awra_widget_set_valign(self.as_ptr(), align as _) }
    }
    pub fn set_margin_top(&self, m: i32) {
        unsafe { ffi::awra_widget_set_margin_top(self.as_ptr(), m) }
    }
    pub fn set_margin_bottom(&self, m: i32) {
        unsafe { ffi::awra_widget_set_margin_bottom(self.as_ptr(), m) }
    }
    pub fn set_margin_start(&self, m: i32) {
        unsafe { ffi::awra_widget_set_margin_start(self.as_ptr(), m) }
    }
    pub fn set_margin_end(&self, m: i32) {
        unsafe { ffi::awra_widget_set_margin_end(self.as_ptr(), m) }
    }
    pub fn set_margins(&self, top: i32, end: i32, bottom: i32, start: i32) {
        unsafe { ffi::awra_widget_set_margins(self.as_ptr(), top, end, bottom, start) }
    }
    pub fn set_size_request(&self, width: i32, height: i32) {
        unsafe { ffi::awra_widget_set_size_request(self.as_ptr(), width, height) }
    }
    pub fn size_request(&self) -> (i32, i32) {
        let (mut w, mut h) = (-1, -1);
        unsafe { ffi::awra_widget_get_size_request(self.as_ptr(), &mut w, &mut h) };
        (w, h)
    }
    pub fn set_width_request(&self, width: i32) {
        let (_, h) = self.size_request();
        self.set_size_request(width, h);
    }
    pub fn set_height_request(&self, height: i32) {
        let (w, _) = self.size_request();
        self.set_size_request(w, height);
    }
    pub fn width_request(&self) -> i32 {
        self.size_request().0
    }
    pub fn height_request(&self) -> i32 {
        self.size_request().1
    }
    pub fn set_tooltip_text(&self, text: impl IntoOptStr) {
        let text = text.into_opt_string();
        self.set_rust_data("p:tooltip", text.clone());
        let t = OptC::new(text);
        unsafe { ffi::awra_widget_set_tooltip(self.as_ptr(), t.ptr()) }
    }
    pub fn set_tooltip_markup(&self, text: impl IntoOptStr) {
        self.set_tooltip_text(text)
    }
    pub fn set_opacity(&self, opacity: f64) {
        unsafe { ffi::awra_widget_set_opacity(self.as_ptr(), opacity) }
    }
    pub fn set_can_target(&self, can: bool) {
        unsafe { ffi::awra_widget_set_can_target(self.as_ptr(), can) }
    }
    pub fn set_focusable(&self, focusable: bool) {
        unsafe { ffi::awra_widget_set_focusable(self.as_ptr(), focusable) }
    }
    pub fn set_can_focus(&self, focusable: bool) {
        self.set_focusable(focusable)
    }
    pub fn set_focus_on_click(&self, _focus: bool) {}
    pub fn grab_focus(&self) -> bool {
        unsafe { ffi::awra_widget_grab_focus(self.as_ptr()) };
        self.has_focus()
    }
    pub fn has_focus(&self) -> bool {
        unsafe { ffi::awra_widget_has_focus(self.as_ptr()) }
    }
    pub fn set_cursor_from_name(&self, name: Option<&str>) {
        let n = OptC::new(name);
        unsafe { ffi::awra_widget_set_cursor(self.as_ptr(), n.ptr()) }
    }
    pub fn add_css_class(&self, class: &str) {
        let c = cstring(class);
        unsafe { ffi::awra_widget_add_class(self.as_ptr(), c.as_ptr()) }
    }
    pub fn remove_css_class(&self, class: &str) {
        let c = cstring(class);
        unsafe { ffi::awra_widget_remove_class(self.as_ptr(), c.as_ptr()) }
    }
    pub fn has_css_class(&self, class: &str) -> bool {
        let c = cstring(class);
        unsafe { ffi::awra_widget_has_class(self.as_ptr(), c.as_ptr()) }
    }
    pub fn set_css_classes(&self, classes: &[&str]) {
        for c in classes {
            self.add_css_class(c);
        }
    }
    /// (minimum, natural, -1, -1) in an orientation (gtk's measure).
    pub fn measure(&self, orientation: Orientation, for_size: i32) -> (i32, i32, i32, i32) {
        let (mut mn, mut nt) = (0, 0);
        unsafe { ffi::awra_widget_measure(self.as_ptr(), orientation as _, for_size, &mut mn, &mut nt) };
        (mn, nt, -1, -1)
    }
    pub fn width(&self) -> i32 {
        unsafe { ffi::awra_widget_width(self.as_ptr()) }
    }
    pub fn height(&self) -> i32 {
        unsafe { ffi::awra_widget_height(self.as_ptr()) }
    }
    pub fn allocated_width(&self) -> i32 {
        self.width()
    }
    pub fn allocated_height(&self) -> i32 {
        self.height()
    }
    pub fn queue_draw(&self) {
        unsafe { ffi::awra_widget_queue_draw(self.as_ptr()) }
    }
    pub fn queue_resize(&self) {
        unsafe { ffi::awra_widget_queue_resize(self.as_ptr()) }
    }
    pub fn set_widget_name(&self, name: &str) {
        let n = cstring(name);
        unsafe { ffi::awra_widget_set_name(self.as_ptr(), n.as_ptr()) }
    }
    pub fn set_name(&self, name: &str) {
        self.set_widget_name(name)
    }
    pub fn widget_name(&self) -> String {
        unsafe { from_cstr(ffi::awra_widget_name(self.as_ptr())) }
    }
    pub fn set_typography(&self, typography: Typography) {
        unsafe { ffi::awra_widget_set_typography(self.as_ptr(), typography as _) }
    }
    pub fn set_margin(&self, spacing: Spacing) {
        unsafe { ffi::awra_widget_set_margin(self.as_ptr(), spacing as _) }
    }

    // --- data ---------------------------------------------------------------
    /// Keeps `value` alive with the widget under `key` (replacing any).
    pub fn set_rust_data<T: Any>(&self, key: &str, value: T) {
        let k = cstring(&format!("rs:{key}"));
        let b: Box<Box<dyn Any>> = Box::new(Box::new(value));
        unsafe { ffi::awra_widget_set_data(self.as_ptr(), k.as_ptr(), Box::into_raw(b) as *mut c_void, Some(any_free)) }
    }
    pub fn rust_data<T: Any + Clone>(&self, key: &str) -> Option<T> {
        let k = cstring(&format!("rs:{key}"));
        let p = unsafe { ffi::awra_widget_get_data(self.as_ptr(), k.as_ptr()) };
        if p.is_null() {
            return None;
        }
        let b = unsafe { &*(p as *const Box<dyn Any>) };
        b.downcast_ref::<T>().cloned()
    }

    // --- generic properties ---------------------------------------------------
    pub fn set_property_str(&self, name: &str, value: &str) -> bool {
        let (n, v) = (cstring(name), cstring(value));
        unsafe { ffi::awra_widget_set_property_string(self.as_ptr(), n.as_ptr(), v.as_ptr()) }
    }
    pub fn set_property_f64(&self, name: &str, value: f64) -> bool {
        let n = cstring(name);
        unsafe { ffi::awra_widget_set_property_double(self.as_ptr(), n.as_ptr(), value) }
    }

    // --- signals of every widget -------------------------------------------
    pub fn connect_destroy<F: Fn(&Widget) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_raw(sig::DESTROY, move |w, _| f(w))
    }
    pub fn connect_map<F: Fn(&Widget) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_raw(sig::MAP, move |w, _| f(w))
    }
    pub fn connect_unmap<F: Fn(&Widget) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_raw(sig::UNMAP, move |w, _| f(w))
    }
    pub fn connect_resize<F: Fn(&Widget, i32, i32) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_raw(sig::RESIZE, move |w, a| f(w, a.width(), a.height()))
    }
    /// Accepts dropped files (they arrive through `connect_files_dropped`).
    pub fn set_accepts_file_drops(&self, accepts: bool) {
        unsafe { ffi::awra_widget_set_accepts_file_drops(self.as_ptr(), accepts) }
    }
    pub fn connect_files_dropped<F: Fn(&Widget, Vec<PathBuf>) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_raw(sig::FILES_DROPPED, move |w, a| {
            let paths = a.text().unwrap_or_default().lines().filter(|l| !l.is_empty()).map(PathBuf::from).collect();
            f(w, paths)
        })
    }
    pub fn add_controller(&self, controller: impl Controller) {
        controller.attach(self);
    }
    /// Activates a `win.*` action of the widget's window.
    pub fn activate_action(&self, name: &str, target: Option<&crate::gio::Variant>) -> bool {
        crate::gio::activate_action(self, name, target)
    }
    /// Clipboard text of the widget's window.
    pub fn clipboard_text(&self) -> Option<String> {
        unsafe {
            let p = ffi::awra_clipboard_get_text(self.as_ptr());
            if p.is_null() {
                return None;
            }
            let s = from_cstr(p);
            ffi::awra_free(p as *mut c_void);
            Some(s)
        }
    }
    pub fn set_clipboard_text(&self, text: &str) {
        let t = cstring(text);
        unsafe { ffi::awra_clipboard_set_text(self.as_ptr(), t.as_ptr()) }
    }
    /// gtk-rs compatibility: the widget itself stands for its display.
    pub fn display(&self) -> Display {
        Display(self.clone())
    }
}

/// The display of a widget: its clipboard.
pub struct Display(Widget);

impl Display {
    pub fn clipboard(&self) -> Clipboard {
        Clipboard(self.0.clone())
    }
}

pub struct Clipboard(Widget);

impl Clipboard {
    /// The clipboard of any of the application's windows.
    pub fn of_any_window() -> Clipboard {
        let w = crate::Window::toplevels().into_iter().next().map(|w| w.upcast()).unwrap_or_else(|| {
            crate::Box::new(crate::Orientation::Horizontal, 0).upcast()
        });
        Clipboard(w)
    }
    pub fn text(&self) -> Option<String> {
        self.0.clipboard_text()
    }
    pub fn set_text(&self, text: &str) {
        self.0.set_clipboard_text(text)
    }
    /// Resolves at once: the clipboard is read synchronously.
    pub async fn read_text_future(&self) -> Result<Option<String>, ()> {
        Ok(self.text())
    }
}

// ---------------------------------------------------------------------------
// Event controllers

/// Something `add_controller` attaches to a widget.
pub trait Controller {
    fn attach(&self, widget: &Widget);
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum PropagationPhase {
    None,
    Capture,
    #[default]
    Bubble,
    Target,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EventSequenceState {
    None,
    Claimed,
    Denied,
}

type ClickFn = Rc<dyn Fn(&GestureClick, i32, f64, f64)>;

struct ClickInner {
    button: std::cell::Cell<u32>,
    phase: std::cell::Cell<PropagationPhase>,
    pressed: RefCell<Vec<ClickFn>>,
    released: RefCell<Vec<ClickFn>>,
    current: std::cell::Cell<u32>,
    mods: std::cell::Cell<crate::gdk::ModifierType>,
    widget: RefCell<Option<crate::WeakRef<Widget>>>,
}

/// Clicks on a widget (button 0 = any; default primary).
#[derive(Clone)]
pub struct GestureClick(Rc<ClickInner>);

impl Default for GestureClick {
    fn default() -> Self {
        Self::new()
    }
}

impl GestureClick {
    pub fn new() -> GestureClick {
        GestureClick(Rc::new(ClickInner {
            button: std::cell::Cell::new(1),
            phase: std::cell::Cell::new(PropagationPhase::Bubble),
            pressed: RefCell::new(Vec::new()),
            released: RefCell::new(Vec::new()),
            current: std::cell::Cell::new(0),
            mods: std::cell::Cell::new(crate::gdk::ModifierType::empty()),
            widget: RefCell::new(None),
        }))
    }
    /// The modifiers held at the press or release being handled.
    pub fn current_event_state(&self) -> crate::gdk::ModifierType {
        self.0.mods.get()
    }
    pub fn set_button(&self, button: u32) {
        self.0.button.set(button)
    }
    pub fn current_button(&self) -> u32 {
        self.0.current.get()
    }
    pub fn set_propagation_phase(&self, phase: PropagationPhase) {
        self.0.phase.set(phase)
    }
    pub fn set_state(&self, _state: EventSequenceState) {}
    pub fn widget(&self) -> Option<Widget> {
        self.0.widget.borrow().as_ref().and_then(|w| w.upgrade())
    }
    pub fn connect_pressed<F: Fn(&GestureClick, i32, f64, f64) + 'static>(&self, f: F) {
        self.0.pressed.borrow_mut().push(Rc::new(f));
    }
    pub fn connect_released<F: Fn(&GestureClick, i32, f64, f64) + 'static>(&self, f: F) {
        self.0.released.borrow_mut().push(Rc::new(f));
    }
    fn wants(&self, button: u32) -> bool {
        let b = self.0.button.get();
        b == 0 || b == button
    }
}

impl Controller for GestureClick {
    fn attach(&self, widget: &Widget) {
        *self.0.widget.borrow_mut() = Some(widget.downgrade());
        let capture = self.0.phase.get() == PropagationPhase::Capture && widget.is::<crate::Window>();
        if capture {
            // window-wide observation before anything else
            let me = self.clone();
            widget.connect_raw(sig::PRESS_CAPTURE, move |_, a| {
                if me.wants(a.button()) {
                    me.0.current.set(a.button());
                    me.0.mods.set(a.mods());
                    let hs: Vec<ClickFn> = me.0.pressed.borrow().clone();
                    for h in hs {
                        h(&me, a.n_press(), a.x(), a.y());
                    }
                }
            });
            if self.0.released.borrow().is_empty() {
                return;
            }
        }
        let me = self.clone();
        if !capture {
            widget.connect_raw(sig::PRESSED, move |_, a| {
                if me.wants(a.button()) {
                    me.0.current.set(a.button());
                    me.0.mods.set(a.mods());
                    let hs: Vec<ClickFn> = me.0.pressed.borrow().clone();
                    for h in hs {
                        h(&me, a.n_press(), a.x(), a.y());
                    }
                    if a.button() != 1 {
                        a.stop();
                    }
                }
            });
        }
        let me = self.clone();
        widget.connect_raw(sig::RELEASED, move |_, a| {
            if me.wants(a.button()) {
                me.0.current.set(a.button());
                    me.0.mods.set(a.mods());
                let hs: Vec<ClickFn> = me.0.released.borrow().clone();
                for h in hs {
                    h(&me, a.n_press(), a.x(), a.y());
                }
                if a.button() != 1 {
                    a.stop();
                }
            }
        });
    }
}

type KeyFn = Rc<dyn Fn(&EventControllerKey, crate::gdk::Key, u32, crate::gdk::ModifierType) -> Propagation>;

/// GDK's hardware keycode for a physical key ("KeyA"; W3C code names):
/// the evdev number + 8 (the X11 numbering GDK uses on Linux); 0 unknown.
pub fn hardware_keycode(code: &str) -> u32 {
    let c = cstring(code);
    let evdev = unsafe { ffi::awra_key_code_evdev(c.as_ptr()) };
    if evdev < 0 {
        0
    } else {
        evdev as u32 + 8
    }
}

/// The physical key (W3C code name) of a GDK hardware keycode.
pub fn keycode_name(keycode: u32) -> Option<String> {
    if keycode < 8 {
        return None;
    }
    unsafe { from_cstr_opt(ffi::awra_key_code_name(keycode as i32 - 8)) }
}

struct KeyInner {
    phase: std::cell::Cell<PropagationPhase>,
    pressed: RefCell<Vec<KeyFn>>,
    released: RefCell<Vec<KeyFn>>,
    widget: RefCell<Option<crate::WeakRef<Widget>>>,
}

/// Key presses on a widget (or the focus inside it).
#[derive(Clone)]
pub struct EventControllerKey(Rc<KeyInner>);

impl Default for EventControllerKey {
    fn default() -> Self {
        Self::new()
    }
}

impl EventControllerKey {
    pub fn new() -> EventControllerKey {
        EventControllerKey(Rc::new(KeyInner {
            phase: std::cell::Cell::new(PropagationPhase::Bubble),
            pressed: RefCell::new(Vec::new()),
            released: RefCell::new(Vec::new()),
            widget: RefCell::new(None),
        }))
    }
    pub fn set_propagation_phase(&self, phase: PropagationPhase) {
        self.0.phase.set(phase)
    }
    pub fn widget(&self) -> Option<Widget> {
        self.0.widget.borrow().as_ref().and_then(|w| w.upgrade())
    }
    pub fn connect_key_pressed<
        F: Fn(&EventControllerKey, crate::gdk::Key, u32, crate::gdk::ModifierType) -> Propagation + 'static,
    >(
        &self,
        f: F,
    ) {
        self.0.pressed.borrow_mut().push(Rc::new(f));
    }
    /// (GTK's key-released returns nothing: releases propagate.)
    pub fn connect_key_released<F: Fn(&EventControllerKey, crate::gdk::Key, u32, crate::gdk::ModifierType) + 'static>(
        &self,
        f: F,
    ) {
        self.0.released.borrow_mut().push(Rc::new(move |c, k, code, m| {
            f(c, k, code, m);
            Propagation::Proceed
        }));
    }
}

impl Controller for EventControllerKey {
    fn attach(&self, widget: &Widget) {
        *self.0.widget.borrow_mut() = Some(widget.downgrade());
        let capture = self.0.phase.get() == PropagationPhase::Capture && widget.is::<crate::Window>();
        for (signal, list) in [
            (if capture { sig::KEY_CAPTURE } else { sig::KEY_PRESSED }, 0),
            (sig::KEY_RELEASED, 1),
        ] {
            let me = self.clone();
            if list == 1 && self.0.released.borrow().is_empty() {
                continue;
            }
            widget.connect_raw(signal, move |_, a| {
                let key = crate::gdk::Key::from_name(&a.keysym().unwrap_or_default());
                let code = a.code().map_or(0, |c| hardware_keycode(&c));
                let hs: Vec<KeyFn> =
                    if list == 0 { me.0.pressed.borrow().clone() } else { me.0.released.borrow().clone() };
                for h in hs {
                    if h(&me, key, code, a.mods()) == Propagation::Stop {
                        a.stop();
                        break;
                    }
                }
            });
        }
    }
}

// ---------------------------------------------------------------------------
// Free functions (Awra 1 names)

pub fn set_typography(widget: &impl IsWidget, typography: Typography) {
    widget.upcast_ref().set_typography(typography)
}

pub fn set_margin(widget: &impl IsWidget, spacing: Spacing) {
    widget.upcast_ref().set_margin(spacing)
}

pub fn add_style_role(widget: &impl IsWidget, role: &str) -> bool {
    widget.upcast_ref().add_css_class(role);
    true
}

pub fn remove_style_role(widget: &impl IsWidget, role: &str) -> bool {
    widget.upcast_ref().remove_css_class(role);
    true
}

/// Checked conversions between widget types (gtk-rs `Cast`).
pub trait Cast: IsWidget {
    fn is<T: StaticType>(&self) -> bool {
        T::matches(self.upcast_ref())
    }
    fn downcast_ref_to<T: StaticType>(&self) -> Option<T> {
        self.upcast_ref().clone().downcast().ok()
    }
}
impl<T: IsWidget> Cast for T {}
