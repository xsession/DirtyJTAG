#!/usr/bin/env python3
"""Intel HEX helpers for DirtyJTAG Universal Pico.

The dsPIC/PIC24 path intentionally lives on the host.  The firmware only
receives already-row-aligned 24-bit words, which keeps target algorithms
small and auditable.
"""
from __future__ import annotations
from dataclasses import dataclass
from typing import Dict, Iterable, List, Tuple

class HexError(ValueError):
    pass

@dataclass(frozen=True)
class HexRecord:
    rectype: int
    address: int
    data: bytes

@dataclass(frozen=True)
class DspicRow:
    pc_address: int
    words: Tuple[int, ...]


def _parse_byte(s: str, off: int) -> int:
    try:
        return int(s[off:off + 2], 16)
    except ValueError as exc:
        raise HexError(f"invalid hex byte at offset {off}") from exc


def parse_ihex_lines(lines: Iterable[str]) -> List[HexRecord]:
    """Parse Intel HEX records and validate checksums."""
    records: List[HexRecord] = []
    for lineno, raw in enumerate(lines, 1):
        line = raw.strip()
        if not line:
            continue
        if not line.startswith(":"):
            raise HexError(f"line {lineno}: missing ':'")
        body = line[1:]
        if len(body) < 10 or len(body) % 2:
            raise HexError(f"line {lineno}: malformed record length")
        n = _parse_byte(body, 0)
        addr = (_parse_byte(body, 2) << 8) | _parse_byte(body, 4)
        rectype = _parse_byte(body, 6)
        expected_len = 2 * (5 + n)
        if len(body) != expected_len:
            raise HexError(f"line {lineno}: byte count mismatch")
        data = bytes(_parse_byte(body, 8 + 2 * i) for i in range(n))
        chk = _parse_byte(body, 8 + 2 * n)
        total = n + (addr >> 8) + (addr & 0xff) + rectype + sum(data) + chk
        if (total & 0xff) != 0:
            raise HexError(f"line {lineno}: checksum mismatch")
        records.append(HexRecord(rectype, addr, data))
        if rectype == 1:
            break
    return records


def parse_ihex_file(path: str) -> Dict[int, int]:
    """Return a sparse absolute byte-addressed memory map."""
    with open(path, "r", encoding="ascii") as f:
        records = parse_ihex_lines(f)
    upper = 0
    mem: Dict[int, int] = {}
    eof = False
    for rec in records:
        if rec.rectype == 0x00:
            base = upper + rec.address
            for i, b in enumerate(rec.data):
                a = base + i
                old = mem.get(a)
                if old is not None and old != b:
                    raise HexError(f"conflicting data at 0x{a:08x}")
                mem[a] = b
        elif rec.rectype == 0x01:
            eof = True
            break
        elif rec.rectype == 0x02:
            if len(rec.data) != 2:
                raise HexError("bad extended segment address record")
            upper = (((rec.data[0] << 8) | rec.data[1]) << 4)
        elif rec.rectype == 0x04:
            if len(rec.data) != 2:
                raise HexError("bad extended linear address record")
            upper = (((rec.data[0] << 8) | rec.data[1]) << 16)
        elif rec.rectype in (0x03, 0x05):
            # Start-address records are not needed for programming.
            continue
        else:
            raise HexError(f"unsupported Intel HEX record type 0x{rec.rectype:02x}")
    if not eof:
        raise HexError("missing EOF record")
    return mem


def dspic_words_from_ihex(path: str, include_config: bool = False,
                          config_base: int = 0xF80000) -> Dict[int, int]:
    """Convert common Microchip INHX32 dsPIC/PIC24 HEX to PC-addressed words.

    XC16/MPLAB HEX commonly stores each 24-bit program instruction in four
    Intel-HEX bytes: low, high, upper, phantom.  Program-counter addresses
    advance by two for each 24-bit instruction, so byte address 0 maps to
    PC address 0, byte address 4 maps to PC address 2, etc.

    Configuration/user-ID rows live high in the address space.  They are
    excluded by default because the first safe milestone is application flash.
    Use --include-config only after validating the exact device algorithm.
    """
    mem = parse_ihex_file(path)
    if not mem:
        return {}
    words: Dict[int, int] = {}
    for base in range((min(mem) // 4) * 4, ((max(mem) // 4) + 1) * 4, 4):
        present = [base + i in mem for i in range(4)]
        if not any(present):
            continue
        if base >= config_base and not include_config:
            continue
        # Accept sparse final words but fill unspecified bytes with erased state.
        b0 = mem.get(base + 0, 0xff)
        b1 = mem.get(base + 1, 0xff)
        b2 = mem.get(base + 2, 0xff)
        phantom = mem.get(base + 3, 0x00)
        if phantom not in (0x00, 0xff):
            raise HexError(f"unexpected dsPIC phantom byte 0x{phantom:02x} at 0x{base+3:08x}")
        pc = base // 2
        word = b0 | (b1 << 8) | (b2 << 16)
        if pc in words and words[pc] != word:
            raise HexError(f"conflicting dsPIC word at PC 0x{pc:06x}")
        words[pc] = word
    return words


def make_dspic_rows(words: Dict[int, int], row_words: int, user_end_pc: int,
                    fill_word: int = 0xffffff) -> List[DspicRow]:
    if row_words <= 0:
        raise HexError("row_words must be positive")
    if fill_word & ~0xffffff:
        raise HexError("fill word must be 24-bit")
    good: Dict[int, int] = {}
    for pc, word in words.items():
        if pc & 1:
            raise HexError(f"unaligned dsPIC PC address 0x{pc:06x}")
        if word & ~0xffffff:
            raise HexError(f"non-24-bit dsPIC word at 0x{pc:06x}")
        if pc > user_end_pc:
            raise HexError(f"program word 0x{pc:06x} exceeds user flash end 0x{user_end_pc:06x}")
        good[pc] = word
    if not good:
        return []
    row_span_pc = row_words * 2
    first = (min(good) // row_span_pc) * row_span_pc
    last = (max(good) // row_span_pc) * row_span_pc
    rows: List[DspicRow] = []
    for base in range(first, last + 1, row_span_pc):
        vals = tuple(good.get(base + 2 * i, fill_word) for i in range(row_words))
        if any(v != fill_word for v in vals):
            rows.append(DspicRow(base, vals))
    return rows


def pack_dspic_row_words(words: Iterable[int]) -> bytes:
    out = bytearray()
    for w in words:
        if w & ~0xffffff:
            raise HexError(f"non-24-bit word 0x{w:x}")
        out.extend((w & 0xff, (w >> 8) & 0xff, (w >> 16) & 0xff))
    return bytes(out)


def summarize_dspic_rows(rows: List[DspicRow]) -> str:
    if not rows:
        return "0 rows"
    total_words = sum(len(r.words) for r in rows)
    return (f"{len(rows)} rows, {total_words} instruction words, "
            f"PC 0x{rows[0].pc_address:06x}..0x{rows[-1].pc_address + 2*(len(rows[-1].words)-1):06x}")
