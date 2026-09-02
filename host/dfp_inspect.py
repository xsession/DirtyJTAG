#!/usr/bin/env python3
"""Inspect a Microchip .atpack or unpacked DFP for debug-related assets.

This tool does not decode or redistribute proprietary Debug Executive binaries.
It helps a user point DirtyJTAG at assets already installed/downloaded under the
Microchip pack license and records candidate paths/metadata for clean-room work.
"""
from __future__ import annotations
import argparse
import json
import struct
from pathlib import Path
import re
import zipfile

KEYWORDS = (
    "debug", "executive", "debugexec", "debug_exec", "de.hex", "de.bin",
    "algorithm", "programmer", "icd", "reserved", "edc", "device"
)
TEXT_SUFFIXES = {".xml", ".atdf", ".pds", ".txt", ".json", ".yaml", ".yml", ".properties", ".md"}
DEVICE_RE = re.compile(r"(dsPIC(?:30|33)[A-Za-z0-9_-]+|PIC24[A-Za-z0-9_-]+)", re.I)


def interesting(name: str) -> bool:
    low = name.lower()
    return any(k in low for k in KEYWORDS)


def scan_text(name: str, data: bytes):
    if Path(name).suffix.lower() not in TEXT_SUFFIXES:
        return []
    text = data[:2_000_000].decode("utf-8", errors="ignore")
    hits = []
    for line_no, line in enumerate(text.splitlines(), 1):
        low = line.lower()
        if any(k in low for k in KEYWORDS):
            devices = sorted(set(m.group(0) for m in DEVICE_RE.finditer(line)))
            hits.append({"line": line_no, "text": line.strip()[:300], "devices": devices})
            if len(hits) >= 40:
                break
    return hits


def scan_zip(path: Path):
    files, text_hits = [], []
    with zipfile.ZipFile(path) as zf:
        for zi in zf.infolist():
            if zi.is_dir():
                continue
            if interesting(zi.filename):
                files.append({"path": zi.filename, "size": zi.file_size})
            if Path(zi.filename).suffix.lower() in TEXT_SUFFIXES:
                try:
                    hits = scan_text(zi.filename, zf.read(zi))
                except (KeyError, RuntimeError, OSError):
                    hits = []
                if hits:
                    text_hits.append({"path": zi.filename, "hits": hits})
    return files, text_hits


def scan_dir(path: Path):
    files, text_hits = [], []
    for f in path.rglob("*"):
        if not f.is_file():
            continue
        rel = str(f.relative_to(path))
        if interesting(rel):
            files.append({"path": rel, "size": f.stat().st_size})
        if f.suffix.lower() in TEXT_SUFFIXES:
            try:
                hits = scan_text(rel, f.read_bytes())
            except OSError:
                hits = []
            if hits:
                text_hits.append({"path": rel, "hits": hits})
    return files, text_hits


def write_cleanroom_capsule(path: Path, family: int, hw_breakpoints: int, reg_bytes: int, flags: int):
    # Firmware capsule header: magic JDDE, version, family, register bytes,
    # hw breakpoint count, flags.  This is metadata only; it does not contain
    # or redistribute Microchip Debug Executive code.
    path.write_bytes(struct.pack("<IBBBBI", 0x4544444A, 1, family, reg_bytes, hw_breakpoints, flags) + b"\x00"*4)

def main():
    ap = argparse.ArgumentParser(description="Inspect Microchip DFP/.atpack debug assets without extracting proprietary binaries")
    ap.add_argument("path", type=Path)
    ap.add_argument("--json", dest="json_out", type=Path)
    ap.add_argument("--capsule", type=Path, help="write a clean-room dsPIC debug metadata capsule for DJP2 raw command 0x80")
    ap.add_argument("--family", type=int, default=0, help="capsule family id: 0 dsPIC30, 1 dsPIC33F, 2 dsPIC33E, 3 dsPIC33C/K")
    ap.add_argument("--hw-breakpoints", type=int, default=2)
    ap.add_argument("--reg-bytes", type=int, default=42)
    ap.add_argument("--capsule-flags", type=lambda x:int(x,0), default=0x0F, help="capability bits for local clean-room validation; default enables mock/full API")
    a = ap.parse_args()
    if not a.path.exists():
        raise SystemExit(f"not found: {a.path}")
    if a.path.is_dir():
        files, text_hits = scan_dir(a.path)
        kind = "directory"
    elif zipfile.is_zipfile(a.path):
        files, text_hits = scan_zip(a.path)
        kind = "atpack/zip"
    else:
        raise SystemExit("input must be an unpacked DFP directory or ZIP-compatible .atpack")
    report = {"input": str(a.path), "kind": kind, "candidate_files": files, "metadata_hits": text_hits}
    print(f"Input: {a.path} ({kind})")
    print(f"Candidate files: {len(files)}")
    for x in files[:80]:
        print(f"  {x['size']:9d}  {x['path']}")
    print(f"Metadata files with debug/programming terms: {len(text_hits)}")
    for item in text_hits[:30]:
        print(f"\n[{item['path']}]")
        for hit in item["hits"][:8]:
            print(f"  L{hit['line']}: {hit['text']}")
    if a.json_out:
        a.json_out.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"\nJSON report: {a.json_out}")
    if a.capsule:
        write_cleanroom_capsule(a.capsule, a.family, a.hw_breakpoints, a.reg_bytes, a.capsule_flags)
        print(f"Clean-room debug metadata capsule: {a.capsule}")

if __name__ == "__main__":
    main()
