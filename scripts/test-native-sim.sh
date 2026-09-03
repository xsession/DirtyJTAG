#!/usr/bin/env sh
set -eu
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_root"

: "${WEST:=west}"
: "${NATIVE_SIM_BOARD:=native_sim/native/64}"
: "${NATIVE_SIM_BUILD_DIR:=build/native-sim}"

"$WEST" build -p always -b "$NATIVE_SIM_BOARD" tests/native_sim -d "$NATIVE_SIM_BUILD_DIR"
"./$NATIVE_SIM_BUILD_DIR/zephyr/zephyr.exe"

if [ "${NATIVE_SIM_SANITIZERS:-1}" = "1" ]; then
  san_dir="${NATIVE_SIM_BUILD_DIR}-san"
  "$WEST" build -p always -b "$NATIVE_SIM_BOARD" tests/native_sim -d "$san_dir" -- \
    -DEXTRA_CONF_FILE=sanitizers.conf
  "./$san_dir/zephyr/zephyr.exe"
fi
