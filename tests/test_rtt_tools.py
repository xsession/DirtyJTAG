#!/usr/bin/env python3
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from host.rtt_tools import split_virtual_terminals, strip_ansi, printable

raw = b"boot\xff1warn\xffAtrace\xff0done"
s = split_virtual_terminals(raw)
assert s.all_data == b"bootwarntracedone"
assert s.terminals[0] == b"bootdone"
assert s.terminals[1] == b"warn"
assert s.terminals[10] == b"trace"
assert strip_ansi(b"\x1b[31mred\x1b[0m") == b"red"
assert printable(b"ok") == "ok"
print("test_rtt_tools: PASS")
