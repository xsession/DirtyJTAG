BOARD ?= dirtyjtag_bluepill/stm32f103xb
BUILD_DIR ?= build

all: zephyr-build

zephyr-build:
	west build -b $(BOARD) -d $(BUILD_DIR) .

clean:
	$(RM) -r $(BUILD_DIR)

.PHONY: all zephyr-build clean
