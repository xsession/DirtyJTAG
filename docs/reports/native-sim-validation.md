# DirtyJTAG native_sim validation report

**Repository:** `xsession/DirtyJTAG`
**Branch:** `feature/refactor_to_zephyr_based`
**Reviewed head:** `17f1036accd8e32129545882e5996c796aee7127` (`Add esp32 support`)
**Date:** 2026-09-03

## Scope

The production RP2040 application cannot be meaningfully executed as
`native_sim` because `DIRTYJTAG_UNIVERSAL_FRONTEND` is intentionally tied to
`SOC_RP2040` and its `rpi_pico_hal.c` depends on real GPIO, ADC, USB CDC and PIO
hardware. A separate Zephyr native_sim application was therefore added under
`tests/native_sim/`. It links the same universal core/backends used by the Pico
firmware against the project's existing mock HAL.

## Functional scenarios covered

- DJP2 frame encode/decode and command dispatch
- backend registration and selection
- target measurement/status responses
- runtime pinmap dispatch
- destructive-operation safety arming
- script-VM safety gating
- dsPIC30F mock Debug Executive run-control
- SWD and JTAG OpenOCD remote-bitbang paths
- MSP430 Spy-Bi-Wire and four-wire JTAG raw TAP paths
- TI TMS320/C2000 JTAG transport
- STM8 SWIM native halt/run/step/register/breakpoint path
- SEGGER RTT scan/channel/read/write behavior
- SWO/ITM control-plane buffering
- GPIO/SPI bridge dispatch and safety gate
- power-trace result framing
- TI SimpleLink CC13xx/CC26xx SWD and cJTAG paths
- dsPIC 24-bit packing helpers

## Executed in this environment

The strict host suite passed in full:

```text
test_core: PASS
test_hex_utils: PASS
test_avr_utils: PASS
test_updi_utils: PASS
test_msp430_utils: PASS
test_tms320_utils: PASS
test_rtt_tools: PASS
test_vendor_features: PASS
test_simplelink_utils: PASS
test_bridge_tools: PASS
test_production_jobs: PASS
```

The universal C scenario set was then rebuilt with GCC AddressSanitizer and
UndefinedBehaviorSanitizer. That run initially found one UB defect in the test
itself: `r.payload[7] << 24` promoted to signed `int` before shifting. The fix
casts each byte to `uint32_t` before shifting. After the fix:

```text
test_core: PASS
```

with both ASan and UBSan enabled.

A layout-equivalent build of the new native_sim wrapper (same current source
paths, Ztest wrapper replaced by a tiny local stub because Zephyr is absent in
this sandbox) also passed with ASan+UBSan. This verifies the source list and
mock-HAL test entry point, but is not claimed as a Zephyr native_sim result.

## native_sim execution status

A real Zephyr `native_sim` build could not be executed inside this ChatGPT
sandbox because no Zephyr checkout or `west` is installed and outbound shell
network access is disabled. The connected GitHub app is read-only for this
repository, so a temporary CI branch could not be created either.

The added test target is designed for the repository-pinned Zephyr toolchain:

```sh
./scripts/test-native-sim.sh
```

which builds/runs both normal and ASan+UBSan `native_sim/native/64` variants.

## Not covered by native_sim

These require hardware-in-the-loop validation and must not be inferred from a
native_sim PASS:

- RP2040 GPIO direction and level-shifter behavior
- target voltage/current ADC accuracy and calibration
- VPP/MCLR and UPDI-HV switching/interlocks
- RP2040 PIO timing for SWIM/SWO
- USB CDC enumeration and host reconnect behavior
- JTAG/SWD signal integrity and maximum stable clock
- real target flash erase/program endurance and verify
- ESP32 USB-UART/legacy transport electrical behavior

## Release recommendation

Require both `scripts/test-native.sh` and `scripts/test-native-sim.sh` in CI.
Treat native_sim as the Zephyr integration/core behavior gate, and maintain a
separate HIL matrix for electrical/programming/debug timing qualification.
