#!/usr/bin/env python3
from __future__ import annotations
import os
import sys
import tempfile
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
from host.updi_utils import (load_updi_profile, make_updi_nvm_plan, summarize_updi_plan,
                             updi_ldcs, updi_nvmp_key_bytes, updi_stcs)

HEX=''':100000000C945C000C946E000C946E000C946E00CA
:00000001FF
'''
with tempfile.NamedTemporaryFile('w', delete=False, suffix='.hex') as f:
    f.write(HEX)
    path=f.name
try:
    prof=load_updi_profile('attiny817')
    plan=make_updi_nvm_plan(path, prof)
    assert len(plan)==1
    assert plan[0].file_address==0
    assert plan[0].target_address==prof['flash_base']
    assert plan[0].data[:2]==bytes([0x0c,0x94])
    assert '1 pages' in summarize_updi_plan(plan)
    assert updi_nvmp_key_bytes()==b'NVMProg '
    assert updi_ldcs(2)==bytes([0x55,0x82])
    assert updi_stcs(3,0xaa)==bytes([0x55,0xc3,0xaa])
finally:
    os.unlink(path)
print('test_updi_utils: PASS')
