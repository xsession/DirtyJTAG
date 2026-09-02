#!/usr/bin/env python3
"""MSP430/MSP430X host-side helpers for DirtyJTAG Universal Pico.

The firmware exposes safe JTAG/SBW TAP primitives.  This module handles the
host/device-descriptor side: TI-TXT/Intel-HEX parsing, segment planning, and
profile metadata for MSP430 flash/FRAM families.  Destructive JTAG erase/write
macros remain guarded until device-family bench validation is complete.
"""
from __future__ import annotations
from dataclasses import dataclass
from typing import Dict, Iterable, List, Tuple
import json
import os

try:
    from hex_utils import HexError, parse_ihex_file
except ImportError:
    from host.hex_utils import HexError, parse_ihex_file

MSP430_PROFILE_DIR = os.path.join(os.path.dirname(__file__), "msp430_profiles")

MSP430_RAW_TAP_RESET = 0x00
MSP430_RAW_SHIFT_IR = 0x01
MSP430_RAW_SHIFT_DR = 0x02
MSP430_RAW_CLOCK = 0x03

@dataclass(frozen=True)
class Msp430Segment:
    address: int
    data: bytes


def load_msp430_profile(name: str, search_dir: str = MSP430_PROFILE_DIR) -> dict:
    if not name:
        raise ValueError("MSP430 profile name is required")
    safe = name.lower().replace("/", "_").replace("\\", "_")
    path = os.path.join(search_dir, safe + ".json")
    if not os.path.exists(path):
        path = name
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    required = ("device", "family", "default_interface", "address_bits", "flash_start", "flash_size")
    missing = [k for k in required if k not in data]
    if missing:
        raise ValueError(f"MSP430 profile {name!r} missing {', '.join(missing)}")
    data.setdefault("segment_size", 512)
    data.setdefault("write_block", 2)
    data.setdefault("ram_start", 0)
    data.setdefault("ram_size", 0)
    data.setdefault("info_start", 0x1000)
    data.setdefault("info_size", 0)
    data.setdefault("notes", [])
    data.setdefault("implemented", {})
    return data


def list_msp430_profiles(search_dir: str = MSP430_PROFILE_DIR) -> List[dict]:
    out: List[dict] = []
    if not os.path.isdir(search_dir):
        return out
    for fn in sorted(os.listdir(search_dir)):
        if not fn.endswith(".json"):
            continue
        try:
            out.append(load_msp430_profile(os.path.splitext(fn)[0], search_dir))
        except Exception as exc:
            out.append({"device": fn, "invalid": str(exc)})
    return out


def parse_ti_txt_lines(lines: Iterable[str]) -> Dict[int, int]:
    """Parse TI-TXT firmware files into a sparse byte-addressed memory map."""
    mem: Dict[int, int] = {}
    addr = None
    ended = False
    for lineno, raw in enumerate(lines, 1):
        line = raw.strip()
        if not line:
            continue
        if line.startswith("q") or line.startswith("Q"):
            ended = True
            break
        if line.startswith("@"):
            try:
                addr = int(line[1:], 16)
            except ValueError as exc:
                raise HexError(f"line {lineno}: bad TI-TXT address {line!r}") from exc
            continue
        if addr is None:
            raise HexError(f"line {lineno}: data before address marker")
        parts = line.split()
        if not parts:
            continue
        for tok in parts:
            if len(tok) != 2:
                raise HexError(f"line {lineno}: bad TI-TXT byte {tok!r}")
            try:
                b = int(tok, 16)
            except ValueError as exc:
                raise HexError(f"line {lineno}: bad TI-TXT byte {tok!r}") from exc
            old = mem.get(addr)
            if old is not None and old != b:
                raise HexError(f"conflicting data at 0x{addr:08x}")
            mem[addr] = b
            addr += 1
    if not ended:
        raise HexError("missing TI-TXT q terminator")
    return mem


def parse_ti_txt_file(path: str) -> Dict[int, int]:
    with open(path, "r", encoding="ascii") as f:
        return parse_ti_txt_lines(f)


def load_msp430_image(path: str) -> Dict[int, int]:
    """Load TI-TXT or Intel HEX by file extension/content."""
    with open(path, "r", encoding="ascii") as f:
        first = ""
        for raw in f:
            first = raw.strip()
            if first:
                break
    if first.startswith(":"):
        return parse_ihex_file(path)
    return parse_ti_txt_file(path)


def _join_sparse_runs(mem: Dict[int, int], min_addr: int, max_addr: int) -> List[Tuple[int, bytes]]:
    keys = sorted(a for a in mem if min_addr <= a <= max_addr)
    if not keys:
        return []
    runs: List[Tuple[int, bytes]] = []
    start = prev = keys[0]
    buf = bytearray([mem[start]])
    for a in keys[1:]:
        if a == prev + 1:
            buf.append(mem[a])
        else:
            runs.append((start, bytes(buf)))
            start = a
            buf = bytearray([mem[a]])
        prev = a
    runs.append((start, bytes(buf)))
    return runs


def make_msp430_segments(mem: Dict[int, int], profile: dict, include_info: bool = False,
                         fill: int = 0xff) -> List[Msp430Segment]:
    if not 0 <= fill <= 0xff:
        raise HexError("fill byte must be 0..255")
    flash_start = int(profile["flash_start"])
    flash_end = flash_start + int(profile["flash_size"]) - 1
    info_start = int(profile.get("info_start", 0x1000))
    info_end = info_start + int(profile.get("info_size", 0)) - 1
    allowed: List[Tuple[int, int]] = [(flash_start, flash_end)]
    if include_info and int(profile.get("info_size", 0)) > 0:
        allowed.append((info_start, info_end))
    for addr in mem:
        if not any(lo <= addr <= hi for lo, hi in allowed):
            raise HexError(f"address 0x{addr:08x} is outside allowed MSP430 programming ranges")
    segments: List[Msp430Segment] = []
    block = max(1, int(profile.get("write_block", 2)))
    for lo, hi in allowed:
        for start, data in _join_sparse_runs(mem, lo, hi):
            base = (start // block) * block
            end = ((start + len(data) + block - 1) // block) * block
            out = bytes(mem.get(a, fill) for a in range(base, end))
            if any(x != fill for x in out):
                segments.append(Msp430Segment(base, out))
    return segments


def summarize_msp430_segments(segments: List[Msp430Segment]) -> str:
    if not segments:
        return "0 segments"
    total = sum(len(s.data) for s in segments)
    return f"{len(segments)} segments, {total} bytes, 0x{segments[0].address:05x}..0x{segments[-1].address + len(segments[-1].data) - 1:05x}"


def msp430_shift_payload(op: int, bits: int, data: bytes) -> bytes:
    if bits <= 0 or bits > 2048:
        raise ValueError("bits must be 1..2048")
    need = (bits + 7) // 8
    if len(data) != need:
        raise ValueError(f"data must be {need} bytes for {bits} bits")
    return bytes([op & 0xff, bits & 0xff, (bits >> 8) & 0xff]) + data
