# dsPIC30F / dsPIC33 debug support

## Status

Rev H implements the firmware and host **control plane** for dsPIC debugging, but does not redistribute or guess Microchip's target-side Debug Executive protocol.  This is intentional.

What is implemented:

- dsPIC30F/dsPIC33 programming backend remains available for supported profiles.
- DJP2 native debug API is registered for protocol `dspic`.
- Mock dsPIC target for CI and host/IDE development: `mock-dspic30f5011`.
- Clean-room metadata capsule loader.
- Raw SIX/REGOUT escape hatches for audited host-side scripts.
- Debug state model: attach/detach/halt/run/step/reset/registers/breakpoints.

What is not bundled:

- Microchip Debug Executive binaries.
- Proprietary Debug Executive command packets.
- Device-family reserved resource data copied from MPLAB.

## Why this split exists

PIC/dsPIC debug tools program the application plus a small Debug Executive into the target.  That executive consumes program memory, RAM/registers and stack resources and uses the ICSP/ICD pins during debug.  Ordinary ICSP read/write/erase commands are not sufficient for source-level debugging.

## Mock workflow

```sh
python host/djprog.py --port COM8 config dspic --device mock-dspic30f5011 --power external
python host/djprog.py --port COM8 debug-info
python host/djprog.py --port COM8 debug-attach
python host/djprog.py --port COM8 debug-halt
python host/djprog.py --port COM8 debug-reg-read
python host/djprog.py --port COM8 debug-bp-set 0x100200 --slot 0
python host/djprog.py --port COM8 debug-run
python host/djprog.py --port COM8 debug-detach
```

## Metadata capsule workflow

Inspect a locally installed or downloaded DFP/ATPACK:

```sh
python host/dfp_inspect.py path/to/Microchip.dsPIC30F_DFP.x.y.z.atpack \
  --json dspic30-report.json \
  --capsule dspic30-debug.capsule \
  --family 0 \
  --hw-breakpoints 2 \
  --reg-bytes 42
```

Load the metadata capsule:

```sh
python host/djprog.py --port COM8 config dspic --device dsPIC30F5011 --power 5v --clock 1000000
python host/djprog.py --port COM8 dspic-load-capsule dspic30-debug.capsule
python host/djprog.py --port COM8 dspic-capsule-info
```

The current capsule format is metadata-only.  Future capsule revisions can add audited transaction scripts that issue SIX/REGOUT sequences or Debug-Executive mailbox transfers derived from public documentation and user-owned local assets.

## Raw dsPIC debug/backend commands

`RAW_XFER` on the dsPIC backend:

| Opcode | Meaning |
|---:|---|
| `0x80` | load debug metadata capsule |
| `0x81` | return capsule status |
| `0x82` | execute one 24-bit SIX instruction |
| `0x83` | issue REGOUT and return 16-bit word |

These operations are intentionally low-level and should be used only by reviewed host tools/profiles.

## Next physical validation

1. Confirm dsPIC30F5011 VDD/VPP entry sequence on the Rev B front end.
2. Verify device ID and row readback on real hardware.
3. Program a small blink application and verify row programming.
4. Use a local DFP/MPLAB debug build to locate DE/reserved resources.
5. Capture PICkit/MPLAB debug attach behavior if legally permitted by local license and document only packet behavior, not proprietary code.
6. Convert observed/public behavior into capsule transaction scripts and unit tests.
