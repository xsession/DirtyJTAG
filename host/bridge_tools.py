#!/usr/bin/env python3
"""Host-side bridge command payload helpers."""
from __future__ import annotations
import struct

def bridge_spi_payload(tx: bytes, hz: int=100000, mode: int=0, cs_role: int=4, flags: int=0) -> bytes:
    if not (0 <= mode <= 3): raise ValueError('SPI mode must be 0..3')
    if len(tx) > 2048-10: raise ValueError('SPI transfer too large')
    return struct.pack('<BBHIH', mode, cs_role, flags, hz, len(tx)) + tx

def bridge_i2c_payload(addr: int, tx: bytes=b'', rxlen: int=0, hz: int=100000) -> bytes:
    if not (0 <= addr <= 0x7f): raise ValueError('I2C address must be 7-bit')
    if len(tx) > 2048-9 or rxlen > 2048: raise ValueError('I2C transfer too large')
    return struct.pack('<BHHI', addr, len(tx), rxlen, hz) + tx

def bridge_uart_payload(tx: bytes=b'', rxlen: int=0, baud: int=115200, flags: int=0) -> bytes:
    if baud <= 0: raise ValueError('baud must be positive')
    if len(tx) > 2048-10 or rxlen > 2048: raise ValueError('UART transfer too large')
    return struct.pack('<IHHH', baud, flags, len(tx), rxlen) + tx

def decode_power_trace(payload: bytes):
    if len(payload) % 16: raise ValueError('power trace response must contain 16-byte samples')
    out=[]
    for off in range(0,len(payload),16):
        t, vt, vp, cur = struct.unpack_from('<IIII', payload, off)
        out.append({'t_ms':t,'vtarget_mv':vt,'vpp_mv':vp,'itarget_ma':cur & 0x7fffffff,'fault':bool(cur & 0x80000000)})
    return out
