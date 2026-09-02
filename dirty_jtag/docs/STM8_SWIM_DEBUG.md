# STM8 SWIM native debug engine

Rev D/H provides the first native non-ARM debugger path: STM8 over SWIM.

## Source boundary

This implementation is clean-room and based on ST UM0470, which publicly documents:

- SWIM activation and communication reset;
- SWIM bit formats;
- the three SWIM commands: SRST, ROTF and WOTF;
- CPU register mapping at `0x7F00..0x7F0A`;
- SWIM_CSR at `0x7F80`;
- Debug Module breakpoint/control/status registers at `0x7F90..0x7F9A`;
- CPU stall/run/step and breakpoint behavior.

No ST-LINK firmware or proprietary source was used.

## USB workflow

Configure the backend:

```sh
python host/djprog.py --port COM8 config swim --device stm8s003f3 --power 3v3 --clock 363000
python host/djprog.py --port COM8 enter
```

Attach the native debugger:

```sh
python host/djprog.py --port COM8 debug-info
python host/djprog.py --port COM8 debug-attach
python host/djprog.py --port COM8 debug-halt
python host/djprog.py --port COM8 debug-regs
```

Set a hardware instruction-fetch breakpoint:

```sh
python host/djprog.py --port COM8 debug-bp-set 0x8080 --slot 0
python host/djprog.py --port COM8 debug-run
```

Single-step:

```sh
python host/djprog.py --port COM8 debug-step
python host/djprog.py --port COM8 debug-regs
```

Clear and detach:

```sh
python host/djprog.py --port COM8 debug-bp-clear 0
python host/djprog.py --port COM8 debug-detach
python host/djprog.py --port COM8 leave
```

## Register payload format

`DEBUG_REG_READ` returns 11 bytes, matching the UM0470 CPU register mirror:

| Offset | Register | Address |
|---:|---|---:|
| 0 | A | `0x7F00` |
| 1 | PCE | `0x7F01` |
| 2 | PCH | `0x7F02` |
| 3 | PCL | `0x7F03` |
| 4 | XH | `0x7F04` |
| 5 | XL | `0x7F05` |
| 6 | YH | `0x7F06` |
| 7 | YL | `0x7F07` |
| 8 | SPH | `0x7F08` |
| 9 | SPL | `0x7F09` |
| 10 | CC | `0x7F0A` |

Host-side decoded display is available with:

```sh
python host/djprog.py --port COM8 debug-regs
```

## Breakpoints

Rev H implements the two UM0470 hardware breakpoint address registers for instruction-fetch breakpoints. Data read/write/watchpoint decoding is left disabled until separately validated per STM8 subfamily.

## PIO requirement

The firmware still contains a portable software SWIM path so the code compiles and tests without RP2040 hardware. For production hardware validation, replace the per-edge software path with a PIO driver. The required PIO behavior is documented in `src/swim_rp2040_pio_reference.c`.

Reason: high-speed SWIM uses 10 SWIM clocks per bit. A logical `1` is 2 low clocks followed by 8 high clocks, and a logical `0` is 8 low clocks followed by 2 high clocks. At the documented high-speed timing this includes ~192..208 ns low pulses, which should not be generated with general Zephyr busy-wait GPIO.

## Validation status

Native CI tests use `mock-stm8*` target names to exercise the debug API without hardware:

```sh
./scripts/test-native.sh
```

Hardware validation checklist:

1. Scope DATA0 at reset/entry and confirm communication reset low width.
2. Verify SWIM_CSR read/write in low-speed mode.
3. Enable SWIM_DM and read CPU register mirror.
4. Set STALL, verify CPU stops and memory/register reads work.
5. Set breakpoint slot 0 on a loop address and confirm DM_CSR1.BK1F.
6. Step one instruction and confirm DM_CSR1.STF.
7. Repeat at high speed with the PIO implementation.
