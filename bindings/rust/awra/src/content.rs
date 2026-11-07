//! Avatars, chips, dividers, the rail, the composer and the other content
//! pieces Awra 1 offered conversation applications.
//!
//! SPDX-License-Identifier: GPL-3.0-or-later

use crate::awra_wrapper;
use crate::compat::{show_paintable, widget_paintable, Paintable, Texture};
use crate::enums::*;
use crate::ffi;
use crate::object::{cstring, from_cstr_opt, IntoOptStr, IntoOptWidget, IsWidget, OptC, Widget};
use crate::signal::{sig, SignalHandlerId};

macro_rules! ptr {
    ($s:expr) => {
        $s.0.as_ptr()
    };
}

fn tex_ptr(t: Option<&Texture>) -> *mut ffi::AwraTexture {
    t.map_or(std::ptr::null_mut(), |t| t.as_ptr())
}

// ===========================================================================
// Avatars

/// Awra 1's AwraPresence.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum Presence {
    #[default]
    None = 0,
    Online = 1,
    Idle = 2,
    Busy = 3,
    Offline = 4,
}

/// Awra 1's AwraAvatarShape.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum AvatarShape {
    #[default]
    Circle = 0,
    Rounded = 1,
}

awra_wrapper!(
    /// A picture, or initials on a plate of the name's hue (Awra 1's AwraAvatar).
    Avatar, ["AwraAvatar"]
);

impl Avatar {
    pub fn new(size: i32, text: Option<&str>) -> Avatar {
        let t = OptC::new(text);
        unsafe { Avatar::from_full(ffi::awra_avatar_new(size, t.ptr())) }
    }
    pub fn set_size(&self, size: i32) {
        unsafe { ffi::awra_avatar_set_size(ptr!(self), size) }
    }
    pub fn size(&self) -> i32 {
        unsafe { ffi::awra_avatar_size(ptr!(self)) }
    }
    pub fn set_text(&self, text: Option<&str>) {
        let t = OptC::new(text);
        unsafe { ffi::awra_avatar_set_text(ptr!(self), t.ptr()) }
    }
    pub fn text(&self) -> Option<String> {
        unsafe { from_cstr_opt(ffi::awra_avatar_text(ptr!(self))) }
    }
    pub fn set_show_initials(&self, show: bool) {
        unsafe { ffi::awra_avatar_set_show_initials(ptr!(self), show) }
    }
    pub fn set_custom_image(&self, image: Option<&Paintable>) {
        show_paintable(self, image, |w, t| unsafe { ffi::awra_avatar_set_texture(w.as_ptr(), tex_ptr(t)) })
    }
    pub fn custom_image(&self) -> Option<Paintable> {
        widget_paintable(self)
    }
    pub fn set_icon_name(&self, icon: Option<&str>) {
        let t = OptC::new(icon);
        unsafe { ffi::awra_avatar_set_icon_name(ptr!(self), t.ptr()) }
    }
    pub fn set_shape(&self, shape: AvatarShape) {
        unsafe { ffi::awra_avatar_set_shape(ptr!(self), shape as i32) }
    }
    pub fn set_presence(&self, presence: Presence) {
        unsafe { ffi::awra_avatar_set_presence(ptr!(self), presence as i32) }
    }
    pub fn set_speaking(&self, speaking: bool) {
        unsafe { ffi::awra_avatar_set_speaking(ptr!(self), speaking) }
    }
}

awra_wrapper!(AvatarGroup, ["AwraAvatarGroup"]);
impl AvatarGroup {
    pub fn new() -> AvatarGroup {
        unsafe { AvatarGroup::from_full(ffi::awra_avatar_group_new()) }
    }
    pub fn append(&self, avatar: &impl IsWidget) {
        unsafe { ffi::awra_avatar_group_append(ptr!(self), avatar.widget_ptr()) }
    }
    pub fn remove_all(&self) {
        unsafe { ffi::awra_avatar_group_remove_all(ptr!(self)) }
    }
}
impl Default for AvatarGroup {
    fn default() -> Self {
        Self::new()
    }
}

// ===========================================================================
// Chips, dividers, skeletons, typing, groups, error state

/// Awra 1's AwraChipAppearance.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum ChipAppearance {
    #[default]
    Neutral = 0,
    Accent = 1,
}

awra_wrapper!(
    /// A pill with a picture, a label and a count (Awra 1's AwraChip).
    Chip, ["AwraChip"]
);

impl Chip {
    pub fn new(label: Option<&str>) -> Chip {
        let t = OptC::new(label);
        unsafe { Chip::from_full(ffi::awra_chip_new(t.ptr())) }
    }
    pub fn set_label(&self, label: Option<&str>) {
        let t = OptC::new(label);
        unsafe { ffi::awra_chip_set_label(ptr!(self), t.ptr()) }
    }
    pub fn label(&self) -> Option<String> {
        unsafe { from_cstr_opt(ffi::awra_chip_label(ptr!(self))) }
    }
    pub fn set_icon_name(&self, icon: Option<&str>) {
        let t = OptC::new(icon);
        unsafe { ffi::awra_chip_set_icon_name(ptr!(self), t.ptr()) }
    }
    pub fn set_emoji(&self, emoji: Option<&str>) {
        let t = OptC::new(emoji);
        unsafe { ffi::awra_chip_set_emoji(ptr!(self), t.ptr()) }
    }
    pub fn set_paintable(&self, p: Option<&Paintable>) {
        show_paintable(self, p, |w, t| unsafe { ffi::awra_chip_set_texture(w.as_ptr(), tex_ptr(t)) })
    }
    pub fn paintable(&self) -> Option<Paintable> {
        widget_paintable(self)
    }
    pub fn set_count(&self, count: i32) {
        unsafe { ffi::awra_chip_set_count(ptr!(self), count) }
    }
    pub fn count(&self) -> i32 {
        unsafe { ffi::awra_chip_count(ptr!(self)) }
    }
    pub fn set_active(&self, active: bool) {
        unsafe { ffi::awra_chip_set_active(ptr!(self), active) }
    }
    pub fn is_active(&self) -> bool {
        unsafe { ffi::awra_chip_active(ptr!(self)) }
    }
    pub fn set_toggleable(&self, t: bool) {
        unsafe { ffi::awra_chip_set_toggleable(ptr!(self), t) }
    }
    pub fn set_appearance(&self, a: ChipAppearance) {
        unsafe { ffi::awra_chip_set_appearance(ptr!(self), a as i32) }
    }
    pub fn connect_activate<F: Fn(&Chip) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Chip, _>(sig::CLICKED, move |w, _| f(w))
    }
    pub fn connect_toggled<F: Fn(&Chip) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Chip, _>(sig::TOGGLED, move |w, _| f(w))
    }
}

/// Awra 1's AwraDividerLabelPosition.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum DividerLabelPosition {
    #[default]
    Center = 0,
    End = 1,
    Start = 2,
}

/// Awra 1's AwraDividerAppearance.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum DividerAppearance {
    #[default]
    Neutral = 0,
    Accent = 1,
    Danger = 2,
}

awra_wrapper!(
    /// A line with an optional label (Awra 1's AwraDivider).
    Divider, ["AwraDivider"]
);

impl Divider {
    pub fn new(label: Option<&str>) -> Divider {
        let t = OptC::new(label);
        unsafe { Divider::from_full(ffi::awra_divider_new(t.ptr())) }
    }
    pub fn set_label(&self, label: Option<&str>) {
        self.0.set_rust_data("p:label", label.map(str::to_owned));
        let t = OptC::new(label);
        unsafe { ffi::awra_divider_set_label(ptr!(self), t.ptr()) }
    }
    pub fn label(&self) -> Option<String> {
        self.0.rust_data::<Option<String>>("p:label").flatten()
    }
    pub fn set_label_position(&self, p: DividerLabelPosition) {
        unsafe { ffi::awra_divider_set_label_position(ptr!(self), p as i32) }
    }
    pub fn set_appearance(&self, a: DividerAppearance) {
        unsafe { ffi::awra_divider_set_appearance(ptr!(self), a as i32) }
    }
}

awra_wrapper!(Skeleton, ["AwraSkeleton"]);
impl Skeleton {
    pub fn new() -> Skeleton {
        unsafe { Skeleton::from_full(ffi::awra_skeleton_new()) }
    }
    pub fn set_lines(&self, lines: u32) {
        unsafe { ffi::awra_skeleton_set_lines(ptr!(self), lines as i32) }
    }
    pub fn set_animated(&self, animated: bool) {
        unsafe { ffi::awra_skeleton_set_animated(ptr!(self), animated) }
    }
}
impl Default for Skeleton {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(TypingIndicator, ["AwraTypingIndicator"]);
impl TypingIndicator {
    pub fn new() -> TypingIndicator {
        unsafe { TypingIndicator::from_full(ffi::awra_typing_indicator_new()) }
    }
    pub fn set_label(&self, label: Option<&str>) {
        self.0.set_rust_data("p:label", label.map(str::to_owned));
        let t = OptC::new(label);
        unsafe { ffi::awra_typing_indicator_set_label(ptr!(self), t.ptr()) }
    }
    pub fn label(&self) -> Option<String> {
        self.0.rust_data::<Option<String>>("p:label").flatten()
    }
    pub fn set_active(&self, active: bool) {
        self.0.set_rust_data("p:active", active);
        unsafe { ffi::awra_typing_indicator_set_active(ptr!(self), active) }
    }
    pub fn is_active(&self) -> bool {
        self.0.rust_data::<bool>("p:active").unwrap_or(false)
    }
}
impl Default for TypingIndicator {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(BadgeGroup, ["AwraFlowBox"]);
impl BadgeGroup {
    pub fn new() -> BadgeGroup {
        unsafe { BadgeGroup::from_full(ffi::awra_badge_group_new()) }
    }
    pub fn append(&self, badge: &crate::Badge) {
        unsafe { ffi::awra_badge_group_append(ptr!(self), badge.widget_ptr()) }
    }
    /// The badge at `index`.
    pub fn badge(&self, index: u32) -> Option<crate::Badge> {
        self.children().into_iter().nth(index as usize).and_then(|w| w.downcast().ok())
    }
    pub fn remove(&self, badge: &crate::Badge) {
        unsafe { ffi::awra_container_remove(ptr!(self), badge.widget_ptr()) }
    }
    pub fn remove_all(&self) {
        unsafe { ffi::awra_badge_group_remove_all(ptr!(self)) }
    }
}
impl Default for BadgeGroup {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(MetadataGroup, ["AwraBox"]);
impl MetadataGroup {
    pub fn new() -> MetadataGroup {
        unsafe { MetadataGroup::from_full(ffi::awra_metadata_group_new()) }
    }
    pub fn set_title(&self, title: impl crate::IntoOptStr) {
        let t = OptC::new(title);
        unsafe { ffi::awra_metadata_group_set_title(ptr!(self), t.ptr()) }
    }
    pub fn append(&self, label: &str, value: &str) -> Widget {
        let (l, v) = (cstring(label), cstring(value));
        unsafe { Widget::from_none(ffi::awra_metadata_group_append(ptr!(self), l.as_ptr(), v.as_ptr())) }
    }
}
impl Default for MetadataGroup {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(ErrorState, ["AwraPage"]);
impl ErrorState {
    pub fn new() -> ErrorState {
        unsafe { ErrorState::from_full(ffi::awra_error_state_new(std::ptr::null(), std::ptr::null())) }
    }
    pub fn set_title(&self, t: Option<&str>) {
        let t = OptC::new(t);
        unsafe { ffi::awra_empty_state_set_title(ptr!(self), t.ptr()) }
    }
    pub fn set_description(&self, d: Option<&str>) {
        let t = OptC::new(d);
        unsafe { ffi::awra_empty_state_set_description(ptr!(self), t.ptr()) }
    }
    pub fn set_action(&self, action: Option<&impl IsWidget>) {
        if let Some(a) = action {
            unsafe { ffi::awra_empty_state_add_action(ptr!(self), a.widget_ptr()) }
        }
    }
}
impl Default for ErrorState {
    fn default() -> Self {
        Self::new()
    }
}

// ===========================================================================
// Rich label

awra_wrapper!(
    /// A label with pictures in its text (Awra 1's AwraRichLabel).
    RichLabel, ["AwraRichLabel"]
);

impl RichLabel {
    pub fn new() -> RichLabel {
        unsafe { RichLabel::from_full(ffi::awra_rich_label_new()) }
    }
    pub fn label(&self) -> crate::Label {
        unsafe { Widget::from_none(ffi::awra_rich_label_label(ptr!(self))) }.downcast().expect("a label")
    }
    pub fn clear_paintables(&self) {
        unsafe { ffi::awra_rich_label_clear_textures(ptr!(self)) }
        self.0.set_rust_data("rich-paintables", Vec::<Option<Paintable>>::new());
    }
    /// The next picture; an animated one keeps moving.
    pub fn append_paintable(&self, p: Option<&Paintable>) {
        let mut all: Vec<Option<Paintable>> = self.0.rust_data("rich-paintables").unwrap_or_default();
        let index = all.len() as i32;
        all.push(p.cloned());
        self.0.set_rust_data("rich-paintables", all);
        let label = unsafe { Widget::from_none(ffi::awra_rich_label_label(ptr!(self))) };
        match p {
            Some(p) => {
                // a hidden helper per picture follows the paintable
                let w = label.clone();
                let tracker = crate::Box::new(Orientation::Horizontal, 0);
                p.show_on(&tracker, move |_, t| unsafe {
                    ffi::awra_label_set_inline_texture(w.as_ptr(), index, tex_ptr(t))
                });
                let mut trackers: Vec<crate::Box> = self.0.rust_data("rich-trackers").unwrap_or_default();
                trackers.truncate(index as usize);
                trackers.push(tracker);
                self.0.set_rust_data("rich-trackers", trackers);
            }
            None => unsafe { ffi::awra_label_set_inline_texture(label.as_ptr(), index, std::ptr::null_mut()) },
        }
    }
    pub fn set_inline_size(&self, size: i32) {
        unsafe { ffi::awra_rich_label_set_inline_size(ptr!(self), size) }
    }
}
impl Default for RichLabel {
    fn default() -> Self {
        Self::new()
    }
}

// ===========================================================================
// Rail

awra_wrapper!(
    /// A column of place tiles (Awra 1's AwraRail).
    Rail, ["AwraRail"]
);
awra_wrapper!(RailItem, ["AwraRailItem"]);
awra_wrapper!(RailGroup, ["AwraRailGroup"]);

impl Rail {
    pub fn new() -> Rail {
        unsafe { Rail::from_full(ffi::awra_rail_new()) }
    }
    pub fn append(&self, entry: &impl IsWidget) {
        unsafe { ffi::awra_rail_append(ptr!(self), entry.widget_ptr()) }
    }
    pub fn insert(&self, entry: &impl IsWidget, position: i32) {
        unsafe { ffi::awra_rail_insert(ptr!(self), entry.widget_ptr(), position) }
    }
    pub fn remove(&self, entry: &impl IsWidget) {
        unsafe { ffi::awra_rail_remove(ptr!(self), entry.widget_ptr()) }
    }
    pub fn add_separator(&self) -> Widget {
        unsafe { Widget::from_none(ffi::awra_rail_add_separator(ptr!(self))) }
    }
    pub fn n_entries(&self) -> u32 {
        unsafe { ffi::awra_rail_n_entries(ptr!(self)) as u32 }
    }
    pub fn entry(&self, position: u32) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_rail_entry(ptr!(self), position as i32)) }
    }
    pub fn selected_item(&self) -> Option<RailItem> {
        unsafe { Widget::from_none_opt(ffi::awra_rail_selected_item(ptr!(self))) }.map(RailItem)
    }
    pub fn set_selected_item(&self, item: Option<&RailItem>) {
        unsafe { ffi::awra_rail_set_selected_item(ptr!(self), item.map_or(std::ptr::null_mut(), |i| i.as_ptr())) }
    }
    pub fn set_header(&self, header: Option<&impl IsWidget>) {
        unsafe { ffi::awra_rail_set_header(ptr!(self), header.map_or(std::ptr::null_mut(), |h| h.widget_ptr())) }
    }
    pub fn set_footer(&self, footer: Option<&impl IsWidget>) {
        unsafe { ffi::awra_rail_set_footer(ptr!(self), footer.map_or(std::ptr::null_mut(), |h| h.widget_ptr())) }
    }
    pub fn set_reorderable(&self, r: bool) {
        unsafe { ffi::awra_rail_set_reorderable(ptr!(self), r) }
    }
    pub fn is_reorderable(&self) -> bool {
        unsafe { ffi::awra_rail_reorderable(ptr!(self)) }
    }
    /// An entry dragged to `position` (not offered yet: entries don't drag).
    pub fn connect_item_moved<F: Fn(&Rail, &Widget, u32) + 'static>(&self, _f: F) {}
    /// An entry dropped onto another (not offered yet).
    pub fn connect_item_merged<F: Fn(&Rail, &Widget, &Widget) + 'static>(&self, _f: F) {}
}
impl Default for Rail {
    fn default() -> Self {
        Self::new()
    }
}

impl RailItem {
    pub fn new(label: Option<&str>) -> RailItem {
        let t = OptC::new(label);
        unsafe { RailItem::from_full(ffi::awra_rail_item_new(t.ptr())) }
    }
    pub fn set_label(&self, label: Option<&str>) {
        let t = OptC::new(label);
        unsafe { ffi::awra_rail_item_set_label(ptr!(self), t.ptr()) }
    }
    pub fn label(&self) -> Option<String> {
        unsafe { from_cstr_opt(ffi::awra_rail_item_label(ptr!(self))) }
    }
    pub fn set_child(&self, child: impl IntoOptWidget) {
        unsafe { ffi::awra_rail_item_set_child(ptr!(self), child.into_opt_ptr()) }
    }
    pub fn child(&self) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_rail_item_child(ptr!(self))) }
    }
    pub fn set_icon_name(&self, icon: Option<&str>) {
        let t = OptC::new(icon);
        unsafe { ffi::awra_rail_item_set_icon_name(ptr!(self), t.ptr()) }
    }
    pub fn set_selected(&self, s: bool) {
        unsafe { ffi::awra_rail_item_set_selected(ptr!(self), s) }
    }
    pub fn is_selected(&self) -> bool {
        unsafe { ffi::awra_rail_item_selected(ptr!(self)) }
    }
    pub fn set_unread(&self, u: bool) {
        unsafe { ffi::awra_rail_item_set_unread(ptr!(self), u) }
    }
    pub fn set_mention_count(&self, n: u32) {
        unsafe { ffi::awra_rail_item_set_mention_count(ptr!(self), n) }
    }
    pub fn set_muted(&self, m: bool) {
        unsafe { ffi::awra_rail_item_set_muted(ptr!(self), m) }
    }
    pub fn set_has_tooltip(&self, _t: bool) {}
    pub fn connect_activate<F: Fn(&RailItem) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<RailItem, _>(sig::ACTIVATE, move |w, _| f(w))
    }
}

impl RailGroup {
    pub fn new(label: Option<&str>) -> RailGroup {
        let t = OptC::new(label);
        unsafe { RailGroup::from_full(ffi::awra_rail_group_new(t.ptr())) }
    }
    pub fn set_label(&self, label: Option<&str>) {
        self.0.set_rust_data("p:label", label.map(str::to_owned));
        let t = OptC::new(label);
        unsafe { ffi::awra_rail_group_set_label(ptr!(self), t.ptr()) }
    }
    pub fn label(&self) -> Option<String> {
        self.0.rust_data::<Option<String>>("p:label").flatten()
    }
    pub fn color(&self) -> Option<crate::gdk::RGBA> {
        self.0.rust_data::<Option<crate::gdk::RGBA>>("p:color").flatten()
    }
    pub fn set_icon_name(&self, icon: Option<&str>) {
        let t = OptC::new(icon);
        unsafe { ffi::awra_rail_group_set_icon_name(ptr!(self), t.ptr()) }
    }
    pub fn set_color(&self, c: Option<&crate::gdk::RGBA>) {
        self.0.set_rust_data("p:color", c.copied());
        let v = c.map(|c| c.to_ffi());
        unsafe { ffi::awra_rail_group_set_color(ptr!(self), v.as_ref().map_or(std::ptr::null(), |v| v)) }
    }
    pub fn set_expanded(&self, e: bool) {
        unsafe { ffi::awra_rail_group_set_expanded(ptr!(self), e) }
    }
    pub fn is_expanded(&self) -> bool {
        unsafe { ffi::awra_rail_group_expanded(ptr!(self)) }
    }
    pub fn append(&self, item: &RailItem) {
        unsafe { ffi::awra_rail_group_append(ptr!(self), item.as_ptr()) }
    }
    pub fn insert(&self, item: &RailItem, position: i32) {
        unsafe { ffi::awra_rail_group_insert(ptr!(self), item.as_ptr(), position) }
    }
    pub fn remove(&self, item: &RailItem) {
        unsafe { ffi::awra_rail_group_remove(ptr!(self), item.as_ptr()) }
    }
    pub fn n_items(&self) -> u32 {
        unsafe { ffi::awra_rail_group_n_items(ptr!(self)) as u32 }
    }
    pub fn item(&self, position: u32) -> Option<RailItem> {
        unsafe { Widget::from_none_opt(ffi::awra_rail_group_item(ptr!(self), position as i32)) }.map(RailItem)
    }
    pub fn connect_expanded_notify<F: Fn(&RailGroup) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<RailGroup, _>(sig::TOGGLED, move |w, _| f(w))
    }
}

// ===========================================================================
// Composer

awra_wrapper!(
    /// The message well (Awra 1's AwraComposer).
    Composer, ["AwraComposer"]
);

impl Composer {
    pub fn new() -> Composer {
        unsafe { Composer::from_full(ffi::awra_composer_new()) }
    }
    pub fn view(&self) -> crate::TextView {
        unsafe { Widget::from_none(ffi::awra_composer_view(ptr!(self))) }.downcast().expect("a text view")
    }
    pub fn buffer(&self) -> crate::TextBuffer {
        self.view().buffer()
    }
    pub fn set_text(&self, text: Option<&str>) {
        let t = OptC::new(text);
        unsafe { ffi::awra_composer_set_text(ptr!(self), t.ptr()) }
    }
    pub fn text(&self) -> String {
        unsafe { crate::object::from_cstr(ffi::awra_composer_text(ptr!(self))) }
    }
    pub fn set_placeholder_text(&self, text: Option<&str>) {
        self.0.set_rust_data("placeholder", text.map(str::to_owned));
        let t = OptC::new(text);
        unsafe { ffi::awra_composer_set_placeholder(ptr!(self), t.ptr()) }
    }
    pub fn placeholder_text(&self) -> Option<String> {
        self.0.rust_data::<Option<String>>("placeholder").flatten()
    }
    pub fn set_max_lines(&self, n: u32) {
        unsafe { ffi::awra_composer_set_max_lines(ptr!(self), n as i32) }
    }
    pub fn set_can_send(&self, c: bool) {
        unsafe { ffi::awra_composer_set_can_send(ptr!(self), c) }
    }
    pub fn set_send_on_enter(&self, c: bool) {
        unsafe { ffi::awra_composer_set_send_on_enter(ptr!(self), c) }
    }
    pub fn set_busy(&self, b: bool) {
        unsafe { ffi::awra_composer_set_busy(ptr!(self), b) }
    }
    pub fn set_banner(&self, banner: Option<&impl IsWidget>) {
        unsafe { ffi::awra_composer_set_banner(ptr!(self), banner.map_or(std::ptr::null_mut(), |b| b.widget_ptr())) }
    }
    pub fn banner(&self) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_composer_banner(ptr!(self))) }
    }
    pub fn show_banner(&self, text: &str, closable: bool) {
        let t = cstring(text);
        unsafe { ffi::awra_composer_show_banner(ptr!(self), t.as_ptr(), closable) }
    }
    pub fn set_attachments(&self, a: Option<&impl IsWidget>) {
        unsafe { ffi::awra_composer_set_attachments(ptr!(self), a.map_or(std::ptr::null_mut(), |b| b.widget_ptr())) }
    }
    pub fn attachments(&self) -> Option<Widget> {
        unsafe { Widget::from_none_opt(ffi::awra_composer_attachments(ptr!(self))) }
    }
    pub fn set_start_widget(&self, w: Option<&impl IsWidget>) {
        unsafe { ffi::awra_composer_set_start_widget(ptr!(self), w.map_or(std::ptr::null_mut(), |b| b.widget_ptr())) }
    }
    pub fn set_end_widget(&self, w: Option<&impl IsWidget>) {
        unsafe { ffi::awra_composer_set_end_widget(ptr!(self), w.map_or(std::ptr::null_mut(), |b| b.widget_ptr())) }
    }
    pub fn connect_submit<F: Fn(&Composer) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Composer, _>(sig::ACTIVATE, move |w, _| f(w))
    }
    pub fn connect_banner_closed<F: Fn(&Composer) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Composer, _>(sig::STOP_REQUESTED, move |w, _| f(w))
    }
    pub fn connect_text_notify<F: Fn(&Composer) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<Composer, _>(sig::CHANGED, move |w, _| f(w))
    }
}
impl Default for Composer {
    fn default() -> Self {
        Self::new()
    }
}

/// A text for an optional label (Awra 1 setters took `Option<&str>`).
pub fn opt_text(s: impl IntoOptStr) -> Option<String> {
    s.into_opt_string()
}

// ===========================================================================
// Fields, overlay bar, palette, emoji picker, data view

awra_wrapper!(
    /// A framed multi-line text field (Awra 1's AwraTextArea).
    TextArea, ["AwraTextView", "AwraEntry"]
);
impl TextArea {
    pub fn new() -> TextArea {
        unsafe { TextArea::from_full(ffi::awra_text_area_new()) }
    }
    pub fn delegate(&self) -> crate::TextView {
        unsafe { Widget::from_none(ptr!(self)) }.downcast().expect("a text view")
    }
    pub fn buffer(&self) -> crate::TextBuffer {
        crate::TextBuffer(self.0.clone())
    }
    pub fn text(&self) -> String {
        unsafe { crate::object::from_cstr(ffi::awra_text_view_text(ptr!(self))) }
    }
    pub fn set_text(&self, text: &str) {
        let t = cstring(text);
        unsafe { ffi::awra_text_view_set_text(ptr!(self), t.as_ptr()) }
    }
    pub fn set_placeholder_text(&self, text: &str) {
        let t = cstring(text);
        unsafe { ffi::awra_text_view_set_placeholder(ptr!(self), t.as_ptr()) }
    }
    pub fn set_monospace(&self, mono: bool) {
        self.0.set_typography(if mono { Typography::Monospace } else { Typography::Body });
    }
}
impl Default for TextArea {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(
    /// Hidden text with a peek button (Awra 1's AwraPasswordEntry).
    PasswordEntry, ["AwraOverlay"]
);
impl PasswordEntry {
    pub fn new() -> PasswordEntry {
        unsafe { PasswordEntry::from_full(ffi::awra_password_entry_new()) }
    }
    pub fn delegate(&self) -> crate::Entry {
        unsafe { Widget::from_none(ffi::awra_password_entry_entry(ptr!(self))) }.downcast().expect("an entry")
    }
    pub fn text(&self) -> String {
        unsafe { crate::object::from_cstr(ffi::awra_entry_text(ffi::awra_password_entry_entry(ptr!(self)))) }
    }
    pub fn set_text(&self, text: &str) {
        let t = cstring(text);
        unsafe { ffi::awra_entry_set_text(ffi::awra_password_entry_entry(ptr!(self)), t.as_ptr()) }
    }
    pub fn set_placeholder_text(&self, text: impl crate::IntoOptStr) {
        let t = OptC::new(text);
        unsafe { ffi::awra_entry_set_placeholder(ffi::awra_password_entry_entry(ptr!(self)), t.ptr()) }
    }
    pub fn set_show_peek_icon(&self, show: bool) {
        unsafe { ffi::awra_password_entry_set_show_peek_icon(ptr!(self), show) }
    }
    pub fn set_activates_default(&self, _a: bool) {}
    pub fn set_position(&self, position: i32) {
        unsafe { ffi::awra_entry_set_position(ffi::awra_password_entry_entry(ptr!(self)), position) }
    }
    pub fn connect_activate<F: Fn(&PasswordEntry) + 'static>(&self, f: F) -> SignalHandlerId {
        let me = self.downgrade();
        self.delegate().connect_raw(sig::ACTIVATE, move |_, _| {
            if let Some(p) = me.upgrade() {
                f(&p)
            }
        })
    }
    pub fn connect_changed<F: Fn(&PasswordEntry) + 'static>(&self, f: F) -> SignalHandlerId {
        let me = self.downgrade();
        self.delegate().connect_raw(sig::CHANGED, move |_, _| {
            if let Some(p) = me.upgrade() {
                f(&p)
            }
        })
    }
}
impl Default for PasswordEntry {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(
    /// Words as chips and a field to add one (Awra 1's AwraTagEntry).
    TagEntry, ["AwraBox"]
);
impl TagEntry {
    pub fn new() -> TagEntry {
        unsafe { TagEntry::from_full(ffi::awra_tag_entry_new()) }
    }
    pub fn delegate(&self) -> crate::Entry {
        unsafe { Widget::from_none(ffi::awra_tag_entry_entry(ptr!(self))) }.downcast().expect("an entry")
    }
    pub fn add(&self, tag: &str) -> bool {
        let t = cstring(tag);
        unsafe { ffi::awra_tag_entry_add(ptr!(self), t.as_ptr()) }
    }
    pub fn clear(&self) {
        unsafe { ffi::awra_tag_entry_clear(ptr!(self)) }
    }
    pub fn n_tags(&self) -> u32 {
        unsafe { ffi::awra_tag_entry_n_tags(ptr!(self)) as u32 }
    }
    pub fn tag(&self, i: u32) -> Option<String> {
        unsafe { from_cstr_opt(ffi::awra_tag_entry_tag(ptr!(self), i as i32)) }
    }
    pub fn tags(&self) -> Vec<String> {
        (0..self.n_tags()).filter_map(|i| self.tag(i)).collect()
    }
    pub fn connect_tag_added<F: Fn(&TagEntry, &str) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<TagEntry, _>(sig::ACTIVATE, move |w, a| f(w, &a.text().unwrap_or_default()))
    }
    pub fn connect_tag_removed<F: Fn(&TagEntry, &str) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<TagEntry, _>(sig::STOP_REQUESTED, move |w, a| f(w, &a.text().unwrap_or_default()))
    }
}
impl Default for TagEntry {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(
    /// Controls floating over media (Awra 1's AwraOverlayBar).
    OverlayBar, ["AwraOverlayBar"]
);
impl OverlayBar {
    pub fn new() -> OverlayBar {
        unsafe { OverlayBar::from_full(ffi::awra_overlay_bar_new()) }
    }
    pub fn set_content(&self, content: Option<&impl IsWidget>) {
        unsafe { ffi::awra_overlay_bar_set_content(ptr!(self), content.map_or(std::ptr::null_mut(), |c| c.widget_ptr())) }
    }
    pub fn set_bar(&self, bar: Option<&impl IsWidget>) {
        unsafe { ffi::awra_overlay_bar_set_bar(ptr!(self), bar.map_or(std::ptr::null_mut(), |c| c.widget_ptr())) }
    }
    pub fn set_auto_hide(&self, a: bool) {
        unsafe { ffi::awra_overlay_bar_set_auto_hide(ptr!(self), a) }
    }
    pub fn set_inhibit_auto_hide(&self, a: bool) {
        unsafe { ffi::awra_overlay_bar_set_inhibit_auto_hide(ptr!(self), a) }
    }
    pub fn set_revealed(&self, r: bool) {
        unsafe { ffi::awra_overlay_bar_set_revealed(ptr!(self), r) }
    }
    pub fn reveal(&self) {
        unsafe { ffi::awra_overlay_bar_reveal(ptr!(self)) }
    }
    pub fn set_hide_delay(&self, ms: u32) {
        unsafe { ffi::awra_overlay_bar_set_hide_delay(ptr!(self), ms) }
    }
    pub fn set_max_width(&self, w: u32) {
        unsafe { ffi::awra_overlay_bar_set_max_width(ptr!(self), w as i32) }
    }
    pub fn set_inset(&self, s: Spacing) {
        unsafe { ffi::awra_overlay_bar_set_inset(ptr!(self), s as i32) }
    }
}
impl Default for OverlayBar {
    fn default() -> Self {
        Self::new()
    }
}

awra_wrapper!(
    /// Swatches to choose a colour (Awra 1's AwraColorPalette).
    ColorPalette, ["AwraBox"]
);
impl ColorPalette {
    pub fn new() -> ColorPalette {
        unsafe { ColorPalette::from_full(ffi::awra_color_palette_new()) }
    }
    pub fn add_color(&self, c: &crate::gdk::RGBA, name: Option<&str>) {
        let v = c.to_ffi();
        let n = OptC::new(name);
        unsafe { ffi::awra_color_palette_add_color(ptr!(self), &v, n.ptr()) }
    }
    pub fn clear(&self) {
        unsafe { ffi::awra_color_palette_clear(ptr!(self)) }
    }
    pub fn set_automatic_label(&self, label: Option<&str>) {
        let n = OptC::new(label);
        unsafe { ffi::awra_color_palette_set_automatic_label(ptr!(self), n.ptr()) }
    }
    pub fn set_allows_custom(&self, a: bool) {
        unsafe { ffi::awra_color_palette_set_allows_custom(ptr!(self), a) }
    }
    pub fn set_selected(&self, c: Option<&crate::gdk::RGBA>) {
        let v = c.map(|c| c.to_ffi());
        unsafe { ffi::awra_color_palette_set_selected(ptr!(self), v.as_ref().map_or(std::ptr::null(), |v| v)) }
    }
    pub fn selected(&self) -> Option<crate::gdk::RGBA> {
        let mut c = ffi::AwraRGBA::default();
        unsafe { ffi::awra_color_palette_selected(ptr!(self), &mut c) }.then(|| crate::gdk::RGBA::from_ffi(c))
    }
    /// A colour chosen (None: automatic).
    pub fn connect_chosen<F: Fn(&ColorPalette, Option<&crate::gdk::RGBA>) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<ColorPalette, _>(sig::SELECTED, move |w, a| {
            let c = if a.index() == 0 { None } else { crate::gdk::RGBA::parse(&a.text().unwrap_or_default()).ok() };
            f(w, c.as_ref())
        })
    }
}
impl Default for ColorPalette {
    fn default() -> Self {
        Self::new()
    }
}

/// An emoji of a picker (Awra 1's AwraEmojiItem).
#[derive(Clone, Debug)]
pub struct EmojiItem(std::rc::Rc<EmojiData>);

#[derive(Debug)]
pub struct EmojiData {
    id: String,
    name: String,
    text: Option<String>,
    paintable: Option<Paintable>,
    keywords: std::cell::RefCell<Vec<String>>,
}

impl EmojiItem {
    pub fn new(id: &str, name: &str, text: Option<&str>) -> crate::Object {
        crate::Object::new(EmojiItem(std::rc::Rc::new(EmojiData {
            id: id.to_owned(),
            name: name.to_owned(),
            text: text.map(str::to_owned),
            paintable: None,
            keywords: Default::default(),
        })))
    }
    pub fn for_paintable(id: &str, name: &str, paintable: &Paintable) -> crate::Object {
        crate::Object::new(EmojiItem(std::rc::Rc::new(EmojiData {
            id: id.to_owned(),
            name: name.to_owned(),
            text: None,
            paintable: Some(paintable.clone()),
            keywords: Default::default(),
        })))
    }
    pub fn id(&self) -> String {
        self.0.id.clone()
    }
    pub fn name(&self) -> String {
        self.0.name.clone()
    }
    pub fn text(&self) -> Option<String> {
        self.0.text.clone()
    }
    pub fn paintable(&self) -> Option<Paintable> {
        self.0.paintable.clone()
    }
}

/// Keywords, set on the item a store holds.
pub trait EmojiItemExt {
    fn set_keywords(&self, words: &[&str]);
}
impl EmojiItemExt for crate::Object {
    fn set_keywords(&self, words: &[&str]) {
        if let Some(e) = self.downcast_ref::<EmojiItem>() {
            *e.0.keywords.borrow_mut() = words.iter().map(|w| (*w).to_owned()).collect();
        }
    }
}

awra_wrapper!(
    /// Search, tabs and a grid of emoji (Awra 1's AwraEmojiPicker).
    EmojiPicker, ["AwraBox"]
);
impl EmojiPicker {
    pub fn new() -> EmojiPicker {
        unsafe { EmojiPicker::from_full(ffi::awra_emoji_picker_new()) }
    }
    pub fn clear(&self) {
        unsafe { ffi::awra_emoji_picker_clear(ptr!(self)) }
        self.0.set_rust_data("emoji-items", std::collections::HashMap::<String, EmojiItem>::new());
    }
    /// A section of the items `store` holds (EmojiItem objects).
    pub fn add_section(&self, id: &str, title: &str, icon: Option<&str>, store: &crate::ListStore) {
        let items: Vec<EmojiItem> =
            store.iter().iter().filter_map(|o| o.downcast_ref::<EmojiItem>().cloned()).collect();
        let mut keep = Vec::new();
        let mut textures = Vec::new();
        let mut c = Vec::new();
        for it in &items {
            keep.push(cstring(&it.0.id));
            let i = keep.last().unwrap().as_ptr();
            keep.push(cstring(&it.0.name));
            let n = keep.last().unwrap().as_ptr();
            let t = match &it.0.text {
                Some(t) => {
                    keep.push(cstring(t));
                    keep.last().unwrap().as_ptr()
                }
                None => std::ptr::null(),
            };
            keep.push(cstring(&it.0.keywords.borrow().join(" ")));
            let k = keep.last().unwrap().as_ptr();
            let tex = it.0.paintable.as_ref().and_then(|p| p.texture());
            let tp = tex.as_ref().map_or(std::ptr::null_mut(), |t| t.as_ptr());
            textures.push(tex);
            c.push(ffi::AwraEmoji { id: i, name: n, text: t, texture: tp, keywords: k });
        }
        let (idc, tc) = (cstring(id), cstring(title));
        let ic = OptC::new(icon);
        unsafe { ffi::awra_emoji_picker_add_section(ptr!(self), idc.as_ptr(), tc.as_ptr(), ic.ptr(), c.as_ptr(), c.len() as i32) }
        let mut map: std::collections::HashMap<String, EmojiItem> = self.0.rust_data("emoji-items").unwrap_or_default();
        for it in items {
            map.insert(it.0.id.clone(), it);
        }
        self.0.set_rust_data("emoji-items", map);
    }
    pub fn set_search_text(&self, text: &str) {
        let t = cstring(text);
        unsafe { ffi::awra_emoji_picker_set_search_text(ptr!(self), t.as_ptr()) }
    }
    pub fn search_entry(&self) -> crate::SearchEntry {
        unsafe { Widget::from_none(ffi::awra_emoji_picker_search_entry(ptr!(self))) }.downcast().expect("a search entry")
    }
    pub fn connect_picked<F: Fn(&EmojiPicker, &EmojiItem) + 'static>(&self, f: F) -> SignalHandlerId {
        self.0.connect_typed::<EmojiPicker, _>(sig::ACTIVATE, move |w, a| {
            let id = a.text().unwrap_or_default();
            let item = w.rust_data::<std::collections::HashMap<String, EmojiItem>>("emoji-items")
                .and_then(|m| m.get(&id).cloned());
            if let Some(item) = item {
                f(w, &item)
            }
        })
    }
}
impl Default for EmojiPicker {
    fn default() -> Self {
        Self::new()
    }
}

/// Awra 1's AwraDataViewState.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum DataViewState {
    #[default]
    Content = 0,
    Empty = 1,
    Loading = 2,
    Error = 3,
}

/// A column of a data view: a title and a factory for its cells.
#[derive(Clone)]
pub struct DataColumn(std::rc::Rc<DataColumnInner>);
pub struct DataColumnInner {
    title: String,
    factory: Option<crate::SignalListItemFactory>,
    view: std::cell::RefCell<Option<(crate::object::WeakRef<Widget>, i32)>>,
    visible: std::cell::Cell<bool>,
}

impl DataColumn {
    pub fn new(title: Option<&str>, factory: Option<&crate::SignalListItemFactory>) -> DataColumn {
        DataColumn(std::rc::Rc::new(DataColumnInner {
            title: title.unwrap_or_default().to_owned(),
            factory: factory.cloned(),
            view: Default::default(),
            visible: std::cell::Cell::new(true),
        }))
    }
    /// Awra 1 gave its GtkColumnViewColumn; here the column itself.
    pub fn delegate(&self) -> DataColumn {
        self.clone()
    }
    pub fn is_visible(&self) -> bool {
        self.0.visible.get()
    }
    pub fn set_visible(&self, v: bool) {
        self.0.visible.set(v);
        if let Some((view, index)) = self.0.view.borrow().as_ref().and_then(|(w, i)| w.upgrade().map(|w| (w, *i))) {
            unsafe { ffi::awra_data_view_set_column_visible(view.as_ptr(), index, v) }
        }
    }
    pub fn set_expand(&self, _e: bool) {}
    pub fn set_fixed_width(&self, _w: i32) {}
    pub fn set_resizable(&self, _r: bool) {}
}

awra_wrapper!(
    /// Rows in columns with their titles (Awra 1's AwraDataView).
    DataView, ["AwraBox"]
);
impl DataView {
    pub fn new(model: Option<&crate::NoSelection>) -> DataView {
        let v = unsafe { DataView::from_full(ffi::awra_data_view_new()) };
        if let Some(m) = model {
            v.delegate().set_model(Some(m));
        }
        v
    }
    pub fn delegate(&self) -> crate::ListView {
        unsafe { Widget::from_none(ffi::awra_data_view_list(ptr!(self))) }.downcast().expect("a list view")
    }
    pub fn append_column(&self, column: &DataColumn) {
        let factory = column.0.factory.clone().unwrap_or_default();
        let index = crate::compat::add_data_column(self, &column.0.title, factory);
        *column.0.view.borrow_mut() = Some((self.0.downgrade(), index));
        if !column.is_visible() {
            column.set_visible(false);
        }
    }
    pub fn set_state(&self, s: DataViewState) {
        self.0.set_rust_data("data-state", s);
        unsafe { ffi::awra_data_view_set_state(ptr!(self), s as i32) }
    }
    pub fn state(&self) -> DataViewState {
        self.0.rust_data("data-state").unwrap_or_default()
    }
    pub fn set_empty_widget(&self, w: Option<&impl IsWidget>) {
        unsafe { ffi::awra_data_view_set_empty_widget(ptr!(self), w.map_or(std::ptr::null_mut(), |w| w.widget_ptr())) }
    }
    pub fn set_loading_widget(&self, w: Option<&impl IsWidget>) {
        unsafe { ffi::awra_data_view_set_loading_widget(ptr!(self), w.map_or(std::ptr::null_mut(), |w| w.widget_ptr())) }
    }
    pub fn set_error_widget(&self, w: Option<&impl IsWidget>) {
        unsafe { ffi::awra_data_view_set_error_widget(ptr!(self), w.map_or(std::ptr::null_mut(), |w| w.widget_ptr())) }
    }
}

// ===========================================================================
// Awra 1's layout helpers and roles

/// Awra 1's AwraSurfaceRole.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SurfaceRole {
    Window,
    Content,
    Sidebar,
    Card,
    Toolbar,
    Floating,
    Popover,
    Menu,
    Dialog,
    Tooltip,
    Hud,
}

/// Awra 1's AwraMaterialPreset.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MaterialPreset {
    None,
    Canvas,
    Content,
    Chrome,
    Layer,
    Floating,
    Opaque,
}

impl Material {
    pub fn for_preset(p: MaterialPreset) -> Material {
        match p {
            MaterialPreset::None | MaterialPreset::Content => Material::Content,
            MaterialPreset::Canvas => Material::Canvas,
            MaterialPreset::Chrome => Material::Chrome,
            MaterialPreset::Layer => Material::Layer,
            MaterialPreset::Floating => Material::Floating,
            MaterialPreset::Opaque => Material::Opaque,
        }
    }
}

impl crate::Surface {
    /// A surface for a role (its material and elevation).
    pub fn with_role(role: SurfaceRole) -> crate::Surface {
        let (m, e) = match role {
            SurfaceRole::Window => (Material::Canvas, Elevation::Flat),
            SurfaceRole::Content => (Material::Content, Elevation::Flat),
            SurfaceRole::Sidebar | SurfaceRole::Toolbar => (Material::Chrome, Elevation::Flat),
            SurfaceRole::Card => (Material::Layer, Elevation::Raised),
            SurfaceRole::Floating | SurfaceRole::Popover | SurfaceRole::Menu | SurfaceRole::Tooltip | SurfaceRole::Hud => {
                (Material::Floating, Elevation::Floating)
            }
            SurfaceRole::Dialog => (Material::Floating, Elevation::Modal),
        };
        let s = crate::Surface::new(m);
        s.set_elevation(e);
        s
    }
}

/// Awra 1's AwraLayoutPreset.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LayoutPreset {
    Compact,
    ControlGroup,
    Section,
    Content,
    Page,
    Toolbar,
    Overlay,
}

/// Awra 1's AwraValidationState.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum ValidationState {
    #[default]
    None,
    Success,
    Warning,
    Error,
}

/// A box's spacing from a token.
pub fn set_spacing(widget: &impl IsWidget, s: Spacing) {
    if let Some(b) = widget.upcast_ref().downcast_ref::<crate::Box>() {
        b.set_spacing(crate::spacing(s).max(0));
    }
}

pub fn set_horizontal_margin(widget: &impl IsWidget, s: Spacing) {
    let m = crate::spacing(s).max(0);
    widget.upcast_ref().set_margin_start(m);
    widget.upcast_ref().set_margin_end(m);
}

pub fn set_vertical_margin(widget: &impl IsWidget, s: Spacing) {
    let m = crate::spacing(s).max(0);
    widget.upcast_ref().set_margin_top(m);
    widget.upcast_ref().set_margin_bottom(m);
}

/// Awra 1's layout presets: a box's spacing and margins.
pub fn apply_layout_preset(widget: &impl IsWidget, p: LayoutPreset) {
    let (spacing, margin) = match p {
        LayoutPreset::Compact => (Spacing::Xs, Spacing::None),
        LayoutPreset::ControlGroup => (Spacing::Sm, Spacing::None),
        LayoutPreset::Section => (Spacing::Md, Spacing::None),
        LayoutPreset::Content => (Spacing::Md, Spacing::Md),
        LayoutPreset::Page => (Spacing::Lg, Spacing::Lg),
        LayoutPreset::Toolbar => (Spacing::Sm, Spacing::Sm),
        LayoutPreset::Overlay => (Spacing::Md, Spacing::Md),
    };
    set_spacing(widget, spacing);
    if margin != Spacing::None {
        widget.upcast_ref().set_margin(margin);
    }
}

/// A context menu (right click, long press) on `widget` from a menu model.
pub fn set_context_menu_model(widget: &impl IsWidget, menu: Option<&crate::gio::Menu>) {
    let w = widget.upcast_ref();
    let popover = crate::Popover::from_model(menu);
    popover.set_parent(w);
    let first = w.rust_data::<crate::Popover>("context-popover").is_none();
    w.set_rust_data("context-popover", popover);
    if first {
        let click = crate::GestureClick::new();
        click.set_button(3);
        let weak = w.downgrade();
        click.connect_pressed(move |_, _, x, y| {
            if let Some(w) = weak.upgrade() {
                if let Some(p) = w.rust_data::<crate::Popover>("context-popover") {
                    p.popup_at(&w, x, y);
                }
            }
        });
        w.add_controller(click);
    }
}

/// The popover `set_context_menu_model` made.
pub fn context_menu_popover(widget: &impl IsWidget) -> Option<crate::Popover> {
    widget.upcast_ref().rust_data("context-popover")
}
