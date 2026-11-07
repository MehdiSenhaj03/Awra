//! Data transfer in gtk-rs's shape: the clipboard with images and files
//! (`gdk::Clipboard::set_texture`, `read_texture_future`, `formats()`,
//! `read_value_future(FileList::static_type(), ..)`), textures as PNG,
//! `WidgetPaintable` snapshots, and drags out to other applications.

use std::ffi::c_void;
use std::os::raw::c_char;
use std::path::{Path, PathBuf};

use crate::ffi;
use crate::object::{cstring, from_cstr, IsWidget, Widget};

// ---------------------------------------------------------------------------
// Errors

/// glib::Error's stand-in for the transfers: a message.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Error(String);
impl Error {
    pub fn new(message: impl Into<String>) -> Error {
        Error(message.into())
    }
    pub fn message(&self) -> &str {
        &self.0
    }
}
impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(&self.0)
    }
}
impl std::error::Error for Error {}

// ---------------------------------------------------------------------------
// Types a clipboard can hold

/// What `Texture::static_type()` names: an image of any type.
#[derive(Debug, Clone, Copy, Default)]
pub struct TextureType;

/// A GType stand-in that `ContentFormats::contains_type` understands.
pub trait ContentType {
    /// Whether one of these MIME types carries it.
    fn matches(&self, mime: &str) -> bool;
}
impl ContentType for TextureType {
    fn matches(&self, mime: &str) -> bool {
        matches!(
            mime,
            "image/png" | "image/jpeg" | "image/webp" | "image/gif" | "image/svg+xml" | "image/bmp" | "image/x-bmp"
        )
    }
}
impl ContentType for crate::FileTypeMarker {
    fn matches(&self, mime: &str) -> bool {
        mime == "text/uri-list" || mime == "x-special/gnome-copied-files"
    }
}
impl ContentType for crate::Type {
    /// `glib::Type::STRING`: plain text.
    fn matches(&self, mime: &str) -> bool {
        matches!(mime, "text/plain;charset=utf-8" | "text/plain" | "UTF8_STRING" | "STRING" | "TEXT")
    }
}
impl ContentType for &str {
    fn matches(&self, mime: &str) -> bool {
        *self == mime
    }
}

/// gdk::ContentFormats: the MIME types on offer.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct ContentFormats(Vec<String>);
impl ContentFormats {
    pub fn contains_type<T: ContentType>(&self, t: T) -> bool {
        self.0.iter().any(|m| t.matches(m))
    }
    pub fn contain_mime_type(&self, mime: &str) -> bool {
        self.0.iter().any(|m| m == mime)
    }
    pub fn mime_types(&self) -> Vec<String> {
        self.0.clone()
    }
    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }
}

unsafe fn take_string(p: *mut c_char) -> Option<String> {
    if p.is_null() {
        return None;
    }
    let s = from_cstr(p);
    ffi::awra_free(p as *mut c_void);
    Some(s)
}

unsafe fn take_bytes(p: *mut c_void, len: usize) -> Option<Vec<u8>> {
    if p.is_null() {
        return None;
    }
    let v = std::slice::from_raw_parts(p as *const u8, len).to_vec();
    ffi::awra_free(p);
    Some(v)
}

fn path_cstrings(paths: &[impl AsRef<Path>]) -> (Vec<std::ffi::CString>, Vec<*const c_char>) {
    let owned: Vec<std::ffi::CString> = paths.iter().map(|p| cstring(&p.as_ref().to_string_lossy())).collect();
    let ptrs = owned.iter().map(|c| c.as_ptr()).collect();
    (owned, ptrs)
}

// ---------------------------------------------------------------------------
// The clipboard

impl crate::Clipboard {
    /// The MIME types on the clipboard now.
    pub fn formats(&self) -> ContentFormats {
        let s = unsafe { take_string(ffi::awra_clipboard_formats(self.0.as_ptr())) };
        ContentFormats(s.map(|s| s.lines().map(str::to_owned).collect()).unwrap_or_default())
    }
    /// An image (as PNG; Windows also CF_DIB).
    pub fn set_texture(&self, texture: &crate::Texture) {
        unsafe { ffi::awra_clipboard_set_texture(self.0.as_ptr(), texture.as_ptr()) };
    }
    /// Files (text/uri-list, GNOME's copied-files list, the paths as text;
    /// Windows CF_HDROP).
    pub fn set_files(&self, paths: &[impl AsRef<Path>]) {
        let (_keep, ptrs) = path_cstrings(paths);
        unsafe { ffi::awra_clipboard_set_files(self.0.as_ptr(), ptrs.as_ptr(), ptrs.len() as i32) };
    }
    /// Data under MIME types (copied).
    pub fn set_data(&self, items: &[(&str, &[u8])]) -> bool {
        let mimes: Vec<std::ffi::CString> = items.iter().map(|(m, _)| cstring(m)).collect();
        let mptr: Vec<*const c_char> = mimes.iter().map(|c| c.as_ptr()).collect();
        let data: Vec<*const c_void> = items.iter().map(|(_, d)| d.as_ptr() as *const c_void).collect();
        let lens: Vec<usize> = items.iter().map(|(_, d)| d.len()).collect();
        unsafe {
            ffi::awra_clipboard_set_data(
                self.0.as_ptr(),
                mptr.as_ptr(),
                data.as_ptr() as *mut *const c_void,
                lens.as_ptr(),
                items.len() as i32,
            )
        }
    }
    /// gdk::Clipboard::set_content: a text or a FileList value.
    pub fn set_content(&self, content: Option<&crate::ContentProvider>) -> Result<(), Error> {
        let Some(c) = content else {
            self.set_text("");
            return Ok(());
        };
        if let Ok(files) = c.0.get::<crate::FileList>() {
            self.set_files(&files.0);
            return Ok(());
        }
        if let Ok(text) = c.0.get::<String>() {
            self.set_text(&text);
            return Ok(());
        }
        if let Ok(t) = c.0.get::<crate::Texture>() {
            self.set_texture(&t);
            return Ok(());
        }
        Err(Error::new("the clipboard takes text, files or images"))
    }
    /// The clipboard's data of a MIME type.
    pub fn data(&self, mime: &str) -> Option<Vec<u8>> {
        let m = cstring(mime);
        let mut len = 0usize;
        unsafe {
            let p = ffi::awra_clipboard_get_data(self.0.as_ptr(), m.as_ptr(), &mut len);
            take_bytes(p, len)
        }
    }
    /// The clipboard's image, decoded.
    pub fn texture(&self) -> Option<crate::Texture> {
        unsafe { crate::Texture::from_raw(ffi::awra_clipboard_get_texture(self.0.as_ptr())) }
    }
    /// The clipboard's local files.
    pub fn files(&self) -> Vec<PathBuf> {
        unsafe { take_string(ffi::awra_clipboard_get_files(self.0.as_ptr())) }
            .map(|s| s.lines().filter(|l| !l.is_empty()).map(PathBuf::from).collect())
            .unwrap_or_default()
    }
    /// Resolves at once (the clipboard is read synchronously).
    pub async fn read_texture_future(&self) -> Result<Option<crate::Texture>, Error> {
        Ok(self.texture())
    }
    /// `FileList::static_type()` gives a FileList value, `glib::Type::STRING`
    /// a String, `Texture::static_type()` a Texture.
    pub async fn read_value_future<T: ContentType + 'static>(
        &self,
        t: T,
        _priority: crate::glib::Priority,
    ) -> Result<crate::Value, Error> {
        let any: &dyn std::any::Any = &t;
        if any.is::<crate::FileTypeMarker>() {
            let files = self.files();
            if files.is_empty() {
                return Err(Error::new("no files on the clipboard"));
            }
            return Ok(crate::Value::new(crate::FileList(files)));
        }
        if any.is::<TextureType>() {
            return self.texture().map(crate::Value::new).ok_or_else(|| Error::new("no image on the clipboard"));
        }
        self.text().map(crate::Value::new).ok_or_else(|| Error::new("no text on the clipboard"))
    }
}

// ---------------------------------------------------------------------------
// Textures

impl crate::Texture {
    /// What a clipboard's image types stand for (`formats().contains_type`).
    pub fn static_type() -> TextureType {
        TextureType
    }
    /// The texture as a PNG file (straight RGBA, compressed).
    pub fn save_to_png(&self, path: impl AsRef<Path>) -> Result<(), Error> {
        let p = cstring(&path.as_ref().to_string_lossy());
        if unsafe { ffi::awra_texture_save_png(self.as_ptr(), p.as_ptr()) } {
            Ok(())
        } else {
            Err(Error::new(format!("could not write {}", path.as_ref().display())))
        }
    }
    /// The texture as PNG bytes.
    pub fn save_to_png_bytes(&self) -> crate::glib::Bytes {
        let mut len = 0usize;
        let bytes = unsafe { take_bytes(ffi::awra_texture_save_png_bytes(self.as_ptr(), &mut len), len) };
        crate::glib::Bytes::from(bytes.unwrap_or_default())
    }
    /// A label of `text`, as GtkDragSource shows text it drags.
    pub fn for_text(text: &str) -> Option<crate::Texture> {
        let t = cstring(text);
        unsafe { crate::Texture::from_raw(ffi::awra_texture_new_for_text(t.as_ptr())) }
    }
    /// A symbolic icon in the foreground colour, `size` logical px.
    pub fn for_icon(name: &str, size: i32) -> Option<crate::Texture> {
        let n = cstring(name);
        unsafe { crate::Texture::from_raw(ffi::awra_texture_new_for_icon(n.as_ptr(), size)) }
    }
}

/// GtkWidgetPaintable: a widget's picture. `current_image` is the widget as
/// it is drawn now (at the window's scale).
pub struct WidgetPaintable(Option<Widget>);
impl WidgetPaintable {
    pub fn new(widget: Option<&impl IsWidget>) -> WidgetPaintable {
        WidgetPaintable(widget.map(|w| w.upcast_ref().clone()))
    }
    pub fn set_widget(&mut self, widget: Option<&impl IsWidget>) {
        self.0 = widget.map(|w| w.upcast_ref().clone());
    }
    pub fn current_image(&self) -> crate::Paintable {
        let t = self.0.as_ref().and_then(|w| unsafe { crate::Texture::from_raw(ffi::awra_widget_snapshot(w.as_ptr())) });
        crate::Paintable::new(t)
    }
}

/// The widget drawn now, as a texture (window scale).
pub fn widget_snapshot(widget: &impl IsWidget) -> Option<crate::Texture> {
    unsafe { crate::Texture::from_raw(ffi::awra_widget_snapshot(widget.upcast_ref().as_ptr())) }
}

// ---------------------------------------------------------------------------
// The image following the pointer, drags out of the application

impl crate::Window {
    /// The image a drag inside the window shows under the pointer, its
    /// texture point (hot_x, hot_y) under it; None takes it away.
    pub fn set_drag_icon(&self, icon: Option<&crate::Texture>, hot_x: i32, hot_y: i32) {
        unsafe {
            ffi::awra_window_set_drag_icon(
                IsWidget::upcast_ref::<Widget>(self).as_ptr(),
                icon.map_or(std::ptr::null_mut(), |t| t.as_ptr()),
                hot_x,
                hot_y,
            )
        }
    }
}

type DoneBox = Box<dyn FnOnce(crate::DragAction)>;

unsafe extern "C" fn drag_out_done(_w: *mut ffi::AwraWidget, a: *const ffi::AwraArgs, data: *mut c_void) {
    let f: Box<DoneBox> = Box::from_raw(data as *mut DoneBox);
    let action = match (*a).index {
        1 => crate::DragAction::COPY,
        2 => crate::DragAction::MOVE,
        _ => crate::DragAction::empty(),
    };
    f(action);
}

fn actions_ffi(actions: crate::DragAction) -> i32 {
    let mut a = 0;
    if actions.contains(crate::DragAction::COPY) {
        a |= ffi::AWRA_DRAG_COPY;
    }
    if actions.contains(crate::DragAction::MOVE) {
        a |= ffi::AWRA_DRAG_MOVE;
    }
    if a == 0 {
        ffi::AWRA_DRAG_COPY
    } else {
        a
    }
}

/// Drags files out of the application (to a file manager, the desktop, a
/// browser...): the system's drag, from the button press held now. `done`
/// gets the action the target took (empty: none). False when no drag can
/// start (no button held, no display).
pub fn drag_out_files(
    source: &impl IsWidget,
    paths: &[impl AsRef<Path>],
    icon: Option<&crate::Texture>,
    hot_x: i32,
    hot_y: i32,
    actions: crate::DragAction,
    done: impl FnOnce(crate::DragAction) + 'static,
) -> bool {
    let (_keep, ptrs) = path_cstrings(paths);
    let data = Box::into_raw(Box::new(Box::new(done) as DoneBox)) as *mut c_void;
    let ok = unsafe {
        ffi::awra_drag_start_files(
            source.upcast_ref().as_ptr(),
            ptrs.as_ptr(),
            ptrs.len() as i32,
            icon.map_or(std::ptr::null_mut(), |t| t.as_ptr()),
            hot_x,
            hot_y,
            actions_ffi(actions),
            Some(drag_out_done),
            data,
        )
    };
    if !ok {
        drop(unsafe { Box::from_raw(data as *mut DoneBox) });
    }
    ok
}

/// Drags data out of the application under MIME types (text:
/// "text/plain;charset=utf-8").
pub fn drag_out_data(
    source: &impl IsWidget,
    items: &[(&str, &[u8])],
    icon: Option<&crate::Texture>,
    hot_x: i32,
    hot_y: i32,
    actions: crate::DragAction,
    done: impl FnOnce(crate::DragAction) + 'static,
) -> bool {
    let mimes: Vec<std::ffi::CString> = items.iter().map(|(m, _)| cstring(m)).collect();
    let mptr: Vec<*const c_char> = mimes.iter().map(|c| c.as_ptr()).collect();
    let dptr: Vec<*const c_void> = items.iter().map(|(_, d)| d.as_ptr() as *const c_void).collect();
    let lens: Vec<usize> = items.iter().map(|(_, d)| d.len()).collect();
    let data = Box::into_raw(Box::new(Box::new(done) as DoneBox)) as *mut c_void;
    let ok = unsafe {
        ffi::awra_drag_start(
            source.upcast_ref().as_ptr(),
            mptr.as_ptr(),
            dptr.as_ptr() as *mut *const c_void,
            lens.as_ptr(),
            items.len() as i32,
            icon.map_or(std::ptr::null_mut(), |t| t.as_ptr()),
            hot_x,
            hot_y,
            actions_ffi(actions),
            Some(drag_out_done),
            data,
        )
    };
    if !ok {
        drop(unsafe { Box::from_raw(data as *mut DoneBox) });
    }
    ok
}

// ---------------------------------------------------------------------------
// Window identity

impl crate::Window {
    /// The window's icon by its theme name (an application ID installed in
    /// hicolor, or a registered SVG); None: the default one.
    pub fn set_icon_name(&self, name: Option<&str>) {
        let n = name.map(cstring);
        unsafe {
            ffi::awra_window_set_icon_name(
                IsWidget::upcast_ref::<Widget>(self).as_ptr(),
                n.as_ref().map_or(std::ptr::null(), |c| c.as_ptr()),
            )
        }
    }
    pub fn icon_name(&self) -> Option<String> {
        unsafe { crate::object::from_cstr_opt(ffi::awra_window_icon_name(IsWidget::upcast_ref::<Widget>(self).as_ptr())) }
    }
    /// The icon of windows that set none (gtk_window_set_default_icon_name).
    pub fn set_default_icon_name(name: &str) {
        let n = cstring(name);
        unsafe { ffi::awra_window_set_default_icon_name(n.as_ptr()) }
    }
    /// The startup notification ID (X11) or activation token (Wayland) the
    /// window is presented with; on a window shown already, it comes forward
    /// with it now.
    pub fn set_startup_id(&self, id: &str) {
        let i = cstring(id);
        unsafe { ffi::awra_window_set_startup_id(IsWidget::upcast_ref::<Widget>(self).as_ptr(), i.as_ptr()) }
    }
}
