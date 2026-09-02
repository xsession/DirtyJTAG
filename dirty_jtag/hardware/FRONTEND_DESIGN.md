# DirtyJTAG Universal Pico Programmer + Debugger Front-End — Rev B

This document is the electrical source of truth for the RP2040/Raspberry Pi Pico programmer/debugger front end. Rev B matches `src/rpi_pico_hal.c`.

## 1. Power architecture

USB VBUS (nominal 5 V) feeds:

1. the Raspberry Pi Pico module;
2. U1 TPS630702 adjustable buck-boost configured like the TI 3.3 V / 5.0 V EVM, selected by `TGT_VSEL`;
3. U2 TPS2553 current-limited high-side switch between the regulator and VTARGET;
4. U3 TPS61040 boost converter for the shared ~11.8 V high-voltage rail.

Target-power sequence is break-before-make. Firmware first disables U2 and U1, changes VSEL, enables U1, waits for regulation, enables U2, then validates VTARGET and FAULT# before continuing.

Recommended TPS2553 RILIM is 59 kOhm. TI documents this value for a maximum current limit around 500 mA. The hardware current limiter remains authoritative; firmware only reports the fault and current measurement.

## 2. Target voltage selection

U1 TPS630702 uses the datasheet FB/FB2/VSEL scaling circuit to generate 3.3 V with VSEL low and 5.0 V with VSEL high. Copy the resistor values and compensation/layout around U1 from the TPS630702 3.3/5 V EVM or recalculate from the current datasheet before PCB release.

Signals controlled by Pico:

- GP13 `TGT_REG_EN`
- GP14 `TGT_VSEL`
- GP15 `TGT_SW_EN`
- GP18 `TGT_FAULT_N` input

## 3. Current measurement

Place a 0.1 ohm Kelvin shunt in the VTARGET source path before the target connector. U4 INA180A2 (gain 50 V/V) measures the shunt. At 1 mA the output is about 5 mV, therefore firmware converts ADC millivolts to current with `mA ~= mV / 5`.

INA180 output -> Pico GP28 / ADC2.

## 4. Logic translation

Five SN74LVC1T45 channels translate between Pico 3.3 V and VTARGET:

- U5: CLK
- U6: DATA0
- U7: DATA1
- U8: DATA2
- U9: AUX

VCCA = Pico 3.3 V. VCCB = VTARGET. DIR is controlled separately by GP8..GP12. Put 33 ohm series resistors at the target side.

RESET/MCLR is not passed through a translator. Q3 2N7002 is an open-drain reset sink controlled by GP6.

## 5. VPP/MCLR high-voltage path

U3 TPS61040 generates approximately 11.8 V. Choose the FB divider for the actual TPS61040 feedback reference in the current datasheet; Rev B nominal values are R31=856 kOhm and R32=100 kOhm, targeting roughly 11.8 V.

The rail is measured through 330 kOhm / 100 kOhm divider to GP27/ADC1.

MCLR/VPP application path:

`VPP11V8 -> Q1 AO3401A -> JP1 VPP_MCLR_ENABLE -> R33 100R -> RESET_VPP`

Q2 MMBT3904 pulls Q1 gate low. Q1 gate has 100 kOhm pull-up to VPP and an 8.2 V gate-source zener. GP17 controls Q2. JP1 must be open by default.

Firmware requires boost enabled, RESET not asserted, DATA0-HV inactive and measured VPP inside its safety window before enabling Q1.

## 6. UPDI high-voltage activation path

High-voltage UPDI activation is physically distinct from MCLR/VPP.

Normal DATA0 path:

`U6 SN74LVC1T45 B -> U10 TMUX4827 channel -> R12 33R -> TARGET_DATA0`

U10 is powered from 5 V and selected because it supports bidirectional signals beyond the supply up to the range needed for the intentionally limited ~11.8 V activation rail and provides powered-off isolation. GP19 is `DATA0_ISO_EN`.

HV activation path:

`VPP11V8 -> Q4 AO3401A -> JP2 UPDI_HV_ENABLE -> R34 1k -> TARGET_DATA0`

Q5 MMBT3904 controls Q4 from GP20 `HV_DATA0_APPLY`. Fit a 100 kOhm Q4 gate pull-up and 8.2 V VGS clamp like the MCLR path.

Firmware sequence:

1. disable U10;
2. force the Pico DATA0 pin to input and translator direction B->A;
3. verify VPP is 10.5..12.0 V;
4. apply the DATA0 HV pulse;
5. remove HV;
6. wait for the switching node to settle;
7. re-enable normal DATA0 only when the protocol starts normal signaling.

JP2 must be open unless UPDI HV activation is deliberately required. This prevents a host command or firmware fault from placing HV on DATA0 without physical authorization.

## 7. ADC sensing

- GP26 / ADC0: VTARGET through 100 kOhm / 100 kOhm divider (x2)
- GP27 / ADC1: VPP through 330 kOhm / 100 kOhm divider (x4.3)
- GP28 / ADC2: INA180A2 current sense

Use 1% divider resistors. Calibrate ADC gain/offset for production programmers.

## 8. Pico GPIO assignment

| GPIO | Function |
|---:|---|
| GP2 | CLK A-side |
| GP3 | DATA0 A-side |
| GP4 | DATA1 A-side |
| GP5 | DATA2 A-side |
| GP6 | RESET_LOW_CTL |
| GP7 | AUX A-side |
| GP8 | DIR_CLK |
| GP9 | DIR_DATA0 |
| GP10 | DIR_DATA1 |
| GP11 | DIR_DATA2 |
| GP12 | DIR_AUX |
| GP13 | TGT_REG_EN |
| GP14 | TGT_VSEL |
| GP15 | TGT_SW_EN |
| GP16 | VPP_BOOST_EN |
| GP17 | VPP_MCLR_APPLY |
| GP18 | TGT_FAULT_N input |
| GP19 | DATA0_ISO_EN |
| GP20 | HV_DATA0_APPLY |
| GP26 | VTARGET ADC |
| GP27 | VPP ADC |
| GP28 | target-current ADC |

## 9. Universal target connector

10-pin 2.54 mm header:

1 VTARGET, 2 GND, 3 CLK, 4 DATA0, 5 DATA1, 6 DATA2, 7 RESET/VPP, 8 GND, 9 AUX, 10 reserved.

Dedicated adapters should map this connector to Microchip ICSP/ICD, AVR ISP/UPDI, STM8 SWIM, MSP430 SBW, Silicon Labs C2, RL78 TOOL0 and Cortex SWD/JTAG. For debug use, DATA2 can carry SWO/TDO and AUX can carry nTRST or another device-specific debug signal.

## 10. Layout and protection

- Keep TPS61040 switch loop compact and far from DATA0/SWDIO.
- Use Kelvin routing for the target-current shunt.
- Put 100 nF at every translator and logic IC and adequate bulk capacitance at U1/U2/U3.
- Put the two HV authorization jumpers beside their respective target nets.
- Put test points on VTARGET, VPP11V8, CLK, DATA0, RESET/VPP and GND.
- Route DATA0 so U10 is physically between translator U6 and all target connectors.
- Add ESD protection selected for the normal signal voltage; do not use a clamp on DATA0 that would suppress the intended UPDI HV pulse.
- Before fabrication, run ERC and verify every exact component's absolute maximum ratings against the final VPP tolerance.

## Rev M note: TI TMS320/C2000 XDS110v3-style adapter

For TMS320/C2000 targets, use the universal connector as a basic IEEE 1149.1
JTAG adapter:

| Universal role | C2000/XDS signal |
|---|---|
| CLK | TCK |
| DATA0 | TMS |
| DATA1 | TDI |
| DATA2 | TDO |
| RESET | nRESET / XRSn |
| AUX | nTRST by default; adapter may repurpose as EMU0 |

The present Rev B front end exposes six translated channels, so it is **not** a
full CTI-20/XDS110 replacement with all EMU/trace pins.  It is suitable for
basic TMS320/C2000 JTAG bring-up at target I/O voltages in the XDS110-class
1.8..3.6 V range.  Do not use the 5 V target-source mode for C2000.  If CCS
native compatibility, cJTAG, ET/trace, or full EMU0..EMU4 coverage is required,
add a dedicated C2000 adapter mezzanine or Rev C hardware with additional
translated channels.
