//! Accessibility in gtk-rs's shape: names from labels, tooltips and
//! relations, explicit properties and states, roles (offscreen; no
//! assistive technology needed).
use awra::accessible::{Property, Relation, State, StateKind};
use awra::prelude::*;
use awra::AccessibleRole;

fn main() {
    let _app = awra::Application::new("org.awra.Accessible");
    let window = awra::Window::offscreen("Settings", 400, 300, 1.0);
    let column = awra::Box::new(awra::Orientation::Vertical, 8);
    let label = awra::Label::new(Some("Name"));
    let entry = awra::Entry::new();
    let delete = awra::Button::from_icon_name("edit-delete-symbolic");
    let level = awra::ProgressBar::new();
    column.append(&label);
    column.append(&entry);
    column.append(&delete);
    column.append(&level);
    window.set_child(Some(&column));

    assert_eq!(window.accessible_role(), AccessibleRole::Window);
    assert_eq!(window.accessible_name(), "Settings");
    assert_eq!(delete.accessible_role(), AccessibleRole::Button);

    // labelled-by: the entry is named by its label
    assert_eq!(entry.accessible_name(), "");
    entry.update_relation(&[Relation::LabelledBy(&[label.upcast_ref()])]);
    assert_eq!(entry.accessible_name(), "Name");

    // an icon button: its tooltip names it, an explicit label wins
    delete.set_tooltip_text(Some("Remove"));
    assert_eq!(delete.accessible_name(), "Remove");
    delete.update_property(&[Property::Label("Delete"), Property::Description("Deletes the file")]);
    assert_eq!(delete.accessible_name(), "Delete");
    assert_eq!(delete.accessible_description(), "Deletes the file");

    // states
    assert_eq!(delete.accessible_state(StateKind::Busy), None);
    delete.update_state(&[State::Busy(true)]);
    assert_eq!(delete.accessible_state(StateKind::Busy), Some(1));
    delete.reset_state(StateKind::Busy);
    assert_eq!(delete.accessible_state(StateKind::Busy), None);

    // roles and the GTK-style label of a level bar (Dawra's input level)
    level.set_accessible_role(AccessibleRole::Meter);
    level.update_property(&[Property::Label("Input level")]);
    assert_eq!(level.accessible_role(), AccessibleRole::Meter);
    assert_eq!(level.accessible_name(), "Input level");

    // relations are weak: the label goes, the entry loses its name
    column.remove(&label);
    drop(label);
    assert_eq!(entry.accessible_name(), "");
    assert!(!awra::accessible::is_active());
    println!("accessible: all passed");
}
