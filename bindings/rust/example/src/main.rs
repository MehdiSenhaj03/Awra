// SPDX-License-Identifier: LGPL-2.1-or-later

use awra::prelude::*;

fn assert_action_map<T: IsA<awra::gio::ActionMap>>() {}

fn exercise_projected_gtk_types() {
    let text_area = awra::TextArea::new();
    let _ = (text_area.buffer(), text_area.delegate());
    let _ = awra::SearchEntry::new().delegate();
    let _ = awra::PasswordEntry::new().delegate();
    let _ = awra::Switch::new().delegate();
    let _ = awra::SpinButton::with_range(0.0, 10.0, 1.0).delegate();
    let _ = awra::Slider::with_range(awra::gtk::Orientation::Horizontal, 0.0, 1.0, 0.1);
    let _ = awra::ProgressBar::new().delegate();
    let _ = awra::NavigationView::new().stack();
    let _ = awra::TabView::new().stack();
    let _ = awra::Dropdown::from_strings(&["One", "Two"]).delegate();

    let store = awra::gio::ListStore::new::<awra::glib::Object>();
    let selection = awra::gtk::SingleSelection::new(Some(store));
    let factory = awra::gtk::SignalListItemFactory::new();
    let _ = awra::DataView::new(Some(&selection)).delegate();
    let list = awra::ListView::new(&selection, &factory);
    let list_header = awra::gtk::Label::new(Some("Name"));
    list.set_header(Some(&list_header));
    assert_eq!(list.header(), Some(list_header.upcast()));
    let _ = list.delegate();
    let navigation_item = awra::NavigationItem::with_label("Library");
    let count = awra::Badge::new("3");
    navigation_item.set_suffix(Some(&count));
    assert!(navigation_item.suffix().is_some());
    let column = awra::DataColumn::new(Some("Name"), Some(&factory));
    column.set_alignment(awra::gtk::Align::Start);
    let _ = (column.alignment(), column.delegate(), column.sorter());

    let parent = awra::gtk::Window::new();
    let palette = awra::CommandPalette::new(Some(&parent));
    palette.connect_activate_command(|_, _, _| false);
}

fn exercise_media_input_components() {
    use std::cell::Cell;
    use std::rc::Rc;

    let keyboard = awra::KeyboardView::builder()
        .physical_layout(awra::KeyboardPhysicalLayout::Iso)
        .selection_mode(awra::gtk::SelectionMode::Multiple)
        .show_numpad(true)
        .build();
    let activated = Rc::new(Cell::new(0));
    let counter = activated.clone();
    keyboard.connect_key_activated(move |_, code, modifiers| {
        assert_eq!(code, "KeyA");
        assert!(modifiers.contains(awra::gdk::ModifierType::CONTROL_MASK));
        counter.set(counter.get() + 1);
    });
    keyboard.connect_files_dropped(|_, _code, files| {
        let _ = files.files();
    });
    keyboard.set_selected_codes(&["KeyA", "KeyS"]);
    assert_eq!(keyboard.selected_codes(), vec!["KeyA", "KeyS"]);
    assert!(keyboard.codes().iter().any(|code| code == "IntlBackslash"));
    let cap = keyboard.key_cap("KeyA").expect("KeyA cap");
    cap.set_bound(true);
    cap.set_secondary_label(Some("Thunder"));
    cap.set_tint(Some(&awra::gdk::RGBA::new(0.7, 0.2, 0.25, 1.0)));
    cap.set_badge(Some("2"));
    cap.emit_by_name::<()>("activated", &[&awra::gdk::ModifierType::CONTROL_MASK]);
    assert_eq!(activated.get(), 1);
    keyboard.set_key_label("KeyA", Some("Kick"));
    assert_eq!(cap.label(), "Kick");

    assert_eq!(awra::keyboard_code_from_hardware_keycode(38), Some("KeyA"));
    assert_eq!(awra::keyboard_code_to_hardware_keycode("Space"), 65);
    assert!(awra::keyboard_code_label(None, "ArrowUp").is_some());

    let capture = awra::ShortcutCapture::new();
    capture.set_shortcut(&[37, 38], awra::gdk::ModifierType::CONTROL_MASK);
    assert_eq!(capture.keycodes(), vec![37, 38]);
    assert_eq!(capture.codes(), vec!["ControlLeft", "KeyA"]);
    capture.set_display_labels(&["Ctrl", "Q"]);
    capture.connect_captured(|capture| {
        let _ = (capture.keycodes(), capture.keyvals(), capture.modifiers());
    });

    let waveform = awra::WaveformView::builder()
        .duration(10.0)
        .marker_editable(true)
        .build();
    waveform.set_peaks(&[0.1, 0.6, 0.9, 0.3]);
    assert_eq!(waveform.peaks().len(), 4);
    waveform.set_marker_position(4.0);
    waveform.connect_marker_committed(|_, position| assert!(position >= 0.0));

    let timeline = awra::Timeline::builder()
        .duration(120.0)
        .editable(true)
        .build();
    timeline.add_interval(10.0, 14.0, Some("Cue"));
    timeline.set_offset(-0.5);
    timeline.connect_offset_committed(|_, offset| assert!(offset.is_finite()));
    assert_eq!(timeline.n_intervals(), 1);
    assert_eq!(timeline.offset(), -0.5);
    timeline.set_preview(Some(&awra::gtk::Label::new(Some("Preview"))));
    assert!(timeline.preview().is_some());
    let source = awra::gdk::Paintable::new_empty(720, 576);
    let aspect = awra::AspectPaintable::new(&source, 16.0 / 9.0);
    assert!((aspect.intrinsic_aspect_ratio() - 16.0 / 9.0).abs() < 0.001);
    let pool = std::thread::spawn(|| {
        awra::FramePool::new(4, 2, awra::gdk::MemoryFormat::R8g8b8a8, 2, None)
            .expect("bounded frame pool")
    })
    .join()
    .expect("frame producer");
    let producer = pool.clone();
    let texture = std::thread::spawn(move || {
        let frame = producer.acquire().expect("frame lease");
        assert_ne!(frame.address(0), 0);
        let texture = frame.create_texture().expect("immutable texture");
        assert_eq!(frame.address(0), 0);
        texture
    })
    .join()
    .expect("texture producer");
    let stream = awra::TextureStream::new();
    stream.set_frame(Some(&texture), 16.0 / 9.0);
    assert!((stream.intrinsic_aspect_ratio() - 16.0 / 9.0).abs() < 0.001);
    assert!(stream.texture().is_some());
    stream.set_frame(None::<&awra::gdk::Texture>, 0.0);
    let overlay = awra::OverlayBar::builder().auto_hide(true).build();
    overlay.set_content(Some(&awra::gtk::Label::new(Some("Canvas"))));
    overlay.set_bar(Some(&timeline));
    overlay.set_inhibit_auto_hide(true);
    assert!(overlay.is_inhibit_auto_hide());

    let row = awra::MediaRow::builder()
        .title("Rain")
        .duration(90.0)
        .show_volume(true)
        .build();
    row.connect_seek_requested(|_, position| assert!(position >= 0.0));
    row.connect_volume_changed(|_, volume| assert!((0.0..=1.0).contains(&volume)));
    row.set_suffix(Some(&awra::Badge::new("OST")));

    let volume = awra::VolumeControl::new();
    volume.set_muted(true);
    assert_eq!(volume.effective_volume(), 0.0);

    let drop = awra::DropZone::new();
    drop.set_accepted_extensions(&["mp3", "ogg"]);
    assert!(drop.accepts_file(&awra::gio::File::for_path("/tmp/theme.MP3")));
    drop.connect_files_dropped(|_, files| assert!(!files.files().is_empty()));

    let toast = awra::ToastOverlay::new();
    let id = toast.show_with_action("Sound removed", "Undo", Some("win.undo"), 0);
    assert_eq!(toast.current_toast_id(), id);
    toast.dismiss(id);
    assert!(!toast.is_revealed());

    let context = awra::Context::for_display(&awra::gdk::Display::default().expect("display"));
    let style = context.style_manager();
    let duration = awra::motion_duration(&style, awra::MotionPreset::Normal);
    style.set_reduced_motion(true);
    assert_eq!(awra::motion_duration(&style, awra::MotionPreset::Normal), 0);
    style.set_reduced_motion(false);
    assert!(duration > 0);
    assert_eq!(awra::motion_ease_out_cubic(1.0), 1.0);
    assert_eq!(awra::motion_ease_in_out_cubic(0.0), 0.0);
    let menu = awra::gio::Menu::new();
    menu.append(Some("Remove"), Some("win.remove"));
    awra::set_context_menu_model(&cap, Some(&menu));
    assert!(awra::context_menu_model(&cap).is_some());
    awra::set_context_menu_model(&cap, None::<&awra::gio::MenuModel>);
    assert!(awra::context_menu_model(&cap).is_none());
}

fn exercise_motion() {
    use std::cell::Cell;
    use std::rc::Rc;

    let widget = awra::gtk::Label::new(Some("Animated"));
    let received = Rc::new(Cell::new(0.0));
    let sink = received.clone();
    let target = awra::CallbackAnimationTarget::new(move |value| sink.set(value));
    let timed =
        awra::TimedAnimation::for_preset(&widget, 0.0, 8.0, awra::MotionPreset::Fast, target);
    timed.set_easing(awra::Easing::EaseOutBack);
    timed.connect_done(|animation| assert_eq!(animation.state(), awra::AnimationState::Finished));
    // An unmapped widget has no frames to show: the animation jumps to its end.
    timed.play();
    assert_eq!(received.get(), 8.0);
    assert_eq!(awra::Easing::Linear.ease(0.5), 0.5);

    let property = awra::PropertyAnimationTarget::new(&widget, "opacity");
    let spring = awra::SpringAnimation::new(&widget, 1.0, 0.5, 0.8, 400.0, property);
    assert!(spring.estimated_duration() < awra::DURATION_INFINITE);
    spring.retarget(0.25);
    assert!((widget.opacity() - 0.25).abs() < 0.01);
}

fn exercise_messaging_components() {
    let avatar = awra::Avatar::new(40, Some("Ada Lovelace"));
    avatar.set_presence(awra::Presence::Idle);
    avatar.set_shape(awra::AvatarShape::Rounded);
    avatar.set_speaking(true);
    avatar.set_custom_image(Some(&awra::gdk::Paintable::new_empty(64, 64)));
    assert_eq!(avatar.text().as_deref(), Some("Ada Lovelace"));
    assert_eq!(avatar.presence(), awra::Presence::Idle);

    let rail = awra::Rail::new();
    let workspace = awra::RailItem::new(Some("Workshop"));
    workspace.set_child(Some(&avatar));
    workspace.set_mention_count(3);
    let folder = awra::RailGroup::new(Some("Projects"));
    folder.set_color(Some(&awra::gdk::RGBA::new(0.3, 0.5, 0.9, 1.0)));
    folder.append(&awra::RailItem::new(Some("Atelier")));
    rail.append(&workspace);
    rail.add_separator();
    rail.append(&folder);
    rail.set_reorderable(true);
    rail.connect_item_moved(|_, item, position| {
        let _ = (item, position);
    });
    rail.connect_item_merged(|_, item, onto| {
        let _ = (item, onto);
    });
    workspace.connect_activate(|item| assert_eq!(item.label().as_deref(), Some("Workshop")));
    workspace.emit_by_name::<()>("activate", &[]);
    assert_eq!(rail.selected_item(), Some(workspace.clone()));
    assert_eq!(rail.n_entries(), 3);
}

fn main() {
    assert_action_map::<awra::Window>();
    awra::gtk::init().expect("Rust Awra example requires a GTK display");
    awra::init();
    exercise_projected_gtk_types();
    exercise_media_input_components();
    exercise_motion();
    exercise_messaging_components();

    let window = awra::Window::builder().keep_rounded_corners(true).build();
    assert!(window.is_keep_rounded_corners());
    window.set_keep_rounded_corners(false);
    assert!(!window.is_keep_rounded_corners());
    window.destroy();

    let header = awra::PageHeader::builder()
        .title("Safe generated bindings")
        .subtitle("Awra widgets remain ordinary gtk-rs widgets")
        .build();
    awra::set_typography(&header, awra::Typography::Title2);
    let page = awra::Page::builder().child(&header).build();
    let page_content = awra::gtk::Box::new(awra::gtk::Orientation::Vertical, 0);
    awra::apply_layout_preset(&page_content, awra::LayoutPreset::Page);
    let card = awra::Card::new();
    card.set_child(Some(&awra::gtk::Label::new(Some("Rust consumer"))));
    assert_eq!(page.child(), Some(header.upcast()));
    assert!(card.child().is_some());

    let inspector = awra::Inspector::new();
    inspector.set_target(Some(&card));
    let report_path = std::env::temp_dir().join("awra-rust-report.txt");
    inspector
        .export_report_to(&report_path)
        .expect("safe report export");
    assert!(std::fs::read_to_string(&report_path)
        .expect("read report")
        .contains("AwraCard"));
    std::fs::remove_file(report_path).expect("remove report fixture");
}
