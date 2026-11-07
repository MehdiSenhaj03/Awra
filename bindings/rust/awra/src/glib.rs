//! The main loop: timers, idle callbacks, futures on the UI thread (the
//! subset of GLib the applications use).

use std::cell::RefCell;
use std::collections::HashMap;
use std::ffi::c_void;
use std::future::Future;
use std::path::PathBuf;
use std::pin::Pin;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::task::{Context, Poll, Wake, Waker};
use std::time::Duration;

use crate::ffi;

pub use crate::gio::VariantTy;
pub use crate::signal::SignalHandlerId;

/// Keep a repeating source, or stop it.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ControlFlow {
    Continue,
    Break,
}

impl From<bool> for ControlFlow {
    fn from(b: bool) -> ControlFlow {
        if b {
            ControlFlow::Continue
        } else {
            ControlFlow::Break
        }
    }
}

/// Whether an event goes on to other handlers.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Propagation {
    Proceed,
    Stop,
}

impl Propagation {
    pub fn is_stop(self) -> bool {
        self == Propagation::Stop
    }
}

/// A timer or idle source; `remove` cancels it.
#[derive(Debug, PartialEq, Eq, Hash)]
pub struct SourceId(u32);

impl SourceId {
    pub fn remove(self) {
        unsafe { ffi::awra_source_remove(self.0) }
    }
    pub fn as_raw(&self) -> u32 {
        self.0
    }
}

type RepeatFn = Box<dyn FnMut() -> ControlFlow>;
type OnceFn = Option<Box<dyn FnOnce()>>;

unsafe extern "C" fn repeat_tramp(data: *mut c_void) -> bool {
    let f = &mut *(data as *mut RepeatFn);
    f() == ControlFlow::Continue
}
unsafe extern "C" fn repeat_free(data: *mut c_void) {
    drop(Box::from_raw(data as *mut RepeatFn));
}
unsafe extern "C" fn once_tramp(data: *mut c_void) -> bool {
    let f = &mut *(data as *mut OnceFn);
    if let Some(f) = f.take() {
        f();
    }
    false
}
unsafe extern "C" fn once_free(data: *mut c_void) {
    drop(Box::from_raw(data as *mut OnceFn));
}

fn ms(d: Duration) -> u32 {
    d.as_millis().min(u32::MAX as u128) as u32
}

pub fn timeout_add_local<F: FnMut() -> ControlFlow + 'static>(interval: Duration, f: F) -> SourceId {
    let data: Box<RepeatFn> = Box::new(Box::new(f));
    SourceId(unsafe {
        ffi::awra_timeout_add(ms(interval), Some(repeat_tramp), Box::into_raw(data) as *mut c_void, Some(repeat_free))
    })
}

pub fn timeout_add_local_once<F: FnOnce() + 'static>(delay: Duration, f: F) -> SourceId {
    let data: Box<OnceFn> = Box::new(Some(Box::new(f)));
    SourceId(unsafe {
        ffi::awra_timeout_add(ms(delay), Some(once_tramp), Box::into_raw(data) as *mut c_void, Some(once_free))
    })
}

pub fn timeout_add_seconds_local<F: FnMut() -> ControlFlow + 'static>(seconds: u32, f: F) -> SourceId {
    timeout_add_local(Duration::from_secs(seconds as u64), f)
}

pub fn idle_add_local<F: FnMut() -> ControlFlow + 'static>(f: F) -> SourceId {
    let data: Box<RepeatFn> = Box::new(Box::new(f));
    SourceId(unsafe { ffi::awra_idle_add(Some(repeat_tramp), Box::into_raw(data) as *mut c_void, Some(repeat_free)) })
}

pub fn idle_add_local_once<F: FnOnce() + 'static>(f: F) -> SourceId {
    let data: Box<OnceFn> = Box::new(Some(Box::new(f)));
    SourceId(unsafe { ffi::awra_idle_add(Some(once_tramp), Box::into_raw(data) as *mut c_void, Some(once_free)) })
}

/// Runs `f` on the UI thread soon; callable from any thread.
pub fn invoke<F: FnOnce() + Send + 'static>(f: F) {
    type SendFn = Box<dyn FnOnce() + Send>;
    unsafe extern "C" fn tramp(data: *mut c_void) {
        let f = Box::from_raw(data as *mut SendFn);
        f();
    }
    let data: Box<SendFn> = Box::new(Box::new(f));
    unsafe { ffi::awra_invoke(Some(tramp), Box::into_raw(data) as *mut c_void) }
}

// ---------------------------------------------------------------------------
// Futures on the UI thread

type Task = Pin<Box<dyn Future<Output = ()>>>;

thread_local! {
    static TASKS: RefCell<HashMap<u64, Task>> = RefCell::new(HashMap::new());
    static NEXT_TASK: std::cell::Cell<u64> = const { std::cell::Cell::new(1) };
}

struct TaskWaker {
    id: u64,
    queued: AtomicBool,
}

impl Wake for TaskWaker {
    fn wake(self: Arc<Self>) {
        self.wake_by_ref()
    }
    fn wake_by_ref(self: &Arc<Self>) {
        if !self.queued.swap(true, Ordering::AcqRel) {
            let raw = Arc::into_raw(self.clone()) as *mut c_void;
            unsafe { ffi::awra_invoke(Some(poll_tramp), raw) }
        }
    }
}

unsafe extern "C" fn poll_tramp(data: *mut c_void) {
    let waker = Arc::from_raw(data as *const TaskWaker);
    waker.queued.store(false, Ordering::Release);
    poll_task(waker);
}

fn poll_task(waker: Arc<TaskWaker>) {
    let id = waker.id;
    // out of the table while it runs: it may spawn others
    let Some(mut task) = TASKS.with(|t| t.borrow_mut().remove(&id)) else {
        return;
    };
    let w = Waker::from(waker);
    let mut cx = Context::from_waker(&w);
    if task.as_mut().poll(&mut cx).is_pending() {
        TASKS.with(|t| t.borrow_mut().insert(id, task));
    }
}

/// The handle of a spawned future (detached: it runs to completion).
pub struct JoinHandle<T = ()>(u64, std::marker::PhantomData<T>);

impl<T> JoinHandle<T> {
    /// Drops the future: it runs no further.
    pub fn abort(&self) {
        TASKS.with(|t| t.borrow_mut().remove(&self.0));
    }
}

/// Runs a future on the UI thread (GLib's spawn_future_local).
pub fn spawn_future_local<F: Future<Output = ()> + 'static>(f: F) -> JoinHandle {
    let id = NEXT_TASK.with(|n| {
        let id = n.get();
        n.set(id + 1);
        id
    });
    TASKS.with(|t| t.borrow_mut().insert(id, Box::pin(f)));
    // first poll from the loop, as GLib does
    let waker = Arc::new(TaskWaker { id, queued: AtomicBool::new(false) });
    waker.wake_by_ref();
    JoinHandle(id, std::marker::PhantomData)
}

/// Waits `d` on the UI loop (for async code).
pub fn timeout_future(d: Duration) -> impl Future<Output = ()> {
    let state = Arc::new(std::sync::Mutex::new((false, None::<Waker>)));
    let s2 = state.clone();
    timeout_add_local_once(d, move || {
        let mut g = s2.lock().unwrap();
        g.0 = true;
        if let Some(w) = g.1.take() {
            w.wake();
        }
    });
    std::future::poll_fn(move |cx| {
        let mut g = state.lock().unwrap();
        if g.0 {
            Poll::Ready(())
        } else {
            g.1 = Some(cx.waker().clone());
            Poll::Pending
        }
    })
}

/// Runs `f` on a worker thread; the future resolves on the UI thread.
pub fn spawn_blocking<T: Send + 'static, F: FnOnce() -> T + Send + 'static>(
    f: F,
) -> impl Future<Output = Result<T, Box<dyn std::any::Any + Send>>> {
    let state = Arc::new(std::sync::Mutex::new((None::<Result<T, Box<dyn std::any::Any + Send>>>, None::<Waker>)));
    let s2 = state.clone();
    std::thread::spawn(move || {
        let r = std::panic::catch_unwind(std::panic::AssertUnwindSafe(f));
        let mut g = s2.lock().unwrap();
        g.0 = Some(r);
        if let Some(w) = g.1.take() {
            w.wake();
        }
    });
    std::future::poll_fn(move |cx| {
        let mut g = state.lock().unwrap();
        if let Some(r) = g.0.take() {
            Poll::Ready(r)
        } else {
            g.1 = Some(cx.waker().clone());
            Poll::Pending
        }
    })
}

// ---------------------------------------------------------------------------
// Paths

/// Where applications keep their settings (GLib's g_get_user_config_dir:
/// XDG_CONFIG_HOME or ~/.config; the local AppData on Windows).
pub fn user_config_dir() -> PathBuf {
    #[cfg(windows)]
    {
        if let Some(p) = std::env::var_os("LOCALAPPDATA") {
            return PathBuf::from(p);
        }
    }
    #[cfg(not(windows))]
    {
        if let Some(p) = std::env::var_os("XDG_CONFIG_HOME").filter(|p| !p.is_empty()) {
            return PathBuf::from(p);
        }
    }
    home_dir().join(".config")
}

pub fn user_data_dir() -> PathBuf {
    #[cfg(windows)]
    {
        if let Some(p) = std::env::var_os("LOCALAPPDATA") {
            return PathBuf::from(p);
        }
    }
    #[cfg(not(windows))]
    {
        if let Some(p) = std::env::var_os("XDG_DATA_HOME").filter(|p| !p.is_empty()) {
            return PathBuf::from(p);
        }
    }
    home_dir().join(".local/share")
}

pub fn home_dir() -> PathBuf {
    #[cfg(windows)]
    let var = "USERPROFILE";
    #[cfg(not(windows))]
    let var = "HOME";
    std::env::var_os(var).map(PathBuf::from).unwrap_or_else(|| PathBuf::from("."))
}

/// A program on PATH (GLib's g_find_program_in_path).
pub fn find_program_in_path(program: impl AsRef<std::path::Path>) -> Option<PathBuf> {
    let program = program.as_ref();
    if program.components().count() > 1 {
        return program.is_file().then(|| program.to_path_buf());
    }
    let path = std::env::var_os("PATH")?;
    #[cfg(windows)]
    let exts: Vec<String> = std::env::var("PATHEXT")
        .unwrap_or_else(|_| ".EXE;.CMD;.BAT;.COM".into())
        .split(';')
        .map(|e| e.to_string())
        .collect();
    for dir in std::env::split_paths(&path) {
        let candidate = dir.join(program);
        #[cfg(not(windows))]
        {
            use std::os::unix::fs::PermissionsExt;
            if candidate.metadata().is_ok_and(|m| m.is_file() && m.permissions().mode() & 0o111 != 0) {
                return Some(candidate);
            }
        }
        #[cfg(windows)]
        {
            if candidate.is_file() {
                return Some(candidate);
            }
            for e in &exts {
                let mut c = candidate.clone().into_os_string();
                c.push(e);
                let c = PathBuf::from(c);
                if c.is_file() {
                    return Some(c);
                }
            }
        }
    }
    None
}

pub use crate::compat::{BoxedAnyObject, Object};
pub use crate::object::WeakRef;

/// Text as Pango markup shows it literally.
pub fn markup_escape_text(text: &str) -> String {
    let mut out = String::with_capacity(text.len());
    for c in text.chars() {
        match c {
            '&' => out.push_str("&amp;"),
            '<' => out.push_str("&lt;"),
            '>' => out.push_str("&gt;"),
            '"' => out.push_str("&quot;"),
            '\'' => out.push_str("&#39;"),
            c => out.push(c),
        }
    }
    out
}

pub fn user_cache_dir() -> PathBuf {
    if let Some(d) = std::env::var_os("XDG_CACHE_HOME").filter(|d| !d.is_empty()) {
        return PathBuf::from(d);
    }
    #[cfg(windows)]
    if let Some(d) = std::env::var_os("LOCALAPPDATA") {
        return PathBuf::from(d);
    }
    home_dir().join(".cache")
}

/// Bytes shared without copying: made from a vector, they keep it (a
/// texture over them reads it in place, `Texture::for_data`).
#[derive(Clone)]
pub struct Bytes(Arc<dyn AsRef<[u8]> + Send + Sync>);
impl Bytes {
    pub fn from_owned(v: impl Into<Vec<u8>>) -> Bytes {
        Bytes(Arc::new(v.into()))
    }
    pub fn from_static(v: &'static [u8]) -> Bytes {
        Bytes(Arc::new(v))
    }
}
impl From<&[u8]> for Bytes {
    fn from(v: &[u8]) -> Bytes {
        Bytes(Arc::new(v.to_vec()))
    }
}
impl From<Vec<u8>> for Bytes {
    fn from(v: Vec<u8>) -> Bytes {
        Bytes(Arc::new(v))
    }
}
impl std::ops::Deref for Bytes {
    type Target = [u8];
    fn deref(&self) -> &[u8] {
        (*self.0).as_ref()
    }
}
impl AsRef<[u8]> for Bytes {
    fn as_ref(&self) -> &[u8] {
        self
    }
}
impl PartialEq for Bytes {
    fn eq(&self, other: &Bytes) -> bool {
        **self == **other
    }
}
impl Eq for Bytes {}
impl std::fmt::Debug for Bytes {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Bytes({} bytes)", self.len())
    }
}

/// The UI thread's main context.
#[derive(Debug, Clone, Copy)]
pub struct MainContext;
impl MainContext {
    pub fn default() -> MainContext {
        MainContext
    }
    pub fn spawn_local<F: Future<Output = ()> + 'static>(&self, f: F) -> JoinHandle {
        spawn_future_local(f)
    }
    /// Runs `f` on the UI thread (from any thread).
    pub fn invoke<F: FnOnce() + Send + 'static>(&self, f: F) {
        invoke(f)
    }
}

/// Source priorities (accepted, all sources share one).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Priority(i32);
impl Priority {
    pub const DEFAULT: Priority = Priority(0);
    pub const DEFAULT_IDLE: Priority = Priority(200);
    pub const LOW: Priority = Priority(300);
    pub const HIGH: Priority = Priority(-100);
}
pub use crate::compat2::Type;
pub use crate::compat3::{ToValue, Value};
