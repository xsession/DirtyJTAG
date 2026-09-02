# Rev J — dsPIC30F bench workflow and AVRDUDE-informed AVR support

Rev J turns the previous dsPIC30F HEX pipeline into a safer bench workflow and
adds first-class classic AVR ISP support on top of the existing Pico firmware.

## Clean-room use of AVRDUDE

AVRDUDE is used as a public behavioural reference for architecture, not as a
source-code donor.  The useful model we mirror is:

- programmer type separated from part definition;
- part memory geometry stored in descriptors;
- memory operations for flash, EEPROM, fuses, lock bits and signature bytes;
- a direct instruction/raw mode for cases where the programmer supports a
  low-level instruction but the host has no high-level command yet.

This matters because AVRDUDE is GPL-licensed, while this project is MIT.  Do not
copy AVRDUDE source into firmware or host tools unless the project license is
changed and provenance is documented.

## Added host commands

List AVR descriptors:

```bash
python host/djprog.py avr-profile-list
```

Configure a classic AVR ISP target from a descriptor:

```bash
python host/djprog.py --port COM8 avr-config atmega328p --power 5v --clock 125000
python host/djprog.py --port COM8 enter
python host/djprog.py --port COM8 avr-signature
python host/djprog.py --port COM8 avr-fuses
```

Program an Intel HEX image through high-level AVR ISP flash writes:

```bash
python host/djprog.py --port COM8 program-avr-hex blink.hex \
  --profile atmega328p \
  --erase \
  --verify
```

Dry-run without touching flash:

```bash
python host/djprog.py --port COM8 program-avr-hex blink.hex \
  --profile atmega328p \
  --dry-run
```

Write one fuse byte using the classic ISP direct instruction path:

```bash
python host/djprog.py --port COM8 avr-fuses --write lfuse 0xff
```

## Included AVR profiles

- `atmega328p`: fully supported classic ISP flash/signature/fuse path.
- `attiny85`: fully supported classic ISP flash/signature/fuse path.
- `atmega32u4`: fully supported classic ISP flash/signature/fuse path; bootloader
  protection remains a host policy.
- `atmega2560`: descriptor included, but high-level writes are refused above
  64 KiB until firmware extended-address command handling is validated.
- `attiny817_updi`: UPDI raw/HV descriptor for planning; high-level NVM commands
  are deliberately still marked planned.

## dsPIC30F bench workflow

The new `host/dspic30_bench.py` script runs a conservative hardware bring-up
sequence:

1. safe idle;
2. measure target/VPP/current;
3. configure dsPIC backend;
4. enter ICSP;
5. identify;
6. optional HEX dry-run;
7. optional full erase/program/verify after user explicitly requests it;
8. leave and safe idle.

Example:

```bash
python host/dspic30_bench.py --port COM8 \
  --device dsPIC30F5011 \
  --power external \
  --hex firmware.hex
```

Full programming is intentionally explicit:

```bash
python host/dspic30_bench.py --port COM8 \
  --device dsPIC30F5011 \
  --power 5v \
  --hex firmware.hex \
  --program-full
```

## Hardware notes

Classic AVR ISP uses:

| AVR signal | Universal connector role |
|---|---|
| SCK | CLK |
| MOSI | DATA1 |
| MISO | DATA2 |
| RESET | RESET/VPP, low-voltage reset only |
| VCC | VTARGET |
| GND | GND |

For classic AVR ISP, do **not** enable the 12 V VPP path.  The firmware AVR ISP
backend never requests VPP; the host fuse command uses only the 4-byte ISP
instruction path.

## Remaining work

- Implement and validate the AVR extended-address byte path for ATmega128/2560.
- Promote UPDI NVM read/write from raw transport to high-level programming.
- Add TPI high-level memory operations for ATtiny4/5/9/10 class parts.
- Add AVRDUDE compatibility bridge mode later if direct use from `avrdude -c ...`
  becomes a hard requirement.
