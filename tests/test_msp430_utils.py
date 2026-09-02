#!/usr/bin/env python3
from pathlib import Path
import tempfile
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'host'))

from msp430_utils import (MSP430_RAW_SHIFT_IR, MSP430_RAW_TAP_RESET,
                          load_msp430_profile, make_msp430_segments,
                          msp430_shift_payload, parse_ti_txt_lines,
                          summarize_msp430_segments)
from hex_utils import HexError

sample = [
    '@C000',
    '31 40 00 04 b0 12 34 56',
    '@FFE0',
    '00 C0 02 C0',
    'q',
]
mem = parse_ti_txt_lines(sample)
assert mem[0xC000] == 0x31
assert mem[0xC001] == 0x40
assert mem[0xFFE3] == 0xC0

prof = load_msp430_profile('msp430g2553')
segments = make_msp430_segments(mem, prof)
assert len(segments) == 2
assert segments[0].address == 0xC000
assert segments[0].data[:2] == b'\x31\x40'
assert 'segments' in summarize_msp430_segments(segments)

try:
    make_msp430_segments({0x0200: 0x11}, prof)
    raise AssertionError('out-of-range data accepted')
except HexError:
    pass

payload = msp430_shift_payload(MSP430_RAW_SHIFT_IR, 8, b'\x91')
assert payload == bytes([MSP430_RAW_SHIFT_IR, 8, 0, 0x91])
assert MSP430_RAW_TAP_RESET == 0

print('test_msp430_utils: PASS')
