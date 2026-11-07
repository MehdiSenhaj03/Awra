//! Signal connection: Rust closures behind Awra's C handlers.

use std::ffi::c_void;

use crate::ffi;
use crate::object::{from_cstr_opt, StaticType, Widget};


/// Signal ids (AwraSignal).
#[allow(non_upper_case_globals)]
pub mod sig {
    use crate::ffi;
    pub const CLICKED: i32 = ffi::AWRA_SIGNAL_CLICKED as i32;
    pub const TOGGLED: i32 = ffi::AWRA_SIGNAL_TOGGLED as i32;
    pub const VALUE_CHANGED: i32 = ffi::AWRA_SIGNAL_VALUE_CHANGED as i32;
    pub const SELECTED: i32 = ffi::AWRA_SIGNAL_SELECTED as i32;
    pub const CHANGED: i32 = ffi::AWRA_SIGNAL_CHANGED as i32;
    pub const ACTIVATE: i32 = ffi::AWRA_SIGNAL_ACTIVATE as i32;
    pub const CLOSE_REQUEST: i32 = ffi::AWRA_SIGNAL_CLOSE_REQUEST as i32;
    pub const DESTROY: i32 = ffi::AWRA_SIGNAL_DESTROY as i32;
    pub const PRESSED: i32 = ffi::AWRA_SIGNAL_PRESSED as i32;
    pub const RELEASED: i32 = ffi::AWRA_SIGNAL_RELEASED as i32;
    pub const MOTION: i32 = ffi::AWRA_SIGNAL_MOTION as i32;
    pub const ENTER: i32 = ffi::AWRA_SIGNAL_ENTER as i32;
    pub const LEAVE: i32 = ffi::AWRA_SIGNAL_LEAVE as i32;
    pub const KEY_PRESSED: i32 = ffi::AWRA_SIGNAL_KEY_PRESSED as i32;
    pub const KEY_RELEASED: i32 = ffi::AWRA_SIGNAL_KEY_RELEASED as i32;
    pub const SCROLL: i32 = ffi::AWRA_SIGNAL_SCROLL as i32;
    pub const MAP: i32 = ffi::AWRA_SIGNAL_MAP as i32;
    pub const UNMAP: i32 = ffi::AWRA_SIGNAL_UNMAP as i32;
    pub const RESIZE: i32 = ffi::AWRA_SIGNAL_RESIZE as i32;
    pub const SHOW: i32 = ffi::AWRA_SIGNAL_SHOW as i32;
    pub const HIDE: i32 = ffi::AWRA_SIGNAL_HIDE as i32;
    pub const FILES_DROPPED: i32 = ffi::AWRA_SIGNAL_FILES_DROPPED as i32;
    pub const CAPTURED: i32 = ffi::AWRA_SIGNAL_CAPTURED as i32;
    pub const SEEK: i32 = ffi::AWRA_SIGNAL_SEEK as i32;
    pub const STATE: i32 = ffi::AWRA_SIGNAL_STATE as i32;
    pub const SEARCH_CHANGED: i32 = ffi::AWRA_SIGNAL_SEARCH_CHANGED as i32;
    pub const STOP_SEARCH: i32 = ffi::AWRA_SIGNAL_STOP_SEARCH as i32;
    pub const NOTIFY: i32 = ffi::AWRA_SIGNAL_NOTIFY as i32;
    pub const MOVED: i32 = ffi::AWRA_SIGNAL_MOVED as i32;
    pub const PLAY_REQUESTED: i32 = ffi::AWRA_SIGNAL_PLAY_REQUESTED as i32;
    pub const STOP_REQUESTED: i32 = ffi::AWRA_SIGNAL_STOP_REQUESTED as i32;
    pub const BROWSE_REQUESTED: i32 = ffi::AWRA_SIGNAL_BROWSE_REQUESTED as i32;
    pub const MARKER_MOVED: i32 = ffi::AWRA_SIGNAL_MARKER_MOVED as i32;
    pub const MARKER_COMMITTED: i32 = ffi::AWRA_SIGNAL_MARKER_COMMITTED as i32;
    pub const DROP_HOVER: i32 = ffi::AWRA_SIGNAL_DROP_HOVER as i32;
    pub const KEY_CAPTURE: i32 = ffi::AWRA_SIGNAL_KEY_CAPTURE as i32;
    pub const PRESS_CAPTURE: i32 = ffi::AWRA_SIGNAL_PRESS_CAPTURE as i32;
    pub const ACTIVATE_LINK: i32 = ffi::AWRA_SIGNAL_ACTIVATE_LINK as i32;
    pub const EDGE_REACHED: i32 = ffi::AWRA_SIGNAL_EDGE_REACHED as i32;
    pub const RETRY: i32 = ffi::AWRA_SIGNAL_RETRY as i32;
    pub const JUMP_ACTIVATED: i32 = ffi::AWRA_SIGNAL_JUMP_ACTIVATED as i32;
    pub const COPY_CLIPBOARD: i32 = ffi::AWRA_SIGNAL_COPY_CLIPBOARD as i32;
}

/// What a signal carries.
pub struct Args<'a>(pub(crate) &'a ffi::AwraArgs);

impl Args<'_> {
    pub fn signal(&self) -> i32 {
        self.0.signal as i32
    }
    pub fn active(&self) -> bool {
        self.0.active
    }
    pub fn value(&self) -> f64 {
        self.0.value
    }
    pub fn index(&self) -> i32 {
        self.0.index
    }
    pub fn text(&self) -> Option<String> {
        unsafe { from_cstr_opt(self.0.text) }
    }
    pub fn x(&self) -> f64 {
        self.0.x
    }
    pub fn y(&self) -> f64 {
        self.0.y
    }
    pub fn dx(&self) -> f64 {
        self.0.dx
    }
    pub fn dy(&self) -> f64 {
        self.0.dy
    }
    pub fn button(&self) -> u32 {
        self.0.button.max(0) as u32
    }
    pub fn n_press(&self) -> i32 {
        self.0.n_press
    }
    pub fn mods(&self) -> crate::gdk::ModifierType {
        crate::gdk::ModifierType::from_awra(self.0.mods)
    }
    pub fn keysym(&self) -> Option<String> {
        unsafe { from_cstr_opt(self.0.keysym) }
    }
    /// Physical key of key signals ("KeyA").
    pub fn code(&self) -> Option<String> {
        unsafe { from_cstr_opt(self.0.code) }
    }
    pub fn width(&self) -> i32 {
        self.0.width
    }
    pub fn height(&self) -> i32 {
        self.0.height
    }
    /// Stops default handling / further propagation.
    pub fn stop(&self) {
        if !self.0.handled.is_null() {
            unsafe { *self.0.handled = true }
        }
    }
}

/// A connected handler, for `disconnect` / `block_signal`.
#[derive(Debug, PartialEq, Eq, Hash)]
pub struct SignalHandlerId(pub(crate) std::ffi::c_ulong);

type Callback = Box<dyn Fn(&Widget, &Args)>;

unsafe extern "C" fn trampoline(w: *mut ffi::AwraWidget, args: *const ffi::AwraArgs, data: *mut c_void) {
    let f = &*(data as *const Callback);
    // borrowed: no reference taken for the call
    let widget = std::mem::ManuallyDrop::new(Widget::from_full(w));
    f(&widget, &Args(&*args));
}

unsafe extern "C" fn free_callback(data: *mut c_void) {
    drop(Box::from_raw(data as *mut Callback));
}

impl Widget {
    /// Connects a raw signal handler.
    pub fn connect_raw<F: Fn(&Widget, &Args) + 'static>(&self, signal: i32, f: F) -> SignalHandlerId {
        let data: Box<Callback> = Box::new(Box::new(f));
        let id = unsafe {
            ffi::awra_signal_connect(
                self.as_ptr(),
                signal as _,
                Some(trampoline),
                Box::into_raw(data) as *mut c_void,
                Some(free_callback),
            )
        };
        SignalHandlerId(id)
    }

    /// Connects a handler that receives the widget as `T`.
    pub fn connect_typed<T: StaticType, F: Fn(&T, &Args) + 'static>(&self, signal: i32, f: F) -> SignalHandlerId {
        self.connect_raw(signal, move |w, a| {
            let t = unsafe { T::unchecked_from(w.clone()) };
            f(&t, a)
        })
    }

    pub fn disconnect(&self, id: SignalHandlerId) {
        unsafe { ffi::awra_signal_disconnect(self.as_ptr(), id.0) }
    }

    pub fn block_signal(&self, id: &SignalHandlerId) {
        unsafe { ffi::awra_signal_block(self.as_ptr(), id.0, true) }
    }

    pub fn unblock_signal(&self, id: &SignalHandlerId) {
        unsafe { ffi::awra_signal_block(self.as_ptr(), id.0, false) }
    }

    /// Emits a signal; true when a handler stopped it.
    pub fn emit_raw(&self, args: &ffi::AwraArgs) -> bool {
        unsafe { ffi::awra_signal_emit(self.as_ptr(), args) }
    }
}

/// An empty AwraArgs for `signal`.
pub fn raw_args(signal: i32) -> ffi::AwraArgs {
    let mut a: ffi::AwraArgs = unsafe { std::mem::zeroed() };
    a.signal = signal as _;
    a
}
