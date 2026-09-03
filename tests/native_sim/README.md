# DirtyJTAG native_sim validation

This Zephyr test application links the universal DJP2 core against the mock HAL
already used by `tests/test_core.c`. It deliberately excludes RP2040 GPIO, ADC,
USB CDC and PIO drivers: those require board/HIL validation.

Build and run with the repository-pinned Zephyr version:

```sh
./scripts/test-native-sim.sh
```

Equivalent manual command:

```sh
west build -p always -b native_sim/native/64 tests/native_sim -d build/native-sim
./build/native-sim/zephyr/zephyr.exe
```

The script also builds an ASan+UBSan native_sim variant by default. Disable it
with `NATIVE_SIM_SANITIZERS=0` if the host lacks sanitizer runtimes.

For hosts with 32-bit multilib support, also run the ILP32 target because its
pointer/long sizes better resemble embedded MCUs:

```sh
NATIVE_SIM_BOARD=native_sim NATIVE_SIM_BUILD_DIR=build/native-sim-32 \
  ./scripts/test-native-sim.sh
```

The suite covers DJP2 framing/dispatch, backend registration and selection,
safety arming, dsPIC mock debug, SWD/JTAG bitbang, MSP430 SBW/JTAG,
TMS320/C2000 JTAG, STM8 native debug, RTT, SWO, generic bridge operations,
power trace, and TI SimpleLink SWD/cJTAG.
