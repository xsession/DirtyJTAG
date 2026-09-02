# Revision History

## Rev H — scriptable niche MCU support + dsPIC debug control plane

- Added `DJP2_SCRIPT_XFER` electrical script VM so host profiles can implement niche MCU programming/debug sequences without reflashing the Pico.
- Added dsPIC Debug-Executive clean-room control plane and metadata capsule loader.
- Added mock dsPIC debug target for CI (`mock-dspic30f5011`).
- Added raw dsPIC operations for capsule load/query plus controlled SIX/REGOUT.
- Extended STM8 RP2040 PIO SWIM PHY from TX-only to a TX/RX two-state-machine design.
- Native tests now cover script VM and dsPIC debug mock API.

## Rev E — RP2040 PIO-assisted STM8 SWIM

- Pluggable SWIM PHY.
- RP2040 PIO TX timing path.
- `DJP2_PHY_INFO` host/firmware command.

## Rev D — STM8 native debugging

- Native STM8 SWIM debug-module engine for halt/run/step/register/breakpoints.

## Rev C — programmer + debugger

- OpenOCD remote_bitbang SWD/JTAG bridge.
- First-class DJP2 debug namespace.

## Rev B — protected universal front end

- Target power, VPP MCLR, UPDI HV DATA0, current/voltage telemetry.


## Rev I - dsPIC30F HEX programming pipeline

- Added host Intel HEX parser with checksum validation and sparse address-map handling.
- Added dsPIC/PIC24 INHX32 conversion: four HEX bytes become one 24-bit instruction word, mapped to dsPIC PC address units.
- Added `program-hex` host command for dsPIC row planning, row write and optional readback verify.
- Added safe default exclusion of configuration/user-ID regions.
- Added host algorithm descriptors for dsPIC30F5011 and dsPIC30F4011.
- Added Python unit tests for HEX parsing, dsPIC address mapping, row filling and packed row payloads.

## Rev J — dsPIC30F bench workflow + AVRDUDE-informed AVR support

- Added `host/dspic30_bench.py` for conservative dsPIC30F5011/4011 real-hardware bring-up.
- Added `host/avr_utils.py` and classic AVR ISP Intel HEX planning/programming.
- Added AVR JSON part descriptors under `host/avr_algorithms/`.
- Added host commands: `avr-profile-list`, `avr-config`, `avr-signature`, `avr-fuses`, `program-avr-hex`.
- Added tests for AVR HEX paging/profile loading.
- Kept UPDI/TPI/PDI high-level memory algorithms honest: transport exists, high-level NVM algorithms remain staged.


## Rev K - AVR extended-address ISP + UPDI NVM planning

- ATmega2560/large classic AVR high-level ISP flash read/write/verify now uses the public Load Extended Address byte command when crossing 64K-word flash windows.
- Added UPDI profile descriptors and a guarded NVM HEX planning path for ATtiny817 and ATmega4809.
- Added `updi-profile-list`, `updi-config`, and `program-updi-hex --dry-run`.
- Created a local git commit for this milestone; pushing requires a connected GitHub remote/token in the execution environment.


## Rev L - TI MSP430/MSP430X support

- Added 4-wire `msp430-jtag` protocol ID 31.
- Reworked `msp430-sbw` raw operations into a shared MSP430 TAP command envelope.
- Added host MSP430 profiles for G2553, F5529, FR5969, and FR6989.
- Added TI-TXT / Intel HEX image planning and range validation.
- Added CLI commands for MSP430 profile listing, configuration, TAP reset, IR/DR shift, and image dry-run planning.
- Added native C/Python tests for MSP430 utilities and raw firmware transports.

## Rev M - TI TMS320/C2000 XDS110v3-style JTAG support

- Added separate `tms320-c2000-xds110v3-jtag` protocol ID 70 for TMS320/C2000 targets.
- Added clean-room raw JTAG operation envelope: TAP reset, IR shift, DR shift, constant clock, reset/TRST line control.
- Added TMS320/C2000 host profiles for F2800137, F280049C, F28379D and F28P650DK.
- Added CLI commands for C2000 profile listing, configuration, IDCODE scan and raw JTAG operations.
- Integrated the protocol with DJP2 `DEBUG_BITBANG` so OpenOCD remote_bitbang can use it as a JTAG transport.
- Kept CCS-native XDS110 USB emulation and C2000 flash programming explicitly unsupported until separately validated.

