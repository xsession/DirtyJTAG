#!/usr/bin/env sh
set -eu
python3 -m pip install --user west
if [ ! -d .west ]; then west init -l .; fi
west update
west zephyr-export
west build -b rpi_pico/rp2040 dirty_jtag
