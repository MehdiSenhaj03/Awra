//! Menus built from closures: what an application shows on a right click
//! (a message's Reply, Copy, Delete…), each item its own callback, without
//! the `win.*` actions a menu model needs.
//!
//! ```no_run
//! # let anchor = awra::Label::new(None);
//! awra::menu::popup_at(&anchor, 10.0, 10.0, vec![
//!     awra::menu::Entry::item("Reply", || println!("reply")).icon("mail-reply-sender-symbolic"),
//!     awra::menu::Entry::separator(),
//!     awra::menu::Entry::item("Delete", || println!("delete")).destructive(),
//! ]);
//! ```
//!
//! SPDX-License-Identifier: GPL-3.0-or-later

use std::ffi::{c_void, CString};
use std::rc::Rc;

use crate::object::{cstring, IsWidget};
use crate::{ffi, Popover};

/// One row of a menu.
pub struct Entry {
    kind: Kind,
    label: String,
    icon: Option<String>,
    accel: Option<String>,
    check: Option<bool>,
    sensitive: bool,
    destructive: bool,
    action: Option<Rc<dyn Fn()>>,
}

enum Kind {
    Item,
    Separator,
    Heading,
    Submenu(Vec<Entry>),
    SubmenuEnd,
}

impl Entry {
    /// An item that calls `action` when chosen (after the menu closes).
    pub fn item(label: impl Into<String>, action: impl Fn() + 'static) -> Entry {
        Entry {
            kind: Kind::Item,
            label: label.into(),
            icon: None,
            accel: None,
            check: None,
            sensitive: true,
            destructive: false,
            action: Some(Rc::new(action)),
        }
    }
    /// A line between groups of items.
    pub fn separator() -> Entry {
        Entry {
            kind: Kind::Separator,
            label: String::new(),
            icon: None,
            accel: None,
            check: None,
            sensitive: true,
            destructive: false,
            action: None,
        }
    }
    /// A section's title.
    pub fn heading(label: impl Into<String>) -> Entry {
        Entry {
            kind: Kind::Heading,
            ..Entry::separator()
        }
        .with_label(label)
    }
    /// A row that opens `entries` beside the menu.
    pub fn submenu(label: impl Into<String>, entries: Vec<Entry>) -> Entry {
        Entry {
            kind: Kind::Submenu(entries),
            ..Entry::separator()
        }
        .with_label(label)
    }
    fn with_label(mut self, label: impl Into<String>) -> Entry {
        self.label = label.into();
        self
    }
    pub fn icon(mut self, icon: impl Into<String>) -> Entry {
        self.icon = Some(icon.into());
        self
    }
    /// The shortcut shown on the right ("Ctrl+C").
    pub fn accel(mut self, accel: impl Into<String>) -> Entry {
        self.accel = Some(accel.into());
        self
    }
    /// A check mark, on or off.
    pub fn checked(mut self, on: bool) -> Entry {
        self.check = Some(on);
        self
    }
    pub fn sensitive(mut self, sensitive: bool) -> Entry {
        self.sensitive = sensitive;
        self
    }
    /// Drawn as a destructive choice (Delete, Leave).
    pub fn destructive(mut self) -> Entry {
        self.destructive = true;
        self
    }
}

/// The callbacks of one menu, by item index; freed with the popover.
struct Actions(Vec<Option<Rc<dyn Fn()>>>);

unsafe extern "C" fn activated(_w: *mut ffi::AwraWidget, args: *const ffi::AwraArgs, data: *mut c_void) {
    let actions = &*(data as *const Actions);
    let index = (*args).index.max(0) as usize;
    if let Some(Some(action)) = actions.0.get(index) {
        let action = action.clone();
        // after the menu has closed and its row's handler returned
        crate::glib::idle_add_local_once(move || action());
    }
}

fn flatten(entries: Vec<Entry>, out: &mut Vec<Entry>) {
    for mut entry in entries {
        if let Kind::Submenu(children) = std::mem::replace(&mut entry.kind, Kind::Item) {
            entry.kind = Kind::Submenu(Vec::new());
            entry.action = None;
            out.push(entry);
            flatten(children, out);
            out.push(Entry {
                kind: Kind::SubmenuEnd,
                ..Entry::separator()
            });
        } else {
            out.push(entry);
        }
    }
}

/// A menu popover of `entries`; show it with [`Popover::popup_at`].
pub fn build(entries: Vec<Entry>) -> Popover {
    let mut flat = Vec::new();
    flatten(entries, &mut flat);
    let actions = Box::new(Actions(flat.iter().map(|e| e.action.clone()).collect()));
    let data = &*actions as *const Actions as *mut c_void;
    let mut keep: Vec<CString> = Vec::new();
    let mut text = |s: Option<&str>| -> *const std::os::raw::c_char {
        match s {
            Some(s) => {
                keep.push(cstring(s));
                keep.last().map_or(std::ptr::null(), |c| c.as_ptr())
            }
            None => std::ptr::null(),
        }
    };
    let items: Vec<ffi::AwraMenuItem> = flat
        .iter()
        .map(|e| {
            let mut it: ffi::AwraMenuItem = unsafe { std::mem::zeroed() };
            it.sensitive = e.sensitive;
            it.destructive = e.destructive;
            match &e.kind {
                Kind::Separator => {
                    it.check = -1;
                }
                Kind::SubmenuEnd => {
                    it.check = ffi::AWRA_MENU_ITEM_SUBMENU_END;
                }
                Kind::Heading => {
                    it.label = text(Some(&e.label));
                    it.check = -2;
                }
                Kind::Submenu(_) => {
                    it.label = text(Some(&e.label));
                    it.icon_name = text(e.icon.as_deref());
                    it.check = ffi::AWRA_MENU_ITEM_SUBMENU;
                }
                Kind::Item => {
                    it.label = text(Some(&e.label));
                    it.icon_name = text(e.icon.as_deref());
                    it.accel = text(e.accel.as_deref());
                    it.check = e.check.map_or(-1, i32::from);
                    it.handler = Some(activated);
                    it.data = data;
                }
            }
            it
        })
        .collect();
    let popover = unsafe { Popover::from_full(ffi::awra_menu_popover_new(items.as_ptr(), items.len() as i32)) };
    drop(keep);
    popover.set_rust_data("menu-actions", Rc::new(actions));
    popover
}

/// Builds a menu of `entries` and shows it at (`x`, `y`) of `relative_to`
/// (where the pointer was, for a context menu).
pub fn popup_at(relative_to: &impl IsWidget, x: f64, y: f64, entries: Vec<Entry>) -> Popover {
    let popover = build(entries);
    popover.popup_at(relative_to, x, y);
    popover
}
