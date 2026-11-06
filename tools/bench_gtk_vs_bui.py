#!/usr/bin/env python3
"""GTK Awra 1 vs BUI Awra 2: startup, memory, CPU, size, threads.

Each run: a fresh Xvfb, a fresh HOME, the app launched with the same
environment; measures the time until its window is mapped, then samples
/proc after it settles."""
import json, os, re, shutil, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
CLK = os.sysconf("SC_CLK_TCK")
PAGE = os.sysconf("SC_PAGE_SIZE")

def env_for(kind, home, disp):
    e = {"PATH": os.environ["PATH"], "HOME": home, "XDG_CONFIG_HOME": home + "/.config",
         "XDG_DATA_HOME": home + "/.local/share", "XDG_RUNTIME_DIR": home + "/run",
         "DISPLAY": disp, "GDK_BACKEND": "x11", "LANG": "C.UTF-8"}
    if kind == "old":
        e["LD_LIBRARY_PATH"] = "/opt/baseline/prefix/lib/x86_64-linux-gnu:/opt/gtk422/lib/x86_64-linux-gnu"
        e["XDG_DATA_DIRS"] = "/opt/baseline/prefix/share:/opt/gtk422/share:/usr/local/share:/usr/share"
        e["GSETTINGS_SCHEMA_DIR"] = "/opt/gtk422/share/glib-2.0/schemas"
    else:
        # the same fontconfig as the GTK build: same fonts on both sides
        e["LD_LIBRARY_PATH"] = "/opt/gtk422/lib/x86_64-linux-gnu"
    return e

def proc_stat(pid):
    with open(f"/proc/{pid}/stat") as f:
        parts = f.read().rsplit(")", 1)[1].split()
    utime, stime = int(parts[11]), int(parts[12])
    threads = int(parts[17])
    return (utime + stime) / CLK, threads

def mem(pid):
    out = {}
    with open(f"/proc/{pid}/smaps_rollup") as f:
        for line in f:
            m = re.match(r"(\w+):\s+(\d+) kB", line)
            if m:
                out[m.group(1)] = int(m.group(2))
    with open(f"/proc/{pid}/status") as f:
        for line in f:
            if line.startswith("VmHWM") or line.startswith("RssAnon") or line.startswith("RssFile"):
                k, v = line.split(":")
                out[k] = int(v.split()[0])
    return out

def libs(pid):
    paths = set()
    with open(f"/proc/{pid}/maps") as f:
        for line in f:
            p = line.split()[-1] if len(line.split()) >= 6 else ""
            if p.startswith("/") and (".so" in p or p.endswith("keytomusic-awra") or "showcase" in p):
                paths.add(p)
    size = sum(os.path.getsize(p) for p in paths if os.path.exists(p))
    return len(paths), size

def window_mapped(disp, name):
    r = subprocess.run(["xdotool", "search", "--onlyvisible", "--name", name],
                       env={"DISPLAY": disp, "PATH": os.environ["PATH"]}, capture_output=True, text=True)
    return r.stdout.split()[0] if r.returncode == 0 and r.stdout.strip() else None

def xdo(disp, *args):
    subprocess.run(["xdotool", *args], env={"DISPLAY": disp, "PATH": os.environ["PATH"]},
                   capture_output=True)

def run_once(kind, binary, name, n, settle=6.0, idle=20.0, interact=10.0, extra=None):
    disp = f":{120 + n}"
    home = os.path.join(HERE, f"bhome-{kind}-{n}")
    shutil.rmtree(home, ignore_errors=True)
    os.makedirs(home + "/run", mode=0o700)
    os.makedirs(home + "/.config")
    x = subprocess.Popen(["Xvfb", disp, "-screen", "0", "1920x1080x24", "-nolisten", "tcp"],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.0)
    t0 = time.monotonic()
    e = env_for(kind, home, disp)
    e.update(extra or {})
    p = subprocess.Popen([binary], env=e, stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL)
    win = None
    while time.monotonic() - t0 < 30:
        win = window_mapped(disp, name)
        if win:
            break
        time.sleep(0.005)
    t_map = time.monotonic() - t0
    cpu_start, _ = proc_stat(p.pid)
    time.sleep(settle)
    cpu_settled, threads = proc_stat(p.pid)
    m_settled = mem(p.pid)
    # idle: nothing happens
    c0, _ = proc_stat(p.pid)
    time.sleep(idle)
    c1, _ = proc_stat(p.pid)
    m_idle = mem(p.pid)
    # interaction: the pointer sweeps over the keyboard (hover animations)
    c2, _ = proc_stat(p.pid)
    t_end = time.monotonic() + interact
    i = 0
    while time.monotonic() < t_end:
        xx = 300 + (i * 37) % 820
        yy = 160 + (i * 53) % 330
        xdo(disp, "mousemove", str(xx), str(yy))
        i += 1
        time.sleep(0.016)
    c3, _ = proc_stat(p.pid)
    m_after = mem(p.pid)
    nlibs, libsize = libs(p.pid)
    p.send_signal(signal.SIGTERM)
    try:
        p.wait(5)
    except subprocess.TimeoutExpired:
        p.kill()
    x.terminate()
    x.wait()
    return {
        "map_s": round(t_map, 3),
        "startup_cpu_s": round(cpu_settled, 3),
        "threads": threads,
        "pss_kb": m_idle.get("Pss"), "rss_kb": m_idle.get("Rss"),
        "rss_anon_kb": m_idle.get("RssAnon"), "peak_rss_kb": m_after.get("VmHWM"),
        "pss_after_kb": m_after.get("Pss"),
        "idle_cpu_pct": round(100 * (c1 - c0) / idle, 3),
        "hover_cpu_pct": round(100 * (c3 - c2) / interact, 2),
        "moves": i,
        "mapped_files": nlibs, "mapped_files_mb": round(libsize / 1e6, 1),
    }

def main():
    which = sys.argv[1]
    runs = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    extra = {"GSK_RENDERER": "cairo"} if "cairo" in which else {}
    which_bin = which.replace("-cairo", "")
    binary = {"old": "/opt/baseline/KeyToMusicAwra/target/release/keytomusic-awra",
              "new": "/home/user/KeyToMusicAwra/target/release/keytomusic-awra",
              "old-showcase": "/opt/baseline/prefix/bin/awra-showcase",
              "new-showcase": "/tmp/claude-0/build-awra/awra-showcase"}[which_bin]
    name = "Showcase" if "showcase" in which else "KeyToMusic"
    kind = "old" if which.startswith("old") else "new"
    res = [run_once(kind, binary, name, n + (0 if kind == "old" else 10), extra=extra) for n in range(runs)]
    print(json.dumps({"which": which, "runs": res}))

main()
