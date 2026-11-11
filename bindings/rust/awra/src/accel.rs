//! `gtk_accelerator_get_label`: an accelerator ("<Primary><Shift>t") as GTK
//! shows it beside a menu item ("Maj+Ctrl+T"), with GTK's "keyboard label"
//! translations for the user's language (French; others as GTK's English).

#[path = "accel_fr.rs"]
mod tables;

const SHIFT: u32 = 1;
const CONTROL: u32 = 2;
const ALT: u32 = 4;
const SUPER: u32 = 8;
const HYPER: u32 = 16;
const META: u32 = 32;

/// The user's first language, as `g_get_language_names` orders them.
fn french() -> bool {
    use std::sync::OnceLock;
    static FR: OnceLock<bool> = OnceLock::new();
    *FR.get_or_init(|| {
        for var in ["LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"] {
            if let Ok(v) = std::env::var(var) {
                let first = v.split(':').next().unwrap_or("").to_string();
                if first.is_empty() {
                    continue;
                }
                if first == "C" || first == "POSIX" {
                    return false;
                }
                return first.starts_with("fr");
            }
        }
        false
    })
}

/// `C_("keyboard label", text)`.
fn tr(text: &str) -> Option<&'static str> {
    if !french() {
        return None;
    }
    tables::FR.iter().find(|(k, _)| *k == text).map(|(_, v)| *v)
}

fn label(text: &'static str) -> &'static str {
    tr(text).unwrap_or(text)
}

/// The character a keyval name stands for (`gdk_keyval_to_unicode` of
/// `gdk_keyval_from_name`), for the printable Latin-1 range.
fn keyval_char(name: &str) -> Option<char> {
    let mut chars = name.chars();
    if let (Some(c), None) = (chars.next(), chars.clone().next()) {
        return Some(c);
    }
    tables::LATIN1
        .iter()
        .find(|(k, _)| *k == name)
        .and_then(|(_, v)| char::from_u32(*v))
}

/// Parses GTK's accelerator syntax: modifiers and the key's name.
fn parse(accel: &str) -> (u32, &str) {
    let mut mods = 0;
    let mut rest = accel;
    while let Some(start) = rest.strip_prefix('<') {
        let Some((m, r)) = start.split_once('>') else {
            break;
        };
        mods |= match m.to_ascii_lowercase().as_str() {
            "primary" | "control" | "ctrl" | "ctl" => CONTROL,
            "shift" | "shft" => SHIFT,
            "alt" | "mod1" => ALT,
            "super" => SUPER,
            "hyper" => HYPER,
            "meta" => META,
            _ => 0,
        };
        rest = r;
    }
    (mods, rest)
}

/// `gtk_accelerator_get_label` of a parsed accelerator string.
pub fn accelerator_get_label(accel: &str) -> String {
    let (mods, key) = parse(accel);
    let mut out = String::new();
    let mut seen = false;
    for (bit, name) in [
        (SHIFT, "Shift"),
        (CONTROL, "Ctrl"),
        (ALT, "Alt"),
        (SUPER, "Super"),
        (HYPER, "Hyper"),
        (META, "Meta"),
    ] {
        if mods & bit != 0 {
            if seen {
                out.push('+');
            }
            out.push_str(label(name));
            seen = true;
        }
    }
    if key.is_empty() {
        return out;
    }
    // the keypad's characters: "KP_1" is the digit, after "KP "
    let (kp, base) = match key.strip_prefix("KP_") {
        Some(k) if k.chars().count() == 1 || k == "Space" => {
            (true, if k == "Space" { "space" } else { k })
        }
        _ => (false, key),
    };
    if let Some(ch) =
        keyval_char(base).filter(|c| *c == ' ' || !(c.is_whitespace() || c.is_control()))
    {
        if seen {
            out.push('+');
        }
        if kp {
            out.push_str(label("KP"));
            out.push(' ');
        }
        match ch {
            ' ' => out.push_str(label("Space")),
            '\\' => out.push_str(label("Backslash")),
            c => out.extend(c.to_uppercase()),
        }
        return out;
    }
    if seen {
        out.push('+');
    }
    match tr(key) {
        Some(t) => out.push_str(t),
        None => {
            let k = key
                .strip_prefix("XF86")
                .or_else(|| key.strip_prefix("ISO_"))
                .unwrap_or(key);
            out.push_str(&k.replace('_', " "));
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn labels() {
        // English (the tests run without a French locale)
        if !french() {
            assert_eq!(accelerator_get_label("<Primary><Shift>t"), "Shift+Ctrl+T");
            assert_eq!(accelerator_get_label("<Primary>question"), "Ctrl+?");
            assert_eq!(accelerator_get_label("space"), "Space");
            assert_eq!(accelerator_get_label("Page_Up"), "Page Up");
            assert_eq!(accelerator_get_label("<Alt>Return"), "Alt+Return");
        }
    }
}
