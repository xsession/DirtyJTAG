BOARD ?= dirtyjtag_bluepill/stm32f103xb
BUILD_DIR ?= build
WEST ?= west
CMAKE_EXTRA ?= -DEXTRA_CONF_FILE=legacy.conf

all: zephyr-build

zephyr-build:
	$(WEST) build -p auto -b $(BOARD) -d $(BUILD_DIR) . -- $(CMAKE_EXTRA)

clean:
	$(RM) -r $(BUILD_DIR)

.PHONY: all zephyr-build clean
