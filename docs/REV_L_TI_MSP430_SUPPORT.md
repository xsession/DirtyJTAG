# Rev L - Texas Instruments MSP430/MSP430X Support

Rev L adds the first DirtyJTAG-MCU support layer for Texas Instruments' 16-bit
MSP430 family.

## Scope

Implemented target families:

- MSP430 classic 16-bit flash devices, represented by `msp430g2553`.
- MSP430Xv2 flash devices, represented by `msp430f5529`.
- MSP430Xv2 FRAM devices, represented by `msp430fr5969` and `msp430fr6989`.

Implemented physical/debug transports:

- 2-wire Spy-Bi-Wire: `sbw` / protocol ID 30.
- 4-wire MSP430 JTAG: `msp430-jtag` / protocol ID 31.

The implementation is intentionally clean-room and staged.  Firmware exposes
TAP-level JTAG/SBW primitives and target power control; host descriptors handle
memory layout and image planning.  Destructive erase/write algorithms remain
`guarded` until real MSP430 boards validate TEST/RST entry timing, flash/FRAM
controller access, and lock/fuse behavior.

## Pin mapping

### Spy-Bi-Wire

| DirtyJTAG role | MSP430 signal |
|---|---|
| `CLK` | `SBWTCK` / `TEST` |
| `DATA0` | `SBWTDIO` / `RST/NMI` |
| `RESET` | optional target reset helper |
| `AUX` | unused for SBW default |

### 4-wire MSP430 JTAG

| DirtyJTAG role | MSP430 signal |
|---|---|
| `CLK` | `TCK` |
| `DATA0` | `TMS` |
| `DATA1` | `TDI` |
| `DATA2` | `TDO` |
| `RESET` | `RST/NMI` |
| `AUX` | `TEST` / JTAG-enable gate |

## Firmware raw operations

The new common MSP430 raw protocol is used by both `sbw` and `msp430-jtag`:

| Opcode | Name | Payload | Return |
|---:|---|---|---|
| `0x00` | TAP reset | `op, cycles` | empty |
| `0x01` | shift IR | `op, bits_le16, txbytes` | captured TDO bytes |
| `0x02` | shift DR | `op, bits_le16, txbytes` | captured TDO bytes |
| `0x03` | clock constant | `op, cycles, tms, tdi` | packed TDO bits |

This keeps the Pico firmware useful for bench work immediately while avoiding
hard-coded flash algorithms that vary across MSP430 classic, MSP430Xv2 flash,
and MSP430Xv2 FRAM families.

## Host commands

List profiles:

```bash
python host/djprog.py msp430-profile-list
```

Configure Spy-Bi-Wire target:

```bash
python host/djprog.py --port COM8 msp430-config msp430g2553 \
  --interface sbw \
  --power 3v3 \
  --clock 100000
```

Configure 4-wire JTAG target:

```bash
python host/djprog.py --port COM8 msp430-config msp430f5529 \
  --interface jtag \
  --power external \
  --clock 200000
```

Reset TAP and shift raw IR/DR:

```bash
python host/djprog.py --port COM8 enter
python host/djprog.py --port COM8 msp430-tap-reset --cycles 8
python host/djprog.py --port COM8 msp430-shift-ir 8 91
python host/djprog.py --port COM8 msp430-shift-dr 16 3412
```

Plan TI-TXT/Intel-HEX image programming without touching the target:

```bash
python host/djprog.py program-msp430 firmware.txt \
  --profile msp430g2553 \
  --dry-run
```

The planner rejects out-of-range writes by default and excludes information
memory unless `--include-info` is given.

## Device descriptors

Added descriptor files:

- `host/msp430_profiles/msp430g2553.json`
- `host/msp430_profiles/msp430f5529.json`
- `host/msp430_profiles/msp430fr5969.json`
- `host/msp430_profiles/msp430fr6989.json`

Each descriptor carries family, default interface, address width, flash/FRAM
range, RAM range, information memory range, and an honest implementation status.

## Validation

Native validation added:

- `tests/test_msp430_utils.py`: TI-TXT parser, profile loading, memory-range
  checking, and raw-payload encoding.
- `tests/test_core.c`: SBW and 4-wire JTAG backend registration, entry, raw
  TAP reset, IR/DR shift, and debug-info physical transport status.

Validated locally:

```text
./scripts/test-native.sh
  test_core: PASS
  test_hex_utils: PASS
  test_avr_utils: PASS
  test_updi_utils: PASS
  test_msp430_utils: PASS

./scripts/check-zephyr-syntax.sh
  PASS
```

## Next MSP430 step

Promote one board first, preferably MSP430G2553 LaunchPad-compatible wiring:

1. bench-validate SBW TEST/RST entry timing,
2. shift public JTAG IR/DR commands and confirm device response,
3. add non-destructive memory read of RAM/ROM ranges,
4. add flash controller erase/write only after read/verify works,
5. add MSP430Xv2 and FRAM-specific write flows separately.
