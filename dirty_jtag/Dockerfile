FROM zephyrprojectrtos/zephyr-build:v0.26.13 AS build-stage
LABEL Description="DirtyJTAG Zephyr firmware"

USER root
WORKDIR /work
COPY . /work/DirtyJTAG

RUN west init -l /work/DirtyJTAG
RUN west update --narrow -o=--depth=1

WORKDIR /work/DirtyJTAG/dirty_jtag
RUN west build -b dirtyjtag_bluepill/stm32f103xb -d build . -- \
    -DEXTRA_CONF_FILE=legacy.conf

FROM scratch AS export-stage
COPY --from=build-stage /work/DirtyJTAG/dirty_jtag/build/zephyr/zephyr.bin /dirtyjtag.bin
COPY --from=build-stage /work/DirtyJTAG/dirty_jtag/build/zephyr/zephyr.elf /dirtyjtag.elf
