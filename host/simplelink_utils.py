#!/usr/bin/env python3
"""TI SimpleLink CC13xx/CC26xx clean-room host helpers."""
from __future__ import annotations
import json, os
from dataclasses import dataclass
from typing import Any, Dict, List

BASE=os.path.dirname(__file__)
PROFILE_DIR=os.path.join(BASE,'simplelink_profiles')

class SimpleLinkError(ValueError): pass

@dataclass(frozen=True)
class BootPlan:
    device: str
    debug_protocol: str
    boot_transport: str
    boot_pins: Dict[str, Any]
    flash_size: int
    ram_size: int
    notes: List[str]

def _load_json(path: str) -> Dict[str, Any]:
    with open(path,'r',encoding='utf-8') as f:
        return json.load(f)

def list_simplelink_profiles() -> List[str]:
    if not os.path.isdir(PROFILE_DIR):
        return []
    return sorted(os.path.splitext(x)[0] for x in os.listdir(PROFILE_DIR) if x.endswith('.json'))

def load_simplelink_profile(name: str) -> Dict[str, Any]:
    safe=name.replace('/','_').replace('\\','_').lower()
    path=os.path.join(PROFILE_DIR, safe+'.json')
    if not os.path.exists(path):
        raise SimpleLinkError(f'unknown SimpleLink profile {name!r}; choose one of {", ".join(list_simplelink_profiles())}')
    p=_load_json(path)
    for key in ('device','family','flash_size','ram_size','debug','rom_bootloader'):
        if key not in p:
            raise SimpleLinkError(f'{safe}: missing {key}')
    return p

def make_simplelink_boot_plan(profile: Dict[str, Any], transport: str='uart', debug_protocol: str='swd') -> BootPlan:
    debug=profile.get('debug',{})
    if debug_protocol not in debug.get('protocols',[]):
        raise SimpleLinkError(f'{profile["device"]}: debug protocol {debug_protocol!r} not listed in profile')
    boot=profile.get('rom_bootloader',{})
    transports=boot.get('transports',[])
    if transport not in transports:
        raise SimpleLinkError(f'{profile["device"]}: ROM boot transport {transport!r} not available; choose {transports}')
    notes=[]
    if boot.get('backdoor_config_required', True):
        notes.append('Force-entry with valid flash image requires CCFG bootloader backdoor enable bits in the image.')
    if transport == 'uart':
        notes.append('Use SWRA466-style ping/download/send-data/reset framing from host; DirtyJTAG provides bridge and reset/backdoor sequencing.')
    if transport == 'spi':
        notes.append('SPI bootloader requires target package-specific ROM boot pins and chip-select wiring.')
    return BootPlan(profile['device'], debug_protocol, transport, boot.get('pins',{}), int(profile['flash_size']), int(profile['ram_size']), notes)

def summarize_simplelink_profile(profile: Dict[str, Any]) -> str:
    dbg=','.join(profile.get('debug',{}).get('protocols',[]))
    boot=','.join(profile.get('rom_bootloader',{}).get('transports',[]))
    return f"{profile['device']}: family={profile['family']} flash={profile['flash_size']} ram={profile['ram_size']} debug={dbg} boot={boot}"

def summarize_boot_plan(plan: BootPlan) -> str:
    pins=', '.join(f'{k}={v}' for k,v in sorted(plan.boot_pins.items())) or '-'
    lines=[f'{plan.device}: debug={plan.debug_protocol} boot={plan.boot_transport} flash={plan.flash_size} ram={plan.ram_size}', f'  pins: {pins}']
    lines += [f'  note: {n}' for n in plan.notes]
    return '\n'.join(lines)
