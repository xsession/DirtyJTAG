/* SPDX-License-Identifier: MIT */
#include "djprog/backend.h"
#include "djprog/hw.h"
#include "djprog/phy.h"
#include "djprog/simplelink.h"
#include <errno.h>
#include <string.h>

static struct dj_target_cfg swdc, cjc;

static int sel_common(const struct dj_target_cfg *c, struct dj_target_cfg *dst) {
	if (!c || !dst)
		return -EINVAL;
	if (c->power == DJ_PWR_5V)
		return -ERANGE; /* XDS110-class CC13xx/CC26xx debug IO is 1.8..3.6 V. */
	*dst = *c;
	return 0;
}
static int sels(const struct dj_target_cfg *c) {
	return sel_common(c, &swdc);
}
static int selc(const struct dj_target_cfg *c) {
	return sel_common(c, &cjc);
}

static int enter_swd(void) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	if (h->power) {
		int r = h->power(swdc.power);
		if (r)
			return r;
	}
	int r = h->dir(DJ_PIN_CLK, DJ_DIR_OUTPUT);
	if (r)
		return r; /* SWCLK */
	r = h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT);
	if (r)
		return r; /* SWDIO */
	r = h->dir(DJ_PIN_RESET, DJ_DIR_OUTPUT);
	if (r)
		return r; /* nRESET */
	h->write(DJ_PIN_RESET, true);
	return 0;
}

static int enter_cjtag(void) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	if (h->power) {
		int r = h->power(cjc.power);
		if (r)
			return r;
	}
	int r = h->dir(DJ_PIN_CLK, DJ_DIR_OUTPUT);
	if (r)
		return r; /* TCKC */
	r = h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT);
	if (r)
		return r; /* TMSC */
	r = h->dir(DJ_PIN_RESET, DJ_DIR_OUTPUT);
	if (r)
		return r; /* nRESET */
	r = h->dir(DJ_PIN_AUX, DJ_DIR_OUTPUT);
	if (r)
		return r; /* bootloader/backdoor hint */
	h->write(DJ_PIN_RESET, true);
	h->write(DJ_PIN_AUX, false);
	return 0;
}

static int leave_common(void) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	h->dir(DJ_PIN_CLK, DJ_DIR_INPUT);
	h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
	h->dir(DJ_PIN_DATA1, DJ_DIR_INPUT);
	h->dir(DJ_PIN_DATA2, DJ_DIR_INPUT);
	h->dir(DJ_PIN_RESET, DJ_DIR_INPUT);
	h->dir(DJ_PIN_AUX, DJ_DIR_INPUT);
	return 0;
}

static int identify(uint8_t *out, size_t *len) {
	static const char s[] = "ti-simplelink-cc13xx-cc26xx;debug=swd/cjtag;boot=rom-uart/spi-plan";
	if (!out || !len || *len < sizeof(s) - 1u)
		return -ENOSPC;
	memcpy(out, s, sizeof(s) - 1u);
	*len = sizeof(s) - 1u;
	return 0;
}

static int raw_swd(const uint8_t *t, size_t n, uint8_t *r, size_t *rn) {
	if (!t || !rn || n < 1)
		return -EINVAL;
	if (t[0] == DJ_SIMPLELINK_RAW_LINES) {
		if (n != 3)
			return -EINVAL;
		const struct dj_hw_ops *h = dj_hw();
		if (!h || !h->write)
			return -ENOTSUP;
		*rn = 0;
		int e = h->write(DJ_PIN_RESET, t[1] != 0u);
		if (e)
			return e;
		return h->write(DJ_PIN_AUX, t[2] != 0u);
	}
	if (t[0] != DJ_SIMPLELINK_RAW_SWD_SEQUENCE || n < 5)
		return -EINVAL;
	uint16_t tb = (uint16_t)t[1] | ((uint16_t)t[2] << 8);
	uint16_t rb = (uint16_t)t[3] | ((uint16_t)t[4] << 8);
	size_t txb = ((size_t)tb + 7u) / 8u;
	size_t rxb = ((size_t)rb + 7u) / 8u;
	if (5u + txb > n || rxb > *rn)
		return -EINVAL;
	int e = dj_swd_sequence(t + 5, tb, r, rb, swdc.clock_hz ? swdc.clock_hz : 1000000u);
	if (!e)
		*rn = rxb;
	return e;
}

static int cjtag_clk(bool tmsc, bool *sample) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	int r = h->write(DJ_PIN_CLK, false);
	if (r)
		return r;
	r = h->write(DJ_PIN_DATA0, tmsc);
	if (r)
		return r;
	if (h->delay_us)
		h->delay_us(1);
	bool v = false;
	if (sample) {
		r = h->read(DJ_PIN_DATA0, &v);
		if (r)
			return r;
	}
	r = h->write(DJ_PIN_CLK, true);
	if (r)
		return r;
	if (h->delay_us)
		h->delay_us(1);
	if (sample)
		*sample = v;
	return 0;
}

static int raw_cjtag(const uint8_t *t, size_t n, uint8_t *r, size_t *rn) {
	if (!t || !r || !rn || n < 1)
		return -EINVAL;
	if (t[0] == DJ_SIMPLELINK_RAW_LINES) {
		if (n != 3)
			return -EINVAL;
		const struct dj_hw_ops *h = dj_hw();
		if (!h || !h->write)
			return -ENOTSUP;
		*rn = 0;
		int e = h->write(DJ_PIN_RESET, t[1] != 0u);
		if (e)
			return e;
		return h->write(DJ_PIN_AUX, t[2] != 0u);
	}
	if (t[0] != DJ_SIMPLELINK_RAW_CJTAG_CLOCK || n != 3)
		return -EINVAL;
	uint8_t cycles = t[1];
	size_t bytes = (cycles + 7u) / 8u;
	if (*rn < bytes)
		return -ENOSPC;
	memset(r, 0, bytes);
	for (uint8_t i = 0; i < cycles; ++i) {
		bool out = ((t[2] >> (i & 7u)) & 1u) != 0u;
		bool in = false;
		int e = cjtag_clk(out, &in);
		if (e)
			return e;
		if (in)
			r[i / 8u] |= (uint8_t)(1u << (i & 7u));
	}
	*rn = bytes;
	return 0;
}

const struct dj_backend dj_backend_simplelink_swd = {DJ_PROTO_TI_SIMPLELINK_SWD,
                                                     "ti-simplelink-cc13xx-cc26xx-swd",
                                                     DJ_CAP_IDENTIFY | DJ_CAP_RAW_XFER |
                                                         DJ_CAP_DEBUG_PHY | DJ_CAP_DEBUG_OPENOCD |
                                                         DJ_CAP_TARGET_POWER | DJ_CAP_EXPERIMENTAL,
                                                     1000000,
                                                     8000000,
                                                     sels,
                                                     enter_swd,
                                                     leave_common,
                                                     identify,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     raw_swd};

const struct dj_backend dj_backend_simplelink_cjtag = {DJ_PROTO_TI_SIMPLELINK_CJTAG,
                                                       "ti-simplelink-cc13xx-cc26xx-cjtag-2wire",
                                                       DJ_CAP_IDENTIFY | DJ_CAP_RAW_XFER |
                                                           DJ_CAP_DEBUG_PHY | DJ_CAP_TARGET_POWER |
                                                           DJ_CAP_EXPERIMENTAL,
                                                       1000000,
                                                       5000000,
                                                       selc,
                                                       enter_cjtag,
                                                       leave_common,
                                                       identify,
                                                       NULL,
                                                       NULL,
                                                       NULL,
                                                       raw_cjtag};
