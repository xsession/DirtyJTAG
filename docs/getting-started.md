# Getting started

## Firmware

The repository uses West and pins the Zephyr version in `west.yml`.

```sh
python -m pip install west
west init -l .
west update
west zephyr-export
west build -b rpi_pico/rp2040 dirty_jtag
west flash
```

For the legacy NodeMCU ESP-32S frontend:

```sh
west build -b nodemcu_esp32s/esp32/procpu dirty_jtag -- \
  -DEXTRA_CONF_FILE=esp32s_nodemcu.conf
west flash
```

## Host tools

```sh
python -m pip install -r host/requirements.txt
python host/djprog.py --port COM8 hello
python host/djprog.py --port COM8 list
python host/djprog.py --port COM8 status
```

Replace `COM8` with the serial port assigned to the probe.

## Tests and formatting

```sh
./scripts/test-native.sh
sh scripts/install-git-hooks.sh
```

The native suite uses `-Wall -Wextra -Werror`. The Git hook formats staged C
and C++ files with the repository `.clang-format` configuration.