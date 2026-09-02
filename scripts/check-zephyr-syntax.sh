#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_root"

cc -std=c11 -Wall -Wextra -Werror \
  -Itests/zephyr_stubs -Idirty_jtag/include \
  -fsyntax-only \
  dirty_jtag/src/rpi_pico_hal.c \
  dirty_jtag/src/swim_rp2040_pio.c \
  dirty_jtag/src/main.c \
  tests/zephyr_stubs/zstub.c
