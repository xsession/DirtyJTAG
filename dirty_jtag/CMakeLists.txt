cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dirtyjtag_universal_programmer_debugger)
target_include_directories(app PRIVATE include)
target_sources(app PRIVATE
 src/main.c src/rpi_pico_hal.c src/hw.c src/crc32.c src/script_vm.c src/usb_proto.c src/backend_registry.c src/phy_bitbang.c src/debug_bitbang.c src/debug.c
 src/backend_dspic.c src/power_compat.c src/device_db.c src/dspic_common.c src/dspic30.c src/dspic33f.c src/dspic_debug.c
 src/backend_pic_icsp_raw.c src/backend_avr_isp.c src/backend_updi.c src/backend_tpi.c src/backend_pdi.c
 src/swim.c src/swim_rp2040_pio.c src/backend_swim.c src/backend_sbw.c src/backend_msp430_jtag.c src/backend_c2.c src/backend_rl78.c src/backend_swd_jtag.c src/backend_tms320.c)
