//! Accessibility in gtk-rs's shape (`GtkAccessible`): roles, properties,
//! states and relations that assistive technologies read (AT-SPI2 on Linux,
//! UI Automation on Windows). Widgets have their class's role and keep their
//! states and values themselves; these calls refine them.
//!
//! ```no_run
//! # let button = awra::Button::from_icon_name("edit-delete-symbolic");
//! # let entry = awra::Entry::new();
//! # let label = awra::Label::new(Some("Name"));
//! use awra::accessible::{Property, Relation, State};
//! button.update_property(&[Property::Label("Delete")]);
//! entry.update_relation(&[Relation::LabelledBy(&[label.as_ref()])]);
//! button.update_state(&[State::Busy(true)]);
//! ```
//!
//! SPDX-License-Identifier: GPL-3.0-or-later

use crate::ffi;
use crate::object::{cstring, from_cstr};
use crate::{Orientation, Widget};
use std::ffi::c_void;

macro_rules! a11y_enum {
    ($(#[$m:meta])* $name:ident { $($v:ident = $n:expr),+ $(,)? }) => {
        $(#[$m])*
        #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
        #[repr(i32)]
        #[non_exhaustive]
        pub enum $name { $($v = $n),+ }
        impl $name {
            #[allow(dead_code)]
            fn from_raw(v: i32) -> Option<Self> {
                match v { $($n => Some($name::$v),)+ _ => None }
            }
        }
    };
}

a11y_enum!(
    /// `GtkAccessibleRole`, value for value.
    AccessibleRole {
    Alert = 0, AlertDialog = 1, Banner = 2, Button = 3, Caption = 4, Cell = 5, Checkbox = 6,
    ColumnHeader = 7, ComboBox = 8, Command = 9, Composite = 10, Dialog = 11, Document = 12,
    Feed = 13, Form = 14, Generic = 15, Grid = 16, GridCell = 17, Group = 18, Heading = 19,
    Img = 20, Input = 21, Label = 22, Landmark = 23, Legend = 24, Link = 25, List = 26,
    ListBox = 27, ListItem = 28, Log = 29, Main = 30, Marquee = 31, Math = 32, Meter = 33,
    Menu = 34, MenuBar = 35, MenuItem = 36, MenuItemCheckbox = 37, MenuItemRadio = 38,
    Navigation = 39, None = 40, Note = 41, Option = 42, Presentation = 43, ProgressBar = 44,
    Radio = 45, RadioGroup = 46, Range = 47, Region = 48, Row = 49, RowGroup = 50,
    RowHeader = 51, Scrollbar = 52, Search = 53, SearchBox = 54, Section = 55,
    SectionHead = 56, Select = 57, Separator = 58, Slider = 59, SpinButton = 60, Status = 61,
    Structure = 62, Switch = 63, Tab = 64, Table = 65, TabList = 66, TabPanel = 67,
    TextBox = 68, Time = 69, Timer = 70, Toolbar = 71, Tooltip = 72, Tree = 73, TreeGrid = 74,
    TreeItem = 75, Widget = 76, Window = 77, ToggleButton = 78, Application = 79,
    Paragraph = 80, BlockQuote = 81, Article = 82, Comment = 83, Terminal = 84,
});

a11y_enum!(
    /// `GtkAccessibleTristate`.
    AccessibleTristate { False = 0, True = 1, Mixed = 2 });
a11y_enum!(
    /// `GtkAccessibleInvalidState`.
    AccessibleInvalidState { False = 0, True = 1, Grammar = 2, Spelling = 3 });
a11y_enum!(
    /// `GtkAccessibleAutocomplete`.
    AccessibleAutocomplete { None = 0, Inline = 1, List = 2, Both = 3 });
a11y_enum!(
    /// `GtkAccessibleSort`.
    AccessibleSort { None = 0, Ascending = 1, Descending = 2, Other = 3 });
a11y_enum!(
    /// `GtkAccessibleAnnouncementPriority`.
    AccessibleAnnouncementPriority { Low = 0, Medium = 1, High = 2 });

impl From<bool> for AccessibleTristate {
    fn from(v: bool) -> Self {
        if v {
            AccessibleTristate::True
        } else {
            AccessibleTristate::False
        }
    }
}

/// `gtk::accessible::Property`: what the widget is called and holds.
#[derive(Debug, Clone, Copy)]
#[non_exhaustive]
pub enum Property<'a> {
    Autocomplete(AccessibleAutocomplete),
    Description(&'a str),
    HasPopup(bool),
    KeyShortcuts(&'a str),
    Label(&'a str),
    Level(i32),
    Modal(bool),
    MultiLine(bool),
    MultiSelectable(bool),
    Orientation(Orientation),
    Placeholder(&'a str),
    ReadOnly(bool),
    Required(bool),
    RoleDescription(&'a str),
    Sort(AccessibleSort),
    ValueMax(f64),
    ValueMin(f64),
    ValueNow(f64),
    ValueText(&'a str),
    HelpText(&'a str),
}

/// `gtk::accessible::State`: what the widget is now. `Expanded(None)` /
/// `Selected(None)`: it can't be expanded / selected.
#[derive(Debug, Clone, Copy)]
#[non_exhaustive]
pub enum State {
    Busy(bool),
    Checked(AccessibleTristate),
    Disabled(bool),
    Expanded(Option<bool>),
    Hidden(bool),
    Invalid(AccessibleInvalidState),
    Pressed(AccessibleTristate),
    Selected(Option<bool>),
    Visited(bool),
}

/// `gtk::accessible::Relation`: links to other widgets (kept weakly: a
/// widget that goes leaves the relation) and positions in sets.
#[derive(Debug, Clone, Copy)]
#[non_exhaustive]
pub enum Relation<'a> {
    ActiveDescendant(&'a Widget),
    ColCount(i32),
    ColIndex(i32),
    ColIndexText(&'a str),
    ColSpan(i32),
    Controls(&'a [&'a Widget]),
    DescribedBy(&'a [&'a Widget]),
    Details(&'a [&'a Widget]),
    ErrorMessage(&'a [&'a Widget]),
    FlowTo(&'a [&'a Widget]),
    LabelledBy(&'a [&'a Widget]),
    Owns(&'a [&'a Widget]),
    PosInSet(i32),
    RowCount(i32),
    RowIndex(i32),
    RowIndexText(&'a str),
    RowSpan(i32),
    SetSize(i32),
    LabelFor(&'a [&'a Widget]),
    DescriptionFor(&'a [&'a Widget]),
    ControlledBy(&'a [&'a Widget]),
    DetailsFor(&'a [&'a Widget]),
    ErrorMessageFor(&'a [&'a Widget]),
    FlowFrom(&'a [&'a Widget]),
}

/// The attribute a property, state or relation sets (for `reset_*`).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
#[non_exhaustive]
pub enum PropertyKind {
    Autocomplete = 0,
    Description,
    HasPopup,
    KeyShortcuts,
    Label,
    Level,
    Modal,
    MultiLine,
    MultiSelectable,
    Orientation,
    Placeholder,
    ReadOnly,
    Required,
    RoleDescription,
    Sort,
    ValueMax,
    ValueMin,
    ValueNow,
    ValueText,
    HelpText,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
#[non_exhaustive]
pub enum StateKind {
    Busy = 0,
    Checked,
    Disabled,
    Expanded,
    Hidden,
    Invalid,
    Pressed,
    Selected,
    Visited,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
#[non_exhaustive]
pub enum RelationKind {
    ActiveDescendant = 0,
    ColCount,
    ColIndex,
    ColIndexText,
    ColSpan,
    Controls,
    DescribedBy,
    Details,
    ErrorMessage,
    FlowTo,
    LabelledBy,
    Owns,
    PosInSet,
    RowCount,
    RowIndex,
    RowIndexText,
    RowSpan,
    SetSize,
    LabelFor,
    DescriptionFor,
    ControlledBy,
    DetailsFor,
    ErrorMessageFor,
    FlowFrom,
}

fn set_str(w: &Widget, p: PropertyKind, v: &str) {
    let c = cstring(v);
    unsafe { ffi::awra_accessible_update_property_string(w.as_ptr(), p as i32, c.as_ptr()) }
}

fn set_num(w: &Widget, p: PropertyKind, v: f64) {
    unsafe { ffi::awra_accessible_update_property_number(w.as_ptr(), p as i32, v) }
}

fn set_refs(w: &Widget, r: RelationKind, targets: &[&Widget]) {
    let mut v: Vec<*mut ffi::AwraWidget> = targets.iter().map(|t| t.as_ptr()).collect();
    unsafe {
        ffi::awra_accessible_update_relation(w.as_ptr(), r as i32, v.as_mut_ptr(), v.len() as i32)
    }
}

impl Widget {
    /// `gtk_accessible_update_property`.
    pub fn update_property(&self, props: &[Property]) {
        use Property as P;
        use PropertyKind as K;
        for p in props {
            match *p {
                P::Autocomplete(v) => set_num(self, K::Autocomplete, v as i32 as f64),
                P::Description(s) => set_str(self, K::Description, s),
                P::HasPopup(b) => set_num(self, K::HasPopup, b as i32 as f64),
                P::KeyShortcuts(s) => set_str(self, K::KeyShortcuts, s),
                P::Label(s) => set_str(self, K::Label, s),
                P::Level(n) => set_num(self, K::Level, n as f64),
                P::Modal(b) => set_num(self, K::Modal, b as i32 as f64),
                P::MultiLine(b) => set_num(self, K::MultiLine, b as i32 as f64),
                P::MultiSelectable(b) => set_num(self, K::MultiSelectable, b as i32 as f64),
                P::Orientation(o) => set_num(self, K::Orientation, o as i32 as f64),
                P::Placeholder(s) => set_str(self, K::Placeholder, s),
                P::ReadOnly(b) => set_num(self, K::ReadOnly, b as i32 as f64),
                P::Required(b) => set_num(self, K::Required, b as i32 as f64),
                P::RoleDescription(s) => set_str(self, K::RoleDescription, s),
                P::Sort(v) => set_num(self, K::Sort, v as i32 as f64),
                P::ValueMax(v) => set_num(self, K::ValueMax, v),
                P::ValueMin(v) => set_num(self, K::ValueMin, v),
                P::ValueNow(v) => set_num(self, K::ValueNow, v),
                P::ValueText(s) => set_str(self, K::ValueText, s),
                P::HelpText(s) => set_str(self, K::HelpText, s),
            }
        }
    }

    /// `gtk_accessible_reset_property`: back to what the widget says.
    pub fn reset_property(&self, property: PropertyKind) {
        unsafe { ffi::awra_accessible_reset_property(self.as_ptr(), property as i32) }
    }

    /// `gtk_accessible_update_state`.
    pub fn update_state(&self, states: &[State]) {
        use State as S;
        use StateKind as K;
        let set =
            |k: K, v: i32| unsafe { ffi::awra_accessible_update_state(self.as_ptr(), k as i32, v) };
        let reset = |k: K| unsafe { ffi::awra_accessible_reset_state(self.as_ptr(), k as i32) };
        for s in states {
            match *s {
                S::Busy(b) => set(K::Busy, b as i32),
                S::Checked(t) => set(K::Checked, t as i32),
                S::Disabled(b) => set(K::Disabled, b as i32),
                S::Expanded(Some(b)) => set(K::Expanded, b as i32),
                S::Expanded(None) => reset(K::Expanded),
                S::Hidden(b) => set(K::Hidden, b as i32),
                S::Invalid(v) => set(K::Invalid, v as i32),
                S::Pressed(t) => set(K::Pressed, t as i32),
                S::Selected(Some(b)) => set(K::Selected, b as i32),
                S::Selected(None) => reset(K::Selected),
                S::Visited(b) => set(K::Visited, b as i32),
            }
        }
    }

    /// `gtk_accessible_reset_state`.
    pub fn reset_state(&self, state: StateKind) {
        unsafe { ffi::awra_accessible_reset_state(self.as_ptr(), state as i32) }
    }

    /// `gtk_accessible_update_relation`.
    pub fn update_relation(&self, relations: &[Relation]) {
        use Relation as R;
        use RelationKind as K;
        let num = |k: K, v: i32| unsafe {
            ffi::awra_accessible_update_relation_number(self.as_ptr(), k as i32, v)
        };
        let text = |k: K, s: &str| {
            let c = cstring(s);
            unsafe {
                ffi::awra_accessible_update_relation_string(self.as_ptr(), k as i32, c.as_ptr())
            }
        };
        for r in relations {
            match *r {
                R::ActiveDescendant(w) => set_refs(self, K::ActiveDescendant, &[w]),
                R::ColCount(n) => num(K::ColCount, n),
                R::ColIndex(n) => num(K::ColIndex, n),
                R::ColIndexText(s) => text(K::ColIndexText, s),
                R::ColSpan(n) => num(K::ColSpan, n),
                R::Controls(v) => set_refs(self, K::Controls, v),
                R::DescribedBy(v) => set_refs(self, K::DescribedBy, v),
                R::Details(v) => set_refs(self, K::Details, v),
                R::ErrorMessage(v) => set_refs(self, K::ErrorMessage, v),
                R::FlowTo(v) => set_refs(self, K::FlowTo, v),
                R::LabelledBy(v) => set_refs(self, K::LabelledBy, v),
                R::Owns(v) => set_refs(self, K::Owns, v),
                R::PosInSet(n) => num(K::PosInSet, n),
                R::RowCount(n) => num(K::RowCount, n),
                R::RowIndex(n) => num(K::RowIndex, n),
                R::RowIndexText(s) => text(K::RowIndexText, s),
                R::RowSpan(n) => num(K::RowSpan, n),
                R::SetSize(n) => num(K::SetSize, n),
                R::LabelFor(v) => set_refs(self, K::LabelFor, v),
                R::DescriptionFor(v) => set_refs(self, K::DescriptionFor, v),
                R::ControlledBy(v) => set_refs(self, K::ControlledBy, v),
                R::DetailsFor(v) => set_refs(self, K::DetailsFor, v),
                R::ErrorMessageFor(v) => set_refs(self, K::ErrorMessageFor, v),
                R::FlowFrom(v) => set_refs(self, K::FlowFrom, v),
            }
        }
    }

    /// `gtk_accessible_reset_relation`.
    pub fn reset_relation(&self, relation: RelationKind) {
        unsafe { ffi::awra_accessible_reset_relation(self.as_ptr(), relation as i32) }
    }

    /// The `accessible-role` property (gtk-rs sets it at construction; any
    /// time here).
    pub fn set_accessible_role(&self, role: AccessibleRole) {
        unsafe { ffi::awra_accessible_set_role(self.as_ptr(), role as i32) }
    }

    /// `gtk_accessible_get_accessible_role`: the explicit role, else the
    /// class's.
    pub fn accessible_role(&self) -> AccessibleRole {
        AccessibleRole::from_raw(unsafe { ffi::awra_accessible_get_role(self.as_ptr()) })
            .unwrap_or(AccessibleRole::Generic)
    }

    /// The name assistive technologies read (GTK's naming rules).
    pub fn accessible_name(&self) -> String {
        unsafe { take_string(ffi::awra_accessible_get_name(self.as_ptr())) }
    }

    /// The description assistive technologies read.
    pub fn accessible_description(&self) -> String {
        unsafe { take_string(ffi::awra_accessible_get_description(self.as_ptr())) }
    }

    /// A state's value as assistive technologies see it: `None` when the
    /// widget does not have the state.
    pub fn accessible_state(&self, state: StateKind) -> Option<i32> {
        let v = unsafe { ffi::awra_accessible_get_state(self.as_ptr(), state as i32) };
        (v >= 0).then_some(v)
    }

    /// `gtk_accessible_announce`: a screen reader reads the message.
    pub fn announce(&self, message: &str, priority: AccessibleAnnouncementPriority) {
        let c = cstring(message);
        unsafe { ffi::awra_accessible_announce(self.as_ptr(), c.as_ptr(), priority as i32) }
    }
}

/// An assistive technology is connected: the accessibility tree is exposed.
pub fn is_active() -> bool {
    unsafe { ffi::awra_accessible_active() }
}

unsafe fn take_string(p: *mut std::ffi::c_char) -> String {
    let s = from_cstr(p);
    if !p.is_null() {
        ffi::awra_free(p as *mut c_void);
    }
    s
}
