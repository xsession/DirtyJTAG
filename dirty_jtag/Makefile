BOARD ?= dirtyjtag_bluepill/stm32f103xb
BUILD_DIR ?= build
PYTHON ?= C:/GIT/WORK/Gabor/paper_dispenser/fw/python314/python.exe
WEST ?= $(PYTHON) -m west

all: zephyr-build

zephyr-build:
	$(WEST) build -b $(BOARD) -d $(BUILD_DIR) . -- -DPython3_EXECUTABLE=$(PYTHON) -DWEST_PYTHON=$(PYTHON)

clean:
	$(RM) -r $(BUILD_DIR)

.PHONY: all zephyr-build clean
