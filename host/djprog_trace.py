"""RTT and SWO capture commands for the DirtyJTAG host CLI."""
from __future__ import annotations

import struct
import time

try:
    from djp2 import CMD, MAX_PAYLOAD
    from rtt_tools import printable, split_virtual_terminals
except ImportError:
    from host.djp2 import CMD, MAX_PAYLOAD
    from host.rtt_tools import printable, split_virtual_terminals


def rtt_scan(link, start, end):
    response = link.x(CMD['rtt-scan'], struct.pack('<II', start, end))
    if len(response) != 4:
        raise RuntimeError(f'unexpected RTT scan response length {len(response)}')
    return struct.unpack('<I', response)[0]


def rtt_info(link, control_block):
    response = link.x(CMD['rtt-info'], struct.pack('<I', control_block))
    if len(response) != 60:
        raise RuntimeError(f'unexpected RTT info response length {len(response)}')
    keys = ('cb', 'max_up', 'max_down', 'up_name', 'up_buffer', 'up_size', 'up_wr', 'up_rd', 'up_flags',
            'down_name', 'down_buffer', 'down_size', 'down_wr', 'down_rd', 'down_flags')
    return dict(zip(keys, struct.unpack('<15I', response)))


def print_rtt_info(info):
    print(f'RTT cb=0x{info["cb"]:08x} up={info["max_up"]} down={info["max_down"]}')
    print(f'  up0:   buf=0x{info["up_buffer"]:08x} size={info["up_size"]} wr={info["up_wr"]} rd={info["up_rd"]} flags=0x{info["up_flags"]:08x}')
    print(f'  down0: buf=0x{info["down_buffer"]:08x} size={info["down_size"]} wr={info["down_wr"]} rd={info["down_rd"]} flags=0x{info["down_flags"]:08x}')


def rtt_read(link, control_block, channel, length):
    if length > MAX_PAYLOAD:
        raise ValueError(f'RTT read length limited to {MAX_PAYLOAD}')
    return link.x(CMD['rtt-read'], struct.pack('<IBH', control_block, channel, length))


def rtt_write(link, control_block, channel, data):
    if len(data) > MAX_PAYLOAD - 5:
        raise ValueError(f'RTT write limited to {MAX_PAYLOAD - 5} bytes')
    response = link.x(CMD['rtt-write'], struct.pack('<IB', control_block, channel) + data)
    if len(response) != 4:
        raise RuntimeError(f'unexpected RTT write response length {len(response)}')
    return struct.unpack('<I', response)[0]


def rtt_channel_info(link, control_block, direction, channel):
    response = link.x(CMD['rtt-channel-info'], struct.pack('<IBB', control_block, direction, channel))
    if len(response) < 36:
        raise RuntimeError(f'unexpected RTT channel response length {len(response)}')
    response_direction, response_channel, name_length = response[0], response[1], response[2]
    values = struct.unpack_from('<8I', response, 4)
    name = response[36:36 + name_length].decode(errors='replace') if len(response) >= 36 + name_length else ''
    return dict(direction=response_direction, channel=response_channel, name=name, name_addr=values[0], buffer=values[1],
                size=values[2], wr=values[3], rd=values[4], flags=values[5], used=values[6], free=values[7])


def rtt_print_channels(link, control_block):
    info = rtt_info(link, control_block)
    print(f'RTT cb=0x{control_block:08x} up={info["max_up"]} down={info["max_down"]}')
    for direction, count, label in ((0, info['max_up'], 'up'), (1, info['max_down'], 'down')):
        for channel in range(count):
            channel_info = rtt_channel_info(link, control_block, direction, channel)
            print(f'  {label}{channel}: name={channel_info["name"] or "-"!s:16s} buf=0x{channel_info["buffer"]:08x} size={channel_info["size"]:6d} wr={channel_info["wr"]:6d} rd={channel_info["rd"]:6d} used={channel_info["used"]:6d} free={channel_info["free"]:6d} flags=0x{channel_info["flags"]:08x}')


def rtt_log(link, control_block, channel, output, seconds, interval_ms, append=False):
    mode = 'ab' if append else 'wb'
    end = None if seconds is None else time.time() + seconds
    total = 0
    with open(output, mode) as stream:
        while end is None or time.time() < end:
            data = rtt_read(link, control_block, channel, min(1024, MAX_PAYLOAD))
            if data:
                stream.write(data)
                stream.flush()
                total += len(data)
            time.sleep(max(interval_ms, 1) / 1000.0)
    print(f'logged {total} bytes to {output}')


def rtt_tail(link, control_block, channel, seconds, interval_ms, terminal=None, strip_ansi=False):
    end = None if seconds is None else time.time() + seconds
    while end is None or time.time() < end:
        data = rtt_read(link, control_block, channel, min(1024, MAX_PAYLOAD))
        if data and terminal is not None:
            data = split_virtual_terminals(data).terminals.get(terminal, b'')
        if data:
            print(printable(data, strip_control=strip_ansi), end='', flush=True)
        time.sleep(max(interval_ms, 1) / 1000.0)


def rtt_terminals_once(link, control_block, length, terminal=None, strip_ansi=False):
    terminals = split_virtual_terminals(rtt_read(link, control_block, 0, length)).terminals
    if terminal is not None:
        print(printable(terminals.get(terminal, b''), strip_control=strip_ansi), end='')
        return
    for terminal_id, payload in sorted(terminals.items()):
        print(f'--- terminal {terminal_id} ---')
        print(printable(payload, strip_control=strip_ansi), end='')
        if payload and not payload.endswith(b'\n'):
            print()


def swo_config(link, baud, flags):
    link.x(CMD['swo-config'], struct.pack('<II', baud, flags))


def swo_status(link):
    response = link.x(CMD['swo-status'])
    if len(response) != 20:
        raise RuntimeError(f'unexpected SWO status length {len(response)}')
    baud, flags, available, dropped = struct.unpack_from('<IIII', response, 0)
    return dict(baud=baud, flags=flags, available=available, dropped=dropped, active=bool(response[16]))


def swo_read(link, length):
    if length > MAX_PAYLOAD:
        raise ValueError(f'SWO read length limited to {MAX_PAYLOAD}')
    return link.x(CMD['swo-read'], struct.pack('<H', length))


def swo_tail(link, seconds, interval_ms, strip_ansi=False):
    end = None if seconds is None else time.time() + seconds
    while end is None or time.time() < end:
        data = swo_read(link, min(1024, MAX_PAYLOAD))
        if data:
            print(printable(data, strip_control=strip_ansi), end='', flush=True)
        time.sleep(max(interval_ms, 1) / 1000.0)