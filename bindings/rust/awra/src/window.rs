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

/// The language messages are shown in, as gettext picks it: LANGUAGE's
/// first entry unless the locale (LC_ALL, LC_MESSAGES, LANG) is C.
fn messages_language() -> String {
    let locale = ["LC_ALL", "LC_MESSAGES", "LANG"]
        .iter()
        .filter_map(|v| std::env::var(v).ok())
        .find(|v| !v.is_empty())
        .unwrap_or_default();
    if locale.is_empty() || locale == "C" || locale.starts_with("C.") || locale == "POSIX" {
        return String::new();
    }
    match std::env::var("LANGUAGE") {
        Ok(l) if !l.is_empty() => l.split(':').next().unwrap_or("").to_owned(),
        _ => locale,
    }
}

/// GTK's "keyboard label" translations (gtk/po), for the languages Awra's
/// apps ship in.
fn key_word(word: &'static str) -> &'static str {
    thread_local! {
        static FRENCH: bool = messages_language().starts_with("fr");
    }
    if !FRENCH.with(|f| *f) {
        return word;
    }
    match word {
        "Shift" => "Maj",
        "Meta" => "Méta",
        "KP" => "PNum",
        "Space" => "Espace",
        "Backslash" => "Barre oblique inversée",
        "BackSpace" => "Retour arrière",
        "Tab" => "Tabulation",
        "Return" => "Retour",
        "Escape" => "Échap.",
        "Home" => "Origine",
        "Left" => "Gauche",
        "Up" => "Haut",
        "Right" => "Droite",
        "Down" => "Bas",
        "Page_Up" => "Page_précédente",
        "Page_Down" => "Page_suivante",
        "End" => "Fin",
        "Begin" => "Début",
        "Print" => "Impr",
        "Insert" => "Inser",
        "Delete" => "Suppr",
        w => w,
    }
}

/// The character a key name types (gdk_keyval_to_unicode) for the
/// printable keys accelerators use.
fn key_char(name: &str) -> Option<char> {
    let mut chars = name.chars();
    if let (Some(c), None) = (chars.next(), chars.next()) {
        return Some(c);
    }
    Some(match name {
        "space" => ' ',
        "exclam" => '!',
        "quotedbl" => '"',
        "numbersign" => '#',
        "dollar" => '$',
        "percent" => '%',
        "ampersand" => '&',
        "apostrophe" => '\'',
        "parenleft" => '(',
        "parenright" => ')',
        "asterisk" => '*',
        "plus" => '+',
        "comma" => ',',
        "minus" => '-',
        "period" => '.',
        "slash" => '/',
        "colon" => ':',
        "semicolon" => ';',
        "less" => '<',
        "equal" => '=',
        "greater" => '>',
        "question" => '?',
        "at" => '@',
        "bracketleft" => '[',
        "backslash" => '\\',
        "bracketright" => ']',
        "asciicircum" => '^',
        "underscore" => '_',
        "grave" => '`',
        "braceleft" => '{',
        "bar" => '|',
        "braceright" => '}',
        "asciitilde" => '~',
        _ => return None,
    })
}

/// "<Control><Shift>z" → "Shift+Ctrl+Z", as gtk_accelerator_get_label
/// shows it in menus (modifiers in GTK's order, GTK's key words, in the
/// messages' language: "Maj+Ctrl+Z" in French).
pub fn accel_label(accel: &str) -> String {
    const SHIFT: u8 = 1;
    const CTRL: u8 = 2;
    const ALT: u8 = 4;
    const SUPER: u8 = 8;
    const HYPER: u8 = 16;
    const META: u8 = 32;
    let mut mods = 0u8;
    let mut rest = accel;
    while let Some(start) = rest.strip_prefix('<') {
        let Some((m, r)) = start.split_once('>') else { break };
        mods |= match m.to_ascii_lowercase().as_str() {
            "primary" | "control" | "ctrl" | "ctl" => CTRL,
            "shift" | "shft" => SHIFT,
            "alt" | "mod1" => ALT,
            "super" => SUPER,
            "hyper" => HYPER,
            "meta" => META,
            _ => return accel.to_owned(),
        };
        rest = r;
    }
    if rest.is_empty() || rest.starts_with('<') || accel.contains('+') && !accel.contains('<') {
        return accel.to_owned();
    }
    let mut out: Vec<String> = Vec::new();
    for (bit, word) in [(SHIFT, "Shift"), (CTRL, "Ctrl"), (ALT, "Alt"), (SUPER, "Super"), (HYPER, "Hyper"), (META, "Meta")] {
        if mods & bit != 0 {
            out.push(key_word(word).to_owned());
        }
    }
    match key_char(rest) {
        Some(' ') => out.push(key_word("Space").to_owned()),
        Some('\\') => out.push(key_word("Backslash").to_owned()),
        Some(c) => out.push(c.to_uppercase().collect()),
        None => {
            // gdk_keyval_name of the lower case key, its words translated,
            // or with spaces for underscores (XF86 and ISO_ dropped)
            let name = rest.strip_prefix("XF86").or_else(|| rest.strip_prefix("ISO_")).unwrap_or(rest);
            let words: &[&'static str] = &[
                "BackSpace", "Tab", "Return", "Escape", "Home", "Left", "Up", "Right", "Down", "Page_Up",
                "Page_Down", "End", "Begin", "Print", "Insert", "Delete",
            ];
            match words.iter().find(|w| **w == rest) {
                Some(w) if key_word(w) != *w => out.push(key_word(w).to_owned()),
                _ => out.push(name.replace('_', " ")),
            }
        }
    }
    out.join("+")
}

thread_local! {
    static WINDOW_ADDED: std::cell::RefCell<Vec<std::rc::Rc<dyn Fn(&Window)>>> = const { std::cell::RefCell::new(Vec::new()) };
}

/// Tells window-added listeners about a new window (dialogs included).
pub(crate) fn window_added(w: &Window) {
    let listeners: Vec<_> = WINDOW_ADDED.with(|l| l.borrow().clone());
    for f in listeners {
        f(w);
    }
}

impl Window {
    /// A new window of `application` (registers its accelerators).
    pub fn new(application: &crate::gio::Application) -> Window {
        let t = cstring("");
        // the window list keeps the creation reference until the window is
        // closed (as GTK's toplevel list does): this handle takes its own
        let w = unsafe { Window::from_none_ptr(ffi::awra_window_new(t.as_ptr(), 800, 600)) };
        application.add_window(&w.0);
        window_added(&w);
        w
    }
    pub fn with_title(title: &str, width: i32, height: i32) -> Window {
        let t = cstring(title);
        let w = unsafe { Window::from_none_ptr(ffi::awra_window_new(t.as_ptr(), width, height)) };
        window_added(&w);
        w
    }
    /// # Safety
    /// `ptr` is a live window.
    pub(crate) unsafe fn from_none_ptr(ptr: *mut ffi::AwraWidget) -> Window {
        Window(crate::Widget::from_none(ptr))
    }
    /// Calls `f` for every window created from now on.
    pub fn connect_window_added<F: Fn(&Window) + 'static>(f: F) {
        WINDOW_ADDED.with(|l| l.borrow_mut().push(std::rc::Rc::new(f)));
    }
    /// The keyboard focus moved.
    pub fn connect_focus_widget_notify<F: Fn(&Window) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Window, _>(sig::NOTIFY, move |w, a| {
            if a.text().as_deref() == Some("focus") {
                f(w)
            }
        })
    }
    /// A window rendered offscreen (tests, benchmarks).
    pub fn offscreen(title: &str, width: i32, height: i32, scale: f64) -> Window {
        let t = cstring(title);
        unsafe { Window::from_none_ptr(ffi::awra_window_new_offscreen(t.as_ptr(), width, height, scale)) }
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
    pub fn is_resizable(&self) -> bool {
        unsafe { ffi::awra_window_get_resizable(self.0.as_ptr()) }
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
    /// GtkWindow's modal: while shown, the window it is transient for
    /// takes no pointer or key input.
    pub fn set_modal(&self, m: bool) {
        unsafe { ffi::awra_window_set_modal(self.0.as_ptr(), m) }
    }
    pub fn is_modal(&self) -> bool {
        unsafe { ffi::awra_window_get_modal(self.0.as_ptr()) }
    }
    /// GtkWindow's transient-for: kept above `w` (a dialog to the window
    /// manager).
    pub fn set_transient_for(&self, w: Option<&Window>) {
        unsafe { ffi::awra_window_set_transient_for(self.0.as_ptr(), w.map_or(std::ptr::null_mut(), |w| w.0.as_ptr())) }
    }
    pub fn transient_for(&self) -> Option<Window> {
        let p = unsafe { ffi::awra_window_get_transient_for(self.0.as_ptr()) };
        (!p.is_null()).then(|| unsafe { Window::from_none_ptr(p) })
    }
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
