EESchema Schematic File Version 4
LIBS:power
LIBS:device
LIBS:Connector_Generic
EELAYER 29 0
EELAYER END
$Descr A4 11693 8268
Sheet 1 1
Title "DirtyJTAG Universal Pico Programmer Front-End Rev A1"
Date "2026-09-01"
Rev "A1"
Comp "CodeLayer / DirtyJTAG"
Comment1 "Net source of truth: SCHEMATIC_NETLIST.md; BOM: BOM.csv"
Comment2 "3.3/5V protected target power, translated I/O, open-drain reset, authorized ~12V MCLR/VPP"
$EndDescr
Text Notes 600 600 0 100 ~ 20
TARGET POWER: VBUS -> TPS630702 -> TPS2553 -> 0.1R SHUNT -> VTARGET
Text Notes 600 900 0 60 ~ 0
TPS630702: EN=GP13; VSEL=GP14; Rtop=470k, Rbottom=150k, Rfb2=221k; 1.5uH; 3.3/5.0V selectable.
Text Notes 600 1100 0 60 ~ 0
TPS2553: EN=GP15; FAULT#=GP18; RILIM=59k. INA180A2 across 0.1R shunt -> GP28 ADC2.
$Comp
L Connector_Generic:Conn_01x10 J1
U 1 1 1
P 9800 1900
F 0 "J1" H 9880 1892 50 0000 L CNN
F 1 "UNIVERSAL_TARGET_2x5_LOGICAL" H 9880 1801 50 0000 L CNN
	1    9800 1900
	1 0 0 -1
$EndComp
Text Label 9000 1500 2 50 ~ 0
VTARGET
Text Label 9000 1600 2 50 ~ 0
GND
Text Label 9000 1700 2 50 ~ 0
SIG_CLK
Text Label 9000 1800 2 50 ~ 0
SIG_DATA0
Text Label 9000 1900 2 50 ~ 0
SIG_DATA1
Text Label 9000 2000 2 50 ~ 0
SIG_DATA2
Text Label 9000 2100 2 50 ~ 0
RESET_VPP
Text Label 9000 2200 2 50 ~ 0
GND
Text Label 9000 2300 2 50 ~ 0
SIG_AUX
Text Label 9000 2400 2 50 ~ 0
NC
Wire Wire Line
	9000 1500 9600 1500
Wire Wire Line
	9000 1600 9600 1600
Wire Wire Line
	9000 1700 9600 1700
Wire Wire Line
	9000 1800 9600 1800
Wire Wire Line
	9000 1900 9600 1900
Wire Wire Line
	9000 2000 9600 2000
Wire Wire Line
	9000 2100 9600 2100
Wire Wire Line
	9000 2200 9600 2200
Wire Wire Line
	9000 2300 9600 2300
Wire Wire Line
	9000 2400 9600 2400
Text Notes 600 1800 0 100 ~ 20
SIGNALS: FIVE SN74LVC1T45 TRANSLATORS (VCCA=PICO_3V3, VCCB=VTARGET)
Text Notes 600 2100 0 60 ~ 0
U5 CLK: GP2/A, DIR=GP8, B--33R-->SIG_CLK
Text Notes 600 2300 0 60 ~ 0
U6 DATA0: GP3/A, DIR=GP9, B--220R-->SIG_DATA0; optional SJ1+4.7k pull-up to VTARGET, DNP by default
Text Notes 600 2500 0 60 ~ 0
U7 DATA1: GP4/A, DIR=GP10, B--33R-->SIG_DATA1
Text Notes 600 2700 0 60 ~ 0
U8 DATA2: GP5/A, DIR=GP11, B--33R-->SIG_DATA2
Text Notes 600 2900 0 60 ~ 0
U9 AUX: GP7/A, DIR=GP12, B--33R-->SIG_AUX
Text Notes 600 3200 0 60 ~ 0
IMPORTANT: SN74LVC1T45 has DIR but NO OE. Safe idle = DIR B->A plus Pico A pin input.
$Comp
L Transistor_FET:2N7002 Q3
U 1 1 2
P 7350 4000
F 0 "Q3" H 7555 4046 50 0000 L CNN
F 1 "2N7002 RESET SINK" H 7555 3955 50 0000 L CNN
	1    7350 4000
	1 0 0 -1
$EndComp
Text Label 6900 4000 2 50 ~ 0
RESET_LOW_CTL_GP6
Text Label 7450 3650 0 50 ~ 0
RESET_VPP
Wire Wire Line
	6900 4000 7150 4000
Wire Wire Line
	7450 3800 7450 3650
$Comp
L power:GND #PWR01
U 1 1 3
P 7450 4350
F 0 "#PWR01" H 7450 4100 50 0001 C CNN
F 1 "GND" H 7455 4177 50 0000 C CNN
	1    7450 4350
	1 0 0 -1
$EndComp
Wire Wire Line
	7450 4200 7450 4350
Text Notes 600 3900 0 100 ~ 20
MCLR/VPP: VBUS -> TPS61040 (~12.17V) -> Q1 AO3401A -> JP1 VPP_ENABLE -> 100R -> RESET_VPP
Text Notes 600 4200 0 60 ~ 0
TPS61040 EN=GP16; 10uH + SS14; feedback 887k/100k; VPP12 ADC divider 330k/100k -> GP27 ADC1.
Text Notes 600 4450 0 60 ~ 0
Q1 gate pulled up 100k to VPP12; Q2 MMBT3904 base GP17 via 10k with 100k pulldown; 8.2V zener clamps Q1 VGS.
Text Notes 600 4700 0 60 ~ 0
JP1 is physical HV authorization and is OPEN by default. Q3 independently pulls RESET_VPP low; GP6 high asserts reset.
Text Notes 600 5000 0 60 ~ 0
REV A1 HAS NO HIGH-VOLTAGE DATA0 PATH. DO NOT APPLY 12V UPDI ACTIVATION THROUGH SN74LVC1T45.
Text Notes 600 5450 0 100 ~ 20
ADC / CONTROL NETS
Text Notes 600 5750 0 60 ~ 0
GP26 ADC0 = VTARGET via 100k/100k. GP27 ADC1 = VPP12 via 330k/100k. GP28 ADC2 = INA180A2 OUT.
Text Notes 600 6000 0 60 ~ 0
GP13=TGT_REG_EN; GP14=TGT_VSEL; GP15=TGT_SW_EN; GP18=TGT_FAULT_N.
Text Notes 600 6300 0 80 ~ 12
This legacy sheet is a readable conceptual capture. SCHEMATIC_NETLIST.md is the exact net contract; verify IC package pin numbers and run ERC/DRC before PCB fabrication.
$EndSCHEMATC
