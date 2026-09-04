/* SPDX-License-Identifier: MIT */
#include "djprog/backend.h"
#include "djprog/hw.h"
#include "djprog/msp430.h"
#include <errno.h>
#include <string.h>

static struct dj_target_cfg cfg;

static uint32_t hp(void) {
	uint32_t hz = cfg.clock_hz ? cfg.clock_hz : 100000u;
	uint32_t x = 500000u / hz;
	return x ? x : 1u;
}

static void dly(void) {
	const struct dj_hw_ops *h = dj_hw();
	if (h && h->delay_us)
		h->delay_us(hp());
}

static int sel(const struct dj_target_cfg *c) {
	if (!c)
		return -EINVAL;
	cfg = *c;
	return 0;
}

static int sbw_slot(bool tms, bool tdi, bool *tdo) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	int r;
	/* Spy-Bi-Wire serializes a JTAG TMS/TDI/TDO cycle into three SBWTCK
	 * slots on SBWTDIO.  This is a functional GPIO implementation intended
	 * for bring-up and low-speed validation; a future PIO path can replace
	 * the slot primitive without changing the host protocol. */
	r = h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT);
	if (r)
		return r;
	r = h->write(DJ_PIN_DATA0, tms);
	if (r)
		return r;
	r = h->write(DJ_PIN_CLK, false);
	if (r)
		return r;
	dly();
	r = h->write(DJ_PIN_CLK, true);
	if (r)
		return r;
	dly();

	r = h->write(DJ_PIN_DATA0, tdi);
	if (r)
		return r;
	r = h->write(DJ_PIN_CLK, false);
	if (r)
		return r;
	dly();
	r = h->write(DJ_PIN_CLK, true);
	if (r)
		return r;
	dly();

	r = h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
	if (r)
		return r;
	bool v = false;
	r = h->write(DJ_PIN_CLK, false);
	if (r)
		return r;
	dly();
	r = h->write(DJ_PIN_CLK, true);
	if (r)
		return r;
	if (tdo) {
		r = h->read(DJ_PIN_DATA0, &v);
		if (r)
			return r;
	}
	dly();
	if (tdo)
		*tdo = v;
	return 0;
}

static int tap_reset(unsigned cycles) {
	if (!cycles)
		cycles = 6;
	for (unsigned i = 0; i < cycles; ++i) {
		int r = sbw_slot(true, true, NULL);
		if (r)
			return r;
	}
	return sbw_slot(false, true, NULL);
}

static int goto_shift_ir(void) {
	int r;
	r = sbw_slot(true, true, NULL);
	if (r)
		return r;
	r = sbw_slot(true, true, NULL);
	if (r)
		return r;
	r = sbw_slot(false, true, NULL);
	if (r)
		return r;
	return sbw_slot(false, true, NULL);
}

static int goto_shift_dr(void) {
	int r;
	r = sbw_slot(true, true, NULL);
	if (r)
		return r;
	r = sbw_slot(false, true, NULL);
	if (r)
		return r;
	return sbw_slot(false, true, NULL);
}

static int finish_shift(void) {
	int r;
	r = sbw_slot(true, true, NULL);
	if (r)
		return r;
	return sbw_slot(false, true, NULL);
}

static int shift_bits(bool ir, uint16_t bits, const uint8_t *tx, uint8_t *rx, size_t *rxlen) {
	if (!bits || !tx || !rx || !rxlen)
		return -EINVAL;
	size_t bytes = (bits + 7u) / 8u;
	if (*rxlen < bytes)
		return -ENOSPC;
	memset(rx, 0, bytes);
	int r = ir ? goto_shift_ir() : goto_shift_dr();
	if (r)
		return r;
	for (uint16_t i = 0; i < bits; ++i) {
		bool last = (i + 1u) == bits;
		bool tdi = ((tx[i / 8u] >> (i & 7u)) & 1u) != 0;
		bool tdo = false;
		r = sbw_slot(last, tdi, &tdo);
		if (r)
			return r;
		if (tdo)
			rx[i / 8u] |= (uint8_t)(1u << (i & 7u));
	}
	r = finish_shift();
	if (!r)
		*rxlen = bytes;
	return r;
}

static int clock_const(uint8_t cycles, bool tms, bool tdi, uint8_t *rx, size_t *rxlen) {
	if (!rx || !rxlen)
		return -EINVAL;
	size_t bytes = (cycles + 7u) / 8u;
	if (*rxlen < bytes)
		return -ENOSPC;
	memset(rx, 0, bytes);
	for (uint8_t i = 0; i < cycles; ++i) {
		bool tdo = false;
		int r = sbw_slot(tms, tdi, &tdo);
		if (r)
			return r;
		if (tdo)
			rx[i / 8u] |= (uint8_t)(1u << (i & 7u));
	}
	*rxlen = bytes;
	return 0;
}

static int enter(void) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	if (h->power) {
		int r = h->power(cfg.power);
		if (r)
			return r;
	}
	int r;
	r = h->dir(DJ_PIN_CLK, DJ_DIR_OUTPUT);
	if (r)
		return r; /* SBWTCK / TEST */
	r = h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT);
	if (r)
		return r; /* SBWTDIO / RST */
	h->write(DJ_PIN_CLK, false);
	h->write(DJ_PIN_DATA0, true);
	/* Conservative SBW entry gate: TEST/SBWTCK toggles while RST/SBWTDIO is
	 * held/released.  Exact target-family pulse timing is bench validated in
	 * host scripts before enabling erase/write algorithms. */
	h->write(DJ_PIN_DATA0, false);
	h->delay_us(200);
	h->write(DJ_PIN_CLK, true);
	h->delay_us(200);
	h->write(DJ_PIN_DATA0, true);
	h->delay_us(1000);
	return tap_reset(8);
}

static int leave(void) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	h->write(DJ_PIN_CLK, false);
	h->write(DJ_PIN_DATA0, true);
	h->dir(DJ_PIN_CLK, DJ_DIR_INPUT);
	h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
	return 0;
}

static int raw_xfer(const uint8_t *tx, size_t txlen, uint8_t *rx, size_t *rxlen) {
	if (!tx || !rx || !rxlen || txlen < 1)
		return -EINVAL;
	switch (tx[0]) {
	case DJ_MSP430_RAW_TAP_RESET:
		if (txlen != 2)
			return -EINVAL;
		*rxlen = 0;
		return tap_reset(tx[1]);
	case DJ_MSP430_RAW_SHIFT_IR:
	case DJ_MSP430_RAW_SHIFT_DR: {
		if (txlen < 3)
			return -EINVAL;
		uint16_t bits = (uint16_t)tx[1] | ((uint16_t)tx[2] << 8);
		size_t bytes = (bits + 7u) / 8u;
		if (bits == 0 || txlen != 3u + bytes)
			return -EINVAL;
		return shift_bits(tx[0] == DJ_MSP430_RAW_SHIFT_IR, bits, tx + 3, rx, rxlen);
	}
	case DJ_MSP430_RAW_CLOCK:
		if (txlen != 4)
			return -EINVAL;
		return clock_const(tx[1], tx[2] != 0u, tx[3] != 0u, rx, rxlen);
	default:
		return -EINVAL;
	}
}

const struct dj_backend dj_backend_sbw = {DJ_PROTO_MSP430_SBW,
                                          "msp430-sbw",
                                          DJ_CAP_RAW_XFER | DJ_CAP_DEBUG_PHY | DJ_CAP_TARGET_POWER |
                                              DJ_CAP_EXPERIMENTAL,
                                          100000,
                                          500000,
                                          sel,
                                          enter,
                                          leave,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          raw_xfer};
