# DirtyJTAG

DirtyJTAG is a Zephyr-based universal programmer and debugger for protected
Raspberry Pi Pico front-end hardware. The firmware exposes one USB CDC
interface for target power, signal routing, programming protocols, and debug
transports.

## Start here

| Goal | Read |
| --- | --- |
| Build the firmware | [Getting started](getting-started.md) |
| Operate a probe | [User guide](user_guide.md) |
| Connect GDB or OpenOCD | [Debugging](DEBUGGING.md) |
| Check what is implemented | [Support matrix](SUPPORT_MATRIX.md) |
| Understand the firmware | [Architecture](architecture.md) |
| Verify a hardware revision | [Validation matrix](VALIDATION_MATRIX.md) |

!!! warning
    A protocol transport is not automatically a production-qualified
    programmer or debugger. Check the validation matrix and hardware safety
    documentation before connecting a target.

## Project boundaries

The project separates three responsibilities:

1. **Electrical transport:** protected pins, reset, target power, VPP, and measurement.
2. **Programming algorithms:** device operations where a public specification is implemented.
3. **Debug run-control:** halt, run, step, registers, and breakpoints, either natively or through OpenOCD.