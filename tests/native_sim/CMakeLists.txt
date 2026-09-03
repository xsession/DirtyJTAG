cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dirtyjtag_native_sim_validation LANGUAGES C)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(DJ_APP "${REPO_ROOT}/dirty_jtag")

# Reuse the exact host core scenario set. Rename its main() for the Ztest
# wrapper so native_sim exercises the same assertions as scripts/test-native.sh.
set(DJ_HOST_SCENARIOS "${REPO_ROOT}/tests/test_core.c")
set_source_files_properties(${DJ_HOST_SCENARIOS}
    PROPERTIES COMPILE_DEFINITIONS "main=dj_core_test_run")

target_include_directories(app PRIVATE "${DJ_APP}/include")
target_sources(app PRIVATE
    src/main.c
    ${DJ_HOST_SCENARIOS}

    ${DJ_APP}/src/crc32.c
    ${DJ_APP}/src/hw.c
    ${DJ_APP}/src/safety.c
    ${DJ_APP}/src/backends/backend_registry.c
    ${DJ_APP}/src/phy_bitbang.c
    ${DJ_APP}/src/debug_bitbang.c
    ${DJ_APP}/src/debug.c
    ${DJ_APP}/src/backends/backend_dspic.c
    ${DJ_APP}/src/power_compat.c
    ${DJ_APP}/src/device_db.c
    ${DJ_APP}/src/dspic_common.c
    ${DJ_APP}/src/dspic30.c
    ${DJ_APP}/src/dspic33f.c
    ${DJ_APP}/src/dspic_debug.c
    ${DJ_APP}/src/backends/backend_pic_icsp_raw.c
    ${DJ_APP}/src/backends/backend_avr_isp.c
    ${DJ_APP}/src/backends/backend_updi.c
    ${DJ_APP}/src/backends/backend_tpi.c
    ${DJ_APP}/src/backends/backend_pdi.c
    ${DJ_APP}/src/swim.c
    ${DJ_APP}/src/backends/backend_swim.c
    ${DJ_APP}/src/backends/backend_sbw.c
    ${DJ_APP}/src/backends/backend_msp430_jtag.c
    ${DJ_APP}/src/backends/backend_c2.c
    ${DJ_APP}/src/backends/backend_rl78.c
    ${DJ_APP}/src/backends/backend_swd_jtag.c
    ${DJ_APP}/src/backends/backend_tms320.c
    ${DJ_APP}/src/backends/backend_simplelink.c
    ${DJ_APP}/src/script_vm.c
    ${DJ_APP}/src/bridge.c
    ${DJ_APP}/src/power_trace.c
    ${DJ_APP}/src/rtt.c
    ${DJ_APP}/src/swo.c
    ${DJ_APP}/src/usb_proto.c
)
