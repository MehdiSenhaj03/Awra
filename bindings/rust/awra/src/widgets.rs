//! Typed widgets.

use std::cell::RefCell;
use std::ffi::{c_char, c_void};
use std::path::PathBuf;
use std::rc::Rc;

use crate::awra_wrapper;
use crate::enums::*;
use crate::ffi;
use crate::gdk::{ModifierType, RGBA};
use crate::gio::{FlatEntry, Menu};
use crate::object::{cstring, from_cstr, from_cstr_opt, IntoOptStr, IntoOptWidget, IsWidget, OptC, Widget};
use crate::pango::{EllipsizeMode, WrapMode};
use crate::signal::{sig, SignalHandlerId};

macro_rules! ptr {
    ($s:expr) => {
        $s.0.as_ptr()
    };
}

fn rgba_ptr(c: Option<&RGBA>) -> (ffi::AwraRGBA, bool) {
    match c {
        Some(c) => (c.to_ffi(), true),
        None => (ffi::AwraRGBA { r: 0.0, g: 0.0, b: 0.0, a: 0.0 }, false),
    }
}

macro_rules! with_rgba {
    ($c:expr, |$p:ident| $body:expr) => {{
        let (v, some) = rgba_ptr($c);
        let $p: *const ffi::AwraRGBA = if some { &v } else { std::ptr::null() };
        $body
    }};
}

/// gtk-rs style "parent with children" operations.
macro_rules! container_ops {
    ($t:ty) => {
        impl $t {
            pub fn append(&self, child: &impl IsWidget) {
                unsafe { ffi::awra_container_add(ptr!(self), child.widget_ptr()) }
            }
            pub fn remove(&self, child: &impl IsWidget) {
                unsafe { ffi::awra_container_remove(ptr!(self), child.widget_ptr()) }
            }
            pub fn remove_all(&self) {
                unsafe { ffi::awra_container_remove_all(ptr!(self)) }
            }
        }
    };
}

// ===========================================================================
// Layout

awra_wrapper!(
    /// GtkBox.
    Box, ["AwraBox"]
);
impl Box {
    pub fn new(orientation: Orientation, spacing: i32) -> Box {
        unsafe { Box::from_full(ffi::awra_box_new(orientation as _, spacing)) }
    }
    pub fn append(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_box_append(ptr!(self), child.widget_ptr()) }
    }
    pub fn prepend(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_box_prepend(ptr!(self), child.widget_ptr()) }
    }
    pub fn insert_child_after(&self, child: &impl IsWidget, sibling: Option<&impl IsWidget>) {
        let s = sibling.map_or(std::ptr::null_mut(), |s| s.widget_ptr());
        unsafe { ffi::awra_box_insert_after(ptr!(self), child.widget_ptr(), s) }
    }
    pub fn reorder_child_after(&self, child: &impl IsWidget, sibling: Option<&impl IsWidget>) {
        let keep = child.upcast();
        unsafe { ffi::awra_box_remove(ptr!(self), keep.as_ptr()) };
        self.insert_child_after(&keep, sibling);
    }
    pub fn remove(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_box_remove(ptr!(self), child.widget_ptr()) }
    }
    pub fn remove_all(&self) {
        unsafe { ffi::awra_box_remove_all(ptr!(self)) }
    }
    pub fn set_spacing(&self, spacing: i32) {
        unsafe { ffi::awra_box_set_spacing(ptr!(self), spacing) }
    }
    pub fn set_homogeneous(&self, homogeneous: bool) {
        unsafe { ffi::awra_box_set_homogeneous(ptr!(self), homogeneous) }
    }
    pub fn set_orientation(&self, orientation: Orientation) {
        unsafe { ffi::awra_box_set_orientation(ptr!(self), orientation as _) }
    }
}

awra_wrapper!(
    /// GtkGrid.
    Grid, ["AwraGrid"]
);
impl Grid {
    pub fn new() -> Grid {
        unsafe { Grid::from_full(ffi::awra_grid_new()) }
    }
    pub fn attach(&self, child: &impl IsWidget, column: i32, row: i32, width: i32, height: i32) {
        unsafe { ffi::awra_grid_attach(ptr!(self), child.widget_ptr(), column, row, width, height) }
    }
    pub fn set_row_spacing(&self, s: u32) {
        unsafe { ffi::awra_grid_set_row_spacing(ptr!(self), s as i32) }
    }
    pub fn set_column_spacing(&self, s: u32) {
        unsafe { ffi::awra_grid_set_column_spacing(ptr!(self), s as i32) }
    }
    pub fn set_row_homogeneous(&self, h: bool) {
        unsafe { ffi::awra_grid_set_homogeneous(ptr!(self), h, false) }
    }
    pub fn set_column_homogeneous(&self, h: bool) {
        unsafe { ffi::awra_grid_set_homogeneous(ptr!(self), false, h) }
    }
}
container_ops!(Grid);
impl Default for Grid {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(
    /// GtkFlowBox (children are not wrapped in FlowBoxChild).
    FlowBox, ["AwraFlowBox"]
);
type KeepFn = std::boxed::Box<dyn Fn(&Widget) -> bool>;
unsafe extern "C" fn flow_keep(child: *mut ffi::AwraWidget, data: *mut c_void) -> bool {
    let f = &*(data as *const KeepFn);
    let w = std::mem::ManuallyDrop::new(Widget::from_full(child));
    f(&w)
}
unsafe extern "C" fn flow_keep_free(data: *mut c_void) {
    drop(std::boxed::Box::from_raw(data as *mut KeepFn));
}
impl FlowBox {
    pub fn new() -> FlowBox {
        unsafe { FlowBox::from_full(ffi::awra_flow_box_new()) }
    }
    pub fn append(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_flow_box_append(ptr!(self), child.widget_ptr()) }
    }
    pub fn insert(&self, child: &impl IsWidget, position: i32) {
        unsafe { ffi::awra_flow_box_insert(ptr!(self), child.widget_ptr(), position) }
    }
    pub fn remove(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_container_remove(ptr!(self), child.widget_ptr()) }
    }
    pub fn remove_all(&self) {
        unsafe { ffi::awra_container_remove_all(ptr!(self)) }
    }
    fn spacing(&self) -> (i32, i32) {
        self.0.rust_data::<(i32, i32)>("flow-spacing").unwrap_or((0, 0))
    }
    pub fn set_row_spacing(&self, s: u32) {
        let (_, c) = self.spacing();
        self.0.set_rust_data("flow-spacing", (s as i32, c));
        unsafe { ffi::awra_flow_box_set_spacing(ptr!(self), s as i32, c) }
    }
    pub fn set_column_spacing(&self, s: u32) {
        let (r, _) = self.spacing();
        self.0.set_rust_data("flow-spacing", (r, s as i32));
        unsafe { ffi::awra_flow_box_set_spacing(ptr!(self), r, s as i32) }
    }
    fn columns(&self) -> (i32, i32) {
        self.0.rust_data::<(i32, i32)>("flow-columns").unwrap_or((1, 7))
    }
    pub fn set_min_children_per_line(&self, n: u32) {
        let (_, max) = self.columns();
        self.0.set_rust_data("flow-columns", (n as i32, max));
        unsafe { ffi::awra_flow_box_set_columns(ptr!(self), n as i32, max) }
    }
    pub fn set_max_children_per_line(&self, n: u32) {
        let (min, _) = self.columns();
        self.0.set_rust_data("flow-columns", (min, n as i32));
        unsafe { ffi::awra_flow_box_set_columns(ptr!(self), min, n as i32) }
    }
    pub fn set_homogeneous(&self, h: bool) {
        unsafe { ffi::awra_flow_box_set_homogeneous(ptr!(self), h) }
    }
    pub fn set_selection_mode(&self, mode: SelectionMode) {
        unsafe { ffi::awra_flow_box_set_selection_mode(ptr!(self), mode as _) }
    }
    pub fn select_child(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_flow_box_select(ptr!(self), child.widget_ptr()) }
    }
    pub fn selected_child(&self) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_flow_box_selected(ptr!(self))) }
    }
    pub fn set_filter_func<F: Fn(&Widget) -> bool + 'static>(&self, f: F) {
        let data: std::boxed::Box<KeepFn> = std::boxed::Box::new(std::boxed::Box::new(f));
        unsafe {
            ffi::awra_flow_box_set_filter(
                ptr!(self),
                Some(flow_keep),
                std::boxed::Box::into_raw(data) as *mut c_void,
                Some(flow_keep_free),
            )
        }
    }
    pub fn invalidate_filter(&self) {
        unsafe { ffi::awra_flow_box_invalidate_filter(ptr!(self)) }
    }
    pub fn connect_child_activated<F: Fn(&FlowBox, &Widget) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<FlowBox, _>(sig::ACTIVATE, move |w, a| {
            if let Some(c) = w.0.children().get(a.index().max(0) as usize) {
                f(w, c)
            }
        })
    }
}
impl Default for FlowBox {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(
    /// GtkStack.
    Stack, ["AwraStack"]
);
impl Stack {
    pub fn new() -> Stack {
        unsafe { Stack::from_full(ffi::awra_stack_new()) }
    }
    pub fn add_named(&self, child: &impl IsWidget, name: impl IntoOptStr) -> Widget {
        let n = OptC::new(name);
        unsafe { ffi::awra_stack_add_named(ptr!(self), child.widget_ptr(), n.ptr()) };
        child.upcast()
    }
    pub fn add_child(&self, child: &impl IsWidget) -> Widget {
        self.add_named(child, None::<&str>)
    }
    pub fn remove(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_container_remove(ptr!(self), child.widget_ptr()) }
    }
    pub fn set_visible_child(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_stack_set_visible_child(ptr!(self), child.widget_ptr()) }
    }
    pub fn set_visible_child_name(&self, name: &str) {
        let n = cstring(name);
        unsafe { ffi::awra_stack_set_visible_child_name(ptr!(self), n.as_ptr()) }
    }
    pub fn visible_child(&self) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_stack_visible_child(ptr!(self))) }
    }
    pub fn visible_child_name(&self) -> Option<String> {
        unsafe { from_cstr_opt(ffi::awra_stack_visible_child_name(ptr!(self))) }
    }
    pub fn child_by_name(&self, name: &str) -> Option<Widget> {
        let n = cstring(name);
        unsafe { Widget::from_none_opt(ffi::awra_stack_child_by_name(ptr!(self), n.as_ptr())) }
    }
    fn transition(&self) -> (StackTransitionType, u32) {
        self.0.rust_data("stack-transition").unwrap_or((StackTransitionType::None, 200))
    }
    pub fn set_transition_type(&self, t: StackTransitionType) {
        let (_, ms) = self.transition();
        self.0.set_rust_data("stack-transition", (t, ms));
        unsafe { ffi::awra_stack_set_transition(ptr!(self), t as _, ms) }
    }
    pub fn set_transition_duration(&self, ms: u32) {
        let (t, _) = self.transition();
        self.0.set_rust_data("stack-transition", (t, ms));
        unsafe { ffi::awra_stack_set_transition(ptr!(self), t as _, ms) }
    }
    pub fn set_hhomogeneous(&self, h: bool) {
        unsafe { ffi::awra_stack_set_hhomogeneous(ptr!(self), h) }
    }
    pub fn set_vhomogeneous(&self, h: bool) {
        unsafe { ffi::awra_stack_set_vhomogeneous(ptr!(self), h) }
    }
    pub fn connect_visible_child_notify<F: Fn(&Stack) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Stack, _>(sig::SELECTED, move |w, _| f(w))
    }
}
impl Default for Stack {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(
    /// GtkOverlay.
    Overlay, ["AwraOverlay"]
);
impl Overlay {
    pub fn new() -> Overlay {
        unsafe { Overlay::from_full(ffi::awra_overlay_new()) }
    }
    pub fn set_child(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_overlay_set_child(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn add_overlay(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_overlay_add_overlay(ptr!(self), child.widget_ptr()) }
    }
    pub fn remove_overlay(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_container_remove(ptr!(self), child.widget_ptr()) }
    }
}
impl Default for Overlay {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(
    /// GtkScrolledWindow.
    ScrolledWindow, ["AwraScrolledWindow"]
);
impl ScrolledWindow {
    pub fn new() -> ScrolledWindow {
        unsafe { ScrolledWindow::from_full(ffi::awra_scrolled_new()) }
    }
    pub fn set_child(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_scrolled_set_child(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn child(&self) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_scrolled_child(ptr!(self))) }
    }
    fn policy(&self) -> (PolicyType, PolicyType) {
        self.0.rust_data("scroll-policy").unwrap_or((PolicyType::Automatic, PolicyType::Automatic))
    }
    pub fn set_policy(&self, h: PolicyType, v: PolicyType) {
        self.0.set_rust_data("scroll-policy", (h, v));
        unsafe { ffi::awra_scrolled_set_policy(ptr!(self), h as _, v as _) }
    }
    pub fn set_hscrollbar_policy(&self, h: PolicyType) {
        let (_, v) = self.policy();
        self.set_policy(h, v)
    }
    pub fn set_vscrollbar_policy(&self, v: PolicyType) {
        let (h, _) = self.policy();
        self.set_policy(h, v)
    }
    fn propagate(&self) -> (bool, bool) {
        self.0.rust_data("scroll-propagate").unwrap_or((false, false))
    }
    pub fn set_propagate_natural_width(&self, p: bool) {
        let (_, h) = self.propagate();
        self.0.set_rust_data("scroll-propagate", (p, h));
        unsafe { ffi::awra_scrolled_set_propagate_natural(ptr!(self), p, h) }
    }
    pub fn set_propagate_natural_height(&self, p: bool) {
        let (w, _) = self.propagate();
        self.0.set_rust_data("scroll-propagate", (w, p));
        unsafe { ffi::awra_scrolled_set_propagate_natural(ptr!(self), w, p) }
    }
    pub fn set_max_content_height(&self, h: i32) {
        unsafe { ffi::awra_scrolled_set_max_content_height(ptr!(self), h) }
    }
    pub fn set_min_content_height(&self, h: i32) {
        unsafe { ffi::awra_scrolled_set_min_content_height(ptr!(self), h) }
    }
    pub fn vvalue(&self) -> f64 {
        unsafe { ffi::awra_scrolled_vvalue(ptr!(self)) }
    }
    pub fn set_vvalue(&self, v: f64) {
        unsafe { ffi::awra_scrolled_set_vvalue(ptr!(self), v) }
    }
    pub fn hvalue(&self) -> f64 {
        unsafe { ffi::awra_scrolled_hvalue(ptr!(self)) }
    }
    pub fn set_hvalue(&self, v: f64) {
        unsafe { ffi::awra_scrolled_set_hvalue(ptr!(self), v) }
    }
    pub fn scroll_to(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_scrolled_scroll_to(ptr!(self), child.widget_ptr()) }
    }
}
impl Default for ScrolledWindow {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(Separator, ["AwraSeparator"]);
impl Separator {
    pub fn new(orientation: Orientation) -> Separator {
        unsafe { Separator::from_full(ffi::awra_separator_new(orientation as _)) }
    }
}

awra_wrapper!(Spacer, ["AwraSpacer"]);
impl Spacer {
    pub fn new() -> Spacer {
        unsafe { Spacer::from_full(ffi::awra_spacer_new()) }
    }
}
impl Default for Spacer {
    fn default() -> Self {
        Self::new()
    }
}

// ===========================================================================
// Text and images

awra_wrapper!(
    /// GtkLabel.
    Label, ["AwraLabel"]
);
impl Label {
    pub fn new(text: Option<&str>) -> Label {
        let t = OptC::new(text);
        unsafe { Label::from_full(ffi::awra_label_new(t.ptr())) }
    }
    pub fn with_mnemonic(text: &str) -> Label {
        Label::new(Some(&text.replace('_', "")))
    }
    pub fn set_text(&self, text: &str) {
        let t = cstring(text);
        unsafe { ffi::awra_label_set_text(ptr!(self), t.as_ptr()) }
    }
    pub fn set_label(&self, text: &str) {
        self.set_text(text)
    }
    pub fn text(&self) -> String {
        unsafe { from_cstr(ffi::awra_label_text(ptr!(self))) }
    }
    pub fn label(&self) -> String {
        self.text()
    }
    pub fn set_markup(&self, markup: &str) {
        let t = cstring(markup);
        unsafe { ffi::awra_label_set_markup(ptr!(self), t.as_ptr()) }
    }
    pub fn set_wrap(&self, wrap: bool) {
        let mode = self.0.rust_data::<WrapMode>("wrap-mode").unwrap_or(WrapMode::Word);
        self.0.set_rust_data("wrap-on", wrap);
        unsafe { ffi::awra_label_set_wrap(ptr!(self), if wrap { wrap_c(mode) } else { 0 }) }
    }
    pub fn set_wrap_mode(&self, mode: WrapMode) {
        self.0.set_rust_data("wrap-mode", mode);
        if self.0.rust_data::<bool>("wrap-on").unwrap_or(false) {
            unsafe { ffi::awra_label_set_wrap(ptr!(self), wrap_c(mode)) }
        }
    }
    pub fn set_natural_wrap_mode<T>(&self, _mode: T) {}
    pub fn set_ellipsize(&self, mode: EllipsizeMode) {
        unsafe { ffi::awra_label_set_ellipsize(ptr!(self), mode as i32 as _) }
    }
    pub fn set_xalign(&self, x: f32) {
        unsafe { ffi::awra_label_set_xalign(ptr!(self), x) }
    }
    pub fn set_yalign(&self, y: f32) {
        unsafe { ffi::awra_label_set_yalign(ptr!(self), y) }
    }
    pub fn set_justify(&self, j: Justification) {
        unsafe { ffi::awra_label_set_justify(ptr!(self), j as _) }
    }
    pub fn set_lines(&self, lines: i32) {
        unsafe { ffi::awra_label_set_lines(ptr!(self), lines) }
    }
    pub fn set_max_width_chars(&self, n: i32) {
        unsafe { ffi::awra_label_set_max_width_chars(ptr!(self), n) }
    }
    pub fn set_width_chars(&self, n: i32) {
        unsafe { ffi::awra_label_set_width_chars(ptr!(self), n) }
    }
    pub fn set_selectable(&self, s: bool) {
        unsafe { ffi::awra_label_set_selectable(ptr!(self), s) }
    }
    pub fn set_use_markup(&self, _markup: bool) {}
    pub fn set_mnemonic_widget(&self, _w: Option<&impl IsWidget>) {}
    pub fn set_color(&self, c: Option<&RGBA>) {
        with_rgba!(c, |p| unsafe { ffi::awra_label_set_color(ptr!(self), p) })
    }
    pub fn set_weight(&self, w: i32) {
        unsafe { ffi::awra_label_set_weight(ptr!(self), w) }
    }
    pub fn set_scale(&self, s: f64) {
        unsafe { ffi::awra_label_set_scale(ptr!(self), s) }
    }
    pub fn set_tabular_digits(&self, t: bool) {
        unsafe { ffi::awra_label_set_monospace_digits(ptr!(self), t) }
    }
}
fn wrap_c(mode: WrapMode) -> i32 {
    match mode {
        WrapMode::Word => 1,
        WrapMode::Char => 2,
        WrapMode::WordChar => 1,
    }
}

awra_wrapper!(
    /// GtkImage.
    Image, ["AwraImage"]
);
impl Image {
    pub fn new() -> Image {
        unsafe { Image::from_full(ffi::awra_image_new()) }
    }
    pub fn from_icon_name(name: &str) -> Image {
        let n = cstring(name);
        unsafe { Image::from_full(ffi::awra_image_new_from_icon_name(n.as_ptr())) }
    }
    pub fn from_file(path: impl AsRef<std::path::Path>) -> Image {
        let i = Image::new();
        i.set_from_file(Some(path));
        i
    }
    pub fn set_icon_name(&self, name: Option<&str>) {
        let n = OptC::new(name);
        unsafe { ffi::awra_image_set_icon_name(ptr!(self), n.ptr()) }
    }
    pub fn set_from_icon_name(&self, name: Option<&str>) {
        self.set_icon_name(name)
    }
    pub fn set_pixel_size(&self, size: i32) {
        unsafe { ffi::awra_image_set_pixel_size(ptr!(self), size) }
    }
    pub fn set_from_file(&self, path: Option<impl AsRef<std::path::Path>>) {
        let p = path.map(|p| cstring(&p.as_ref().to_string_lossy()));
        unsafe { ffi::awra_image_set_file(ptr!(self), p.as_ref().map_or(std::ptr::null(), |c| c.as_ptr())) }
    }
    pub fn set_from_bytes(&self, bytes: &[u8]) {
        unsafe { ffi::awra_image_set_bytes(ptr!(self), bytes.as_ptr() as *const c_void, bytes.len()) }
    }
    pub fn set_from_rgba(&self, rgba: &[u8], width: i32, height: i32, stride: i32) {
        unsafe { ffi::awra_image_set_rgba(ptr!(self), rgba.as_ptr(), width, height, stride) }
    }
    pub fn set_corner_radius(&self, r: f64) {
        unsafe { ffi::awra_image_set_corner_radius(ptr!(self), r) }
    }
    pub fn set_content_fit(&self, fit: ContentFit) {
        unsafe { ffi::awra_image_set_content_fit(ptr!(self), (fit as i32).min(2)) }
    }
    pub fn set_color(&self, c: Option<&RGBA>) {
        with_rgba!(c, |p| unsafe { ffi::awra_image_set_color(ptr!(self), p) })
    }
}
impl Default for Image {
    fn default() -> Self {
        Self::new()
    }
}

/// GtkPicture: an image scaled to its allocation.
pub type Picture = Image;

// ===========================================================================
// Buttons

/// Buttons bound to `win.*` actions.
macro_rules! actionable {
    ($t:ty) => {
        impl $t {
            pub fn set_action_name(&self, name: Option<&str>) {
                self.set_action_target_internal(name, None)
            }
            pub fn set_action_target_value(&self, target: Option<&crate::gio::Variant>) {
                let name = self.0.rust_data::<String>("action-name");
                self.set_action_target_internal(name.as_deref(), target.cloned())
            }
            pub fn set_detailed_action_name(&self, detailed: &str) {
                match detailed.split_once("::") {
                    Some((n, t)) => self.set_action_target_internal(Some(n), Some(crate::gio::ToVariant::to_variant(t))),
                    None => self.set_action_target_internal(Some(detailed), None),
                }
            }
            fn set_action_target_internal(&self, name: Option<&str>, target: Option<crate::gio::Variant>) {
                let first = self.0.rust_data::<String>("action-name").is_none();
                match name {
                    Some(n) => {
                        self.0.set_rust_data("action-name", n.to_owned());
                        self.0.set_rust_data("action-target", target);
                        crate::gio::bind_widget(&self.0, n);
                    }
                    None => self.0.set_rust_data("action-name", String::new()),
                }
                if first {
                    self.0.connect_raw(sig::CLICKED, |w, _| {
                        let name = w.rust_data::<String>("action-name").unwrap_or_default();
                        if !name.is_empty() {
                            let target = w.rust_data::<Option<crate::gio::Variant>>("action-target").flatten();
                            crate::gio::activate_action(w, &name, target.as_ref());
                        }
                    });
                }
            }
        }
    };
}

awra_wrapper!(Button, ["AwraButton"]);
impl Button {
    pub fn new() -> Button {
        unsafe { Button::from_full(ffi::awra_button_new()) }
    }
    pub fn with_label(label: &str) -> Button {
        let l = cstring(label);
        unsafe { Button::from_full(ffi::awra_button_new_with_label(l.as_ptr())) }
    }
    pub fn from_icon_name(name: &str) -> Button {
        let n = cstring(name);
        unsafe { Button::from_full(ffi::awra_button_new_from_icon_name(n.as_ptr())) }
    }
    pub fn set_label(&self, label: impl IntoOptStr) {
        let l = OptC::new(label);
        unsafe { ffi::awra_button_set_label(ptr!(self), l.ptr()) }
    }
    pub fn label(&self) -> Option<String> {
        unsafe { from_cstr_opt(ffi::awra_button_label(ptr!(self))) }
    }
    pub fn set_icon_name(&self, name: impl IntoOptStr) {
        let n = OptC::new(name);
        unsafe { ffi::awra_button_set_icon_name(ptr!(self), n.ptr()) }
    }
    pub fn set_child(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_button_set_child(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn set_appearance(&self, a: ButtonAppearance) {
        unsafe { ffi::awra_button_set_appearance(ptr!(self), a as _) }
    }
    pub fn appearance(&self) -> ButtonAppearance {
        match unsafe { ffi::awra_button_appearance(ptr!(self)) } {
            0 => ButtonAppearance::Primary,
            2 => ButtonAppearance::Ghost,
            3 => ButtonAppearance::Destructive,
            4 => ButtonAppearance::Toolbar,
            _ => ButtonAppearance::Secondary,
        }
    }
    pub fn set_has_frame(&self, frame: bool) {
        unsafe { ffi::awra_button_set_has_frame(ptr!(self), frame) }
    }
    pub fn set_use_underline(&self, _u: bool) {}
    pub fn connect_clicked<F: Fn(&Button) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Button, _>(sig::CLICKED, move |w, _| f(w))
    }
    pub fn emit_clicked(&self) {
        self.0.emit_raw(&crate::signal::raw_args(sig::CLICKED));
    }
}
actionable!(Button);
impl Default for Button {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(ToggleButton, ["AwraToggleButton"]);
impl ToggleButton {
    pub fn new() -> ToggleButton {
        unsafe { ToggleButton::from_full(ffi::awra_toggle_button_new()) }
    }
    pub fn with_label(label: &str) -> ToggleButton {
        let l = cstring(label);
        unsafe { ToggleButton::from_full(ffi::awra_toggle_button_new_with_label(l.as_ptr())) }
    }
    pub fn as_button(&self) -> Button {
        Button(self.0.clone())
    }
    pub fn set_label(&self, label: impl IntoOptStr) {
        self.as_button().set_label(label)
    }
    pub fn set_icon_name(&self, name: impl IntoOptStr) {
        self.as_button().set_icon_name(name)
    }
    pub fn set_child(&self, child: impl IntoOptWidget) {
        self.as_button().set_child(child)
    }
    pub fn set_appearance(&self, a: ButtonAppearance) {
        self.as_button().set_appearance(a)
    }
    pub fn set_active(&self, active: bool) {
        unsafe { ffi::awra_toggle_button_set_active(ptr!(self), active) }
    }
    pub fn is_active(&self) -> bool {
        unsafe { ffi::awra_toggle_button_active(ptr!(self)) }
    }
    pub fn set_group(&self, group: Option<&ToggleButton>) {
        unsafe { ffi::awra_toggle_button_set_group(ptr!(self), group.map_or(std::ptr::null_mut(), |g| g.0.as_ptr())) }
    }
    pub fn connect_toggled<F: Fn(&ToggleButton) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<ToggleButton, _>(sig::TOGGLED, move |w, _| f(w))
    }
    pub fn connect_active_notify<F: Fn(&ToggleButton) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_toggled(f)
    }
    pub fn connect_clicked<F: Fn(&ToggleButton) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<ToggleButton, _>(sig::CLICKED, move |w, _| f(w))
    }
}
actionable!(ToggleButton);
impl Default for ToggleButton {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(CheckButton, ["AwraCheckButton"]);
impl CheckButton {
    /// Awra 1: the inner GTK widget; here the widget itself.
    pub fn delegate(&self) -> CheckButton {
        self.clone()
    }
    pub fn new() -> CheckButton {
        unsafe { CheckButton::from_full(ffi::awra_check_button_new()) }
    }
    pub fn with_label(label: &str) -> CheckButton {
        let l = cstring(label);
        unsafe { CheckButton::from_full(ffi::awra_check_button_new_with_label(l.as_ptr())) }
    }
    pub fn set_active(&self, a: bool) {
        unsafe { ffi::awra_check_button_set_active(ptr!(self), a) }
    }
    pub fn is_active(&self) -> bool {
        unsafe { ffi::awra_check_button_active(ptr!(self)) }
    }
    pub fn set_label(&self, label: impl IntoOptStr) {
        let l = OptC::new(label);
        unsafe { ffi::awra_check_button_set_label(ptr!(self), l.ptr()) }
    }
    /// A label widget as the label (its text).
    pub fn set_child(&self, child: Option<&impl IsWidget>) {
        match child.and_then(|c| c.upcast().downcast::<Label>().ok()) {
            Some(l) => self.set_label(l.text()),
            None => self.set_label(None::<&str>),
        }
    }
    pub fn set_group(&self, group: Option<&CheckButton>) {
        unsafe { ffi::awra_check_button_set_group(ptr!(self), group.map_or(std::ptr::null_mut(), |g| g.0.as_ptr())) }
    }
    pub fn set_inconsistent(&self, i: bool) {
        unsafe { ffi::awra_check_button_set_inconsistent(ptr!(self), i) }
    }
    pub fn connect_toggled<F: Fn(&CheckButton) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<CheckButton, _>(sig::TOGGLED, move |w, _| f(w))
    }
    pub fn connect_active_notify<F: Fn(&CheckButton) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_toggled(f)
    }
}
impl Default for CheckButton {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(Switch, ["AwraSwitch"]);
impl Switch {
    /// Awra 1: the inner GTK widget; here the widget itself.
    pub fn delegate(&self) -> Switch {
        self.clone()
    }
    pub fn new() -> Switch {
        unsafe { Switch::from_full(ffi::awra_switch_new()) }
    }
    pub fn set_active(&self, a: bool) {
        unsafe { ffi::awra_switch_set_active(ptr!(self), a) }
    }
    pub fn is_active(&self) -> bool {
        unsafe { ffi::awra_switch_active(ptr!(self)) }
    }
    pub fn connect_active_notify<F: Fn(&Switch) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Switch, _>(sig::TOGGLED, move |w, _| f(w))
    }
    /// gtk's state-set: return Propagation::Stop to refuse.
    pub fn connect_state_set<F: Fn(&Switch, bool) -> crate::glib::Propagation + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Switch, _>(sig::TOGGLED, move |w, a| {
            let _ = f(w, a.active());
        })
    }
}
impl Default for Switch {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(SegmentedControl, ["AwraSegmentedControl"]);
impl SegmentedControl {
    /// Awra 1: the inner GTK widget; here the widget itself.
    pub fn delegate(&self) -> SegmentedControl {
        self.clone()
    }
    pub fn new() -> SegmentedControl {
        unsafe { SegmentedControl::from_full(ffi::awra_segmented_control_new()) }
    }
    pub fn append(&self, label: &str) -> i32 {
        self.append_with_icon(label, None)
    }
    pub fn append_with_icon(&self, label: &str, icon: Option<&str>) -> i32 {
        let (l, i) = (cstring(label), OptC::new(icon));
        unsafe { ffi::awra_segmented_control_append(ptr!(self), l.as_ptr(), i.ptr()) }
    }
    /// Awra 1: (control, selected index).
    pub fn connect_changed<F: Fn(&SegmentedControl, u32) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<SegmentedControl, _>(sig::SELECTED, move |w, a| f(w, a.index().max(0) as u32))
    }
    /// Awra 1: (id, label) segments.
    pub fn append_segment(&self, _id: &str, label: &str) -> i32 {
        self.append(label)
    }
    pub fn set_selected(&self, index: u32) {
        unsafe { ffi::awra_segmented_control_set_selected(ptr!(self), index as i32) }
    }
    pub fn selected(&self) -> u32 {
        unsafe { ffi::awra_segmented_control_selected(ptr!(self)) }.max(0) as u32
    }
    pub fn remove_all(&self) {
        unsafe { ffi::awra_segmented_control_remove_all(ptr!(self)) }
    }
    pub fn connect_selected_notify<F: Fn(&SegmentedControl) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<SegmentedControl, _>(sig::SELECTED, move |w, _| f(w))
    }
}
impl Default for SegmentedControl {
    fn default() -> Self {
        Self::new()
    }
}

// ===========================================================================
// Menus

type ActivateItem = (Option<String>, Option<crate::gio::Variant>);

struct MenuDispatch {
    items: RefCell<Vec<ActivateItem>>,
    owner: crate::WeakRef<Widget>,
    _watch: RefCell<Option<Rc<dyn Fn()>>>,
}

unsafe extern "C" fn menu_item_activated(_w: *mut ffi::AwraWidget, args: *const ffi::AwraArgs, data: *mut c_void) {
    let d = &*(data as *const MenuDispatch);
    let index = (*args).index.max(0) as usize;
    let item = d.items.borrow().get(index).cloned();
    if let (Some((Some(action), target)), Some(owner)) = (item, d.owner.upgrade()) {
        crate::gio::activate_action(&owner, &action, target.as_ref());
    }
}

/// AwraMenuItems for a menu model; strings live in `keep`.
fn menu_items(menu: &Menu, keep: &mut Vec<std::ffi::CString>, dispatch: *mut c_void) -> (Vec<ffi::AwraMenuItem>, Vec<ActivateItem>) {
    let mut items = Vec::new();
    let mut acts = Vec::new();
    let push_str = |s: Option<&str>, keep: &mut Vec<std::ffi::CString>| -> *const c_char {
        match s {
            Some(s) => {
                keep.push(cstring(s));
                keep.last().unwrap().as_ptr()
            }
            None => std::ptr::null(),
        }
    };
    for e in menu.flatten() {
        let mut it: ffi::AwraMenuItem = unsafe { std::mem::zeroed() };
        it.check = -1;
        it.sensitive = true;
        match e {
            FlatEntry::Separator => {
                acts.push((None, None));
            }
            FlatEntry::Heading(h) => {
                it.label = push_str(Some(&h), keep);
                it.check = -2;
                acts.push((None, None));
            }
            FlatEntry::Item(d, _) => {
                it.label = push_str(Some(d.label.as_deref().unwrap_or("")), keep);
                it.icon_name = push_str(d.icon.as_deref(), keep);
                it.accel = push_str(d.accel.as_deref().map(crate::window::accel_label).as_deref(), keep);
                it.handler = Some(menu_item_activated);
                it.data = dispatch;
                acts.push((d.action.clone(), d.target.clone()));
            }
        }
        items.push(it);
    }
    (items, acts)
}

unsafe extern "C" fn dispatch_free(data: *mut c_void) {
    drop(std::boxed::Box::from_raw(data as *mut MenuDispatch));
}

/// Shows `menu` in a menu button / popover, following its changes.
fn attach_menu(owner: &Widget, menu: Option<&Menu>, apply: fn(&Widget, &[ffi::AwraMenuItem])) {
    let key = c"awra-rs-menu";
    let dispatch = std::boxed::Box::into_raw(std::boxed::Box::new(MenuDispatch {
        items: RefCell::new(Vec::new()),
        owner: owner.downgrade(),
        _watch: RefCell::new(None),
    }));
    unsafe { ffi::awra_widget_set_data(owner.as_ptr(), key.as_ptr(), dispatch as *mut c_void, Some(dispatch_free)) };
    let Some(menu) = menu else {
        apply(owner, &[]);
        return;
    };
    let rebuild = {
        let menu = menu.clone();
        let owner = owner.downgrade();
        move || {
            let Some(owner) = owner.upgrade() else { return };
            let d = unsafe { ffi::awra_widget_get_data(owner.as_ptr(), c"awra-rs-menu".as_ptr()) } as *mut MenuDispatch;
            if d.is_null() {
                return;
            }
            let mut keep = Vec::new();
            let (items, acts) = menu_items(&menu, &mut keep, d as *mut c_void);
            unsafe { *(*d).items.borrow_mut() = acts };
            apply(&owner, &items);
        }
    };
    rebuild();
    let watch: Rc<dyn Fn()> = Rc::new(rebuild);
    menu.watch(watch.clone());
    unsafe { *(*dispatch)._watch.borrow_mut() = Some(watch) };
}

awra_wrapper!(MenuButton, ["AwraMenuButton"]);
impl MenuButton {
    pub fn new() -> MenuButton {
        unsafe { MenuButton::from_full(ffi::awra_menu_button_new()) }
    }
    pub fn set_icon_name(&self, name: impl IntoOptStr) {
        let n = OptC::new(name);
        unsafe { ffi::awra_menu_button_set_icon_name(ptr!(self), n.ptr()) }
    }
    pub fn set_label(&self, label: impl IntoOptStr) {
        let l = label.into_opt_string();
        self.0.set_rust_data("menu-label", l.clone());
        let l = OptC::new(l);
        unsafe { ffi::awra_menu_button_set_label(ptr!(self), l.ptr()) }
    }
    pub fn label(&self) -> String {
        self.0.rust_data::<Option<String>>("menu-label").flatten().unwrap_or_default()
    }
    /// Custom content in place of the label and icon.
    pub fn set_child(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_button_set_child(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn set_popover(&self, popover: Option<&Popover>) {
        unsafe { ffi::awra_menu_button_set_popover(ptr!(self), popover.map_or(std::ptr::null_mut(), |p| p.0.as_ptr())) }
    }
    pub fn popover(&self) -> Option<Popover> {
        unsafe { Widget::from_none_opt(ffi::awra_menu_button_popover(ptr!(self))) }.map(Popover)
    }
    pub fn set_appearance(&self, a: ButtonAppearance) {
        unsafe { ffi::awra_menu_button_set_appearance(ptr!(self), a as _) }
    }
    pub fn set_menu_model(&self, menu: Option<&Menu>) {
        attach_menu(&self.0, menu, |w, items| unsafe {
            ffi::awra_menu_button_set_menu(w.as_ptr(), items.as_ptr(), items.len() as i32)
        });
    }
    pub fn popup(&self) {
        if let Some(p) = self.popover() {
            p.popup()
        }
    }
    pub fn popdown(&self) {
        if let Some(p) = self.popover() {
            p.popdown()
        }
    }
    pub fn set_direction(&self, _d: PositionType) {}
    pub fn set_always_show_arrow(&self, _a: bool) {}
}
impl Default for MenuButton {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(
    /// A floating panel anchored to a widget.
    Popover, ["AwraPopover"]
);
impl Popover {
    pub fn new() -> Popover {
        unsafe { Popover::from_full(ffi::awra_popover_new()) }
    }
    /// A menu popover for a model (context menus).
    pub fn from_model(menu: Option<&Menu>) -> Popover {
        let p = Popover::new();
        p.set_menu_model(menu);
        p
    }
    pub fn set_menu_model(&self, menu: Option<&Menu>) {
        attach_menu(&self.0, menu, |w, items| unsafe {
            let m = ffi::awra_menu_popover_new(items.as_ptr(), items.len() as i32);
            // the menu popover's content moves into this popover
            let content = ffi::awra_widget_child(m, 0);
            if !content.is_null() {
                ffi::awra_widget_ref(content);
                ffi::awra_container_remove(m, content);
                ffi::awra_popover_set_child(w.as_ptr(), content);
                ffi::awra_widget_unref(content);
            }
            ffi::awra_widget_unref(m);
        });
    }
    pub fn set_child(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_popover_set_child(ptr!(self), child.into_opt_ptr()) }
    }
    /// Anchors the popover to `parent` (GTK's set_parent on popovers).
    pub fn set_parent(&self, parent: &impl IsWidget) {
        self.0.set_rust_data("popover-anchor", parent.upcast_ref().downgrade());
        unsafe { ffi::awra_popover_set_anchor(ptr!(self), parent.widget_ptr()) }
    }
    pub fn set_anchor(&self, anchor: &impl IsWidget) {
        self.set_parent(anchor)
    }
    pub fn popup(&self) {
        unsafe { ffi::awra_popover_popup(ptr!(self)) }
    }
    /// Pops up at (x, y) of `relative_to`.
    pub fn popup_at(&self, relative_to: &impl IsWidget, x: f64, y: f64) {
        unsafe { ffi::awra_popover_popup_at(ptr!(self), relative_to.widget_ptr(), x, y) }
    }
    pub fn set_pointing_to(&self, rect: Option<&crate::Rectangle>) {
        self.0.set_rust_data("pointing-to", rect.copied());
    }
    /// Pops up at the point given by `set_pointing_to` (of the anchor).
    pub fn popup_pointing(&self, anchor: &impl IsWidget) {
        match self.0.rust_data::<Option<crate::Rectangle>>("pointing-to").flatten() {
            Some(r) => self.popup_at(anchor, r.x as f64 + r.width as f64 / 2.0, r.y as f64 + r.height as f64),
            None => self.popup(),
        }
    }
    pub fn popdown(&self) {
        unsafe { ffi::awra_popover_popdown(ptr!(self)) }
    }
    pub fn is_open(&self) -> bool {
        unsafe { ffi::awra_popover_visible(ptr!(self)) }
    }
    pub fn set_position(&self, p: PositionType) {
        unsafe { ffi::awra_popover_set_position(ptr!(self), p as i32) }
    }
    pub fn set_has_arrow(&self, _a: bool) {}
    pub fn set_autohide(&self, _a: bool) {}
    pub fn connect_closed<F: Fn(&Popover) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Popover, _>(sig::HIDE, move |w, _| f(w))
    }
    pub fn connect_show<F: Fn(&Popover) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Popover, _>(sig::SHOW, move |w, _| f(w))
    }
}
impl Default for Popover {
    fn default() -> Self {
        Self::new()
    }
}

/// A rectangle (popover anchors).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Rectangle {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}
impl Rectangle {
    pub fn new(x: i32, y: i32, width: i32, height: i32) -> Rectangle {
        Rectangle { x, y, width, height }
    }
}

// ===========================================================================
// Inputs

awra_wrapper!(
    /// GtkEntry (also the base of search entries, spin buttons, text views).
    Entry, ["AwraEntry"]
);
macro_rules! entry_ops {
    ($t:ty) => {
        impl $t {
            pub fn set_text(&self, text: &str) {
                let t = cstring(text);
                unsafe { ffi::awra_entry_set_text(ptr!(self), t.as_ptr()) }
            }
            pub fn text(&self) -> String {
                unsafe { from_cstr(ffi::awra_entry_text(ptr!(self))) }
            }
            pub fn set_placeholder_text(&self, text: impl IntoOptStr) {
                let t = OptC::new(text);
                unsafe { ffi::awra_entry_set_placeholder(ptr!(self), t.ptr()) }
            }
            pub fn set_placeholder(&self, text: impl IntoOptStr) {
                self.set_placeholder_text(text)
            }
            pub fn set_max_length(&self, n: i32) {
                unsafe { ffi::awra_entry_set_max_length(ptr!(self), n) }
            }
            pub fn select_region(&self, _start: i32, _end: i32) {
                unsafe { ffi::awra_entry_select_all(ptr!(self)) }
            }
            pub fn select_all(&self) {
                unsafe { ffi::awra_entry_select_all(ptr!(self)) }
            }
            pub fn set_position(&self, p: i32) {
                unsafe { ffi::awra_entry_set_position(ptr!(self), p) }
            }
            pub fn set_editable(&self, e: bool) {
                unsafe { ffi::awra_entry_set_editable(ptr!(self), e) }
            }
            pub fn set_width_chars(&self, n: i32) {
                let w = (n.max(1) * 8 + 24).max(60);
                self.0.set_width_request(w);
            }
            pub fn set_max_width_chars(&self, _n: i32) {}
            pub fn set_activates_default(&self, _a: bool) {}
            pub fn set_input_purpose<T>(&self, _p: T) {}
            /// The editable part (the entry itself).
            pub fn delegate(&self) -> $t {
                self.clone()
            }
            pub fn connect_changed<F: Fn(&$t) + 'static>(&self, f: F) -> SignalHandlerId {
                self.0.connect_typed::<$t, _>(sig::CHANGED, move |w, _| f(w))
            }
            pub fn connect_activate<F: Fn(&$t) + 'static>(&self, f: F) -> SignalHandlerId {
                self.0.connect_typed::<$t, _>(sig::ACTIVATE, move |w, _| f(w))
            }
        }
    };
}
impl Entry {
    pub fn new() -> Entry {
        unsafe { Entry::from_full(ffi::awra_entry_new()) }
    }
    pub fn set_icon_name(&self, name: Option<&str>) {
        let n = OptC::new(name);
        unsafe { ffi::awra_entry_set_icon_name(ptr!(self), n.ptr()) }
    }
    pub fn set_visibility(&self, v: bool) {
        unsafe { ffi::awra_entry_set_visibility(ptr!(self), v) }
    }
    pub fn set_accel_hint(&self, hint: Option<&str>) {
        let h = OptC::new(hint);
        unsafe { ffi::awra_entry_set_accel_hint(ptr!(self), h.ptr()) }
    }
    pub fn set_quiet(&self, quiet: bool) {
        unsafe { ffi::awra_entry_set_quiet(ptr!(self), quiet) }
    }
}
entry_ops!(Entry);
impl Default for Entry {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(SearchEntry, ["AwraSearchEntry"]);
impl SearchEntry {
    pub fn new() -> SearchEntry {
        unsafe { SearchEntry::from_full(ffi::awra_search_entry_new()) }
    }
    pub fn set_search_delay(&self, ms: u32) {
        unsafe { ffi::awra_search_entry_set_delay(ptr!(self), ms) }
    }
    pub fn set_accel_hint(&self, hint: Option<&str>) {
        let h = OptC::new(hint);
        unsafe { ffi::awra_entry_set_accel_hint(ptr!(self), h.ptr()) }
    }
    pub fn set_quiet(&self, quiet: bool) {
        unsafe { ffi::awra_entry_set_quiet(ptr!(self), quiet) }
    }
    pub fn connect_search_changed<F: Fn(&SearchEntry) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<SearchEntry, _>(sig::SEARCH_CHANGED, move |w, _| f(w))
    }
    pub fn connect_stop_search<F: Fn(&SearchEntry) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<SearchEntry, _>(sig::STOP_SEARCH, move |w, _| f(w))
    }
}
entry_ops!(SearchEntry);
impl Default for SearchEntry {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(
    /// GtkTextView (a multi-line entry; the text is the buffer).
    TextView, ["AwraTextView"]
);
impl TextView {
    pub fn new() -> TextView {
        unsafe { TextView::from_full(ffi::awra_text_view_new()) }
    }
    pub fn set_text(&self, text: &str) {
        let t = cstring(text);
        unsafe { ffi::awra_text_view_set_text(ptr!(self), t.as_ptr()) }
    }
    pub fn text(&self) -> String {
        unsafe { from_cstr(ffi::awra_text_view_text(ptr!(self))) }
    }
    pub fn set_placeholder(&self, text: impl IntoOptStr) {
        let t = OptC::new(text);
        unsafe { ffi::awra_text_view_set_placeholder(ptr!(self), t.ptr()) }
    }
    pub fn set_editable(&self, e: bool) {
        unsafe { ffi::awra_text_view_set_editable(ptr!(self), e) }
    }
    pub fn set_wrap_mode(&self, mode: WrapMode) {
        unsafe { ffi::awra_text_view_set_wrap(ptr!(self), wrap_c(mode)) }
    }
    pub fn set_accepts_tab(&self, _a: bool) {}
    pub fn set_top_margin(&self, _m: i32) {}
    pub fn set_bottom_margin(&self, _m: i32) {}
    pub fn set_left_margin(&self, _m: i32) {}
    pub fn set_right_margin(&self, _m: i32) {}
    pub fn connect_changed<F: Fn(&TextView) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<TextView, _>(sig::CHANGED, move |w, _| f(w))
    }
}
impl Default for TextView {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(Slider, ["AwraSlider"]);
impl Slider {
    /// Awra 1: the inner GTK widget; here the widget itself.
    pub fn delegate(&self) -> Slider {
        self.clone()
    }
    pub fn with_range(orientation: Orientation, min: f64, max: f64, step: f64) -> Slider {
        let s = unsafe { Slider::from_full(ffi::awra_slider_new(min, max, step)) };
        unsafe { ffi::awra_slider_set_orientation(ptr!(s), orientation as _) };
        s
    }
    pub fn set_value(&self, v: f64) {
        unsafe { ffi::awra_slider_set_value(ptr!(self), v) }
    }
    pub fn value(&self) -> f64 {
        unsafe { ffi::awra_slider_value(ptr!(self)) }
    }
    pub fn set_range(&self, min: f64, max: f64) {
        unsafe { ffi::awra_slider_set_range(ptr!(self), min, max) }
    }
    pub fn set_draw_value(&self, _d: bool) {}
    pub fn connect_value_changed<F: Fn(&Slider) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Slider, _>(sig::VALUE_CHANGED, move |w, _| f(w))
    }
}

/// GtkScale.
pub type Scale = Slider;

awra_wrapper!(SpinButton, ["AwraSpinButton"]);
impl SpinButton {
    /// Awra 1: the inner GTK widget; here the widget itself.
    pub fn delegate(&self) -> SpinButton {
        self.clone()
    }
    pub fn with_range(min: f64, max: f64, step: f64) -> SpinButton {
        unsafe { SpinButton::from_full(ffi::awra_spin_button_new(min, max, step)) }
    }
    pub fn set_value(&self, v: f64) {
        unsafe { ffi::awra_spin_button_set_value(ptr!(self), v) }
    }
    pub fn value(&self) -> f64 {
        unsafe { ffi::awra_spin_button_value(ptr!(self)) }
    }
    pub fn value_as_int(&self) -> i32 {
        self.value().round() as i32
    }
    pub fn set_digits(&self, d: u32) {
        unsafe { ffi::awra_spin_button_set_digits(ptr!(self), d as i32) }
    }
    pub fn set_range(&self, min: f64, max: f64) {
        unsafe { ffi::awra_spin_button_set_range(ptr!(self), min, max) }
    }
    pub fn set_numeric(&self, _n: bool) {}
    pub fn connect_value_changed<F: Fn(&SpinButton) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<SpinButton, _>(sig::VALUE_CHANGED, move |w, _| f(w))
    }
}

awra_wrapper!(Dropdown, ["AwraDropdown"]);
impl Dropdown {
    /// Awra 1: the inner GTK widget; here the widget itself.
    pub fn delegate(&self) -> Dropdown {
        self.clone()
    }
    pub fn from_strings(items: &[&str]) -> Dropdown {
        let cs: Vec<_> = items.iter().map(|s| cstring(s)).collect();
        let ps: Vec<*const c_char> = cs.iter().map(|c| c.as_ptr()).collect();
        unsafe { Dropdown::from_full(ffi::awra_dropdown_new(ps.as_ptr(), ps.len() as i32)) }
    }
    pub fn set_strings(&self, items: &[&str]) {
        let cs: Vec<_> = items.iter().map(|s| cstring(s)).collect();
        let ps: Vec<*const c_char> = cs.iter().map(|c| c.as_ptr()).collect();
        unsafe { ffi::awra_dropdown_set_items(ptr!(self), ps.as_ptr(), ps.len() as i32) }
    }
    /// gtk's StringList model.
    pub fn set_model(&self, model: Option<&StringList>) {
        let items: Vec<String> = model.map(|m| m.0.borrow().clone()).unwrap_or_default();
        let refs: Vec<&str> = items.iter().map(String::as_str).collect();
        self.set_strings(&refs)
    }
    pub fn set_selected(&self, index: u32) {
        let i = if index == u32::MAX { -1 } else { index as i32 };
        unsafe { ffi::awra_dropdown_set_selected(ptr!(self), i) }
    }
    /// The selected index (u32::MAX: none, GTK_INVALID_LIST_POSITION).
    pub fn selected(&self) -> u32 {
        let i = unsafe { ffi::awra_dropdown_selected(ptr!(self)) };
        if i < 0 {
            u32::MAX
        } else {
            i as u32
        }
    }
    pub fn selected_text(&self) -> Option<String> {
        unsafe { from_cstr_opt(ffi::awra_dropdown_selected_text(ptr!(self))) }
    }
    pub fn set_enable_search(&self, s: bool) {
        unsafe { ffi::awra_dropdown_set_enable_search(ptr!(self), s) }
    }
    pub fn connect_selected_notify<F: Fn(&Dropdown) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Dropdown, _>(sig::SELECTED, move |w, _| f(w))
    }
}

/// GtkStringList (dropdown models).
#[derive(Clone, Default)]
pub struct StringList(Rc<RefCell<Vec<String>>>);
impl StringList {
    pub fn new(items: &[&str]) -> StringList {
        StringList(Rc::new(RefCell::new(items.iter().map(|s| s.to_string()).collect())))
    }
    pub fn append(&self, s: &str) {
        self.0.borrow_mut().push(s.to_owned())
    }
    pub fn n_items(&self) -> u32 {
        self.0.borrow().len() as u32
    }
    pub fn string(&self, i: u32) -> Option<String> {
        self.0.borrow().get(i as usize).cloned()
    }
}

awra_wrapper!(VolumeControl, ["AwraVolumeControl"]);
impl VolumeControl {
    /// Awra 1: the inner GTK widget; here the widget itself.
    pub fn delegate(&self) -> VolumeControl {
        self.clone()
    }
    pub fn new() -> VolumeControl {
        unsafe { VolumeControl::from_full(ffi::awra_volume_control_new()) }
    }
    /// 0..1
    pub fn set_value(&self, v: f64) {
        unsafe { ffi::awra_volume_control_set_value(ptr!(self), v) }
    }
    pub fn value(&self) -> f64 {
        unsafe { ffi::awra_volume_control_value(ptr!(self)) }
    }
    pub fn set_volume(&self, v: f64) {
        self.set_value(v)
    }
    pub fn volume(&self) -> f64 {
        self.value()
    }
    pub fn set_muted(&self, m: bool) {
        unsafe { ffi::awra_volume_control_set_muted(ptr!(self), m) }
    }
    pub fn is_muted(&self) -> bool {
        unsafe { ffi::awra_volume_control_muted(ptr!(self)) }
    }
    pub fn connect_value_changed<F: Fn(&VolumeControl) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<VolumeControl, _>(sig::VALUE_CHANGED, move |w, _| f(w))
    }
    pub fn connect_volume_changed<F: Fn(&VolumeControl, f64) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<VolumeControl, _>(sig::VALUE_CHANGED, move |w, a| f(w, a.value()))
    }
}
impl Default for VolumeControl {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(ShortcutCapture, ["AwraShortcutCapture"]);
impl ShortcutCapture {
    /// Awra 1: the inner GTK widget; here the widget itself.
    pub fn delegate(&self) -> ShortcutCapture {
        self.clone()
    }
    pub fn new() -> ShortcutCapture {
        unsafe { ShortcutCapture::from_full(ffi::awra_shortcut_capture_new()) }
    }
    pub fn set_accelerator(&self, accel: Option<&str>) {
        let a = OptC::new(accel);
        unsafe { ffi::awra_shortcut_capture_set_accelerator(ptr!(self), a.ptr()) }
    }
    pub fn accelerator(&self) -> Option<String> {
        unsafe { from_cstr_opt(ffi::awra_shortcut_capture_accelerator(ptr!(self))) }
    }
    pub fn set_placeholder(&self, text: impl IntoOptStr) {
        let t = OptC::new(text);
        unsafe { ffi::awra_shortcut_capture_set_placeholder(ptr!(self), t.ptr()) }
    }
    pub fn start(&self) {
        unsafe { ffi::awra_shortcut_capture_start(ptr!(self)) }
    }
    pub fn is_listening(&self) -> bool {
        unsafe { ffi::awra_shortcut_capture_capturing(ptr!(self)) }
    }
    pub fn set_allow_chords(&self, allow: bool) {
        unsafe { ffi::awra_shortcut_capture_set_allow_chords(ptr!(self), allow) }
    }
    pub fn clear(&self) {
        unsafe { ffi::awra_shortcut_capture_clear(ptr!(self)) }
    }
    /// Physical keys of the capture ("ControlLeft", "KeyA").
    pub fn codes(&self) -> Vec<String> {
        let mut p: *const *const c_char = std::ptr::null();
        let n = unsafe { ffi::awra_shortcut_capture_codes(ptr!(self), &mut p) };
        (0..n.max(0) as usize).map(|i| unsafe { from_cstr(*p.add(i)) }).collect()
    }
    pub fn set_display_labels(&self, labels: &[&str]) {
        let cs: Vec<_> = labels.iter().map(|s| cstring(s)).collect();
        let ps: Vec<*const c_char> = cs.iter().map(|c| c.as_ptr()).collect();
        unsafe { ffi::awra_shortcut_capture_set_display_labels(ptr!(self), ps.as_ptr(), ps.len() as i32) }
    }
    pub fn connect_captured<F: Fn(&ShortcutCapture) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<ShortcutCapture, _>(sig::CAPTURED, move |w, _| f(w))
    }
    pub fn connect_listening_notify<F: Fn(&ShortcutCapture) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<ShortcutCapture, _>(sig::NOTIFY, move |w, a| {
            if a.text().as_deref() == Some("listening") {
                f(w)
            }
        })
    }
}
impl Default for ShortcutCapture {
    fn default() -> Self {
        Self::new()
    }
}

// ===========================================================================
// Composition

awra_wrapper!(Card, ["AwraCard"]);
impl Card {
    pub fn new() -> Card {
        unsafe { Card::from_full(ffi::awra_card_new()) }
    }
    pub fn set_child(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_card_set_child(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn set_appearance(&self, a: CardAppearance) {
        unsafe { ffi::awra_card_set_appearance(ptr!(self), a as _) }
    }
    pub fn set_activatable(&self, a: bool) {
        unsafe { ffi::awra_card_set_activatable(ptr!(self), a) }
    }
    pub fn set_content_inset(&self, s: Spacing) {
        unsafe { ffi::awra_card_set_padding(ptr!(self), s as _) }
    }
    pub fn connect_activated<F: Fn(&Card) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Card, _>(sig::CLICKED, move |w, _| f(w))
    }
    /// Cards that hold a box: append to it (Awra 1 cards were boxes).
    pub fn append(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_container_add(ptr!(self), child.widget_ptr()) }
    }
}
impl Default for Card {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(Surface, ["AwraSurface"]);
impl Surface {
    pub fn new(material: Material) -> Surface {
        unsafe { Surface::from_full(ffi::awra_surface_new(material as _)) }
    }
    pub fn set_child(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_surface_set_child(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn set_material(&self, m: Material) {
        unsafe { ffi::awra_surface_set_material(ptr!(self), m as _) }
    }
    pub fn set_elevation(&self, e: Elevation) {
        unsafe { ffi::awra_surface_set_elevation(ptr!(self), e as _) }
    }
    pub fn set_radius(&self, r: f64) {
        unsafe { ffi::awra_surface_set_radius(ptr!(self), r) }
    }
    pub fn set_padding(&self, top: i32, end: i32, bottom: i32, start: i32) {
        unsafe { ffi::awra_surface_set_padding(ptr!(self), top, end, bottom, start) }
    }
}

awra_wrapper!(Page, ["AwraPage"]);
impl Page {
    pub fn new() -> Page {
        unsafe { Page::from_full(ffi::awra_page_new()) }
    }
    pub fn set_child(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_page_set_child(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn set_content(&self, child: impl IntoOptWidget) {
        self.set_child(child)
    }
    pub fn set_header(&self, header: impl IntoOptWidget) {
        unsafe { ffi::awra_page_set_header(ptr!(self), header.into_opt_ptr()) }
    }
    pub fn set_scrollable(&self, s: bool) {
        unsafe { ffi::awra_page_set_scrollable(ptr!(self), s) }
    }
    pub fn set_max_content_width(&self, w: i32) {
        unsafe { ffi::awra_page_set_max_content_width(ptr!(self), w) }
    }
}
impl Default for Page {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(PageHeader, ["AwraBox"]);
impl PageHeader {
    pub fn new() -> PageHeader {
        unsafe { PageHeader::from_full(ffi::awra_page_header_new(std::ptr::null(), std::ptr::null())) }
    }
    pub fn set_title(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_page_header_set_title(ptr!(self), t.ptr()) }
    }
    pub fn set_subtitle(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_page_header_set_subtitle(ptr!(self), t.ptr()) }
    }
    pub fn set_description(&self, t: impl IntoOptStr) {
        self.set_subtitle(t)
    }
    pub fn add_action(&self, w: &impl IsWidget) {
        unsafe { ffi::awra_page_header_add_action(ptr!(self), w.widget_ptr()) }
    }
    pub fn set_actions(&self, w: impl IntoOptWidget) {
        let p = w.into_opt_ptr();
        if !p.is_null() {
            unsafe { ffi::awra_page_header_add_action(ptr!(self), p) }
        }
    }
}
impl Default for PageHeader {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(Section, ["AwraBox"]);
impl Section {
    pub fn new() -> Section {
        unsafe { Section::from_full(ffi::awra_section_new(std::ptr::null())) }
    }
    pub fn set_title(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_section_set_title(ptr!(self), t.ptr()) }
    }
    pub fn set_description(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_section_set_description(ptr!(self), t.ptr()) }
    }
    pub fn append(&self, w: &impl IsWidget) {
        unsafe { ffi::awra_section_append(ptr!(self), w.widget_ptr()) }
    }
    pub fn set_child(&self, w: impl IntoOptWidget) {
        let p = w.into_opt_ptr();
        if !p.is_null() {
            unsafe { ffi::awra_section_append(ptr!(self), p) }
        }
    }
    pub fn set_content(&self, w: impl IntoOptWidget) {
        self.set_child(w)
    }
    pub fn set_header_suffix(&self, w: impl IntoOptWidget) {
        unsafe { ffi::awra_section_set_header_suffix(ptr!(self), w.into_opt_ptr()) }
    }
}
impl Default for Section {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(Form, ["AwraBox"]);
impl Form {
    pub fn new() -> Form {
        unsafe { Form::from_full(ffi::awra_form_new()) }
    }
    pub fn append(&self, row: &impl IsWidget) {
        unsafe { ffi::awra_form_append(ptr!(self), row.widget_ptr()) }
    }
}
impl Form {
    pub fn remove(&self, child: &impl IsWidget) {
        unsafe { ffi::awra_container_remove(ptr!(self), child.widget_ptr()) }
    }
    pub fn remove_all(&self) {
        unsafe { ffi::awra_container_remove_all(ptr!(self)) }
    }
}
impl Default for Form {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(FormRow, ["AwraFormRow"]);
impl FormRow {
    pub fn new() -> FormRow {
        unsafe { FormRow::from_full(ffi::awra_form_row_new(std::ptr::null(), std::ptr::null())) }
    }
    pub fn set_title(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_form_row_set_title(ptr!(self), t.ptr()) }
    }
    pub fn set_label(&self, t: impl IntoOptStr) {
        self.set_title(t)
    }
    pub fn set_subtitle(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_form_row_set_subtitle(ptr!(self), t.ptr()) }
    }
    pub fn set_description(&self, t: impl IntoOptStr) {
        self.set_subtitle(t)
    }
    pub fn set_help_text(&self, t: impl IntoOptStr) {
        self.set_subtitle(t)
    }
    pub fn set_control(&self, w: impl IntoOptWidget) {
        unsafe { ffi::awra_form_row_set_control(ptr!(self), w.into_opt_ptr()) }
    }
    pub fn set_vertical(&self, v: bool) {
        unsafe { ffi::awra_form_row_set_vertical(ptr!(self), v) }
    }
}
impl Default for FormRow {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(ActionRow, ["AwraActionRow"]);
impl ActionRow {
    pub fn new(title: &str, subtitle: &str) -> ActionRow {
        let (t, s) = (cstring(title), OptC::new((!subtitle.is_empty()).then_some(subtitle)));
        unsafe { ActionRow::from_full(ffi::awra_action_row_new(t.as_ptr(), s.ptr())) }
    }
    pub fn set_title(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_action_row_set_title(ptr!(self), t.ptr()) }
    }
    pub fn set_subtitle(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_action_row_set_subtitle(ptr!(self), t.ptr()) }
    }
    pub fn set_description(&self, t: impl IntoOptStr) {
        self.set_subtitle(t)
    }
    pub fn set_icon_name(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_action_row_set_icon_name(ptr!(self), t.ptr()) }
    }
    pub fn set_prefix(&self, w: impl IntoOptWidget) {
        unsafe { ffi::awra_action_row_set_prefix(ptr!(self), w.into_opt_ptr()) }
    }
    pub fn add_suffix(&self, w: &impl IsWidget) {
        unsafe { ffi::awra_action_row_add_suffix(ptr!(self), w.widget_ptr()) }
    }
    pub fn set_suffix(&self, w: impl IntoOptWidget) {
        let p = w.into_opt_ptr();
        if !p.is_null() {
            unsafe { ffi::awra_action_row_add_suffix(ptr!(self), p) }
        }
    }
    pub fn set_activatable(&self, a: bool) {
        unsafe { ffi::awra_action_row_set_activatable(ptr!(self), a) }
    }
    pub fn connect_activated<F: Fn(&ActionRow) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<ActionRow, _>(sig::CLICKED, move |w, _| f(w))
    }
}

awra_wrapper!(Badge, ["AwraBadge"]);
impl Badge {
    pub fn new(label: &str) -> Badge {
        let l = cstring(label);
        // Awra 1's default appearance: accent
        unsafe { Badge::from_full(ffi::awra_badge_new(l.as_ptr(), 1)) }
    }
    pub fn set_label(&self, label: &str) {
        let l = cstring(label);
        unsafe { ffi::awra_badge_set_label(ptr!(self), l.as_ptr()) }
    }
    pub fn set_appearance(&self, a: BadgeAppearance) {
        unsafe { ffi::awra_badge_set_appearance(ptr!(self), a as _) }
    }
    pub fn set_icon_name(&self, n: impl IntoOptStr) {
        let n = OptC::new(n);
        unsafe { ffi::awra_badge_set_icon_name(ptr!(self), n.ptr()) }
    }
    pub fn set_custom_color(&self, c: &RGBA) {
        with_rgba!(Some(c), |p| unsafe { ffi::awra_badge_set_color(ptr!(self), p) })
    }
    pub fn unset_custom_color(&self) {
        unsafe { ffi::awra_badge_set_color(ptr!(self), std::ptr::null()) }
    }
    pub fn set_text(&self, label: &str) {
        self.set_label(label)
    }
    /// Awra 1 badges with an action were clickable chips.
    pub fn connect_activated<F: Fn(&Badge, ModifierType) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Badge, _>(sig::CLICKED, move |w, a| f(w, a.mods()))
    }
}

awra_wrapper!(StatusBanner, ["AwraStatusBanner"]);
impl StatusBanner {
    pub fn new() -> StatusBanner {
        unsafe { StatusBanner::from_full(ffi::awra_status_banner_new(std::ptr::null(), std::ptr::null(), 0)) }
    }
    pub fn set_title(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_status_banner_set_title(ptr!(self), t.ptr()) }
    }
    pub fn set_description(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_status_banner_set_description(ptr!(self), t.ptr()) }
    }
    pub fn set_appearance(&self, a: StatusAppearance) {
        unsafe { ffi::awra_status_banner_set_appearance(ptr!(self), a as _) }
    }
    pub fn add_action(&self, w: &impl IsWidget) {
        unsafe { ffi::awra_status_banner_add_action(ptr!(self), w.widget_ptr()) }
    }
    pub fn set_action(&self, w: impl IntoOptWidget) {
        let p = w.into_opt_ptr();
        if !p.is_null() {
            unsafe { ffi::awra_status_banner_add_action(ptr!(self), p) }
        }
    }
    pub fn set_dismissible(&self, d: bool) {
        unsafe { ffi::awra_status_banner_set_dismissible(ptr!(self), d) }
    }
    pub fn set_revealed(&self, r: bool) {
        self.0.set_visible(r)
    }
}
impl Default for StatusBanner {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(EmptyState, ["AwraBox"]);
impl EmptyState {
    pub fn new() -> EmptyState {
        unsafe { EmptyState::from_full(ffi::awra_empty_state_new(std::ptr::null(), std::ptr::null(), std::ptr::null())) }
    }
    pub fn set_title(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_empty_state_set_title(ptr!(self), t.ptr()) }
    }
    pub fn set_description(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_empty_state_set_description(ptr!(self), t.ptr()) }
    }
    pub fn set_icon_name(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_empty_state_set_icon_name(ptr!(self), t.ptr()) }
    }
    pub fn add_action(&self, w: &impl IsWidget) {
        unsafe { ffi::awra_empty_state_add_action(ptr!(self), w.widget_ptr()) }
    }
    pub fn set_action(&self, w: impl IntoOptWidget) {
        let p = w.into_opt_ptr();
        if !p.is_null() {
            unsafe { ffi::awra_empty_state_add_action(ptr!(self), p) }
        }
    }
}
impl Default for EmptyState {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(LoadingState, ["AwraBox"]);
impl LoadingState {
    /// The spinner runs while shown.
    pub fn set_active(&self, _active: bool) {}
    pub fn new() -> LoadingState {
        unsafe { LoadingState::from_full(ffi::awra_loading_state_new(std::ptr::null(), std::ptr::null())) }
    }
    pub fn set_title(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_loading_state_set_title(ptr!(self), t.ptr()) }
    }
    pub fn set_description(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_loading_state_set_description(ptr!(self), t.ptr()) }
    }
}
impl Default for LoadingState {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(Spinner, ["AwraSpinner"]);
impl Spinner {
    pub fn new() -> Spinner {
        unsafe { Spinner::from_full(ffi::awra_spinner_new()) }
    }
    pub fn set_spinning(&self, s: bool) {
        unsafe { ffi::awra_spinner_set_spinning(ptr!(self), s) }
    }
    pub fn start(&self) {
        self.set_spinning(true)
    }
    pub fn stop(&self) {
        self.set_spinning(false)
    }
}
impl Default for Spinner {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(ProgressBar, ["AwraProgressBar"]);
impl ProgressBar {
    /// Awra 1: the inner GTK widget; here the widget itself.
    pub fn delegate(&self) -> ProgressBar {
        self.clone()
    }
    pub fn new() -> ProgressBar {
        unsafe { ProgressBar::from_full(ffi::awra_progress_bar_new()) }
    }
    pub fn set_fraction(&self, f: f64) {
        unsafe { ffi::awra_progress_bar_set_fraction(ptr!(self), f) }
    }
    pub fn fraction(&self) -> f64 {
        unsafe { ffi::awra_progress_bar_fraction(ptr!(self)) }
    }
    pub fn pulse(&self) {
        unsafe { ffi::awra_progress_bar_pulse(ptr!(self)) }
    }
    pub fn set_text(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_progress_bar_set_text(ptr!(self), t.ptr()) }
    }
    pub fn set_show_text(&self, show: bool) {
        if !show {
            self.set_text(None::<&str>)
        }
    }
}
impl Default for ProgressBar {
    fn default() -> Self {
        Self::new()
    }
}

// ===========================================================================
// Navigation and chrome

awra_wrapper!(Header, ["AwraHeader"]);
impl Header {
    pub fn new() -> Header {
        unsafe { Header::from_full(ffi::awra_header_new()) }
    }
    pub fn set_title(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_header_set_title(ptr!(self), t.ptr()) }
    }
    pub fn set_subtitle(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_header_set_subtitle(ptr!(self), t.ptr()) }
    }
    pub fn pack_start(&self, w: &impl IsWidget) {
        unsafe { ffi::awra_header_pack_start(ptr!(self), w.widget_ptr()) }
    }
    pub fn pack_end(&self, w: &impl IsWidget) {
        unsafe { ffi::awra_header_pack_end(ptr!(self), w.widget_ptr()) }
    }
    pub fn set_start_widget(&self, w: impl IntoOptWidget) {
        let p = w.into_opt_ptr();
        if !p.is_null() {
            unsafe { ffi::awra_header_pack_start(ptr!(self), p) }
        }
    }
    pub fn set_end_widget(&self, w: impl IntoOptWidget) {
        let p = w.into_opt_ptr();
        if !p.is_null() {
            unsafe { ffi::awra_header_pack_end(ptr!(self), p) }
        }
    }
    pub fn set_center_widget(&self, w: impl IntoOptWidget) {
        unsafe { ffi::awra_header_set_center(ptr!(self), w.into_opt_ptr()) }
    }
    pub fn set_show_window_controls(&self, s: bool) {
        unsafe { ffi::awra_header_set_show_controls(ptr!(self), s) }
    }
    pub fn set_blend_with_window(&self, b: bool) {
        unsafe { ffi::awra_header_set_blend_with_window(ptr!(self), b) }
    }
    pub fn set_drag_enabled(&self, d: bool) {
        unsafe { ffi::awra_header_set_drag_enabled(ptr!(self), d) }
    }
    pub fn set_compact(&self, c: bool) {
        unsafe { ffi::awra_header_set_compact(ptr!(self), c) }
    }
}
impl Default for Header {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(Sidebar, ["AwraSurface"]);
impl Sidebar {
    pub fn new() -> Sidebar {
        unsafe { Sidebar::from_full(ffi::awra_sidebar_new()) }
    }
    pub fn set_child(&self, w: impl IntoOptWidget) {
        unsafe { ffi::awra_sidebar_set_child(ptr!(self), w.into_opt_ptr()) }
    }
    pub fn set_content(&self, w: impl IntoOptWidget) {
        self.set_child(w)
    }
    pub fn append(&self, w: &impl IsWidget) {
        unsafe { ffi::awra_sidebar_append(ptr!(self), w.widget_ptr()) }
    }
    pub fn append_heading(&self, title: &str) {
        let t = cstring(title);
        unsafe { ffi::awra_sidebar_append_heading(ptr!(self), t.as_ptr()) }
    }
    pub fn set_footer(&self, w: impl IntoOptWidget) {
        unsafe { ffi::awra_sidebar_set_footer(ptr!(self), w.into_opt_ptr()) }
    }
    pub fn set_inset(&self, s: Spacing) {
        unsafe { ffi::awra_sidebar_set_inset(ptr!(self), s as _) }
    }
}
impl Default for Sidebar {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(NavigationItem, ["AwraNavigationItem"]);
impl NavigationItem {
    pub fn with_label(label: &str) -> NavigationItem {
        let l = cstring(label);
        unsafe { NavigationItem::from_full(ffi::awra_navigation_item_new(l.as_ptr(), std::ptr::null())) }
    }
    pub fn new(label: &str, icon: Option<&str>) -> NavigationItem {
        let (l, i) = (cstring(label), OptC::new(icon));
        unsafe { NavigationItem::from_full(ffi::awra_navigation_item_new(l.as_ptr(), i.ptr())) }
    }
    pub fn set_label(&self, l: &str) {
        let l = cstring(l);
        unsafe { ffi::awra_navigation_item_set_label(ptr!(self), l.as_ptr()) }
    }
    pub fn set_icon_name(&self, n: impl IntoOptStr) {
        let n = OptC::new(n);
        unsafe { ffi::awra_navigation_item_set_icon_name(ptr!(self), n.ptr()) }
    }
    pub fn set_selected(&self, s: bool) {
        unsafe { ffi::awra_navigation_item_set_selected(ptr!(self), s) }
    }
    pub fn is_selected(&self) -> bool {
        unsafe { ffi::awra_navigation_item_selected(ptr!(self)) }
    }
    pub fn set_badge(&self, b: impl IntoOptStr) {
        let b = OptC::new(b);
        unsafe { ffi::awra_navigation_item_set_badge(ptr!(self), b.ptr()) }
    }
    pub fn set_group(&self, g: &NavigationItem) {
        unsafe { ffi::awra_navigation_item_set_group(ptr!(self), g.0.as_ptr()) }
    }
    pub fn set_suffix(&self, w: impl IntoOptWidget) {
        unsafe { ffi::awra_container_add(ptr!(self), w.into_opt_ptr()) }
    }
    pub fn connect_selected_notify<F: Fn(&NavigationItem) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<NavigationItem, _>(sig::TOGGLED, move |w, _| f(w))
    }
    pub fn connect_activated<F: Fn(&NavigationItem) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<NavigationItem, _>(sig::CLICKED, move |w, _| f(w))
    }
    pub fn connect_clicked<F: Fn(&NavigationItem) + 'static>(&self, f: F) -> SignalHandlerId {
        self.connect_activated(f)
    }
}
actionable!(NavigationItem);

awra_wrapper!(SplitView, ["AwraSplitView"]);
impl SplitView {
    pub fn new() -> SplitView {
        unsafe { SplitView::from_full(ffi::awra_split_view_new()) }
    }
    pub fn set_sidebar(&self, w: impl IntoOptWidget) {
        unsafe { ffi::awra_split_view_set_sidebar(ptr!(self), w.into_opt_ptr()) }
    }
    pub fn set_content(&self, w: impl IntoOptWidget) {
        unsafe { ffi::awra_split_view_set_content(ptr!(self), w.into_opt_ptr()) }
    }
    pub fn set_sidebar_width(&self, w: i32) {
        unsafe { ffi::awra_split_view_set_sidebar_width(ptr!(self), w) }
    }
    pub fn set_show_sidebar(&self, s: bool) {
        unsafe { ffi::awra_split_view_set_show_sidebar(ptr!(self), s) }
    }
    pub fn set_end_pane(&self, w: impl IntoOptWidget) {
        unsafe { ffi::awra_split_view_set_end_pane(ptr!(self), w.into_opt_ptr()) }
    }
    pub fn set_end_pane_width(&self, w: i32) {
        unsafe { ffi::awra_split_view_set_end_pane_width(ptr!(self), w) }
    }
}
impl Default for SplitView {
    fn default() -> Self {
        Self::new()
    }
}

// ===========================================================================
// Overlays

awra_wrapper!(ToastOverlay, ["AwraToastOverlay"]);
type ToastAction = std::boxed::Box<dyn Fn()>;
unsafe extern "C" fn toast_action(_w: *mut ffi::AwraWidget, _a: *const ffi::AwraArgs, data: *mut c_void) {
    let f = &*(data as *const ToastAction);
    f();
}
impl ToastOverlay {
    pub fn new() -> ToastOverlay {
        unsafe { ToastOverlay::from_full(ffi::awra_toast_overlay_new()) }
    }
    pub fn set_child(&self, w: impl IntoOptWidget) {
        unsafe { ffi::awra_toast_overlay_set_child(ptr!(self), w.into_opt_ptr()) }
    }
    /// A toast; `timeout` in ms (0: until dismissed).
    pub fn show_message(&self, message: &str, timeout: u32) -> u32 {
        let m = cstring(message);
        unsafe { ffi::awra_toast_overlay_show(ptr!(self), m.as_ptr(), timeout) }
    }
    /// A toast whose button activates `action` ("win.undo").
    pub fn show_with_action(&self, message: &str, label: &str, action: Option<&str>, timeout: u32) -> u32 {
        let owner = self.0.downgrade();
        let action = action.map(str::to_owned);
        self.show_with_callback(message, label, timeout, move || {
            if let (Some(owner), Some(action)) = (owner.upgrade(), action.as_deref()) {
                crate::gio::activate_action(&owner, action, None);
            }
        })
    }
    /// A toast with a button running `f`.
    pub fn show_with_callback<F: Fn() + 'static>(&self, message: &str, action: &str, timeout: u32, f: F) -> u32 {
        let (m, a) = (cstring(message), cstring(action));
        // the closure lives with the overlay (toasts are short lived)
        let data: std::boxed::Box<ToastAction> = std::boxed::Box::new(std::boxed::Box::new(f));
        let raw = std::boxed::Box::into_raw(data);
        let id = unsafe {
            ffi::awra_toast_overlay_show_with_action(ptr!(self), m.as_ptr(), a.as_ptr(), Some(toast_action), raw as *mut c_void, timeout)
        };
        let keep: Rc<ToastHolder> = Rc::new(ToastHolder(raw));
        self.0.set_rust_data(&format!("toast-{id}"), keep);
        id
    }
    pub fn dismiss(&self, id: u32) {
        unsafe { ffi::awra_toast_overlay_dismiss(ptr!(self), id) }
    }
}
struct ToastHolder(*mut ToastAction);
impl Drop for ToastHolder {
    fn drop(&mut self) {
        unsafe { drop(std::boxed::Box::from_raw(self.0)) }
    }
}
impl Default for ToastOverlay {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(CommandPalette, ["AwraCommandPalette"]);
impl CommandPalette {
    /// A palette shown over `window`.
    pub fn new(window: Option<&crate::Window>) -> CommandPalette {
        let p = unsafe { CommandPalette::from_full(ffi::awra_command_palette_new()) };
        if let Some(w) = window {
            p.0.set_rust_data("palette-window", w.downgrade());
        }
        p.0.connect_raw(sig::ACTIVATE, |w, a| {
            let acts = w.rust_data::<Rc<Vec<ActivateItem>>>("palette-items").unwrap_or_default();
            if let Some((Some(action), target)) = acts.get(a.index().max(0) as usize).cloned() {
                let window = w
                    .rust_data::<crate::WeakRef<crate::Window>>("palette-window")
                    .and_then(|w| w.upgrade())
                    .map(Widget::from)
                    .unwrap_or_else(|| w.clone());
                crate::gio::activate_action(&window, &action, target.as_ref());
            }
        });
        p
    }
    pub fn set_title(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_command_palette_set_title(ptr!(self), t.ptr()) }
    }
    pub fn set_placeholder_text(&self, t: &str) {
        let t = cstring(t);
        unsafe { ffi::awra_command_palette_set_placeholder(ptr!(self), t.as_ptr()) }
    }
    pub fn set_empty_text(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_command_palette_set_empty_text(ptr!(self), t.ptr()) }
    }
    pub fn set_menu_model(&self, menu: Option<&Menu>) {
        let mut keep: Vec<std::ffi::CString> = Vec::new();
        let mut cmds = Vec::new();
        let mut acts = Vec::new();
        let mut s = |v: Option<&str>| -> *const c_char {
            match v {
                Some(v) => {
                    keep.push(cstring(v));
                    keep.last().unwrap().as_ptr()
                }
                None => std::ptr::null(),
            }
        };
        if let Some(menu) = menu {
            for e in menu.flatten() {
                if let FlatEntry::Item(d, section) = e {
                    cmds.push(ffi::AwraCommand {
                        title: s(d.label.as_deref()),
                        subtitle: s(d.subtitle.as_deref()),
                        icon_name: s(d.icon.as_deref()),
                        accel: s(d.accel.as_deref().map(crate::window::accel_label).as_deref()),
                        section: s(section.as_deref()),
                    });
                    acts.push((d.action.clone(), d.target.clone()));
                }
            }
        }
        unsafe { ffi::awra_command_palette_set_commands(ptr!(self), cmds.as_ptr(), cmds.len() as i32) };
        self.0.set_rust_data("palette-items", Rc::new(acts));
    }
    pub fn present(&self) {
        let window = self
            .0
            .rust_data::<crate::WeakRef<crate::Window>>("palette-window")
            .and_then(|w| w.upgrade());
        let wp = window.as_ref().map_or(std::ptr::null_mut(), |w| w.as_ptr());
        unsafe { ffi::awra_command_palette_present(ptr!(self), wp) }
    }
    pub fn close(&self) {
        unsafe { ffi::awra_command_palette_close(ptr!(self)) }
    }
}

awra_wrapper!(
    /// A modal dialog window.
    Dialog, ["AwraDialog"]
);
impl Dialog {
    pub fn new(parent: Option<&crate::Window>) -> Dialog {
        let d = unsafe { Dialog::from_full(ffi::awra_dialog_new(std::ptr::null())) };
        if let Some(p) = parent {
            d.0.set_rust_data("dialog-parent", p.downgrade());
        }
        if let Ok(w) = d.0.clone().downcast::<crate::Window>() {
            crate::window::window_added(&w);
        }
        d
    }
    pub fn set_title(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_window_set_title(ptr!(self), t.ptr()) }
    }
    pub fn set_modal(&self, _m: bool) {}
    pub fn set_content(&self, w: impl IntoOptWidget) {
        unsafe { ffi::awra_dialog_set_child(ptr!(self), w.into_opt_ptr()) }
    }
    pub fn set_child(&self, w: impl IntoOptWidget) {
        self.set_content(w)
    }
    pub fn set_body(&self, text: &str) {
        let t = cstring(text);
        unsafe { ffi::awra_dialog_set_body(ptr!(self), t.as_ptr()) }
    }
    pub fn add_response(&self, id: i32, label: &str, appearance: ButtonAppearance) {
        let l = cstring(label);
        unsafe { ffi::awra_dialog_add_response(ptr!(self), id, l.as_ptr(), appearance as _) }
    }
    pub fn connect_response<F: Fn(&Dialog, i32) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Dialog, _>(sig::SELECTED, move |w, a| f(w, a.index()))
    }
    pub fn set_content_width(&self, w: i32) {
        unsafe { ffi::awra_dialog_set_content_width(ptr!(self), w) }
    }
    pub fn set_default_size(&self, w: i32, _h: i32) {
        self.set_content_width(w)
    }
    pub fn present(&self) {
        let parent = self.0.rust_data::<crate::WeakRef<crate::Window>>("dialog-parent").and_then(|w| w.upgrade());
        let pp = parent.as_ref().map_or(std::ptr::null_mut(), |w| w.as_ptr());
        unsafe { ffi::awra_dialog_present(ptr!(self), pp) }
    }
    pub fn close(&self) {
        unsafe { ffi::awra_dialog_close(ptr!(self)) }
    }
    pub fn connect_close_request<F: Fn(&Dialog) -> crate::glib::Propagation + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Dialog, _>(sig::CLOSE_REQUEST, move |w, a| {
            if f(w) == crate::glib::Propagation::Stop {
                a.stop()
            }
        })
    }
    pub fn add_shortcut<F: Fn(&Widget) + 'static>(&self, accel: &str, f: F) {
        crate::window::add_shortcut(&self.0, accel, f);
    }
}

// ===========================================================================
// Media

awra_wrapper!(KeyCap, ["AwraKeyCap"]);
impl KeyCap {
    pub fn new(code: impl IntoOptStr, label: impl IntoOptStr) -> KeyCap {
        let (c, l) = (OptC::new(code), OptC::new(label));
        unsafe { KeyCap::from_full(ffi::awra_key_cap_new(c.ptr(), l.ptr())) }
    }
    pub fn code(&self) -> Option<String> {
        unsafe { from_cstr_opt(ffi::awra_key_cap_code(ptr!(self))) }
    }
    pub fn key_code(&self) -> Option<String> {
        self.code()
    }
    pub fn set_label(&self, l: impl IntoOptStr) {
        let l = OptC::new(l);
        unsafe { ffi::awra_key_cap_set_label(ptr!(self), l.ptr()) }
    }
    pub fn set_secondary_label(&self, l: impl IntoOptStr) {
        let l = OptC::new(l);
        unsafe { ffi::awra_key_cap_set_secondary_label(ptr!(self), l.ptr()) }
    }
    pub fn set_badge(&self, b: impl IntoOptStr) {
        let b = OptC::new(b);
        unsafe { ffi::awra_key_cap_set_badge(ptr!(self), b.ptr()) }
    }
    pub fn set_bound(&self, b: bool) {
        unsafe { ffi::awra_key_cap_set_bound(ptr!(self), b) }
    }
    pub fn set_active(&self, a: bool) {
        unsafe { ffi::awra_key_cap_set_active(ptr!(self), a) }
    }
    pub fn set_playing(&self, a: bool) {
        self.set_active(a)
    }
    pub fn set_selected(&self, s: bool) {
        unsafe { ffi::awra_key_cap_set_selected(ptr!(self), s) }
    }
    pub fn set_dimmed(&self, d: bool) {
        unsafe { ffi::awra_key_cap_set_dimmed(ptr!(self), d) }
    }
    pub fn set_tint(&self, c: Option<&RGBA>) {
        with_rgba!(c, |p| unsafe { ffi::awra_key_cap_set_tint(ptr!(self), p) })
    }
    pub fn set_edge_color(&self, c: Option<&RGBA>) {
        with_rgba!(c, |p| unsafe { ffi::awra_key_cap_set_edge_color(ptr!(self), p) })
    }
    pub fn set_width_units(&self, u: f64) {
        unsafe { ffi::awra_key_cap_set_width_units(ptr!(self), u) }
    }
    pub fn set_height_units(&self, u: f64) {
        unsafe { ffi::awra_key_cap_set_height_units(ptr!(self), u) }
    }
    pub fn set_drop_highlight(&self, on: bool) {
        unsafe { ffi::awra_key_cap_set_drop_highlight(ptr!(self), on) }
    }
    pub fn flash(&self) {
        unsafe { ffi::awra_key_cap_flash(ptr!(self)) }
    }
    pub fn set_compact(&self, c: bool) {
        unsafe { ffi::awra_key_cap_set_compact(ptr!(self), c) }
    }
    pub fn connect_activated<F: Fn(&KeyCap, ModifierType) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<KeyCap, _>(sig::CLICKED, move |w, a| f(w, a.mods()))
    }
}

awra_wrapper!(KeyboardView, ["AwraKeyboardView"]);
fn c_list(p: *const *const c_char) -> Vec<String> {
    let mut out = Vec::new();
    if p.is_null() {
        return out;
    }
    let mut i = 0;
    loop {
        let s = unsafe { *p.add(i) };
        if s.is_null() {
            break;
        }
        out.push(unsafe { from_cstr(s) });
        i += 1;
    }
    out
}
impl KeyboardView {
    /// Awra 1: the inner GTK widget; here the widget itself.
    pub fn delegate(&self) -> KeyboardView {
        self.clone()
    }
    pub fn new() -> KeyboardView {
        unsafe { KeyboardView::from_full(ffi::awra_keyboard_view_new(0)) }
    }
    pub fn set_physical_layout(&self, l: PhysicalLayout) {
        unsafe { ffi::awra_keyboard_view_set_layout(ptr!(self), l as _) }
    }
    pub fn set_show_function_row(&self, s: bool) {
        unsafe { ffi::awra_keyboard_view_set_show_function_row(ptr!(self), s) }
    }
    pub fn set_show_navigation_cluster(&self, s: bool) {
        unsafe { ffi::awra_keyboard_view_set_show_navigation(ptr!(self), s) }
    }
    pub fn shows_navigation_cluster(&self) -> bool {
        unsafe { ffi::awra_keyboard_view_shows_navigation(ptr!(self)) }
    }
    pub fn set_show_numpad(&self, s: bool) {
        unsafe { ffi::awra_keyboard_view_set_show_numpad(ptr!(self), s) }
    }
    pub fn set_selection_mode(&self, m: SelectionMode) {
        unsafe { ffi::awra_keyboard_view_set_selection_mode(ptr!(self), m as _) }
    }
    /// Awra 1 property names ("keys-movable").
    pub fn set_property(&self, name: &str, value: bool) {
        if name == "keys-movable" {
            self.set_keys_movable(value)
        }
    }
    pub fn set_keys_movable(&self, m: bool) {
        unsafe { ffi::awra_keyboard_view_set_keys_movable(ptr!(self), m) }
    }
    pub fn set_accepts_file_drops(&self, a: bool) {
        unsafe { ffi::awra_keyboard_view_set_accepts_file_drops(ptr!(self), a) }
    }
    pub fn key_cap(&self, code: &str) -> Option<KeyCap> {
        let c = cstring(code);
        unsafe { Widget::from_none_opt(ffi::awra_keyboard_view_key_cap(ptr!(self), c.as_ptr())) }.map(KeyCap)
    }
    pub fn codes(&self) -> Vec<String> {
        c_list(unsafe { ffi::awra_keyboard_view_codes(ptr!(self)) })
    }
    pub fn selected_codes(&self) -> Vec<String> {
        c_list(unsafe { ffi::awra_keyboard_view_selected_codes(ptr!(self)) })
    }
    pub fn set_selected_codes(&self, codes: &[&str]) {
        let cs: Vec<_> = codes.iter().map(|s| cstring(s)).collect();
        let ps: Vec<*const c_char> = cs.iter().map(|c| c.as_ptr()).collect();
        unsafe { ffi::awra_keyboard_view_set_selected_codes(ptr!(self), ps.as_ptr(), ps.len() as i32) }
    }
    pub fn unselect_all(&self) {
        unsafe { ffi::awra_keyboard_view_unselect_all(ptr!(self)) }
    }
    pub fn code_at(&self, x: f64, y: f64) -> Option<String> {
        unsafe { from_cstr_opt(ffi::awra_keyboard_view_code_at(ptr!(self), x, y)) }
    }
    pub fn connect_selection_changed<F: Fn(&KeyboardView) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<KeyboardView, _>(sig::SELECTED, move |w, _| f(w))
    }
    /// Files dropped on a key: (key code, paths).
    pub fn connect_files_dropped<F: Fn(&KeyboardView, String, Vec<PathBuf>) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<KeyboardView, _>(sig::FILES_DROPPED, move |w, a| {
            let text = a.text().unwrap_or_default();
            // "code\npath\npath…"
            let mut lines = text.lines();
            let code = lines.next().unwrap_or_default().to_owned();
            let paths = lines.filter(|l| !l.is_empty()).map(PathBuf::from).collect();
            f(w, code, paths)
        })
    }
    /// A key dragged onto another.
    pub fn connect_key_moved<F: Fn(&KeyboardView, String, String) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<KeyboardView, _>(sig::MOVED, move |w, a| {
            let text = a.text().unwrap_or_default();
            let (from, to) = text.split_once('\t').unwrap_or((&text, ""));
            f(w, from.to_owned(), to.to_owned())
        })
    }
}
impl Default for KeyboardView {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(WaveformView, ["AwraWaveformView"]);
impl WaveformView {
    /// Awra 1: the inner GTK widget; here the widget itself.
    pub fn delegate(&self) -> WaveformView {
        self.clone()
    }
    pub fn new() -> WaveformView {
        unsafe { WaveformView::from_full(ffi::awra_waveform_view_new()) }
    }
    pub fn set_peaks(&self, peaks: &[f32]) {
        unsafe { ffi::awra_waveform_view_set_peaks(ptr!(self), peaks.as_ptr(), peaks.len() as i32) }
    }
    pub fn set_expected_peaks(&self, n: u32) {
        unsafe { ffi::awra_waveform_view_set_expected_peaks(ptr!(self), n as i32) }
    }
    pub fn set_duration(&self, s: f64) {
        unsafe { ffi::awra_waveform_view_set_duration(ptr!(self), s) }
    }
    pub fn set_position(&self, s: f64) {
        unsafe { ffi::awra_waveform_view_set_position(ptr!(self), s) }
    }
    pub fn set_hint_position(&self, s: f64) {
        unsafe { ffi::awra_waveform_view_set_hint_position(ptr!(self), s) }
    }
    pub fn hint_position(&self) -> f64 {
        unsafe { ffi::awra_waveform_view_hint_position(ptr!(self)) }
    }
    pub fn set_marker_position(&self, s: f64) {
        unsafe { ffi::awra_waveform_view_set_marker_position(ptr!(self), s) }
    }
    pub fn marker_position(&self) -> f64 {
        unsafe { ffi::awra_waveform_view_marker_position(ptr!(self)) }
    }
    pub fn set_marker_editable(&self, e: bool) {
        unsafe { ffi::awra_waveform_view_set_marker_editable(ptr!(self), e) }
    }
    pub fn connect_seek_requested<F: Fn(&WaveformView, f64) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<WaveformView, _>(sig::SEEK, move |w, a| f(w, a.value()))
    }
    pub fn connect_marker_moved<F: Fn(&WaveformView, f64) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<WaveformView, _>(sig::MARKER_MOVED, move |w, a| f(w, a.value()))
    }
    pub fn connect_marker_committed<F: Fn(&WaveformView, f64) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<WaveformView, _>(sig::MARKER_COMMITTED, move |w, a| f(w, a.value()))
    }
    pub fn connect_hint_position_notify<F: Fn(&WaveformView) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<WaveformView, _>(sig::NOTIFY, move |w, a| {
            if a.text().as_deref() == Some("hint-position") {
                f(w)
            }
        })
    }
}
impl Default for WaveformView {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(MediaRow, ["AwraMediaRow"]);
impl MediaRow {
    pub fn new(title: impl IntoOptStr) -> MediaRow {
        let t = OptC::new(title);
        unsafe { MediaRow::from_full(ffi::awra_media_row_new(t.ptr())) }
    }
    pub fn set_title(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_media_row_set_title(ptr!(self), t.ptr()) }
    }
    pub fn set_subtitle(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_media_row_set_subtitle(ptr!(self), t.ptr()) }
    }
    pub fn set_icon_name(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_media_row_set_icon_name(ptr!(self), t.ptr()) }
    }
    pub fn set_icon(&self, t: impl IntoOptStr) {
        self.set_icon_name(t)
    }
    pub fn set_accent_color(&self, c: Option<&RGBA>) {
        with_rgba!(c, |p| unsafe { ffi::awra_media_row_set_accent_color(ptr!(self), p) })
    }
    pub fn set_show_play_button(&self, s: bool) {
        unsafe { ffi::awra_media_row_set_show_play_button(ptr!(self), s) }
    }
    pub fn set_show_stop_button(&self, s: bool) {
        unsafe { ffi::awra_media_row_set_show_stop_button(ptr!(self), s) }
    }
    pub fn set_show_volume(&self, s: bool) {
        unsafe { ffi::awra_media_row_set_show_volume(ptr!(self), s) }
    }
    pub fn set_volume(&self, v: f64) {
        unsafe { ffi::awra_media_row_set_volume(ptr!(self), v) }
    }
    pub fn set_playing(&self, p: bool) {
        unsafe { ffi::awra_media_row_set_playing(ptr!(self), p) }
    }
    pub fn set_duration(&self, s: f64) {
        unsafe { ffi::awra_media_row_set_duration(ptr!(self), s) }
    }
    pub fn set_position(&self, s: f64) {
        unsafe { ffi::awra_media_row_set_position(ptr!(self), s) }
    }
    pub fn set_seekable(&self, s: bool) {
        unsafe { ffi::awra_media_row_set_seekable(ptr!(self), s) }
    }
    pub fn set_suffix(&self, w: impl IntoOptWidget) {
        unsafe { ffi::awra_media_row_set_suffix(ptr!(self), w.into_opt_ptr()) }
    }
    pub fn connect_play_requested<F: Fn(&MediaRow) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<MediaRow, _>(sig::PLAY_REQUESTED, move |w, _| f(w))
    }
    pub fn connect_stop_requested<F: Fn(&MediaRow) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<MediaRow, _>(sig::STOP_REQUESTED, move |w, _| f(w))
    }
    pub fn connect_seek_requested<F: Fn(&MediaRow, f64) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<MediaRow, _>(sig::SEEK, move |w, a| f(w, a.value()))
    }
    pub fn connect_volume_changed<F: Fn(&MediaRow, f64) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<MediaRow, _>(sig::VALUE_CHANGED, move |w, a| f(w, a.value()))
    }
}

awra_wrapper!(DropZone, ["AwraDropZone"]);
impl DropZone {
    pub fn new() -> DropZone {
        unsafe { DropZone::from_full(ffi::awra_drop_zone_new()) }
    }
    pub fn set_title(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_drop_zone_set_title(ptr!(self), t.ptr()) }
    }
    pub fn set_subtitle(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_drop_zone_set_subtitle(ptr!(self), t.ptr()) }
    }
    pub fn set_icon_name(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        unsafe { ffi::awra_drop_zone_set_icon_name(ptr!(self), t.ptr()) }
    }
    pub fn set_browse_label(&self, t: impl IntoOptStr) {
        let t = OptC::new(t);
        self.0.set_rust_data("browse-label", t.ptr().is_null());
        unsafe { ffi::awra_drop_zone_set_browse_label(ptr!(self), t.ptr()) }
    }
    pub fn set_show_browse_button(&self, show: bool) {
        if !show {
            unsafe { ffi::awra_drop_zone_set_browse_label(ptr!(self), std::ptr::null()) }
        }
    }
    pub fn set_child(&self, w: impl IntoOptWidget) {
        unsafe { ffi::awra_drop_zone_set_child(ptr!(self), w.into_opt_ptr()) }
    }
    pub fn connect_files_dropped<F: Fn(&DropZone, Vec<PathBuf>) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<DropZone, _>(sig::FILES_DROPPED, move |w, a| {
            let paths = a.text().unwrap_or_default().lines().filter(|l| !l.is_empty()).map(PathBuf::from).collect();
            f(w, paths)
        })
    }
    pub fn connect_browse_requested<F: Fn(&DropZone) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<DropZone, _>(sig::BROWSE_REQUESTED, move |w, _| f(w))
    }
}
impl Default for DropZone {
    fn default() -> Self {
        Self::new()
    }
}
