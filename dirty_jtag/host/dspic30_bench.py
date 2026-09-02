#!/usr/bin/env python3
"""Conservative dsPIC30F real-hardware bring-up workflow.

The goal is to validate power/VPP/ICSP and only then expand to one-row and full
HEX programming.  It shells out to djprog.py so it exercises the same public CLI
that users will run on Windows/Linux.
"""
from __future__ import annotations
import argparse
import subprocess
import sys
from pathlib import Path

THIS = Path(__file__).resolve().parent / 'djprog.py'


def run(args, check=True):
    cmd = [sys.executable, str(THIS)] + args
    print('+', ' '.join(str(x) for x in cmd))
    cp = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(cp.stdout, end='')
    if check and cp.returncode:
        raise SystemExit(cp.returncode)
    return cp


def main():
    ap = argparse.ArgumentParser(description='Safe dsPIC30F5011/4011 bench bring-up sequence')
    ap.add_argument('--port', required=True)
    ap.add_argument('--device', default='dsPIC30F5011', choices=['dsPIC30F5011', 'dsPIC30F4011'])
    ap.add_argument('--power', default='external', choices=['external', '5v'])
    ap.add_argument('--clock', type=int, default=1000000)
    ap.add_argument('--hex', help='optional Intel HEX to dry-run/program')
    ap.add_argument('--program-one-row', action='store_true', help='after dry-run, program first row only using the normal program-hex path when supported')
    ap.add_argument('--program-full', action='store_true', help='erase/program/verify the full image; do not use before one-row validation')
    a = ap.parse_args()

    base = ['--port', a.port]
    run(base + ['safe'])
    run(base + ['measure'])
    run(base + ['config', 'dspic', '--device', a.device, '--power', a.power, '--clock', str(a.clock)])
    run(base + ['status'])
    run(base + ['enter'])
    run(base + ['identify'], check=False)
    if a.hex:
        run(base + ['program-hex', a.hex, '--dry-run'])
        if a.program_full:
            run(base + ['program-hex', a.hex, '--erase', '--verify'])
        elif a.program_one_row:
            print('\nOne-row mode is intentionally a policy step: crop the HEX to one page/row or use the firmware write command after reviewing the dry-run plan. Full programming was not run.\n')
    run(base + ['leave'], check=False)
    run(base + ['safe'], check=False)

if __name__ == '__main__':
    main()
