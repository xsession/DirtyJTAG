#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -Itests/zephyr_stubs -Iinclude -fsyntax-only src/rpi_pico_hal.c src/main.c tests/zephyr_stubs/zstub.c
