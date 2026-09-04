"""Core programmer operations shared by the DirtyJTAG CLI command handlers."""
from __future__ import annotations

import struct

try:
    from djp2 import CMD, MAX_PAYLOAD, POWER, PROTO
    from hex_utils import dspic_words_from_ihex, make_dspic_rows, pack_dspic_row_words, summarize_dspic_rows
except ImportError:
    from host.djp2 import CMD, MAX_PAYLOAD, POWER, PROTO
    from host.hex_utils import dspic_words_from_ihex, make_dspic_rows, pack_dspic_row_words, summarize_dspic_rows


CAP_WRITE = 1 << 3
CAP_READ = 1 << 2
SAFETY_CONFIRM_PHRASE = 'I understand this can damage hardware'
SAFETY_FLAGS = {'erase': 1 << 0, 'write': 1 << 1, 'vpp': 1 << 2, 'script': 1 << 3,
                'bridge': 1 << 4, 'power': 1 << 5, 'debug': 1 << 6}


def safety_arm(link, flags, uses=100000):
    if not flags:
        return
    phrase = SAFETY_CONFIRM_PHRASE.encode()
    link.x(CMD['safety-arm'], struct.pack('<IIB', flags, uses, len(phrase)) + phrase)


def safety_status(link):
    response = link.x(CMD['safety-status'])
    if len(response) != 8:
        raise RuntimeError(f'unexpected safety-status length {len(response)}')
    flags, uses = struct.unpack('<II', response)
    names = [name for name, bit in SAFETY_FLAGS.items() if flags & bit]
    print(f'safety_armed={"|".join(names) if names else "none"} remaining_uses={uses}')


def command_safety_flags(args):
    command = getattr(args, 'cmd', '')
    flags = 0
    if command == 'erase':
        flags |= SAFETY_FLAGS['erase']
    if command in ('write', 'program') or (command in ('program-hex', 'program-avr-hex') and not getattr(args, 'dry_run', False)):
        flags |= SAFETY_FLAGS['write']
    if getattr(args, 'erase', False) and command in ('program', 'program-hex', 'program-avr-hex'):
        flags |= SAFETY_FLAGS['erase']
    if command == 'power':
        flags |= SAFETY_FLAGS['power']
    if command == 'vpp':
        flags |= SAFETY_FLAGS['vpp']
    if command == 'script':
        flags |= SAFETY_FLAGS['script']
    if command in ('bridge-gpio', 'bridge-spi', 'bridge-i2c', 'bridge-uart'):
        flags |= SAFETY_FLAGS['bridge']
    return flags


def require_safety_confirmation(args, flags):
    if not flags or getattr(args, 'safety_confirm', False):
        return
    names = ', '.join(name for name, bit in SAFETY_FLAGS.items() if flags & bit)
    raise SystemExit(f'Command requires explicit safety authorization for: {names}. Re-run with --safety-confirm after verifying target isolation, voltage, pinout, and risk controls.')


def cmd_config(link, args):
    name = args.device.encode()
    if len(name) > 47:
        raise ValueError('device name too long')
    config_flags = 1 if getattr(args, 'hv_activate', False) else 0
    payload = struct.pack('<HBBIIIHB', PROTO[args.protocol], POWER[args.power], config_flags, args.clock,
                          args.vtarget, args.flash_size, args.page_size, len(name)) + name
    link.x(CMD['config'], payload)


def decode_status(response):
    if len(response) < 28:
        raise RuntimeError(f'unexpected status length {len(response)}')
    pid, power, name_length, clock, caps, vtarget, vpp, current, flags = struct.unpack_from('<HBBIIIIII', response, 0)
    flash = page = config_flags = 0
    device = ''
    if len(response) >= 36:
        flash = struct.unpack_from('<I', response, 28)[0]
        page, config_flags = struct.unpack_from('<HH', response, 32)
        if name_length and len(response) >= 36 + name_length:
            device = response[36:36 + name_length].decode(errors='replace')
    return dict(pid=pid, power=power, clock=clock, caps=caps, vt=vtarget, vpp=vpp, current=current,
                fault=bool(flags & 1), flash=flash, page=page, cfg_flags=config_flags, device=device)


def get_devices(link):
    response = link.x(CMD['devices'])
    offset = 0
    devices = []
    while offset < len(response):
        if offset + 14 > len(response):
            raise RuntimeError('truncated device list')
        family, name_length, row, page, flags, end = struct.unpack_from('<BBHHII', response, offset)
        offset += 14
        if offset + name_length > len(response):
            raise RuntimeError('truncated device name')
        name = response[offset:offset + name_length].decode()
        offset += name_length
        devices.append(dict(family=family, name=name, row=row, page=page, flags=flags, end=end))
    return devices


def print_measure(response):
    if len(response) == 8:
        vtarget, vpp = struct.unpack('<II', response)
        print(f'VTARGET={vtarget / 1000:.3f} V  VPP={vpp / 1000:.3f} V')
        return
    if len(response) < 16:
        raise RuntimeError(f'unexpected measurement length {len(response)}')
    vtarget, vpp, current, flags = struct.unpack_from('<IIII', response)
    print(f'VTARGET={vtarget / 1000:.3f} V  VPP={vpp / 1000:.3f} V  ITARGET={current} mA  POWER_FAULT={bool(flags & 1)}')


def write_chunk(link, address, data):
    if len(data) > MAX_PAYLOAD - 8:
        raise ValueError('write chunk too large')
    link.x(CMD['write'], struct.pack('<II', address, len(data)) + data)


def read_chunk(link, address, length):
    if length > MAX_PAYLOAD:
        raise ValueError('read chunk too large')
    return link.x(CMD['read'], struct.pack('<II', address, length))


def dspic_row_plan_from_hex(path, device, include_config=False, fill_word=0xffffff):
    words = dspic_words_from_ihex(path, include_config=include_config)
    return make_dspic_rows(words, device['row'], device['end'], fill_word)


def program_dspic_hex(link, path, verify=False, erase=False, include_config=False, dry_run=False, fill_word=0xffffff):
    if link is None:
        raise RuntimeError('program-hex without --port requires --dry-run and --device-profile support from caller')
    status = decode_status(link.x(CMD['status']))
    if status['pid'] != PROTO['dspic']:
        raise RuntimeError('program-hex currently supports selected dspic backend only')
    if not status['caps'] & CAP_WRITE:
        raise RuntimeError('selected backend has no high-level WRITE capability')
    device = next((item for item in get_devices(link) if item['name'] == status['device']), None)
    if not device:
        raise RuntimeError('selected dsPIC device profile not found; CONFIG with --device first')
    rows = dspic_row_plan_from_hex(path, device, include_config=include_config, fill_word=fill_word)
    print('dsPIC HEX plan:', summarize_dspic_rows(rows))
    print(f'device={device["name"]} row_words={device["row"]} user_end=0x{device["end"]:06x}')
    if dry_run:
        for row in rows[:12]:
            nonblank = sum(1 for word in row.words if word != fill_word)
            print(f'  row PC 0x{row.pc_address:06x}: {nonblank}/{len(row.words)} non-erased words')
        if len(rows) > 12:
            print(f'  ... {len(rows) - 12} more rows')
        return
    if not rows:
        print('nothing to program')
        return
    if erase:
        link.x(CMD['erase'])
    for index, row in enumerate(rows, 1):
        block = pack_dspic_row_words(row.words)
        write_chunk(link, row.pc_address, block)
        if verify:
            received = read_chunk(link, row.pc_address, len(block))
            if received != block:
                for byte_index, (written, readback) in enumerate(zip(block, received)):
                    if written != readback:
                        pc = row.pc_address + 2 * (byte_index // 3)
                        raise RuntimeError(f'verify failed near PC 0x{pc:06x}, row byte +0x{byte_index:x}: wrote {written:02x}, read {readback:02x}')
                raise RuntimeError(f'verify mismatch at row PC 0x{row.pc_address:06x}')
        print(f'programmed row {index}/{len(rows)} at PC 0x{row.pc_address:06x}', end='\r', flush=True)
    print(f'programmed {len(rows)} dsPIC rows OK' + (' + verified' if verify else ''))