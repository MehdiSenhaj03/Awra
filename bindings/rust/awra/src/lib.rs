//! Awra 2 for Rust: a frosted-glass desktop UI framework (no GTK).
//!
//! The API follows gtk-rs names where Awra 1 applications used them
//! (`Box`, `Label`, `connect_clicked`, `glib::timeout_add_local`, actions
//! named `win.*`…) so that applications port with little change, over a
//! small C library: Awra's components on BUI's retained kit, drawn in
//! software into the compositor's buffers.
//!
//! Everything but [`glib::invoke`] runs on the thread that called [`init`].
//!
//! SPDX-License-Identifier: GPL-3.0-or-later

#![allow(clippy::new_without_default)]
#![allow(clippy::missing_safety_doc)]

pub use awra_sys as ffi;

pub mod object;
pub mod accessible;
mod compat;
mod compat2;
mod compat3;
mod content;
mod signal;
mod enums;
mod widget;
mod widgets;
mod collection;
mod tilegrid;
pub use tilegrid::TileGrid;
mod window;
mod video;
mod texture_data;
mod files;
pub use files::{DragPreview, TransitionBin};
mod dnd;
mod transfer;
mod metric;
mod accel;
mod transitions;
pub use transitions::widget_finish_transitions;
pub use accel::accelerator_get_label;
pub use metric::MetricRow;
mod gtk_look;
mod icontheme;
pub use icontheme::{IconLookupFlags, IconPaintable, IconTheme, IconThemeDisplay, IconThemeHandlerId};
pub mod gdk;
pub mod gio;
pub mod glib;
pub mod pango;

use std::boxed::Box as StdBox;
use std::ffi::c_void;
use std::path::PathBuf;
use std::cell::RefCell;
use std::rc::Rc;

pub use enums::*;
pub use accessible::{
    AccessibleAnnouncementPriority, AccessibleAutocomplete, AccessibleInvalidState, AccessibleRole,
    AccessibleSort, AccessibleTristate,
};
pub use gio::Application;
pub use object::{IntoOptStr, IntoOptWidget, IsWidget, StaticType, WeakRef, Widget};
pub use signal::{raw_args as signal_args, sig, Args, SignalHandlerId};
pub use widget::*;
pub use widgets::*;
pub use video::*;
pub use texture_data::*;
pub use collection::*;
pub use compat::*;
pub use compat2::*;
pub use compat3::*;
pub use pango::WrapMode;
pub use content::*;
pub use transfer::{drag_out_data, drag_out_files, widget_snapshot, ContentFormats, ContentType, TextureType, WidgetPaintable};
pub use window::{accel_label, ApplicationWindow, Window};

pub mod prelude {
    //! Traits for method syntax (gtk-rs's prelude).
    pub use crate::gio::ToVariant;
    pub use crate::object::{IsWidget, StaticType};
    pub use crate::widget::{Cast, Controller};
    pub use crate::content::EmojiItemExt;
    pub use crate::compat2::{StaticTypeExt, WidgetExt};
    pub use crate::compat3::{GtkWindowExt, IsPaintable, StringObjectDowncast, ToValue, TooltipExt, WidgetExt3};

    /// gtk-rs `and_downcast`: an optional widget as a `T`.
    pub trait OptionDowncast {
        fn and_downcast<T: crate::object::StaticType>(self) -> Option<T>;
    }
    impl<W: Into<crate::Widget>> OptionDowncast for Option<W> {
        fn and_downcast<T: crate::object::StaticType>(self) -> Option<T> {
            self.and_then(|w| w.into().downcast().ok())
        }
    }

    /// gtk-rs's `IsA<T>` bound: anything that is a `T`.
    pub trait IsA<T>: AsRef<T> + Clone {}
    impl<T, U: AsRef<T> + Clone> IsA<T> for U {}
}

fn init_with_id(id: &str) {
    let c = object::cstring(id);
    unsafe { ffi::awra_init(c.as_ptr()) }
}

/// Initializes Awra (idempotent). Applications usually go through
/// [`Application::new`], which names the app to the desktop.
pub fn init() {
    unsafe { ffi::awra_init(std::ptr::null()) }
}

/// Runs the main loop until [`quit`] (or the last window closes, unless
/// [`set_keep_alive`]).
pub fn run() {
    unsafe { ffi::awra_run() }
}

pub fn quit() {
    unsafe { ffi::awra_quit() }
}

/// Keeps the loop running with no window shown (tray applications).
pub fn set_keep_alive(keep: bool) {
    unsafe { ffi::awra_set_keep_alive(keep) }
}

/// Handles pending events without blocking; returns the open window count.
pub fn pump() -> i32 {
    unsafe { ffi::awra_pump() }
}

/// "wayland", "x11" or "win32".
pub fn backend() -> String {
    unsafe { object::from_cstr_opt(ffi::awra_backend()) }.unwrap_or_default()
}

pub fn now_ms() -> f64 {
    unsafe { ffi::awra_now_ms() }
}

// ---------------------------------------------------------------------------
// Style

/// The style: appearance, accent, motion (Awra 1's StyleManager).
#[derive(Clone, Copy, Debug, Default)]
pub struct StyleManager;

impl StyleManager {
    pub fn default() -> StyleManager {
        StyleManager
    }
    pub fn set_appearance(&self, a: Appearance) {
        unsafe { ffi::awra_style_set_appearance(a as _) }
    }
    pub fn appearance(&self) -> Appearance {
        match unsafe { ffi::awra_style_appearance() } {
            1 => Appearance::Light,
            2 => Appearance::Dark,
            _ => Appearance::System,
        }
    }
    pub fn is_dark(&self) -> bool {
        unsafe { ffi::awra_style_is_dark() }
    }
    pub fn set_accent_source(&self, s: AccentSource) {
        unsafe { ffi::awra_style_set_accent_source(s as _) }
    }
    pub fn accent_source(&self) -> AccentSource {
        match unsafe { ffi::awra_style_accent_source() } {
            1 => AccentSource::System,
            2 => AccentSource::Custom,
            _ => AccentSource::Default,
        }
    }
    pub fn set_accent_color(&self, c: &gdk::RGBA) {
        let v = c.to_ffi();
        unsafe {
            ffi::awra_style_set_accent(&v);
            ffi::awra_style_set_accent_source(AccentSource::Custom as _);
        }
    }
    pub fn set_reduced_motion(&self, on: bool) {
        unsafe { ffi::awra_style_set_reduced_motion(on) }
    }
    pub fn reduced_motion(&self) -> bool {
        unsafe { ffi::awra_style_reduced_motion() }
    }
    pub fn set_reduced_transparency(&self, on: bool) {
        unsafe { ffi::awra_style_set_reduced_transparency(on) }
    }
    pub fn set_high_contrast(&self, on: bool) {
        unsafe { ffi::awra_style_set_high_contrast(on) }
    }
    pub fn blur_active(&self) -> bool {
        unsafe { ffi::awra_style_blur_active() }
    }
    pub fn color(&self, name: &str) -> Option<gdk::RGBA> {
        let n = object::cstring(name);
        let mut c = ffi::AwraRGBA { r: 0.0, g: 0.0, b: 0.0, a: 0.0 };
        unsafe { ffi::awra_style_color(n.as_ptr(), &mut c) }.then(|| gdk::RGBA::from_ffi(c))
    }
    /// Calls `f` after every style change.
    pub fn connect_changed<F: Fn() + 'static>(&self, f: F) -> u64 {
        type ChangedFn = StdBox<dyn Fn()>;
        unsafe extern "C" fn tramp(data: *mut c_void) {
            let f = &*(data as *const ChangedFn);
            f()
        }
        let data: StdBox<ChangedFn> = StdBox::new(StdBox::new(f));
        // lives as long as the process (style listeners are few)
        unsafe { ffi::awra_style_connect_changed(Some(tramp), StdBox::into_raw(data) as *mut c_void) as u64 }
    }
}

/// Awra 1's per-display context.
#[derive(Clone, Copy, Debug, Default)]
pub struct Context;

impl Context {
    pub fn for_display<T>(_display: &T) -> Context {
        Context
    }
    pub fn default() -> Context {
        Context
    }
    pub fn style_manager(&self) -> StyleManager {
        StyleManager
    }
}

/// Duration of a motion preset in ms (0 with reduced motion).
pub fn motion_duration(_manager: &StyleManager, preset: MotionPreset) -> u32 {
    unsafe { ffi::awra_motion_duration(preset as _) }
}

pub fn spacing(s: Spacing) -> i32 {
    unsafe { ffi::awra_spacing(s as _) }
}

pub fn radius(r: Radius) -> i32 {
    unsafe { ffi::awra_radius(r as _) }
}

// ---------------------------------------------------------------------------
// Keys

/// The label the current keyboard layout prints on a physical key ("KeyQ"
/// → "A" on AZERTY).
/// Calls `f` whenever the keyboard layout changes.
pub fn connect_keyboard_layout_changed<F: Fn() + 'static>(f: F) {
    type LayoutFn = StdBox<dyn Fn()>;
    unsafe extern "C" fn tramp(data: *mut c_void) {
        let f = &*(data as *const LayoutFn);
        f()
    }
    let data: StdBox<LayoutFn> = StdBox::new(StdBox::new(f));
    unsafe { ffi::awra_keyboard_connect_layout_changed(Some(tramp), StdBox::into_raw(data) as *mut c_void) }
}

pub fn key_code_label(code: &str) -> Option<String> {
    let c = object::cstring(code);
    unsafe { object::from_cstr_opt(ffi::awra_key_code_label(c.as_ptr())) }
}

// ---------------------------------------------------------------------------
// Native dialogs, URIs

/// A file type filter of file dialogs (GtkFileFilter).
#[derive(Debug, Clone, Default)]
pub struct FileFilter(Rc<RefCell<(String, Vec<String>)>>);

impl FileFilter {
    pub fn new() -> FileFilter {
        FileFilter::default()
    }
    pub fn set_name(&self, name: Option<&str>) {
        self.0.borrow_mut().0 = name.unwrap_or_default().to_owned();
    }
    pub fn add_pattern(&self, pattern: &str) {
        let mut d = self.0.borrow_mut();
        if !d.1.iter().any(|p| p == pattern) {
            d.1.push(pattern.to_owned());
        }
    }
    pub fn add_suffix(&self, suffix: &str) {
        self.add_pattern(&format!("*.{suffix}"));
    }
}

/// Why a file dialog gave no file (glib::Error).
#[derive(Debug, Clone)]
pub struct DialogError(pub String);
impl std::fmt::Display for DialogError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(&self.0)
    }
}

type FileDone = StdBox<dyn FnOnce(Option<Vec<PathBuf>>)>;
unsafe extern "C" fn file_done(paths: *const std::ffi::c_char, data: *mut c_void) {
    let f = StdBox::from_raw(data as *mut FileDone);
    let r = object::from_cstr_opt(paths).map(|s| s.lines().filter(|l| !l.is_empty()).map(PathBuf::from).collect());
    f(r)
}

#[derive(Debug, Default)]
struct DialogData {
    title: String,
    filters: Vec<FileFilter>,
    initial_name: Option<String>,
}

/// Native file dialogs: open files or a folder, save a file (GtkFileDialog).
#[derive(Debug, Clone, Default)]
pub struct FileDialog(Rc<RefCell<DialogData>>);

fn picked(r: Option<Vec<PathBuf>>) -> Result<gio::File, DialogError> {
    r.and_then(|v| v.into_iter().next()).map(gio::File::for_path).ok_or_else(|| DialogError("dismissed".into()))
}

impl FileDialog {
    pub fn new() -> FileDialog {
        FileDialog::default()
    }
    pub fn builder() -> FileDialogBuilder {
        FileDialogBuilder(FileDialog::default())
    }
    pub fn set_title(&self, t: &str) {
        self.0.borrow_mut().title = t.to_owned()
    }
    pub fn add_filter(&self, f: &FileFilter) {
        self.0.borrow_mut().filters.push(f.clone())
    }
    /// The filters offered (the first is the default).
    pub fn set_filters(&self, filters: &[FileFilter]) {
        self.0.borrow_mut().filters = filters.to_vec()
    }
    pub fn set_default_filter(&self, f: Option<&FileFilter>) {
        if let Some(f) = f {
            let mut d = self.0.borrow_mut();
            d.filters.retain(|x| !Rc::ptr_eq(&x.0, &f.0));
            d.filters.insert(0, f.clone());
        }
    }
    pub fn set_initial_name(&self, name: Option<&str>) {
        self.0.borrow_mut().initial_name = name.map(str::to_owned)
    }
    fn c_filters(&self) -> (Vec<std::ffi::CString>, Vec<ffi::AwraFileFilter>) {
        let mut keep = Vec::new();
        let mut out = Vec::new();
        for f in &self.0.borrow().filters {
            let f = f.0.borrow();
            keep.push(object::cstring(&f.0));
            let n = keep.last().unwrap().as_ptr();
            keep.push(object::cstring(&f.1.join(";")));
            let p = keep.last().unwrap().as_ptr();
            out.push(ffi::AwraFileFilter { name: n, patterns: p });
        }
        (keep, out)
    }
    fn run(&self, parent: Option<&Window>, multiple: bool, folder: bool, done: FileDone) {
        let t = object::cstring(&self.0.borrow().title);
        let (_keep, filters) = self.c_filters();
        let data: StdBox<FileDone> = StdBox::new(done);
        unsafe {
            ffi::awra_file_dialog_open(
                parent.map_or(std::ptr::null_mut(), |p| p.as_ptr()),
                t.as_ptr(),
                multiple,
                folder,
                filters.as_ptr(),
                filters.len() as i32,
                Some(file_done),
                StdBox::into_raw(data) as *mut c_void,
            )
        }
    }
    /// One file; an error when dismissed.
    pub fn open<F: FnOnce(Result<gio::File, DialogError>) + 'static>(
        &self,
        parent: Option<&Window>,
        _cancellable: Option<&gio::Cancellable>,
        done: F,
    ) {
        self.run(parent, false, false, StdBox::new(move |r| done(picked(r))))
    }
    pub fn open_multiple<F: FnOnce(Result<Vec<gio::File>, DialogError>) + 'static>(
        &self,
        parent: Option<&Window>,
        _cancellable: Option<&gio::Cancellable>,
        done: F,
    ) {
        self.run(
            parent,
            true,
            false,
            StdBox::new(move |r: Option<Vec<PathBuf>>| {
                done(r.map(|v| v.into_iter().map(gio::File::for_path).collect()).ok_or_else(|| DialogError("dismissed".into())))
            }),
        )
    }
    pub fn select_folder<F: FnOnce(Result<gio::File, DialogError>) + 'static>(
        &self,
        parent: Option<&Window>,
        _cancellable: Option<&gio::Cancellable>,
        done: F,
    ) {
        self.run(parent, false, true, StdBox::new(move |r| done(picked(r))))
    }
    pub fn save<F: FnOnce(Result<gio::File, DialogError>) + 'static>(
        &self,
        parent: Option<&Window>,
        _cancellable: Option<&gio::Cancellable>,
        done: F,
    ) {
        let d = self.0.borrow();
        let t = object::cstring(&d.title);
        let name = d.initial_name.as_deref().map(object::cstring);
        drop(d);
        let (_keep, filters) = self.c_filters();
        let data: StdBox<FileDone> = StdBox::new(StdBox::new(move |r: Option<Vec<PathBuf>>| done(picked(r))));
        unsafe {
            ffi::awra_file_dialog_save(
                parent.map_or(std::ptr::null_mut(), |p| p.as_ptr()),
                t.as_ptr(),
                name.as_ref().map_or(std::ptr::null(), |n| n.as_ptr()),
                filters.as_ptr(),
                filters.len() as i32,
                Some(file_done),
                StdBox::into_raw(data) as *mut c_void,
            )
        }
    }
}

pub struct FileDialogBuilder(FileDialog);
impl FileDialogBuilder {
    pub fn title(self, t: &str) -> Self {
        self.0.set_title(t);
        self
    }
    pub fn filter(self, f: &FileFilter) -> Self {
        self.0.add_filter(f);
        self
    }
    /// Several filters at once (gtk-rs takes a list model).
    pub fn filters(self, filters: &[FileFilter]) -> Self {
        for f in filters {
            self.0.add_filter(f);
        }
        self
    }
    pub fn initial_name(self, n: &str) -> Self {
        self.0.set_initial_name(Some(n));
        self
    }
    pub fn modal(self, _m: bool) -> Self {
        self
    }
    pub fn build(self) -> FileDialog {
        self.0
    }
}

/// Opens a URI or file with the desktop's default handler.
pub fn show_uri(uri: &str) -> bool {
    let u = object::cstring(uri);
    unsafe { ffi::awra_show_uri(u.as_ptr()) }
}
