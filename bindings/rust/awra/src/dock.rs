//! Dock: a workspace the user (or a program, or an AI) rearranges.
//!
//! Panels are registered once with an id, a title and a widget; a layout
//! tree of splits and tab groups places them. The tree is plain data
//! ([`DockNode`]) with a compact text form (`h(0.72,timeline,v(0.5,lyrics,
//! assistant))`, `t(0,a,b)`), so it can be saved, restored, edited by
//! code ([`Dock::move_panel`], [`Dock::resize_panel`], [`Dock::hide`]…) or
//! by hand: in edit mode each panel shows a small bar to move, resize or
//! hide it.
//!
//! SPDX-License-Identifier: GPL-3.0-or-later

use std::cell::RefCell;
use std::rc::Rc;

use crate::object::IsWidget;
use crate::{Orientation, Widget};

/// Where a panel goes relative to the whole workspace.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DockArea {
    Left,
    Right,
    Top,
    Bottom,
    /// As a tab beside the largest panel.
    Center,
}

impl DockArea {
    pub fn parse(s: &str) -> Option<DockArea> {
        Some(match s.trim().to_lowercase().as_str() {
            "left" | "gauche" | "start" => DockArea::Left,
            "right" | "droite" | "end" => DockArea::Right,
            "top" | "haut" => DockArea::Top,
            "bottom" | "bas" | "below" | "dessous" => DockArea::Bottom,
            "center" | "centre" | "middle" | "tab" => DockArea::Center,
            _ => return None,
        })
    }
}

/// The layout tree.
#[derive(Clone, Debug, PartialEq)]
pub enum DockNode {
    Panel(String),
    /// Two children side by side (`vertical`: one above the other); the
    /// first takes `fraction` of the room.
    Split { vertical: bool, fraction: f64, a: Box<DockNode>, b: Box<DockNode> },
    Tabs { panels: Vec<String>, active: usize },
}

impl DockNode {
    pub fn panels(&self) -> Vec<String> {
        match self {
            DockNode::Panel(p) => vec![p.clone()],
            DockNode::Split { a, b, .. } => {
                let mut v = a.panels();
                v.extend(b.panels());
                v
            }
            DockNode::Tabs { panels, .. } => panels.clone(),
        }
    }
    pub fn contains(&self, id: &str) -> bool {
        self.panels().iter().any(|p| p == id)
    }

    /// The tree without `id` (None when nothing is left).
    pub fn without(self, id: &str) -> Option<DockNode> {
        match self {
            DockNode::Panel(p) if p == id => None,
            DockNode::Panel(p) => Some(DockNode::Panel(p)),
            DockNode::Split { vertical, fraction, a, b } => match (a.without(id), b.without(id)) {
                (Some(a), Some(b)) => Some(DockNode::Split { vertical, fraction, a: Box::new(a), b: Box::new(b) }),
                (Some(x), None) | (None, Some(x)) => Some(x),
                (None, None) => None,
            },
            DockNode::Tabs { mut panels, active } => {
                panels.retain(|p| p != id);
                match panels.len() {
                    0 => None,
                    1 => Some(DockNode::Panel(panels.remove(0))),
                    n => Some(DockNode::Tabs { panels, active: active.min(n - 1) }),
                }
            }
        }
    }

    /// The text form: `h(0.7,a,b)`, `v(0.5,a,b)`, `t(0,a,b,c)`, `a`.
    pub fn to_text(&self) -> String {
        match self {
            DockNode::Panel(p) => p.clone(),
            DockNode::Split { vertical, fraction, a, b } => {
                format!("{}({:.3},{},{})", if *vertical { 'v' } else { 'h' }, fraction, a.to_text(), b.to_text())
            }
            DockNode::Tabs { panels, active } => format!("t({},{})", active, panels.join(",")),
        }
    }

    pub fn parse(s: &str) -> Option<DockNode> {
        let s: String = s.chars().filter(|c| !c.is_whitespace()).collect();
        let (n, rest) = parse_node(&s)?;
        rest.is_empty().then_some(n)
    }

    /// Changes the share of the split nearest to `id` (factor > 1 grows it).
    fn resize(&mut self, id: &str, factor: f64) -> bool {
        match self {
            DockNode::Split { fraction, a, b, .. } => {
                if a.resize(id, factor) || b.resize(id, factor) {
                    return true;
                }
                let in_a = a.contains(id);
                let in_b = b.contains(id);
                if !in_a && !in_b {
                    return false;
                }
                let share = if in_a { *fraction } else { 1.0 - *fraction };
                let new = (share * factor).clamp(0.12, 0.88);
                *fraction = if in_a { new } else { 1.0 - new };
                true
            }
            _ => false,
        }
    }

    fn set_fraction(&mut self, path: &[bool], f: f64) {
        if let DockNode::Split { fraction, a, b, .. } = self {
            match path.split_first() {
                None => *fraction = f,
                Some((first, rest)) => {
                    if *first { b } else { a }.set_fraction(rest, f);
                }
            }
        }
    }

    fn set_active(&mut self, path: &[bool], i: usize) {
        match (self, path.split_first()) {
            (DockNode::Tabs { active, .. }, None) => *active = i,
            (DockNode::Split { a, b, .. }, Some((first, rest))) => {
                if *first { b } else { a }.set_active(rest, i);
            }
            _ => {}
        }
    }

    /// The largest panel (by area share), for tabs added at the centre.
    fn largest(&self) -> Option<(String, f64)> {
        match self {
            DockNode::Panel(p) => Some((p.clone(), 1.0)),
            DockNode::Tabs { panels, active } => panels.get(*active).or(panels.first()).map(|p| (p.clone(), 1.0)),
            DockNode::Split { fraction, a, b, .. } => {
                let x = a.largest().map(|(p, s)| (p, s * fraction));
                let y = b.largest().map(|(p, s)| (p, s * (1.0 - fraction)));
                match (x, y) {
                    (Some(x), Some(y)) => Some(if x.1 >= y.1 { x } else { y }),
                    (x, y) => x.or(y),
                }
            }
        }
    }

    fn add_tab_beside(self, target: &str, id: &str) -> DockNode {
        match self {
            DockNode::Panel(p) if p == target => {
                DockNode::Tabs { panels: vec![p, id.to_string()], active: 1 }
            }
            DockNode::Tabs { mut panels, .. } if panels.iter().any(|p| p == target) => {
                panels.push(id.to_string());
                let n = panels.len();
                DockNode::Tabs { panels, active: n - 1 }
            }
            DockNode::Split { vertical, fraction, a, b } => DockNode::Split {
                vertical,
                fraction,
                a: Box::new(a.add_tab_beside(target, id)),
                b: Box::new(b.add_tab_beside(target, id)),
            },
            other => other,
        }
    }
}

fn parse_node(s: &str) -> Option<(DockNode, &str)> {
    let open = s.find('(');
    let stop = s.find([',', ')']).unwrap_or(s.len());
    match open {
        Some(o) if o < stop => {
            let kind = &s[..o];
            let mut rest = &s[o + 1..];
            let num_end = rest.find(',')?;
            let num = &rest[..num_end];
            rest = &rest[num_end + 1..];
            match kind {
                "h" | "v" => {
                    let fraction: f64 = num.parse().ok()?;
                    let (a, r) = parse_node(rest)?;
                    let r = r.strip_prefix(',')?;
                    let (b, r) = parse_node(r)?;
                    let r = r.strip_prefix(')')?;
                    Some((
                        DockNode::Split {
                            vertical: kind == "v",
                            fraction: fraction.clamp(0.05, 0.95),
                            a: Box::new(a),
                            b: Box::new(b),
                        },
                        r,
                    ))
                }
                "t" => {
                    let active: usize = num.parse().ok()?;
                    let close = rest.find(')')?;
                    let panels: Vec<String> = rest[..close].split(',').filter(|p| !p.is_empty()).map(String::from).collect();
                    if panels.is_empty() {
                        return None;
                    }
                    let n = panels.len();
                    Some((DockNode::Tabs { panels, active: active.min(n - 1) }, &rest[close + 1..]))
                }
                _ => None,
            }
        }
        _ => {
            let id = &s[..stop];
            if id.is_empty() {
                return None;
            }
            Some((DockNode::Panel(id.to_string()), &s[stop..]))
        }
    }
}

struct Panel {
    id: String,
    title: String,
    content: Widget,
    frame: crate::Box,
    bar: crate::Box,
}

struct State {
    tree: Option<DockNode>,
    panels: Vec<Panel>,
    editing: bool,
    changed: Vec<Rc<dyn Fn(&Dock)>>,
}

/// The workspace widget.
#[derive(Clone)]
pub struct Dock {
    root: crate::Box,
    state: Rc<RefCell<State>>,
}

impl AsRef<Widget> for Dock {
    fn as_ref(&self) -> &Widget {
        self.root.as_ref()
    }
}

impl Default for Dock {
    fn default() -> Self {
        Dock::new()
    }
}

fn icon_button(icon: &str, tip: &str) -> crate::Button {
    let b = crate::Button::from_icon_name(icon);
    b.set_appearance(crate::ButtonAppearance::Ghost);
    b.set_tooltip_text(Some(tip));
    b
}

impl Dock {
    pub fn new() -> Dock {
        let root = crate::Box::new(Orientation::Vertical, 0);
        root.set_hexpand(true);
        root.set_vexpand(true);
        Dock { root, state: Rc::new(RefCell::new(State { tree: None, panels: Vec::new(), editing: false, changed: Vec::new() })) }
    }

    /// Registers a panel (not shown until a layout places it).
    pub fn add_panel(&self, id: &str, title: &str, content: &impl IsWidget) {
        let content: Widget = unsafe { Widget::from_none(content.widget_ptr()) };
        let frame = crate::Box::new(Orientation::Vertical, 0);
        frame.set_hexpand(true);
        frame.set_vexpand(true);
        let bar = crate::Box::new(Orientation::Horizontal, 2);
        bar.add_css_class("dock-bar");
        bar.set_margin_start(6);
        bar.set_margin_end(4);
        let title_l = crate::Label::new(Some(title));
        title_l.set_typography(crate::Typography::Label);
        title_l.set_hexpand(true);
        title_l.set_xalign(0.0);
        bar.append(&title_l);
        let actions: [(&str, &str, Box<dyn Fn(&Dock, &str)>); 6] = [
            ("pan-start-symbolic", "À gauche", Box::new(|d, id| d.move_panel(id, DockArea::Left))),
            ("pan-down-symbolic", "En bas", Box::new(|d, id| d.move_panel(id, DockArea::Bottom))),
            ("pan-end-symbolic", "À droite", Box::new(|d, id| d.move_panel(id, DockArea::Right))),
            ("zoom-in-symbolic", "Agrandir", Box::new(|d, id| d.resize_panel(id, 1.25))),
            ("zoom-out-symbolic", "Réduire", Box::new(|d, id| d.resize_panel(id, 0.8))),
            ("window-close-symbolic", "Masquer", Box::new(|d, id| d.hide(id))),
        ];
        for (icon, tip, f) in actions {
            let b = icon_button(icon, tip);
            let weak = Rc::downgrade(&self.state);
            let root = self.root.clone();
            let pid = id.to_string();
            b.connect_clicked(move |_| {
                if let Some(state) = weak.upgrade() {
                    let d = Dock { root: root.clone(), state };
                    f(&d, &pid);
                }
            });
            bar.append(&b);
        }
        bar.set_visible(false);
        frame.append(&bar);
        content.set_hexpand(true);
        content.set_vexpand(true);
        frame.append(&content);
        self.state.borrow_mut().panels.push(Panel { id: id.to_string(), title: title.to_string(), content, frame, bar });
    }

    pub fn panel_ids(&self) -> Vec<(String, String)> {
        self.state.borrow().panels.iter().map(|p| (p.id.clone(), p.title.clone())).collect()
    }

    pub fn panel_widget(&self, id: &str) -> Option<Widget> {
        self.state.borrow().panels.iter().find(|p| p.id == id).map(|p| p.content.clone())
    }

    pub fn layout(&self) -> Option<DockNode> {
        self.state.borrow().tree.clone()
    }

    pub fn is_shown(&self, id: &str) -> bool {
        self.state.borrow().tree.as_ref().is_some_and(|t| t.contains(id))
    }

    /// Calls `f` after every change of the layout (to save it).
    pub fn connect_changed<F: Fn(&Dock) + 'static>(&self, f: F) {
        self.state.borrow_mut().changed.push(Rc::new(f));
    }

    fn emit_changed(&self) {
        let hs = self.state.borrow().changed.clone();
        for h in hs {
            h(self);
        }
    }

    /// Replaces the layout (unknown panels are dropped).
    pub fn set_layout(&self, tree: DockNode) {
        let known: Vec<String> = self.state.borrow().panels.iter().map(|p| p.id.clone()).collect();
        let mut t = Some(tree);
        for id in t.as_ref().map(|t| t.panels()).unwrap_or_default() {
            if !known.contains(&id) {
                t = t.and_then(|t| t.without(&id));
            }
        }
        self.state.borrow_mut().tree = t;
        self.rebuild();
        self.emit_changed();
    }

    pub fn show(&self, id: &str) {
        if self.is_shown(id) {
            return;
        }
        self.move_panel(id, DockArea::Right);
    }

    pub fn hide(&self, id: &str) {
        let t = self.state.borrow_mut().tree.take();
        self.state.borrow_mut().tree = t.and_then(|t| t.without(id));
        self.rebuild();
        self.emit_changed();
    }

    /// Moves (or shows) a panel to an area of the workspace.
    pub fn move_panel(&self, id: &str, area: DockArea) {
        if !self.state.borrow().panels.iter().any(|p| p.id == id) {
            return;
        }
        let rest = self.state.borrow_mut().tree.take().and_then(|t| t.without(id));
        let p = DockNode::Panel(id.to_string());
        let tree = match rest {
            None => p,
            Some(rest) => match area {
                DockArea::Left => DockNode::Split { vertical: false, fraction: 0.28, a: Box::new(p), b: Box::new(rest) },
                DockArea::Right => DockNode::Split { vertical: false, fraction: 0.72, a: Box::new(rest), b: Box::new(p) },
                DockArea::Top => DockNode::Split { vertical: true, fraction: 0.3, a: Box::new(p), b: Box::new(rest) },
                DockArea::Bottom => DockNode::Split { vertical: true, fraction: 0.68, a: Box::new(rest), b: Box::new(p) },
                DockArea::Center => match rest.largest() {
                    Some((target, _)) => rest.add_tab_beside(&target, id),
                    None => p,
                },
            },
        };
        self.state.borrow_mut().tree = Some(tree);
        self.rebuild();
        self.emit_changed();
    }

    /// Grows (factor > 1) or shrinks a panel's share.
    pub fn resize_panel(&self, id: &str, factor: f64) {
        let changed = self.state.borrow_mut().tree.as_mut().is_some_and(|t| t.resize(id, factor));
        if changed {
            self.rebuild();
            self.emit_changed();
        }
    }

    /// Edit mode: each panel shows its bar (move, resize, hide).
    pub fn set_editing(&self, editing: bool) {
        let mut s = self.state.borrow_mut();
        s.editing = editing;
        for p in &s.panels {
            p.bar.set_visible(editing);
        }
    }

    pub fn is_editing(&self) -> bool {
        self.state.borrow().editing
    }

    fn rebuild(&self) {
        // take every panel out of the old containers
        for c in self.root.children() {
            self.root.remove(&c);
        }
        let s = self.state.borrow();
        for p in &s.panels {
            if p.frame.parent().is_some() {
                p.frame.unparent();
            }
        }
        let Some(tree) = s.tree.clone() else {
            let empty = crate::Label::new(Some("Espace vide : affiche un panneau depuis le menu Interface."));
            empty.add_css_class("dim-label");
            empty.set_vexpand(true);
            self.root.append(&empty);
            return;
        };
        drop(s);
        let w = self.build(&tree, &mut Vec::new());
        w.set_hexpand(true);
        w.set_vexpand(true);
        self.root.append(&w);
    }

    fn build(&self, node: &DockNode, path: &mut Vec<bool>) -> Widget {
        match node {
            DockNode::Panel(id) => {
                let s = self.state.borrow();
                let p = s.panels.iter().find(|p| p.id == *id).expect("known panel");
                p.frame.clone().upcast::<Widget>()
            }
            DockNode::Split { vertical, fraction, a, b } => {
                let paned = crate::Paned::new(if *vertical { Orientation::Vertical } else { Orientation::Horizontal });
                path.push(false);
                let wa = self.build(a, path);
                path.pop();
                path.push(true);
                let wb = self.build(b, path);
                path.pop();
                paned.set_start_child(Some(&wa));
                paned.set_end_child(Some(&wb));
                paned.set_fraction(*fraction);
                let weak = Rc::downgrade(&self.state);
                let my_path = path.clone();
                let root = self.root.clone();
                paned.connect_position_notify(move |p| {
                    if let Some(st) = weak.upgrade() {
                        if let Some(t) = st.borrow_mut().tree.as_mut() {
                            t.set_fraction(&my_path, p.fraction());
                        }
                        let d = Dock { root: root.clone(), state: st };
                        d.emit_changed();
                    }
                });
                paned.upcast::<Widget>()
            }
            DockNode::Tabs { panels, active } => {
                let col = crate::Box::new(Orientation::Vertical, 4);
                let seg = crate::SegmentedControl::new();
                seg.set_halign(crate::Align::Center);
                seg.set_margin_top(4);
                let stack = crate::Stack::new();
                stack.set_vexpand(true);
                {
                    let s = self.state.borrow();
                    for id in panels {
                        if let Some(p) = s.panels.iter().find(|p| p.id == *id) {
                            seg.append(&p.title);
                            stack.add_named(&p.frame, Some(id.as_str()));
                        }
                    }
                }
                seg.set_selected(*active as u32);
                if let Some(id) = panels.get(*active) {
                    stack.set_visible_child_name(id);
                }
                let ids = panels.clone();
                let weak = Rc::downgrade(&self.state);
                let my_path = path.clone();
                let st2 = stack.clone();
                seg.connect_changed(move |_, i| {
                    if let Some(id) = ids.get(i as usize) {
                        st2.set_visible_child_name(id);
                    }
                    if let Some(st) = weak.upgrade() {
                        if let Some(t) = st.borrow_mut().tree.as_mut() {
                            t.set_active(&my_path, i as usize);
                        }
                    }
                });
                col.append(&seg);
                col.append(&stack);
                col.upcast::<Widget>()
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn text_roundtrip_and_edits() {
        let t = DockNode::parse("h(0.72, v(0.6,timeline,effects), t(1,assistant,lyrics))").unwrap();
        assert_eq!(DockNode::parse(&t.to_text()).unwrap(), t);
        assert_eq!(t.panels(), ["timeline", "effects", "assistant", "lyrics"]);
        let t2 = t.clone().without("effects").unwrap();
        assert_eq!(t2.to_text(), "h(0.720,timeline,t(1,assistant,lyrics))");
        let t3 = t2.without("lyrics").unwrap();
        assert_eq!(t3.to_text(), "h(0.720,timeline,assistant)");
        let mut t4 = t3.clone();
        assert!(t4.resize("assistant", 1.5));
        assert_eq!(t4.to_text(), "h(0.580,timeline,assistant)");
        assert!(DockNode::parse("h(0.5,a").is_none());
        assert_eq!(t3.largest().unwrap().0, "timeline");
    }
}
