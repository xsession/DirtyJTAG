#!/usr/bin/env python3
"""Descriptor-driven UPDI NVM helpers for DirtyJTAG Universal Pico.

This module implements safe planning and instruction-level metadata, not a blind
writer. Modern UPDI AVR families share the UPDI instruction set but use NVMCTRL
variants, memory bases and erase/write rules that must come from a part
descriptor.
"""
from __future__ import annotations
from dataclasses import dataclass
from typing import Dict, List
import json
import os
try:
    from hex_utils import HexError, parse_ihex_file
except ImportError:
    from host.hex_utils import HexError, parse_ihex_file

UPDI_PROFILE_DIR = os.path.join(os.path.dirname(__file__), "updi_profiles")
UPDI_SYNC = 0x55
UPDI_LDS = 0x00
UPDI_STS = 0x40
UPDI_LDCS = 0x80
UPDI_STCS = 0xC0
UPDI_REPEAT = 0xA0
UPDI_KEY = 0xE0
UPDI_KEY_NVMPROG = b"NVMProg "

@dataclass(frozen=True)
class UpdiPage:
    file_address: int
    target_address: int
    data: bytes

def updi_nvmp_key_bytes() -> bytes:
    return UPDI_KEY_NVMPROG

def load_updi_profile(name: str, search_dir: str = UPDI_PROFILE_DIR) -> dict:
    if not name:
        raise ValueError("UPDI profile name is required")
    safe = name.lower().replace("/", "_").replace("\\", "_")
    path = os.path.join(search_dir, safe + ".json")
    if not os.path.exists(path):
        path = name
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    required = ("device", "protocol", "flash_size", "page_size", "flash_base")
    missing = [k for k in required if k not in data]
    if missing:
        raise ValueError(f"UPDI profile {name!r} missing {', '.join(missing)}")
    if data["protocol"] != "updi":
        raise ValueError(f"UPDI profile {name!r} has protocol {data['protocol']!r}")
    data.setdefault("nvm_version", "unknown")
    data.setdefault("implemented", {})
    return data

def list_updi_profiles(search_dir: str = UPDI_PROFILE_DIR) -> List[dict]:
    out: List[dict] = []
    if not os.path.isdir(search_dir):
        return out
    for fn in sorted(os.listdir(search_dir)):
        if not fn.endswith(".json"):
            continue
        try:
            out.append(load_updi_profile(os.path.splitext(fn)[0], search_dir))
        except Exception as exc:
            out.append({"device": fn, "invalid": str(exc)})
    return out

def make_updi_nvm_plan(path: str, profile: dict, fill: int = 0xff) -> List[UpdiPage]:
    if not 0 <= fill <= 0xff:
        raise HexError("fill byte must be 0..255")
    flash_size = int(profile["flash_size"])
    page_size = int(profile["page_size"])
    flash_base = int(profile["flash_base"])
    if page_size <= 0 or page_size & (page_size - 1):
        raise HexError("UPDI page_size must be a positive power of two")
    mem = parse_ihex_file(path)
    flash: Dict[int, int] = {}
    for addr, b in mem.items():
        if addr < 0 or addr >= flash_size:
            raise HexError(f"HEX address 0x{addr:08x} outside UPDI flash size 0x{flash_size:x}")
        flash[addr] = b
    if not flash:
        return []
    first = (min(flash) // page_size) * page_size
    last = (max(flash) // page_size) * page_size
    out: List[UpdiPage] = []
    for base in range(first, last + 1, page_size):
        data = bytes(flash.get(base + i, fill) for i in range(page_size))
        if any(x != fill for x in data):
            out.append(UpdiPage(base, flash_base + base, data))
    return out

def summarize_updi_plan(plan: List[UpdiPage]) -> str:
    if not plan:
        return "0 pages"
    total = sum(len(p.data) for p in plan)
    return f"{len(plan)} pages, {total} bytes, target 0x{plan[0].target_address:05x}..0x{plan[-1].target_address + len(plan[-1].data) - 1:05x}"

def updi_ldcs(reg: int) -> bytes:
    if not 0 <= reg <= 15:
        raise ValueError("UPDI CS register address must be 0..15")
    return bytes([UPDI_SYNC, UPDI_LDCS | reg])

def updi_stcs(reg: int, value: int) -> bytes:
    if not 0 <= reg <= 15 or not 0 <= value <= 0xff:
        raise ValueError("UPDI STCS arguments out of range")
    return bytes([UPDI_SYNC, UPDI_STCS | reg, value])
