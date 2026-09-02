# SEGGER-inspired trace workflows on DirtyJTAG

This document maps practical SEGGER-style workflows onto clean-room DirtyJTAG features.

## 1. RTT Viewer-like terminal usage

Scan and inspect:

```bash
python host/djprog.py --port COM8 rtt-scan 0x20000000 0x20020000
python host/djprog.py --port COM8 rtt-channels 0x20000000
```

Tail terminal channel 0:

```bash
python host/djprog.py --port COM8 rtt-tail 0x20000000 --channel 0
```

Split virtual terminals from channel 0:

```bash
python host/djprog.py --port COM8 rtt-terminals 0x20000000 --length 2048
python host/djprog.py --port COM8 rtt-tail 0x20000000 --terminal 1 --strip-ansi
```

Write input to down-channel 0:

```bash
python host/djprog.py --port COM8 rtt-write 0x20000000 "help\n" --channel 0
```

## 2. RTT Logger-like binary logging

Log channel 1 to a binary file:

```bash
python host/djprog.py --port COM8 rtt-log 0x20000000 --channel 1 -o channel1.bin --seconds 60
```

This is useful for application-specific binary records, profiling data, telemetry snapshots and event streams.

## 3. SystemView-style raw event capture

If target firmware writes SystemView/event data into an RTT up-channel, capture it raw:

```bash
python host/djprog.py --port COM8 sysview-capture 0x20000000 --channel 1 -o sysview-rtt.bin --seconds 30
```

DirtyJTAG only captures the transport stream. It does not implement SEGGER SystemView visualization or decode proprietary event semantics.

## 4. SWO/ITM terminal capture

Configure and start the SWO control path:

```bash
python host/djprog.py --port COM8 swo-config --baud 2000000
python host/djprog.py --port COM8 swo-start
python host/djprog.py --port COM8 swo-tail --seconds 10
python host/djprog.py --port COM8 swo-stop
```

Rev O provides the USB/host API and tested firmware ring-buffer path. Production SWO capture needs RP2040 PIO RX hardware validation before being used as a real oscilloscope-free trace receiver.
