# Architecture

The repository contains one Zephyr application, rooted at `dirty_jtag/`.
Firmware policy and protocol behavior stay independent of Zephyr drivers so the
core can also be compiled and tested on a host.

## Universal firmware

The RP2040 build selects `CONFIG_DIRTYJTAG_UNIVERSAL_FRONTEND` and is split into
four layers:

1. `src/main.c` owns the Zephyr USB CDC event loop.
2. `src/usb_proto.c` validates DJP2 frames and dispatches commands.
3. `src/backends/` and the protocol modules implement target behavior against
   the interfaces in `include/djprog/` and `include/djpk4/`.
4. `src/rpi_pico_hal.c` is the Zephyr hardware adapter for the protected Pico
   frontend. The board overlay enables the required USB and ADC devices.

The native suite binds mock `dj_hw_ops` to the same backend and command code.
This keeps protocol behavior testable without copying it into host utilities or
Zephyr-specific modules.

## Legacy frontend

`CONFIG_DIRTYJTAG_LEGACY_FRONTEND` selects the original DirtyJTAG USB command,
JTAG, delay, and USB modules. It is retained for STM32F103-based probes whose
hardware does not provide the protected universal frontend. `legacy.conf`
selects this mode explicitly.

The two frontends are mutually exclusive Kconfig choices and are assembled by
one `dirty_jtag/CMakeLists.txt`; there is no second application at repository
root.

## Portability

Board definitions and application overlays live under `dirty_jtag/boards/`.
New hardware support should describe devices and pins through devicetree, use
Zephyr driver APIs in a hardware adapter, and keep target algorithms behind the
existing `dj_hw_ops` and backend contracts.
