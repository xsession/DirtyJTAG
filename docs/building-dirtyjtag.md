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
make
```

The default `make` target builds `dirtyjtag_bluepill/stm32f103xb`, an in-tree
Zephyr board definition for the common STM32F103C8 Bluepill-style pinout.

Once the build is completed, your freshly compiled firmware will be available in
`build/zephyr/` as `zephyr.bin` and `zephyr.elf`.

DirtyJTAG now uses Zephyr devicetree overlays for hardware independence. Build a
different Zephyr-supported board by changing `BOARD`:

```
make BOARD=olimex_stm32_h103/stm32f103xb
```

Board-specific JTAG pins live in Zephyr board DTS files or overlays under
`boards/`. To port DirtyJTAG to a new Zephyr board, define `tck-gpios`,
`tdi-gpios`, `tdo-gpios`, `tms-gpios`, and optional `trst-gpios` and
`srst-gpios` in the `zephyr,user` node.

`unicore-mx/` is vendored into this repository as legacy source at the commit
that was previously referenced by the submodule. It is no longer used by the
default firmware build.

## Docker build

If you have [Docker](https://www.docker.com/) (or podman) installed on your computer, you can also choose to build inside a container:

```
docker build https://github.com/dirtyjtag/DirtyJTAG.git --output type=tar,dest=dirtyjtag.tar
```

At the end of the build process, you'll find a `dirtyjtag.tar` archive containing all the build artifacts.
