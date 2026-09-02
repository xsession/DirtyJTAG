# Rev I - dsPIC30F HEX programming pipeline

Rev I promotes the dsPIC30F path from row-level firmware primitives to a usable host programming workflow for the first target devices:

- `dsPIC30F5011`
- `dsPIC30F4011`

The firmware side already exposes dsPIC row write/read/erase primitives through the DJP2 `WRITE`, `READ`, `ERASE` and `IDENTIFY` commands. Rev I adds the missing host-side layer that converts XC16/MPLAB Intel HEX files into safe, row-aligned 24-bit dsPIC write blocks.

## Clean-room boundary

The implementation is based on publicly documented dsPIC30F ICSP behavior and does not embed Microchip proprietary Programming Executive or Debug Executive binaries. Debug Executive assets still have to come from a local user-installed Microchip DFP/MPLAB installation through the existing capsule workflow.

## Address model

XC16/MPLAB HEX commonly stores each 24-bit dsPIC/PIC24 instruction as four Intel HEX bytes:

```text
low-byte, high-byte, upper-byte, phantom-byte
```

The dsPIC program counter increments by two per instruction. Therefore:

```text
Intel HEX byte address 0x00000000 -> dsPIC PC address 0x000000
Intel HEX byte address 0x00000004 -> dsPIC PC address 0x000002
Intel HEX byte address 0x00000008 -> dsPIC PC address 0x000004
```

The host keeps the Pico firmware simple by doing this conversion before USB transfer. The firmware receives one row as packed 24-bit words:

```text
word0_low, word0_high, word0_upper, word1_low, ...
```

## Safe default behavior

`program-hex` excludes configuration/user-ID addresses by default. The first real-hardware milestone is application flash:

- enter ICSP
- read device ID
- optional bulk erase
- write user flash rows
- optional readback verify

Use `--include-config` only after validating the exact device configuration-word algorithm and board-level recovery path.

## Example

```bash
python host/djprog.py --port COM8 config dspic \
  --device dsPIC30F5011 \
  --power 5v \
  --clock 1000000

python host/djprog.py --port COM8 enter
python host/djprog.py --port COM8 identify

python host/djprog.py --port COM8 program-hex firmware.hex \
  --erase \
  --verify

python host/djprog.py --port COM8 leave
```

Dry-run the conversion and row plan without programming the target:

```bash
python host/djprog.py --port COM8 program-hex firmware.hex --dry-run
```

List installed host algorithm descriptors:

```bash
python host/djprog.py algorithm-list
```

## Added files

- `host/hex_utils.py`
- `tests/test_hex_utils.py`
- `host/algorithms/dspic30f5011.json`
- `host/algorithms/dspic30f4011.json`

## Validation

```bash
./scripts/test-native.sh
# test_core: PASS
# test_hex_utils: PASS

./scripts/check-zephyr-syntax.sh
# PASS, silent on success
```

The runtime used for this package still does not include `west` or the Zephyr SDK, so the actual Pico firmware build remains a CI/workstation validation step:

```bash
west build -p always -b rpi_pico/rp2040 dirty_jtag
```
