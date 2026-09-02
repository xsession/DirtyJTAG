#!/usr/bin/env python3
from __future__ import annotations
import os
import sys
import tempfile
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
from host.avr_utils import (avr_isp_extended_address_for_byte, avr_memop_from_ihex,
                            load_avr_profile, summarize_avr_extended_windows,
                            summarize_avr_pages)

# Intel HEX with 16 bytes at 0 and EOF.
HEX = ''':100000000C945C000C946E000C946E000C946E00CA
:00000001FF
'''
HEX_HIGH = ''':020000040002F8
:100000000C945C000C946E000C946E000C946E00CA
:00000001FF
'''

with tempfile.NamedTemporaryFile('w', delete=False, suffix='.hex') as f:
    f.write(HEX)
    path = f.name
try:
    prof = load_avr_profile('atmega328p')
    pages = avr_memop_from_ihex(path, prof)
    assert len(pages) == 1
    assert pages[0].address == 0
    assert len(pages[0].data) == prof['page_size']
    assert pages[0].data[:4] == bytes([0x0c, 0x94, 0x5c, 0x00])
    assert pages[0].data[16:] == b'\xff' * (prof['page_size'] - 16)
    assert '1 pages' in summarize_avr_pages(pages)
    assert avr_isp_extended_address_for_byte(0x00000) == 0
    assert avr_isp_extended_address_for_byte(0x1ffff) == 0
    assert avr_isp_extended_address_for_byte(0x20000) == 1
    mega = load_avr_profile('atmega2560')
    assert mega['requires_extended_address'] is True
    with tempfile.NamedTemporaryFile('w', delete=False, suffix='.hex') as hf:
        hf.write(HEX_HIGH)
        hpath = hf.name
    try:
        high_pages = avr_memop_from_ihex(hpath, mega)
        assert high_pages[0].address == 0x20000
        assert summarize_avr_extended_windows(high_pages) == [1]
    finally:
        os.unlink(hpath)
finally:
    os.unlink(path)
print('test_avr_utils: PASS')
