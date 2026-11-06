//! Windows.

use std::ffi::c_void;

use crate::awra_wrapper;
use crate::ffi;
use crate::glib::Propagation;
use crate::object::{cstring, from_cstr, IntoOptStr, IntoOptWidget, IsWidget, OptC, Widget};
use crate::signal::{sig, SignalHandlerId};

awra_wrapper!(
    /// A top-level window: Awra's canvas material, blurred by the compositor,
    /// with client-side decorations.
    Window, ["AwraWindow"]
);

type ShortcutFn = Box<dyn Fn(&Widget)>;
unsafe extern "C" fn shortcut_tramp(w: *mut ffi::AwraWidget, _a: *const ffi::AwraArgs, data: *mut c_void) {
    let f = &*(data as *const ShortcutFn);
    let w = std::mem::ManuallyDrop::new(Widget::from_full(w));
    f(&w)
}
unsafe extern "C" fn shortcut_free(data: *mut c_void) {
    drop(Box::from_raw(data as *mut ShortcutFn));
}

/// A keyboard shortcut on a window ("<Ctrl>k", "<Primary>z", "F11").
pub(crate) fn add_shortcut<F: Fn(&Widget) + 'static>(window: &Widget, accel: &str, f: F) -> u64 {
    let a = cstring(&accel.replace("<Primary>", "<Ctrl>").replace("<Control>", "<Ctrl>"));
    let data: Box<ShortcutFn> = Box::new(Box::new(f));
    unsafe {
        ffi::awra_window_add_shortcut(
            window.as_ptr(),
            a.as_ptr(),
            Some(shortcut_tramp),
            Box::into_raw(data) as *mut c_void,
            Some(shortcut_free),
        ) as u64
    }
}

/// "<Ctrl><Shift>z" → "Ctrl+Shift+Z" (menus and palettes show it).
pub fn accel_label(accel: &str) -> String {
    let mut out = Vec::new();
    let mut rest = accel;
    while let Some(start) = rest.strip_prefix('<') {
        let Some((m, r)) = start.split_once('>') else { break };
        out.push(match m {
            "Primary" | "Control" | "Ctrl" => "Ctrl".to_owned(),
            "Shift" => "Shift".to_owned(),
            "Alt" => "Alt".to_owned(),
            "Super" | "Meta" => "Super".to_owned(),
            o => o.to_owned(),
        });
        rest = r;
    }
    if rest.starts_with('<') || accel.contains('+') && !accel.contains('<') {
        return accel.to_owned();
    }
    let key = match rest {
        "comma" => ",".to_owned(),
        "period" => ".".to_owned(),
        "plus" => "+".to_owned(),
        "minus" => "-".to_owned(),
        "space" => "Space".to_owned(),
        "Return" => "Enter".to_owned(),
        "Escape" => "Esc".to_owned(),
        k if k.chars().count() == 1 => k.to_uppercase(),
        k => k.to_owned(),
    };
    out.push(key);
    out.join("+")
}

impl Window {
    /// A new window of `application` (registers its accelerators).
    pub fn new(application: &crate::gio::Application) -> Window {
        let w = Window::with_title("", 800, 600);
        application.add_window(&w.0);
        w
    }
    pub fn with_title(title: &str, width: i32, height: i32) -> Window {
        let t = cstring(title);
        unsafe { Window::from_full(ffi::awra_window_new(t.as_ptr(), width, height)) }
    }
    /// A window rendered offscreen (tests, benchmarks).
    pub fn offscreen(title: &str, width: i32, height: i32, scale: f64) -> Window {
        let t = cstring(title);
        unsafe { Window::from_full(ffi::awra_window_new_offscreen(t.as_ptr(), width, height, scale)) }
    }
    pub fn render_offscreen(&self, full: bool) -> i32 {
        unsafe { ffi::awra_window_render_offscreen(self.0.as_ptr(), full) }
    }
    pub fn snapshot_png(&self, path: &str) -> bool {
        let p = cstring(path);
        unsafe { ffi::awra_window_snapshot_png(self.0.as_ptr(), p.as_ptr()) }
    }
    pub fn set_title(&self, title: impl IntoOptStr) {
        let t = OptC::new(title);
        unsafe { ffi::awra_window_set_title(self.0.as_ptr(), t.ptr()) }
    }
    pub fn title(&self) -> Option<String> {
        Some(unsafe { from_cstr(ffi::awra_window_title(self.0.as_ptr())) })
    }
    pub fn set_child(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_window_set_child(self.0.as_ptr(), child.into_opt_ptr()) }
    }
    pub fn set_content(&self, child: impl IntoOptWidget) {
        self.set_child(child)
    }
    pub fn child(&self) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_window_child(self.0.as_ptr())) }
    }
    pub fn set_titlebar(&self, titlebar: impl IntoOptWidget) {
        unsafe { ffi::awra_window_set_titlebar(self.0.as_ptr(), titlebar.into_opt_ptr()) }
    }
    /// Awra 1 name of the title bar.
    pub fn set_chrome(&self, chrome: impl IntoOptWidget) {
        self.set_titlebar(chrome)
    }
    pub fn present(&self) {
        unsafe { ffi::awra_window_present(self.0.as_ptr()) }
    }
    pub fn hide(&self) {
        unsafe { ffi::awra_window_hide(self.0.as_ptr()) }
    }
    pub fn close(&self) {
        unsafe { ffi::awra_window_close(self.0.as_ptr()) }
    }
    pub fn set_default_size(&self, width: i32, height: i32) {
        unsafe { ffi::awra_window_set_default_size(self.0.as_ptr(), width, height) }
    }
    pub fn default_size(&self) -> (i32, i32) {
        let (mut w, mut h) = (0, 0);
        unsafe { ffi::awra_window_get_size(self.0.as_ptr(), &mut w, &mut h) };
        (w, h)
    }
    /// The window's minimum size.
    pub fn set_size_request(&self, width: i32, height: i32) {
        unsafe { ffi::awra_window_set_min_size(self.0.as_ptr(), width, height) }
    }
    pub fn set_resizable(&self, r: bool) {
        unsafe { ffi::awra_window_set_resizable(self.0.as_ptr(), r) }
    }
    pub fn maximize(&self) {
        unsafe { ffi::awra_window_maximize(self.0.as_ptr()) }
    }
    pub fn unmaximize(&self) {
        unsafe { ffi::awra_window_unmaximize(self.0.as_ptr()) }
    }
    pub fn minimize(&self) {
        unsafe { ffi::awra_window_minimize(self.0.as_ptr()) }
    }
    pub fn fullscreen(&self) {
        unsafe { ffi::awra_window_fullscreen(self.0.as_ptr(), true) }
    }
    pub fn unfullscreen(&self) {
        unsafe { ffi::awra_window_fullscreen(self.0.as_ptr(), false) }
    }
    pub fn state(&self) -> i32 {
        unsafe { ffi::awra_window_state(self.0.as_ptr()) }
    }
    pub fn is_maximized(&self) -> bool {
        self.state() & ffi::AWRA_WINDOW_STATE_MAXIMIZED as i32 != 0
    }
    pub fn is_fullscreen(&self) -> bool {
        self.state() & ffi::AWRA_WINDOW_STATE_FULLSCREEN as i32 != 0
    }
    pub fn is_active(&self) -> bool {
        unsafe { ffi::awra_window_is_active(self.0.as_ptr()) }
    }
    /// Shown on screen (not hidden to the background).
    pub fn is_shown(&self) -> bool {
        self.0.is_visible() && self.state() & ffi::AWRA_WINDOW_STATE_HIDDEN as i32 == 0
    }
    pub fn set_hide_on_close(&self, hide: bool) {
        unsafe { ffi::awra_window_set_hide_on_close(self.0.as_ptr(), hide) }
    }
    pub fn set_keep_rounded_corners(&self, keep: bool) {
        unsafe { ffi::awra_window_set_keep_rounded_corners(self.0.as_ptr(), keep) }
    }
    pub fn set_modal(&self, _m: bool) {}
    pub fn set_transient_for(&self, _w: Option<&Window>) {}
    pub fn set_destroy_with_parent(&self, _d: bool) {}
    pub fn set_decorated(&self, _d: bool) {}
    pub fn native_handle(&self) -> *mut c_void {
        unsafe { ffi::awra_window_native_handle(self.0.as_ptr()) }
    }
    /// The widget with the keyboard focus.
    pub fn focus(&self) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_window_focus(self.0.as_ptr())) }
    }
    pub fn set_focus(&self, widget: Option<&impl IsWidget>) {
        let p = widget.map_or(std::ptr::null_mut(), |w| w.widget_ptr());
        unsafe { ffi::awra_window_set_focus(self.0.as_ptr(), p) }
    }
    /// Keyboard shortcut ("<Ctrl>k").
    pub fn add_shortcut<F: Fn(&Widget) + 'static>(&self, accel: &str, f: F) -> u64 {
        add_shortcut(&self.0, accel, f)
    }
    pub fn add_action(&self, action: &crate::gio::SimpleAction) {
        crate::gio::add_action(&self.0, action)
    }
    pub fn remove_action(&self, name: &str) {
        crate::gio::remove_action(&self.0, name)
    }
    pub fn lookup_action(&self, name: &str) -> Option<crate::gio::SimpleAction> {
        crate::gio::lookup_action(&self.0, name)
    }
    pub fn application(&self) -> Option<crate::gio::Application> {
        crate::gio::Application::default()
    }
    pub fn stats(&self) -> (u64, u64) {
        let (mut f, mut p) = (0u64, 0u64);
        unsafe { ffi::awra_window_stats(self.0.as_ptr(), &mut f, &mut p) };
        (f, p)
    }
    /// Return `Propagation::Stop` to keep the window open.
    pub fn connect_close_request<F: Fn(&Window) -> Propagation + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Window, _>(sig::CLOSE_REQUEST, move |w, a| {
            if f(w) == Propagation::Stop {
                a.stop()
            }
        })
    }
    pub fn connect_is_active_notify<F: Fn(&Window) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Window, _>(sig::STATE, move |w, _| f(w))
    }
    pub fn connect_state_changed<F: Fn(&Window, i32) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Window, _>(sig::STATE, move |w, a| f(w, a.index()))
    }
    pub fn connect_default_size_notify<F: Fn(&Window) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Window, _>(sig::RESIZE, move |w, _| f(w))
    }
    /// Every open window.
    pub fn toplevels() -> Vec<Window> {
        let n = unsafe { ffi::awra_window_count() };
        (0..n)
            .filter_map(|i| unsafe { Widget::from_none_opt(ffi::awra_window_nth(i)) })
            .filter_map(|w| w.downcast().ok())
            .collect()
    }
}

/// GtkApplicationWindow.
pub type ApplicationWindow = Window;
