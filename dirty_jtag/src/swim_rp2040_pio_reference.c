/* SPDX-License-Identifier: MIT
 *
 * RP2040 PIO SWIM timing reference for the rpi_pico Zephyr build.
 *
 * Zephyr currently expects pre-assembled PIO programs or RPI_PICO_PIO_DEFINE_PROGRAM()
 * style embedded programs.  This file intentionally stays out of CMake until the
 * board-level PIO allocation is pinned; the portable src/swim.c software path keeps
 * the firmware buildable, while this reference defines the required Rev-D hardware
 * behavior for the production PIO driver.
 *
 * Required PIO behavior:
 *   - drive DATA0 low through the translator for N cycles, then release high-Z;
 *   - high-speed SWIM bit 1: low for 2 SWIM clocks, release for 8 clocks;
 *   - high-speed SWIM bit 0: low for 8 SWIM clocks, release for 2 clocks;
 *   - low-speed SWIM bit 1: low for 2 SWIM clocks, release for 20 clocks;
 *   - low-speed SWIM bit 0: low for 20 SWIM clocks, release for 2 clocks;
 *   - sample incoming target bits by counting consecutive low samples inside the
 *     10/22-clock bit window;
 *   - expose DMA/FIFO operation blocks so USB can request WOTF/ROTF without
 *     per-edge interrupts.
 *
 * The production driver should use <zephyr/drivers/misc/pio_rpi_pico/pio_rpi_pico.h>
 * and the RPI_PICO_PIO_DEFINE_PROGRAM helper.  Do not use k_busy_wait() for final
 * high-speed SWIM validation because UM0470 requires ~192-208 ns low pulses for a
 * high-speed logical 1.
 */
