//! Actions and menu models (the subset of GIO the applications use):
//! `win.*` actions on windows, activated by buttons, menus, the command
//! palette and keyboard accelerators.

use std::cell::{Cell, RefCell};
use std::collections::HashMap;
use std::rc::{Rc, Weak};

use crate::object::{IntoOptStr, Widget, WeakRef};

/// Action parameters: strings only (all Awra applications need).
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Variant(String);

impl Variant {
    pub fn str(&self) -> Option<&str> {
        Some(&self.0)
    }
    pub fn get<T: FromVariant>(&self) -> Option<T> {
        T::from_variant(self)
    }
}

pub trait ToVariant {
    fn to_variant(&self) -> Variant;
}
impl ToVariant for str {
    fn to_variant(&self) -> Variant {
        Variant(self.to_owned())
    }
}
impl ToVariant for &str {
    fn to_variant(&self) -> Variant {
        Variant((*self).to_owned())
    }
}
impl ToVariant for String {
    fn to_variant(&self) -> Variant {
        Variant(self.clone())
    }
}

pub trait FromVariant: Sized {
    fn from_variant(v: &Variant) -> Option<Self>;
}
impl FromVariant for String {
    fn from_variant(v: &Variant) -> Option<String> {
        Some(v.0.clone())
    }
}
macro_rules! variant_num {
    ($($t:ty),*) => {$(
        impl ToVariant for $t {
            fn to_variant(&self) -> Variant {
                Variant(self.to_string())
            }
        }
        impl FromVariant for $t {
            fn from_variant(v: &Variant) -> Option<$t> {
                v.0.parse().ok()
            }
        }
    )*};
}
variant_num!(bool, u32, i32, u64, i64, f64);

/// Parameter types (all carried as text).
pub struct VariantTy(());
impl VariantTy {
    pub const STRING: &'static VariantTy = &VariantTy(());
    pub const UINT32: &'static VariantTy = &VariantTy(());
    pub const INT32: &'static VariantTy = &VariantTy(());
    pub const BOOLEAN: &'static VariantTy = &VariantTy(());
}

/// A named action with an optional string parameter.
#[derive(Clone)]
pub struct SimpleAction(Rc<ActionInner>);

type ActivateFn = Rc<dyn Fn(&SimpleAction, Option<&Variant>)>;

struct ActionInner {
    name: String,
    has_param: bool,
    enabled: Cell<bool>,
    handlers: RefCell<Vec<ActivateFn>>,
    state: RefCell<Option<Variant>>,
}

impl SimpleAction {
    pub fn new(name: &str, parameter_type: Option<&VariantTy>) -> SimpleAction {
        SimpleAction(Rc::new(ActionInner {
            name: name.to_owned(),
            has_param: parameter_type.is_some(),
            enabled: Cell::new(true),
            handlers: RefCell::new(Vec::new()),
            state: RefCell::new(None),
        }))
    }
    /// An action with a state (a menu's check mark: "true" / "false").
    pub fn new_stateful(name: &str, parameter_type: Option<&VariantTy>, state: &Variant) -> SimpleAction {
        let a = SimpleAction::new(name, parameter_type);
        *a.0.state.borrow_mut() = Some(state.clone());
        a
    }
    pub fn state(&self) -> Option<Variant> {
        self.0.state.borrow().clone()
    }
    pub fn set_state(&self, state: &Variant) {
        *self.0.state.borrow_mut() = Some(state.clone());
    }
    pub fn change_state(&self, state: &Variant) {
        self.set_state(state)
    }
    pub fn name(&self) -> String {
        self.0.name.clone()
    }
    pub fn connect_activate<F: Fn(&SimpleAction, Option<&Variant>) + 'static>(&self, f: F) {
        self.0.handlers.borrow_mut().push(Rc::new(f));
    }
    pub fn activate(&self, parameter: Option<&Variant>) {
        if !self.0.enabled.get() {
            return;
        }
        if self.0.has_param != parameter.is_some() {
            return;
        }
        let handlers: Vec<ActivateFn> = self.0.handlers.borrow().clone();
        for h in handlers {
            h(self, parameter);
        }
    }
    pub fn set_enabled(&self, enabled: bool) {
        if self.0.enabled.replace(enabled) != enabled {
            sync_bound_widgets(&self.0.name, enabled);
        }
    }
    pub fn is_enabled(&self) -> bool {
        self.0.enabled.get()
    }
}

// ---------------------------------------------------------------------------
// Action maps of windows, widgets bound to actions

thread_local! {
    static MAPS: RefCell<HashMap<usize, HashMap<String, SimpleAction>>> = RefCell::new(HashMap::new());
    static BOUND: RefCell<Vec<(WeakRef<Widget>, String)>> = const { RefCell::new(Vec::new()) };
}

pub(crate) fn add_action(window: &Widget, action: &SimpleAction) {
    let key = window.as_ptr() as usize;
    let fresh = MAPS.with(|m| {
        let mut m = m.borrow_mut();
        let fresh = !m.contains_key(&key);
        m.entry(key).or_default().insert(action.name(), action.clone());
        fresh
    });
    if fresh {
        window.connect_raw(crate::signal::sig::DESTROY, move |_, _| {
            MAPS.with(|m| m.borrow_mut().remove(&key));
        });
    }
    if !action.is_enabled() {
        sync_bound_widgets(&action.name(), false);
    }
}

pub(crate) fn remove_action(window: &Widget, name: &str) {
    MAPS.with(|m| {
        if let Some(map) = m.borrow_mut().get_mut(&(window.as_ptr() as usize)) {
            map.remove(name);
        }
    });
}

pub(crate) fn lookup_action(window: &Widget, name: &str) -> Option<SimpleAction> {
    MAPS.with(|m| m.borrow().get(&(window.as_ptr() as usize)).and_then(|map| map.get(name).cloned()))
}

/// Activates "win.name" (or "name") for the window of `widget`.
pub fn activate_action(widget: &Widget, detailed: &str, target: Option<&Variant>) -> bool {
    if let Some(a) = crate::compat::lookup_group_action(widget, detailed) {
        a.activate(target);
        return true;
    }
    let name = detailed.strip_prefix("win.").or_else(|| detailed.strip_prefix("app.")).unwrap_or(detailed);
    let window = widget.root_widget().unwrap_or_else(|| widget.clone());
    let action = lookup_action(&window, name).or_else(|| {
        // app.* or a window-less widget: any window's action of that name
        MAPS.with(|m| m.borrow().values().find_map(|map| map.get(name).cloned()))
    });
    match action {
        Some(a) => {
            a.activate(target);
            true
        }
        None => false,
    }
}

pub(crate) fn bind_widget(widget: &Widget, detailed: &str) {
    let name = detailed.strip_prefix("win.").unwrap_or(detailed).to_owned();
    BOUND.with(|b| {
        let mut b = b.borrow_mut();
        b.retain(|(w, _)| w.upgrade().is_some_and(|w| &w != widget));
        b.push((widget.downgrade(), name.clone()));
    });
    let enabled = MAPS.with(|m| {
        m.borrow().values().find_map(|map| map.get(&name).map(|a| a.is_enabled()))
    });
    if let Some(enabled) = enabled {
        widget.set_sensitive(enabled);
    }
}

fn sync_bound_widgets(name: &str, enabled: bool) {
    let widgets: Vec<Widget> = BOUND.with(|b| {
        let mut b = b.borrow_mut();
        b.retain(|(w, _)| w.upgrade().is_some());
        b.iter().filter(|(_, n)| n == name).filter_map(|(w, _)| w.upgrade()).collect()
    });
    for w in widgets {
        w.set_sensitive(enabled);
    }
}

// ---------------------------------------------------------------------------
// Menus

/// An icon by name (menus show it).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ThemedIcon(String);
impl ThemedIcon {
    pub fn new(name: &str) -> ThemedIcon {
        ThemedIcon(name.to_owned())
    }
    pub fn name(&self) -> &str {
        &self.0
    }
}

/// One entry of a menu.
#[derive(Debug, Clone, Default)]
pub struct MenuItem(Rc<RefCell<ItemData>>);

#[derive(Debug, Clone, Default)]
pub(crate) struct ItemData {
    pub label: Option<String>,
    pub action: Option<String>,
    pub target: Option<Variant>,
    pub icon: Option<String>,
    pub accel: Option<String>,
    pub subtitle: Option<String>,
    pub custom: Option<String>,
    pub submenu: Option<Menu>,
}

impl MenuItem {
    pub fn new(label: Option<&str>, detailed_action: Option<&str>) -> MenuItem {
        MenuItem(Rc::new(RefCell::new(ItemData {
            label: label.map(str::to_owned),
            action: detailed_action.map(str::to_owned),
            ..Default::default()
        })))
    }
    pub fn set_label(&self, label: Option<&str>) {
        self.0.borrow_mut().label = label.map(str::to_owned);
    }
    /// An item opening `submenu` (shown as a titled section).
    pub fn new_submenu(label: Option<&str>, submenu: &Menu) -> MenuItem {
        let item = MenuItem::new(label, None);
        item.0.borrow_mut().submenu = Some(submenu.clone());
        item
    }
    /// A slot for the application's own widget (Popover::add_menu_widget).
    pub fn set_custom(&self, id: &str) {
        self.0.borrow_mut().custom = Some(id.to_owned());
    }
    pub fn set_icon(&self, icon: &ThemedIcon) {
        self.0.borrow_mut().icon = Some(icon.0.clone());
    }
    pub fn set_action_and_target_value(&self, action: Option<&str>, target: Option<&Variant>) {
        let mut d = self.0.borrow_mut();
        d.action = action.map(str::to_owned);
        d.target = target.cloned();
    }
    pub fn set_detailed_action(&self, action: &str) {
        self.0.borrow_mut().action = Some(action.to_owned());
    }
    /// "accel" (shown on the right) and "subtitle" are understood.
    pub fn set_attribute_value(&self, attribute: &str, value: Option<&Variant>) {
        let v = value.map(|v| v.0.clone());
        let mut d = self.0.borrow_mut();
        match attribute {
            "accel" => d.accel = v,
            "subtitle" | "description" => d.subtitle = v,
            "custom" => d.custom = v,
            _ => {}
        }
    }
    pub(crate) fn data(&self) -> ItemData {
        self.0.borrow().clone()
    }
}

#[derive(Clone)]
pub(crate) enum MenuEntry {
    Item(MenuItem),
    Section(Option<String>, Menu),
}

/// A menu model: items and titled sections; widgets showing it follow its
/// changes.
#[derive(Clone, Default)]
pub struct Menu(Rc<MenuInner>);

impl std::fmt::Debug for Menu {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("Menu")
    }
}

#[derive(Default)]
struct MenuInner {
    entries: RefCell<Vec<MenuEntry>>,
    listeners: RefCell<Vec<Weak<dyn Fn()>>>,
    parents: RefCell<Vec<Weak<MenuInner>>>,
}

impl Menu {
    pub fn new() -> Menu {
        Menu::default()
    }
    fn changed(&self) {
        notify(&self.0);
    }
    pub fn append(&self, label: Option<&str>, detailed_action: Option<&str>) {
        self.append_item(&MenuItem::new(label, detailed_action));
    }
    pub fn append_item(&self, item: &MenuItem) {
        self.0.entries.borrow_mut().push(MenuEntry::Item(item.clone()));
        self.changed();
    }
    pub fn append_submenu(&self, label: Option<&str>, submenu: &Menu) {
        self.append_item(&MenuItem::new_submenu(label, submenu));
    }
    pub fn insert_section(&self, position: i32, label: Option<&str>, section: &Menu) {
        section.0.parents.borrow_mut().push(Rc::downgrade(&self.0));
        let mut e = self.0.entries.borrow_mut();
        let at = if position < 0 { e.len() } else { (position as usize).min(e.len()) };
        e.insert(at, MenuEntry::Section(label.map(str::to_owned), section.clone()));
        drop(e);
        self.changed();
    }
    pub fn insert_item(&self, position: i32, item: &MenuItem) {
        let mut e = self.0.entries.borrow_mut();
        let at = if position < 0 { e.len() } else { (position as usize).min(e.len()) };
        e.insert(at, MenuEntry::Item(item.clone()));
        drop(e);
        self.changed();
    }
    pub fn append_section(&self, label: Option<&str>, section: &Menu) {
        section.0.parents.borrow_mut().push(Rc::downgrade(&self.0));
        self.0.entries.borrow_mut().push(MenuEntry::Section(label.map(str::to_owned), section.clone()));
        self.changed();
    }
    pub fn remove_all(&self) {
        self.0.entries.borrow_mut().clear();
        self.changed();
    }
    pub fn n_items(&self) -> i32 {
        self.0.entries.borrow().len() as i32
    }
    pub(crate) fn entries(&self) -> Vec<MenuEntry> {
        self.0.entries.borrow().clone()
    }
    /// Calls `f` after every change (while the returned token lives).
    pub(crate) fn watch(&self, f: Rc<dyn Fn()>) {
        let mut l = self.0.listeners.borrow_mut();
        l.retain(|w| w.upgrade().is_some());
        l.push(Rc::downgrade(&f));
    }
    /// Flattened: (section heading or item), separators between sections.
    pub(crate) fn flatten(&self) -> Vec<FlatEntry> {
        let mut out = Vec::new();
        flatten_into(self, &mut out, 0);
        out
    }
    pub fn ptr_eq(&self, other: &Menu) -> bool {
        Rc::ptr_eq(&self.0, &other.0)
    }
}

fn notify(inner: &Rc<MenuInner>) {
    let listeners: Vec<Rc<dyn Fn()>> = inner.listeners.borrow().iter().filter_map(|w| w.upgrade()).collect();
    for l in listeners {
        l();
    }
    let parents: Vec<Rc<MenuInner>> = inner.parents.borrow().iter().filter_map(|w| w.upgrade()).collect();
    for p in parents {
        notify(&p);
    }
}

pub(crate) enum FlatEntry {
    Separator,
    Heading(String),
    Item(ItemData, Option<String>),
    Custom(String),
}

fn flatten_into(menu: &Menu, out: &mut Vec<FlatEntry>, depth: usize) {
    let mut section_name: Option<String> = None;
    flatten_level(menu, out, depth, &mut section_name);
}

fn flatten_level(menu: &Menu, out: &mut Vec<FlatEntry>, depth: usize, section: &mut Option<String>) {
    for e in menu.entries() {
        match e {
            MenuEntry::Item(i) => {
                let d = i.data();
                if let Some(id) = d.custom.clone() {
                    out.push(FlatEntry::Custom(id));
                } else if let Some(sub) = d.submenu.clone() {
                    if !out.is_empty() && !matches!(out.last(), Some(FlatEntry::Separator)) {
                        out.push(FlatEntry::Separator);
                    }
                    if let Some(l) = &d.label {
                        out.push(FlatEntry::Heading(l.clone()));
                    }
                    let mut inner = d.label.clone().or_else(|| section.clone());
                    flatten_level(&sub, out, depth + 1, &mut inner);
                    out.push(FlatEntry::Separator);
                } else {
                    out.push(FlatEntry::Item(d, section.clone()));
                }
            }
            MenuEntry::Section(label, sub) => {
                if sub.n_items() == 0 {
                    continue;
                }
                if !out.is_empty() && !matches!(out.last(), Some(FlatEntry::Separator)) {
                    out.push(FlatEntry::Separator);
                }
                if let Some(l) = &label {
                    out.push(FlatEntry::Heading(l.clone()));
                }
                let mut inner = label.clone().or_else(|| section.clone());
                flatten_level(&sub, out, depth + 1, &mut inner);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Application: accelerators for actions, the app id

/// The application: its id, and keyboard accelerators of actions.
#[derive(Clone)]
pub struct Application(Rc<AppInner>);

struct AppInner {
    id: String,
    accels: RefCell<HashMap<String, Vec<String>>>,
    windows: RefCell<Vec<WeakRef<Widget>>>,
    /// shortcut ids per (window, action)
    installed: RefCell<HashMap<(usize, String), Vec<u64>>>,
}

thread_local! {
    static DEFAULT_APP: RefCell<Option<Application>> = const { RefCell::new(None) };
}

impl Application {
    /// Initializes Awra for application `id`.
    pub fn new(id: impl IntoOptStr) -> Application {
        let id = id.into_opt_string().unwrap_or_default();
        crate::init_with_id(&id);
        let app = Application(Rc::new(AppInner {
            id,
            accels: RefCell::new(HashMap::new()),
            windows: RefCell::new(Vec::new()),
            installed: RefCell::new(HashMap::new()),
        }));
        DEFAULT_APP.with(|d| *d.borrow_mut() = Some(app.clone()));
        app
    }
    pub fn default() -> Option<Application> {
        DEFAULT_APP.with(|d| d.borrow().clone())
    }
    pub fn application_id(&self) -> String {
        self.0.id.clone()
    }
    pub fn set_accels_for_action(&self, detailed: &str, accels: &[&str]) {
        self.0.accels.borrow_mut().insert(detailed.to_owned(), accels.iter().map(|s| s.to_string()).collect());
        let windows: Vec<Widget> = self.0.windows.borrow().iter().filter_map(|w| w.upgrade()).collect();
        for w in windows {
            self.install(&w, detailed);
        }
    }
    pub fn accels_for_action(&self, detailed: &str) -> Vec<String> {
        self.0.accels.borrow().get(detailed).cloned().unwrap_or_default()
    }
    pub(crate) fn add_window(&self, window: &Widget) {
        self.0.windows.borrow_mut().push(window.downgrade());
        let names: Vec<String> = self.0.accels.borrow().keys().cloned().collect();
        for n in names {
            self.install(window, &n);
        }
    }
    pub fn windows(&self) -> Vec<crate::Window> {
        self.0.windows.borrow().iter().filter_map(|w| w.upgrade()).filter_map(|w| w.downcast().ok()).collect()
    }
    fn install(&self, window: &Widget, detailed: &str) {
        let key = (window.as_ptr() as usize, detailed.to_owned());
        if let Some(ids) = self.0.installed.borrow_mut().remove(&key) {
            for id in ids {
                unsafe { crate::ffi::awra_window_remove_shortcut(window.as_ptr(), id as _) };
            }
        }
        let accels = self.accels_for_action(detailed);
        let (name, target) = match detailed.split_once("::") {
            Some((n, t)) => (n.to_owned(), Some(Variant(t.to_owned()))),
            None => (detailed.to_owned(), None),
        };
        let mut ids = Vec::new();
        for accel in accels {
            let name = name.clone();
            let target = target.clone();
            let id = crate::window::add_shortcut(window, &accel, move |w| {
                activate_action(w, &name, target.as_ref());
            });
            ids.push(id);
        }
        self.0.installed.borrow_mut().insert(key, ids);
    }
    /// Runs the main loop until the last window closes or `quit`.
    pub fn run(&self) {
        crate::run();
    }
    pub fn quit(&self) {
        crate::quit();
    }
}

/// A desktop notification (notify-send on Linux; Windows applications
/// use their tray icon).
#[derive(Debug, Clone, Default)]
pub struct Notification {
    title: String,
    body: Option<String>,
}

impl Notification {
    pub fn new(title: &str) -> Notification {
        Notification { title: title.to_owned(), body: None }
    }
    pub fn set_body(&mut self, body: Option<&str>) {
        self.body = body.map(str::to_owned);
    }
}

impl Application {
    /// Shows a notification; false when the desktop has no way to.
    pub fn send_notification(&self, _id: Option<&str>, n: &Notification) -> bool {
        #[cfg(unix)]
        {
            let Some(prog) = crate::glib::find_program_in_path("notify-send") else {
                return false;
            };
            let mut cmd = std::process::Command::new(prog);
            cmd.arg("--app-name").arg(&self.0.id).arg(&n.title);
            if let Some(b) = &n.body {
                cmd.arg(b);
            }
            cmd.stdin(std::process::Stdio::null()).stdout(std::process::Stdio::null()).stderr(std::process::Stdio::null());
            return cmd.spawn().map(|mut c| {
                std::thread::spawn(move || {
                    let _ = c.wait();
                });
            }).is_ok();
        }
        #[allow(unreachable_code)]
        {
            let _ = n;
            false
        }
    }
}

pub use crate::compat::{ListStore, SimpleActionGroup};
pub use crate::compat2::{AppInfo, AppLaunchContext, ApplicationFlags, ApplicationHoldGuard, Cancellable, File};

/// The action a menu item names, from `widget`: its groups and ancestors',
/// then the window's.
pub(crate) fn find_action(widget: &Widget, detailed: &str) -> Option<SimpleAction> {
    if let Some(a) = crate::compat::lookup_group_action(widget, detailed) {
        return Some(a);
    }
    let name = detailed.strip_prefix("win.").or_else(|| detailed.strip_prefix("app.")).unwrap_or(detailed);
    let window = widget.root_widget().unwrap_or_else(|| widget.clone());
    lookup_action(&window, name).or_else(|| MAPS.with(|m| m.borrow().values().find_map(|map| map.get(name).cloned())))
}
