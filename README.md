# DirtyJTAG Universal Pico Programmer + Debugger — Rev R

A clean-room, Zephyr-based universal programming/debug probe for Raspberry Pi Pico (RP2040), structured for integration with the Zephyr DirtyJTAG refactor. One firmware image exposes USB-selectable backends for Microchip dsPIC/PIC ICSP, AVR interfaces, STM8 SWIM, MSP430 Spy-Bi-Wire/JTAG, TI TMS320/C2000 XDS110v3-style JTAG, Silicon Labs C2, Renesas RL78, ARM SWD and generic JTAG.

Rev D added the first native non-ARM debugger path: STM8 SWIM halt/run/step/register/breakpoint control through the public UM0470 debug module model. Rev E added a pluggable SWIM PHY and a Raspberry Pi Pico RP2040 PIO-assisted timing path. Rev H adds the dsPIC Debug-Executive clean-room control plane plus a USB script VM so niche MCU support can be configured from the host without reflashing the Pico. Rev M adds a separate TI TMS320/C2000 XDS110v3-style JTAG transport for C2000 bench bring-up. The project deliberately separates three levels of support:

1. **electrical/physical transport** — safe pin control, level shifting, reset/VPP and target power;
2. **programming algorithm** — identify/erase/read/write for device families where public programming specifications have been implemented;
3. **debug run-control** — halt/run/step/register/breakpoint behavior, either natively or through OpenOCD.

A raw wire transport is never advertised as a finished debugger.

## What is usable now for debugging

ARM SWD and generic JTAG work through the included OpenOCD remote_bitbang bridge:

```text
GDB / VS Code / Eclipse / CLion
            |
         OpenOCD
            |
 remote_bitbang TCP
            |
 host/openocd_bridge.py
            |
       DJP2 USB CDC
            |
 Raspberry Pi Pico
            |
 protected SWD/JTAG front end
            |
          target
```

OpenOCD handles target-specific CPU debug, memory/register access, breakpoints/watchpoints and flash loaders; the Pico handles electrical timing, reset, target power and signal routing.

See `docs/DEBUGGING.md`.

## Build for Raspberry Pi Pico

The manifest pins Zephyr **v4.4.1** and uses board target `rpi_pico/rp2040`.

```sh
python -m pip install west
west init -l .
west update
west zephyr-export
west build -b rpi_pico/rp2040 dirty_jtag
west flash
```

This package can run its host-independent tests without a Zephyr SDK:

```sh
./scripts/test-native.sh
```

The native suite is compiled with `-Wall -Wextra -Werror`.

## Host setup

```sh
python -m pip install -r host/requirements.txt
python host/djprog.py --port COM8 hello
python host/djprog.py --port COM8 list
python host/djprog.py --port COM8 devices
python host/djprog.py --port COM8 status
python host/djprog.py --port COM8 measure
```

### ARM SWD debugging

Terminal 1:

```sh
python host/openocd_bridge.py --port COM8 --transport swd --power external
```

Terminal 2:

```sh
openocd -f host/openocd/dirtyjtag-remote-swd.cfg -f target/stm32f4x.cfg
```

Terminal 3:

```sh
arm-none-eabi-gdb build/app.elf
```

GDB example:

```gdb
target extended-remote localhost:3333
monitor reset halt
load
break main
continue
```

Select the OpenOCD target configuration matching the actual MCU.

### Generic JTAG debugging

```sh
python host/openocd_bridge.py --port COM8 --transport jtag --power external
openocd -f host/openocd/dirtyjtag-remote-jtag.cfg -f target/<target>.cfg
```

### Debug capability inspection

```sh
python host/djprog.py --port COM8 config swd --power external
python host/djprog.py --port COM8 debug-info
python host/djprog.py --port COM8 debug-attach
```

Native halt/run/step/register/breakpoint commands exist in DJP2. STM8 SWIM now implements the first native non-ARM engine; incomplete family engines still return `unsupported` instead of pretending to work.


### STM8 SWIM native debugging

Rev H keeps the Rev D native STM8 run-control and expands the Pico PHY toward PIO TX/RX timing:

```sh
python host/djprog.py --port COM8 config swim --device stm8s003f3 --power 3v3 --clock 363000
python host/djprog.py --port COM8 enter
python host/djprog.py --port COM8 debug-attach
python host/djprog.py --port COM8 debug-halt
python host/djprog.py --port COM8 debug-regs
python host/djprog.py --port COM8 debug-bp-set 0x8080 --slot 0
python host/djprog.py --port COM8 debug-run
```

The generic GPIO/SWIM path is mock-tested and suitable for slow bring-up. Rev H's `src/swim_rp2040_pio.c` uses separate RP2040 PIO state machines for transmit timing and receive sampling. Real high-speed readiness still needs logic-analyzer validation on the Rev B front end at each target voltage before it should be considered production-qualified.

Inspect the selected PHY over USB:

```sh
python host/djprog.py --port COM8 phy-info
```

See `docs/STM8_SWIM_DEBUG.md` and `docs/RP2040_PIO_SWIM.md`.

## dsPIC30F / dsPIC33 programming

Initial high-level programming profiles include dsPIC30F4011, dsPIC30F5011 and dsPIC33FJ128GP802.

```sh
python host/djprog.py --port COM8 config dspic \
  --device dsPIC30F5011 --power 5v --clock 1000000
python host/djprog.py --port COM8 enter
python host/djprog.py --port COM8 identify
python host/djprog.py --port COM8 leave
```

PIC/dsPIC source-level debugging requires Microchip's target-side **Debug Executive** and device-specific reserved-resource information. This project does not redistribute proprietary DE binaries. Use the included DFP discovery tool against a pack you have installed/downloaded:

```sh
python host/dfp_inspect.py path/to/family.atpack --json dfp-report.json
```

Rev H adds the firmware-side dsPIC Debug-Executive control plane.  It does not bundle proprietary DE binaries.  For real hardware, generate/load a clean-room metadata capsule from locally installed Microchip packs/assets before enabling high-level debug commands:

```sh
python host/dfp_inspect.py path/to/family.atpack \
  --json dfp-report.json \
  --capsule dspic-debug.capsule
python host/djprog.py --port COM8 config dspic --device dsPIC30F5011 --power 5v --clock 1000000
python host/djprog.py --port COM8 dspic-load-capsule dspic-debug.capsule
python host/djprog.py --port COM8 dspic-capsule-info
```

CI and host integration can use the mock target:

```sh
python host/djprog.py --port COM8 config dspic --device mock-dspic30f5011 --power external
python host/djprog.py --port COM8 debug-attach
python host/djprog.py --port COM8 debug-halt
python host/djprog.py --port COM8 debug-reg-read
```

Without a capsule or mock target, real-target dsPIC debug run-control returns `unsupported` instead of guessing the DE command ABI.

## AVR UPDI

Normal low-voltage UPDI:

```sh
python host/djprog.py --port COM8 config updi --power 3v3 --clock 115200
python host/djprog.py --port COM8 enter
```

If the device requires UPDI high-voltage activation, Rev B/Rev E hardware has an isolated DATA0-HV path. It is opt-in and still requires physical jumper `JP2 UPDI_HV_ENABLE`:

```sh
python host/djprog.py --port COM8 config updi --power 3v3 --clock 115200 --hv-activate
python host/djprog.py --port COM8 enter
```

The firmware isolates the normal DATA0 translator before applying the activation pulse.


## TI TMS320/C2000 XDS110v3-style JTAG

Rev M adds a TMS320/C2000 path that is separate from MSP430:

```sh
python host/djprog.py tms320-profile-list
python host/djprog.py --port COM8 tms320-config tms320f2800137 --power external --clock 1000000
python host/djprog.py --port COM8 enter
python host/djprog.py --port COM8 tms320-idcode
python host/djprog.py --port COM8 tms320-shift-ir 6 3f
python host/djprog.py --port COM8 tms320-shift-dr 32 00000000
```

Connector roles for the basic six-pin universal header are `CLK=TCK`,
`DATA0=TMS`, `DATA1=TDI`, `DATA2=TDO`, `RESET=nRESET/XRSn`, and `AUX=nTRST`
by default.  The backend rejects 5 V target-power selection because XDS110-class
TMS320/C2000 JTAG uses target I/O in the 1.8..3.6 V range.

This is an XDS110v3-style electrical/JTAG transport, not a native CCS XDS110 USB
emulator.  CCS visibility, cJTAG and C2000 flash algorithms remain future work.
See `docs/REV_M_TI_TMS320_XDS110V3.md`.

## USB script VM for niche MCU support

Rev H adds `DJP2_SCRIPT_XFER` and `src/script_vm.c`.  This is an electrical scripting interface for host-side profiles: pin direction, pin write/read, delays, power, VPP, arbitrary bit clocking, SPI and one-wire UART transactions.

Example, read DATA0:

```sh
python host/djprog.py --port COM8 script "030000"
```

Example, force safe idle:

```sh
python host/djprog.py --port COM8 script "0a00"
```

This is how additional niche families should be added first: exact public device algorithms live in host profiles/scripts while the Pico supplies protected, measured electrical operations.  See `docs/REV_F_G_H_IMPLEMENTATION.md`.

## Runtime electrical controls

Bench diagnostics:

```sh
python host/djprog.py --port COM8 pinmap 2 3 4 5 6 7
python host/djprog.py --port COM8 power 3v3
python host/djprog.py --port COM8 vpp boost on
python host/djprog.py --port COM8 vpp apply on
python host/djprog.py --port COM8 vpp data0 on
python host/djprog.py --port COM8 safe
```

Explicit VPP commands are for controlled hardware bring-up. Normal backends should execute their own safe sequence.

## Hardware front end

`hardware/` contains the Rev B electrical source of truth used by Rev E firmware:

- `FRONTEND_DESIGN.md`
- `SCHEMATIC_CONNECTIONS.csv`
- `BOM.csv`
- `connector_pinout.csv`
- `frontend_rev_b.svg` / `.png`

The older `dirtyjtag_universal_frontend_rev_a0_legacy.sch` is retained as a conceptual capture only and must not be treated as fabrication source.

The front end provides:

- programmer-sourced ~3.3 V / ~5.0 V or externally powered targets;
- ~500 mA protected target output and current measurement;
- target-voltage-aware bidirectional translators;
- open-drain reset/MCLR;
- measured ~11.8 V MCLR/VPP path with physical authorization;
- separately isolated ~11.8 V UPDI DATA0 activation path with its own physical authorization;
- SWD/JTAG wiring including optional SWO on DATA2 and nTRST on AUX.

Run KiCad ERC/DRC and verify exact footprint pin numbers/absolute maximum ratings before fabrication.

## Current family status

See `docs/SUPPORT_MATRIX.md` for the authoritative matrix. In short:

- **ARM SWD/JTAG:** usable debugger through OpenOCD/GDB now.
- **dsPIC30F/dsPIC33F:** programming present; Debug Executive integration is the remaining source-level debug layer.
- **STM8 SWIM:** native debug ABI/DM engine implemented; high-speed PIO physical validation remains before field use.
- **AVR UPDI/PDI, MSP430 SBW, Silicon Labs C2:** physical debug transport architecture exists; native high-level run-control is not yet claimed.
- **TPI:** programming transport only.

## Clean-room boundary

Implementation is based on public device/programming/debug documentation and independently observable protocol behavior. Proprietary PICkit/MPLAB source is not copied. User-installed Microchip packs can be inspected/referenced locally without bundling their proprietary debug binaries into this repository.

See:

- `docs/CLEANROOM.md`
- `docs/DEBUGGING.md`
- `docs/USB_PROTOCOL.md`
- `docs/SUPPORT_MATRIX.md`


### Rev I dsPIC30F HEX programming

The host can now program XC16/MPLAB Intel HEX files into dsPIC30F user flash as row-aligned 24-bit words. Configuration words are excluded by default for safety.

```bash
python host/djprog.py --port COM8 config dspic --device dsPIC30F5011 --power 5v --clock 1000000
python host/djprog.py --port COM8 enter
python host/djprog.py --port COM8 identify
python host/djprog.py --port COM8 program-hex firmware.hex --erase --verify
python host/djprog.py --port COM8 leave
```

Inspect the plan before touching flash:

```bash
python host/djprog.py --port COM8 program-hex firmware.hex --dry-run
python host/djprog.py algorithm-list
```

See `docs/REV_I_DSPIC_HEX_PROGRAMMING.md`.

## Rev J quick start: classic AVR ISP

List supported AVR descriptors:

```bash
python host/djprog.py avr-profile-list
```

Configure an ATmega328P target and program an Intel HEX image:

```bash
python host/djprog.py --port COM8 avr-config atmega328p --power 5v --clock 125000
python host/djprog.py --port COM8 enter
python host/djprog.py --port COM8 avr-signature
python host/djprog.py --port COM8 avr-fuses
python host/djprog.py --port COM8 program-avr-hex blink.hex --profile atmega328p --erase --verify
python host/djprog.py --port COM8 leave
```

Rev J mirrors AVRDUDE's useful split between programmer type, part descriptor,
memory operation and raw/direct instruction mode, but does not copy AVRDUDE GPL
source.  See `docs/REV_J_DSPIC_AVRDUDE_AVR.md`.

## Rev J quick start: dsPIC30F bench bring-up

```bash
python host/dspic30_bench.py --port COM8 --device dsPIC30F5011 --power external --hex firmware.hex
```

Full erase/program/verify remains explicit:

```bash
python host/dspic30_bench.py --port COM8 --device dsPIC30F5011 --power 5v --hex firmware.hex --program-full
```


## Rev K - AVR extended-address ISP + UPDI NVM planning

- ATmega2560/large classic AVR high-level ISP flash read/write/verify now uses the public Load Extended Address byte command when crossing 64K-word flash windows.
- Added UPDI profile descriptors and a guarded NVM HEX planning path for ATtiny817 and ATmega4809.
- Added `updi-profile-list`, `updi-config`, and `program-updi-hex --dry-run`.
- Created a local git commit for this milestone; pushing requires a connected GitHub remote/token in the execution environment.


## Rev L: TI MSP430/MSP430X

DirtyJTAG Universal Pico now includes first-stage Texas Instruments 16-bit MCU
support for MSP430/MSP430X:

- `sbw` protocol ID 30: 2-wire Spy-Bi-Wire.
- `msp430-jtag` protocol ID 31: 4-wire MSP430 JTAG.
- Host profiles: MSP430G2553, MSP430F5529, MSP430FR5969, MSP430FR6989.
- TI-TXT and Intel HEX planning with flash/FRAM range validation.
- Raw TAP commands: TAP reset, shift IR, shift DR, constant clock.

Example:

```bash
python host/djprog.py msp430-profile-list
python host/djprog.py --port COM8 msp430-config msp430g2553 --interface sbw --power 3v3 --clock 100000
python host/djprog.py --port COM8 enter
python host/djprog.py --port COM8 msp430-tap-reset --cycles 8
python host/djprog.py --port COM8 msp430-shift-ir 8 91
python host/djprog.py program-msp430 firmware.txt --profile msp430g2553 --dry-run
```

Erase/write/debug run-control remain guarded until physical board validation.
See `docs/REV_L_TI_MSP430_SUPPORT.md`.


## SEGGER RTT

DirtyJTAG includes DJP2 commands and host CLI helpers for SEGGER RTT control-block scanning, channel information, up-buffer reads, and down-buffer writes on backends that expose target memory read/write.

```bash
python host/djprog.py --port COM8 rtt-scan 0x20000000 0x20010000
python host/djprog.py --port COM8 rtt-info 0x20000000
python host/djprog.py --port COM8 rtt-read 0x20000000 --channel 0 --length 256
python host/djprog.py --port COM8 rtt-write 0x20000000 "help\n" --channel 0
```

For ARM SWD/JTAG sessions through OpenOCD remote-bitbang, use OpenOCD's native RTT commands instead; the Pico remains the SWD/JTAG transport in that mode.

## Rev O SEGGER-inspired trace workflows

DirtyJTAG now includes practical clean-room equivalents for several high-value SEGGER-style workflows:

- richer RTT channel inspection (`rtt-channels`);
- RTT terminal tailing and input (`rtt-tail`, `rtt-write`);
- RTT virtual terminal splitting (`rtt-terminals`);
- binary RTT logging (`rtt-log`);
- raw SystemView/event-stream capture over RTT (`sysview-capture`);
- SWO/ITM capture control plane (`swo-config`, `swo-start`, `swo-read`, `swo-tail`).

See `docs/SEGGER_TRACE_WORKFLOWS.md` and `docs/REV_O_SEGGER_TRACE_FEATURES.md`.

### Rev P vendor feature deepsearch

DirtyJTAG now includes a clean-room vendor feature matrix covering 50+ vendors/products. Use it to discover practical features worth implementing without copying proprietary protocols:

```bash
python host/vendor_features.py summary
python host/vendor_features.py list --min-priority 9
python host/vendor_features.py show SEGGER
python host/vendor_features.py roadmap --top 15
python host/vendor_features.py export-csv vendor_features.csv
```

See `docs/VENDOR_FEATURE_DEEPSEARCH_50.md`.


## Rev Q: bridge modes, production jobs, power trace, TI SimpleLink

Rev Q implements the highest-value generic findings from the 50+ vendor scan:

- GPIO/SPI/I2C/UART bridge commands over the universal connector.
- EnergyTrace/ULINKplus-style target power trace sampling.
- Production-job JSON descriptors for repeatable programming/debug workflows.
- Bootloader-entry profiles for families such as TI CC13xx/CC26xx.
- TI SimpleLink CC13xx/CC26xx SWD and cJTAG protocol backends.

Examples:

```bash
python host/djprog.py simplelink-profile-list
python host/djprog.py simplelink-boot-plan cc2652r --transport uart
python host/djprog.py --port COM8 simplelink-config cc2652r --interface swd --power external
python host/djprog.py --port COM8 bridge-info
python host/djprog.py --port COM8 bridge-spi 9f0000 --hz 1000000
python host/djprog.py --port COM8 bridge-i2c 0x50 --tx 0000 --rxlen 16
python host/djprog.py --port COM8 power-trace --samples 32 --interval-ms 10
```


## Medical-grade safety review

Rev R adds a medical-device-style safety review and command-level arming for high-risk operations. See [`docs/SAFETY_MEDICAL_GRADE_REVIEW.md`](docs/SAFETY_MEDICAL_GRADE_REVIEW.md). This project is not a certified medical device and must not be connected to patient-connected equipment without a formal system safety assessment.
