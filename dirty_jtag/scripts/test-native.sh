#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_core.c \
  src/crc32.c src/hw.c src/backend_registry.c src/phy_bitbang.c src/debug_bitbang.c src/debug.c \
  src/backend_dspic.c src/power_compat.c src/device_db.c src/dspic_common.c \
  src/dspic30.c src/dspic33f.c src/dspic_debug.c src/backend_pic_icsp_raw.c src/backend_avr_isp.c \
  src/backend_updi.c src/backend_tpi.c src/backend_pdi.c src/swim.c src/backend_swim.c \
  src/backend_sbw.c src/backend_msp430_jtag.c src/backend_c2.c src/backend_rl78.c src/backend_swd_jtag.c \
  src/backend_tms320.c src/script_vm.c src/usb_proto.c -o tests/test_core
./tests/test_core
python3 -m py_compile host/djprog.py host/openocd_bridge.py host/dfp_inspect.py host/hex_utils.py host/avr_utils.py host/updi_utils.py host/msp430_utils.py host/tms320_utils.py
python3 tests/test_hex_utils.py
python3 tests/test_avr_utils.py

python3 tests/test_updi_utils.py
python3 tests/test_msp430_utils.py

python3 tests/test_tms320_utils.py
