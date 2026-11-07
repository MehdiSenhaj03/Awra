//! Icon themes as gtk-rs has them: [`IconTheme`] (GtkIconTheme),
//! [`IconPaintable`] (GtkIconPaintable), [`ThemedIcon`] (GThemedIcon) and
//! GIO's content types ([`crate::gio::content_type_get_icon`] and the other
//! `content_type_*` functions), over Awra's GTK-exact icon theme lookup.
//!
//! Every widget looks its icon names up in the theme by itself (a symbolic
//! icon tinted with the text, a full-colour one as it is): applications name
//! icons, never files, and show a file's type icon with
//! `Image::set_from_gicon(&gio::content_type_get_icon(&type))`.
//!
//! SPDX-License-Identifier: GPL-3.0-or-later

use crate::ffi;
use crate::object::cstring;
use crate::TextDirection;
use std::ffi::{c_char, c_void, CStr};
use std::path::{Path, PathBuf};
use std::ptr::NonNull;

unsafe fn take(p: *mut c_char) -> Option<String> {
    if p.is_null() {
        return None;
    }
    let s = CStr::from_ptr(p).to_string_lossy().into_owned();
    ffi::awra_free(p as *mut c_void);
    Some(s)
}

fn lines(s: Option<String>) -> Vec<String> {
    s.map(|s| s.split('\n').filter(|l| !l.is_empty()).map(str::to_owned).collect()).unwrap_or_default()
}

// ---------------------------------------------------------------------------
// GThemedIcon

/// GThemedIcon: icon names tried in turn (their symbolic / regular variants
/// after them), as widgets look them up.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct ThemedIcon(pub(crate) String);

impl ThemedIcon {
    /// `g_themed_icon_new`.
    pub fn new(name: &str) -> ThemedIcon {
        ThemedIcon(name.replace('\n', ""))
    }
    /// `g_themed_icon_new_from_names`.
    pub fn from_names(names: &[&str]) -> ThemedIcon {
        ThemedIcon(names.iter().map(|n| n.replace('\n', "")).filter(|n| !n.is_empty()).collect::<Vec<_>>().join("\n"))
    }
    /// `g_themed_icon_new_with_default_fallbacks`: "a-b-c" also tries
    /// "a-b" and "a".
    pub fn with_default_fallbacks(name: &str) -> ThemedIcon {
        let n = cstring(name);
        let p = [n.as_ptr()];
        let names = lines(unsafe { take(ffi::awra_themed_icon_names(p.as_ptr(), 1, true)) });
        ThemedIcon(names.join("\n"))
    }
    /// The first name.
    pub fn name(&self) -> &str {
        self.0.split('\n').next().unwrap_or("")
    }
    /// `g_themed_icon_get_names`: every name the theme is asked for, in order.
    pub fn names(&self) -> Vec<String> {
        let owned: Vec<_> = self.0.split('\n').map(cstring).collect();
        let ptrs: Vec<*const c_char> = owned.iter().map(|c| c.as_ptr()).collect();
        lines(unsafe { take(ffi::awra_themed_icon_names(ptrs.as_ptr(), ptrs.len() as i32, false)) })
    }
    /// `g_themed_icon_append_name`.
    pub fn append_name(&mut self, name: &str) {
        if !self.0.is_empty() {
            self.0.push('\n');
        }
        self.0.push_str(&name.replace('\n', ""));
    }
    /// `g_themed_icon_prepend_name`.
    pub fn prepend_name(&mut self, name: &str) {
        let mut s = name.replace('\n', "");
        if !self.0.is_empty() {
            s.push('\n');
        }
        self.0.insert_str(0, &s);
    }
    /// The names as one string, '\n'-separated: what an icon name argument
    /// of Awra's widgets takes for a list (menus, rows, buttons...).
    pub fn to_icon_spec(&self) -> &str {
        &self.0
    }
}

impl crate::Image {
    /// `gtk::Image::from_gicon`.
    pub fn from_gicon(icon: &ThemedIcon) -> crate::Image {
        let i = crate::Image::new();
        i.set_from_gicon(icon);
        i
    }
    /// `gtk::Image::set_from_gicon`: the first of the icon's names the theme
    /// has (a content type's icon, a list of fallbacks).
    pub fn set_from_gicon(&self, icon: &ThemedIcon) {
        let owned: Vec<_> = icon.0.split('\n').map(cstring).collect();
        let ptrs: Vec<*const c_char> = owned.iter().map(|c| c.as_ptr()).collect();
        unsafe {
            ffi::awra_image_set_from_icon_names(AsRef::<crate::Widget>::as_ref(self).as_ptr(), ptrs.as_ptr(), ptrs.len() as i32);
        }
    }
}

// ---------------------------------------------------------------------------
// GtkIconTheme

/// `gtk::IconLookupFlags`.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub struct IconLookupFlags(i32);

impl IconLookupFlags {
    pub const NONE: IconLookupFlags = IconLookupFlags(0);
    pub const FORCE_REGULAR: IconLookupFlags = IconLookupFlags(1);
    pub const FORCE_SYMBOLIC: IconLookupFlags = IconLookupFlags(2);
    pub const PRELOAD: IconLookupFlags = IconLookupFlags(4);
    pub fn empty() -> IconLookupFlags {
        IconLookupFlags(0)
    }
    pub fn bits(&self) -> i32 {
        self.0
    }
    pub fn contains(&self, other: IconLookupFlags) -> bool {
        self.0 & other.0 == other.0
    }
}

impl std::ops::BitOr for IconLookupFlags {
    type Output = IconLookupFlags;
    fn bitor(self, o: IconLookupFlags) -> IconLookupFlags {
        IconLookupFlags(self.0 | o.0)
    }
}

/// Something with a display: `gdk::Display` and a widget's `display()`.
pub trait IconThemeDisplay {}
impl IconThemeDisplay for crate::gdk::Display {}
impl IconThemeDisplay for crate::Display {}

/// GtkIconTheme: the application's icon theme (one for all its windows,
/// as GTK has one per display).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct IconTheme(());

impl IconTheme {
    /// `gtk::IconTheme::for_display`.
    pub fn for_display(_display: &impl IconThemeDisplay) -> IconTheme {
        IconTheme(())
    }
    /// The application's icon theme.
    pub fn get() -> IconTheme {
        IconTheme(())
    }
    /// The theme in use (`gtk-icon-theme-name`, or the one set).
    pub fn theme_name(&self) -> String {
        unsafe { take(ffi::awra_icon_theme_get_theme_name()) }.unwrap_or_default()
    }
    /// A theme other than the desktop's; `None`: the desktop's again.
    pub fn set_theme_name(&self, name: Option<&str>) {
        let n = name.map(cstring);
        unsafe { ffi::awra_icon_theme_set_theme_name(n.as_ref().map_or(std::ptr::null(), |c| c.as_ptr())) }
    }
    pub fn search_path(&self) -> Vec<PathBuf> {
        lines(unsafe { take(ffi::awra_icon_theme_get_search_path()) }).into_iter().map(PathBuf::from).collect()
    }
    pub fn set_search_path(&self, path: &[impl AsRef<Path>]) {
        let owned: Vec<_> = path.iter().map(|p| cstring(&p.as_ref().to_string_lossy())).collect();
        let ptrs: Vec<*const c_char> = owned.iter().map(|c| c.as_ptr()).collect();
        unsafe { ffi::awra_icon_theme_set_search_path(ptrs.as_ptr(), ptrs.len() as i32) }
    }
    pub fn add_search_path(&self, path: impl AsRef<Path>) {
        let p = cstring(&path.as_ref().to_string_lossy());
        unsafe { ffi::awra_icon_theme_add_search_path(p.as_ptr()) }
    }
    pub fn has_icon(&self, icon_name: &str) -> bool {
        let n = cstring(icon_name);
        unsafe { ffi::awra_icon_theme_has_icon(n.as_ptr()) }
    }
    /// `gtk::IconTheme::has_gicon`: any of the icon's names.
    pub fn has_gicon(&self, icon: &ThemedIcon) -> bool {
        let owned: Vec<_> = icon.0.split('\n').map(cstring).collect();
        let ptrs: Vec<*const c_char> = owned.iter().map(|c| c.as_ptr()).collect();
        unsafe { ffi::awra_icon_theme_has_names(ptrs.as_ptr(), ptrs.len() as i32) }
    }
    /// `gtk::IconTheme::lookup_icon`: the file for `size` application
    /// pixels at window `scale` (image-missing when nothing matches).
    pub fn lookup_icon(
        &self,
        icon_name: &str,
        fallbacks: &[&str],
        size: i32,
        scale: i32,
        direction: TextDirection,
        flags: IconLookupFlags,
    ) -> IconPaintable {
        let n = cstring(icon_name);
        let owned: Vec<_> = fallbacks.iter().map(|f| cstring(f)).collect();
        let mut ptrs: Vec<*const c_char> = owned.iter().map(|c| c.as_ptr()).collect();
        ptrs.push(std::ptr::null());
        let p = unsafe {
            ffi::awra_icon_theme_lookup_icon(n.as_ptr(), ptrs.as_ptr(), size.max(1), scale.max(1), direction as i32, flags.0)
        };
        IconPaintable(NonNull::new(p).expect("icon lookup out of memory"))
    }
    /// `gtk::IconTheme::lookup_by_gicon`.
    pub fn lookup_by_gicon(
        &self,
        icon: &ThemedIcon,
        size: i32,
        scale: i32,
        direction: TextDirection,
        flags: IconLookupFlags,
    ) -> IconPaintable {
        let owned: Vec<_> = icon.0.split('\n').map(cstring).collect();
        let ptrs: Vec<*const c_char> = owned.iter().map(|c| c.as_ptr()).collect();
        let p = unsafe {
            ffi::awra_icon_theme_lookup_names(ptrs.as_ptr(), ptrs.len() as i32, size.max(1), scale.max(1), direction as i32, flags.0)
        };
        IconPaintable(NonNull::new(p).expect("icon lookup out of memory"))
    }
    /// `changed`: another theme, files installed or removed. Windows draw
    /// their icons again by themselves; this is for what the application
    /// made of icons (textures).
    pub fn connect_changed<F: Fn(&IconTheme) + 'static>(&self, f: F) -> IconThemeHandlerId {
        unsafe extern "C" fn tramp(data: *mut c_void) {
            let f = &*(data as *const Box<dyn Fn(&IconTheme)>);
            f(&IconTheme(()));
        }
        let boxed: Box<Box<dyn Fn(&IconTheme)>> = Box::new(Box::new(f));
        let data = Box::into_raw(boxed) as *mut c_void;
        let id = unsafe { ffi::awra_icon_theme_connect_changed(Some(tramp), data) };
        IconThemeHandlerId { id, data }
    }
    pub fn disconnect(&self, handler: IconThemeHandlerId) {
        unsafe {
            ffi::awra_icon_theme_disconnect(handler.id);
            drop(Box::from_raw(handler.data as *mut Box<dyn Fn(&IconTheme)>));
        }
    }
}

/// A `connect_changed` handler, for `disconnect`.
#[derive(Debug)]
pub struct IconThemeHandlerId {
    id: std::os::raw::c_ulong,
    data: *mut c_void,
}

// ---------------------------------------------------------------------------
// GtkIconPaintable

/// GtkIconPaintable: the file a lookup chose, at its size and scale.
#[derive(Debug)]
pub struct IconPaintable(NonNull<ffi::AwraIconPaintable>);

impl IconPaintable {
    /// `gtk::IconPaintable::for_file`.
    pub fn for_file(file: &crate::gio::File, size: i32, scale: i32) -> Option<IconPaintable> {
        let path = file.path()?;
        let p = cstring(&path.to_string_lossy());
        NonNull::new(unsafe { ffi::awra_icon_paintable_new_for_file(p.as_ptr(), size.max(1), scale.max(1)) })
            .map(IconPaintable)
    }
    /// The file shown; `None` for an icon built into Awra (GTK's resource
    /// icons) or one the application registered.
    pub fn file(&self) -> Option<crate::gio::File> {
        let p = unsafe { ffi::awra_icon_paintable_get_file(self.0.as_ptr()) };
        (!p.is_null()).then(|| crate::gio::File::for_path(unsafe { CStr::from_ptr(p) }.to_string_lossy().as_ref()))
    }
    /// The name chosen (image-missing when nothing matched).
    pub fn icon_name(&self) -> Option<PathBuf> {
        let p = unsafe { ffi::awra_icon_paintable_get_icon_name(self.0.as_ptr()) };
        (!p.is_null()).then(|| PathBuf::from(unsafe { CStr::from_ptr(p) }.to_string_lossy().into_owned()))
    }
    /// Whether it is tinted with the text colour (a symbolic file).
    pub fn is_symbolic(&self) -> bool {
        unsafe { ffi::awra_icon_paintable_is_symbolic(self.0.as_ptr()) }
    }
    pub fn size(&self) -> i32 {
        unsafe { ffi::awra_icon_paintable_get_size(self.0.as_ptr()) }
    }
    pub fn scale(&self) -> i32 {
        unsafe { ffi::awra_icon_paintable_get_scale(self.0.as_ptr()) }
    }
    /// Drawn at size x scale pixels, a symbolic icon in `color` (`None`:
    /// the text colour) and GTK's success / warning / error colours.
    pub fn render(&self, color: Option<&crate::gdk::RGBA>) -> Option<crate::Texture> {
        let c = color.map(|c| ffi::AwraRGBA { r: c.red(), g: c.green(), b: c.blue(), a: c.alpha() });
        unsafe {
            crate::Texture::from_raw(ffi::awra_icon_paintable_render(
                self.0.as_ptr(),
                c.as_ref().map_or(std::ptr::null(), |c| c as *const ffi::AwraRGBA),
            ))
        }
    }
}

impl Clone for IconPaintable {
    fn clone(&self) -> IconPaintable {
        unsafe { ffi::awra_icon_paintable_ref(self.0.as_ptr()) };
        IconPaintable(self.0)
    }
}

impl Drop for IconPaintable {
    fn drop(&mut self) {
        unsafe { ffi::awra_icon_paintable_unref(self.0.as_ptr()) }
    }
}

// ---------------------------------------------------------------------------
// GIO content types

fn ct1(f: unsafe extern "C" fn(*const c_char) -> *mut c_char, t: &str) -> Option<String> {
    let c = cstring(t);
    unsafe { take(f(c.as_ptr())) }
}

/// `gio::content_type_guess`: the type of a file name and/or the start of
/// its data, and whether it is a guess.
pub fn content_type_guess(filename: Option<impl AsRef<Path>>, data: &[u8]) -> (String, bool) {
    let f = filename.map(|f| cstring(&f.as_ref().to_string_lossy()));
    let mut uncertain = false;
    let t = unsafe {
        take(ffi::awra_content_type_guess(
            f.as_ref().map_or(std::ptr::null(), |c| c.as_ptr()),
            if data.is_empty() { std::ptr::null() } else { data.as_ptr() as *const c_void },
            data.len(),
            &mut uncertain,
        ))
    };
    (t.unwrap_or_else(|| "application/octet-stream".to_owned()), uncertain)
}

/// The content type GIO gives a file on disk (`standard::content-type`, or
/// `standard::fast-content-type` with `fast`: the name only).
pub fn content_type_for_file(path: impl AsRef<Path>, fast: bool) -> String {
    let p = cstring(&path.as_ref().to_string_lossy());
    unsafe { take(ffi::awra_content_type_for_file(p.as_ptr(), fast)) }
        .unwrap_or_else(|| "application/octet-stream".to_owned())
}

/// `gio::content_type_get_icon`.
pub fn content_type_get_icon(type_: &str) -> ThemedIcon {
    ThemedIcon(lines(ct1(ffi::awra_content_type_get_icon_names, type_)).join("\n"))
}

/// `gio::content_type_get_symbolic_icon`.
pub fn content_type_get_symbolic_icon(type_: &str) -> ThemedIcon {
    ThemedIcon(lines(ct1(ffi::awra_content_type_get_symbolic_icon_names, type_)).join("\n"))
}

pub fn content_type_get_generic_icon_name(type_: &str) -> Option<String> {
    ct1(ffi::awra_content_type_get_generic_icon_name, type_)
}

/// `gio::content_type_get_description`: in the user's language.
pub fn content_type_get_description(type_: &str) -> String {
    ct1(ffi::awra_content_type_get_description, type_).unwrap_or_default()
}

pub fn content_type_get_mime_type(type_: &str) -> Option<String> {
    ct1(ffi::awra_content_type_get_mime_type, type_)
}

pub fn content_type_from_mime_type(mime_type: &str) -> Option<String> {
    ct1(ffi::awra_content_type_from_mime_type, mime_type)
}

pub fn content_type_is_a(type_: &str, supertype: &str) -> bool {
    let (a, b) = (cstring(type_), cstring(supertype));
    unsafe { ffi::awra_content_type_is_a(a.as_ptr(), b.as_ptr()) }
}

pub fn content_type_is_mime_type(type_: &str, mime_type: &str) -> bool {
    let (a, b) = (cstring(type_), cstring(mime_type));
    unsafe { ffi::awra_content_type_is_mime_type(a.as_ptr(), b.as_ptr()) }
}

pub fn content_type_equals(type1: &str, type2: &str) -> bool {
    let (a, b) = (cstring(type1), cstring(type2));
    unsafe { ffi::awra_content_type_equals(a.as_ptr(), b.as_ptr()) }
}

pub fn content_type_is_unknown(type_: &str) -> bool {
    let a = cstring(type_);
    unsafe { ffi::awra_content_type_is_unknown(a.as_ptr()) }
}

pub fn content_type_can_be_executable(type_: &str) -> bool {
    let a = cstring(type_);
    unsafe { ffi::awra_content_type_can_be_executable(a.as_ptr()) }
}
