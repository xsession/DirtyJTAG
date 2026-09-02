# Rev Q - Vendor findings implementation + TI SimpleLink CC13xx/CC26xx support

Rev Q turns the 50+ vendor feature research into generic DirtyJTAG features rather than proprietary probe clones.

## Implemented generic features

- **Bridge commands**: GPIO, bit-banged SPI, bit-banged I2C, and bit-banged UART over the universal connector roles.
- **Power/event trace command**: compact target voltage, VPP, target-current and fault samples for EnergyTrace/ULINKplus-style workflows.
- **Production-job descriptors**: JSON job model for power preflight, target configuration, erase/program/verify, logging, reports and safe-idle.
- **Bootloader-entry profiles**: ROM bootloader planning layer for families where debug flash algorithms are not yet native.
- **Feature database status update**: bridge, power-trace and production-programming findings are now marked as implemented-partial where the generic implementation covers the vendor takeaway.

## TI SimpleLink CC13xx/CC26xx

Added two clean-room protocol backends:

- `ti-simplelink-cc13xx-cc26xx-swd` (`simplelink-swd`, protocol 80)
- `ti-simplelink-cc13xx-cc26xx-cjtag-2wire` (`simplelink-cjtag`, protocol 81)

The SWD backend is OpenOCD-transport-ready, so target semantics remain host-owned. The cJTAG backend exposes a conservative physical bring-up path; full IEEE 1149.7 class negotiation remains future work.

Profiles added:

- CC1310
- CC1352P7
- CC2640R2F
- CC2652R
- CC2674R10

The ROM bootloader layer is host-side and descriptor-driven. It plans UART/SPI bootloader entry using CCFG/backdoor/reset sequencing, but it does not yet implement the complete SWRA466 download/send-data command stream in firmware.

## Example commands

```bash
python host/djprog.py simplelink-profile-list
python host/djprog.py simplelink-boot-plan cc2652r --transport uart
python host/djprog.py --port COM8 simplelink-config cc2652r --interface swd --power external --clock 1000000
python host/djprog.py --port COM8 enter
python host/djprog.py --port COM8 debug-attach
python host/djprog.py --port COM8 bridge-info
python host/djprog.py --port COM8 power-trace --samples 32 --interval-ms 10
```

## Safety boundaries

- CC13xx/CC26xx backends reject 5 V target power.
- SWD/cJTAG transport does not emulate TI XDS USB protocols.
- ROM bootloader entry depends on device/package CCFG/backdoor configuration.
- Bridge commands are electrical tools; destructive programming is handled by explicit backend/profile logic only.
