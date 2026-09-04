#!/usr/bin/env python3
"""DirtyJTAG Universal Programmer host CLI (DJP2 over USB CDC ACM)."""
from __future__ import annotations
import argparse, json, os, struct, time
try:
    from djp2 import CMD, HDR, MAX_PAYLOAD, POWER, PROTO, Link, crc32, pack, unpack
    from djprog_core import (CAP_READ, CAP_WRITE, SAFETY_CONFIRM_PHRASE, SAFETY_FLAGS,
                             cmd_config, command_safety_flags, decode_status, dspic_row_plan_from_hex,
                             get_devices, print_measure, program_dspic_hex, read_chunk,
                             require_safety_confirmation, safety_arm, safety_status, write_chunk)
    from djprog_trace import (print_rtt_info, rtt_channel_info, rtt_info, rtt_log, rtt_print_channels,
                              rtt_read, rtt_scan, rtt_tail, rtt_terminals_once, rtt_write, swo_config,
                              swo_read, swo_status, swo_tail)
    from hex_utils import (HexError, dspic_words_from_ihex, make_dspic_rows,
                           pack_dspic_row_words, summarize_dspic_rows)
    from avr_utils import (avr_memop_from_ihex, avrdude_like_signature,
                           avr_pages_need_extended_address, list_avr_profiles,
                           load_avr_profile, summarize_avr_extended_windows,
                           summarize_avr_pages)
    from updi_utils import (list_updi_profiles, load_updi_profile,
                            make_updi_nvm_plan, summarize_updi_plan,
                            updi_nvmp_key_bytes)
    from msp430_utils import (MSP430_RAW_CLOCK, MSP430_RAW_SHIFT_DR,
                              MSP430_RAW_SHIFT_IR, MSP430_RAW_TAP_RESET,
                              list_msp430_profiles, load_msp430_profile,
                              load_msp430_image, make_msp430_segments,
                              msp430_shift_payload, summarize_msp430_segments)
    from tms320_utils import (TMS320_RAW_CLOCK, TMS320_RAW_LINES,
                              TMS320_RAW_SHIFT_DR, TMS320_RAW_SHIFT_IR,
                              TMS320_RAW_TAP_RESET, format_idcode,
                              list_tms320_profiles, load_tms320_profile,
                              summarize_profile, tms320_idcode_payload,
                              tms320_shift_payload)
    from rtt_tools import split_virtual_terminals, printable
    from simplelink_utils import (list_simplelink_profiles, load_simplelink_profile,
                                  make_simplelink_boot_plan, summarize_boot_plan,
                                  summarize_simplelink_profile)
    from bridge_tools import bridge_spi_payload, bridge_i2c_payload, bridge_uart_payload, decode_power_trace
    from production_jobs import load_job, summarize_job, make_default_job
except ImportError:  # Allows direct unit import from outside host/.
    from host.djp2 import CMD, HDR, MAX_PAYLOAD, POWER, PROTO, Link, crc32, pack, unpack
    from host.djprog_core import (CAP_READ, CAP_WRITE, SAFETY_CONFIRM_PHRASE, SAFETY_FLAGS,
                                  cmd_config, command_safety_flags, decode_status, dspic_row_plan_from_hex,
                                  get_devices, print_measure, program_dspic_hex, read_chunk,
                                  require_safety_confirmation, safety_arm, safety_status, write_chunk)
    from host.djprog_trace import (print_rtt_info, rtt_channel_info, rtt_info, rtt_log, rtt_print_channels,
                                   rtt_read, rtt_scan, rtt_tail, rtt_terminals_once, rtt_write, swo_config,
                                   swo_read, swo_status, swo_tail)
    from host.hex_utils import (HexError, dspic_words_from_ihex, make_dspic_rows,
                                pack_dspic_row_words, summarize_dspic_rows)
    from host.avr_utils import (avr_memop_from_ihex, avrdude_like_signature,
                                avr_pages_need_extended_address, list_avr_profiles,
                                load_avr_profile, summarize_avr_extended_windows,
                                summarize_avr_pages)
    from host.updi_utils import (list_updi_profiles, load_updi_profile,
                                 make_updi_nvm_plan, summarize_updi_plan,
                                 updi_nvmp_key_bytes)
    from host.msp430_utils import (MSP430_RAW_CLOCK, MSP430_RAW_SHIFT_DR,
                                   MSP430_RAW_SHIFT_IR, MSP430_RAW_TAP_RESET,
                                   list_msp430_profiles, load_msp430_profile,
                                   load_msp430_image, make_msp430_segments,
                                   msp430_shift_payload, summarize_msp430_segments)
    from host.tms320_utils import (TMS320_RAW_CLOCK, TMS320_RAW_LINES,
                                   TMS320_RAW_SHIFT_DR, TMS320_RAW_SHIFT_IR,
                                   TMS320_RAW_TAP_RESET, format_idcode,
                                   list_tms320_profiles, load_tms320_profile,
                                   summarize_profile, tms320_idcode_payload,
                                   tms320_shift_payload)
    from host.rtt_tools import split_virtual_terminals, printable
    from host.simplelink_utils import (list_simplelink_profiles, load_simplelink_profile,
                                       make_simplelink_boot_plan, summarize_boot_plan,
                                       summarize_simplelink_profile)
    from host.bridge_tools import bridge_spi_payload, bridge_i2c_payload, bridge_uart_payload, decode_power_trace
    from host.production_jobs import load_job, summarize_job, make_default_job

FAMILY={0:'dsPIC30',1:'dsPIC33F',2:'dsPIC33E',3:'dsPIC33CK',4:'dsPIC33A'}
ALG_DIR=os.path.join(os.path.dirname(__file__), "algorithms")

def avr_isp_cmd(l, a, b, c, d):
    """Execute one 4-byte AVR ISP instruction through the backend raw path."""
    rx = l.x(CMD['raw'], bytes([a & 0xff, b & 0xff, c & 0xff, d & 0xff]))
    if len(rx) != 4:
        raise RuntimeError(f'AVR ISP raw command returned {len(rx)} bytes, expected 4')
    return rx

AVR_FUSE_READ = {
    'lfuse': (0x50, 0x00, 0x00, 0x00),
    'hfuse': (0x58, 0x08, 0x00, 0x00),
    'efuse': (0x50, 0x08, 0x00, 0x00),
    'lock':  (0x58, 0x00, 0x00, 0x00),
}
AVR_FUSE_WRITE = {
    'lfuse': (0xac, 0xa0, 0x00),
    'hfuse': (0xac, 0xa8, 0x00),
    'efuse': (0xac, 0xa4, 0x00),
    'lock':  (0xac, 0xe0, 0x00),
}

def avr_read_signature(l):
    return bytes(avr_isp_cmd(l, 0x30, 0x00, i, 0x00)[3] for i in range(3))

def avr_fuse_read(l, name):
    if name not in AVR_FUSE_READ:
        raise ValueError('fuse must be one of: ' + ', '.join(sorted(AVR_FUSE_READ)))
    return avr_isp_cmd(l, *AVR_FUSE_READ[name])[3]

def avr_fuse_write(l, name, value):
    if name not in AVR_FUSE_WRITE:
        raise ValueError('fuse must be one of: ' + ', '.join(sorted(AVR_FUSE_WRITE)))
    a, b, c = AVR_FUSE_WRITE[name]
    avr_isp_cmd(l, a, b, c, value & 0xff)
    time.sleep(0.015)

def ensure_avr_isp_selected(l, profile=None):
    st = decode_status(l.x(CMD['status']))
    if st['pid'] != PROTO['avr-isp']:
        raise RuntimeError('select avr-isp first: djprog.py --port <port> config avr-isp --device <part> --power 3v3|5v --page-size <bytes>')
    if profile and st['device'] and st['device'].lower() != profile['device'].lower():
        raise RuntimeError(f'selected device {st["device"]!r} does not match profile {profile["device"]!r}')
    return st

def configure_avr_from_profile(l, profile_name, power='external', clock=125000):
    prof = load_avr_profile(profile_name)
    if prof.get('protocol') != 'avr-isp':
        raise RuntimeError(f'profile {profile_name} uses protocol {prof.get("protocol")}, not avr-isp')
    device = prof['device'].encode()
    p = struct.pack('<HBBIIIHB', PROTO['avr-isp'], POWER[power], 0, int(clock), 3300,
                    int(prof['flash_size']), int(prof['page_size']), len(device)) + device
    l.x(CMD['config'], p)
    return prof


def print_avr_hex_plan(path, profile_name, fill=0xff):
    prof = load_avr_profile(profile_name)
    pages = avr_memop_from_ihex(path, prof, fill=fill)
    print('AVR HEX plan:', summarize_avr_pages(pages))
    ext_windows = summarize_avr_extended_windows(pages)
    ext_note = f" ext_windows={ext_windows}" if avr_pages_need_extended_address(pages) else ""
    print(f'device={prof["device"]} signature={avrdude_like_signature(prof.get("signature", []))} page_bytes={prof["page_size"]} flash=0x{prof["flash_size"]:x}{ext_note}')
    for pg in pages[:16]:
        nonblank = sum(1 for x in pg.data if x != fill)
        print(f'  page 0x{pg.address:05x}: {nonblank}/{len(pg.data)} programmed bytes')
    if len(pages) > 16:
        print(f'  ... {len(pages)-16} more pages')
    return prof, pages


def program_avr_hex(l, path, profile_name, erase=False, verify=False, dry_run=False, fill=0xff):
    prof = load_avr_profile(profile_name)
    if prof.get('protocol') != 'avr-isp':
        raise RuntimeError(f'program-avr-hex currently supports avr-isp profiles only; {profile_name} is {prof.get("protocol")}')
    ensure_avr_isp_selected(l, prof)
    prof, pages = print_avr_hex_plan(path, profile_name, fill=fill)
    if dry_run:
        return
    if erase:
        l.x(CMD['erase'])
    for idx, pg in enumerate(pages, 1):
        write_chunk(l, pg.address, pg.data)
        if verify:
            got = read_chunk(l, pg.address, len(pg.data))
            if got != pg.data:
                for off, (a, b) in enumerate(zip(pg.data, got)):
                    if a != b:
                        raise RuntimeError(f'AVR verify failed at 0x{pg.address + off:04x}: wrote {a:02x}, read {b:02x}')
                raise RuntimeError(f'AVR verify mismatch at page 0x{pg.address:04x}')
        print(f'programmed AVR page {idx}/{len(pages)} at 0x{pg.address:04x}', end='\r', flush=True)
    print(f'programmed {len(pages)} AVR pages OK' + (' + verified' if verify else ''))

def print_avr_profile_list():
    profs = list_avr_profiles()
    if not profs:
        print('no AVR profiles installed')
        return
    for prof in profs:
        if 'invalid' in prof:
            print(f'{prof.get("device", "?"):20s} invalid={prof["invalid"]}')
            continue
        impl = prof.get('implemented', {})
        implemented = ', '.join(k for k, v in impl.items() if v is True) or '-'
        partial = ', '.join(f'{k}={v}' for k, v in impl.items() if v is not True) or '-'
        print(f'{prof["device"]:20s} proto={prof.get("protocol", "-"):8s} sig={avrdude_like_signature(prof.get("signature", [])):10s} flash={prof.get("flash_size",0):6d} page={prof.get("page_size",0):4d} implemented=[{implemented}] partial=[{partial}]')



def print_updi_profile_list():
    profs = list_updi_profiles()
    if not profs:
        print('no UPDI profiles installed')
        return
    for prof in profs:
        if 'invalid' in prof:
            print(f'{prof.get("device", "?"):20s} invalid={prof["invalid"]}')
            continue
        impl = prof.get('implemented', {})
        implemented = ', '.join(k for k, v in impl.items() if v is True) or '-'
        partial = ', '.join(f'{k}={v}' for k, v in impl.items() if v is not True) or '-'
        print(f'{prof["device"]:20s} proto={prof.get("protocol", "-"):8s} flash={prof.get("flash_size",0):6d} page={prof.get("page_size",0):4d} nvm={prof.get("nvm_version","-")} implemented=[{implemented}] partial=[{partial}]')


def configure_updi_from_profile(l, profile_name, power='external', clock=115200, hv_activate=False):
    prof = load_updi_profile(profile_name)
    device = prof['device'].encode()
    flags = 1 if hv_activate else 0
    p = struct.pack('<HBBIIIHB', PROTO['updi'], POWER[power], flags, int(clock), 3300,
                    int(prof['flash_size']), int(prof['page_size']), len(device)) + device
    l.x(CMD['config'], p)
    return prof


def updi_plan_hex(path, profile_name, dry_run=True, experimental=False, erase=False, verify=False):
    (void_erase, void_verify) = (erase, verify)
    prof = load_updi_profile(profile_name)
    plan = make_updi_nvm_plan(path, prof)
    print('UPDI NVM plan:', summarize_updi_plan(plan))
    print(f'device={prof["device"]} nvm={prof.get("nvm_version", "-")} flash_base=0x{prof["flash_base"]:x} page={prof["page_size"]}')
    print('NVM programming key:', updi_nvmp_key_bytes().hex(' '), '(host-side descriptor only)')
    for row in plan[:16]:
        nonblank=sum(1 for x in row.data if x != 0xff)
        print(f'  page file=0x{row.file_address:05x} target=0x{row.target_address:05x}: {nonblank}/{len(row.data)} bytes')
    if len(plan)>16:
        print(f'  ... {len(plan)-16} more pages')
    if not dry_run:
        if not experimental:
            raise RuntimeError('UPDI NVM write path is guarded: add --experimental after bench validation')
        raise RuntimeError('UPDI NVM experimental write transport is not enabled in this offline build; use raw/script for bench traces first')


def print_msp430_profile_list():
    profs = list_msp430_profiles()
    if not profs:
        print('no MSP430 profiles installed')
        return
    for prof in profs:
        if 'invalid' in prof:
            print(f'{prof.get("device", "?"):20s} invalid={prof["invalid"]}')
            continue
        impl = prof.get('implemented', {})
        implemented = ', '.join(k for k, v in impl.items() if v is True) or '-'
        partial = ', '.join(f'{k}={v}' for k, v in impl.items() if v is not True) or '-'
        print(f'{prof["device"]:20s} family={prof.get("family", "-"):22s} iface={prof.get("default_interface", "-"):4s} addr={prof.get("address_bits",0):2d} flash=0x{prof.get("flash_start",0):05x}+0x{prof.get("flash_size",0):x} implemented=[{implemented}] partial=[{partial}]')


def configure_msp430_from_profile(l, profile_name, interface=None, power='external', clock=100000):
    prof = load_msp430_profile(profile_name)
    iface = (interface or prof.get('default_interface') or 'sbw').lower()
    if iface not in ('sbw', 'jtag'):
        raise RuntimeError('MSP430 interface must be sbw or jtag')
    proto = PROTO['sbw'] if iface == 'sbw' else PROTO['msp430-jtag']
    device = prof['device'].encode()
    p = struct.pack('<HBBIIIHB', proto, POWER[power], 0, int(clock), 3300,
                    int(prof['flash_size']), int(prof.get('write_block', 2)), len(device)) + device
    l.x(CMD['config'], p)
    return prof


def msp430_program_plan(path, profile_name, include_info=False, dry_run=True, experimental=False, fill=0xff):
    prof = load_msp430_profile(profile_name)
    mem = load_msp430_image(path)
    segs = make_msp430_segments(mem, prof, include_info=include_info, fill=fill)
    print('MSP430 image plan:', summarize_msp430_segments(segs))
    print(f'device={prof["device"]} family={prof.get("family", "-")} addr_bits={prof.get("address_bits",0)} flash=0x{prof["flash_start"]:05x}+0x{prof["flash_size"]:x}')
    for seg in segs[:16]:
        nonblank = sum(1 for x in seg.data if x != fill)
        print(f'  segment 0x{seg.address:05x}: {nonblank}/{len(seg.data)} bytes')
    if len(segs) > 16:
        print(f'  ... {len(segs)-16} more segments')
    if not dry_run:
        if not experimental:
            raise RuntimeError('MSP430 erase/write is guarded: use --experimental only after bench validation')
        raise RuntimeError('MSP430 high-level erase/write is not enabled in this build; use raw TAP/script for bench traces first')


def ensure_msp430_selected(l):
    st = decode_status(l.x(CMD['status']))
    if st['pid'] not in (PROTO['sbw'], PROTO['msp430-jtag']):
        raise RuntimeError('select MSP430 first: msp430-config <profile> --interface sbw|jtag')
    return st


def msp430_raw_shift(l, ir, bits, hexbytes):
    ensure_msp430_selected(l)
    data = bytes.fromhex(hexbytes)
    op = MSP430_RAW_SHIFT_IR if ir else MSP430_RAW_SHIFT_DR
    payload = msp430_shift_payload(op, bits, data)
    print(l.x(CMD['raw'], payload).hex(' '))


def msp430_tap_reset(l, cycles=8):
    ensure_msp430_selected(l)
    l.x(CMD['raw'], bytes([MSP430_RAW_TAP_RESET, cycles & 0xff]))


def msp430_clock(l, cycles, tms, tdi):
    ensure_msp430_selected(l)
    print(l.x(CMD['raw'], bytes([MSP430_RAW_CLOCK, cycles & 0xff, 1 if tms else 0, 1 if tdi else 0])).hex(' '))

def print_algorithm_list():
    if not os.path.isdir(ALG_DIR):
        print('no host algorithm descriptors installed')
        return
    for fn in sorted(os.listdir(ALG_DIR)):
        if not fn.endswith('.json'):
            continue
        path=os.path.join(ALG_DIR, fn)
        try:
            data=json.load(open(path, 'r', encoding='utf-8'))
        except Exception as exc:
            print(f'{fn}: invalid ({exc})')
            continue
        impl=data.get('implemented', {})
        implemented=', '.join(k for k,v in impl.items() if v is True) or '-'
        partial=', '.join(f'{k}={v}' for k,v in impl.items() if v is not True) or '-'
        print(f'{data.get("device", fn):18s} family={data.get("family","-"):9s} implemented=[{implemented}] partial=[{partial}]')

def program_binary(l,path,address,verify=False,erase=False):
    st=decode_status(l.x(CMD['status']))
    if not (st['caps'] & CAP_WRITE): raise RuntimeError('selected backend has no high-level WRITE capability')
    data=open(path,'rb').read()
    if erase: l.x(CMD['erase'])

    if st['pid']==PROTO['dspic']:
        dev=next((d for d in get_devices(l) if d['name']==st['device']),None)
        if not dev: raise RuntimeError('selected dsPIC device profile not found; CONFIG with --device first')
        chunk=dev['row']*3
        addr_step=dev['row']*2  # dsPIC program-counter address units are 2 per 24-bit instruction
        if len(data)%chunk: raise RuntimeError(f'dsPIC binary must be whole rows: {chunk} bytes/row for {dev["name"]}')
    elif st['pid']==PROTO['avr-isp']:
        chunk=st['page']
        if not chunk: raise RuntimeError('AVR ISP needs --page-size in CONFIG')
        addr_step=chunk
        if len(data)%chunk:
            data += b'\xff'*(chunk-(len(data)%chunk))
    else:
        chunk=min(MAX_PAYLOAD-8,1024)
        addr_step=chunk

    off=0; target=address
    while off<len(data):
        block=data[off:off+chunk]
        write_chunk(l,target,block)
        if verify:
            if not (st['caps'] & CAP_READ): raise RuntimeError('selected backend cannot verify by readback')
            got=read_chunk(l,target,len(block))
            if got!=block:
                for i,(a,b) in enumerate(zip(block,got)):
                    if a!=b: raise RuntimeError(f'verify failed at target 0x{target:x}, byte +0x{i:x}: wrote {a:02x}, read {b:02x}')
                raise RuntimeError(f'verify length/content mismatch at target 0x{target:x}')
        off+=len(block); target+=addr_step
        print(f'programmed {off}/{len(data)} bytes',end='\r',flush=True)
    print(f'programmed {len(data)} bytes OK' + (' + verified' if verify else ''))


def format_stm8_regs(b):
    if len(b) != 11:
        return b.hex(' ')
    a=b[0]; pc=(b[1]<<16)|(b[2]<<8)|b[3]; x=(b[4]<<8)|b[5]; y=(b[6]<<8)|b[7]; sp=(b[8]<<8)|b[9]; cc=b[10]
    return f'A=0x{a:02x} PC=0x{pc:06x} X=0x{x:04x} Y=0x{y:04x} SP=0x{sp:04x} CC=0x{cc:02x}'


def print_tms320_profile_list():
    profs = list_tms320_profiles()
    if not profs:
        print('no TMS320/C2000 profiles installed')
        return
    for prof in profs:
        if 'invalid' in prof:
            print(f'{prof.get("device", "?"):20s} invalid={prof["invalid"]}')
            continue
        impl = prof.get('implemented', {})
        implemented = ', '.join(k for k, v in impl.items() if v is True) or '-'
        partial = ', '.join(f'{k}={v}' for k, v in impl.items() if v is not True) or '-'
        print(f'{summarize_profile(prof)} implemented=[{implemented}] partial=[{partial}]')


def configure_tms320_from_profile(l, profile_name, power='external', clock=None):
    prof = load_tms320_profile(profile_name)
    if power == '5v':
        raise RuntimeError('XDS110/TMS320 JTAG VTREF is 1.8..3.6 V; do not select 5v')
    hz = int(clock or prof.get('default_clock_hz', 1000000))
    device = prof['device'].encode()
    p = struct.pack('<HBBIIIHB', PROTO['tms320'], POWER[power], 0, hz,
                    int(prof.get('vtref_mv_max', 3300)), 0, int(prof.get('ir_length', 6)), len(device)) + device
    l.x(CMD['config'], p)
    return prof


def ensure_tms320_selected(l):
    st = decode_status(l.x(CMD['status']))
    if st['pid'] != PROTO['tms320']:
        raise RuntimeError('select TMS320/C2000 first: tms320-config <profile> --power external|3v3')
    return st


def tms320_tap_reset(l, cycles=8):
    ensure_tms320_selected(l)
    l.x(CMD['raw'], bytes([TMS320_RAW_TAP_RESET, cycles & 0xff]))


def tms320_raw_shift(l, ir, bits, hexbytes):
    ensure_tms320_selected(l)
    data = bytes.fromhex(hexbytes)
    op = TMS320_RAW_SHIFT_IR if ir else TMS320_RAW_SHIFT_DR
    print(l.x(CMD['raw'], tms320_shift_payload(op, bits, data)).hex(' '))


def tms320_clock(l, cycles, tms, tdi):
    ensure_tms320_selected(l)
    print(l.x(CMD['raw'], bytes([TMS320_RAW_CLOCK, cycles & 0xff, 1 if tms else 0, 1 if tdi else 0])).hex(' '))


def tms320_lines(l, nreset=True, ntrst=True, emu_hint=1):
    ensure_tms320_selected(l)
    l.x(CMD['raw'], bytes([TMS320_RAW_LINES, 1 if nreset else 0, 1 if ntrst else 0, emu_hint & 0xff]))


def tms320_idcode(l):
    ensure_tms320_selected(l)
    tms320_tap_reset(l, 8)
    raw = l.x(CMD['raw'], tms320_idcode_payload())
    print(format_idcode(raw), raw.hex(' '))



def print_simplelink_profile_list():
    for name in list_simplelink_profiles():
        print(summarize_simplelink_profile(load_simplelink_profile(name)))

def configure_simplelink_from_profile(l, profile_name, interface='swd', power='external', clock=None):
    prof = load_simplelink_profile(profile_name)
    proto = 'simplelink-cjtag' if interface == 'cjtag' else 'simplelink-swd'
    hz = int(clock if clock is not None else 1000000)
    device = prof['device'].encode()
    payload = struct.pack('<HBBIIIHB', PROTO[proto], POWER[power], 0, hz, 3300,
                          int(prof['flash_size']), 0, len(device)) + device
    l.x(CMD['config'], payload)
    return prof

def simplelink_boot_plan(profile_name, transport='uart', interface='swd'):
    prof = load_simplelink_profile(profile_name)
    plan = make_simplelink_boot_plan(prof, transport=transport, debug_protocol=interface)
    print(summarize_boot_plan(plan))

def bridge_print_power_trace(l, samples, interval_ms):
    samples = max(1, min(samples, MAX_PAYLOAD // 16))
    b = l.x(CMD['power-trace'], struct.pack('<HHI', samples, interval_ms, 0))
    for s in decode_power_trace(b):
        print(f'{s["t_ms"]:8d} ms  VTARGET={s["vtarget_mv"]/1000:.3f} V  VPP={s["vpp_mv"]/1000:.3f} V  I={s["itarget_ma"]} mA  fault={s["fault"]}')

def print_bridge_info(l):
    print(l.x(CMD['bridge-info']).decode(errors='replace'))

def bridge_gpio(l, op, role, value=None):
    ops={'read':0,'write':1,'dir':2}
    payload=bytes([ops[op], role]) + (bytes([value & 0xff]) if value is not None else b'')
    b=l.x(CMD['bridge-gpio'], payload)
    if b: print(int(b[0]))
    else: print('OK')

def bridge_spi(l, hexbytes, hz=100000, mode=0, cs=4, lsb_first=False, cs_active_high=False):
    flags=(1 if lsb_first else 0) | (2 if cs_active_high else 0)
    data=bytes.fromhex(hexbytes)
    print(l.x(CMD['bridge-spi'], bridge_spi_payload(data, hz=hz, mode=mode, cs_role=cs, flags=flags)).hex(' '))

def bridge_i2c(l, addr, txhex='', rxlen=0, hz=100000):
    tx=bytes.fromhex(txhex) if txhex else b''
    print(l.x(CMD['bridge-i2c'], bridge_i2c_payload(addr, tx, rxlen, hz)).hex(' '))

def bridge_uart(l, text='', hexbytes=None, rxlen=0, baud=115200, invert=False):
    tx=bytes.fromhex(hexbytes) if hexbytes is not None else text.encode()
    flags=1 if invert else 0
    data=l.x(CMD['bridge-uart'], bridge_uart_payload(tx, rxlen, baud, flags))
    print(data.hex(' ') if hexbytes is not None else data.decode(errors='replace'))

def production_job_summary(path):
    print(summarize_job(load_job(path)))

def production_job_template(name, protocol, device, image, output):
    job=make_default_job(name, protocol, device, image)
    txt=json.dumps(job, indent=2)
    if output:
        open(output,'w',encoding='utf-8').write(txt+'\n')
        print(output)
    else:
        print(txt)

def main():
    ap=argparse.ArgumentParser(description='DirtyJTAG Universal Pico programmer/debugger CLI')
    ap.add_argument('--port', help='USB CDC ACM port; not required for offline algorithm-list')
    sp=ap.add_subparsers(dest='cmd',required=True)
    for name in ('hello','list','devices','status','enter','leave','identify','erase','measure','safe',
                 'phy-info','safety-status','safety-disarm','debug-info','debug-attach','debug-detach','debug-halt','debug-run','debug-step','debug-reg-read','debug-regs'):
        sp.add_parser(name)
    dr=sp.add_parser('debug-reset'); dr.add_argument('--halt',action='store_true')
    dw=sp.add_parser('debug-reg-write'); dw.add_argument('hexbytes')
    db=sp.add_parser('debug-bp-set'); db.add_argument('address',type=lambda x:int(x,0)); db.add_argument('--type',type=int,default=0); db.add_argument('--slot',type=int,default=0)
    dc=sp.add_parser('debug-bp-clear'); dc.add_argument('slot',type=int)
    rs=sp.add_parser('rtt-scan'); rs.add_argument('start',type=lambda x:int(x,0)); rs.add_argument('end',type=lambda x:int(x,0))
    ri=sp.add_parser('rtt-info'); ri.add_argument('control_block',type=lambda x:int(x,0))
    rr=sp.add_parser('rtt-read'); rr.add_argument('control_block',type=lambda x:int(x,0)); rr.add_argument('--channel',type=int,default=0); rr.add_argument('--length',type=int,default=256); rr.add_argument('-o','--output')
    rw=sp.add_parser('rtt-write'); rw.add_argument('control_block',type=lambda x:int(x,0)); rw.add_argument('data'); rw.add_argument('--channel',type=int,default=0); rw.add_argument('--hex',action='store_true',help='interpret DATA as hex bytes instead of UTF-8 text')
    rc=sp.add_parser('rtt-channels'); rc.add_argument('control_block',type=lambda x:int(x,0))
    rt=sp.add_parser('rtt-tail'); rt.add_argument('control_block',type=lambda x:int(x,0)); rt.add_argument('--channel',type=int,default=0); rt.add_argument('--seconds',type=float); rt.add_argument('--interval-ms',type=int,default=50); rt.add_argument('--terminal',type=int); rt.add_argument('--strip-ansi',action='store_true')
    rl=sp.add_parser('rtt-log'); rl.add_argument('control_block',type=lambda x:int(x,0)); rl.add_argument('-o','--output',required=True); rl.add_argument('--channel',type=int,default=1); rl.add_argument('--seconds',type=float,default=5.0); rl.add_argument('--interval-ms',type=int,default=20); rl.add_argument('--append',action='store_true')
    rv=sp.add_parser('rtt-terminals'); rv.add_argument('control_block',type=lambda x:int(x,0)); rv.add_argument('--length',type=int,default=1024); rv.add_argument('--terminal',type=int); rv.add_argument('--strip-ansi',action='store_true')
    sv=sp.add_parser('sysview-capture'); sv.add_argument('control_block',type=lambda x:int(x,0)); sv.add_argument('-o','--output',required=True); sv.add_argument('--channel',type=int,default=1); sv.add_argument('--seconds',type=float,default=5.0); sv.add_argument('--interval-ms',type=int,default=10); sv.add_argument('--append',action='store_true')
    sc=sp.add_parser('swo-config'); sc.add_argument('--baud',type=int,required=True); sc.add_argument('--flags',type=lambda x:int(x,0),default=0)
    sp.add_parser('swo-start')
    sp.add_parser('swo-stop')
    sp.add_parser('swo-status')
    sr=sp.add_parser('swo-read'); sr.add_argument('--length',type=int,default=256); sr.add_argument('-o','--output'); sr.add_argument('--strip-ansi',action='store_true')
    stail=sp.add_parser('swo-tail'); stail.add_argument('--seconds',type=float); stail.add_argument('--interval-ms',type=int,default=20); stail.add_argument('--strip-ansi',action='store_true')
    c=sp.add_parser('config'); c.add_argument('protocol',choices=PROTO); c.add_argument('--device',default='')
    c.add_argument('--power',choices=POWER,default='external'); c.add_argument('--clock',type=int,default=0)
    c.add_argument('--vtarget',type=int,default=3300); c.add_argument('--flash-size',type=int,default=0)
    c.add_argument('--page-size',type=int,default=0)
    c.add_argument('--hv-activate',action='store_true',help='request backend-specific HV activation (currently UPDI; still requires physical jumper)')
    r=sp.add_parser('read'); r.add_argument('address',type=lambda x:int(x,0)); r.add_argument('length',type=lambda x:int(x,0)); r.add_argument('-o','--output')
    w=sp.add_parser('write'); w.add_argument('address',type=lambda x:int(x,0)); w.add_argument('file')
    pg=sp.add_parser('program'); pg.add_argument('file'); pg.add_argument('--address',type=lambda x:int(x,0),default=0); pg.add_argument('--erase',action='store_true'); pg.add_argument('--verify',action='store_true')
    ph=sp.add_parser('program-hex'); ph.add_argument('file'); ph.add_argument('--erase',action='store_true'); ph.add_argument('--verify',action='store_true'); ph.add_argument('--include-config',action='store_true'); ph.add_argument('--dry-run',action='store_true'); ph.add_argument('--fill-word',type=lambda x:int(x,0),default=0xffffff)
    av=sp.add_parser('avr-profile-list')
    sp.add_parser('updi-profile-list')
    sp.add_parser('msp430-profile-list')
    sp.add_parser('tms320-profile-list')
    sp.add_parser('simplelink-profile-list')
    avc=sp.add_parser('avr-config'); avc.add_argument('profile'); avc.add_argument('--power',choices=POWER,default='external'); avc.add_argument('--clock',type=int,default=125000)
    avs=sp.add_parser('avr-signature')
    avf=sp.add_parser('avr-fuses'); avf.add_argument('--write',nargs=2,metavar=('NAME','VALUE'))
    avp=sp.add_parser('program-avr-hex'); avp.add_argument('file'); avp.add_argument('--profile',required=True); avp.add_argument('--erase',action='store_true'); avp.add_argument('--verify',action='store_true'); avp.add_argument('--dry-run',action='store_true'); avp.add_argument('--fill',type=lambda x:int(x,0),default=0xff)
    upc=sp.add_parser('updi-config'); upc.add_argument('profile'); upc.add_argument('--power',choices=POWER,default='external'); upc.add_argument('--clock',type=int,default=115200); upc.add_argument('--hv-activate',action='store_true')
    mspc=sp.add_parser('msp430-config'); mspc.add_argument('profile'); mspc.add_argument('--interface',choices=('sbw','jtag')); mspc.add_argument('--power',choices=POWER,default='external'); mspc.add_argument('--clock',type=int,default=100000)
    tmsc=sp.add_parser('tms320-config'); tmsc.add_argument('profile'); tmsc.add_argument('--power',choices=('external','3v3'),default='external'); tmsc.add_argument('--clock',type=int,default=None)
    slc=sp.add_parser('simplelink-config'); slc.add_argument('profile'); slc.add_argument('--interface',choices=('swd','cjtag'),default='swd'); slc.add_argument('--power',choices=('external','3v3'),default='external'); slc.add_argument('--clock',type=int,default=None)
    slb=sp.add_parser('simplelink-boot-plan'); slb.add_argument('profile'); slb.add_argument('--transport',choices=('uart','spi'),default='uart'); slb.add_argument('--interface',choices=('swd','cjtag'),default='swd')
    mspp=sp.add_parser('program-msp430'); mspp.add_argument('file'); mspp.add_argument('--profile',required=True); mspp.add_argument('--include-info',action='store_true'); mspp.add_argument('--dry-run',action='store_true',default=True); mspp.add_argument('--experimental',action='store_true'); mspp.add_argument('--fill',type=lambda x:int(x,0),default=0xff)
    mspi=sp.add_parser('msp430-shift-ir'); mspi.add_argument('bits',type=int); mspi.add_argument('hexbytes')
    mspd=sp.add_parser('msp430-shift-dr'); mspd.add_argument('bits',type=int); mspd.add_argument('hexbytes')
    mspr=sp.add_parser('msp430-tap-reset'); mspr.add_argument('--cycles',type=int,default=8)
    mspclk=sp.add_parser('msp430-clock'); mspclk.add_argument('cycles',type=int); mspclk.add_argument('--tms',action='store_true'); mspclk.add_argument('--tdi',action='store_true')
    tmsi=sp.add_parser('tms320-shift-ir'); tmsi.add_argument('bits',type=int); tmsi.add_argument('hexbytes')
    tmsd=sp.add_parser('tms320-shift-dr'); tmsd.add_argument('bits',type=int); tmsd.add_argument('hexbytes')
    tmsr=sp.add_parser('tms320-tap-reset'); tmsr.add_argument('--cycles',type=int,default=8)
    tmsclk=sp.add_parser('tms320-clock'); tmsclk.add_argument('cycles',type=int); tmsclk.add_argument('--tms',action='store_true'); tmsclk.add_argument('--tdi',action='store_true')
    sp.add_parser('tms320-idcode')
    tmsln=sp.add_parser('tms320-lines'); tmsln.add_argument('--reset',choices=('release','assert'),default='release'); tmsln.add_argument('--trst',choices=('release','assert'),default='release'); tmsln.add_argument('--emu-hint',type=int,default=1)
    upp=sp.add_parser('program-updi-hex'); upp.add_argument('file'); upp.add_argument('--profile',required=True); upp.add_argument('--erase',action='store_true'); upp.add_argument('--verify',action='store_true'); upp.add_argument('--dry-run',action='store_true',default=True); upp.add_argument('--experimental',action='store_true')
    sp.add_parser('bridge-info')
    bg=sp.add_parser('bridge-gpio'); bg.add_argument('op',choices=('read','write','dir')); bg.add_argument('role',type=int); bg.add_argument('value',type=lambda x:int(x,0),nargs='?')
    bs=sp.add_parser('bridge-spi'); bs.add_argument('hexbytes'); bs.add_argument('--hz',type=int,default=100000); bs.add_argument('--mode',type=int,default=0); bs.add_argument('--cs',type=int,default=4); bs.add_argument('--lsb-first',action='store_true'); bs.add_argument('--cs-active-high',action='store_true')
    bi=sp.add_parser('bridge-i2c'); bi.add_argument('addr',type=lambda x:int(x,0)); bi.add_argument('--tx',default=''); bi.add_argument('--rxlen',type=int,default=0); bi.add_argument('--hz',type=int,default=100000)
    bu=sp.add_parser('bridge-uart'); bu.add_argument('--text',default=''); bu.add_argument('--hex'); bu.add_argument('--rxlen',type=int,default=0); bu.add_argument('--baud',type=int,default=115200); bu.add_argument('--invert',action='store_true')
    pt=sp.add_parser('power-trace'); pt.add_argument('--samples',type=int,default=16); pt.add_argument('--interval-ms',type=int,default=10)
    pj=sp.add_parser('production-job-summary'); pj.add_argument('file')
    pjt=sp.add_parser('production-job-template'); pjt.add_argument('name'); pjt.add_argument('protocol'); pjt.add_argument('device'); pjt.add_argument('image'); pjt.add_argument('-o','--output')
    sp.add_parser('algorithm-list')
    x=sp.add_parser('raw'); x.add_argument('hexbytes')
    cap=sp.add_parser('dspic-load-capsule'); cap.add_argument('file')
    ci=sp.add_parser('dspic-capsule-info')
    xs=sp.add_parser('script'); xs.add_argument('hexbytes',help='execute electrical script VM bytecode over USB; returns captured bytes as hex')
    p=sp.add_parser('power'); p.add_argument('mode',choices=POWER)
    pm=sp.add_parser('pinmap'); pm.add_argument('gpio',type=int,nargs=6,metavar='GPIO')
    vp=sp.add_parser('vpp'); vp.add_argument('path',choices=('boost','apply','data0')); vp.add_argument('state',choices=('on','off'))
    a=ap.parse_args()
    if a.cmd=='algorithm-list':
        print_algorithm_list(); return
    if a.cmd=='avr-profile-list':
        print_avr_profile_list(); return
    if a.cmd=='updi-profile-list':
        print_updi_profile_list(); return
    if a.cmd=='msp430-profile-list':
        print_msp430_profile_list(); return
    if a.cmd=='tms320-profile-list':
        print_tms320_profile_list(); return
    if a.cmd=='simplelink-profile-list':
        print_simplelink_profile_list(); return
    if a.cmd=='simplelink-boot-plan':
        simplelink_boot_plan(a.profile,a.transport,a.interface); return
    if a.cmd=='production-job-summary':
        production_job_summary(a.file); return
    if a.cmd=='production-job-template':
        production_job_template(a.name,a.protocol,a.device,a.image,a.output); return
    if a.cmd=='program-avr-hex' and a.dry_run:
        print_avr_hex_plan(a.file,a.profile,fill=a.fill); return
    if a.cmd=='program-updi-hex' and a.dry_run:
        updi_plan_hex(a.file,a.profile,dry_run=True,experimental=a.experimental,erase=a.erase,verify=a.verify); return
    if a.cmd=='program-msp430' and a.dry_run:
        msp430_program_plan(a.file,a.profile,include_info=a.include_info,dry_run=True,experimental=a.experimental,fill=a.fill); return
    if not a.port:
        raise SystemExit('--port is required for USB operations')
    l=Link(a.port)
    safety_flags = command_safety_flags(a)
    require_safety_confirmation(a, safety_flags)
    if safety_flags:
        safety_arm(l, safety_flags, max(1, int(a.safety_uses)))

    if a.cmd=='hello': print(l.x(CMD['hello']).decode(errors='replace'))
    elif a.cmd=='list':
        b=l.x(CMD['list']); o=0
        while o<len(b):
            pid,n=struct.unpack_from('<HH',b,o); caps,maxhz=struct.unpack_from('<II',b,o+4)
            name=b[o+12:o+12+n].decode(); o+=12+n
            print(f'{pid:2d} {name:34s} caps=0x{caps:08x} max={maxhz} Hz')
    elif a.cmd=='devices':
        for d in get_devices(l):
            print(f'{d["name"]:24s} family={FAMILY.get(d["family"],d["family"])!s:10s} row={d["row"]:4d} page={d["page"]:4d} end=0x{d["end"]:06x} flags=0x{d["flags"]:08x}')
    elif a.cmd=='status':
        st=decode_status(l.x(CMD['status']))
        print(f'protocol={st["pid"]} device={st["device"] or "-"} power={st["power"]} clock={st["clock"]}Hz caps=0x{st["caps"]:08x} page={st["page"]} flash={st["flash"]} VTARGET={st["vt"]}mV VPP={st["vpp"]}mV ITARGET={st["current"]}mA fault={st["fault"]}')
    elif a.cmd=='config': cmd_config(l,a); print('OK')
    elif a.cmd in ('enter','leave','erase','safe'): l.x(CMD[a.cmd]); print('OK')
    elif a.cmd=='identify': print(l.x(CMD['identify']).hex(' '))
    elif a.cmd=='measure': print_measure(l.x(CMD['measure']))
    elif a.cmd=='power': l.x(CMD['power'],bytes([POWER[a.mode]])); print('OK')
    elif a.cmd=='pinmap':
        if any(x<0 or x>255 for x in a.gpio): raise SystemExit('GPIO numbers must fit u8')
        l.x(CMD['pinmap'],bytes(a.gpio)); print('OK')
    elif a.cmd=='vpp':
        path={'boost':0,'apply':1,'data0':2}[a.path]
        l.x(CMD['vpp'],bytes([path,1 if a.state=='on' else 0])); print('OK')
    elif a.cmd=='safety-status': safety_status(l)
    elif a.cmd=='safety-disarm': l.x(CMD['safety-disarm']); print('OK')
    elif a.cmd=='phy-info':
        b=l.x(CMD['phy-info'])
        if len(b)<4: raise RuntimeError(f'unexpected phy-info length {len(b)}')
        active,n=b[0],b[1]
        name=b[4:4+n].decode(errors='replace') if len(b)>=4+n else ''
        print(f'swim_phy={name or "-"} active={bool(active)}')
    elif a.cmd=='debug-info':
        b=l.x(CMD['debug-info'])
        if len(b)!=12: raise RuntimeError(f'unexpected debug-info length {len(b)}')
        pid,state,transport,caps,bps,width,_=struct.unpack('<HBBIHBB',b)
        states={0:'detached',1:'transport-ready',2:'attached',3:'running',4:'halted',5:'reset'}
        transports={0:'none',1:'openocd-remote-bitbang',2:'native'}
        print(f'protocol={pid} state={states.get(state,state)} transport={transports.get(transport,transport)} caps=0x{caps:08x} hw_breakpoints={bps} register_width={width}')
    elif a.cmd in ('debug-attach','debug-detach','debug-halt','debug-run','debug-step'):
        l.x(CMD[a.cmd]); print('OK')
    elif a.cmd=='debug-reset': l.x(CMD['debug-reset'],bytes([1 if a.halt else 0])); print('OK')
    elif a.cmd=='debug-reg-read': print(l.x(CMD['debug-reg-read']).hex(' '))
    elif a.cmd=='debug-regs': print(format_stm8_regs(l.x(CMD['debug-reg-read'])))
    elif a.cmd=='debug-reg-write': l.x(CMD['debug-reg-write'],bytes.fromhex(a.hexbytes)); print('OK')
    elif a.cmd=='debug-bp-set': l.x(CMD['debug-bp-set'],struct.pack('<IBB',a.address,a.type,a.slot)); print('OK')
    elif a.cmd=='debug-bp-clear': l.x(CMD['debug-bp-clear'],bytes([a.slot])); print('OK')
    elif a.cmd=='rtt-scan':
        cb = rtt_scan(l, a.start, a.end)
        print(f'RTT control block: 0x{cb:08x}')
    elif a.cmd=='rtt-info':
        print_rtt_info(rtt_info(l, a.control_block))
    elif a.cmd=='rtt-read':
        data = rtt_read(l, a.control_block, a.channel, a.length)
        if a.output:
            open(a.output, 'wb').write(data)
        else:
            try:
                print(data.decode('utf-8'), end='')
                if data and not data.endswith(b'\n'):
                    print()
            except UnicodeDecodeError:
                print(data.hex(' '))
    elif a.cmd=='rtt-write':
        payload = bytes.fromhex(a.data) if a.hex else a.data.encode('utf-8')
        written = rtt_write(l, a.control_block, a.channel, payload)
        print(f'wrote {written} bytes')
    elif a.cmd=='rtt-channels':
        rtt_print_channels(l, a.control_block)
    elif a.cmd=='rtt-tail':
        rtt_tail(l, a.control_block, a.channel, a.seconds, a.interval_ms, a.terminal, a.strip_ansi)
    elif a.cmd=='rtt-log':
        rtt_log(l, a.control_block, a.channel, a.output, a.seconds, a.interval_ms, append=a.append)
    elif a.cmd=='rtt-terminals':
        rtt_terminals_once(l, a.control_block, a.length, a.terminal, a.strip_ansi)
    elif a.cmd=='sysview-capture':
        rtt_log(l, a.control_block, a.channel, a.output, a.seconds, a.interval_ms, append=a.append)
    elif a.cmd=='swo-config':
        swo_config(l, a.baud, a.flags); print('OK')
    elif a.cmd=='swo-start':
        l.x(CMD['swo-start']); print('OK')
    elif a.cmd=='swo-stop':
        l.x(CMD['swo-stop']); print('OK')
    elif a.cmd=='swo-status':
        st=swo_status(l); print(f'active={st["active"]} baud={st["baud"]} available={st["available"]} dropped={st["dropped"]} flags=0x{st["flags"]:08x}')
    elif a.cmd=='swo-read':
        data=swo_read(l, a.length)
        if a.output:
            open(a.output,'wb').write(data)
        else:
            print(printable(data, strip_control=a.strip_ansi), end='')
            if data and not data.endswith(b'\n'):
                print()
    elif a.cmd=='swo-tail':
        swo_tail(l, a.seconds, a.interval_ms, strip_ansi=a.strip_ansi)
    elif a.cmd=='read':
        if a.length>MAX_PAYLOAD: raise SystemExit(f'read is limited to {MAX_PAYLOAD} bytes per command; use program/another chunking client for larger transfers')
        b=read_chunk(l,a.address,a.length)
        if a.output: open(a.output,'wb').write(b)
        else: print(b.hex(' '))
    elif a.cmd=='write':
        b=open(a.file,'rb').read(); write_chunk(l,a.address,b); print('OK')
    elif a.cmd=='program': program_binary(l,a.file,a.address,a.verify,a.erase)
    elif a.cmd=='program-hex': program_dspic_hex(l,a.file,a.verify,a.erase,a.include_config,a.dry_run,a.fill_word)
    elif a.cmd=='avr-config': configure_avr_from_profile(l,a.profile,a.power,a.clock); print('OK')
    elif a.cmd=='avr-signature': print(avr_read_signature(l).hex(' '))
    elif a.cmd=='avr-fuses':
        ensure_avr_isp_selected(l)
        if a.write:
            name,val=a.write; avr_fuse_write(l,name,int(val,0)); print('OK')
        else:
            for name in ('lfuse','hfuse','efuse','lock'):
                print(f'{name}=0x{avr_fuse_read(l,name):02x}')
    elif a.cmd=='program-avr-hex': program_avr_hex(l,a.file,a.profile,a.erase,a.verify,a.dry_run,a.fill)
    elif a.cmd=='updi-config': configure_updi_from_profile(l,a.profile,a.power,a.clock,a.hv_activate); print('OK')
    elif a.cmd=='program-updi-hex': updi_plan_hex(a.file,a.profile,dry_run=a.dry_run,experimental=a.experimental,erase=a.erase,verify=a.verify)
    elif a.cmd=='msp430-config': configure_msp430_from_profile(l,a.profile,a.interface,a.power,a.clock); print('OK')
    elif a.cmd=='program-msp430': msp430_program_plan(a.file,a.profile,include_info=a.include_info,dry_run=a.dry_run,experimental=a.experimental,fill=a.fill)
    elif a.cmd=='msp430-shift-ir': msp430_raw_shift(l,True,a.bits,a.hexbytes)
    elif a.cmd=='msp430-shift-dr': msp430_raw_shift(l,False,a.bits,a.hexbytes)
    elif a.cmd=='msp430-tap-reset': msp430_tap_reset(l,a.cycles); print('OK')
    elif a.cmd=='msp430-clock': msp430_clock(l,a.cycles,a.tms,a.tdi)
    elif a.cmd=='tms320-config': configure_tms320_from_profile(l,a.profile,a.power,a.clock); print('OK')
    elif a.cmd=='simplelink-config': configure_simplelink_from_profile(l,a.profile,a.interface,a.power,a.clock); print('OK')
    elif a.cmd=='tms320-shift-ir': tms320_raw_shift(l,True,a.bits,a.hexbytes)
    elif a.cmd=='tms320-shift-dr': tms320_raw_shift(l,False,a.bits,a.hexbytes)
    elif a.cmd=='tms320-tap-reset': tms320_tap_reset(l,a.cycles); print('OK')
    elif a.cmd=='tms320-clock': tms320_clock(l,a.cycles,a.tms,a.tdi)
    elif a.cmd=='tms320-idcode': tms320_idcode(l)
    elif a.cmd=='tms320-lines': tms320_lines(l,a.reset=='release',a.trst=='release',a.emu_hint); print('OK')
    elif a.cmd=='bridge-info': print_bridge_info(l)
    elif a.cmd=='bridge-gpio': bridge_gpio(l,a.op,a.role,a.value)
    elif a.cmd=='bridge-spi': bridge_spi(l,a.hexbytes,a.hz,a.mode,a.cs,a.lsb_first,a.cs_active_high)
    elif a.cmd=='bridge-i2c': bridge_i2c(l,a.addr,a.tx,a.rxlen,a.hz)
    elif a.cmd=='bridge-uart': bridge_uart(l,a.text,a.hex,a.rxlen,a.baud,a.invert)
    elif a.cmd=='power-trace': bridge_print_power_trace(l,a.samples,a.interval_ms)
    elif a.cmd=='raw': print(l.x(CMD['raw'],bytes.fromhex(a.hexbytes)).hex(' '))
    elif a.cmd=='dspic-load-capsule':
        data=open(a.file,'rb').read()
        if len(data)>MAX_PAYLOAD-1: raise SystemExit('capsule too large for one DJP2 raw transfer')
        l.x(CMD['raw'],bytes([0x80])+data); print('OK')
    elif a.cmd=='dspic-capsule-info':
        b=l.x(CMD['raw'],bytes([0x81]))
        if len(b)!=8: raise RuntimeError(f'unexpected capsule info length {len(b)}')
        loaded,fam,bps,reg,flags=struct.unpack('<BBBBI',b)
        print(f'loaded={bool(loaded)} family={fam} hw_breakpoints={bps} register_bytes={reg} flags=0x{flags:08x}')
    elif a.cmd=='script': print(l.x(CMD['script'],bytes.fromhex(a.hexbytes)).hex(' '))

if __name__=='__main__': main()
