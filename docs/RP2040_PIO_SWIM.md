# Rev H RP2040 PIO-assisted STM8 SWIM PHY

STM8 SWIM has narrow timing windows, especially after high-speed mode is enabled.  Rev H keeps the portable GPIO/mock implementation for CI, but the Raspberry Pi Pico build binds a pluggable RP2040 PIO PHY when `CONFIG_DJPROG_SWIM_RP2040_PIO=y`.

## Architecture

```text
STM8 debugger / UM0470 command layer
              |
       dj_swim_phy_ops
              |
  +-----------+-------------+
  |                         |
mock/gpio-soft       rp2040-pio-txrx
                            |
               PIO TX state machine
               PIO RX state machine
                            |
                       DATA0/SWIM
```

The upper layer still owns:

- SRST / ROTF / WOTF command construction;
- parity;
- ACK/NACK interpretation;
- STM8 debug-module register access;
- halt/run/step/register/breakpoint semantics.

The PIO PHY owns packet-level timing only.

## Firmware files

- `include/djprog/swim_phy.h`
- `src/swim.c`
- `src/swim_rp2040_pio.c`
- `src/backend_swim.c`

## USB inspection

```sh
python host/djprog.py --port COM8 config swim --device stm8s003f3 --power 3v3 --clock 363000
python host/djprog.py --port COM8 phy-info
```

Expected Pico path:

```text
swim_phy=rp2040-pio-txrx active=True
```

Expected native/mock path:

```text
swim_phy=mock active=False
```

## Validation checklist

Before high-speed field use, validate with a logic analyzer:

1. low-speed entry pulse and synchronization;
2. WOTF write to `SWIM_CSR`;
3. switch to high speed;
4. ACK timing after command/data packets;
5. ROTF read of `0x7F00..0x7F0A` CPU register mirror;
6. halt/run/step using `DM_CSR1/DM_CSR2`;
7. breakpoint slot programming at `0x7F90..0x7F95`;
8. repeat at external VTARGET, sourced 3.3 V and sourced 5 V.

Store captures under `tests/logic/` once available.

## Build switches

```text
CONFIG_DJPROG_SWIM_RP2040_PIO=y
CONFIG_DJPROG_SWIM_PIO_DEFAULT_HZ=363000
```

If the PIO PHY cannot bind, the SWIM code falls back to the portable software path.  That fallback is useful for bring-up but should not be used to claim robust high-speed SWIM.
