use std::cell::Cell;
use std::rc::Rc;

fn main() {
    let app = awra::Application::new("org.awra.Smoke");
    let window = awra::Window::offscreen("Smoke", 640, 400, 1.0);
    let column = awra::Box::new(awra::Orientation::Vertical, 8);
    let label = awra::Label::new(Some("Hello"));
    let button = awra::Button::with_label("Click");
    button.set_action_name(Some("win.hello"));
    column.append(&label);
    column.append(&button);
    window.set_child(Some(&column));

    let hits = Rc::new(Cell::new(0));
    let action = awra::gio::SimpleAction::new("hello", None);
    {
        let hits = hits.clone();
        action.connect_activate(move |_, _| hits.set(hits.get() + 1));
    }
    window.add_action(&action);
    app.set_accels_for_action("win.hello", &["<Ctrl>h"]);

    button.emit_clicked();
    assert_eq!(hits.get(), 1, "action through the button");
    action.set_enabled(false);
    assert!(!button.is_sensitive());
    action.set_enabled(true);

    let weak = label.downgrade();
    assert!(weak.upgrade().is_some());
    let w2: awra::Widget = column.first_child().unwrap();
    assert!(w2.is::<awra::Label>());
    assert!(!w2.is::<awra::Button>());

    let ticks = Rc::new(Cell::new(0));
    {
        let ticks = ticks.clone();
        awra::glib::timeout_add_local(std::time::Duration::from_millis(5), move || {
            ticks.set(ticks.get() + 1);
            if ticks.get() == 3 {
                awra::glib::ControlFlow::Break
            } else {
                awra::glib::ControlFlow::Continue
            }
        });
    }
    let done = Rc::new(Cell::new(false));
    {
        let done = done.clone();
        awra::glib::spawn_future_local(async move {
            let (tx, rx) = std::sync::mpsc::channel::<u32>();
            std::thread::spawn(move || tx.send(42).unwrap());
            awra::glib::timeout_future(std::time::Duration::from_millis(20)).await;
            assert_eq!(rx.recv().unwrap(), 42);
            let v = awra::glib::spawn_blocking(|| 7).await.unwrap();
            assert_eq!(v, 7);
            done.set(true);
        });
    }
    let start = std::time::Instant::now();
    while (!done.get() || ticks.get() < 3) && start.elapsed().as_secs() < 5 {
        awra::pump();
        std::thread::sleep(std::time::Duration::from_millis(1));
    }
    assert!(done.get(), "future finished");
    assert_eq!(ticks.get(), 3);
    window.render_offscreen(true);
    let menu = awra::gio::Menu::new();
    let section = awra::gio::Menu::new();
    section.append(Some("Hello"), Some("win.hello"));
    menu.append_section(Some("Greetings"), &section);
    let mb = awra::MenuButton::new();
    mb.set_menu_model(Some(&menu));
    column.append(&mb);
    section.append(Some("Again"), Some("win.hello"));
    window.render_offscreen(true);
    drop(label);
    drop(w2);
    column.remove_all();
    assert!(
        weak.upgrade().is_none(),
        "label freed with its last reference"
    );
    println!("smoke ok: backend={} hits={}", awra::backend(), hits.get());
}
