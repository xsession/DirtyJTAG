BOARD ?= rpi_pico/rp2040
BUILD_DIR ?= build
PYTHON ?= python3
WEST ?= $(PYTHON) -m west
ZEPHYR_APP ?= dirty_jtag
CMAKE_EXTRA ?=

all: zephyr-build

zephyr-build:
	$(WEST) build -p auto -b $(BOARD) -d $(BUILD_DIR) $(ZEPHYR_APP) -- $(CMAKE_EXTRA)

rpi-pico:
	$(MAKE) zephyr-build BOARD=rpi_pico/rp2040 BUILD_DIR=build-rpi-pico

bluepill:
	$(MAKE) zephyr-build BOARD=dirtyjtag_bluepill/stm32f103xb BUILD_DIR=build-bluepill CMAKE_EXTRA="-DEXTRA_CONF_FILE=legacy.conf"

clean:
	$(RM) -r $(BUILD_DIR) build-rpi-pico build-bluepill

.PHONY: all zephyr-build rpi-pico bluepill clean
