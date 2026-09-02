#!/usr/bin/env python3
"""DirtyJTAG Universal Programmer host CLI (DJP2 over USB CDC ACM)."""
from __future__ import annotations
import argparse, binascii, json, os, struct, time
try:
    import serial
except ImportError:
    serial = None
try:
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
except ImportError:  # Allows direct unit import from outside host/.
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

MAGIC=0x32504A44; VER=2; HDR=20; MAX_PAYLOAD=2048
CMD={'hello':1,'list':2,'config':3,'status':4,'devices':5,
     'enter':0x10,'leave':0x11,'identify':0x12,
     'erase':0x20,'read':0x21,'write':0x22,'raw':0x30,
     'power':0x40,'measure':0x41,'pinmap':0x42,'vpp':0x43,'phy-info':0x44,'script':0x45,
     'debug-info':0x51,'debug-attach':0x52,'debug-detach':0x53,
     'debug-halt':0x54,'debug-run':0x55,'debug-step':0x56,'debug-reset':0x57,
     'debug-reg-read':0x58,'debug-reg-write':0x59,'debug-bp-set':0x5a,'debug-bp-clear':0x5b,
     'safe':0x7e}
PROTO={'dspic':1,'pic24':2,'pic-raw':3,'avr-isp':10,'updi':11,'tpi':12,'pdi':13,
       'swim':20,'sbw':30,'msp430-jtag':31,'c2':40,'rl78':50,'swd':60,'jtag':61,'tms320':70,'c2000':70,'xds110v3':70}
POWER={'off':0,'external':1,'3v3':2,'5v':3}
FAMILY={0:'dsPIC30',1:'dsPIC33F',2:'dsPIC33E',3:'dsPIC33CK',4:'dsPIC33A'}
CAP_WRITE=1<<3; CAP_READ=1<<2
ALG_DIR=os.path.join(os.path.dirname(__file__), "algorithms")

def crc32(data,seed=0): return binascii.crc32(data,seed)&0xffffffff

def pack(cmd,seq,payload=b'',flags=0,status=0):
    if len(payload)>MAX_PAYLOAD: raise ValueError(f'payload too large: {len(payload)} > {MAX_PAYLOAD}')
    h=struct.pack('<IHHHHHIH',MAGIC,VER,cmd,seq,status,flags,len(payload),0)
    c=crc32(payload,crc32(h[:18])); fold=(c^(c>>16))&0xffff
    return h[:18]+struct.pack('<H',fold)+payload

def unpack(data):
    if len(data)<HDR: raise ValueError('short frame')
    magic,ver,cmd,seq,status,flags,n,fold=struct.unpack('<IHHHHHIH',data[:HDR])
    if magic!=MAGIC or ver!=VER or len(data)!=HDR+n: raise ValueError('bad frame')
    c=crc32(data[HDR:],crc32(data[:18]))
    if ((c^(c>>16))&0xffff)!=fold: raise ValueError('bad crc')
    return cmd,seq,status,flags,data[HDR:]

class Link:
    def __init__(self,port,baud=115200,timeout=3):
        if serial is None: raise SystemExit('Install pyserial: python -m pip install pyserial')
        self.s=serial.Serial(port,baudrate=baud,timeout=timeout)
        self.seq=1; time.sleep(.1); self.s.reset_input_buffer()
    def x(self,cmd,p=b''):
        q=pack(cmd,self.seq,p); seq=self.seq; self.seq=(self.seq+1)&0xffff
        self.s.write(q); self.s.flush()
        h=self.s.read(HDR)
        if len(h)!=HDR: raise TimeoutError('no DJP2 reply')
        n=struct.unpack_from('<I',h,14)[0]
        body=self.s.read(n); _,s,st,_,p=unpack(h+body)
        if s!=seq: raise RuntimeError(f'sequence mismatch {s}!={seq}')
        if st: raise RuntimeError(f'device status={st}')
        return p

def cmd_config(l,a):
    name=a.device.encode()
    if len(name)>47: raise ValueError('device name too long')
    cfg_flags = 1 if getattr(a,'hv_activate',False) else 0
    payload=struct.pack('<HBBIIIHB',PROTO[a.protocol],POWER[a.power],cfg_flags,a.clock,
                        a.vtarget,a.flash_size,a.page_size,len(name))+name
    l.x(CMD['config'],payload)

def decode_status(b):
    if len(b)<28: raise RuntimeError(f'unexpected status length {len(b)}')
    # Backward-compatible first 28 bytes.
    pid,pwr,nlen,clk,caps,vt,vp,ma,flags=struct.unpack_from('<HBBIIIIII',b,0)
    flash=page=cfg_flags=0; dev=''
    if len(b)>=36:
        flash=struct.unpack_from('<I',b,28)[0]
        page,cfg_flags=struct.unpack_from('<HH',b,32)
        if nlen and len(b)>=36+nlen: dev=b[36:36+nlen].decode(errors='replace')
    return dict(pid=pid,power=pwr,clock=clk,caps=caps,vt=vt,vpp=vp,current=ma,
                fault=bool(flags&1),flash=flash,page=page,cfg_flags=cfg_flags,device=dev)

def get_devices(l):
    b=l.x(CMD['devices']); o=0; out=[]
    while o<len(b):
        if o+14>len(b): raise RuntimeError('truncated device list')
        fam,n,row,page,flags,end=struct.unpack_from('<BBHHII',b,o); o+=14
        if o+n>len(b): raise RuntimeError('truncated device name')
        name=b[o:o+n].decode(); o+=n
        out.append(dict(family=fam,name=name,row=row,page=page,flags=flags,end=end))
    return out

def print_measure(b):
    if len(b)==8:
        vt,vp=struct.unpack('<II',b); print(f'VTARGET={vt/1000:.3f} V  VPP={vp/1000:.3f} V'); return
    if len(b)<16: raise RuntimeError(f'unexpected measurement length {len(b)}')
    vt,vp,ma,fl=struct.unpack_from('<IIII',b)
    print(f'VTARGET={vt/1000:.3f} V  VPP={vp/1000:.3f} V  ITARGET={ma} mA  POWER_FAULT={bool(fl&1)}')

def write_chunk(l,address,data):
    if len(data)>MAX_PAYLOAD-8: raise ValueError('write chunk too large')
    l.x(CMD['write'],struct.pack('<II',address,len(data))+data)

def read_chunk(l,address,length):
    if length>MAX_PAYLOAD: raise ValueError('read chunk too large')
    return l.x(CMD['read'],struct.pack('<II',address,length))


def dspic_row_plan_from_hex(path, dev, include_config=False, fill_word=0xffffff):
    words = dspic_words_from_ihex(path, include_config=include_config)
    rows = make_dspic_rows(words, dev['row'], dev['end'], fill_word)
    return rows

def program_dspic_hex(l, path, verify=False, erase=False, include_config=False, dry_run=False, fill_word=0xffffff):
    if l is None:
        # Offline dry-run path uses conservative dsPIC30F5011 defaults unless --device-profile is supplied later.
        raise RuntimeError('program-hex without --port requires --dry-run and --device-profile support from caller')
    st = decode_status(l.x(CMD['status']))
    if st['pid'] != PROTO['dspic']:
        raise RuntimeError('program-hex currently supports selected dspic backend only')
    if not (st['caps'] & CAP_WRITE):
        raise RuntimeError('selected backend has no high-level WRITE capability')
    dev = next((d for d in get_devices(l) if d['name'] == st['device']), None)
    if not dev:
        raise RuntimeError('selected dsPIC device profile not found; CONFIG with --device first')
    rows = dspic_row_plan_from_hex(path, dev, include_config=include_config, fill_word=fill_word)
    print('dsPIC HEX plan:', summarize_dspic_rows(rows))
    print(f'device={dev["name"]} row_words={dev["row"]} user_end=0x{dev["end"]:06x}')
    if dry_run:
        for row in rows[:12]:
            nonblank=sum(1 for w in row.words if w != fill_word)
            print(f'  row PC 0x{row.pc_address:06x}: {nonblank}/{len(row.words)} non-erased words')
        if len(rows)>12: print(f'  ... {len(rows)-12} more rows')
        return
    if not rows:
        print('nothing to program')
        return
    if erase:
        l.x(CMD['erase'])
    done = 0
    total = len(rows)
    for row in rows:
        block = pack_dspic_row_words(row.words)
        write_chunk(l, row.pc_address, block)
        if verify:
            got = read_chunk(l, row.pc_address, len(block))
            if got != block:
                for i, (a, b) in enumerate(zip(block, got)):
                    if a != b:
                        pc = row.pc_address + 2 * (i // 3)
                        raise RuntimeError(f'verify failed near PC 0x{pc:06x}, row byte +0x{i:x}: wrote {a:02x}, read {b:02x}')
                raise RuntimeError(f'verify mismatch at row PC 0x{row.pc_address:06x}')
        done += 1
        print(f'programmed row {done}/{total} at PC 0x{row.pc_address:06x}', end='\r', flush=True)
    print(f'programmed {total} dsPIC rows OK' + (' + verified' if verify else ''))


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

def main():
    ap=argparse.ArgumentParser(description='DirtyJTAG Universal Pico programmer/debugger CLI')
    ap.add_argument('--port', help='USB CDC ACM port; not required for offline algorithm-list')
    sp=ap.add_subparsers(dest='cmd',required=True)
    for name in ('hello','list','devices','status','enter','leave','identify','erase','measure','safe',
                 'phy-info','debug-info','debug-attach','debug-detach','debug-halt','debug-run','debug-step','debug-reg-read','debug-regs'):
        sp.add_parser(name)
    dr=sp.add_parser('debug-reset'); dr.add_argument('--halt',action='store_true')
    dw=sp.add_parser('debug-reg-write'); dw.add_argument('hexbytes')
    db=sp.add_parser('debug-bp-set'); db.add_argument('address',type=lambda x:int(x,0)); db.add_argument('--type',type=int,default=0); db.add_argument('--slot',type=int,default=0)
    dc=sp.add_parser('debug-bp-clear'); dc.add_argument('slot',type=int)
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
    avc=sp.add_parser('avr-config'); avc.add_argument('profile'); avc.add_argument('--power',choices=POWER,default='external'); avc.add_argument('--clock',type=int,default=125000)
    avs=sp.add_parser('avr-signature')
    avf=sp.add_parser('avr-fuses'); avf.add_argument('--write',nargs=2,metavar=('NAME','VALUE'))
    avp=sp.add_parser('program-avr-hex'); avp.add_argument('file'); avp.add_argument('--profile',required=True); avp.add_argument('--erase',action='store_true'); avp.add_argument('--verify',action='store_true'); avp.add_argument('--dry-run',action='store_true'); avp.add_argument('--fill',type=lambda x:int(x,0),default=0xff)
    upc=sp.add_parser('updi-config'); upc.add_argument('profile'); upc.add_argument('--power',choices=POWER,default='external'); upc.add_argument('--clock',type=int,default=115200); upc.add_argument('--hv-activate',action='store_true')
    mspc=sp.add_parser('msp430-config'); mspc.add_argument('profile'); mspc.add_argument('--interface',choices=('sbw','jtag')); mspc.add_argument('--power',choices=POWER,default='external'); mspc.add_argument('--clock',type=int,default=100000)
    tmsc=sp.add_parser('tms320-config'); tmsc.add_argument('profile'); tmsc.add_argument('--power',choices=('external','3v3'),default='external'); tmsc.add_argument('--clock',type=int,default=None)
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
    if a.cmd=='program-avr-hex' and a.dry_run:
        print_avr_hex_plan(a.file,a.profile,fill=a.fill); return
    if a.cmd=='program-updi-hex' and a.dry_run:
        updi_plan_hex(a.file,a.profile,dry_run=True,experimental=a.experimental,erase=a.erase,verify=a.verify); return
    if a.cmd=='program-msp430' and a.dry_run:
        msp430_program_plan(a.file,a.profile,include_info=a.include_info,dry_run=True,experimental=a.experimental,fill=a.fill); return
    if not a.port:
        raise SystemExit('--port is required for USB operations')
    l=Link(a.port)

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
    elif a.cmd=='tms320-shift-ir': tms320_raw_shift(l,True,a.bits,a.hexbytes)
    elif a.cmd=='tms320-shift-dr': tms320_raw_shift(l,False,a.bits,a.hexbytes)
    elif a.cmd=='tms320-tap-reset': tms320_tap_reset(l,a.cycles); print('OK')
    elif a.cmd=='tms320-clock': tms320_clock(l,a.cycles,a.tms,a.tdi)
    elif a.cmd=='tms320-idcode': tms320_idcode(l)
    elif a.cmd=='tms320-lines': tms320_lines(l,a.reset=='release',a.trst=='release',a.emu_hint); print('OK')
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
