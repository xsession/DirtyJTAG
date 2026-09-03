# RP2040 Universal Front-End ADC Calibration

## 1. Purpose

The universal RP2040 frontend uses ADC measurements as operator information and as part of safety decisions such as target-power verification and VPP range checking. Nominal divider and amplifier ratios are not sufficient for production safety without calibration/verification.

Measurements:

- ADC0: VTARGET, nominal x2 divider reconstruction.
- ADC1: VPP, nominal x4.3 divider reconstruction.
- ADC2: target current, nominal 5 mV/mA from 0.1 ohm shunt and INA180A2 gain.

## 2. Firmware calibration parameters

This production update adds build-time calibration coefficients:

```text
CONFIG_DJPROG_VTARGET_SCALE_PPM
CONFIG_DJPROG_VTARGET_OFFSET_MV
CONFIG_DJPROG_VPP_SCALE_PPM
CONFIG_DJPROG_VPP_OFFSET_MV
CONFIG_DJPROG_ITARGET_SCALE_PPM
CONFIG_DJPROG_ITARGET_OFFSET_MA
```

`1000000` ppm means unity gain correction.

The firmware applies:

```text
corrected = nominal * scale_ppm / 1,000,000 + offset
```

Safety thresholds are also Kconfig-controlled:

```text
CONFIG_DJPROG_VPP_MIN_MV
CONFIG_DJPROG_VPP_MAX_MV
CONFIG_DJPROG_3V3_MIN_MV
CONFIG_DJPROG_3V3_MAX_MV
CONFIG_DJPROG_5V_MIN_MV
CONFIG_DJPROG_5V_MAX_MV
```

## 3. Equipment

Recommended minimum:

- calibrated 4.5-digit or better DMM;
- programmable bench supply;
- electronic load or precision resistor set for current calibration;
- oscilloscope for VPP transient verification;
- known-good USB host and production firmware image.

## 4. VTARGET calibration

1. Keep VPP disabled.
2. Apply an external 3.300 V target rail measured by DMM.
3. Record DirtyJTAG `measure` result.
4. Repeat near the lower and upper intended operating points, for example 1.8 V, 3.3 V and 5.0 V where hardware permits.
5. Determine best-fit scale and offset.
6. Rebuild with the selected Kconfig values.
7. Repeat measurements and verify error against the product requirement.

Example single-point gain estimate when offset is negligible:

```text
scale_ppm = reference_mV * 1,000,000 / reported_mV
```

## 5. VPP calibration

1. Disconnect the target.
2. Enable the VPP boost only; do not apply it to a target connector during initial calibration.
3. Measure the VPP rail with the DMM.
4. Read DirtyJTAG VPP measurement.
5. Calculate gain/offset correction.
6. Verify corrected reading across the expected VPP operating window.
7. Check transient overshoot with an oscilloscope.
8. Verify firmware refuses VPP below/above configured safety limits.

Do not widen VPP safety limits merely to compensate for a measurement error. Fix/calibrate the measurement path first.

## 6. Target-current calibration

1. Use an external target supply or approved internal supply configuration.
2. Apply several known loads spanning the intended current range, for example 0, 10, 50, 100, 250 and 400 mA if the hardware/current limiter permits.
3. Measure actual current with a calibrated instrument.
4. Record DirtyJTAG current readings.
5. Fit scale and offset.
6. Rebuild and repeat.
7. Confirm the hardware current limiter independently; ADC calibration is not a substitute for the limiter.

## 7. Acceptance criteria

Define project-specific limits before production. A reasonable engineering starting point is tighter than the firmware power-valid windows so measurement uncertainty cannot consume the whole safety margin.

Record:

- board serial/revision;
- firmware commit and SHA-256;
- calibration equipment IDs/calibration dates;
- raw measurements;
- fitted coefficients;
- final verification results;
- reviewer/date.

## 8. Calibration strategy

Build-time Kconfig calibration is appropriate for prototype and hardware-revision calibration. If production units show significant unit-to-unit spread, add nonvolatile per-unit calibration storage and a locked calibration command instead of producing a unique firmware build per unit.
