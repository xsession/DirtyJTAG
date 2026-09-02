# Rev K - AVR extended-address ISP and UPDI NVM planning

Rev K promotes the large classic AVR path from descriptor-only to high-level
ISP flash programming for devices such as ATmega2560.

## ATmega2560 / large classic AVR

Classic AVR ISP read, page-load and page-write commands carry only a 16-bit
flash word address. Devices with more than 64K words of flash expose the upper
word-address byte through the public `Load Extended Address byte` instruction:

```text
0x4D 0x00 EXT 0x00
```

The firmware now tracks the active 64K-word window and emits the extended
address command before reads and page writes when needed. The host no longer
refuses ATmega2560 HEX images above 64 KiB.

## UPDI

UPDI support now has descriptor-driven NVM planning for modern tinyAVR/megaAVR
parts. This includes Intel HEX parsing, page planning and UPDI instruction
helpers (`LDCS`, `STCS`, `NVMProg` key metadata). Non-dry-run UPDI NVM writes
remain guarded behind explicit experimental flow because NVMCTRL command values,
flash base addresses and erase/write rules differ across device families and
must be verified against the exact DFP/data sheet before destructive use.

## Commands

```bash
python host/djprog.py avr-profile-list
python host/djprog.py --port COM8 avr-config atmega2560 --power 5v --clock 125000
python host/djprog.py --port COM8 program-avr-hex mega.hex --profile atmega2560 --erase --verify

python host/djprog.py updi-profile-list
python host/djprog.py --port COM8 updi-config attiny817 --power 3v3 --clock 115200
python host/djprog.py --port COM8 program-updi-hex app.hex --profile attiny817 --dry-run
```
