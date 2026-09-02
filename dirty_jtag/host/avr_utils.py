#!/usr/bin/env python3
"""AVR host-side helpers for DirtyJTAG Universal Pico.

This module is intentionally descriptor-driven, inspired by AVRDUDE's public
model of part definitions plus programmer operations, but it is not a copy of
AVRDUDE source code.  It covers the common classic AVR ISP path first and keeps
UPDI/TPI/PDI algorithms as separate future high-level layers.
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

AVR_ALG_DIR = os.path.join(os.path.dirname(__file__), "avr_algorithms")

@dataclass(frozen=True)
class AvrPage:
    address: int
    data: bytes


def load_avr_profile(name: str, search_dir: str = AVR_ALG_DIR) -> dict:
    if not name:
        raise ValueError("AVR profile name is required")
    safe = name.lower().replace("/", "_").replace("\\", "_")
    path = os.path.join(search_dir, safe + ".json")
    if not os.path.exists(path):
        # allow exact file path for local experiments
        path = name
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    required = ("device", "protocol", "flash_size", "page_size")
    missing = [k for k in required if k not in data]
    if missing:
        raise ValueError(f"AVR profile {name!r} missing {', '.join(missing)}")
    data.setdefault("signature", [])
    data.setdefault("eeprom_size", 0)
    data.setdefault("fuses", {})
    data.setdefault("lock_bits", True)
    data.setdefault("notes", [])
    return data


def list_avr_profiles(search_dir: str = AVR_ALG_DIR) -> List[dict]:
    out: List[dict] = []
    if not os.path.isdir(search_dir):
        return out
    for fn in sorted(os.listdir(search_dir)):
        if not fn.endswith(".json"):
            continue
        try:
            out.append(load_avr_profile(os.path.splitext(fn)[0], search_dir))
        except Exception as exc:
            out.append({"device": fn, "invalid": str(exc)})
    return out


def avr_flash_bytes_from_ihex(path: str, flash_size: int, fill: int = 0xff) -> Dict[int, int]:
    if not 0 <= fill <= 0xff:
        raise HexError("fill byte must be 0..255")
    mem = parse_ihex_file(path)
    flash: Dict[int, int] = {}
    for addr, b in mem.items():
        if addr < 0 or addr >= flash_size:
            raise HexError(f"HEX address 0x{addr:08x} outside AVR flash size 0x{flash_size:x}")
        flash[addr] = b
    return flash


def make_avr_pages(mem: Dict[int, int], page_size: int, flash_size: int,
                   fill: int = 0xff) -> List[AvrPage]:
    if page_size <= 0:
        raise HexError("AVR page_size must be positive")
    if page_size & (page_size - 1):
        # AVR page sizes are usually powers of two; this catches bad descriptors.
        raise HexError("AVR page_size must be a power of two")
    if not mem:
        return []
    pages: List[AvrPage] = []
    first = (min(mem) // page_size) * page_size
    last = (max(mem) // page_size) * page_size
    for base in range(first, last + 1, page_size):
        if base >= flash_size:
            raise HexError(f"page base 0x{base:x} outside flash")
        data = bytes(mem.get(base + i, fill) for i in range(page_size))
        if any(x != fill for x in data):
            pages.append(AvrPage(base, data))
    return pages


def summarize_avr_pages(pages: List[AvrPage]) -> str:
    if not pages:
        return "0 pages"
    total = sum(len(p.data) for p in pages)
    return f"{len(pages)} pages, {total} bytes, 0x{pages[0].address:04x}..0x{pages[-1].address + len(pages[-1].data) - 1:04x}"


def avr_memop_from_ihex(path: str, profile: dict, fill: int = 0xff) -> List[AvrPage]:
    mem = avr_flash_bytes_from_ihex(path, int(profile["flash_size"]), fill=fill)
    return make_avr_pages(mem, int(profile["page_size"]), int(profile["flash_size"]), fill=fill)


def avrdude_like_signature(signature: Iterable[int]) -> str:
    sig = list(signature)
    if not sig:
        return "unknown"
    return "0x" + "".join(f"{x & 0xff:02x}" for x in sig)


def avr_isp_extended_address_for_byte(byte_address: int) -> int:
    """Return the AVR ISP Load Extended Address byte for a flash byte address.

    Serial ISP read/write instructions address flash words with a 16-bit word
    address. Large flash devices provide the upper word-address byte through
    the Load Extended Address instruction, so byte address 0x20000 maps to
    extended address 1.
    """
    if byte_address < 0:
        raise HexError("negative AVR flash address")
    return (byte_address >> 17) & 0xff


def avr_pages_need_extended_address(pages: List[AvrPage]) -> bool:
    return any(avr_isp_extended_address_for_byte(pg.address) or
               avr_isp_extended_address_for_byte(pg.address + len(pg.data) - 1)
               for pg in pages)


def summarize_avr_extended_windows(pages: List[AvrPage]) -> List[int]:
    windows = set()
    for pg in pages:
        if not pg.data:
            continue
        windows.add(avr_isp_extended_address_for_byte(pg.address))
        windows.add(avr_isp_extended_address_for_byte(pg.address + len(pg.data) - 1))
    return sorted(windows)
