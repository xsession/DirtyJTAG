"""TMS320/C2000 host helpers for DirtyJTAG Universal Pico.

Clean-room helper layer for XDS110v3-style electrical/JTAG bring-up.  It does
not implement TI's proprietary CCS/XDS USB stack; it plans target profiles and
builds the raw DJP2 JTAG operation envelopes understood by the Pico firmware.
"""
from __future__ import annotations
from dataclasses import dataclass
import json
import os
from typing import Dict, Iterable, List

TMS320_RAW_TAP_RESET = 0x00
TMS320_RAW_SHIFT_IR = 0x01
TMS320_RAW_SHIFT_DR = 0x02
TMS320_RAW_CLOCK = 0x03
TMS320_RAW_LINES = 0x04

PROFILE_DIR = os.path.join(os.path.dirname(__file__), "tms320_profiles")

@dataclass(frozen=True)
class TMS320Profile:
    device: str
    family: str
    core: str
    interface: str
    ir_length: int
    default_clock_hz: int
    vtref_mv_min: int
    vtref_mv_max: int
    connector: str
    notes: str = ""


def _load_json(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def list_tms320_profiles() -> List[dict]:
    out: List[dict] = []
    if not os.path.isdir(PROFILE_DIR):
        return out
    for name in sorted(os.listdir(PROFILE_DIR)):
        if not name.endswith(".json"):
            continue
        path = os.path.join(PROFILE_DIR, name)
        try:
            p = _load_json(path)
            p["profile"] = os.path.splitext(name)[0]
            out.append(p)
        except Exception as e:  # pragma: no cover - surfaced by CLI
            out.append({"device": os.path.splitext(name)[0], "invalid": str(e)})
    return out


def load_tms320_profile(name: str) -> dict:
    base = name[:-5] if name.endswith(".json") else name
    path = os.path.join(PROFILE_DIR, base + ".json")
    if not os.path.exists(path):
        raise FileNotFoundError(f"unknown TMS320 profile {name!r}; see tms320-profile-list")
    p = _load_json(path)
    for key in ("device", "family", "core", "interface", "ir_length", "default_clock_hz"):
        if key not in p:
            raise ValueError(f"{path}: missing {key}")
    return p


def pack_bits_lsb(value: int, bits: int) -> bytes:
    if bits <= 0:
        raise ValueError("bits must be positive")
    n = (bits + 7) // 8
    return int(value).to_bytes(n, "little")


def tms320_shift_payload(op: int, bits: int, data: bytes) -> bytes:
    if bits <= 0 or bits > 2048 * 8:
        raise ValueError("invalid bit count")
    need = (bits + 7) // 8
    if len(data) != need:
        raise ValueError(f"{bits} bits need {need} bytes, got {len(data)}")
    return bytes([op & 0xff, bits & 0xff, (bits >> 8) & 0xff]) + data


def tms320_idcode_payload() -> bytes:
    """Build a conservative 32-bit DR scan payload after TAP reset."""
    return tms320_shift_payload(TMS320_RAW_SHIFT_DR, 32, b"\x00\x00\x00\x00")


def format_idcode(raw: bytes) -> str:
    if len(raw) < 4:
        return raw.hex(" ")
    v = int.from_bytes(raw[:4], "little")
    return f"0x{v:08x}"


def summarize_profile(p: dict) -> str:
    rng = ""
    if "vtref_mv_min" in p and "vtref_mv_max" in p:
        rng = f" vtref={p['vtref_mv_min']/1000:.1f}-{p['vtref_mv_max']/1000:.1f}V"
    return (f"{p['device']} family={p.get('family','-')} core={p.get('core','-')} "
            f"if={p.get('interface','-')} ir={p.get('ir_length','?')} "
            f"clock={p.get('default_clock_hz','?')}Hz{rng}")
