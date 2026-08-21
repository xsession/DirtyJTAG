FROM zephyrprojectrtos/zephyr-build:v0.26.13 AS build-stage
LABEL Description="DirtyJTAG Zephyr firmware"

USER root
WORKDIR /work
COPY . /work/dirtyjtag

RUN west init -l /work/dirtyjtag
RUN west update --narrow -o=--depth=1

WORKDIR /work/dirtyjtag
RUN west build -b dirtyjtag_bluepill/stm32f103xb -d build .

FROM scratch AS export-stage
COPY --from=build-stage /work/dirtyjtag/build/zephyr/zephyr.bin /dirtyjtag.bin
COPY --from=build-stage /work/dirtyjtag/build/zephyr/zephyr.elf /dirtyjtag.elf
