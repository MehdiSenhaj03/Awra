//! Widget handles: reference counted, compared by identity, with weak
//! references and checked down-casts.

use std::cell::Cell;
use std::ffi::{c_char, c_void, CStr, CString};
use std::fmt;
use std::hash::{Hash, Hasher};
use std::ptr::NonNull;
use std::rc::Rc;

use crate::ffi;

/// Any Awra widget. Cloning takes a reference, dropping releases it.
pub struct Widget(NonNull<ffi::AwraWidget>);

impl Widget {
    /// Wraps a pointer the caller owns a reference to.
    ///
    /// # Safety
    /// `ptr` must be a live Awra widget with a reference owned by the caller.
    pub unsafe fn from_full(ptr: *mut ffi::AwraWidget) -> Widget {
        Widget(NonNull::new(ptr).expect("Awra returned a NULL widget"))
    }

    /// Wraps a borrowed pointer, taking a new reference.
    ///
    /// # Safety
    /// `ptr` must be a live Awra widget.
    pub unsafe fn from_none(ptr: *mut ffi::AwraWidget) -> Widget {
        ffi::awra_widget_ref(ptr);
        Widget::from_full(ptr)
    }

    /// # Safety
    /// As [`Widget::from_none`], or NULL.
    pub unsafe fn from_none_opt(ptr: *mut ffi::AwraWidget) -> Option<Widget> {
        if ptr.is_null() {
            None
        } else {
            Some(Widget::from_none(ptr))
        }
    }

    pub fn as_ptr(&self) -> *mut ffi::AwraWidget {
        self.0.as_ptr()
    }

    pub fn type_name(&self) -> String {
        unsafe { from_cstr(ffi::awra_widget_type(self.as_ptr())) }
    }

    /// True when the widget is a `T` (or derives from one).
    pub fn is<T: StaticType>(&self) -> bool {
        T::matches(self)
    }

    pub fn downcast<T: StaticType>(self) -> Result<T, Widget> {
        if T::matches(&self) {
            Ok(unsafe { T::unchecked_from(self) })
        } else {
            Err(self)
        }
    }

    pub fn downcast_ref<T: StaticType>(&self) -> Option<T> {
        self.clone().downcast().ok()
    }

    pub fn downgrade(&self) -> WeakRef<Widget> {
        WeakRef::new(self)
    }
}

impl Clone for Widget {
    fn clone(&self) -> Widget {
        unsafe { Widget::from_none(self.as_ptr()) }
    }
}

impl Drop for Widget {
    fn drop(&mut self) {
        unsafe { ffi::awra_widget_unref(self.as_ptr()) }
    }
}

impl PartialEq for Widget {
    fn eq(&self, other: &Widget) -> bool {
        self.0 == other.0
    }
}
impl Eq for Widget {}
impl Hash for Widget {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.0.hash(state)
    }
}
impl fmt::Debug for Widget {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}({:p})", self.type_name(), self.0)
    }
}

/// Typed widgets: a type name check and an unchecked conversion.
pub trait StaticType: Sized + Clone {
    fn matches(widget: &Widget) -> bool;
    /// # Safety
    /// `widget` must match the type.
    unsafe fn unchecked_from(widget: Widget) -> Self;
}

impl StaticType for Widget {
    fn matches(_: &Widget) -> bool {
        true
    }
    unsafe fn unchecked_from(widget: Widget) -> Widget {
        widget
    }
}

/// Everything that is a widget.
pub trait IsWidget: AsRef<Widget> {
    fn upcast_ref(&self) -> &Widget {
        self.as_ref()
    }
    fn upcast(&self) -> Widget {
        self.as_ref().clone()
    }
    fn widget_ptr(&self) -> *mut ffi::AwraWidget {
        self.as_ref().as_ptr()
    }
}
impl<T: AsRef<Widget>> IsWidget for T {}

impl AsRef<Widget> for Widget {
    fn as_ref(&self) -> &Widget {
        self
    }
}

/// Declares a typed widget handle over `Widget` (or another typed handle).
#[macro_export]
#[doc(hidden)]
macro_rules! awra_wrapper {
    ($(#[$m:meta])* $name:ident, [$($ty:literal),+] $(, $parent:ty)?) => {
        $(#[$m])*
        #[derive(Clone, PartialEq, Eq, Hash, Debug)]
        #[repr(transparent)]
        pub struct $name($crate::Widget);

        impl $crate::StaticType for $name {
            fn matches(widget: &$crate::Widget) -> bool {
                [$($ty),+].iter().any(|t| $crate::object::widget_is(widget, t))
            }
            unsafe fn unchecked_from(widget: $crate::Widget) -> Self {
                $name(widget)
            }
        }
        impl AsRef<$crate::Widget> for $name {
            fn as_ref(&self) -> &$crate::Widget {
                &self.0
            }
        }
        impl std::ops::Deref for $name {
            type Target = $crate::Widget;
            fn deref(&self) -> &$crate::Widget {
                &self.0
            }
        }
        impl From<$name> for $crate::Widget {
            fn from(w: $name) -> $crate::Widget {
                w.0
            }
        }
        impl $name {
            #[allow(dead_code)]
            pub(crate) unsafe fn from_full(ptr: *mut $crate::ffi::AwraWidget) -> $name {
                $name($crate::Widget::from_full(ptr))
            }
            pub fn downgrade(&self) -> $crate::WeakRef<$name> {
                $crate::WeakRef::new(self)
            }
        }
    };
}

pub fn widget_is(widget: &Widget, type_name: &str) -> bool {
    let t = cstring(type_name);
    unsafe { ffi::awra_widget_is(widget.as_ptr(), t.as_ptr()) }
}

/// A weak reference: does not keep the widget alive; `upgrade` returns it
/// while something else does.
pub struct WeakRef<T> {
    ptr: *mut ffi::AwraWidget,
    alive: Rc<Cell<bool>>,
    _t: std::marker::PhantomData<T>,
}

const LIFE_KEY: &CStr = c"awra-rs-life";

unsafe extern "C" fn life_free(data: *mut c_void) {
    let alive = Rc::from_raw(data as *const Cell<bool>);
    alive.set(false);
}

impl<T: StaticType + AsRef<Widget>> WeakRef<T> {
    pub fn new(w: &T) -> WeakRef<T> {
        let widget = w.as_ref();
        let alive = unsafe {
            let data = ffi::awra_widget_get_data(widget.as_ptr(), LIFE_KEY.as_ptr());
            if data.is_null() {
                let alive = Rc::new(Cell::new(true));
                ffi::awra_widget_set_data(
                    widget.as_ptr(),
                    LIFE_KEY.as_ptr(),
                    Rc::into_raw(alive.clone()) as *mut c_void,
                    Some(life_free),
                );
                alive
            } else {
                let rc = data as *const Cell<bool>;
                Rc::increment_strong_count(rc);
                Rc::from_raw(rc)
            }
        };
        WeakRef { ptr: widget.as_ptr(), alive, _t: std::marker::PhantomData }
    }

    pub fn upgrade(&self) -> Option<T> {
        if self.alive.get() {
            Some(unsafe { T::unchecked_from(Widget::from_none(self.ptr)) })
        } else {
            None
        }
    }
}

impl<T> Clone for WeakRef<T> {
    fn clone(&self) -> WeakRef<T> {
        WeakRef { ptr: self.ptr, alive: self.alive.clone(), _t: std::marker::PhantomData }
    }
}

impl<T> Default for WeakRef<T> {
    fn default() -> WeakRef<T> {
        WeakRef { ptr: std::ptr::null_mut(), alive: Rc::new(Cell::new(false)), _t: std::marker::PhantomData }
    }
}

/// `CString` from Rust text, dropping interior NULs.
pub(crate) fn cstring(s: &str) -> CString {
    CString::new(s).unwrap_or_else(|_| CString::new(s.replace('\0', "")).unwrap())
}

/// # Safety
/// `p` is NULL or a valid C string.
pub(crate) unsafe fn from_cstr(p: *const c_char) -> String {
    if p.is_null() {
        String::new()
    } else {
        CStr::from_ptr(p).to_string_lossy().into_owned()
    }
}

/// # Safety
/// `p` is NULL or a valid C string.
pub(crate) unsafe fn from_cstr_opt(p: *const c_char) -> Option<String> {
    if p.is_null() {
        None
    } else {
        Some(CStr::from_ptr(p).to_string_lossy().into_owned())
    }
}

/// Text arguments: `"x"`, `String`, `Some("x")`, `None`.
pub trait IntoOptStr {
    fn into_opt_string(self) -> Option<String>;
}
impl IntoOptStr for &str {
    fn into_opt_string(self) -> Option<String> {
        Some(self.to_owned())
    }
}
impl IntoOptStr for &&str {
    fn into_opt_string(self) -> Option<String> {
        Some((*self).to_owned())
    }
}
impl IntoOptStr for String {
    fn into_opt_string(self) -> Option<String> {
        Some(self)
    }
}
impl IntoOptStr for &String {
    fn into_opt_string(self) -> Option<String> {
        Some(self.clone())
    }
}
impl IntoOptStr for Option<&str> {
    fn into_opt_string(self) -> Option<String> {
        self.map(str::to_owned)
    }
}
impl IntoOptStr for Option<String> {
    fn into_opt_string(self) -> Option<String> {
        self
    }
}
impl IntoOptStr for Option<&String> {
    fn into_opt_string(self) -> Option<String> {
        self.cloned()
    }
}

/// Holds an optional C string for the duration of a call.
pub(crate) struct OptC(Option<CString>);
impl OptC {
    pub fn new(s: impl IntoOptStr) -> OptC {
        OptC(s.into_opt_string().map(|s| cstring(&s)))
    }
    pub fn ptr(&self) -> *const c_char {
        self.0.as_ref().map_or(std::ptr::null(), |c| c.as_ptr())
    }
}

/// Child arguments: `&w`, `Some(&w)`, `None::<&Widget>`.
pub trait IntoOptWidget {
    fn into_opt_ptr(self) -> *mut ffi::AwraWidget;
}
impl<T: AsRef<Widget>> IntoOptWidget for &T {
    fn into_opt_ptr(self) -> *mut ffi::AwraWidget {
        self.as_ref().as_ptr()
    }
}
impl<T: AsRef<Widget>> IntoOptWidget for Option<&T> {
    fn into_opt_ptr(self) -> *mut ffi::AwraWidget {
        self.map_or(std::ptr::null_mut(), |w| w.as_ref().as_ptr())
    }
}
