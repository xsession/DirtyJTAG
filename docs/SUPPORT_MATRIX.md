# Programming and debugging support matrix

`high-level program` means erase/read/write or equivalent device memory operations are implemented in firmware. `debug usable` means a normal debugger stack can halt/run/step/read registers/use breakpoints. `physical only` means the electrical/link transport exists but the project deliberately does not claim complete target run-control.

| Family/interface | Programming | Debugging | USB ID | Notes |
|---|---|---|---:|---|
| dsPIC30F4011 | high-level program | DE integration pending | 1 | high-voltage ICSP; Debug Executive required for source-level debug |
| dsPIC30F5011 | high-level program | DE integration pending | 1 | first-priority dsPIC target; DFP/DE discovery tool included |
| dsPIC33FJ128GP802 | high-level program | DE integration pending | 1 | ICSP programming; debug uses device DE/ICD resources |
| dsPIC33E/PIC24E | architecture/raw | pending | 1/2 | do not reuse dsPIC33F destructive sequences without exact device spec |
| dsPIC33C/dsPIC33A | architecture | pending | 1 | separate programming/debug algorithms |
| PIC24 ICSP | raw transport | pending | 2 | shared ICSP/VPP primitives |
| PIC10/12/16/18 ICSP | raw transport | pending | 3 | family/device debug-executive work not yet enabled |
| AVR classic ISP | high-level program | no | 10 | ISP itself is programming, not OCD |
| AVR UPDI | raw physical transport | physical only | 11 | 8E2 one-wire; optional isolated HV activation |
| AVR TPI | raw physical transport | no | 12 | treated as programming-only |
| AVR XMEGA PDI | raw physical transport | physical only | 13 | native OCD command layer not yet enabled |
| STM8 SWIM | read/write/identify via WOTF/ROTF | native debug ABI/DM engine; PIO timing validation pending | 20 | halt/run/step/registers/2 instruction breakpoints implemented; high-speed hardware path still experimental |
| MSP430 Spy-Bi-Wire | raw physical | physical only | 30 | high-level SBW/JTAG debug state machine not yet implemented |
| Silicon Labs C2 | high-level program | physical only | 40 | C2 wire access works; target run-control layer pending |
| Renesas RL78 | serial/raw | not yet | 50 | current backend is programming transport building block |
| ARM SWD | OpenOCD can flash | **debug usable via OpenOCD** | 60 | DJP2 remote_bitbang bridge -> OpenOCD -> GDB/IDE |
| Generic JTAG | OpenOCD target-dependent | **debug usable via OpenOCD** | 61 | supports JTAG targets known by OpenOCD |
| TI TMS320/C2000 XDS110v3-style JTAG | raw JTAG/ID scan only | transport via OpenOCD remote_bitbang; CCS-native XDS emulation not implemented | 70 | 1.8..3.6 V target I/O; six-role connector supports basic JTAG/nRESET/nTRST, not full CTI-20 trace/EMU superset |

## Debug capability policy

A backend is not marked as a complete debugger merely because the target wire can be toggled. `DJP2 DEBUG_INFO` distinguishes physical transport from high-level run control. Unsupported native halt/run/step/register/breakpoint commands return `DJP2_E_UNSUPPORTED`.

For SWD/JTAG, OpenOCD owns high-level target semantics, so the DJP2 state is `transport-ready`, not a fake CPU `halted/running` state.

## Hardware voltage coverage

- Normal translated signals: target-side VTARGET nominally 1.65..5.5 V when externally powered; local source options are ~3.3 V and ~5.0 V.
- RESET/MCLR is an open-drain sink during normal reset operation.
- MCLR/VPP has a separate ~11.8 V authorized path through physical jumper JP1.
- UPDI DATA0 has a distinct isolated ~11.8 V activation path through JP2; the normal DATA0 translator is isolated before HV is applied.
- Debugger code never implicitly enables either high-voltage path simply because a debug transport was selected.

## Validation status

The transport/core is built in native CI/tests with `-Wall -Wextra -Werror`. Tests cover DJP2 framing, backend selection, OpenOCD SWD/JTAG remote-bitbang operations, debugger state/attach semantics, telemetry and dsPIC packing helpers.

Actual on-target validation is still required per device and silicon revision, especially for destructive programming and the STM8 high-speed PIO path.


## Rev H additions

- dsPIC protocol now exposes a native debug control plane and mock target. Real-target Debug Executive transactions require a user-owned DFP/MPLAB-derived clean-room capsule.
- STM8 Pico PHY name changes to `rp2040-pio-txrx`; GPIO/mock PHYs remain available.
- `DJP2_SCRIPT_XFER` enables host-defined electrical algorithms for additional niche controllers without reflashing firmware.
- See `docs/DSPIC_DEBUG.md` and `docs/NICHE_CONTROLLER_SUPPORT.md`.

## Rev J AVR support status

| Family | Transport | High-level programming | Debug | Notes |
|---|---:|---:|---:|---|
| ATmega328P / ATtiny85 / ATmega32U4 classic ISP | yes | flash/signature/fuse read-write | no | Intel HEX host programmer added in Rev J. |
| ATmega2560 classic ISP | partial | descriptor only | no | Extended-address firmware path still pending before full image programming. |
| tinyAVR/megaAVR UPDI | yes | raw + HV activation only | physical only | High-level NVM commands planned. |
| AVR TPI | yes | raw only | no | High-level memory operations planned. |
| AVR XMEGA PDI | yes | raw only | physical only | High-level NVM operations planned. |

See `docs/REV_J_DSPIC_AVRDUDE_AVR.md` for AVRDUDE-informed design notes.


## Rev K update

| Family | Programming | Debug | Notes |
|---|---|---|---|
| ATmega2560 / large classic AVR | High-level ISP flash read/write/verify | JTAG via OpenOCD if wired/target supported | Uses Load Extended Address byte for >64K-word flash windows. |
| ATtiny817 / ATmega4809 UPDI | Raw UPDI + guarded NVM HEX planning | UPDI debug transport only | Non-dry-run NVM writes remain experimental until bench validation and exact DFP values. |


## Texas Instruments MSP430/MSP430X - Rev L

| Family | Transport | Programming status | Debug status | Notes |
|---|---|---|---|---|
| MSP430 classic flash | SBW / 4-wire JTAG | TI-TXT/HEX planner + raw TAP primitives; erase/write guarded | physical debug transport | First profile: MSP430G2553. |
| MSP430Xv2 flash | 4-wire JTAG / SBW | TI-TXT/HEX planner + raw TAP primitives; erase/write guarded | physical debug transport | First profile: MSP430F5529. |
| MSP430Xv2 FRAM | SBW / 4-wire JTAG | TI-TXT/HEX planner + raw TAP primitives; FRAM write guarded | physical debug transport | First profiles: MSP430FR5969, MSP430FR6989. |

DirtyJTAG firmware now exposes safe MSP430 TAP operations for both SBW and
4-wire JTAG.  High-level destructive algorithms are intentionally profile-guarded
until board-level validation confirms entry timing, memory controller behavior,
and lock/fuse edge cases.

## Texas Instruments TMS320/C2000 - Rev M

| Family | Transport | Programming status | Debug status | Notes |
|---|---|---|---|---|
| TMS320F2800137 / F28001x | XDS110v3-style IEEE 1149.1 JTAG | no flash algorithm yet | raw JTAG + OpenOCD transport | First C2000 profile. |
| TMS320F280049C / F28004x | XDS110v3-style IEEE 1149.1 JTAG | no flash algorithm yet | raw JTAG + OpenOCD transport | CLA-aware target semantics are host-owned. |
| TMS320F28379D / F2837xD | XDS110v3-style IEEE 1149.1 JTAG | no flash algorithm yet | raw JTAG + OpenOCD transport | Multi-core/TAP details require host-side target support. |
| TMS320F28P650DK / F28P65x | XDS110v3-style IEEE 1149.1 JTAG | no flash algorithm yet | raw JTAG + OpenOCD transport | Profile added for modern C2000 bench bring-up. |

This is separate from MSP430.  The firmware exposes safe JTAG operations for
TMS320/C2000 and rejects 5 V target-power selection.  It is not a native CCS
XDS110 USB clone yet.



## SEGGER RTT

| Backend | RTT status | Notes |
|---|---|---|
| STM8 SWIM native | Implemented for memory read/write backends; CI covers mock target | Uses SWIM ROTF/WOTF memory access. |
| ARM SWD/JTAG via OpenOCD remote-bitbang | Host-side OpenOCD RTT recommended | Pico transports bits; OpenOCD owns memory transactions. |
| dsPIC/PIC Debug Executive | Planned | Requires completed DE memory-access service. |
| AVR/MSP430/TMS320 raw transports | Not yet | Requires a validated memory-access layer above raw transport. |

## Rev O trace and terminal support

| Feature | Status | Notes |
|---|---|---|
| RTT scan/info/read/write | Implemented | Requires selected backend with target memory read/write. |
| RTT arbitrary channel inspection | Implemented | `rtt-channels` uses `DJP2_RTT_CHANNEL_INFO` for all up/down descriptors. |
| RTT virtual terminal split | Host implemented | Splits Channel 0 into virtual terminals 0..15 using the public RTT terminal selector convention. |
| RTT binary logging | Host implemented | `rtt-log` records any up-channel to a file. |
| SystemView-style raw capture | Host implemented | Captures raw RTT channel bytes; no proprietary decoding/visualization. |
| SWO/ITM USB control plane | Implemented | Firmware command path and host CLI exist. |
| SWO/ITM real Pico pin capture | Planned | Needs RP2040 PIO RX timing validation. |


## Rev Q additions

| Family / workflow | Programming | Debug | Notes |
|---|---:|---:|---|
| TI SimpleLink CC13xx/CC26xx SWD | boot-plan / OpenOCD-hosted | OpenOCD transport ready | Uses SWD backend and host profiles; no TI XDS USB emulation. |
| TI SimpleLink CC13xx/CC26xx cJTAG | physical bring-up | physical transport only | Conservative 2-wire cJTAG clock/line support; full 1149.7 negotiation remains future work. |
| Generic GPIO/SPI/I2C/UART bridge | n/a | n/a | For ROM bootloaders, board bring-up, register pokes and fixture control. |
| Production job descriptors | planned executor | n/a | JSON validation/templates implemented; firmware execution remains host-orchestrated. |
| Power trace | n/a | n/a | Captures VTARGET, VPP, target current and power-fault state. |
