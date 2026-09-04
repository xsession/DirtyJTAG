/* SPDX-License-Identifier: MIT */
#include "djprog/backend.h"
#include "djprog/hw.h"
#include "djprog/phy.h"
#include <errno.h>

static struct dj_target_cfg swdc, jtc;
static int sels(const struct dj_target_cfg *c) {
	swdc = *c;
	return 0;
}
static int selj(const struct dj_target_cfg *c) {
	jtc = *c;
	return 0;
}
static int ent(void) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	h->dir(DJ_PIN_CLK, DJ_DIR_OUTPUT);
	h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT);
	return 0;
}
static int lev(void) {
	if (dj_hw()) {
		dj_hw()->dir(DJ_PIN_CLK, DJ_DIR_INPUT);
		dj_hw()->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
	}
	return 0;
}
static int raws(const uint8_t *t, size_t n, uint8_t *r, size_t *rn) {
	if (!t || n < 4 || !rn)
		return -EINVAL;
	uint16_t tb = (uint16_t)t[0] | ((uint16_t)t[1] << 8),
	         rb = (uint16_t)t[2] | ((uint16_t)t[3] << 8);
	size_t txb = ((size_t)tb + 7u) / 8u, rxb = ((size_t)rb + 7u) / 8u;
	if (4u + txb > n || rxb > *rn)
		return -EINVAL;
	int e = dj_swd_sequence(t + 4, tb, r, rb, swdc.clock_hz ? swdc.clock_hz : 1000000u);
	if (!e)
		*rn = rxb;
	return e;
}
static int rawj(const uint8_t *t, size_t n, uint8_t *r, size_t *rn) {
	if (!t || n < 2 || !rn)
		return -EINVAL;
	uint16_t b = (uint16_t)t[0] | ((uint16_t)t[1] << 8);
	size_t nb = ((size_t)b + 7u) / 8u;
	if (2u + nb > n || nb > *rn)
		return -EINVAL;
	int e = dj_jtag_shift(t + 2, b, r, jtc.clock_hz ? jtc.clock_hz : 1000000u);
	if (!e)
		*rn = nb;
	return e;
}
const struct dj_backend dj_backend_swd = {DJ_PROTO_ARM_SWD,
                                          "arm-swd-raw",
                                          DJ_CAP_RAW_XFER | DJ_CAP_DEBUG_PHY | DJ_CAP_DEBUG_OPENOCD,
                                          1000000,
                                          5000000,
                                          sels,
                                          ent,
                                          lev,
                                          0,
                                          0,
                                          0,
                                          0,
                                          raws};
const struct dj_backend dj_backend_jtag = {DJ_PROTO_JTAG,
                                           "jtag-raw",
                                           DJ_CAP_RAW_XFER | DJ_CAP_DEBUG_PHY |
                                               DJ_CAP_DEBUG_OPENOCD,
                                           1000000,
                                           5000000,
                                           selj,
                                           ent,
                                           lev,
                                           0,
                                           0,
                                           0,
                                           0,
                                           rawj};
