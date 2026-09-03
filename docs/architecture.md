# Architecture

The repository has one canonical Zephyr application: `dirty_jtag/`. Everything
else either supports that app, documents it, validates it, or provides host-side
tools and target profiles.

## Repository Layout

```text
.
|-- dirty_jtag/        Zephyr application, board definitions and firmware code
|-- host/              Python host tools, profiles and OpenOCD bridge configs
|-- tests/             Host tests, Zephyr stubs and native_sim wrapper
|-- scripts/           Build, validation and simulator helper scripts
|-- hardware/          Front-end schematics, connector maps and BOM material
|-- docs/              User, design, validation and release documentation
|-- contrib/           Optional helper scripts and archived patch artifacts
|-- .github/workflows/ Active CI definitions
|-- Makefile           Convenience wrapper for common west builds
`-- west.yml           Workspace manifest pinned to the reviewed Zephyr release
```

Root files are kept to project entry points: `README.md`, `LICENSE`,
`REVISION.md`, `west.yml`, `.gitignore`, and the convenience `Makefile`.
Generated build directories, release artifacts and Python caches are ignored.

## Firmware App

`dirty_jtag/` owns the only firmware application:

```text
dirty_jtag/
|-- CMakeLists.txt     Selects frontend source sets
|-- Kconfig            Frontend and board-independent firmware options
|-- prj.conf           Universal RP2040 defaults
|-- legacy.conf        Legacy USB bulk transport defaults
|-- esp32s_nodemcu.conf Legacy UART transport defaults for ESP32-WROOM boards
|-- boards/            In-tree Zephyr board DTS files and app overlays
|-- dts/bindings/      App-specific devicetree bindings
|-- include/           Firmware interfaces shared across modules
`-- src/               Firmware implementation
```

Backends live only in `dirty_jtag/src/backends/`. Protocol engines and shared
services such as `swim.c`, `rtt.c`, `power_trace.c`, `safety.c`, `script_vm.c`
and `phy_bitbang.c` remain directly under `src/` because they are not board
ports and are shared by several backends.

## Universal Frontend

The RP2040 build selects `CONFIG_DIRTYJTAG_UNIVERSAL_FRONTEND` and is split into
four layers:

1. `src/main.c` owns the Zephyr USB CDC event loop.
2. `src/usb_proto.c` validates DJP2 frames and dispatches commands.
3. `src/backends/` and protocol modules implement target behavior against
   `include/djprog/` and `include/djpk4/`.
4. `src/rpi_pico_hal.c` adapts the protected Pico front-end to Zephyr GPIO, ADC
   and PIO drivers. The full Pico pin map is declared in
   `boards/rpi_pico_rp2040.overlay` and validated by
   `dts/bindings/dirtyjtag,pico-front-end.yaml`.

The native suite binds mock `dj_hw_ops` to the same backend and command code.
This keeps protocol behavior testable without copying it into host utilities or
Zephyr-specific modules.

## Legacy Frontend

`CONFIG_DIRTYJTAG_LEGACY_FRONTEND` selects the original DirtyJTAG command and
JTAG engine. It is retained for boards whose hardware does not provide the
protected universal programmer front-end.

Legacy transport is selected by Kconfig:

- `CONFIG_DIRTYJTAG_LEGACY_USB_TRANSPORT` uses the original vendor USB bulk
  endpoints for STM32F103-style probes.
- `CONFIG_DIRTYJTAG_LEGACY_UART_TRANSPORT` uses the same command packets over
  UART for classic ESP32-WROOM NodeMCU boards whose USB connector is a USB-UART
  bridge rather than a native USB device controller.

The frontends are mutually exclusive Kconfig choices assembled by
`dirty_jtag/CMakeLists.txt`; there is no second application at repository root.

## Host And Tests

Host tools in `host/` own command-line workflows, target profiles, OpenOCD
remote-bitbang bridges and JSON profile data. Firmware code does not import
host modules.

`tests/` has three roles:

- Python tests for host utilities.
- `tests/test_core.c`, which compiles the firmware core against a mock HAL.
- `tests/native_sim/`, a Zephyr native_sim wrapper for integration-style checks.

`tests/zephyr_stubs/` is deliberately small and exists only for syntax checks of
Zephyr-facing files when a full Zephyr SDK is not available.

## Portability

Board support belongs under `dirty_jtag/boards/`. New hardware support should
describe devices and pins through devicetree, use Zephyr driver APIs in a board
adapter, and keep target algorithms behind the existing `dj_hw_ops` and backend
contracts.
