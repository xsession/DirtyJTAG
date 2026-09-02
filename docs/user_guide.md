west build -p always -b rpi_pico/rp2040 -d build-rp2040 dirty_jtag -- -DDTC_OVERLAY_FILE=dirty_jtag/boards/rpi_pico_rp2040.overlay

west build -p always -b nodemcu_esp32s/esp32/procpu -d build-nodemcu-esp32s dirty_jtag -- -DEXTRA_CONF_FILE=esp32s_nodemcu.conf
