# DirtyJTAG Operator Guide

This guide is for normal bench and controlled production use of the RP2040 universal frontend.

## 1. Before connecting a target

1. Confirm the target MCU and programming/debug interface.
2. Confirm target I/O voltage from the target schematic or datasheet.
3. Confirm whether the target is externally powered or should be powered by DirtyJTAG.
4. Remove/disable VPP authorization jumpers unless the selected protocol explicitly requires high voltage.
5. Inspect the target connector orientation and ground connection.
6. Start with DirtyJTAG in safe state.

Never use the universal front end on patient-connected medical equipment without approved isolation and system-level safety controls.

## 2. Host setup

```sh
python -m pip install -r host/requirements.txt
python host/djprog.py --port COM8 hello
python host/djprog.py --port COM8 status
python host/djprog.py --port COM8 measure
```

Replace `COM8` with the actual CDC ACM device.

## 3. Mandatory startup sequence

```sh
python host/djprog.py --port COM8 safe
python host/djprog.py --port COM8 measure
```

Expected initial condition:

- no VPP applied;
- target supply off or explicitly external;
- no target-power fault;
- signal pins not actively driving unless a selected protocol has entered a session.

## 4. Target power

Prefer external target power during initial bring-up.

Example internally powered 3.3 V target:

```sh
python host/djprog.py --port COM8 --safety-confirm power 3v3
python host/djprog.py --port COM8 measure
```

Check measured voltage before continuing.

For 5 V operation, confirm the selected MCU/interface is 5 V compatible. Several modern SWD/JTAG/SimpleLink interfaces are not.

## 5. Selecting a protocol

Examples:

```sh
python host/djprog.py --port COM8 config swd --power external --clock 1000000
python host/djprog.py --port COM8 avr-config atmega328p --power 5v --clock 125000
python host/djprog.py --port COM8 simplelink-config cc2652r --interface swd --power external
```

Always re-check `status` after configuration.

## 6. Destructive operations

Erase, write/program, target-power switching, VPP, bridge stimulation and script execution require explicit safety confirmation.

Example:

```sh
python host/djprog.py --port COM8 --safety-confirm program-avr-hex firmware.hex \
  --profile atmega328p --erase --verify
```

Never bypass verify in production unless the approved process documents why verification is impossible or intentionally omitted.

## 7. High-voltage programming

High voltage is a controlled engineering function.

Before VPP:

1. confirm exact target family;
2. confirm VPP requirement and allowed range;
3. install the correct physical authorization jumper;
4. confirm measured VPP with no target connected during first hardware setup;
5. keep unrelated signal drivers released.

Manual VPP commands are for bench diagnosis, not routine production jobs.

## 8. Debugging

For ARM SWD/JTAG use the OpenOCD bridge unless a native backend is explicitly documented.

Terminal 1:

```sh
python host/openocd_bridge.py --port COM8 --transport swd --power external
```

Terminal 2:

```sh
openocd -f host/openocd/dirtyjtag-remote-swd.cfg -f target/<target>.cfg
```

Use the exact OpenOCD target configuration for the MCU.

## 9. RTT, SWO and trace

Use RTT only after the target memory range/control block is known. For OpenOCD-owned SWD/JTAG sessions, prefer OpenOCD's target-memory-aware RTT support.

SWO support must be treated according to `docs/VALIDATION_MATRIX.md`; do not assume high-speed capture is production-qualified without timing evidence.

## 10. Bridge mode

SPI/I2C/UART/GPIO bridge commands electrically stimulate the target and are therefore privileged operations. Confirm voltage, pin assignment and bus ownership before use.

Do not connect the bridge to a bus simultaneously driven by incompatible external masters.

## 11. Shutdown

At the end of every session:

```sh
python host/djprog.py --port COM8 safe
python host/djprog.py --port COM8 measure
```

Then remove DirtyJTAG-provided target power and disconnect the target.

## 12. Stop conditions

Stop immediately if any of the following occurs:

- unexpected target voltage;
- target-power fault indication;
- excessive current;
- VPP outside its approved window;
- target becomes hot;
- USB disconnect/reconnect during a destructive operation;
- device identification differs from the approved production job;
- verify failure.

Return to safe state and investigate before retrying.
