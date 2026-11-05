#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

"""Generate the export list from Awra's installed C headers.

An ELF version script by default; with --def, the module-definition file
that limits a Windows DLL to the same symbols (PE has no symbol versions).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


def symbols_from_headers(paths: list[Path]) -> set[str]:
    text = "\n".join(path.read_text(encoding="utf-8") for path in paths)
    symbols = set(re.findall(r"\b(awra_[a-z0-9_]+)\s*\(", text))
    for prefix in re.findall(
        r"(?:G_DECLARE_(?:FINAL|DERIVABLE)|GDK_DECLARE_INTERNAL)_TYPE\s*\([^,]+,\s*([a-z0-9_]+)", text
    ):
        symbols.add(f"{prefix}_get_type")
    return symbols


def main() -> int:
    args = sys.argv[1:]
    module_definition = bool(args) and args[0] == "--def"
    if module_definition:
        args = args[1:]
    if len(args) < 2:
        raise SystemExit("usage: generate-version-script.py [--def] OUTPUT HEADER...")
    output = Path(args[0])
    symbols = symbols_from_headers([Path(value) for value in args[1:]])
    if not symbols:
        raise SystemExit("no Awra API symbols found")
    if module_definition:
        lines = ["EXPORTS"]
        lines.extend(f"    {symbol}" for symbol in sorted(symbols))
        lines.append("")
    else:
        lines = ["AWRA_0_1 {", "  global:"]
        lines.extend(f"    {symbol};" for symbol in sorted(symbols))
        lines.extend(["  local:", "    *;", "};", ""])
    output.write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
