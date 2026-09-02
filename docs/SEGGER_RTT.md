# SEGGER RTT support

DirtyJTAG now includes a generic SEGGER RTT helper layer in the DJP2 firmware and host CLI.

RTT is memory based: the debugger searches target RAM for the `SEGGER RTT` control block, reads bytes from the selected up-buffer, and writes bytes to the selected down-buffer by updating the target-side ring-buffer offsets.  Because of this, RTT requires a selected backend that can read and write target memory.

## Supported paths

| Target path | Status |
|---|---|
| STM8 SWIM native backend | Firmware RTT commands can use SWIM ROTF/WOTF memory access. Mock target covered by CI. |
| dsPIC Debug Executive future backend | Control plane ready; RTT-style memory polling requires DE memory read/write completion. |
| ARM SWD/JTAG through OpenOCD remote-bitbang | Use OpenOCD's native RTT support on the host side; the Pico only transports SWD/JTAG bits. |
| Raw JTAG/SWD transports without memory access | Not supported by DJP2 RTT commands until a target memory-access layer exists. |

## USB commands

| Command | ID | Payload | Response |
|---|---:|---|---|
| `DJP2_RTT_SCAN` | `0x60` | `start:u32 end:u32` | `control_block:u32` |
| `DJP2_RTT_INFO` | `0x61` | `control_block:u32` | 15 little-endian `u32` fields |
| `DJP2_RTT_READ` | `0x62` | `control_block:u32 channel:u8 max_len:u16` | raw bytes read from up-buffer |
| `DJP2_RTT_WRITE` | `0x63` | `control_block:u32 channel:u8 bytes...` | `written:u32` |

`DJP2_RTT_INFO` response fields are:

```text
cb, max_up, max_down,
up_name, up_buffer, up_size, up_wr, up_rd, up_flags,
down_name, down_buffer, down_size, down_wr, down_rd, down_flags
```

The firmware assumes the common 32-bit SEGGER RTT control-block layout:

```c
char acID[16];
uint32_t MaxNumUpBuffers;
uint32_t MaxNumDownBuffers;
SEGGER_RTT_BUFFER_UP   aUp[];
SEGGER_RTT_BUFFER_DOWN aDown[];
```

with 24-byte buffer descriptors:

```c
uint32_t sName;
uint32_t pBuffer;
uint32_t SizeOfBuffer;
uint32_t WrOff;
uint32_t RdOff;
uint32_t Flags;
```

## Host usage

Example with a native memory backend:

```bash
python host/djprog.py --port COM8 config swim --device mock-stm8s003f3 --power external --clock 363000
python host/djprog.py --port COM8 enter
python host/djprog.py --port COM8 rtt-scan 0x00000000 0x00010000
python host/djprog.py --port COM8 rtt-info 0x00000100
python host/djprog.py --port COM8 rtt-read 0x00000100 --channel 0 --length 256
python host/djprog.py --port COM8 rtt-write 0x00000100 "help\n" --channel 0
```

For ARM Cortex-M targets debugged through OpenOCD remote-bitbang, use OpenOCD's RTT commands because OpenOCD owns the SWD/JTAG memory transactions:

```tcl
rtt setup <ram_start> <ram_size> "SEGGER RTT"
rtt start
rtt server start 9090 0
```

Then connect to the RTT channel from the host:

```bash
nc localhost 9090
```

## Safety notes

- RTT scanning is bounded by the supplied address range.
- The firmware never writes target memory during scan or info.
- Reading from an up-buffer advances `RdOff` only after the bytes have been copied.
- Writing to a down-buffer respects ring-buffer free space and reports the accepted byte count.
- If the selected backend does not provide target memory read/write, RTT commands return `unsupported`.

## Rev O RTT workflow extensions

Rev O adds host tooling inspired by practical RTT Viewer / RTT Logger workflows:

```bash
python host/djprog.py --port COM8 rtt-channels 0x20000000
python host/djprog.py --port COM8 rtt-tail 0x20000000 --channel 0
python host/djprog.py --port COM8 rtt-terminals 0x20000000 --length 2048
python host/djprog.py --port COM8 rtt-log 0x20000000 --channel 1 -o channel1.bin --seconds 60
python host/djprog.py --port COM8 sysview-capture 0x20000000 --channel 1 -o sysview-rtt.bin --seconds 30
```

`rtt-terminals` is a clean-room host-side parser for Channel-0 virtual terminal selectors. It does not require any proprietary viewer protocol.
