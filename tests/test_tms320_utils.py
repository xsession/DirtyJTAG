#!/usr/bin/env python3
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'host'))
from tms320_utils import *

profiles = list_tms320_profiles()
assert any(p.get('device') == 'TMS320F2800137' for p in profiles)
p = load_tms320_profile('tms320f2800137')
assert p['core'] == 'C28x'
assert pack_bits_lsb(0x1234, 16) == b'\x34\x12'
pay = tms320_shift_payload(TMS320_RAW_SHIFT_IR, 6, b'\x2a')
assert pay == bytes([TMS320_RAW_SHIFT_IR, 6, 0, 0x2a])
assert tms320_idcode_payload() == bytes([TMS320_RAW_SHIFT_DR, 32, 0, 0, 0, 0, 0])
assert format_idcode(b'\x78\x56\x34\x12') == '0x12345678'
print('test_tms320_utils: PASS')
