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

Initialize the Zephyr workspace and build the default STM32F103 "blue pill"
configuration:

```
west init -l .
west update
cd dirty_jtag
make
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

Do not use `import: true` in DirtyJTAG's manifest when you want to reuse an
existing Zephyr checkout. Importing Zephyr's manifest makes west manage Zephyr's
module list here too, which can clone many projects under the parent workspace
such as `modules/lib/picolibc` and `modules/debug/percepio`.

West manifest `path` entries are workspace-relative paths, not shell-expanded
environment variables. If `ZEPHYR_BASE` is
`C:\GIT\WORK\Gabor\paper_dispenser\fw\zephyrproject\zephyr` and the west
workspace root is `C:\GIT`, the matching manifest path is:

```
WORK/Gabor/paper_dispenser/fw/zephyrproject/zephyr
```

Zephyr 4.4 requires Python 3.12 or newer. If west is launched by an older Python
environment, force the desired interpreter through the workspace config:

```powershell
west config build.cmake-args -- "-DPython3_EXECUTABLE=C:/GIT/WORK/Gabor/paper_dispenser/fw/python314/python.exe -DWEST_PYTHON=C:/GIT/WORK/Gabor/paper_dispenser/fw/python314/python.exe"
```

The Makefile also accepts `PYTHON` and defaults to that local Python 3.14
interpreter, so `make` builds use the same interpreter path.

The default `make` target builds `dirtyjtag_bluepill/stm32f103xb`, an in-tree
Zephyr board definition for the common STM32F103C8 Bluepill-style pinout.

Once the build is completed, your freshly compiled firmware will be available in
`dirty_jtag/build/zephyr/` as `zephyr.bin` and `zephyr.elf`.

DirtyJTAG now uses Zephyr devicetree overlays for hardware independence. Build a
different Zephyr-supported board by changing `BOARD`:

```
make BOARD=olimex_stm32_h103/stm32f103xb
```

Use Zephyr's board names exactly. For example, the Raspberry Pi Pico board is
`rpi_pico`, not `rpico`:

```
west build -b rpi_pico
```

For `rpi_pico`, a successful build also creates a UF2 image:

```
dirty_jtag/build/zephyr/zephyr.uf2
```

DirtyJTAG uses Zephyr's `USB_DEVICE_STACK_NEXT` API to avoid the deprecated
legacy USB device stack. If `USB_DEVICE_DRIVER` or `USB_DEVICE_STACK`
deprecation warnings return, check that `dirty_jtag/prj.conf` has not been
changed back to `CONFIG_USB_DEVICE_STACK=y` and that `src/usb.c` is not using
legacy calls such as `usb_enable()`, `usb_read()`, `usb_write()`, or
`USBD_DEFINE_CFG_DATA()`.

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
