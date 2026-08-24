# Architecture

DirtyJTAG is organized as a Zephyr application with the protocol logic kept
separate from hardware access.

## Active Firmware Path

The active build is rooted at `dirty_jtag/CMakeLists.txt` and uses these layers:

 * `dirty_jtag/src/cmd.*`: DirtyJTAG USB command protocol.
 * `dirty_jtag/src/jtag.*`: JTAG signal operations using Zephyr GPIO APIs and devicetree
   pin descriptions.
 * `dirty_jtag/src/usb.*`: vendor-specific bulk USB transport using Zephyr's USB device
   stack.
 * `dirty_jtag/src/delay.*`: timing helpers using Zephyr kernel timing.

Board portability comes from Zephyr board definitions and overlays in `dirty_jtag/boards/`.
The default `dirtyjtag_bluepill/stm32f103xb` board provides the JTAG pins
through the `zephyr,user` node, while Zephyr provides clocks, startup, linker
scripts, USB controller drivers, and GPIO drivers.

## Vendored Legacy Source

`unicore-mx/` is now vendored source, not a Git submodule. It is pinned to the
old submodule commit:

```
c80139a036466c3eab4ab8eca48f26b3a92333c1
```

DirtyJTAG no longer builds against this library. Keep it in-tree for historical
reference, comparison while migrating board support, and extracting behavior
that has not yet been represented with Zephyr APIs.

New hardware support should be added with Zephyr board support or application
overlays instead of adding new direct `unicore-mx` register dependencies.
