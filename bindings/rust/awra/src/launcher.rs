//! gtk::UriLauncher and gtk::FileLauncher: the desktop's default handler
//! opens a URI, a file, or the folder holding a file (`awra_show_uri`:
//! xdg-open, ShellExecute on Windows). As GTK's, `launch` answers once,
//! after the request went out, from the main loop.

use crate::gio::{Cancellable, File};
use crate::glib::Error;

fn answer<F: FnOnce(Result<(), Error>) + 'static>(uri: String, done: F) {
    let result = if crate::show_uri(&uri) {
        Ok(())
    } else {
        Err(Error::new(format!("Could not open {uri}")))
    };
    crate::glib::idle_add_local_once(move || done(result));
}

fn refuse<F: FnOnce(Result<(), Error>) + 'static>(message: &'static str, done: F) {
    crate::glib::idle_add_local_once(move || done(Err(Error::new(message))));
}

/// gtk::UriLauncher.
#[derive(Debug, Clone, Default)]
pub struct UriLauncher {
    uri: Option<String>,
}

impl UriLauncher {
    pub fn new(uri: &str) -> UriLauncher {
        UriLauncher {
            uri: Some(uri.to_string()),
        }
    }
    pub fn uri(&self) -> Option<String> {
        self.uri.clone()
    }
    pub fn set_uri(&mut self, uri: Option<&str>) {
        self.uri = uri.map(str::to_string);
    }
    /// Opens the URI with the desktop's handler for its scheme.
    pub fn launch<F: FnOnce(Result<(), Error>) + 'static>(
        &self,
        _parent: Option<&crate::Window>,
        _cancellable: Option<&Cancellable>,
        done: F,
    ) {
        match &self.uri {
            Some(uri) => answer(uri.clone(), done),
            None => refuse("No URI to open", done),
        };
    }
}

/// gtk::FileLauncher.
#[derive(Debug, Clone, Default)]
pub struct FileLauncher {
    file: Option<File>,
}

impl FileLauncher {
    pub fn new(file: Option<&File>) -> FileLauncher {
        FileLauncher {
            file: file.cloned(),
        }
    }
    pub fn file(&self) -> Option<File> {
        self.file.clone()
    }
    pub fn set_file(&mut self, file: Option<&File>) {
        self.file = file.cloned();
    }
    /// Opens the file (a folder: the file manager there) with its default
    /// application.
    pub fn launch<F: FnOnce(Result<(), Error>) + 'static>(
        &self,
        _parent: Option<&crate::Window>,
        _cancellable: Option<&Cancellable>,
        done: F,
    ) {
        match &self.file {
            Some(file) => answer(file.uri(), done),
            None => refuse("No file to open", done),
        };
    }
    /// Opens the folder holding the file in the file manager.
    pub fn open_containing_folder<F: FnOnce(Result<(), Error>) + 'static>(
        &self,
        _parent: Option<&crate::Window>,
        _cancellable: Option<&Cancellable>,
        done: F,
    ) {
        let folder = self
            .file
            .as_ref()
            .and_then(File::path)
            .and_then(|p| p.parent().map(File::for_path));
        match folder {
            Some(folder) => answer(folder.uri(), done),
            None => refuse("No folder to open", done),
        };
    }
}
