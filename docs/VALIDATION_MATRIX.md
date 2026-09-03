# DirtyJTAG Validation Matrix

This file records qualification evidence. Source-code presence is not qualification.

## Status definitions

- `qualified`: production acceptance evidence recorded.
- `validated-lab`: real-hardware function demonstrated; environmental/edge qualification incomplete.
- `ci-only`: unit/mock/build coverage only.
- `experimental`: engineering use only.
- `unsupported`: incomplete high-level implementation.
- `not-recorded`: no auditable result currently committed.

## Firmware platforms

| Platform | Frontend | CI build | Hardware evidence in repo | Current status |
|---|---|---:|---:|---|
| Raspberry Pi Pico RP2040 | Universal DJP2 | yes | not sufficient for production qualification | `ci-only` |
| DirtyJTAG Blue Pill STM32F103 | Legacy USB | yes | not recorded | `ci-only` |
| Olimex STM32-H103 | Legacy USB | yes | not recorded | `ci-only` |
| STM32 minimum dev STM32F103 | Legacy USB | yes | not recorded | `ci-only` |
| NodeMCU ESP-32S | Legacy UART | added to production CI by this update | not recorded | `ci-only` |

## Protocol and feature qualification

| Feature/family | Software implementation | Real-hardware production evidence | Status |
|---|---|---|---|
| ARM SWD / generic JTAG transport | OpenOCD remote-bitbang path implemented | target-specific evidence not centrally recorded | `validated-lab` pending records |
| dsPIC30F programming | high-level flow and HEX planning present | qualification record required per device | `experimental` |
| dsPIC33F programming/debug | partial family/debug-executive architecture | qualification record required | `experimental` |
| AVR ISP | flash/signature/fuse flows present | qualification record required per device | `experimental` |
| AVR UPDI | transport + planning/HV activation architecture | high-level NVM qualification incomplete | `experimental` |
| STM8 SWIM | native debug engine + RP2040 PIO work | timing/voltage qualification incomplete | `experimental` |
| MSP430 SBW/JTAG | raw TAP/transport + host planning | erase/write/run-control qualification incomplete | `experimental` |
| TMS320/C2000 | JTAG/XDS110-style transport | CCS/XDS personality and flash qualification incomplete | `experimental` |
| TI CC13xx/CC26xx | SWD/cJTAG profile/backend architecture | device programming/debug qualification incomplete | `experimental` |
| Silicon Labs C2 | transport/programming architecture | qualification record required | `experimental` |
| Renesas RL78 | serial transport architecture | qualification record required | `experimental` |
| RTT | memory-backend RTT handling present | backend-specific validation required | `experimental` |
| SWO/ITM capture | control/ring-buffer architecture present | PIO timing capture validation incomplete | `experimental` |
| SPI/I2C/UART/GPIO bridge | generic bridge commands present | voltage/timing/bus-contention validation required | `experimental` |
| Target power/VPP measurement | implemented on RP2040 frontend | calibration and production fixture evidence required | `experimental` |

## Required validation record fields

For every production-qualified target add a row with:

| Field | Required content |
|---|---|
| Date | ISO date |
| Reviewer | accountable engineer |
| DirtyJTAG commit/tag | exact source identifier |
| Firmware SHA-256 | exact binary hash |
| DirtyJTAG hardware revision | PCB/front-end revision |
| Target board | exact board revision |
| MCU | full orderable/device identifier |
| Target voltage | measured nominal and tolerance |
| Protocol | exact programming/debug interface |
| Test equipment | DMM/scope/logic analyzer/programmer references |
| Test cases | identify, erase, program, verify, debug, fault cases as applicable |
| Result | pass/fail plus deviations |
| Evidence | log/report/trace path |

## Production promotion rule

A protocol/device combination may be changed to `qualified` only when an auditable validation record covers the destructive operations and relevant electrical corner cases used in production.
