cmake_minimum_required(VERSION 3.20.0)

list(APPEND BOARD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dirtyjtag LANGUAGES C)

target_include_directories(app PRIVATE include)

if(CONFIG_DIRTYJTAG_UNIVERSAL_FRONTEND)
  target_sources(app PRIVATE
    src/main.c
    src/rpi_pico_hal.c
    src/swim_rp2040_pio.c

    src/bridge.c
    src/crc32.c
    src/debug.c
    src/debug_bitbang.c
    src/device_db.c
    src/dspic30.c
    src/dspic33f.c
    src/dspic_common.c
    src/dspic_debug.c
    src/hw.c
    src/phy_bitbang.c
    src/power_compat.c
    src/power_trace.c
    src/rtt.c
    src/safety.c
    src/script_vm.c
    src/swim.c
    src/swo.c
    src/usb_proto.c

    src/backends/backend_avr_isp.c
    src/backends/backend_c2.c
    src/backends/backend_dspic.c
    src/backends/backend_msp430_jtag.c
    src/backends/backend_pdi.c
    src/backends/backend_pic_icsp_raw.c
    src/backends/backend_registry.c
    src/backends/backend_rl78.c
    src/backends/backend_sbw.c
    src/backends/backend_simplelink.c
    src/backends/backend_swd_jtag.c
    src/backends/backend_swim.c
    src/backends/backend_tms320.c
    src/backends/backend_tpi.c
    src/backends/backend_updi.c
  )
else()
  target_sources(app PRIVATE
    src/cmd.c
    src/delay.c
    src/dirtyjtag.c
    src/jtag.c
    src/usb.c
  )
endif()
