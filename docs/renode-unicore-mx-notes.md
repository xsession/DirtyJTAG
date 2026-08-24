# Renode Notes for unicore-mx

## Scope

DirtyJTAG now builds as a Zephyr application, while `unicore-mx/` is kept in
tree as legacy reference source. Renode is still useful for this repository
because it can exercise the same STM32F103-class startup path that the old
unicore-mx Blue Pill port targeted, without needing physical hardware for every
basic regression check.

These notes were checked against:

* DirtyJTAG `dirty_jtag/build/zephyr/zephyr.elf`
* installed Renode `1.15.3.22387`
* `https://github.com/xsession/renode.git`, branch `custom-cores`, commit
  `6eb3f1f97a36f6f0e265455b6291ae1ee958cc68`

## What Works Today

The current firmware artifact can be loaded by Renode on an STM32F103 CPU
platform and execution can be started. That makes Renode useful as a low-cost
smoke test for:

* ELF validity and Cortex-M vector-table startup.
* Early reset/boot flow regressions.
* Whether the firmware reaches the STM32 clock and peripheral setup code.
* Comparing Zephyr-generated STM32F1 behavior with assumptions from the old
  unicore-mx/libopencm3-style startup model.

The stock CPU-level test command used was:

```powershell
Renode --disable-gui --console --plain -e 'using sysbus; mach create; machine LoadPlatformDescription @platforms/cpus/stm32f103.repl; sysbus LoadELF @C:/GIT/DirtyJTAG/dirty_jtag/build/zephyr/zephyr.elf; start; emulation RunFor "0.1"; quit'
```

This loaded the ELF and started emulation, but produced repeated warnings around
unimplemented STM32F103 RCC/FLASH registers. That means the stock CPU platform
is useful for "does it load and begin executing?" checks, not for confirming
that the full board boot path is correct.

## custom-cores Branch Result

The DirtyJTAG Renode script uses:

```renode
machine LoadPlatformDescription @platforms/boards/st/blue_pill_stm32f103.repl
```

Stock Renode `1.15.3.22387` does not include that board file, so the script
fails immediately with:

```text
Could not find file 'platforms/boards/st/blue_pill_stm32f103.repl'.
```

The `xsession/renode` `custom-cores` branch does include this board file. It is
a thin Blue Pill wrapper around `platforms/cpus/st/stm32f103.repl` and adds the
active-low PC13 LED:

```renode
using "platforms/cpus/st/stm32f103.repl"

gpioPortC:
    13 -> led@0

led: Miscellaneous.LED @ gpioPortC 13
    invert: true
```

However, using the installed Renode binary with the checked-out `custom-cores`
platform files currently fails while parsing the custom branch CPU platform:

```text
Error E00: Syntax error, unexpected '['
At platforms/cpus/st/stm32f103.repl:65:
    invertedAFPins: [[7, 1]]
```

So the branch has the missing model files, but the matching Renode runtime needs
to be built or installed from that branch. Mixing the branch's newer platform
catalog with the older installed `1.15.3` parser is not compatible.

## RP2040 / Raspberry Pi Pico Result

This repository does not currently contain an in-tree DirtyJTAG RP2040 Zephyr
board target. The README points Raspberry Pi Pico users to the external
`pico-dirtyJtag` project, so there is no local `west build` target here that
produces a DirtyJTAG RP2040 ELF.

The `xsession/renode` `custom-cores` branch does include Raspberry Pi Pico
platform descriptions:

* `platforms/boards/raspberrypi/raspberry_pi_pico.repl`
* `platforms/boards/raspberrypi/raspberry_pi_pico_w.repl`
* `platforms/cpus/raspberrypi/rp2040.repl`

The Pico board file is a thin wrapper around the RP2040 CPU model and connects
the normal Pico LED on GPIO25:

```renode
using "platforms/cpus/raspberrypi/rp2040.repl"

gpio0:
    25 -> led@0

led: Miscellaneous.LED @ gpio0 25
```

The RP2040 CPU platform describes Cortex-M0+, ROM, XIP flash, SRAM, UART, SPI,
I2C, ADC, PWM, timer, GPIO, watchdog, DMA, PIO, and SSI/QSPI blocks. Loading it
with installed Renode `1.15.3.22387` fails at the first RP2040-specific
peripheral:

```text
Error E04: Could not resolve type: 'UART.RP2040_UART'.
At platforms/cpus/raspberrypi/rp2040.repl:34:
uart0: UART.RP2040_UART @ sysbus <0x40034000, +0x1000>
```

The branch contains RP2040 peripheral implementation files under
`_backup_peripherals/`, including `RP2040_UART.cs`, `RP2040_GPIO.cs`,
`RP2040_PIO.cs`, `RP2040_SSI.cs`, `RP2040_Timer.cs`, and others. They are not in
the active `src` tree in this checkout, so the checked-out platform catalog is
not enough by itself. A runnable RP2040 test needs those peripherals integrated
into a Renode build, or the platform description rewritten to use existing
Renode peripheral models.

For DirtyJTAG, RP2040 Renode support is still useful, but it is one step further
away than STM32F1 support:

* It can eventually cover the external Pico DirtyJTAG port's boot and GPIO/JTAG
  pin setup.
* It is not directly useful for this Zephyr app until an RP2040 board target is
  added locally or an external Pico DirtyJTAG ELF is supplied.
* It cannot validate USB DirtyJTAG behavior yet unless the RP2040 USB device
  controller is modeled or replaced with a test transport.
* PIO is relevant for high-speed RP2040 JTAG implementations, but the custom
  branch's PIO model appears to be a basic stub, so it should be treated as a
  smoke-test helper rather than a timing-accurate validator.

## Why This Helps unicore-mx

Renode can be a practical replacement for some historical unicore-mx bring-up
checks:

* The Blue Pill target is STM32F103C8-compatible, matching the old
  `unicore-mx/lib/stm32/f1` family.
* The current Zephyr board DTS uses the same class of memory map, GPIO ports,
  RCC setup, and Cortex-M3 vector startup that unicore-mx handled directly.
* JTAG pin setup is now expressed in Zephyr devicetree:
  `tck=PA5`, `tdo=PA6`, `tdi=PA7`, `tms=PA3`, `trst=PA4`, `srst=PA2`.
  Renode GPIO hooks can eventually observe these pins without keeping
  unicore-mx register writes in the application.
* The custom branch's STM32F1 GPIO alternate-function modeling is relevant for
  testing old assumptions about pin remapping and GPIO behavior from unicore-mx.

That makes Renode good for migration confidence: it can catch startup, linker,
devicetree, and GPIO initialization regressions while the project moves away
from direct unicore-mx dependencies.

## Current Limits

Renode should not yet be treated as a complete DirtyJTAG validator:

* USB enumeration and bulk endpoint behavior are the important user-visible
  paths, and they still need host-side testing with real hardware or a more
  complete USB model.
* JTAG correctness requires either a modeled target TAP or explicit GPIO-level
  assertions; the current smoke script only runs time forward.
* Stock Renode is missing the Blue Pill board wrapper used by
  `scripts/renode/dirtyjtag_bluepill_smoke.resc`.
* The checked-out `custom-cores` platform files require a compatible Renode
  runtime, not the installed `1.15.3` binary.
* RP2040/Pico platform files in `custom-cores` reference custom peripheral
  classes that are not available in the installed Renode runtime.

## Recommended Next Steps

1. Build or install Renode from `xsession/renode` branch `custom-cores`.
2. Run the existing script from the custom Renode repository root, or configure
   Renode so `@platforms/boards/st/blue_pill_stm32f103.repl` resolves to that
   branch's platform catalog.
3. Extend `scripts/renode/dirtyjtag_bluepill_smoke.resc` with a real pass/fail
   condition after the board boots. Useful first checks would be:
   * CPU reaches `main`.
   * PA2-PA7 GPIOs are configured as expected.
   * JTAG command handlers can be reached in a test build with USB replaced by
     a Renode-friendly transport.
4. Keep `unicore-mx/` as a reference for STM32F1 register-level expectations,
   but add new validation against Zephyr DTS and drivers rather than reviving
   direct unicore-mx dependencies.
5. For RP2040, decide whether DirtyJTAG should grow a local Zephyr
   `rpi_pico/rp2040` target or whether Renode testing should track the external
   `pico-dirtyJtag` firmware separately.
