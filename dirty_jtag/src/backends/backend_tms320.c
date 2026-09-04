/* SPDX-License-Identifier: MIT */
#include "djprog/backend.h"
#include "djprog/hw.h"
#include "djprog/tms320.h"
#include <errno.h>
#include <string.h>

static struct dj_target_cfg cfg;

static uint32_t hp(void) {
	uint32_t hz = cfg.clock_hz ? cfg.clock_hz : 1000000u;
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
	if (c->power == DJ_PWR_5V)
		return -ERANGE; /* XDS110-class target I/O is 1.8..3.6 V. */
	cfg = *c;
	return 0;
}

static int jtag_clk(bool tms, bool tdi, bool *tdo) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	int r = h->write(DJ_PIN_CLK, false);
	if (r)
		return r;
	r = h->write(DJ_PIN_DATA0, tms);
	if (r)
		return r; /* TMS */
	r = h->write(DJ_PIN_DATA1, tdi);
	if (r)
		return r; /* TDI */
	dly();
	bool v = false;
	if (tdo) {
		r = h->read(DJ_PIN_DATA2, &v);
		if (r)
			return r;
	}
	r = h->write(DJ_PIN_CLK, true);
	if (r)
		return r;
	dly();
	if (tdo)
		*tdo = v;
	return 0;
}

static int tap_reset(unsigned cycles) {
	if (!cycles)
		cycles = 6;
	for (unsigned i = 0; i < cycles; ++i) {
		int r = jtag_clk(true, true, NULL);
		if (r)
			return r;
	}
	return jtag_clk(false, true, NULL); /* Run-Test/Idle */
}

static int goto_shift_ir(void) {
	int r;
	r = jtag_clk(true, true, NULL);
	if (r)
		return r; /* Select-DR */
	r = jtag_clk(true, true, NULL);
	if (r)
		return r; /* Select-IR */
	r = jtag_clk(false, true, NULL);
	if (r)
		return r;                       /* Capture-IR */
	return jtag_clk(false, true, NULL); /* Shift-IR */
}

static int goto_shift_dr(void) {
	int r;
	r = jtag_clk(true, true, NULL);
	if (r)
		return r; /* Select-DR */
	r = jtag_clk(false, true, NULL);
	if (r)
		return r;                       /* Capture-DR */
	return jtag_clk(false, true, NULL); /* Shift-DR */
}

static int finish_shift(void) {
	int r;
	r = jtag_clk(true, true, NULL);
	if (r)
		return r;                       /* Exit1 then Update */
	return jtag_clk(false, true, NULL); /* Run-Test/Idle */
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
		r = jtag_clk(last, tdi, &tdo);
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
		int r = jtag_clk(tms, tdi, &tdo);
		if (r)
			return r;
		if (tdo)
			rx[i / 8u] |= (uint8_t)(1u << (i & 7u));
	}
	*rxlen = bytes;
	return 0;
}

static int set_lines(bool nreset_released, bool ntrst_or_aux_released) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	int r = h->write(DJ_PIN_RESET, nreset_released);
	if (r)
		return r;
	return h->write(DJ_PIN_AUX, ntrst_or_aux_released);
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
		return r; /* TCK */
	r = h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT);
	if (r)
		return r; /* TMS */
	r = h->dir(DJ_PIN_DATA1, DJ_DIR_OUTPUT);
	if (r)
		return r; /* TDI */
	r = h->dir(DJ_PIN_DATA2, DJ_DIR_INPUT);
	if (r)
		return r; /* TDO */
	r = h->dir(DJ_PIN_RESET, DJ_DIR_OUTPUT);
	if (r)
		return r; /* nRESET */
	r = h->dir(DJ_PIN_AUX, DJ_DIR_OUTPUT);
	if (r)
		return r; /* nTRST or EMU0 hint */
	h->write(DJ_PIN_CLK, false);
	h->write(DJ_PIN_DATA0, true);
	h->write(DJ_PIN_DATA1, true);
	(void)set_lines(false, false);
	h->delay_us(1000);
	(void)set_lines(true, true);
	h->delay_us(1000);
	return tap_reset(8);
}

static int leave(void) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	(void)set_lines(true, true);
	h->dir(DJ_PIN_CLK, DJ_DIR_INPUT);
	h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
	h->dir(DJ_PIN_DATA1, DJ_DIR_INPUT);
	h->dir(DJ_PIN_DATA2, DJ_DIR_INPUT);
	h->dir(DJ_PIN_RESET, DJ_DIR_INPUT);
	h->dir(DJ_PIN_AUX, DJ_DIR_INPUT);
	return 0;
}

static int identify(uint8_t *out, size_t *len) {
	if (!out || !len || *len < 4)
		return -EINVAL;
	uint8_t z[4] = {0, 0, 0, 0};
	size_t n = 4;
	int r = tap_reset(8);
	if (r)
		return r;
	r = shift_bits(false, 32, z, out, &n);
	if (!r)
		*len = n;
	return r;
}

static int raw_xfer(const uint8_t *tx, size_t txlen, uint8_t *rx, size_t *rxlen) {
	if (!tx || !rx || !rxlen || txlen < 1)
		return -EINVAL;
	switch (tx[0]) {
	case DJ_TMS320_RAW_TAP_RESET:
		if (txlen != 2)
			return -EINVAL;
		*rxlen = 0;
		return tap_reset(tx[1]);
	case DJ_TMS320_RAW_SHIFT_IR:
	case DJ_TMS320_RAW_SHIFT_DR: {
		if (txlen < 3)
			return -EINVAL;
		uint16_t bits = (uint16_t)tx[1] | ((uint16_t)tx[2] << 8);
		size_t bytes = (bits + 7u) / 8u;
		if (bits == 0 || txlen != 3u + bytes)
			return -EINVAL;
		return shift_bits(tx[0] == DJ_TMS320_RAW_SHIFT_IR, bits, tx + 3, rx, rxlen);
	}
	case DJ_TMS320_RAW_CLOCK:
		if (txlen != 4)
			return -EINVAL;
		return clock_const(tx[1], tx[2] != 0u, tx[3] != 0u, rx, rxlen);
	case DJ_TMS320_RAW_LINES:
		if (txlen != 4)
			return -EINVAL;
		*rxlen = 0;
		(void)tx[3]; /* reserved for EMU hint on future Rev C/D hardware. */
		return set_lines(tx[1] != 0u, tx[2] != 0u);
	default:
		return -EINVAL;
	}
}

const struct dj_backend dj_backend_tms320_jtag = {DJ_PROTO_TMS320_C2000_JTAG,
                                                  "tms320-c2000-xds110v3-jtag",
                                                  DJ_CAP_IDENTIFY | DJ_CAP_RAW_XFER |
                                                      DJ_CAP_DEBUG_PHY | DJ_CAP_DEBUG_OPENOCD |
                                                      DJ_CAP_TARGET_POWER | DJ_CAP_EXPERIMENTAL,
                                                  1000000,
                                                  5000000,
                                                  sel,
                                                  enter,
                                                  leave,
                                                  identify,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  raw_xfer};
