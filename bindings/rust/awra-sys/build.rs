//! Builds Awra 2 (and its BUI kit) from the C sources with CMake and links
//! the static libraries. AWRA_LIB_DIR points to an existing build instead.
use std::env;
use std::path::{Path, PathBuf};

fn main() {
    // bindings/rust/awra-sys -> the Awra root. Not canonicalize(): on Windows
    // it gives a \\?\ path, which CMake's globs do not read (no sources).
    let manifest = env::var("CARGO_MANIFEST_DIR").unwrap();
    let root = Path::new(&manifest).ancestors().nth(3).expect("the Awra source tree").to_path_buf();
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    let target_env = env::var("CARGO_CFG_TARGET_ENV").unwrap_or_default();

    println!("cargo:rerun-if-env-changed=AWRA_LIB_DIR");
    for dir in ["src", "include", "bui/kit", "bui/gpu", "bui/core", "bui/types"] {
        println!("cargo:rerun-if-changed={}", root.join(dir).display());
    }
    println!("cargo:rerun-if-changed={}", root.join("CMakeLists.txt").display());

    println!("cargo:rerun-if-env-changed=CMAKE");
    if target_env == "msvc" && env::var_os("CMAKE").is_none() {
        // the CMake installed with Visual Studio knows its generator; one
        // found first on the PATH may be older than this Visual Studio
        if let Some(cmake) = visual_studio_cmake() {
            env::set_var("CMAKE", cmake);
        }
    }
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
                "user32", "gdi32", "dwmapi", "uxtheme", "shell32", "ole32", "uuid", "comdlg32", "imm32",
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

/// The newest Visual Studio's own cmake.exe (vswhere knows where it is).
fn visual_studio_cmake() -> Option<PathBuf> {
    let base = env::var_os("ProgramFiles(x86)").map(PathBuf::from)?;
    let vswhere = base.join("Microsoft Visual Studio").join("Installer").join("vswhere.exe");
    let out = std::process::Command::new(vswhere)
        .args(["-latest", "-products", "*", "-property", "installationPath"])
        .output()
        .ok()?;
    let install = PathBuf::from(String::from_utf8_lossy(&out.stdout).trim());
    let cmake = install.join("Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe");
    cmake.is_file().then_some(cmake)
}
