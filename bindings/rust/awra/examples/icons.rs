//! The icon theme as applications use it: a header bar's symbolic buttons,
//! places in a sidebar, files with their content type's icon (GIO's names,
//! looked up as GTK does), and what the lookups chose.
//!
//! `cargo run --example icons [theme] [out.png]` (the desktop's theme by
//! default; out.png: an offscreen render instead of a window).

use awra::prelude::*;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let app = awra::Application::new("org.awra.Icons");
    let theme = awra::IconTheme::for_display(&awra::gdk::Display::default().unwrap());
    if let Some(t) = args.get(1).filter(|t| !t.is_empty()) {
        theme.set_theme_name(Some(t));
    }
    println!("theme: {}", theme.theme_name());
    let out = args.get(2).cloned();
    let window = if out.is_some() {
        awra::Window::offscreen("Icons", 720, 520, 1.0)
    } else {
        awra::Window::new(&app)
    };
    let column = awra::Box::new(awra::Orientation::Vertical, 12);
    column.set_margin_start(16);
    column.set_margin_end(16);
    column.set_margin_top(16);

    // a toolbar of symbolic icons
    let bar = awra::Box::new(awra::Orientation::Horizontal, 6);
    for name in [
        "sidebar-show-symbolic",
        "go-previous-symbolic",
        "go-next-symbolic",
        "go-up-symbolic",
        "view-more-symbolic",
        "edit-find-symbolic",
        "view-grid-symbolic",
        "open-menu-symbolic",
        "window-minimize-symbolic",
        "window-maximize-symbolic",
        "window-close-symbolic",
    ] {
        bar.append(&awra::Button::from_icon_name(name));
    }
    column.append(&bar);

    // places
    let places = awra::Box::new(awra::Orientation::Horizontal, 14);
    for name in [
        "user-home-symbolic",
        "folder-documents-symbolic",
        "folder-download-symbolic",
        "folder-pictures-symbolic",
        "folder-videos-symbolic",
        "user-trash-symbolic",
        "drive-harddisk-symbolic",
        "computer-symbolic",
    ] {
        places.append(&awra::Image::from_icon_name(name));
    }
    column.append(&places);

    // files: their type's icon
    let files = awra::Box::new(awra::Orientation::Horizontal, 10);
    for file in [
        "Documents/",
        "rapport.pdf",
        "photo.png",
        "notes.md",
        "archive.zip",
        "film.mkv",
        "song.mp3",
        "script.sh",
        "data.bin",
        "page.html",
    ] {
        let (ct, _) = if file.ends_with('/') {
            ("inode/directory".to_owned(), false)
        } else {
            awra::gio::content_type_guess(Some(file), &[])
        };
        let icon = awra::gio::content_type_get_icon(&ct);
        let image = awra::Image::from_gicon(&icon);
        image.set_pixel_size(48);
        let cell = awra::Box::new(awra::Orientation::Vertical, 4);
        cell.append(&image);
        let label = awra::Label::new(Some(file));
        cell.append(&label);
        files.append(&cell);
        let p = theme.lookup_by_gicon(
            &icon,
            48,
            1,
            awra::TextDirection::Ltr,
            awra::IconLookupFlags::NONE,
        );
        println!(
            "{file:12} {ct:28} -> {} ({})",
            p.file()
                .and_then(|f| f.path())
                .map(|p| p.display().to_string())
                .unwrap_or_else(|| "built in".into()),
            awra::gio::content_type_get_description(&ct)
        );
    }
    column.append(&files);
    window.set_child(Some(&column));
    let _changed = theme.connect_changed(|t| println!("icon theme now {}", t.theme_name()));

    if let Some(out) = out {
        window.render_offscreen(true);
        assert!(window.snapshot_png(&out), "snapshot");
        return;
    }
    window.present();
    app.run();
}
