/* SPDX-License-Identifier: MIT */
#include "djprog/backend.h"
#include "djprog/hw.h"
#include <errno.h>
#include <string.h>
/*
 * Device-independent PIC ICSP electrical transport. This deliberately does not guess
 * PIC16/PIC18 device command sets. Host/device profiles can use it to implement an
 * exact published programming specification while reusing Pico power/VPP/clocking.
 * raw opcodes:
 *   0x01 SET_PGD_DIR  [dir:0 input,1 output]
 *   0x02 WRITE_BITS   [bits:u8][flags:u8 bit0=lsb][value:u32le]
 *   0x03 READ_BITS    [bits:u8][flags:u8 bit0=lsb] -> ceil(bits/8) bytes little-endian
 *   0x04 MCLR         [released:u8]
 *   0x05 VPP_BOOST    [on:u8]
 *   0x06 VPP_APPLY    [on:u8]
 *   0x07 DELAY_US     [u32le]
 */
static struct dj_target_cfg cfg;
static uint32_t le32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int sel(const struct dj_target_cfg *c) {
	cfg = *c;
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
	h->write(DJ_PIN_RESET, true);
	h->dir(DJ_PIN_CLK, DJ_DIR_OUTPUT);
	h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
	return 0;
}
static int leave(void) {
	return dj_hw_safe_idle();
}
static int raw(const uint8_t *t, size_t n, uint8_t *r, size_t *rn) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h || !t || n < 1 || !rn)
		return -EINVAL;
	uint32_t hz = cfg.clock_hz ? cfg.clock_hz : 250000u;
	switch (t[0]) {
	case 0x01:
		if (n < 2)
			return -EINVAL;
		*rn = 0;
		return h->dir(DJ_PIN_DATA0, t[1] ? DJ_DIR_OUTPUT : DJ_DIR_INPUT);
	case 0x02: {
		if (n < 7)
			return -EINVAL;
		unsigned bits = t[1];
		if (bits == 0 || bits > 32)
			return -EINVAL;
		uint32_t v = le32(t + 3);
		uint8_t tx[4] = {v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
		*rn = 0;
		return h->clock_bits(DJ_PIN_CLK, DJ_PIN_DATA0, DJ_PIN_DATA0, tx, NULL, bits,
		                     (t[2] & 1u) != 0, hz);
	}
	case 0x03: {
		if (n < 3 || t[1] == 0 || t[1] > 32)
			return -EINVAL;
		size_t need = (t[1] + 7u) / 8u;
		if (*rn < need)
			return -ENOSPC;
		memset(r, 0, need);
		int e = h->clock_bits(DJ_PIN_CLK, DJ_PIN_DATA0, DJ_PIN_DATA0, NULL, r, t[1],
		                      (t[2] & 1u) != 0, hz);
		if (!e)
			*rn = need;
		return e;
	}
	case 0x04:
		if (n < 2)
			return -EINVAL;
		*rn = 0;
		return h->write(DJ_PIN_RESET, t[1] != 0);
	case 0x05:
		if (n < 2 || !h->vpp_boost)
			return -EINVAL;
		*rn = 0;
		return h->vpp_boost(t[1] != 0);
	case 0x06:
		if (n < 2 || !h->vpp_apply)
			return -EINVAL;
		*rn = 0;
		return h->vpp_apply(t[1] != 0);
	case 0x07:
		if (n < 5)
			return -EINVAL;
		h->delay_us(le32(t + 1));
		*rn = 0;
		return 0;
	default:
		return -ENOSYS;
	}
}
const struct dj_backend dj_backend_pic24_raw = {DJ_PROTO_PIC24_ICSP,
                                                "pic24-icsp-raw",
                                                DJ_CAP_RAW_XFER | DJ_CAP_HV | DJ_CAP_TARGET_POWER |
                                                    DJ_CAP_EXPERIMENTAL,
                                                250000,
                                                2000000,
                                                sel,
                                                enter,
                                                leave,
                                                0,
                                                0,
                                                0,
                                                0,
                                                raw};
const struct dj_backend dj_backend_pic_raw = {DJ_PROTO_PIC_ICSP_RAW,
                                              "pic10-pic12-pic16-pic18-icsp-raw",
                                              DJ_CAP_RAW_XFER | DJ_CAP_HV | DJ_CAP_TARGET_POWER |
                                                  DJ_CAP_EXPERIMENTAL,
                                              250000,
                                              2000000,
                                              sel,
                                              enter,
                                              leave,
                                              0,
                                              0,
                                              0,
                                              0,
                                              raw};
