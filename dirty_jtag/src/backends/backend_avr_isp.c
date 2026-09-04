/* SPDX-License-Identifier: MIT */
#include "djprog/backend.h"
#include "djprog/hw.h"
#include "djprog/phy.h"
#include <errno.h>
#include <string.h>
static struct dj_target_cfg cfg;
static uint16_t loaded_ext_addr = 0xffffu;
static int sel(const struct dj_target_cfg *c) {
	cfg = *c;
	loaded_ext_addr = 0xffffu;
	return 0;
}
static int pwr(void) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	h->dir(DJ_PIN_CLK, DJ_DIR_OUTPUT);
	h->dir(DJ_PIN_DATA1, DJ_DIR_OUTPUT);
	h->dir(DJ_PIN_DATA2, DJ_DIR_INPUT);
	h->dir(DJ_PIN_RESET, DJ_DIR_OUTPUT);
	if (h->power) {
		int r = h->power(cfg.power);
		if (r)
			return r;
	}
	h->write(DJ_PIN_RESET, true);
	h->delay_us(2000);
	h->write(DJ_PIN_RESET, false);
	h->delay_us(20000);
	return 0;
}
static int cmd(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t *r) {
	uint8_t tx[4] = {a, b, c, d}, rx[4] = {0};
	int e = dj_spi_xfer(tx, rx, 4, cfg.clock_hz ? cfg.clock_hz : 125000);
	if (r)
		*r = rx[3];
	return e;
}
static int enter(void) {
	int r = pwr();
	if (r)
		return r;
	for (int i = 0; i < 3; i++) {
		uint8_t tx[4] = {0xAC, 0x53, 0, 0}, rx[4] = {0};
		r = dj_spi_xfer(tx, rx, 4, cfg.clock_hz ? cfg.clock_hz : 125000);
		if (!r && rx[2] == 0x53)
			return 0;
		dj_hw()->write(DJ_PIN_RESET, true);
		dj_hw()->delay_us(2000);
		dj_hw()->write(DJ_PIN_RESET, false);
		dj_hw()->delay_us(20000);
	}
	return -EIO;
}
static int leave(void) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	h->write(DJ_PIN_RESET, true);
	return 0;
}
static int identify(uint8_t *out, size_t *len) {
	if (!out || !len || *len < 3)
		return -ENOSPC;
	for (int i = 0; i < 3; i++) {
		uint8_t x = 0;
		int r = cmd(0x30, 0, (uint8_t)i, 0, &x);
		if (r)
			return r;
		out[i] = x;
	}
	*len = 3;
	return 0;
}
static int erase(void) {
	loaded_ext_addr = 0xffffu;
	int r = cmd(0xAC, 0x80, 0, 0, 0);
	if (!r)
		dj_hw()->delay_us(12000);
	return r;
}

/* Classic AVR ISP uses 16-bit word addresses in read/write/page-write
 * instructions. Large parts such as ATmega1280/2560 expose higher flash
 * address bits through the public Load Extended Address Byte instruction:
 *   0x4D 0x00 ext 0x00
 * where ext is the high byte of the word address. */
static int load_ext_for_word(uint32_t word_addr) {
	uint16_t ext = (uint16_t)((word_addr >> 16) & 0xffu);
	if (ext == loaded_ext_addr)
		return 0;
	int r = cmd(0x4D, 0x00, (uint8_t)ext, 0x00, 0);
	if (!r)
		loaded_ext_addr = ext;
	return r;
}
static int readm(uint32_t a, uint8_t *d, size_t n) {
	if (!d)
		return -EINVAL;
	for (size_t i = 0; i < n; i++) {
		uint32_t ba = a + i, wa = ba >> 1;
		int r = load_ext_for_word(wa);
		if (r)
			return r;
		uint8_t x = 0;
		r = cmd((ba & 1) ? 0x28 : 0x20, (uint8_t)(wa >> 8), (uint8_t)wa, 0, &x);
		if (r)
			return r;
		d[i] = x;
	}
	return 0;
}
static int writem(uint32_t a, const uint8_t *d, size_t n) {
	if (!d || !cfg.page_size || cfg.page_size > 512 || (a % cfg.page_size) || (n % cfg.page_size))
		return -EINVAL;
	for (size_t p = 0; p < n; p += cfg.page_size) {
		uint32_t page_word = (a + p) >> 1;
		int r = load_ext_for_word(page_word);
		if (r)
			return r;
		for (size_t i = 0; i < cfg.page_size; i++) {
			uint32_t ba = a + p + i, wa = ba >> 1;
			r = cmd((ba & 1) ? 0x48 : 0x40, 0, (uint8_t)wa, d[p + i], 0);
			if (r)
				return r;
		}
		r = cmd(0x4C, (uint8_t)(page_word >> 8), (uint8_t)page_word, 0, 0);
		if (r)
			return r;
		dj_hw()->delay_us(10000);
	}
	return 0;
}
static int raw(const uint8_t *tx, size_t tn, uint8_t *rx, size_t *rn) {
	if (!rn || tn % 4)
		return -EINVAL;
	size_t n = tn;
	if (n > *rn)
		return -ENOSPC;
	int r = dj_spi_xfer(tx, rx, n, cfg.clock_hz ? cfg.clock_hz : 125000);
	if (!r)
		*rn = n;
	return r;
}
const struct dj_backend dj_backend_avr_isp = {DJ_PROTO_AVR_ISP,
                                              "avr-isp",
                                              DJ_CAP_IDENTIFY | DJ_CAP_ERASE | DJ_CAP_READ |
                                                  DJ_CAP_WRITE | DJ_CAP_RAW_XFER,
                                              125000,
                                              1000000,
                                              sel,
                                              enter,
                                              leave,
                                              identify,
                                              erase,
                                              readm,
                                              writem,
                                              raw};
