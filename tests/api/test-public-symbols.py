#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path


def header_symbols(paths: list[Path]) -> set[str]:
    text = "\n".join(path.read_text(encoding="utf-8") for path in paths)
    symbols = set(re.findall(r"\b(awra_[a-z0-9_]+)\s*\(", text))
    for prefix in re.findall(
        r"(?:G_DECLARE_(?:FINAL|DERIVABLE)|GDK_DECLARE_INTERNAL)_TYPE\s*\([^,]+,\s*([a-z0-9_]+)", text
    ):
        symbols.add(f"{prefix}_get_type")
    return symbols


def exported_symbols(library: Path) -> set[str]:
    if library.suffix.lower() == ".dll":
        # A PE image's export names: "[  12] +base[  13]  000c awra_button_new".
        output = subprocess.check_output(["objdump", "-p", library], text=True)
        return set(re.findall(r"\]\s+[0-9a-fA-F]{4}\s+(\w+)\s*$", output, re.MULTILINE))
    output = subprocess.check_output(
        ["nm", "-D", "--defined-only", library], text=True
    )
    return {
        fields[-1].split("@", 1)[0]
        for line in output.splitlines()
        if (fields := line.split()) and fields[-1].startswith("awra_")
    }


def main() -> int:
    if len(sys.argv) < 4:
        raise SystemExit("usage: test-public-symbols.py LIB BASELINE HEADER...")
    library = Path(sys.argv[1])
    baseline = {
        line.strip()
        for line in Path(sys.argv[2]).read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.startswith("#")
    }
    public = header_symbols([Path(value) for value in sys.argv[3:]])
    exported = exported_symbols(library)
    missing = sorted(public - exported)
    leaked = sorted(exported - public)
    broken_baseline = sorted(baseline - exported)
    if missing or leaked or broken_baseline:
        details = []
        if missing:
            details.append("public but not exported: " + ", ".join(missing))
        if leaked:
            details.append("private symbol exported: " + ", ".join(leaked))
        if broken_baseline:
            details.append("Phase A contract removed: " + ", ".join(broken_baseline))
        raise AssertionError("\n".join(details))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
