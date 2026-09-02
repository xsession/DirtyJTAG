# Rev O - SEGGER-inspired RTT, SystemView and SWO workflow features

Rev O deep-reviewed the practical developer workflows around SEGGER J-Link tools and implemented the clean-room subset that is useful on DirtyJTAG Pico hardware.

## Clean-room findings

High-value workflows to emulate conceptually:

1. **RTT Viewer-style multi-channel terminal work**
   - Channel 0 is normally terminal output.
   - Down-channel 0 is normally keyboard/input.
   - RTT supports multiple up/down channels.
   - RTT channel 0 can multiplex up to 16 virtual terminals.
   - Text control codes / ANSI escape sequences are useful for colors and screen control.

2. **RTT Logger-style binary capture**
   - Up-channel 1 is commonly used for binary logging/profiling data.
   - A host should be able to log a channel continuously to a file without an IDE.

3. **SystemView-style event capture**
   - SystemView streams event data over RTT for continuous recording when background memory access is available.
   - Single-shot/post-mortem style workflows can also be captured by reading a target buffer after the interesting event.
   - DirtyJTAG does not decode proprietary SystemView event semantics; it captures raw RTT channel data for offline tools/analysis.

4. **SWO/ITM terminal capture**
   - J-Link SWO Viewer is a standalone terminal-style viewer for SWO output.
   - DirtyJTAG adds a DJP2 SWO control/read API and host CLI now; production RP2040 PIO signal capture remains a hardware-validation task.

## Implemented in Rev O

### Firmware / protocol

New DJP2 commands:

| Command | ID | Function |
|---|---:|---|
| `DJP2_RTT_CHANNEL_INFO` | `0x64` | Inspect any RTT up/down channel descriptor, including name, buffer, offsets, flags and used/free bytes. |
| `DJP2_SWO_CONFIG` | `0x68` | Configure SWO baud/flags. |
| `DJP2_SWO_START` | `0x69` | Start SWO capture state. |
| `DJP2_SWO_STOP` | `0x6a` | Stop SWO capture state. |
| `DJP2_SWO_READ` | `0x6b` | Read captured SWO bytes. |
| `DJP2_SWO_STATUS` | `0x6c` | Query SWO state, available bytes and dropped count. |

New firmware files:

- `include/djprog/swo.h`
- `src/swo.c`

Updated firmware files:

- `include/djprog/rtt.h`
- `src/rtt.c`
- `include/djprog/usb_proto.h`
- `src/usb_proto.c`
- `CMakeLists.txt`

### Host CLI

New host helper:

- `host/rtt_tools.py`

New commands:

```bash
python host/djprog.py --port COM8 rtt-channels 0x20000000
python host/djprog.py --port COM8 rtt-tail 0x20000000 --channel 0
python host/djprog.py --port COM8 rtt-terminals 0x20000000 --length 2048
python host/djprog.py --port COM8 rtt-terminals 0x20000000 --terminal 2 --strip-ansi
python host/djprog.py --port COM8 rtt-log 0x20000000 --channel 1 -o channel1.bin --seconds 10
python host/djprog.py --port COM8 sysview-capture 0x20000000 --channel 1 -o sysview-rtt.bin --seconds 10
python host/djprog.py --port COM8 swo-config --baud 2000000
python host/djprog.py --port COM8 swo-start
python host/djprog.py --port COM8 swo-status
python host/djprog.py --port COM8 swo-read --length 256
python host/djprog.py --port COM8 swo-tail --seconds 10
python host/djprog.py --port COM8 swo-stop
```

## Compatibility and limitations

- RTT operations require the selected backend to provide background target memory read/write. Native memory backends can use the Pico-side RTT commands directly.
- For ARM SWD/JTAG through OpenOCD remote-bitbang, use OpenOCD RTT commands when possible because OpenOCD owns target memory transactions in that setup.
- The SWO DJP2 API and host workflow are implemented and tested with a mock capture source. Real SWO pin capture still needs RP2040 PIO RX validation and clock tolerance testing before it should be advertised as production hardware capture.
- SystemView capture is intentionally raw. DirtyJTAG does not clone SEGGER SystemView visualization or proprietary decoding.

## Tests

Rev O added tests for:

- RTT channel descriptor inspection and channel names.
- RTT virtual-terminal splitting (`0xff` + `0..9/A..F`).
- ANSI escape stripping for terminal output.
- SWO configure/start/status/read/stop command path using a mock capture feed.
