#!/usr/bin/env python3
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from host.simplelink_utils import list_simplelink_profiles, load_simplelink_profile, make_simplelink_boot_plan, summarize_boot_plan

profiles=list_simplelink_profiles()
assert 'cc2652r' in profiles
p=load_simplelink_profile('cc2652r')
assert p['device']=='CC2652R'
plan=make_simplelink_boot_plan(p, transport='uart', debug_protocol='swd')
assert plan.device=='CC2652R'
text=summarize_boot_plan(plan)
assert 'CCFG' in text and 'uart' in text
try:
    make_simplelink_boot_plan(p, transport='can', debug_protocol='swd')
    raise AssertionError('expected invalid transport')
except Exception:
    pass
print('test_simplelink_utils: PASS')
