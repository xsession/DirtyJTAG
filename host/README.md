# Host Tools Guide

The `host` directory contains standalone Python tools for the DirtyJTAG DJP2
firmware. Run them from the repository root, for example:

```powershell
python host/djprog.py --help
python host/openocd_bridge.py --help
```

## Start Here

| Module | Responsibility |
|---|---|
| `djprog.py` | Main CLI parser and command dispatch. It retains the family-specific AVR, UPDI, MSP430, TMS320, SimpleLink, bridge, and production-job workflows. |
| `djp2.py` | DJP2 wire constants, frame encoding/decoding, and the USB CDC serial `Link`. Use this for any new host executable that talks to the probe. |
| `djprog_core.py` | CLI-independent safety authorization, configuration/status parsing, chunked read/write, measurements, and dsPIC HEX programming. |
| `djprog_trace.py` | RTT virtual-terminal operations and SWO configuration/capture. |
| `openocd_bridge.py` | TCP `remote_bitbang` server that forwards OpenOCD traffic through DJP2. |

## Controller Support Helpers

| Module | Responsibility |
|---|---|
| `hex_utils.py` | Intel HEX parsing plus dsPIC rows. |
| `avr_utils.py` | AVR profiles and flash-page planning. |
| `updi_utils.py` | UPDI profiles and NVM planning. |
| `msp430_utils.py` | MSP430 image segments and raw TAP envelopes. |
| `tms320_utils.py` | TMS320/C2000 profiles, IDCODE, and TAP payloads. |
| `simplelink_utils.py` | TI SimpleLink profiles and boot plans. |
| `bridge_tools.py` | SPI, I2C, UART, and power-trace payload helpers. |
| `rtt_tools.py` | Segger RTT virtual-terminal parsing and terminal-safe text output. |

## Supporting Tools

| Module | Responsibility |
|---|---|
| `production_jobs.py` | Production job file loading, validation, and templates. |
| `dfp_inspect.py` | Microchip Device Family Pack inspection. |
| `dspic30_bench.py` | dsPIC30 bench and diagnostic utility. |
| `vendor_features.py` | Searchable clean-room vendor-feature research database. |

## Adding a Command

Keep protocol framing in `djp2.py` and reusable controller or workflow logic in
a focused helper module. `djprog.py` should contain only argument definitions,
command dispatch, and small command-specific presentation code. Add or extend a
matching script in `tests` whenever a helper has deterministic behavior.