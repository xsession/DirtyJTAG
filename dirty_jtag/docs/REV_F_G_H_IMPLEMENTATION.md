# Rev F/G/H Implementation Notes

This revision continues the programmer/debugger expansion in three layers.

## Rev F: RP2040 PIO-assisted SWIM RX

Rev E used PIO for SWIM transmit timing but still sampled receive/ACK bits from CPU GPIO code.  Rev F keeps the same `dj_swim_phy_ops` upper-layer contract and adds a second RP2040 PIO state machine for receive sampling.  The STM8 debug engine in `src/swim.c` is unchanged; only `src/swim_rp2040_pio.c` changes.

Current Pico PHY name returned by `DJP2_PHY_INFO`:

```text
rp2040-pio-txrx
```

Design intent:

- TX state machine generates open-drain low/release timing tokens.
- RX state machine samples at the configured SWIM bit cadence and pushes samples through RX FIFO.
- C code still owns SWIM packet parity, ACK/NACK interpretation and UM0470 command sequencing.

Real hardware validation that still must be done before high-speed SWIM is treated as production-qualified:

1. capture DATA0 at 3.3 V and 5 V target levels;
2. measure translator/front-end propagation delay;
3. adjust RX start offset if needed;
4. verify low-speed entry, high-speed switch, WOTF/ROTF, halt/run/step and breakpoint cycles;
5. store expected Saleae/CSV traces in `tests/logic/`.

The GPIO/mock path remains for CI and deterministic unit tests.

## Rev G: dsPIC Debug-Executive control plane

PIC/dsPIC debugging is not the same as ICSP programming.  The target must run a Debug Executive (DE), and the exact DE command ABI/binaries are not redistributed by this project.  The firmware now implements a clean-room control plane:

- `src/dspic_debug.c`
- `include/djprog/dspic_debug.h`
- native DJP2 debug commands for dsPIC protocol ID 1
- mock target support using `--device mock-dspic30f5011`
- raw dsPIC capsule operations through the dsPIC backend:
  - `0x80`: load debug metadata capsule
  - `0x81`: query loaded capsule metadata
  - `0x82`: execute one SIX instruction
  - `0x83`: REGOUT one 16-bit word

The metadata capsule is deliberately not a Microchip binary.  It only describes which clean-room capabilities the current session may expose after the host has found local DFP/MPLAB assets.

Generate a metadata capsule from a locally installed pack/report:

```sh
python host/dfp_inspect.py path/to/Microchip.dsPIC30F_DFP.x.y.z.atpack \
  --json dspic30-report.json \
  --capsule dspic30-debug.capsule \
  --family 0 \
  --hw-breakpoints 2 \
  --reg-bytes 42
```

Load it after selecting a dsPIC target:

```sh
python host/djprog.py --port COM8 config dspic --device dsPIC30F5011 --power 5v --clock 1000000
python host/djprog.py --port COM8 dspic-load-capsule dspic30-debug.capsule
python host/djprog.py --port COM8 dspic-capsule-info
```

Without a capsule, real-target dsPIC debug run-control returns `unsupported`.  With `mock-dspic...`, the same DJP2 API is exercised in CI without proprietary code.

## Rev H: USB script VM for niche controllers

Many niche MCUs have public programming specs but device-specific algorithms that vary by family/flash geometry.  Reflashing the Pico for each controller family is a bad architecture.  Rev H adds a tiny electrical script VM over USB:

- command: `DJP2_SCRIPT_XFER = 0x45`
- host command: `python host/djprog.py --port COM8 script <hexbytes>`
- firmware: `src/script_vm.c`, `include/djprog/script_vm.h`

The VM can:

- set pin directions;
- write/read logical pins;
- delay in microseconds;
- switch target power;
- switch VPP paths;
- clock arbitrary bit sequences;
- perform SPI transactions;
- perform one-wire UART transactions;
- force safe idle.

This makes PIC10/12/16/18, PIC24 variants, RL78 Protocol A details, MSP430 SBW JTAG state flows and vendor-specific init/unlock sequences configurable from the host without new firmware.

Script bytecodes:

| Opcode | Name | Payload |
|---:|---|---|
| `0x00` | END | none |
| `0x01` | DIR | `role:u8 dir:u8` where dir is 0=input, 1=output, 2=OD-low, 3=release |
| `0x02` | WRITE | `role:u8 value:u8` |
| `0x03` | READ | `role:u8`, returns one byte |
| `0x04` | DELAY_US | `u32le` |
| `0x05` | POWER | `mode:u8` |
| `0x06` | VPP | `path:u8 on:u8`, path 0=boost, 1=MCLR, 2=DATA0 |
| `0x07` | CLOCK_BITS | `clk:u8 dout:u8 din:u8 bits:u8 flags:u8 hz:u32le txbytes` |
| `0x08` | SPI | `hz:u32le len:u16le txbytes`, returns `len` bytes |
| `0x09` | UART1W | `baud:u32le tx:u16le rx:u16le flags:u8 txbytes`, returns `rx` bytes |
| `0x0A` | SAFE_IDLE | none |

Pin role IDs follow `enum dj_pin_role` from `include/djprog/hw.h`.

Example: read DATA0 once:

```sh
python host/djprog.py --port COM8 script "030000"
```

Example: safe idle:

```sh
python host/djprog.py --port COM8 script "0a00"
```

The VM is intentionally electrical.  High-level algorithms should live in host profiles/scripts with exact device-family references and validation vectors.
