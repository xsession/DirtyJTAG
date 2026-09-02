# Debugging architecture

DirtyJTAG Universal Pico is designed as both a programmer and a debug probe. Programming and debugging share the protected target-power/level-translation front end, but the firmware keeps **transport** and **target run-control** separate.

## 1. Debug layers

### Layer A — physical debug transport

The Pico owns electrical signaling and safety:

- ARM SWD: SWCLK/SWDIO/nRESET, optional SWO input on DATA2.
- JTAG: TCK/TMS/TDI/TDO, nRESET and optional nTRST on AUX.
- AVR UPDI/PDI: physical transport only in the current revision.
- STM8 SWIM: Rev H implements native halt/run/step/register/breakpoint control over WOTF/ROTF and includes an RP2040 PIO TX/RX PHY; high-speed hardware margins still require logic-analyzer validation.
- MSP430 Spy-Bi-Wire: physical slot transport only.
- Silicon Labs C2: C2 programming transport exists; native debugger run-control is not yet enabled.
- dsPIC/PIC: ICSP programming is separate from the device Debug Executive used for source-level debugging.

### Layer B — debug controller

The DJP2 USB ABI has a generic debugger command namespace (0x50..0x5B). A protocol can expose native halt/run/register/breakpoint operations when they have been independently implemented from public specifications.

For ARM SWD and generic JTAG, the current production path deliberately delegates target-specific debug semantics to **OpenOCD**. The Pico exposes OpenOCD remote_bitbang SWD/JTAG operations over USB CDC, and `host/openocd_bridge.py` converts between OpenOCD TCP and DJP2.

This avoids duplicating ARM ADIv5/ADIv6, Cortex-M, RISC-V JTAG DTM, vendor flash loaders, breakpoint/watchpoint logic and GDB-server behavior in Pico firmware.

## 2. ARM SWD quick start

Terminal 1:

```sh
python host/openocd_bridge.py --port COM8 --transport swd --power external
```

Terminal 2, choose the OpenOCD target file for the MCU under test:

```sh
openocd \
  -f host/openocd/dirtyjtag-remote-swd.cfg \
  -f target/stm32f4x.cfg
```

Terminal 3:

```sh
arm-none-eabi-gdb build/app.elf
```

Then in GDB:

```gdb
target extended-remote localhost:3333
monitor reset halt
load
break main
continue
```

OpenOCD handles halt/run/step, memory/register access, hardware/software breakpoints, flash programming, reset handling and the GDB remote protocol.

## 3. Generic JTAG

Start the bridge with:

```sh
python host/openocd_bridge.py --port COM8 --transport jtag --power external
```

Then run OpenOCD with:

```sh
openocd \
  -f host/openocd/dirtyjtag-remote-jtag.cfg \
  -f target/<target>.cfg
```

This can support any target OpenOCD already understands through JTAG, subject to electrical compatibility and target-specific reset wiring.

## 4. DJP2 debugger commands

The host CLI can query the debug layer independently of programming:

```sh
python host/djprog.py --port COM8 config swd --power external
python host/djprog.py --port COM8 debug-info
python host/djprog.py --port COM8 debug-attach
```

For the OpenOCD-backed SWD/JTAG path, `debug-attach` means **transport ready**. It does not claim that the MCU is halted. OpenOCD is the owner of CPU run-control and breakpoint state.

The generic ABI also reserves native operations:

```text
DEBUG_HALT
DEBUG_RUN
DEBUG_STEP
DEBUG_RESET
DEBUG_REG_READ
DEBUG_REG_WRITE
DEBUG_BP_SET
DEBUG_BP_CLEAR
```

An unfinished family returns `DJP2_E_UNSUPPORTED`. This is intentional: a raw wire protocol is not advertised as a complete debugger.

## 5. dsPIC30/dsPIC33/PIC debug path

Microchip PIC/dsPIC source-level debugging uses a small **Debug Executive (DE)** placed in target memory. The DE receives debugger commands over the ICSP debug channel and controls the target when a breakpoint or host debug command transfers control to it.

The clean-room implementation therefore has three independent pieces:

1. ICSP electrical/programming support — already present for the initial dsPIC30F/dsPIC33F targets.
2. Device-specific debug resource metadata and DE image — obtained from a user's locally installed Microchip DFP/tool installation; it is **not redistributed** by this project.
3. Clean-room host-to-DE command implementation — to be derived from public documentation and behavior captured from legally owned tools, not copied from proprietary MPLAB/PICkit source.

`host/dfp_inspect.py` scans an installed DFP or `.atpack` and reports candidate debug/programming assets and metadata without decoding or copying private binaries:

```sh
python host/dfp_inspect.py path/to/Microchip.dsPIC30F_DFP.1.x.y.atpack --json dspic30-dfp-report.json
```

Microchip publishes DFPs as `.atpack` archives and states that DFP content includes device programming/debug information and algorithms. The inspector is therefore a discovery tool, not a redistribution mechanism.

### dsPIC hardware wiring

For devices where the primary ICSP pair is also the selected ICD pair, the existing connector is sufficient:

- SIG_CLK -> PGC/EMUC
- SIG_DATA0 -> PGD/EMUD
- RESET_VPP -> MCLR/VPP
- VTARGET/GND as usual

Some dsPIC30/33 devices offer alternate ICD channel pairs selected by configuration bits. A target adapter must route the selected PGCx/PGDx pair. If an alternate EMUC/EMUD pair is physically distinct on a particular device, route it to configurable signal pins instead of assuming the primary pair.

## 6. AVR UPDI/PDI debugging

UPDI-capable AVR devices expose on-chip debug through the UPDI physical interface, but high-level OCD control is device-generation specific. The current firmware exposes the reliable 8E2 half-duplex physical layer and protected optional HV activation. Native register/run-control/breakpoint support remains disabled until the OCD command layer is independently validated.

TPI is treated as programming-only and is not advertised as a debugger.

## 7. STM8 SWIM debugging

ST's UM0470 publicly documents both the SWIM communication protocol and STM8 debug module. Rev E uses those public registers directly: SWIM_CSR at `0x7F80`, the CPU register mirror at `0x7F00..0x7F0A`, and DM breakpoint/control/status registers at `0x7F90..0x7F9A`.

Usable Rev H commands:

```sh
python host/djprog.py --port COM8 config swim --device stm8s003f3 --power 3v3 --clock 363000
python host/djprog.py --port COM8 enter
python host/djprog.py --port COM8 debug-attach
python host/djprog.py --port COM8 debug-halt
python host/djprog.py --port COM8 debug-regs
python host/djprog.py --port COM8 debug-step
python host/djprog.py --port COM8 debug-bp-set 0x8080 --slot 0
python host/djprog.py --port COM8 debug-run
```

The native engine is mock-tested and implements halt/run/step/register access plus two instruction-fetch hardware breakpoints. Hardware use is still marked experimental until the RP2040 PIO timing driver replaces software edge timing for high-speed SWIM. See `STM8_SWIM_DEBUG.md`.

## 8. Hardware signals for debug

The Rev B universal connector is sufficient for the first debug modes:

| Mode | CLK | DATA0 | DATA1 | DATA2 | RESET/VPP | AUX |
|---|---|---|---|---|---|---|
| SWD | SWCLK | SWDIO | optional | SWO | nRESET | optional |
| JTAG | TCK | TMS | TDI | TDO | nRESET | nTRST |
| dsPIC ICD | PGC/EMUC | PGD/EMUD | optional alternate | optional alternate | MCLR/VPP | optional |
| UPDI | - | UPDI | - | - | optional reset | - |
| SWIM | - | SWIM | - | - | optional reset | - |
| SBW | SBWTCK | SBWTDIO | - | - | RST mapping depends on adapter | - |
| C2 | C2CK | C2D | - | - | reset if device requires it | - |

DATA2 is input-capable and can later be used for SWO trace sampling, but continuous SWO trace capture is not implemented in Rev E firmware.

## 9. Performance roadmap

OpenOCD remote_bitbang is intentionally simple and testable but host-paced and relatively slow. The next ARM-performance layer should be CMSIS-DAP v2 (USB bulk) or a batched SWD command protocol while keeping DJP2 for power, safety, programming backends and non-ARM native debuggers.

The current architecture allows that change without altering the target front end.


## SEGGER RTT

The DJP2 firmware now includes generic SEGGER RTT scan/info/read/write commands for selected backends that provide target memory read/write callbacks.  This is useful for native debug backends such as STM8 SWIM.  For ARM SWD/JTAG debug sessions routed through OpenOCD remote-bitbang, use OpenOCD's built-in RTT support because OpenOCD is the component performing target memory transactions.

See `docs/SEGGER_RTT.md`.
