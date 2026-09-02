#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_root"

app=dirty_jtag
test_build=build/native
mkdir -p "$test_build"

cc -std=c11 -Wall -Wextra -Werror -I"$app/include" \
  -include tests/posix_errno_compat.h tests/test_core.c \
  "$app/src/crc32.c" "$app/src/hw.c" "$app/src/safety.c" \
  "$app/src/backends/backend_registry.c" "$app/src/phy_bitbang.c" \
  "$app/src/debug_bitbang.c" "$app/src/debug.c" \
  "$app/src/backends/backend_dspic.c" "$app/src/power_compat.c" \
  "$app/src/device_db.c" "$app/src/dspic_common.c" \
  "$app/src/dspic30.c" "$app/src/dspic33f.c" "$app/src/dspic_debug.c" \
  "$app/src/backends/backend_pic_icsp_raw.c" \
  "$app/src/backends/backend_avr_isp.c" "$app/src/backends/backend_updi.c" \
  "$app/src/backends/backend_tpi.c" "$app/src/backends/backend_pdi.c" \
  "$app/src/swim.c" "$app/src/backends/backend_swim.c" \
  "$app/src/backends/backend_sbw.c" "$app/src/backends/backend_msp430_jtag.c" \
  "$app/src/backends/backend_c2.c" "$app/src/backends/backend_rl78.c" \
  "$app/src/backends/backend_swd_jtag.c" "$app/src/backends/backend_tms320.c" \
  "$app/src/backends/backend_simplelink.c" "$app/src/script_vm.c" \
  "$app/src/bridge.c" "$app/src/power_trace.c" "$app/src/rtt.c" \
  "$app/src/swo.c" "$app/src/usb_proto.c" -o "$test_build/test_core"
"$test_build/test_core"

python3 -m py_compile host/djprog.py host/openocd_bridge.py host/dfp_inspect.py host/hex_utils.py host/avr_utils.py host/updi_utils.py host/msp430_utils.py host/tms320_utils.py host/simplelink_utils.py host/bridge_tools.py host/production_jobs.py host/rtt_tools.py host/vendor_features.py
python3 tests/test_hex_utils.py
python3 tests/test_avr_utils.py
python3 tests/test_updi_utils.py
python3 tests/test_msp430_utils.py
python3 tests/test_tms320_utils.py
python3 tests/test_rtt_tools.py
python3 tests/test_vendor_features.py
python3 tests/test_simplelink_utils.py
python3 tests/test_bridge_tools.py
python3 tests/test_production_jobs.py
