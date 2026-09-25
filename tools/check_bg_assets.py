#!/usr/bin/env python3
"""Check that every nonempty extracted background made it into the native build.

Uses filenames and byte counts from the decomp's extraction manifest; never reads
or prints game data. Run from any directory with --decomp pointing at the checkout.
"""
import argparse
import csv
from pathlib import Path
import re


def expected_backgrounds(decomp):
    manifest = decomp / "scripts/filelist.u.csv"
    if not manifest.is_file():
        return {}, [f"missing extraction manifest: {manifest}"]
    expected = {}
    with manifest.open(newline="", encoding="utf-8") as source:
        for row in csv.reader(source):
            if len(row) < 3 or "assets/obseg/bg/" not in row[2]:
                continue
            try:
                raw_size = row[1].strip()
                size = int(raw_size, 16 if raw_size.lower().startswith("0x") else 10)
            except ValueError:
                return {}, [f"invalid background size in {manifest}"]
            if size > 0:
                name = Path(row[2].strip()).name
                expected[name if name.endswith(".bin") else name + ".bin"] = size
    if not expected:
        return {}, [f"no nonempty backgrounds in {manifest}"]
    return expected, []


def background_errors(decomp, generated=False):
    expected, errors = expected_backgrounds(decomp)
    if errors:
        return errors
    for name, size in expected.items():
        path = decomp / "assets/obseg/bg" / name
        if not path.is_file() or path.stat().st_size != size:
            errors.append(f"{name}: expected {size} extracted bytes; missing or incomplete")
    if not generated or errors:
        return errors
    assembly = decomp / "assets/obseg/ob_seg.s"
    sizes = decomp / "assets/obseg/ge_obseg_bg_sizes.h"
    blobs = decomp / "assets/obseg/ge_obseg_blobs.c"
    registration = decomp / "assets/obseg/file_resource_table.inc.c"
    if not assembly.is_file() or not sizes.is_file() or not blobs.is_file() or not registration.is_file():
        return ["background blob source, generated size table, or resource table is missing"]
    assembly_text = assembly.read_text(encoding="utf-8", errors="replace")
    sizes_text = sizes.read_text(encoding="utf-8")
    # The generated C file can be huge. Only declarations carry names/sizes.
    declarations = {}
    with blobs.open(encoding="utf-8") as source:
        for line in source:
            match = re.match(r"unsigned char\s+(\w+)\s*\[(\d+)\]", line)
            if match:
                declarations[match.group(1)] = int(match.group(2))
    found = set()
    for symbol, source in re.findall(r"^\s*bg_file_seg\s+(\w+)\s*,\s*(\w+)\s*$",
                                     assembly_text, re.M):
        name = source + ".bin"
        if name not in expected:
            continue
        found.add(name)
        size = expected[name]
        if not re.search(r"\{\s*" + re.escape(symbol) + r"\s*,\s*" + str(size) +
                         r"u\s*\}", sizes_text):
            errors.append(f"{symbol}: absent or wrong size in generated background table")
        if declarations.get(symbol) != size:
            errors.append(f"{symbol}: absent or wrong size in generated blob source")
    for name in expected.keys() - found:
        errors.append(f"{name}: no background symbol in ob_seg.s")
    # The ROM contains blobs for maps whose resource entries upstream aliases to a
    # cut-level stub. Those entries link successfully but register a size of zero.
    # Cross-check by filename so one map cannot silently load another map's blob.
    registered = {}
    for filename, symbol in re.findall(
        r'\{\s*\w+\s*,\s*"bg/([^"/]+)\.seg"\s*,\s*&([A-Za-z0-9_]+)\s*\}',
        registration.read_text(encoding="utf-8")):
        registered[filename + ".bin"] = symbol
    for name in expected:
        wanted = name[:-4] + "_seg"
        actual = registered.get(name)
        if actual != wanted:
            errors.append(f"{name}: resource table registers {actual or 'nothing'}; expected {wanted}")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decomp", type=Path, default=Path.cwd())
    parser.add_argument("--generated", action="store_true")
    args = parser.parse_args()
    errors = background_errors(args.decomp, args.generated)
    for error in errors:
        print(f"background assets: {error}")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
