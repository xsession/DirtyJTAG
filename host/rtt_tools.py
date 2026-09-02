#!/usr/bin/env python3
"""Clean-room helper functions for RTT-style host features."""
from __future__ import annotations
from dataclasses import dataclass
from typing import Dict
import re

TERMINAL_PREFIX = 0xFF
ANSI_RE = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")

@dataclass
class RttTerminalSplit:
    all_data: bytes
    terminals: Dict[int, bytes]


def terminal_id_from_code(code: int) -> int | None:
    """Decode the public RTT virtual-terminal selector byte.

    SEGGER's RTT implementations use 0xFF followed by '0'..'9' or 'A'..'F'
    to route subsequent Channel-0 terminal output to virtual terminals 0..15.
    """
    if ord('0') <= code <= ord('9'):
        return code - ord('0')
    if ord('A') <= code <= ord('F'):
        return 10 + code - ord('A')
    if ord('a') <= code <= ord('f'):
        return 10 + code - ord('a')
    return None


def split_virtual_terminals(data: bytes, initial_terminal: int = 0) -> RttTerminalSplit:
    terms: Dict[int, bytearray] = {i: bytearray() for i in range(16)}
    active = initial_terminal if 0 <= initial_terminal < 16 else 0
    all_out = bytearray()
    i = 0
    while i < len(data):
        b = data[i]
        if b == TERMINAL_PREFIX and i + 1 < len(data):
            tid = terminal_id_from_code(data[i + 1])
            if tid is not None:
                active = tid
                i += 2
                continue
        all_out.append(b)
        terms[active].append(b)
        i += 1
    return RttTerminalSplit(bytes(all_out), {k: bytes(v) for k, v in terms.items() if v})


def strip_ansi(data: bytes) -> bytes:
    return ANSI_RE.sub(b"", data)


def printable(data: bytes, strip_control: bool = False) -> str:
    if strip_control:
        data = strip_ansi(data)
    return data.decode('utf-8', errors='replace')
