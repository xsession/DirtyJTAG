# DJP2 USB protocol

The Pico enumerates as USB CDC ACM and carries framed binary requests. One firmware image can switch programming and debugging transports at runtime without reflashing the Pico.

## Frame

Little-endian, 20-byte header:

| Offset | Type | Field |
|---:|---|---|
|0|u32|magic `DJP2` = `0x32504A44`|
|4|u16|version = 2|
|6|u16|command|
|8|u16|sequence|
|10|u16|status (responses)|
|12|u16|flags|
|14|u32|payload length|
|18|u16|folded CRC32|

CRC32 is computed over header bytes 0..17 and payload, then folded as `crc ^ (crc >> 16)`.

## Commands

| Cmd | Name | Purpose |
|---:|---|---|
|0x01|HELLO|firmware/board identification|
|0x02|LIST_PROTOCOLS|runtime backends/capabilities/rate limits|
|0x03|CONFIG|select backend/device/power/clock without reflashing|
|0x04|STATUS|selected backend + telemetry + device configuration|
|0x05|LIST_DEVICES|built-in device profiles|
|0x10/0x11|ENTER/LEAVE|enter/leave selected physical target transport|
|0x12|IDENTIFY|backend-specific ID/signature|
|0x20|ERASE|high-level erase when supported|
|0x21/0x22|READ/WRITE|high-level memory operation when supported|
|0x30|RAW_XFER|backend-defined raw framed transport|
|0x40|POWER|direct target-power selection|
|0x41|MEASURE|VTARGET/VPP/current/fault telemetry|
|0x42|SET_PINMAP|six runtime role GPIO values: CLK,DATA0,DATA1,DATA2,RESET,AUX|
|0x43|VPP|bench-control subcommand for boost/MCLR-HV/DATA0-HV|
|0x44|PHY_INFO|selected SWIM physical layer information|
|0x45|SCRIPT_XFER|electrical script VM for host-defined niche MCU algorithms|
|0x50|DEBUG_BITBANG|batched OpenOCD remote_bitbang ASCII operations|
|0x51|DEBUG_INFO|query selected debug transport/capabilities/state|
|0x52|DEBUG_ATTACH|claim/prepare native or OpenOCD debug transport|
|0x53|DEBUG_DETACH|release debug transport|
|0x54|DEBUG_HALT|native halt when implemented|
|0x55|DEBUG_RUN|native continue when implemented|
|0x56|DEBUG_STEP|native single step when implemented|
|0x57|DEBUG_RESET|native reset; payload byte 0 requests halt-after-reset|
|0x58|DEBUG_REG_READ|native backend register-bank response|
|0x59|DEBUG_REG_WRITE|native backend register-bank write|
|0x5A|DEBUG_BP_SET|`u32 address,u8 type,u8 slot`|
|0x5B|DEBUG_BP_CLEAR|`u8 slot`|
|0x7e|SAFE_IDLE|remove HV/power and tri-state translated outputs|
|0x7f|ABORT|same safety action as SAFE_IDLE|

## CONFIG payload

`u16 protocol, u8 power, u8 flags, u32 clock_hz, u32 vtarget_mv, u32 flash_size, u16 page_size, u8 name_len, name[]`.

CONFIG flag bit 0 currently requests backend-specific HV activation for UPDI. Physical JP2 authorization is still required.

Protocol IDs: dsPIC=1, PIC24 raw=2, PIC10/12/16/18 raw=3, AVR ISP=10, UPDI=11, TPI=12, PDI=13, SWIM=20, MSP430 SBW=30, MSP430 JTAG=31, C2=40, RL78=50, SWD=60, JTAG=61, TMS320/C2000 XDS110v3-style JTAG=70.

Power: 0=off, 1=external target power, 2=programmer ~3.3 V, 3=programmer ~5.0 V.

## STATUS payload

Current response is 36 bytes plus the optional device name:

`u16 proto, u8 configured_power, u8 name_len, u32 clock_hz, u32 backend_caps, u32 vtarget_mv, u32 vpp_mv, u32 itarget_ma, u32 power_flags, u32 flash_size, u16 page_size, u16 config_flags, name[]`.

Power flag bit 0 = target power switch reports fault.

## DEBUG_INFO payload (12 bytes)

`u16 proto, u8 state, u8 transport, u32 debug_caps, u16 hw_breakpoints, u8 register_width_bits, u8 reserved`.

Debug states:

- 0 detached
- 1 transport-ready
- 2 native attached
- 3 running
- 4 halted
- 5 reset

Debug transports:

- 0 none
- 1 OpenOCD remote_bitbang
- 2 native family engine

OpenOCD owns actual target run state for SWD/JTAG, so the Pico reports `transport-ready` after DEBUG_ATTACH rather than attempting to mirror OpenOCD's internal target state.

## MEASURE payload (16 bytes)

`u32 vtarget_mv, u32 vpp_mv, u32 itarget_ma, u32 flags`.

## LIST_DEVICES entries

Repeated variable-length entries:

`u8 family, u8 name_len, u16 row_words, u16 page_words, u32 flags, u32 user_end_pc, name[name_len]`.

## Safety policy

CONFIG never enables VPP by itself except for an explicitly requested backend activation sequence after all firmware checks. MCLR-VPP and UPDI-DATA0-HV are separate paths and cannot be active simultaneously. The PCB adds independent physical authorization jumpers JP1 and JP2.

## Rev E STM8 native debug payloads

When protocol `20` (`stm8-swim`) is selected, the generic DJP2 debug namespace maps to the STM8 SWIM/DM engine:

- `DEBUG_ATTACH`: enters SWIM, enables `SWIM_CSR.SWIM_DM`, and configures DM for debug access.
- `DEBUG_HALT`: sets `DM_CSR2.STALL` by WOTF.
- `DEBUG_RUN`: clears `DM_CSR2.STALL` by WOTF.
- `DEBUG_STEP`: sets `DM_CSR1.STE`, clears `STALL`, then waits for the debug module to stall again. The native test model completes this deterministically; hardware code should poll `DM_CSR1.STF`/`DM_CSR2.STALL` with a timeout.
- `DEBUG_RESET`: emits SWIM `SRST`; payload byte 0 nonzero requests halt after reset.
- `DEBUG_REG_READ`: returns the 11-byte CPU register mirror from `0x7F00..0x7F0A`.
- `DEBUG_REG_WRITE`: accepts exactly 11 bytes and writes them back to `0x7F00..0x7F0A`; target must be halted.
- `DEBUG_BP_SET`: payload `[addr32le, type, slot]`; Rev E supports `type=0` instruction-fetch breakpoints, `slot=0..1`.
- `DEBUG_BP_CLEAR`: payload `[slot]`.

The STM8 backend also supports `READ`/`WRITE` over WOTF/ROTF and a structured `RAW_XFER`:

```text
01 addrE addrH addrL lenLE16       -> read len bytes
02 addrE addrH addrL lenLE16 data  -> write len bytes
03                                  -> SWIM SRST
```

The older raw pulse-triplet API is removed from the STM8 backend because it bypassed ACK/NACK handling and could not support debugging reliably.


## Rev E PHY inspection

`DJP2_PHY_INFO` (`0x44`) returns the currently selected STM8 SWIM physical layer.

Payload:

```text
byte 0: active flag, 1 if an external packet PHY is active
byte 1: PHY name length N
byte 2: reserved
byte 3: reserved
byte 4..: UTF-8 PHY name
```

Known names:

- `mock`: deterministic native test target.
- `gpio-soft`: portable software GPIO path.
- `rp2040-pio-txrx`: Rev H Raspberry Pi Pico PIO-assisted transmit and receive timing path.


## Rev H dsPIC debug raw capsule commands

When protocol `1` (`dspic`) is selected, `RAW_XFER` subcommands expose the clean-room dsPIC Debug-Executive control plane:

| Subcommand | Payload | Response |
|---:|---|---|
| `0x80` | `debug_capsule[]` | empty |
| `0x81` | none | `loaded:u8 family:u8 hw_breakpoints:u8 reg_bytes:u8 flags:u32le` |
| `0x82` | `insn24le` | empty; executes one SIX instruction |
| `0x83` | none | `u16le` REGOUT value |

The capsule is metadata only.  It must not contain proprietary Microchip Debug Executive binaries.  Real-target halt/run/step/register/breakpoint commands return `unsupported` unless a capsule declares the corresponding clean-room transaction capability.

Capsule header:

```text
u32 magic = 0x4544444A  # "JDDE" little-endian
u8  version = 1
u8  family
u8  register_bytes
u8  hw_breakpoints
u32 flags
u32 reserved
```

Flag bits:

- bit 0: attach script/capability present
- bit 1: run-control capability present
- bit 2: register access capability present
- bit 3: breakpoint capability present

## Rev H SCRIPT_XFER payload

`DJP2_SCRIPT_XFER` runs one bounded electrical script and returns captured bytes.

| Opcode | Name | Payload |
|---:|---|---|
| `0x00` | END | none |
| `0x01` | DIR | `role:u8 dir:u8`; dir 0=input, 1=output, 2=OD-low, 3=release |
| `0x02` | WRITE | `role:u8 value:u8` |
| `0x03` | READ | `role:u8`; returns one byte |
| `0x04` | DELAY_US | `u32le` |
| `0x05` | POWER | `mode:u8` |
| `0x06` | VPP | `path:u8 on:u8`; path 0=boost, 1=MCLR, 2=DATA0 |
| `0x07` | CLOCK_BITS | `clk:u8 dout:u8 din:u8 bits:u8 flags:u8 hz:u32le txbytes`; returns RX bytes when flag bit 1 is set |
| `0x08` | SPI | `hz:u32le len:u16le txbytes`; returns `len` bytes |
| `0x09` | UART1W | `baud:u32le tx:u16le rx:u16le flags:u8 txbytes`; returns `rx` bytes |
| `0x0A` | SAFE_IDLE | none |

The script VM is not a replacement for reviewed high-level backends.  It is a safe way to prototype and ship exact host-side algorithms for additional niche controller families while reusing the Pico front end, VPP/current telemetry and interlocks.

## Rev J host-only AVR commands

Rev J does not add new DJP2 USB opcodes.  AVR support uses existing opcodes:

- `DJP2_CONFIG` with `protocol=10` (`avr-isp`)
- `DJP2_ENTER` / `DJP2_LEAVE`
- `DJP2_IDENTIFY`
- `DJP2_ERASE`
- `DJP2_READ` / `DJP2_WRITE`
- `DJP2_RAW_XFER` for 4-byte AVR ISP direct instructions such as fuse and lock operations

The host commands `avr-config`, `avr-signature`, `avr-fuses`, and
`program-avr-hex` are convenience wrappers around these stable opcodes.


## Rev K host commands

No DJP2 wire-version bump. Large AVR support is implemented inside the AVR ISP backend using the existing `WRITE`/`READ`/`RAW` command set. UPDI planning is host-side and uses existing `CONFIG` and `RAW_XFER` for bench experiments.


## MSP430 raw TAP envelope - Rev L

Protocol IDs:

- `30` - `msp430-sbw` two-wire Spy-Bi-Wire.
- `31` - `msp430-jtag` four-wire MSP430 JTAG.

Both protocols use `DJP2_RAW_XFER` with this MSP430 operation envelope:

| Opcode | Function | Payload | Response |
|---:|---|---|---|
| `0x00` | TAP reset | `op, cycles` | empty |
| `0x01` | shift IR | `op, bits_le16, txbytes` | captured TDO bytes |
| `0x02` | shift DR | `op, bits_le16, txbytes` | captured TDO bytes |
| `0x03` | clock constant | `op, cycles, tms, tdi` | packed TDO bits |

The response is packed LSB-first by shifted bit index.  These primitives are
non-destructive and are intended for clean-room bench validation before enabling
family-specific erase/write/debug run-control algorithms.

## Rev M TMS320/C2000 raw payloads

When protocol `70` (`tms320`, `c2000`, `xds110v3`) is selected, `RAW_XFER` accepts:

| Op | Name | Payload | Response |
|---:|---|---|---|
| `0x00` | TAP reset | `op,u8 cycles` | empty |
| `0x01` | shift IR | `op,u16 bits,txbytes` | packed TDO bytes |
| `0x02` | shift DR | `op,u16 bits,txbytes` | packed TDO bytes |
| `0x03` | constant clocks | `op,u8 cycles,u8 tms,u8 tdi` | packed TDO bytes |
| `0x04` | line control | `op,u8 nreset_released,u8 ntrst_or_aux_released,u8 emu_hint` | empty |

`DEBUG_BITBANG` also accepts this backend as a JTAG-like OpenOCD remote_bitbang
transport.  High-level C2000 CPU run-control and flash programming are not
implemented in Pico firmware in Rev M.

