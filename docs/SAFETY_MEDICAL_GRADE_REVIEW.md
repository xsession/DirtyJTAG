# DirtyJTAG Universal Pico — medical-grade safety documentation and codebase review

**Revision:** Rev R — medical-grade safety review and controls
**Date:** 2026-09-02
**Scope:** `dirty_jtag/` firmware, host tools, tests, hardware documentation, production-job descriptors and protocol documentation.
**Codebase snapshot reviewed:** 148 source/config/documentation artifacts; 8,841 counted lines across C headers/sources, Python host/test code and native unit tests.

## 1. Intended use and medical-use boundary

DirtyJTAG Universal Pico is a universal programmer/debugger, trace bridge and production-aid tool for embedded targets. It is **not** a medical device, is **not certified** for clinical use, and must not be connected to patient-applied parts, energized medical equipment, or patient-connected systems unless the complete end product, accessories, isolation system, workflow, verification and quality system are formally assessed by qualified medical-device engineers.

The “medical-grade” treatment in this review means the project has been reviewed using medical-device engineering principles: hazard analysis, risk-control traceability, safe default states, verification evidence, production traceability and explicit unsupported-use boundaries. It does **not** assert conformity, clearance, approval, or certification.

## 2. Reference framework used for the review

The review was structured around these public standard/guidance descriptions:

- IEC 62304 medical-device software life-cycle expectations: software planning, requirements, architecture, implementation, verification, change and maintenance discipline.
- ISO 14971 medical-device risk management expectations: hazard identification, hazardous situations, risk controls, residual-risk evaluation and risk-management-file traceability.
- IEC 60601-1 electrical safety principles: basic safety, essential performance and patient/environment isolation expectations for medical electrical equipment.
- FDA cybersecurity quality-system guidance themes: secure design, threat modeling, SBOM/update processes, secure maintenance, labeling and postmarket vulnerability management.

These references are used as an engineering review framework only. The repository does not contain a complete regulatory submission package.

## 3. Review method

The review covered:

1. Firmware command dispatcher and USB protocol.
2. Electrical high-risk paths: target power, VPP/MCLR, UPDI high-voltage DATA0, bridge GPIO/SPI/I2C/UART, script VM.
3. Debug/programming backends: dsPIC, PIC raw ICSP, AVR ISP/UPDI/PDI/TPI, STM8 SWIM, MSP430, TMS320/C2000, SimpleLink, C2, RL78, SWD/JTAG.
4. Host tools: `djprog.py`, production-job descriptors, bridge tools, profile descriptors and parsers.
5. Hardware docs: front-end power/VPP/translation assumptions and connector mappings.
6. Verification assets: native tests, syntax checks and CI-facing scripts.

## 4. Safety classification used for this tool

| Class | Meaning in this project | Examples |
|---|---|---|
| Informational | Read-only, no target state change expected | `hello`, `status`, `measure`, `devices`, profile lists |
| Low-risk control | Target connection state changes but no destructive memory/power operation | `enter`, `leave`, debug attach/detach on unpowered or externally powered bench targets |
| High-risk electrical | Can drive target pins, power rails or high-voltage paths | `power`, `vpp`, bridge GPIO/SPI/I2C/UART, script VM |
| High-risk destructive | Can erase/program/write target memory or configuration | `erase`, `write`, `program-*` |
| Development-only/experimental | May be incomplete, target-family-specific or destructive if misused | raw ICSP, raw TAP, Debug Executive capsule work, vendor bridge features |

## 5. Key hazards and risk controls

| Hazard ID | Hazard / hazardous situation | Possible harm | Risk control implemented or required | Status |
|---|---|---|---|---|
| H-001 | VPP/MCLR or UPDI-HV applied to wrong pin | Target damage, fixture damage, operator surprise | Hardware jumpers documented; firmware safe-idle disables VPP; Rev R adds command-level safety arming for VPP | Implemented + hardware-dependent |
| H-002 | Target powered at wrong voltage | Target damage, back-powering | Power mode selection, voltage telemetry, C2000/SimpleLink reject 5 V profiles; Rev R safety arming for power changes | Implemented |
| H-003 | Accidental erase/program of clinical firmware | Loss of device function, latent fault | Rev R safety arming for `erase` and `write`; production jobs require verify-after-program and operator confirmation | Implemented |
| H-004 | Script VM toggles arbitrary pins or HV | Target/fixture damage | Rev R safety arming for script execution; docs require scripts to be version-controlled and reviewed | Implemented |
| H-005 | Bridge modes drive a live medical bus | Device malfunction | Rev R safety arming for bridge GPIO/SPI/I2C/UART; bridge use classified as bench-only | Implemented |
| H-006 | Incomplete backend falsely appears production-ready | Misprogramming or misdiagnosis | Support matrix marks experimental/transport-only states; docs state unsupported operations clearly | Partially implemented; keep updating |
| H-007 | Lack of lot/operator traceability in production jobs | Non-reproducible build/programming results | Rev R production-job validation requires report artifact, safe start/end and safety object for destructive jobs | Implemented |
| H-008 | Cybersecurity exposure through host tooling/scripts | Unauthorized target modification | Not network service by default; FDA-style threat model needed before connected deployment | Documented gap |
| H-009 | Patient-connected equipment used without isolation | Shock or equipment malfunction | Explicit medical-use boundary and requirement for external isolation/safety assessment | Documented boundary |
| H-010 | Overcurrent fault ignored during operations | Hardware damage | Measurement/power-fault telemetry exists; production jobs should fail on fault in executor | Partial; executor still roadmap |

## 6. Issues found and fixed in Rev R

### R-001 — No command-level arming for destructive/high-energy operations

**Finding:** The dispatcher allowed high-risk commands such as `ERASE`, `WRITE`, `VPP`, `POWER`, bridge operations and script execution immediately after USB connection. This is acceptable for a hobby probe but below a medical-grade safety posture.

**Fix:** Added firmware safety arming:

- `DJP2_SAFETY_STATUS = 0x4c`
- `DJP2_SAFETY_ARM = 0x4d`
- `DJP2_SAFETY_DISARM = 0x4e`

High-risk command groups now require explicit arming with the phrase:

```text
I understand this can damage hardware
```

Arming is counted by use count and is cleared by `SAFE_IDLE` / `ABORT`.

**Files:**

- `include/djprog/safety.h`
- `src/safety.c`
- `src/usb_proto.c`
- `include/djprog/usb_proto.h`
- `host/djprog.py`
- `tests/test_core.c`

### R-002 — Host CLI did not force explicit acknowledgement before dangerous operations

**Finding:** Host commands such as `erase`, `write`, `program`, `vpp`, `script` and bridge commands could be run accidentally from shell history or copy/paste.

**Fix:** Added global CLI options:

```bash
--safety-confirm
--safety-uses N
```

High-risk commands now abort with a clear message unless `--safety-confirm` is present. The host sends the firmware safety arm packet before the operation.

### R-003 — Production-job descriptors lacked medical-grade traceability checks

**Finding:** Production jobs accepted destructive flows without requiring safety metadata, report artifacts, safe start/end states or verify-after-program sequencing.

**Fix:** `host/production_jobs.py` now validates:

- `target.protocol` and `target.device` are present;
- job starts and ends with `safe`;
- `artifacts.report` is present;
- destructive jobs include `safety.operator_confirmation_required=true`;
- destructive jobs include `safety.verify_after_program=true`;
- every `program` step is followed by a later `verify` step.

### R-004 — Tests did not exercise safety authorization failures

**Finding:** Native tests validated successful VPP/script/bridge operations but did not check that unarmed dangerous operations are rejected.

**Fix:** `tests/test_core.c` now verifies that unarmed VPP/script/bridge calls fail first, then pass only after safety arming.

## 7. Requirement traceability matrix

| Requirement | Implementation | Verification |
|---|---|---|
| SREQ-001: safe idle disables HV and clears high-risk authorization | `dj_hw_safe_idle()`, `DJP2_SAFE_IDLE`, `dj_safety_reset()` | `test_core` safety path |
| SREQ-002: VPP cannot be applied accidentally through DJP2 | `dj_safety_require(DJ_SAFETY_VPP)` | `test_core` rejects unarmed VPP |
| SREQ-003: arbitrary script execution must be explicitly authorized | `dj_safety_require(DJ_SAFETY_SCRIPT)` | `test_core` rejects unarmed script |
| SREQ-004: bridge pin/bus stimulation must be explicitly authorized | `dj_safety_require(DJ_SAFETY_BRIDGE)` | `test_core` rejects unarmed bridge |
| SREQ-005: erase/write must be explicitly authorized | dispatcher guards on `DJP2_ERASE` and `DJP2_WRITE` | native compile + host safety flow |
| SREQ-006: host high-risk commands require operator acknowledgement | `host/djprog.py` safety gate | `py_compile`, manual CLI contract |
| SREQ-007: production job must be traceable and verifiable | `host/production_jobs.py` validation | `test_production_jobs` |
| SREQ-008: unsupported debug/programming paths must remain explicit | support matrix, backend capability flags | existing backend tests and docs |

## 8. Residual risks and required controls before medical-adjacent use

1. **Electrical isolation is not proven.** The Pico/front-end design must not touch patient-connected equipment without a reviewed isolation barrier, creepage/clearance assessment and IEC 60601-1 system evaluation.
2. **No formal IEC 62304 software safety class assignment exists.** A regulated project must create a software development plan, software safety classification, SOUP inventory, requirements, architecture, verification plan and maintenance plan.
3. **No complete ISO 14971 risk-management file exists.** This document is a seed. It must be converted into a controlled risk-management file with severity/probability scoring, risk acceptability criteria and residual-risk review.
4. **The firmware does not authenticate the host.** Safety arming prevents accidental use, not malicious use. A connected/clinical production environment needs access control, audit logging and host hardening.
5. **Many MCU backends are experimental.** Only promote a target family to production after bench validation, read/erase/write/verify evidence and device-specific acceptance tests.
6. **No calibrated metrology.** Target current/voltage measurements are diagnostic, not certified measurement channels.
7. **No production executor yet.** Production-job descriptors are validated, but a locked-down executor that produces signed reports remains a roadmap item.

## 9. Recommended next medical-grade steps

1. Add a controlled **software requirements specification** with IDs tied to every DJP2 command.
2. Add a **software architecture document** covering process/threading, command state machine, backend boundaries and failure modes.
3. Add a **risk-control verification table** with hardware-in-loop test IDs.
4. Add a production executor that produces a signed JSON report with operator, lot, serial number, firmware hash, profile hash, measured voltage/current ranges, command log and final pass/fail.
5. Add SBOM generation and dependency lock files for host tooling and Zephyr modules.
6. Add fuzz tests for DJP2 frame parsing and malformed command payloads.
7. Add hardware-in-loop tests for: target-power fault, VPP interlock, UPDI-HV DATA0 isolation, current-limit behavior and emergency safe-idle.
8. Add explicit `UNSAFE_EXPERIMENTAL` compile-time gating for incomplete target families.

## 10. Current verification result

The following checks passed after Rev R changes:

```text
./scripts/test-native.sh
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

./scripts/check-zephyr-syntax.sh
  PASS

python3 -m py_compile host/*.py tests/*.py
  PASS
```

A real Zephyr `west build -b rpi_pico/rp2040 dirty_jtag` still requires a workstation or CI environment with Zephyr SDK installed.
