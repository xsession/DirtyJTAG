"""DJP2 frame codec and USB CDC transport shared by host tools."""
from __future__ import annotations

import binascii
import struct
import time

try:
    import serial
except ImportError:
    serial = None


MAGIC = 0x32504A44
VER = 2
HDR = 20
MAX_PAYLOAD = 2048
CMD = {
    'hello': 1, 'list': 2, 'config': 3, 'status': 4, 'devices': 5,
    'enter': 0x10, 'leave': 0x11, 'identify': 0x12,
    'erase': 0x20, 'read': 0x21, 'write': 0x22, 'raw': 0x30,
    'power': 0x40, 'measure': 0x41, 'pinmap': 0x42, 'vpp': 0x43, 'phy-info': 0x44, 'script': 0x45,
    'bridge-info': 0x46, 'bridge-gpio': 0x47, 'bridge-spi': 0x48, 'bridge-i2c': 0x49, 'bridge-uart': 0x4a, 'power-trace': 0x4b,
    'safety-status': 0x4c, 'safety-arm': 0x4d, 'safety-disarm': 0x4e,
    'debug-info': 0x51, 'debug-attach': 0x52, 'debug-detach': 0x53,
    'debug-halt': 0x54, 'debug-run': 0x55, 'debug-step': 0x56, 'debug-reset': 0x57,
    'debug-reg-read': 0x58, 'debug-reg-write': 0x59, 'debug-bp-set': 0x5a, 'debug-bp-clear': 0x5b,
    'rtt-scan': 0x60, 'rtt-info': 0x61, 'rtt-read': 0x62, 'rtt-write': 0x63, 'rtt-channel-info': 0x64,
    'swo-config': 0x68, 'swo-start': 0x69, 'swo-stop': 0x6a, 'swo-read': 0x6b, 'swo-status': 0x6c,
    'safe': 0x7e,
}
PROTO = {
    'dspic': 1, 'pic24': 2, 'pic-raw': 3, 'avr-isp': 10, 'updi': 11, 'tpi': 12, 'pdi': 13,
    'swim': 20, 'sbw': 30, 'msp430-jtag': 31, 'c2': 40, 'rl78': 50, 'swd': 60, 'jtag': 61,
    'tms320': 70, 'c2000': 70, 'xds110v3': 70, 'simplelink-swd': 80, 'cc13xx-swd': 80,
    'cc26xx-swd': 80, 'simplelink-cjtag': 81, 'cc13xx-cjtag': 81, 'cc26xx-cjtag': 81,
}
POWER = {'off': 0, 'external': 1, '3v3': 2, '5v': 3}


def crc32(data, seed=0):
    return binascii.crc32(data, seed) & 0xffffffff


def pack(cmd, seq, payload=b'', flags=0, status=0):
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f'payload too large: {len(payload)} > {MAX_PAYLOAD}')
    header = struct.pack('<IHHHHHIH', MAGIC, VER, cmd, seq, status, flags, len(payload), 0)
    checksum = crc32(payload, crc32(header[:18]))
    folded_checksum = (checksum ^ (checksum >> 16)) & 0xffff
    return header[:18] + struct.pack('<H', folded_checksum) + payload


def unpack(data):
    if len(data) < HDR:
        raise ValueError('short DJP2 frame')
    magic, version, cmd, seq, status, flags, length, folded_checksum = struct.unpack(
        '<IHHHHHIH', data[:HDR])
    if magic != MAGIC or version != VER or len(data) != HDR + length:
        raise ValueError('bad DJP2 frame')
    checksum = crc32(data[HDR:], crc32(data[:18]))
    if ((checksum ^ (checksum >> 16)) & 0xffff) != folded_checksum:
        raise ValueError('bad DJP2 CRC')
    return cmd, seq, status, flags, data[HDR:]


class Link:
    """A sequenced request/reply connection to a DirtyJTAG DJP2 device."""

    def __init__(self, port, baud=115200, timeout=3, connect_delay=.1):
        if serial is None:
            raise SystemExit('Install pyserial: python -m pip install pyserial')
        self.s = serial.Serial(port, baudrate=baud, timeout=timeout)
        self.seq = 1
        time.sleep(connect_delay)
        self.s.reset_input_buffer()

    def x(self, cmd, payload=b''):
        request = pack(cmd, self.seq, payload)
        expected_sequence = self.seq
        self.seq = (self.seq + 1) & 0xffff
        self.s.write(request)
        self.s.flush()
        header = self.s.read(HDR)
        if len(header) != HDR:
            raise TimeoutError('no DJP2 reply')
        length = struct.unpack_from('<I', header, 14)[0]
        body = self.s.read(length)
        _, sequence, status, _, response = unpack(header + body)
        if sequence != expected_sequence:
            raise RuntimeError(f'DJP2 sequence mismatch {sequence}!={expected_sequence}')
        if status:
            raise RuntimeError(f'DJP2 device status={status} cmd=0x{cmd:02x}')
        return response

    def close(self):
        self.s.close()