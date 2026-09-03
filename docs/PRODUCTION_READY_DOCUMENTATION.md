# DirtyJTAG Production Readiness Manual

**Status:** production-candidate engineering baseline, not a certified product release  
**Reviewed branch:** `feature/refactor_to_zephyr_based`  
**Reviewed head before this update:** `17f1036accd8e32129545882e5996c796aee7127` (`Add esp32 support`)  
**Primary production target:** Raspberry Pi Pico / RP2040 universal DJP2 frontend  
**Compatibility targets:** STM32 legacy DirtyJTAG frontend and NodeMCU ESP-32S legacy UART frontend

## 1. Purpose

This document defines the minimum engineering, build, verification, hardware, operator and release controls required before a DirtyJTAG firmware image is treated as a production release.

The repository contains a broad set of programmer/debugger transports and experimental family backends. A protocol being present in source code is not equivalent to production qualification. Production qualification is granted only by recorded evidence in `docs/VALIDATION_MATRIX.md` and the release checklist in `docs/RELEASE_PROCESS.md`.

## 2. Product architecture

DirtyJTAG currently has two firmware frontends.

### 2.1 Universal DJP2 frontend

The universal frontend is selected with `CONFIG_DIRTYJTAG_UNIVERSAL_FRONTEND=y` and is currently restricted to RP2040. It provides the protected target-power/VPP hardware abstraction, DJP2 USB protocol, programming/debug backends, bridge functions, safety arming, RTT/SWO support and measurement functions.

Production build target:

```sh
west build dirty_jtag -p always -b rpi_pico/rp2040 -d build-rpi-pico
```

### 2.2 Legacy DirtyJTAG frontend

The compatibility frontend uses the original DirtyJTAG command model. It supports native USB on supported STM32 boards and UART packet transport on NodeMCU ESP-32S.

Representative builds:

```sh
west build dirty_jtag -p always -b dirtyjtag_bluepill/stm32f103xb \
  -d build-bluepill -- -DEXTRA_CONF_FILE=legacy.conf

west build dirty_jtag -p always -b nodemcu_esp32s/esp32/procpu \
  -d build-nodemcu-esp32s -- -DEXTRA_CONF_FILE=esp32s_nodemcu.conf
```

## 3. Production status model

Every feature is classified as one of the following:

| State | Meaning |
|---|---|
| `qualified` | Bench-tested on named hardware with recorded evidence and release criteria passed. |
| `validated-lab` | Functional on real hardware but not yet qualified across voltage/timing/error limits. |
| `ci-only` | Builds/tests pass in CI or mock tests; no recorded hardware qualification. |
| `experimental` | Intended for engineering use only; destructive use requires explicit operator acknowledgement. |
| `unsupported` | Transport or descriptor exists but required high-level behavior is incomplete. |

Only `qualified` items may be advertised as production-supported.

## 4. Required release evidence

A production release SHALL have all of the following:

1. Native host/unit test suite passes with warnings treated as errors where applicable.
2. Zephyr builds pass for every advertised firmware target.
3. Firmware artifacts are produced by CI from the tagged commit.
4. SHA-256 hashes are generated for all release artifacts.
5. `docs/VALIDATION_MATRIX.md` is updated with exact board, MCU, target voltage, protocol, firmware revision and observed result.
6. ADC calibration is completed for each production hardware revision or production calibration policy is documented.
7. VPP and target-power interlocks are bench-verified.
8. Destructive-programming workflows include readback/verify unless the target protocol makes verify impossible and the exception is documented.
9. Safety arming behavior is tested after reset, USB reconnect and protocol change.
10. Known limitations and unsupported combinations are reviewed before release.

## 5. Safety controls

The authoritative safety analysis is `docs/SAFETY_MEDICAL_GRADE_REVIEW.md`.

Production operation requires the following controls:

- power and VPP disabled at startup;
- translator directions in non-driving state at startup;
- explicit safety arm for destructive operations;
- hardware VPP authorization jumpers where required;
- VPP voltage measurement before high-voltage application;
- target-power fault/current monitoring;
- safe-idle execution after errors, aborts and normal shutdown;
- no 5 V selection on target families whose electrical interface is limited to lower voltages;
- no connection to patient-connected medical equipment without system-level isolation and formal safety assessment.

## 6. Electrical production requirements

The Rev B front end documentation is the current electrical design baseline. Before production fabrication:

1. Convert the conceptual/source-of-truth connectivity into a maintained KiCad project.
2. Run ERC and DRC on the exact release schematic/PCB.
3. Verify all translator, analog switch, ADC-divider and MOSFET absolute maximum ratings.
4. Verify the target-current limiter worst-case tolerance.
5. Verify the 12 V VPP rail under no-load and expected-load conditions.
6. Verify DATA0 high-voltage isolation prevents 12 V from reaching the normal level translator.
7. Confirm creepage/clearance and isolation requirements for the intended product environment.
8. Record PCB revision in every hardware validation entry.

The legacy `.sch` file is not approved as production fabrication source unless it is explicitly reviewed and promoted.

## 7. Measurement subsystem

RP2040 measurements currently cover:

- VTARGET through ADC0;
- VPP through ADC1;
- target current through ADC2;
- target-power fault through a dedicated GPIO.

The firmware uses nominal divider/amplifier ratios plus Kconfig calibration coefficients. Production boards must follow `docs/ADC_CALIBRATION.md`.

A production configuration must not rely only on nominal resistor tolerances when voltage measurements are used as safety interlocks.

## 8. Host software

Install host dependencies with:

```sh
python -m pip install -r host/requirements.txt
```

The runtime dependency is intentionally small: `pyserial` plus the Python standard library.

Production hosts should record:

- host OS;
- Python version;
- DirtyJTAG host-tool commit;
- firmware SHA-256;
- target profile/descriptor revision;
- operation log and verify result.

## 9. Programming and debug policy

### 9.1 Programming

Before programming a production target:

1. identify/confirm the exact device;
2. confirm target voltage;
3. confirm image address ranges;
4. preserve configuration/calibration areas unless the approved job explicitly changes them;
5. erase only when required;
6. program;
7. verify readback;
8. save a result record.

### 9.2 Debug

Debug support is divided between native family engines and OpenOCD-based transport. A raw SWD/JTAG transport must not be described as a complete target debugger unless the host stack provides CPU-level run control.

### 9.3 Experimental backends

Features marked experimental must not be used in automated production until a validation record promotes them.

## 10. Production jobs

Production-job descriptors should be immutable inputs to a release process. Each approved job should include:

- target family and exact device;
- interface and target voltage;
- firmware image and SHA-256;
- erase policy;
- verify policy;
- protected regions;
- serialization/calibration behavior, if applicable;
- expected device identification;
- operator confirmation requirements;
- output report path/format.

A production job must begin and end in a safe electrical state.

## 11. CI and release artifacts

CI SHALL build all advertised board targets and upload checksummed binaries. The release artifact set should contain, where generated:

- `zephyr.elf`;
- `zephyr.bin`;
- `zephyr.hex`;
- `zephyr.uf2` for RP2040;
- `zephyr.map` for engineering traceability;
- `SHA256SUMS`;
- release metadata with source commit and Zephyr version.

CI artifacts are candidates. A human-controlled release process decides whether those artifacts are promoted to an official release.

## 12. Configuration management

Release tags should use semantic versions and correspond to `dirty_jtag/VERSION`.

Recommended flow:

```text
feature branch
   -> reviewed pull request
   -> CI green
   -> validation matrix updated
   -> version updated
   -> release candidate tag
   -> hardware smoke test
   -> production tag/release
```

Do not release from an unreviewed working tree.

## 13. Cybersecurity and supply-chain controls

For production use:

- pin the Zephyr revision in `west.yml`;
- review dependency changes before updating the manifest;
- use GitHub branch protection and required CI checks;
- generate checksums for released binaries;
- retain source commit identifiers in release records;
- do not accept unsigned/untrusted programming profiles for automated production;
- treat script VM and bridge commands as privileged target-stimulation interfaces;
- review third-party licenses when importing algorithms or descriptors.

## 14. Current release blockers

The codebase should remain a production candidate until the following are closed:

- real-hardware validation evidence is incomplete for several MCU families;
- the universal front-end still needs a production KiCad PCB source and fabrication review;
- SWIM PIO RX and SWO capture need bench timing evidence before high-speed production claims;
- some dsPIC debug functionality depends on user-provided vendor pack/debug-executive assets;
- several niche backends are transport-level or experimental rather than fully qualified;
- release branch protection/signing policy is not enforced by repository configuration visible in source;
- per-unit or per-revision ADC calibration records must be established for hardware shipped as a product.

## 15. Release acceptance

A release may be called **production-qualified** only when:

- all release-blocking tests are green;
- intended production protocols are marked `qualified` in the validation matrix;
- hardware revision is frozen and reviewed;
- calibration and electrical-safety tests pass;
- release artifacts and checksums are archived;
- release notes identify all experimental/unsupported features;
- an accountable reviewer approves the release record.

Until then, use the term **production candidate** or **engineering release**.
