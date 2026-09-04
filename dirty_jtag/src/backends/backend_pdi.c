/* SPDX-License-Identifier: MIT */
#include "djprog/backend.h"
#include "djprog/hw.h"
#include "djprog/phy.h"
#include <errno.h>
static struct dj_target_cfg cfg;
static int sel(const struct dj_target_cfg *c) {
	cfg = *c;
	return 0;
}
static uint32_t hz(void) {
	uint32_t x = cfg.clock_hz ? cfg.clock_hz : 250000u;
	return x > 500000u ? 500000u : x;
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
	/* XMEGA PDI: RESET/PDI_CLK is clock, DATA is half-duplex. Keep clock active >=10 kHz. */
	h->dir(DJ_PIN_CLK, DJ_DIR_OUTPUT);
	h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT);
	h->write(DJ_PIN_DATA0, true);
	h->write(DJ_PIN_CLK, false);
	h->delay_us(1000);
	return dj_sync_1wire_break(hz(), 24);
}
static int leave(void) {
	if (dj_hw()) {
		dj_hw()->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
		dj_hw()->dir(DJ_PIN_CLK, DJ_DIR_INPUT);
	}
	return 0;
}
static int raw(const uint8_t *t, size_t n, uint8_t *r, size_t *rn) {
	if (!t || n < 4 || !rn)
		return -EINVAL;
	uint16_t txc = (uint16_t)t[0] | ((uint16_t)t[1] << 8),
	         rxc = (uint16_t)t[2] | ((uint16_t)t[3] << 8);
	if (4u + (size_t)txc > n || (size_t)rxc > *rn)
		return -EINVAL;
	int e = dj_sync_1wire_txrx_8e2(t + 4, txc, r, rxc, hz());
	if (!e)
		*rn = rxc;
	return e;
}
const struct dj_backend dj_backend_pdi = {DJ_PROTO_AVR_PDI,
                                          "avr-xmega-pdi-8e2-raw",
                                          DJ_CAP_RAW_XFER | DJ_CAP_DEBUG_PHY | DJ_CAP_TARGET_POWER |
                                              DJ_CAP_EXPERIMENTAL,
                                          250000,
                                          500000,
                                          sel,
                                          enter,
                                          leave,
                                          0,
                                          0,
                                          0,
                                          0,
                                          raw};
