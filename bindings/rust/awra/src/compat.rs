//! gtk-rs pieces Awra 1 applications used beyond widgets: list models and
//! factories, list views, revealers, textures and paintables, text buffers,
//! Pango attributes, widget action groups and event controllers.
//!
//! SPDX-License-Identifier: GPL-3.0-or-later

use std::any::Any;
use std::cell::{Cell, Ref, RefCell};
use std::ffi::c_void;
use std::rc::Rc;

use crate::awra_wrapper;
use crate::enums::*;
use crate::ffi;
use crate::glib::Propagation;
use crate::object::{cstring, from_cstr_opt, IntoOptWidget, IsWidget, StaticType, WeakRef, Widget};
use crate::signal::{sig, SignalHandlerId};
use crate::widget::Controller;

macro_rules! ptr {
    ($s:expr) => {
        $s.0.as_ptr()
    };
}

/// GTK_INVALID_LIST_POSITION.
pub const INVALID_LIST_POSITION: u32 = u32::MAX;

// ===========================================================================
// Objects and list models

/// A reference-counted value a list model holds (GObject's role for
/// application rows).
#[derive(Clone)]
pub struct Object(Rc<dyn Any>);

impl Object {
    pub fn new<T: Any>(value: T) -> Object {
        Object(Rc::new(value))
    }
    pub fn downcast_ref<T: Any>(&self) -> Option<&T> {
        self.0.downcast_ref::<T>()
    }
    pub fn is<T: Any>(&self) -> bool {
        self.0.is::<T>()
    }
    pub fn ptr_eq(&self, other: &Object) -> bool {
        Rc::ptr_eq(&self.0, &other.0)
    }
    /// gtk-rs `upcast`: an `Object` is already one.
    pub fn upcast<T: From<Object>>(self) -> T {
        T::from(self)
    }
}

impl std::fmt::Debug for Object {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Object({:p})", Rc::as_ptr(&self.0) as *const ())
    }
}

impl PartialEq for Object {
    fn eq(&self, other: &Object) -> bool {
        self.ptr_eq(other)
    }
}

/// glib::BoxedAnyObject: any value as an object of a list model.
pub struct BoxedAnyObject(RefCell<Box<dyn Any>>);

impl BoxedAnyObject {
    #[allow(clippy::new_ret_no_self)]
    pub fn new<T: Any>(value: T) -> Object {
        Object::new(BoxedAnyObject(RefCell::new(Box::new(value))))
    }
    pub fn try_borrow<T: Any>(&self) -> Result<Ref<'_, T>, ()> {
        let r = self.0.try_borrow().map_err(|_| ())?;
        if !r.is::<T>() {
            return Err(());
        }
        Ok(Ref::map(r, |b| b.downcast_ref::<T>().unwrap()))
    }
    pub fn borrow<T: Any>(&self) -> Ref<'_, T> {
        self.try_borrow().expect("BoxedAnyObject of another type")
    }
}

type ItemsChanged = Rc<dyn Fn(u32, u32, u32)>;

struct StoreInner {
    items: RefCell<Vec<Object>>,
    handlers: RefCell<Vec<(u64, ItemsChanged)>>,
    next: Cell<u64>,
}

/// gio::ListStore: the items a list view shows.
#[derive(Clone)]
pub struct ListStore(Rc<StoreInner>);

impl ListStore {
    /// gtk-rs names the item type; any object goes here.
    pub fn new<T>() -> ListStore {
        ListStore(Rc::new(StoreInner {
            items: RefCell::new(Vec::new()),
            handlers: RefCell::new(Vec::new()),
            next: Cell::new(1),
        }))
    }
    pub fn with_type<T>(_t: T) -> ListStore {
        ListStore::new::<()>()
    }
    pub fn n_items(&self) -> u32 {
        self.0.items.borrow().len() as u32
    }
    pub fn item(&self, position: u32) -> Option<Object> {
        self.0.items.borrow().get(position as usize).cloned()
    }
    fn emit(&self, position: u32, removed: u32, added: u32) {
        let hs: Vec<ItemsChanged> = self.0.handlers.borrow().iter().map(|(_, h)| h.clone()).collect();
        for h in hs {
            h(position, removed, added);
        }
    }
    pub fn splice(&self, position: u32, n_removals: u32, additions: &[Object]) {
        {
            let mut items = self.0.items.borrow_mut();
            let at = (position as usize).min(items.len());
            let end = (at + n_removals as usize).min(items.len());
            items.splice(at..end, additions.iter().cloned());
        }
        self.emit(position, n_removals, additions.len() as u32);
    }
    pub fn append(&self, item: &Object) {
        let n = self.n_items();
        self.splice(n, 0, std::slice::from_ref(item));
    }
    pub fn insert(&self, position: u32, item: &Object) {
        self.splice(position, 0, std::slice::from_ref(item));
    }
    pub fn remove(&self, position: u32) {
        self.splice(position, 1, &[]);
    }
    pub fn remove_all(&self) {
        let n = self.n_items();
        if n > 0 {
            self.splice(0, n, &[]);
        }
    }
    pub fn extend_from_slice(&self, items: &[Object]) {
        let n = self.n_items();
        self.splice(n, 0, items);
    }
    pub fn connect_items_changed<F: Fn(&ListStore, u32, u32, u32) + 'static>(&self, f: F) -> u64 {
        let id = self.0.next.get();
        self.0.next.set(id + 1);
        let weak = Rc::downgrade(&self.0);
        self.0.handlers.borrow_mut().push((
            id,
            Rc::new(move |p, r, a| {
                if let Some(inner) = weak.upgrade() {
                    f(&ListStore(inner), p, r, a)
                }
            }),
        ));
        id
    }
    pub fn disconnect(&self, id: u64) {
        self.0.handlers.borrow_mut().retain(|(i, _)| *i != id);
    }
    pub fn iter(&self) -> Vec<Object> {
        self.0.items.borrow().clone()
    }
}

impl std::fmt::Debug for ListStore {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "ListStore({} items)", self.n_items())
    }
}

/// gtk::StringList: strings as a model.
#[derive(Clone, Debug)]
pub struct StringObject(pub String);
impl StringObject {
    pub fn string(&self) -> String {
        self.0.clone()
    }
}

/// GtkNoSelection / GtkSingleSelection: a model whose rows aren't selected.
#[derive(Clone, Debug)]
pub struct NoSelection(pub ListStore);
impl NoSelection {
    pub fn new(model: Option<ListStore>) -> NoSelection {
        NoSelection(model.unwrap_or_else(ListStore::new::<()>))
    }
    pub fn model(&self) -> ListStore {
        self.0.clone()
    }
}
pub type SingleSelection = NoSelection;

// ===========================================================================
// Factories and list items

awra_wrapper!(
    /// A row of a list view (GtkListItem): its child, its item.
    ListItem, ["AwraListItem"]
);

impl ListItem {
    pub fn set_child(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_list_item_set_child(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn child(&self) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_list_item_child(ptr!(self))) }
    }
    pub fn position(&self) -> u32 {
        unsafe { ffi::awra_list_item_position(ptr!(self)) }
    }
    /// The model's item at this row's position.
    pub fn item(&self) -> Option<Object> {
        // the list holding it (a data view's cells sit in a row first)
        let mut w = self.0.parent();
        while let Some(cur) = w {
            if let Some(model) = cur.rust_data::<ListStore>("list-model") {
                return model.item(self.position());
            }
            w = cur.parent();
        }
        None
    }
    pub fn set_activatable(&self, a: bool) {
        unsafe { ffi::awra_list_item_set_activatable(ptr!(self), a) }
    }
    pub fn set_selectable(&self, s: bool) {
        unsafe { ffi::awra_list_item_set_selectable(ptr!(self), s) }
    }
    pub fn set_focusable(&self, f: bool) {
        unsafe { ffi::awra_list_item_set_focusable(ptr!(self), f) }
    }
}

type ItemFn = Rc<dyn Fn(&SignalListItemFactory, &ListItem)>;

#[derive(Default)]
struct FactoryInner {
    setup: RefCell<Vec<ItemFn>>,
    bind: RefCell<Vec<ItemFn>>,
    unbind: RefCell<Vec<ItemFn>>,
    teardown: RefCell<Vec<ItemFn>>,
}

/// GtkSignalListItemFactory.
#[derive(Clone, Default)]
pub struct SignalListItemFactory(Rc<FactoryInner>);

impl SignalListItemFactory {
    pub fn new() -> SignalListItemFactory {
        SignalListItemFactory::default()
    }
    pub fn connect_setup<F: Fn(&SignalListItemFactory, &ListItem) + 'static>(&self, f: F) {
        self.0.setup.borrow_mut().push(Rc::new(f));
    }
    pub fn connect_bind<F: Fn(&SignalListItemFactory, &ListItem) + 'static>(&self, f: F) {
        self.0.bind.borrow_mut().push(Rc::new(f));
    }
    pub fn connect_unbind<F: Fn(&SignalListItemFactory, &ListItem) + 'static>(&self, f: F) {
        self.0.unbind.borrow_mut().push(Rc::new(f));
    }
    pub fn connect_teardown<F: Fn(&SignalListItemFactory, &ListItem) + 'static>(&self, f: F) {
        self.0.teardown.borrow_mut().push(Rc::new(f));
    }
    fn run(&self, which: u8, item: &ListItem) {
        let list = match which {
            0 => &self.0.setup,
            1 => &self.0.bind,
            2 => &self.0.unbind,
            _ => &self.0.teardown,
        };
        let hs: Vec<ItemFn> = list.borrow().clone();
        for h in hs {
            h(self, item);
        }
    }
}

unsafe extern "C" fn factory_cb<const W: u8>(_list: *mut ffi::AwraWidget, item: *mut ffi::AwraWidget, data: *mut c_void) {
    let f = &*(data as *const SignalListItemFactory);
    let item = std::mem::ManuallyDrop::new(ListItem(Widget::from_full(item)));
    f.run(W, &item);
}
unsafe extern "C" fn factory_free(data: *mut c_void) {
    drop(Box::from_raw(data as *mut SignalListItemFactory));
}

/// Gives a C list view a factory and a model whose changes it follows;
/// `changed` reports a model change to the view (a conversation view's own).
fn attach_list(
    list: &Widget,
    model: Option<ListStore>,
    factory: Option<SignalListItemFactory>,
    changed: unsafe extern "C" fn(*mut ffi::AwraWidget, u32, u32, u32),
    target: &Widget,
) {
    if let Some(factory) = factory {
        let data = Box::into_raw(Box::new(factory)) as *mut c_void;
        unsafe {
            ffi::awra_list_view_set_factory(
                list.as_ptr(),
                Some(factory_cb::<0>),
                Some(factory_cb::<1>),
                Some(factory_cb::<2>),
                Some(factory_cb::<3>),
                data,
                Some(factory_free),
            )
        }
    }
    if let Some(model) = model {
        set_model(list, &model, changed, target);
    }
}

fn set_model(
    list: &Widget,
    model: &ListStore,
    changed: unsafe extern "C" fn(*mut ffi::AwraWidget, u32, u32, u32),
    target: &Widget,
) {
    if let Some(old) = list.rust_data::<ListStore>("list-model") {
        if let Some(id) = list.rust_data::<u64>("list-model-handler") {
            old.disconnect(id);
        }
        let n = old.n_items();
        unsafe { changed(target.as_ptr(), 0, n, 0) };
    }
    list.set_rust_data("list-model", model.clone());
    let weak = target.downgrade();
    let id = model.connect_items_changed(move |_, p, r, a| {
        if let Some(t) = weak.upgrade() {
            unsafe { changed(t.as_ptr(), p, r, a) };
        }
    });
    list.set_rust_data("list-model-handler", id);
    let n = model.n_items();
    if n > 0 {
        unsafe { changed(target.as_ptr(), 0, 0, n) };
    }
}

unsafe extern "C" fn list_changed(w: *mut ffi::AwraWidget, p: u32, r: u32, a: u32) {
    ffi::awra_list_view_items_changed(w, p, r, a)
}

awra_wrapper!(
    /// GtkListView: rows for the items near what shows.
    ListView, ["AwraListView"]
);

impl ListView {
    pub fn new(model: Option<NoSelection>, factory: Option<SignalListItemFactory>) -> ListView {
        let v = unsafe { ListView::from_full(ffi::awra_list_view_new()) };
        attach_list(&v.0, model.map(|m| m.0), factory, list_changed, &v.0);
        v
    }
    pub fn set_model(&self, model: Option<&NoSelection>) {
        if let Some(m) = model {
            set_model(&self.0, &m.0, list_changed, &self.0);
        }
    }
    pub fn set_factory(&self, factory: Option<&SignalListItemFactory>) {
        attach_list(&self.0, None, factory.cloned(), list_changed, &self.0);
    }
    pub fn model(&self) -> Option<ListStore> {
        self.0.rust_data::<ListStore>("list-model")
    }
    pub fn set_single_click_activate(&self, single: bool) {
        unsafe { ffi::awra_list_view_set_single_click_activate(ptr!(self), single) }
    }
    pub fn set_show_separators(&self, _s: bool) {}
    pub fn set_enable_rubberband(&self, _r: bool) {}
    pub fn set_tab_behavior<T>(&self, _t: T) {}
    pub fn connect_activate<F: Fn(&ListView, u32) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<ListView, _>(sig::ACTIVATE, move |w, a| f(w, a.index() as u32))
    }
    pub fn scroll_to(&self, position: u32, _flags: ListScrollFlags, _info: Option<()>) {
        unsafe { ffi::awra_list_view_scroll_to(ptr!(self), position, 0) }
    }
    /// Binds `position` again (gtk-rs: a new item for the same row).
    pub fn rebind(&self, position: u32) {
        unsafe { ffi::awra_list_view_rebind(ptr!(self), position) }
    }
    pub fn bound_items(&self) -> Vec<ListItem> {
        let n = unsafe { ffi::awra_list_view_bound_count(ptr!(self)) };
        (0..n)
            .filter_map(|i| unsafe { Widget::from_none_opt(ffi::awra_list_view_bound_item(ptr!(self), i)) })
            .map(ListItem)
            .collect()
    }
    pub fn visible_range(&self) -> Option<(u32, u32)> {
        let (mut a, mut b) = (0u32, 0u32);
        unsafe { ffi::awra_list_view_visible_range(ptr!(self), &mut a, &mut b) }.then_some((a, b))
    }
}

/// GtkListScrollFlags.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct ListScrollFlags(u32);
impl ListScrollFlags {
    pub const NONE: ListScrollFlags = ListScrollFlags(0);
    pub const FOCUS: ListScrollFlags = ListScrollFlags(1);
    pub const SELECT: ListScrollFlags = ListScrollFlags(2);
}

// ===========================================================================
// Conversation view, completion popover (their lists are list views)

unsafe extern "C" fn conversation_changed(w: *mut ffi::AwraWidget, p: u32, r: u32, a: u32) {
    ffi::awra_conversation_view_items_changed(w, p, r, a)
}

/// Awra 1's AwraConversationAlign.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ConversationAlign {
    Top = 0,
    Center = 1,
    End = 2,
}

/// Awra 1's AwraEdgeStatus.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EdgeStatus {
    None = 0,
    Loading = 1,
    Failed = 2,
}

awra_wrapper!(
    /// A list of messages that follows the newest (Awra 1's AwraConversationView).
    ConversationView, ["AwraConversationView"]
);

fn edge_of(p: PositionType) -> i32 {
    if p == PositionType::Top {
        0
    } else {
        1
    }
}

impl ConversationView {
    pub fn new(model: Option<ListStore>, factory: Option<SignalListItemFactory>) -> ConversationView {
        let v = unsafe { ConversationView::from_full(ffi::awra_conversation_view_new()) };
        let list = unsafe { Widget::from_none(ffi::awra_conversation_view_list(ptr!(v))) };
        attach_list(&list, model, factory, conversation_changed, &v.0);
        v
    }
    /// The list view inside.
    pub fn delegate(&self) -> ListView {
        ListView(unsafe { Widget::from_none(ffi::awra_conversation_view_list(ptr!(self))) })
    }
    pub fn is_at_bottom(&self) -> bool {
        unsafe { ffi::awra_conversation_view_at_bottom(ptr!(self)) }
    }
    pub fn set_follow(&self, follow: bool) {
        unsafe { ffi::awra_conversation_view_set_follow(ptr!(self), follow) }
    }
    pub fn follows(&self) -> bool {
        unsafe { ffi::awra_conversation_view_follow(ptr!(self)) }
    }
    pub fn scroll_to_end(&self, animate: bool) {
        unsafe { ffi::awra_conversation_view_scroll_to_end(ptr!(self), animate) }
    }
    pub fn scroll_to_item(&self, position: u32, align: ConversationAlign, highlight: bool) {
        unsafe { ffi::awra_conversation_view_scroll_to_item(ptr!(self), position, align as i32, highlight) }
    }
    pub fn unread_count(&self) -> u32 {
        unsafe { ffi::awra_conversation_view_unread_count(ptr!(self)) }
    }
    pub fn set_unread_count(&self, n: u32) {
        unsafe { ffi::awra_conversation_view_set_unread_count(ptr!(self), n) }
    }
    pub fn jump_label(&self) -> Option<String> {
        unsafe { from_cstr_opt(ffi::awra_conversation_view_jump_label(ptr!(self))) }
    }
    pub fn set_jump_label(&self, label: Option<&str>) {
        let l = label.map(cstring);
        unsafe {
            ffi::awra_conversation_view_set_jump_label(ptr!(self), l.as_ref().map_or(std::ptr::null(), |c| c.as_ptr()))
        }
    }
    pub fn set_edge_threshold(&self, t: i32) {
        unsafe { ffi::awra_conversation_view_set_edge_threshold(ptr!(self), t) }
    }
    pub fn set_edge_status(&self, edge: PositionType, status: EdgeStatus, label: Option<&str>) {
        let l = label.map(cstring);
        unsafe {
            ffi::awra_conversation_view_set_edge_status(
                ptr!(self),
                edge_of(edge),
                status as i32,
                l.as_ref().map_or(std::ptr::null(), |c| c.as_ptr()),
            )
        }
    }
    pub fn visible_range(&self) -> Option<(u32, u32)> {
        let (mut a, mut b) = (0u32, 0u32);
        unsafe { ffi::awra_conversation_view_visible_range(ptr!(self), &mut a, &mut b) }.then_some((a, b))
    }
    pub fn connect_edge_reached<F: Fn(&ConversationView, PositionType) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<ConversationView, _>(sig::EDGE_REACHED, move |w, a| {
            f(w, if a.index() == 0 { PositionType::Top } else { PositionType::Bottom })
        })
    }
    pub fn connect_retry<F: Fn(&ConversationView, PositionType) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<ConversationView, _>(sig::RETRY, move |w, a| {
            f(w, if a.index() == 0 { PositionType::Top } else { PositionType::Bottom })
        })
    }
    pub fn connect_jump_activated<F: Fn(&ConversationView) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<ConversationView, _>(sig::JUMP_ACTIVATED, move |w, _| f(w))
    }
    /// "at-bottom" changes.
    pub fn connect_at_bottom_notify<F: Fn(&ConversationView) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<ConversationView, _>(sig::NOTIFY, move |w, a| {
            if a.text().as_deref() == Some("at-bottom") {
                f(w)
            }
        })
    }
}

unsafe extern "C" fn completion_changed(w: *mut ffi::AwraWidget, p: u32, r: u32, a: u32) {
    ffi::awra_list_view_items_changed(ffi::awra_completion_popover_list(w), p, r, a)
}

awra_wrapper!(
    /// Suggestions above a text view's caret (Awra 1's AwraCompletionPopover).
    CompletionPopover, ["AwraBox"]
);

impl CompletionPopover {
    pub fn new() -> CompletionPopover {
        unsafe { CompletionPopover::from_full(ffi::awra_completion_popover_new()) }
    }
    fn list(&self) -> Widget {
        unsafe { Widget::from_none(ffi::awra_completion_popover_list(ptr!(self))) }
    }
    pub fn set_model(&self, model: Option<&ListStore>) {
        if let Some(m) = model {
            set_model(&self.list(), m, completion_changed, &self.0);
        }
    }
    pub fn set_factory(&self, factory: Option<&SignalListItemFactory>) {
        attach_list(&self.list(), None, factory.cloned(), completion_changed, &self.0);
    }
    pub fn attach(&self, view: Option<&impl IsWidget>) {
        unsafe { ffi::awra_completion_popover_attach(ptr!(self), view.map_or(std::ptr::null_mut(), |v| v.widget_ptr())) }
    }
    pub fn selected(&self) -> u32 {
        unsafe { ffi::awra_completion_popover_selected(ptr!(self)) }
    }
    pub fn set_selected(&self, p: u32) {
        unsafe { ffi::awra_completion_popover_set_selected(ptr!(self), p) }
    }
    /// Shown (gtk-rs: the popover's visibility).
    pub fn is_visible(&self) -> bool {
        unsafe { ffi::awra_completion_popover_visible(ptr!(self)) }
    }
    pub fn present_at_cursor(&self) {
        unsafe { ffi::awra_completion_popover_present_at_cursor(ptr!(self)) }
    }
    pub fn popdown(&self) {
        unsafe { ffi::awra_completion_popover_popdown(ptr!(self)) }
    }
    pub fn connect_activated<F: Fn(&CompletionPopover, u32) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<CompletionPopover, _>(sig::ACTIVATE, move |w, a| f(w, a.index() as u32))
    }
    pub fn connect_dismissed<F: Fn(&CompletionPopover) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<CompletionPopover, _>(sig::STOP_SEARCH, move |w, _| f(w))
    }
}

impl Default for CompletionPopover {
    fn default() -> Self {
        Self::new()
    }
}

// ===========================================================================
// Revealer, aspect frame, window handle, layouts

/// GtkRevealerTransitionType.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum RevealerTransitionType {
    None = 0,
    Crossfade = 1,
    SlideRight = 2,
    SlideLeft = 3,
    SlideUp = 4,
    #[default]
    SlideDown = 5,
    SwingRight = 6,
    SwingLeft = 7,
    SwingUp = 8,
    SwingDown = 9,
}

awra_wrapper!(
    /// GtkRevealer.
    Revealer, ["AwraRevealer"]
);

impl Revealer {
    pub fn new() -> Revealer {
        unsafe { Revealer::from_full(ffi::awra_revealer_new()) }
    }
    pub fn set_child(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_revealer_set_child(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn child(&self) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_revealer_child(ptr!(self))) }
    }
    fn transition(&self) -> (RevealerTransitionType, u32) {
        self.0.rust_data("reveal-transition").unwrap_or((RevealerTransitionType::SlideDown, 250))
    }
    pub fn set_transition_type(&self, t: RevealerTransitionType) {
        let (_, ms) = self.transition();
        self.0.set_rust_data("reveal-transition", (t, ms));
        unsafe { ffi::awra_revealer_set_transition(ptr!(self), t as i32, ms) }
    }
    pub fn set_transition_duration(&self, ms: u32) {
        let (t, _) = self.transition();
        self.0.set_rust_data("reveal-transition", (t, ms));
        unsafe { ffi::awra_revealer_set_transition(ptr!(self), t as i32, ms) }
    }
    pub fn set_reveal_child(&self, reveal: bool) {
        unsafe { ffi::awra_revealer_set_reveal_child(ptr!(self), reveal) }
    }
    pub fn reveals_child(&self) -> bool {
        unsafe { ffi::awra_revealer_reveal_child(ptr!(self)) }
    }
    pub fn is_child_revealed(&self) -> bool {
        unsafe { ffi::awra_revealer_child_revealed(ptr!(self)) }
    }
    pub fn connect_child_revealed_notify<F: Fn(&Revealer) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Revealer, _>(sig::NOTIFY, move |w, a| {
            if a.text().as_deref() == Some("child-revealed") {
                f(w)
            }
        })
    }
}

impl Default for Revealer {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(AspectFrame, ["AwraAspectFrame"]);
impl AspectFrame {
    pub fn new(xalign: f32, yalign: f32, ratio: f32, _obey_child: bool) -> AspectFrame {
        unsafe { AspectFrame::from_full(ffi::awra_aspect_frame_new(xalign, yalign, ratio)) }
    }
    pub fn set_child(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_aspect_frame_set_child(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn set_ratio(&self, ratio: f32) {
        unsafe { ffi::awra_aspect_frame_set_ratio(ptr!(self), ratio) }
    }
}

awra_wrapper!(WindowHandle, ["AwraWindowHandle"]);
impl WindowHandle {
    pub fn new() -> WindowHandle {
        unsafe { WindowHandle::from_full(ffi::awra_window_handle_new()) }
    }
    pub fn set_child(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_window_handle_set_child(ptr!(self), child.into_opt_ptr()) }
    }
}
impl Default for WindowHandle {
    fn default() -> Self {
        Self::new()
    }
}

type MeasureFn = Box<dyn Fn(&Layout, Orientation, i32) -> (i32, i32)>;
type AllocateFn = Box<dyn Fn(&Layout, i32, i32)>;
struct LayoutFns {
    measure: Option<MeasureFn>,
    allocate: Option<AllocateFn>,
}

awra_wrapper!(
    /// A widget that lays its children out itself (a GtkWidget subclass's
    /// measure and size_allocate), in logical pixels.
    Layout, ["AwraLayout"]
);

unsafe extern "C" fn layout_measure(
    w: *mut ffi::AwraWidget,
    o: i32,
    for_size: i32,
    mn: *mut i32,
    nt: *mut i32,
    data: *mut c_void,
) {
    let fns = &*(data as *const LayoutFns);
    let l = std::mem::ManuallyDrop::new(Layout(Widget::from_full(w)));
    let o = if o == 0 { Orientation::Horizontal } else { Orientation::Vertical };
    let (a, b) = fns.measure.as_ref().map_or((0, 0), |m| m(&l, o, for_size));
    *mn = a;
    *nt = b;
}
unsafe extern "C" fn layout_allocate(w: *mut ffi::AwraWidget, width: i32, height: i32, data: *mut c_void) {
    let fns = &*(data as *const LayoutFns);
    let l = std::mem::ManuallyDrop::new(Layout(Widget::from_full(w)));
    if let Some(a) = &fns.allocate {
        a(&l, width, height)
    }
}
unsafe extern "C" fn layout_free(data: *mut c_void) {
    drop(Box::from_raw(data as *mut LayoutFns));
}

impl Layout {
    /// Children laid out on top of each other (GtkBinLayout).
    pub fn bin() -> Layout {
        unsafe { Layout::from_full(ffi::awra_layout_new(None, None, std::ptr::null_mut(), None)) }
    }
    pub fn new<M, A>(measure: M, allocate: A) -> Layout
    where
        M: Fn(&Layout, Orientation, i32) -> (i32, i32) + 'static,
        A: Fn(&Layout, i32, i32) + 'static,
    {
        let fns = Box::new(LayoutFns { measure: Some(Box::new(measure)), allocate: Some(Box::new(allocate)) });
        unsafe {
            Layout::from_full(ffi::awra_layout_new(
                Some(layout_measure),
                Some(layout_allocate),
                Box::into_raw(fns) as *mut c_void,
                Some(layout_free),
            ))
        }
    }
    pub fn append(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_container_add(ptr!(self), child.widget_ptr()) }
    }
    pub fn remove(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_container_remove(ptr!(self), child.widget_ptr()) }
    }
    pub fn set_clip(&self, clip: bool) {
        unsafe { ffi::awra_layout_set_clip(ptr!(self), clip) }
    }
}

impl Widget {
    /// Places a child of a [`Layout`] (logical px, relative to it).
    pub fn allocate(&self, x: i32, y: i32, width: i32, height: i32) {
        unsafe { ffi::awra_widget_allocate(self.as_ptr(), x, y, width, height) }
    }
    /// gtk_widget_set_child_visible.
    pub fn set_child_visible(&self, visible: bool) {
        unsafe { ffi::awra_widget_set_child_visible(self.as_ptr(), visible) }
    }
    pub fn is_child_visible(&self) -> bool {
        unsafe { ffi::awra_widget_child_visible(self.as_ptr()) }
    }
    /// The widget's box in `target`'s coordinates (gtk_widget_compute_bounds).
    pub fn compute_bounds(&self, target: &impl IsWidget) -> Option<crate::Rectangle> {
        let (x, y) = self.window_position()?;
        let (tx, ty) = target.upcast_ref().window_position()?;
        Some(crate::Rectangle::new((x - tx) as i32, (y - ty) as i32, self.width(), self.height()))
    }
}

/// GtkOverflow.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Overflow {
    Visible,
    Hidden,
}

// ===========================================================================
// Textures and paintables

/// GdkTexture: immutable pixels, shared.
pub struct Texture(std::ptr::NonNull<ffi::AwraTexture>);

impl Texture {
    /// # Safety
    /// `p` is a texture reference the caller owns.
    pub unsafe fn from_raw(p: *mut ffi::AwraTexture) -> Option<Texture> {
        std::ptr::NonNull::new(p).map(Texture)
    }
    pub fn as_ptr(&self) -> *mut ffi::AwraTexture {
        self.0.as_ptr()
    }
    /// Straight RGBA rows (copied).
    pub fn from_rgba(width: i32, height: i32, rgba: &[u8], stride: usize) -> Option<Texture> {
        if width <= 0 || height <= 0 || rgba.len() < stride * (height as usize - 1) + width as usize * 4 {
            return None;
        }
        unsafe { Texture::from_raw(ffi::awra_texture_new_rgba(rgba.as_ptr(), width, height, stride as i32)) }
    }
    /// A video frame in NV12.
    pub fn from_nv12(width: i32, height: i32, data: &[u8], bt709: bool) -> Option<Texture> {
        if data.len() < (width as usize * height as usize * 3) / 2 {
            return None;
        }
        unsafe { Texture::from_raw(ffi::awra_texture_new_nv12(data.as_ptr(), width, height, bt709)) }
    }
    /// A PNG, JPEG, GIF, WebP or SVG.
    pub fn from_bytes(bytes: &[u8]) -> Result<Texture, ()> {
        unsafe { Texture::from_raw(ffi::awra_texture_new_from_bytes(bytes.as_ptr() as *const c_void, bytes.len())) }
            .ok_or(())
    }
    pub fn from_filename(path: impl AsRef<std::path::Path>) -> Result<Texture, ()> {
        let p = cstring(&path.as_ref().to_string_lossy());
        unsafe { Texture::from_raw(ffi::awra_texture_new_from_file(p.as_ptr())) }.ok_or(())
    }
    pub fn width(&self) -> i32 {
        unsafe { ffi::awra_texture_width(self.as_ptr()) }
    }
    pub fn height(&self) -> i32 {
        unsafe { ffi::awra_texture_height(self.as_ptr()) }
    }
    /// Straight RGBA rows, `stride` bytes apart.
    pub fn download(&self, rgba: &mut [u8], stride: usize) -> bool {
        if rgba.len() < stride * self.height() as usize {
            return false;
        }
        unsafe { ffi::awra_texture_download(self.as_ptr(), rgba.as_mut_ptr(), stride as i32) }
    }
    pub fn ptr_eq(&self, other: &Texture) -> bool {
        self.0 == other.0
    }
}

impl Clone for Texture {
    fn clone(&self) -> Texture {
        unsafe { ffi::awra_texture_ref(self.as_ptr()) };
        Texture(self.0)
    }
}
impl Drop for Texture {
    fn drop(&mut self) {
        unsafe { ffi::awra_texture_unref(self.as_ptr()) }
    }
}
impl PartialEq for Texture {
    fn eq(&self, other: &Texture) -> bool {
        self.ptr_eq(other)
    }
}
impl std::fmt::Debug for Texture {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Texture({}x{})", self.width(), self.height())
    }
}

type PaintableSetter = Rc<dyn Fn(&Widget, Option<&Texture>)>;

struct PaintableInner {
    texture: RefCell<Option<Texture>>,
    viewers: RefCell<Vec<(WeakRef<Widget>, PaintableSetter)>>,
    width: Cell<i32>,
    height: Cell<i32>,
}

/// GdkPaintable: what an image, an avatar or a chip shows; its texture may
/// change (animations), and every widget showing it follows.
#[derive(Clone)]
pub struct Paintable(Rc<PaintableInner>);

impl Paintable {
    pub fn new(texture: Option<Texture>) -> Paintable {
        let (w, h) = texture.as_ref().map_or((0, 0), |t| (t.width(), t.height()));
        Paintable(Rc::new(PaintableInner {
            texture: RefCell::new(texture),
            viewers: RefCell::new(Vec::new()),
            width: Cell::new(w),
            height: Cell::new(h),
        }))
    }
    pub fn texture(&self) -> Option<Texture> {
        self.0.texture.borrow().clone()
    }
    pub fn intrinsic_width(&self) -> i32 {
        self.0.width.get()
    }
    pub fn intrinsic_height(&self) -> i32 {
        self.0.height.get()
    }
    pub fn ptr_eq(&self, other: &Paintable) -> bool {
        Rc::ptr_eq(&self.0, &other.0)
    }
    /// Shows `texture` from now on wherever this is shown
    /// (gdk_paintable_invalidate_contents).
    pub fn set_texture(&self, texture: Option<Texture>) {
        *self.0.texture.borrow_mut() = texture.clone();
        let viewers: Vec<(Widget, PaintableSetter)> = {
            let mut v = self.0.viewers.borrow_mut();
            v.retain(|(w, _)| w.upgrade().is_some_and(|w| showing(&w).is_some_and(|p| p.ptr_eq(self))));
            v.iter().filter_map(|(w, s)| w.upgrade().map(|w| (w, s.clone()))).collect()
        };
        for (w, s) in viewers {
            s(&w, texture.as_ref());
        }
    }
    /// Whether a widget showing it is on screen.
    pub fn is_shown(&self) -> bool {
        self.0.viewers.borrow().iter().any(|(w, _)| w.upgrade().is_some_and(|w| w.is_mapped()))
    }
    /// `widget` shows this paintable through `setter` (until it shows another).
    pub fn show_on(&self, widget: &Widget, setter: impl Fn(&Widget, Option<&Texture>) + 'static) {
        widget.set_rust_data("paintable", Some(self.clone()));
        let setter: PaintableSetter = Rc::new(setter);
        setter(widget, self.0.texture.borrow().as_ref());
        let mut v = self.0.viewers.borrow_mut();
        v.retain(|(w, _)| w.upgrade().is_some_and(|w| &w != widget));
        v.push((widget.downgrade(), setter));
    }
    /// gtk-rs `upcast`.
    pub fn upcast<T: From<Paintable>>(self) -> T {
        T::from(self)
    }
}

fn showing(w: &Widget) -> Option<Paintable> {
    w.rust_data::<Option<Paintable>>("paintable").flatten()
}

/// The paintable a widget shows (images, avatars, chips).
pub fn widget_paintable(w: &Widget) -> Option<Paintable> {
    showing(w)
}

/// Shows `paintable` on `w` with `setter`, or nothing.
pub fn show_paintable(w: &Widget, paintable: Option<&Paintable>, setter: impl Fn(&Widget, Option<&Texture>) + 'static) {
    match paintable {
        Some(p) => p.show_on(w, setter),
        None => {
            w.set_rust_data("paintable", None::<Paintable>);
            setter(w, None);
        }
    }
}

impl From<Texture> for Paintable {
    fn from(t: Texture) -> Paintable {
        Paintable::new(Some(t))
    }
}
impl PartialEq for Paintable {
    fn eq(&self, other: &Paintable) -> bool {
        self.ptr_eq(other)
    }
}
impl std::fmt::Debug for Paintable {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Paintable({:p})", Rc::as_ptr(&self.0))
    }
}

fn image_set_texture(w: &Widget, t: Option<&Texture>) {
    unsafe { ffi::awra_image_set_texture(w.as_ptr(), t.map_or(std::ptr::null_mut(), |t| t.as_ptr())) }
}

impl crate::Image {
    /// GtkPicture's paintable.
    pub fn set_paintable(&self, paintable: Option<&Paintable>) {
        show_paintable(self, paintable, image_set_texture)
    }
    pub fn paintable(&self) -> Option<Paintable> {
        showing(self)
    }
    pub fn for_paintable(paintable: &Paintable) -> crate::Image {
        let i = crate::Image::new();
        i.set_paintable(Some(paintable));
        i
    }
    pub fn set_texture(&self, texture: Option<&Texture>) {
        self.set_rust_data("paintable", None::<Paintable>);
        image_set_texture(self, texture)
    }
    pub fn set_can_shrink(&self, shrink: bool) {
        unsafe { ffi::awra_image_set_can_shrink(self.as_ptr(), shrink) }
    }
    pub fn set_keep_aspect_ratio(&self, _keep: bool) {}
    pub fn set_alternative_text(&self, _t: Option<&str>) {}
}

/// A Rust value for the texture of a list of arbitrary images (GdkMemoryFormat).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MemoryFormat {
    R8g8b8a8,
    R8g8b8a8Premultiplied,
    B8g8r8a8,
    B8g8r8a8Premultiplied,
    R8g8b8,
}

// ===========================================================================
// Pango attributes (whole-text ones, as labels take them)

/// PangoWeight.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Weight {
    Thin = 100,
    Light = 300,
    Normal = 400,
    Medium = 500,
    Semibold = 600,
    Bold = 700,
    Ultrabold = 800,
    Heavy = 900,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Attribute {
    Weight(i32),
    Scale(f64),
    Foreground(u16, u16, u16),
}

#[derive(Debug, Clone, Default, PartialEq)]
pub struct AttrList(RefCell<Vec<Attribute>>);
impl AttrList {
    pub fn new() -> AttrList {
        AttrList::default()
    }
    pub fn insert(&self, a: impl Into<Attribute>) {
        self.0.borrow_mut().push(a.into());
    }
    pub fn attributes(&self) -> Vec<Attribute> {
        self.0.borrow().clone()
    }
}
pub struct AttrInt;
impl AttrInt {
    pub fn new_weight(w: Weight) -> Attribute {
        Attribute::Weight(w as i32)
    }
}
pub struct AttrFloat;
impl AttrFloat {
    pub fn new_scale(s: f64) -> Attribute {
        Attribute::Scale(s)
    }
}
pub struct AttrColor;
impl AttrColor {
    pub fn new_foreground(r: u16, g: u16, b: u16) -> Attribute {
        Attribute::Foreground(r, g, b)
    }
}

impl crate::Label {
    /// Pango attributes over the whole text: weight, scale, colour.
    pub fn set_attributes(&self, attrs: Option<&AttrList>) {
        let mut weight = 0;
        let mut scale = 1.0;
        let mut color = None;
        for a in attrs.map(|a| a.attributes()).unwrap_or_default() {
            match a {
                Attribute::Weight(w) => weight = w,
                Attribute::Scale(s) => scale = s,
                Attribute::Foreground(r, g, b) => {
                    color = Some(crate::gdk::RGBA::new(r as f32 / 65535.0, g as f32 / 65535.0, b as f32 / 65535.0, 1.0))
                }
            }
        }
        self.set_weight(weight);
        self.set_scale(scale);
        self.set_color(color.as_ref());
    }
    /// A link of the label clicked; Stop keeps it from the desktop's handler.
    pub fn connect_activate_link<F: Fn(&crate::Label, &str) -> Propagation + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_typed::<crate::Label, _>(sig::ACTIVATE_LINK, move |w, a| {
            if f(w, &a.text().unwrap_or_default()) == Propagation::Stop {
                a.stop()
            }
        })
    }
    pub fn selection_bounds(&self) -> Option<(i32, i32)> {
        let (mut s, mut e) = (0, 0);
        unsafe { ffi::awra_label_selection_bounds(self.as_ptr(), &mut s, &mut e) }.then_some((s, e))
    }
    pub fn select_region(&self, start: i32, end: i32) {
        unsafe { ffi::awra_label_select_region(self.as_ptr(), start, end) }
    }
    pub fn set_single_line_mode(&self, single: bool) {
        if single {
            self.set_lines(1)
        }
    }
}

// ===========================================================================
// Text buffers (GtkTextBuffer over an entry's or a text view's text)

/// A position in a text buffer: a character offset.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TextIter {
    widget: Widget,
    offset: i32,
}

impl TextIter {
    pub fn offset(&self) -> i32 {
        self.offset
    }
    pub fn char(&self) -> char {
        let text = TextBuffer(self.widget.clone()).all_text();
        text.chars().nth(self.offset.max(0) as usize).unwrap_or('\0')
    }
    pub fn set_offset(&mut self, offset: i32) {
        self.offset = offset;
    }
    pub fn forward_char(&mut self) -> bool {
        let n = TextBuffer(self.widget.clone()).char_count();
        if self.offset < n {
            self.offset += 1;
        }
        self.offset < n
    }
    pub fn backward_char(&mut self) -> bool {
        if self.offset > 0 {
            self.offset -= 1;
            true
        } else {
            false
        }
    }
    pub fn is_end(&self) -> bool {
        self.offset >= TextBuffer(self.widget.clone()).char_count()
    }
    pub fn is_start(&self) -> bool {
        self.offset <= 0
    }
}

/// The insert mark (gtk-rs `buffer.get_insert()`).
#[derive(Debug, Clone, Copy)]
pub struct TextMark;

/// GtkTextBuffer of an entry or a text view.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TextBuffer(pub Widget);

impl TextBuffer {
    fn all_text(&self) -> String {
        unsafe { crate::object::from_cstr(ffi::awra_entry_text(self.0.as_ptr())) }
    }
    fn iter(&self, offset: i32) -> TextIter {
        TextIter { widget: self.0.clone(), offset: offset.clamp(0, self.char_count()) }
    }
    pub fn char_count(&self) -> i32 {
        unsafe { ffi::awra_editable_length(self.0.as_ptr()) }
    }
    pub fn start_iter(&self) -> TextIter {
        self.iter(0)
    }
    pub fn end_iter(&self) -> TextIter {
        self.iter(self.char_count())
    }
    pub fn bounds(&self) -> (TextIter, TextIter) {
        (self.start_iter(), self.end_iter())
    }
    pub fn iter_at_offset(&self, offset: i32) -> TextIter {
        self.iter(if offset < 0 { self.char_count() } else { offset })
    }
    pub fn cursor_position(&self) -> i32 {
        unsafe { ffi::awra_editable_cursor(self.0.as_ptr()) }
    }
    pub fn get_insert(&self) -> TextMark {
        TextMark
    }
    pub fn iter_at_mark(&self, _mark: &TextMark) -> TextIter {
        self.iter(self.cursor_position())
    }
    pub fn text(&self, start: &TextIter, end: &TextIter, _hidden: bool) -> String {
        self.all_text()
            .chars()
            .skip(start.offset.max(0) as usize)
            .take((end.offset - start.offset).max(0) as usize)
            .collect()
    }
    pub fn set_text(&self, text: &str) {
        let t = cstring(text);
        unsafe { ffi::awra_entry_set_text(self.0.as_ptr(), t.as_ptr()) }
    }
    pub fn insert_at_cursor(&self, text: &str) {
        let t = cstring(text);
        unsafe { ffi::awra_editable_insert(self.0.as_ptr(), -1, t.as_ptr()) }
    }
    /// Inserts at `iter`, which then points after the insertion.
    pub fn insert(&self, iter: &mut TextIter, text: &str) {
        let t = cstring(text);
        unsafe { ffi::awra_editable_insert(self.0.as_ptr(), iter.offset, t.as_ptr()) }
        iter.offset += text.chars().count() as i32;
    }
    pub fn delete(&self, start: &mut TextIter, end: &mut TextIter) {
        unsafe { ffi::awra_editable_delete(self.0.as_ptr(), start.offset, end.offset) }
        end.offset = start.offset;
    }
    pub fn place_cursor(&self, iter: &TextIter) {
        unsafe { ffi::awra_editable_set_cursor(self.0.as_ptr(), iter.offset) }
    }
    pub fn select_range(&self, insert: &TextIter, bound: &TextIter) {
        unsafe { ffi::awra_editable_select(self.0.as_ptr(), bound.offset, insert.offset) }
    }
    pub fn selection_bounds(&self) -> Option<(TextIter, TextIter)> {
        let (mut s, mut e) = (0, 0);
        unsafe { ffi::awra_editable_selection(self.0.as_ptr(), &mut s, &mut e) }.then(|| (self.iter(s), self.iter(e)))
    }
    pub fn has_selection(&self) -> bool {
        self.selection_bounds().is_some()
    }
    pub fn begin_user_action(&self) {}
    pub fn end_user_action(&self) {}
    pub fn connect_changed<F: Fn(&TextBuffer) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_raw(sig::CHANGED, move |w, _| f(&TextBuffer(w.clone())))
    }
    /// "cursor-position" changes.
    pub fn connect_cursor_position_notify<F: Fn(&TextBuffer) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_raw(sig::NOTIFY, move |w, a| {
            if a.text().as_deref() == Some("cursor-position") {
                f(&TextBuffer(w.clone()))
            }
        })
    }
}

impl crate::TextView {
    pub fn buffer(&self) -> TextBuffer {
        TextBuffer(self.upcast())
    }
}

impl crate::Entry {
    pub fn buffer(&self) -> TextBuffer {
        TextBuffer(self.upcast())
    }
    pub fn cursor_position(&self) -> i32 {
        unsafe { ffi::awra_editable_cursor(self.as_ptr()) }
    }
    pub fn insert_text_at_cursor(&self, text: &str) {
        TextBuffer(self.upcast()).insert_at_cursor(text)
    }
}

// ===========================================================================
// Action groups of widgets ("composer.upload")

/// gio::SimpleActionGroup.
#[derive(Clone, Default)]
pub struct SimpleActionGroup(Rc<RefCell<Vec<crate::gio::SimpleAction>>>);

impl SimpleActionGroup {
    pub fn new() -> SimpleActionGroup {
        SimpleActionGroup::default()
    }
    pub fn add_action(&self, action: &crate::gio::SimpleAction) {
        let mut v = self.0.borrow_mut();
        v.retain(|a| a.name() != action.name());
        v.push(action.clone());
    }
    pub fn remove_action(&self, name: &str) {
        self.0.borrow_mut().retain(|a| a.name() != name);
    }
    pub fn lookup_action(&self, name: &str) -> Option<crate::gio::SimpleAction> {
        self.0.borrow().iter().find(|a| a.name() == name).cloned()
    }
    pub fn activate_action(&self, name: &str, target: Option<&crate::gio::Variant>) {
        if let Some(a) = self.lookup_action(name) {
            a.activate(target)
        }
    }
    pub fn add_action_entries<I: IntoIterator<Item = crate::gio::SimpleAction>>(&self, entries: I) {
        for e in entries {
            self.add_action(&e);
        }
    }
}

impl Widget {
    /// Actions named `prefix.name` for this widget and what's inside it.
    pub fn insert_action_group(&self, prefix: &str, group: Option<&SimpleActionGroup>) {
        let mut groups: Vec<(String, SimpleActionGroup)> = self.rust_data("action-groups").unwrap_or_default();
        groups.retain(|(p, _)| p != prefix);
        if let Some(g) = group {
            groups.push((prefix.to_owned(), g.clone()));
        }
        self.set_rust_data("action-groups", groups);
    }
}

/// Finds `prefix.name` in the action groups of `widget` and its ancestors.
pub(crate) fn lookup_group_action(widget: &Widget, detailed: &str) -> Option<crate::gio::SimpleAction> {
    let (prefix, name) = detailed.split_once('.')?;
    let mut w = Some(widget.clone());
    while let Some(cur) = w {
        if let Some(groups) = cur.rust_data::<Vec<(String, SimpleActionGroup)>>("action-groups") {
            if let Some((_, g)) = groups.iter().find(|(p, _)| p == prefix) {
                if let Some(a) = g.lookup_action(name) {
                    return Some(a);
                }
            }
        }
        // a popover's content: its anchor's ancestors next
        w = cur.parent().or_else(|| cur.rust_data::<WeakRef<Widget>>("popover-anchor").and_then(|a| a.upgrade()));
    }
    None
}

// ===========================================================================
// Event controllers

type MotionFn = Rc<dyn Fn(&EventControllerMotion, f64, f64)>;
type LeaveFn = Rc<dyn Fn(&EventControllerMotion)>;

#[derive(Default)]
struct MotionInner {
    enter: RefCell<Vec<MotionFn>>,
    motion: RefCell<Vec<MotionFn>>,
    leave: RefCell<Vec<LeaveFn>>,
    widget: RefCell<Option<WeakRef<Widget>>>,
}

/// GtkEventControllerMotion: the pointer entering, moving over, leaving.
#[derive(Clone, Default)]
pub struct EventControllerMotion(Rc<MotionInner>);

impl EventControllerMotion {
    pub fn new() -> EventControllerMotion {
        EventControllerMotion::default()
    }
    pub fn connect_enter<F: Fn(&EventControllerMotion, f64, f64) + 'static>(&self, f: F) {
        self.0.enter.borrow_mut().push(Rc::new(f));
    }
    pub fn connect_motion<F: Fn(&EventControllerMotion, f64, f64) + 'static>(&self, f: F) {
        self.0.motion.borrow_mut().push(Rc::new(f));
    }
    pub fn connect_leave<F: Fn(&EventControllerMotion) + 'static>(&self, f: F) {
        self.0.leave.borrow_mut().push(Rc::new(f));
    }
    pub fn widget(&self) -> Option<Widget> {
        self.0.widget.borrow().as_ref().and_then(|w| w.upgrade())
    }
    pub fn contains_pointer(&self) -> bool {
        self.widget().is_some_and(|w| w.rust_data::<bool>("pointer-inside").unwrap_or(false))
    }
}

impl Controller for EventControllerMotion {
    fn attach(&self, widget: &Widget) {
        *self.0.widget.borrow_mut() = Some(widget.downgrade());
        let me = self.clone();
        widget.connect_raw(sig::ENTER, move |w, a| {
            w.set_rust_data("pointer-inside", true);
            let hs: Vec<MotionFn> = me.0.enter.borrow().clone();
            for h in hs {
                h(&me, a.x(), a.y());
            }
        });
        let me = self.clone();
        widget.connect_raw(sig::LEAVE, move |w, _| {
            w.set_rust_data("pointer-inside", false);
            let hs: Vec<LeaveFn> = me.0.leave.borrow().clone();
            for h in hs {
                h(&me);
            }
        });
        if !self.0.motion.borrow().is_empty() {
            let me = self.clone();
            widget.connect_raw(sig::MOTION, move |_, a| {
                let hs: Vec<MotionFn> = me.0.motion.borrow().clone();
                for h in hs {
                    h(&me, a.x(), a.y());
                }
            });
        }
    }
}

/// GtkEventControllerScrollFlags.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct EventControllerScrollFlags(u32);
impl EventControllerScrollFlags {
    pub const NONE: Self = Self(0);
    pub const VERTICAL: Self = Self(1);
    pub const HORIZONTAL: Self = Self(2);
    pub const BOTH_AXES: Self = Self(3);
    pub const DISCRETE: Self = Self(4);
    pub const KINETIC: Self = Self(8);
}
impl std::ops::BitOr for EventControllerScrollFlags {
    type Output = Self;
    fn bitor(self, o: Self) -> Self {
        Self(self.0 | o.0)
    }
}

type ScrollFn = Rc<dyn Fn(&EventControllerScroll, f64, f64) -> Propagation>;

/// GtkEventControllerScroll.
#[derive(Clone)]
pub struct EventControllerScroll(Rc<(EventControllerScrollFlags, RefCell<Vec<ScrollFn>>, Cell<PropagationPhase>)>);
use crate::widget::PropagationPhase;

impl EventControllerScroll {
    pub fn new(flags: EventControllerScrollFlags) -> EventControllerScroll {
        EventControllerScroll(Rc::new((flags, RefCell::new(Vec::new()), Cell::new(PropagationPhase::Bubble))))
    }
    pub fn connect_scroll<F: Fn(&EventControllerScroll, f64, f64) -> Propagation + 'static>(&self, f: F) {
        self.0 .1.borrow_mut().push(Rc::new(f));
    }
    pub fn set_propagation_phase(&self, p: PropagationPhase) {
        self.0 .2.set(p)
    }
    pub fn unit(&self) -> crate::gdk::ScrollUnit {
        crate::gdk::ScrollUnit::Wheel
    }
}
impl Controller for EventControllerScroll {
    fn attach(&self, widget: &Widget) {
        let me = self.clone();
        widget.connect_raw(sig::SCROLL, move |_, a| {
            let hs: Vec<ScrollFn> = me.0 .1.borrow().clone();
            for h in hs {
                if h(&me, a.dx(), a.dy()) == Propagation::Stop {
                    a.stop();
                    break;
                }
            }
        });
    }
}

type FocusFn = Rc<dyn Fn(&EventControllerFocus)>;

/// GtkEventControllerFocus: the keyboard focus entering or leaving the
/// widget itself.
#[derive(Clone, Default)]
pub struct EventControllerFocus(Rc<(RefCell<Vec<FocusFn>>, RefCell<Vec<FocusFn>>)>);
impl EventControllerFocus {
    pub fn new() -> EventControllerFocus {
        EventControllerFocus::default()
    }
    pub fn connect_enter<F: Fn(&EventControllerFocus) + 'static>(&self, f: F) {
        self.0 .0.borrow_mut().push(Rc::new(f));
    }
    pub fn connect_leave<F: Fn(&EventControllerFocus) + 'static>(&self, f: F) {
        self.0 .1.borrow_mut().push(Rc::new(f));
    }
}
impl Controller for EventControllerFocus {
    fn attach(&self, widget: &Widget) {
        let me = self.clone();
        widget.connect_raw(sig::NOTIFY, move |w, a| {
            if a.text().as_deref() != Some("has-focus") {
                return;
            }
            let list = if w.has_focus() { &me.0 .0 } else { &me.0 .1 };
            let hs: Vec<FocusFn> = list.borrow().clone();
            for h in hs {
                h(&me);
            }
        });
    }
}

/// gtk::PickFlags.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PickFlags(u32);
impl PickFlags {
    pub const DEFAULT: PickFlags = PickFlags(0);
    pub const INSENSITIVE: PickFlags = PickFlags(1);
    pub const NON_TARGETABLE: PickFlags = PickFlags(2);
}

/// gtk::SizeRequestMode.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SizeRequestMode {
    HeightForWidth,
    WidthForHeight,
    ConstantSize,
}

/// gtk::InputPurpose.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum InputPurpose {
    FreeForm,
    Alpha,
    Digits,
    Number,
    Phone,
    Url,
    Email,
    Name,
    Password,
    Pin,
    Terminal,
}

/// A tick callback (runs every frame on the UI thread until removed).
#[derive(Debug)]
pub struct TickCallbackId(crate::glib::SourceId);
impl TickCallbackId {
    pub fn remove(self) {
        self.0.remove()
    }
}

impl Widget {
    /// Calls `f` about every frame (16 ms) while it returns Continue.
    pub fn add_tick_callback<F: FnMut(&Widget, &FrameClock) -> crate::glib::ControlFlow + 'static>(
        &self,
        mut f: F,
    ) -> TickCallbackId {
        let weak = self.downgrade();
        let clock = FrameClock;
        TickCallbackId(crate::glib::timeout_add_local(std::time::Duration::from_millis(16), move || {
            match weak.upgrade() {
                Some(w) => f(&w, &clock),
                None => crate::glib::ControlFlow::Break,
            }
        }))
    }
}

/// GdkFrameClock: the time of the frame.
#[derive(Debug, Clone, Copy)]
pub struct FrameClock;
impl FrameClock {
    /// Microseconds, monotonic.
    pub fn frame_time(&self) -> i64 {
        (crate::now_ms() * 1000.0) as i64
    }
}

/// Registers an icon from SVG source (application icons).
pub fn register_icon_svg(name: &str, svg: &[u8]) -> bool {
    let n = cstring(name);
    unsafe { ffi::awra_icon_register_svg(n.as_ptr(), svg.as_ptr() as *const std::ffi::c_char, svg.len()) }
}

impl<T: StaticType> crate::object::WeakRef<T> {}

/// Adds a column to a C data view with a factory for its cells.
pub(crate) fn add_data_column(view: &Widget, title: &str, factory: SignalListItemFactory) -> i32 {
    let t = cstring(title);
    let data = Box::into_raw(Box::new(factory)) as *mut c_void;
    unsafe {
        ffi::awra_data_view_add_column(
            view.as_ptr(),
            t.as_ptr(),
            Some(factory_cb::<0>),
            Some(factory_cb::<1>),
            Some(factory_cb::<2>),
            Some(factory_cb::<3>),
            data,
            Some(factory_free),
        )
    }
}
