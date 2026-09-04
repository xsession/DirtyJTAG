/* SPDX-License-Identifier: MIT
 *
 * RP2040 PIO-backed STM8 SWIM PHY for the Raspberry Pi Pico Zephyr build.
 *
 * The upper STM8 debugger remains in src/swim.c.  This file only replaces the
 * timing-sensitive packet waveform engine when CONFIG_DJPROG_SWIM_RP2040_PIO is
 * enabled.  The implementation is intentionally split into a small, auditable
 * PHY so the same DJP2 USB/debug ABI can be exercised with the mock/GPIO path
 * in CI and with PIO timing on real Pico hardware.
 */
#include "djprog/common.h"
#include "djprog/swim_phy.h"
#include <errno.h>

#if defined(CONFIG_DJPROG_SWIM_RP2040_PIO)

#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <hardware/pio_instructions.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/misc/pio_rpi_pico/pio_rpi_pico.h>
#include <zephyr/kernel.h>

#ifndef BIT
#define BIT(n) (1u << (n))
#endif

/* PIO instruction buffers are generated at runtime with Pico SDK encoders so
 * the source stays readable and avoids hand-maintained instruction constants.
 *
 * TX token format pushed from C:
 *   bits  0..15 : low-loop count
 *   bits 16..31 : release/high-loop count
 *
 * The PIO state machine drives the SWIM line low by changing pin direction to
 * output-low, then releases it by changing pin direction to input.  The external
 * front-end pull-up/translator returns the line high.  This matches the open
 * drain-style electrical behavior expected by STM8 SWIM. */
static uint16_t swim_tx_prog_insn[9];
static struct pio_program swim_tx_program = {
    .instructions = swim_tx_prog_insn,
    .length = 9,
    .origin = -1,
};

/* RX program: sample one SWIM bit/packet at the configured bit-center cadence.
 * C waits for the falling edge/header window, then enables the RX SM.  The SM
 * shifts one sample per bit into RX FIFO.  This keeps the shortest timing away
 * from k_busy_wait(); the remaining job for real hardware validation is to tune
 * the start offset per front-end delay and target voltage. */
static uint16_t swim_rx_prog_insn[5];
static struct pio_program swim_rx_program = {
    .instructions = swim_rx_prog_insn,
    .length = 5,
    .origin = -1,
};

struct pio_swim_ctx {
	bool bound;
	bool selected;
	const struct device *piodev;
	PIO pio;
	size_t tx_sm;
	size_t rx_sm;
	uint tx_offset;
	uint rx_offset;
	uint8_t pin;
	uint32_t hz;
};

static struct pio_swim_ctx ctx;
static int pio_recv_bit(bool *bit);

static void build_tx_program(void) {
	/*
	 * 0 pull block
	 * 1 out x,16        ; low count
	 * 2 out y,16        ; high/release count
	 * 3 set pindirs,1   ; drive low (pin value is already 0)
	 * 4 jmp x--,4
	 * 5 set pindirs,0   ; release line
	 * 6 jmp y--,6
	 * 7 irq 0 rel       ; optional timing marker for logic analyzer
	 * 8 jmp 0
	 */
	swim_tx_prog_insn[0] = (uint16_t)pio_encode_pull(false, true);
	swim_tx_prog_insn[1] = (uint16_t)pio_encode_out(pio_x, 16);
	swim_tx_prog_insn[2] = (uint16_t)pio_encode_out(pio_y, 16);
	swim_tx_prog_insn[3] = (uint16_t)pio_encode_set(pio_pindirs, 1);
	swim_tx_prog_insn[4] = (uint16_t)pio_encode_jmp_x_dec(4);
	swim_tx_prog_insn[5] = (uint16_t)pio_encode_set(pio_pindirs, 0);
	swim_tx_prog_insn[6] = (uint16_t)pio_encode_jmp_y_dec(6);
	swim_tx_prog_insn[7] = (uint16_t)pio_encode_irq_set(false, 0);
	swim_tx_prog_insn[8] = (uint16_t)pio_encode_jmp(0);
}

static void build_rx_program(void) {
	/*
	 * 0 pull block       ; bit count minus one in OSR
	 * 1 out x,8
	 * 2 in pins,1
	 * 3 jmp x--,2
	 * 4 push block
	 */
	swim_rx_prog_insn[0] = (uint16_t)pio_encode_pull(false, true);
	swim_rx_prog_insn[1] = (uint16_t)pio_encode_out(pio_x, 8);
	swim_rx_prog_insn[2] = (uint16_t)pio_encode_in(pio_pins, 1);
	swim_rx_prog_insn[3] = (uint16_t)pio_encode_jmp_x_dec(2);
	swim_rx_prog_insn[4] = (uint16_t)pio_encode_push(false, true);
}

static bool pio_available(void) {
	return ctx.bound;
}

static int pio_select(uint32_t requested_hz) {
	if (!ctx.bound)
		return -ENODEV;
	if (requested_hz < 100000u || requested_hz > 8000000u)
		return -ERANGE;
	ctx.hz = requested_hz;

	float div = (float)clock_get_hz(clk_sys) / (float)(ctx.hz * 10u);
	if (div < 1.0f)
		div = 1.0f;
	pio_sm_set_clkdiv(ctx.pio, (uint)ctx.tx_sm, div);
	pio_sm_set_clkdiv(ctx.pio, (uint)ctx.rx_sm, div);
	ctx.selected = true;
	return 0;
}

static int pio_enter(void) {
	if (!ctx.selected)
		return -ENODEV;
	/* Hold SWIM low for a conservative reset/entry pulse. */
	uint32_t token = (160u & 0xffffu) | (8u << 16);
	pio_sm_put_blocking(ctx.pio, (uint)ctx.tx_sm, token);
	k_busy_wait(500);
	return 0;
}

static int pio_leave(void) {
	if (!ctx.bound)
		return 0;
	pio_sm_set_enabled(ctx.pio, (uint)ctx.tx_sm, false);
	pio_sm_set_enabled(ctx.pio, (uint)ctx.rx_sm, false);
	pio_sm_set_pindirs_with_mask(ctx.pio, (uint)ctx.tx_sm, 0u, BIT(ctx.pin));
	ctx.selected = false;
	return 0;
}

static int pio_send_bit(bool bit) {
	if (!ctx.selected)
		return -ENODEV;
	uint16_t low, high;
	if (ctx.hz >= 700000u) {
		low = bit ? 2u : 8u;
		high = bit ? 8u : 2u;
	} else {
		low = bit ? 2u : 20u;
		high = bit ? 20u : 2u;
	}
	uint32_t token = (uint32_t)low | ((uint32_t)high << 16);
	pio_sm_put_blocking(ctx.pio, (uint)ctx.tx_sm, token);
	return 0;
}

static int pio_send_packet(uint32_t value, unsigned bits) {
	if (bits != 3u && bits != 8u)
		return -EINVAL;
	int r = pio_send_bit(false); /* header */
	if (r)
		return r;
	unsigned parity = 0u;
	for (unsigned i = 0; i < bits; ++i) {
		bool b = ((value >> i) & 1u) != 0u;
		parity ^= b ? 1u : 0u;
		r = pio_send_bit(b);
		if (r)
			return r;
	}
	r = pio_send_bit(parity != 0u);
	if (r)
		return r;

	/* Rev-F receive path is sampled by a second PIO state machine; the CPU only
	 * starts the capture window.  Wait long enough for the PIO TX state machine
	 * to finish the final parity pulse before reading the target ACK. */
	k_busy_wait(ctx.hz >= 700000u ? 4u : 80u);
	bool ack = true;
	r = pio_recv_bit(&ack);
	if (r)
		return r;
	return ack ? 0 : -EAGAIN;
}

static int pio_recv_bit(bool *bit) {
	if (!bit)
		return -EINVAL;
	if (!ctx.selected)
		return -ENODEV;
	pio_sm_clear_fifos(ctx.pio, (uint)ctx.rx_sm);
	pio_sm_restart(ctx.pio, (uint)ctx.rx_sm);
	pio_sm_put_blocking(ctx.pio, (uint)ctx.rx_sm, 0u); /* one bit: count-1 = 0 */
	pio_sm_set_enabled(ctx.pio, (uint)ctx.rx_sm, true);
	uint32_t v = pio_sm_get_blocking(ctx.pio, (uint)ctx.rx_sm);
	pio_sm_set_enabled(ctx.pio, (uint)ctx.rx_sm, false);
	*bit = (v & 0x80000000u) != 0u;
	return 0;
}

static int pio_recv_packet(uint8_t *value) {
	if (!value)
		return -EINVAL;
	bool bit = true;
	int r = pio_recv_bit(&bit); /* header */
	if (r)
		return r;
	if (bit)
		return -EIO;
	uint8_t x = 0u;
	unsigned parity = 0u;
	for (unsigned i = 0; i < 8u; ++i) {
		r = pio_recv_bit(&bit);
		if (r)
			return r;
		if (bit)
			x |= (uint8_t)(1u << i);
		parity ^= bit ? 1u : 0u;
	}
	r = pio_recv_bit(&bit);
	if (r)
		return r;
	if ((bit ? 1u : 0u) != parity)
		return -EBADMSG;
	r = pio_send_bit(true); /* ACK */
	if (r)
		return r;
	*value = x;
	return 0;
}

static int pio_set_speed(bool high_speed, uint32_t hz) {
	(void)high_speed;
	return pio_select(hz);
}

static const struct dj_swim_phy_ops pio_ops = {
    .name = "rp2040-pio-txrx",
    .available = pio_available,
    .select = pio_select,
    .enter = pio_enter,
    .leave = pio_leave,
    .send_packet = pio_send_packet,
    .recv_packet = pio_recv_packet,
    .set_speed = pio_set_speed,
};

int dj_swim_rp2040_pio_try_bind(uint8_t data0_gpio, uint32_t initial_hz) {
	if (ctx.bound)
		return 0;
	/* Use an application alias instead of DT_NODELABEL(pio0): hardware/pio.h
     * defines pio0 as
	 * a Pico SDK register-pointer macro. */
	const struct device *piodev = DEVICE_DT_GET(DT_ALIAS(dirtyjtag_pio));
	if (!device_is_ready(piodev))
		return -ENODEV;

	build_tx_program();
	build_rx_program();
	PIO pio = pio_rpi_pico_get_pio(piodev);
	size_t tx_sm = 0u;
	size_t rx_sm = 0u;
	int r = pio_rpi_pico_allocate_sm(piodev, &tx_sm);
	if (r)
		return r;
	r = pio_rpi_pico_allocate_sm(piodev, &rx_sm);
	if (r)
		return r;
	if (!pio_can_add_program(pio, &swim_tx_program))
		return -EBUSY;
	if (!pio_can_add_program(pio, &swim_rx_program))
		return -EBUSY;
	uint tx_offset = pio_add_program(pio, &swim_tx_program);
	uint rx_offset = pio_add_program(pio, &swim_rx_program);

	pio_sm_config cfg = pio_get_default_sm_config();
	sm_config_set_out_shift(&cfg, true, false, 32);
	sm_config_set_out_pins(&cfg, data0_gpio, 1);
	sm_config_set_set_pins(&cfg, data0_gpio, 1);
	sm_config_set_fifo_join(&cfg, PIO_FIFO_JOIN_TX);
	sm_config_set_wrap(&cfg, tx_offset, tx_offset + 8u);
	sm_config_set_clkdiv(&cfg, 1.0f);

	pio_sm_set_pins_with_mask(pio, (uint)tx_sm, 0u, BIT(data0_gpio));
	pio_sm_set_pindirs_with_mask(pio, (uint)tx_sm, 0u, BIT(data0_gpio));
	pio_sm_init(pio, (uint)tx_sm, tx_offset, &cfg);
	pio_sm_set_enabled(pio, (uint)tx_sm, true);

	pio_sm_config rxcfg = pio_get_default_sm_config();
	sm_config_set_in_pins(&rxcfg, data0_gpio);
	sm_config_set_in_shift(&rxcfg, true, false, 32);
	sm_config_set_wrap(&rxcfg, rx_offset, rx_offset + 4u);
	sm_config_set_clkdiv(&rxcfg, 1.0f);
	pio_sm_init(pio, (uint)rx_sm, rx_offset, &rxcfg);
	pio_sm_set_enabled(pio, (uint)rx_sm, false);

	ctx.bound = true;
	ctx.selected = false;
	ctx.piodev = piodev;
	ctx.pio = pio;
	ctx.tx_sm = tx_sm;
	ctx.rx_sm = rx_sm;
	ctx.tx_offset = tx_offset;
	ctx.rx_offset = rx_offset;
	ctx.pin = data0_gpio;
	ctx.hz = initial_hz ? initial_hz : 363000u;
	(void)dj_swim_phy_register(&pio_ops);
	return pio_select(ctx.hz);
}

#endif /* CONFIG_DJPROG_SWIM_RP2040_PIO */
