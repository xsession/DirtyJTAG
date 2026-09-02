# Building DirtyJTAG

## Manual build

In order to compile DirtyJTAG, you will need the following software:

 * git
 * Zephyr SDK
 * west
 * make

Clone this repository :

```
git clone https://github.com/dirtyjtag/dirtyjtag
cd dirtyjtag
```

Initialize the Zephyr workspace and build the universal RP2040 firmware:

```
west init -l .
west update
west build -p always -b rpi_pico/rp2040 dirty_jtag
```

If `west init` reports that a parent directory is already initialized, do not
run it again from `dirty_jtag/`. Use the existing workspace and run `west update`
from this repository instead. If `ZEPHYR_BASE` points at an unrelated Zephyr
checkout, unset it for this shell before running west commands.

## West workspace notes

DirtyJTAG's Zephyr application source lives in `dirty_jtag/`, but the west
manifest belongs at the repository root. This keeps existing workspaces that use
`DirtyJTAG/west.yml` as their manifest working after the source move.

West searches upward for `.west/config`. If this repository is under a parent
west workspace, such as `C:\GIT\.west`, commands run from `dirty_jtag/` still use
that parent workspace. In that layout, run `west init` only once for the parent
workspace and use `west update` afterward.

The manifest imports the module set pinned by Zephyr. In an existing west
workspace, make sure its manifest and module revisions are compatible with the
Zephyr revision in this repository before building.

The root `Makefile` is only a convenience wrapper. Its default target builds
`rpi_pico/rp2040` from the canonical `dirty_jtag/` application.

Once the build is completed, your freshly compiled firmware will be available in
`build/zephyr/` as `zephyr.bin`, `zephyr.elf`, and `zephyr.uf2`.

The original DirtyJTAG frontend remains available for STM32F103 probes. Select
it with the provided Kconfig fragment:

```
west build -p always -b dirtyjtag_bluepill/stm32f103xb dirty_jtag -- \
  -DEXTRA_CONF_FILE=legacy.conf
```

The same legacy configuration can be used with the supplied Olimex and minimum
development-board overlays:

```
west build -p always -b olimex_stm32_h103/stm32f103xb dirty_jtag -- \
  -DEXTRA_CONF_FILE=legacy.conf
```

NodeMCU ESP-32S boards are supported as a low-cost legacy JTAG bitbang adapter
over their on-board USB-UART bridge:

```
west build -p always -b nodemcu_esp32s/esp32/procpu dirty_jtag -- \
  -DEXTRA_CONF_FILE=esp32s_nodemcu.conf
```

The NodeMCU mapping intentionally avoids ESP32 boot strapping pins:

| DirtyJTAG signal | ESP32 GPIO |
|---|---:|
| TCK | 18 |
| TDO | 19 |
| TDI | 23 |
| TMS | 21 |
| TRST | 22 |
| SRST | 25 |

The DJP2 CDC frontend currently uses Zephyr's compatibility USB device stack.
The original vendor-class frontend uses `USB_DEVICE_STACK_NEXT`; `legacy.conf`
switches the stack together with the frontend so incompatible APIs are never
linked into one image. ESP32-WROOM NodeMCU boards do not expose a native USB
device controller, so `esp32s_nodemcu.conf` selects the UART packet transport
instead.

Board-specific JTAG pins live in Zephyr board DTS files or overlays under
`dirty_jtag/boards/`. To port DirtyJTAG to a new Zephyr board, define `tck-gpios`,
`tdi-gpios`, `tdo-gpios`, `tms-gpios`, and optional `trst-gpios` and
`srst-gpios` in the `zephyr,user` node.

`unicore-mx/` is vendored into this repository as legacy source at the commit
that was previously referenced by the submodule. It is no longer used by the
default firmware build.

## Docker build

If you have [Docker](https://www.docker.com/) (or podman) installed on your computer, you can also choose to build inside a container:

```
docker build -f dirty_jtag/Dockerfile . --output type=tar,dest=dirtyjtag.tar
```

At the end of the build process, you'll find a `dirtyjtag.tar` archive containing all the build artifacts.
