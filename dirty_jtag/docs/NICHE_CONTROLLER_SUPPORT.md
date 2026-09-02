# Niche controller support model

Rev H supports niche controllers through three paths:

1. **Dedicated high-level backend** — implemented in firmware when the public specification is stable and broadly useful.
2. **Native debug engine** — implemented in firmware when public debug registers/commands are available.
3. **USB script VM / raw transport** — host-defined algorithms for family-specific or rare controllers without reflashing firmware.

## Current implementation state

| Family | Programming | Debug | Path |
|---|---|---|---|
| dsPIC30F/dsPIC33F | high-level ICSP for listed profiles | clean-room DE control plane + mock; real DE scripts pending user-owned DFP/assets | firmware backend + capsule |
| PIC24 | raw ICSP electrical transport | not implemented | raw + script VM |
| PIC10/12/16/18 | raw ICSP electrical transport | not implemented | raw + script VM |
| AVR classic | high-level ISP | not implemented | firmware backend |
| AVR UPDI | raw 8E2 + HV activation path | physical only | raw + script VM |
| AVR TPI | raw 8E2 | not implemented | raw + script VM |
| AVR XMEGA PDI | raw 8E2 | physical only | raw + script VM |
| STM8 | SWIM read/write | native halt/run/step/registers/breakpoints | firmware backend |
| MSP430 | SBW slot raw transport | physical only | raw + script VM |
| Silicon Labs C2 | identify/erase/read/write + raw | physical only | firmware backend |
| Renesas RL78 | Protocol A entry/raw UART | not implemented | raw + script VM |
| ARM SWD | OpenOCD target-specific | OpenOCD/GDB | remote_bitbang bridge |
| JTAG | OpenOCD target-specific | OpenOCD/GDB | remote_bitbang bridge |

## Why use script VM first for rare families

Some families need unlock keys, voltage modes, part-specific timing and memory maps.  A Pico firmware release should not bake in half-reviewed algorithms for every variant.  The script VM lets a host profile express the exact public algorithm while still using the protected front end: measured VTARGET/VPP, current limiting, pin mapping and safe-idle interlocks.

Once a script profile is validated on real hardware and has test vectors, it can be promoted into a dedicated C backend.
