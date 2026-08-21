cmake_minimum_required(VERSION 3.20.0)

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dirtyjtag)

target_sources(app PRIVATE
  src/cmd.c
  src/delay.c
  src/dirtyjtag.c
  src/jtag.c
  src/usb.c
)
