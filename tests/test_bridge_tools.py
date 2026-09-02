#!/usr/bin/env python3
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import struct
from host.bridge_tools import bridge_spi_payload, bridge_i2c_payload, bridge_uart_payload, decode_power_trace

p=bridge_spi_payload(bytes.fromhex('9f00'), hz=1000000, mode=0, cs_role=4)
assert len(p)==12 and p[0]==0 and p[1]==4 and struct.unpack_from('<I',p,4)[0]==1000000
p=bridge_i2c_payload(0x50, b'\x00\x10', 4)
assert p[0]==0x50 and struct.unpack_from('<HH',p,1)==(2,4)
p=bridge_uart_payload(b'AT\r', baud=115200)
assert struct.unpack_from('<I',p,0)[0]==115200
samples=decode_power_trace(struct.pack('<IIII',0,3300,12000,42|0x80000000))
assert samples[0]['fault'] and samples[0]['itarget_ma']==42
print('test_bridge_tools: PASS')
