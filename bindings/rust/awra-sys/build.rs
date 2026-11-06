//! Builds Awra 2 (and its BUI kit) from the C sources with CMake and links
//! the static libraries. AWRA_LIB_DIR points to an existing build instead.
use std::env;
use std::path::PathBuf;

fn main() {
    let root = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap())
        .join("../../..")
        .canonicalize()
        .expect("the Awra source tree");
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    let target_env = env::var("CARGO_CFG_TARGET_ENV").unwrap_or_default();

    println!("cargo:rerun-if-env-changed=AWRA_LIB_DIR");
    for dir in ["src", "include", "bui/kit", "bui/gpu", "bui/core", "bui/types"] {
        println!("cargo:rerun-if-changed={}", root.join(dir).display());
    }
    println!("cargo:rerun-if-changed={}", root.join("CMakeLists.txt").display());

    let build_dir = if let Ok(dir) = env::var("AWRA_LIB_DIR") {
        PathBuf::from(dir)
    } else {
        let mut cfg = cmake::Config::new(&root);
        cfg.define("AWRA_SHOWCASE", "OFF")
            .define("AWRA_TESTS", "OFF")
            .define("CMAKE_POLICY_VERSION_MINIMUM", "3.5")
            .profile("Release")
            .build_target("awra");
        if target_env == "msvc" {
            cfg.static_crt(false);
        }
        cfg.build().join("build")
    };

    let search = |sub: &str| {
        for cand in [build_dir.join(sub), build_dir.join(sub).join("Release")] {
            println!("cargo:rustc-link-search=native={}", cand.display());
        }
    };
    search("");
    search("bui/kit");
    search("bui/gpu");
    search("bui/core");
    for lib in ["awra", "bui_kit", "bui_gpu", "bui_core"] {
        println!("cargo:rustc-link-lib=static={lib}");
    }

    match target_os.as_str() {
        "linux" | "freebsd" | "openbsd" | "netbsd" | "dragonfly" => {
            for lib in ["wayland-client", "wayland-cursor", "xkbcommon"] {
                println!("cargo:rustc-link-lib=dylib={lib}");
            }
            println!("cargo:rustc-link-lib=dylib=stdc++");
            println!("cargo:rustc-link-lib=dylib=m");
            println!("cargo:rustc-link-lib=dylib=dl");
            println!("cargo:rustc-link-lib=dylib=pthread");
        }
        "windows" => {
            for lib in [
                "user32", "gdi32", "dwmapi", "uxtheme", "shell32", "ole32", "comdlg32", "imm32",
                "shcore", "advapi32", "winmm",
            ] {
                println!("cargo:rustc-link-lib=dylib={lib}");
            }
            if target_env == "gnu" {
                // MinGW: the C++ runtime linked in (no DLL beside the executable)
                let cxx = env::var("CXX_x86_64_pc_windows_gnu")
                    .or_else(|_| env::var("CXX"))
                    .unwrap_or_else(|_| "x86_64-w64-mingw32-g++".into());
                if let Ok(out) = std::process::Command::new(&cxx).arg("-print-file-name=libstdc++.a").output() {
                    let p = PathBuf::from(String::from_utf8_lossy(&out.stdout).trim());
                    if let Some(dir) = p.parent().filter(|d| d.is_absolute()) {
                        println!("cargo:rustc-link-search=native={}", dir.display());
                    }
                }
                println!("cargo:rustc-link-lib=static=stdc++");
            }
        }
        _ => {}
    }
}
