# Rev M - TI TMS320/C2000 XDS110v3-style support

Rev M adds a separate Texas Instruments TMS320/C2000 path.  This is **not** the
same as MSP430.  TMS320/C2000 devices are accessed through a conventional TI
JTAG debug header, while MSP430 uses MSP430-specific 4-wire JTAG or 2-wire
Spy-Bi-Wire sequences.

## Clean-room boundary

The firmware implements an XDS110v3-style **electrical/JTAG transport**, not a
byte-for-byte clone of TI's proprietary CCS/XDS USB stack.  The new backend is
suitable for:

- C2000/TMS320 JTAG electrical bring-up.
- TAP reset and IDCODE scans.
- Raw IR/DR scans for bench scripts and future host bridges.
- OpenOCD `remote_bitbang` bridging where the host tool has suitable target
  support.
- Future C2000 algorithm development.

It does **not** yet make Code Composer Studio see the Pico as a genuine XDS110.
That would require either a licensed/official bridge or a separately validated
clean-room implementation of the XDS USB/API layer.

## New protocol

| Item | Value |
|---|---:|
| Protocol ID | `70` |
| CLI aliases | `tms320`, `c2000`, `xds110v3` |
| Backend name | `tms320-c2000-xds110v3-jtag` |
| Voltage policy | external/3.3 V only; 5 V rejected |
| Debug path | OpenOCD remote_bitbang transport + raw JTAG |

## Universal connector mapping

| DirtyJTAG role | TMS320/C2000 signal |
|---|---|
| `CLK` | TCK |
| `DATA0` | TMS |
| `DATA1` | TDI |
| `DATA2` | TDO |
| `RESET` | nRESET / XRSn |
| `AUX` | nTRST by default; can be repurposed as EMU0 on a board adapter |

The Rev B front end has only six universal signal roles, so it cannot expose a
complete CTI-20/XDS110 superset with TRST plus EMU0/EMU1/EMU2/EMU3/EMU4 all at
once.  For C2000 debug bring-up this is acceptable for basic IEEE 1149.1 JTAG,
but trace/ET and full CCS adapter fidelity need a later hardware adapter.

## Host commands

List supported C2000 profiles:

```bash
python host/djprog.py tms320-profile-list
```

Configure a C2000 target:

```bash
python host/djprog.py --port COM8 tms320-config tms320f2800137 \
  --power external \
  --clock 1000000
```

Enter and read a conservative JTAG DR ID scan:

```bash
python host/djprog.py --port COM8 enter
python host/djprog.py --port COM8 tms320-idcode
```

Raw JTAG operations:

```bash
python host/djprog.py --port COM8 tms320-tap-reset --cycles 8
python host/djprog.py --port COM8 tms320-shift-ir 6 3f
python host/djprog.py --port COM8 tms320-shift-dr 32 00000000
python host/djprog.py --port COM8 tms320-clock 8 --tms --tdi
python host/djprog.py --port COM8 tms320-lines --reset release --trst release
```

OpenOCD bridge path:

```bash
python host/openocd_bridge.py --port COM8 --transport jtag --power external
```

Then point OpenOCD at `host/openocd/dirtyjtag-remote-jtag.cfg` and a target
configuration only if the host-side OpenOCD build actually supports the target.
The firmware only provides the JTAG wire engine; CPU semantics are host-owned.

## Profiles added

- `tms320f2800137`
- `tms320f280049c`
- `tms320f28379d`
- `tms320f28p650dk`

## Current support matrix

| Feature | Status |
|---|---|
| 4-wire JTAG electrical transport | implemented |
| Target reset / TRST line control | implemented within six-role connector limits |
| IDCODE-style DR scan | implemented |
| Raw IR/DR shift | implemented |
| OpenOCD remote_bitbang JTAG | implemented as transport |
| cJTAG / IEEE 1149.7 | planned, not enabled |
| CCS native XDS110 USB emulation | not implemented |
| C2000 flash programming | not implemented |
| C28x/CLA halt/run/register engine | host/tool dependent; not native yet |

## Safety notes

- Do not use `--power 5v`; the backend rejects it.
- Prefer `--power external` and let the target board provide VTREF.
- Verify target voltage is in the XDS110-class range before connecting.
- Use a proper TI 14-pin/20-pin adapter with short ground return paths.
- Treat EMU pins as board-specific.  The universal six-role connector exposes
  only one auxiliary line.
