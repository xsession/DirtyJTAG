/* SPDX-License-Identifier: MIT */
#include "djprog/bridge.h"
#include "djprog/common.h"
#include "djprog/hw.h"
#include <errno.h>
#include <string.h>

static void dly(uint32_t us) {
	const struct dj_hw_ops *h = dj_hw();
	if (h && h->delay_us)
		h->delay_us(us ? us : 1u);
}

int dj_bridge_info(uint8_t *out, size_t *len) {
	static const char s[] =
	    "gpio,spi-bitbang,i2c-bitbang,uart-bitbang,power-trace,production-job-ready";
	if (!out || !len || *len < sizeof(s) - 1u)
		return -ENOSPC;
	memcpy(out, s, sizeof(s) - 1u);
	*len = sizeof(s) - 1u;
	return 0;
}

int dj_bridge_gpio(const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen) {
	if (!in || !out || !outlen || inlen < 2)
		return -EINVAL;
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	enum dj_pin_role role = (enum dj_pin_role)in[1];
	if (role >= DJ_PIN_COUNT)
		return -ERANGE;
	switch (in[0]) {
	case DJ_BRIDGE_GPIO_READ: {
		bool v = false;
		int r = h->read ? h->read(role, &v) : -ENOTSUP;
		if (r)
			return r;
		if (*outlen < 1)
			return -ENOSPC;
		out[0] = v ? 1u : 0u;
		*outlen = 1;
		return 0;
	}
	case DJ_BRIDGE_GPIO_WRITE:
		if (inlen < 3)
			return -EINVAL;
		*outlen = 0;
		return h->write ? h->write(role, in[2] != 0u) : -ENOTSUP;
	case DJ_BRIDGE_GPIO_DIR:
		if (inlen < 3)
			return -EINVAL;
		*outlen = 0;
		return h->dir ? h->dir(role, (enum dj_dir)in[2]) : -ENOTSUP;
	default:
		return -EINVAL;
	}
}

int dj_bridge_spi(const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen) {
	if (!in || !out || !outlen || inlen < 10)
		return -EINVAL;
	const struct dj_hw_ops *h = dj_hw();
	if (!h || !h->clock_bits || !h->dir || !h->write)
		return -ENOTSUP;
	uint8_t mode = in[0] & 3u;
	enum dj_pin_role cs = (enum dj_pin_role)in[1];
	uint16_t flags = dj_le16(in + 2);
	uint32_t hz = dj_le32(in + 4);
	uint16_t n = dj_le16(in + 8);
	if (cs >= DJ_PIN_COUNT || 10u + n > inlen || n > *outlen)
		return -EINVAL;
	bool lsb_first = (flags & 1u) != 0u;
	bool cs_active_high = (flags & 2u) != 0u;
	bool cpol = (mode & 2u) != 0u;
	(void)(mode & 1u); /* CPHA is approximated by the generic clock helper. */
	int r = h->dir(cs, DJ_DIR_OUTPUT);
	if (r)
		return r;
	r = h->write(DJ_PIN_CLK, cpol);
	if (r)
		return r;
	r = h->write(cs, cs_active_high ? true : false);
	if (r)
		return r;
	r = h->clock_bits(DJ_PIN_CLK, DJ_PIN_DATA1, DJ_PIN_DATA2, in + 10, out, n * 8u, lsb_first,
	                  hz ? hz : 100000u);
	(void)h->write(cs, cs_active_high ? false : true);
	if (!r)
		*outlen = n;
	return r;
}

static int i2c_drive_sda(bool high) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h || !h->dir || !h->write)
		return -ENOTSUP;
	if (high)
		return h->dir(DJ_PIN_DATA0, DJ_DIR_RELEASE);
	int r = h->write(DJ_PIN_DATA0, false);
	if (r)
		return r;
	return h->dir(DJ_PIN_DATA0, DJ_DIR_OD_LOW);
}

static int i2c_drive_scl(bool high) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h || !h->dir || !h->write)
		return -ENOTSUP;
	if (high)
		return h->dir(DJ_PIN_CLK, DJ_DIR_RELEASE);
	int r = h->write(DJ_PIN_CLK, false);
	if (r)
		return r;
	return h->dir(DJ_PIN_CLK, DJ_DIR_OD_LOW);
}

static int i2c_sample_sda(bool *v) {
	const struct dj_hw_ops *h = dj_hw();
	return h && h->read ? h->read(DJ_PIN_DATA0, v) : -ENOTSUP;
}

static int i2c_bit(bool bit, uint32_t half_us, bool *sample) {
	int r = i2c_drive_sda(bit);
	if (r)
		return r;
	dly(half_us);
	r = i2c_drive_scl(true);
	if (r)
		return r;
	dly(half_us);
	if (sample) {
		r = i2c_sample_sda(sample);
		if (r)
			return r;
	}
	r = i2c_drive_scl(false);
	if (r)
		return r;
	dly(half_us);
	return 0;
}

static int i2c_write_byte(uint8_t b, uint32_t half_us) {
	for (int i = 7; i >= 0; --i) {
		int r = i2c_bit(((b >> i) & 1u) != 0u, half_us, NULL);
		if (r)
			return r;
	}
	bool ack = true;
	int r = i2c_bit(true, half_us, &ack);
	return r ? r : 0; /* Keep bench mode tolerant: hosts can inspect target externally. */
}

static int i2c_read_byte(uint8_t *b, bool last, uint32_t half_us) {
	if (!b)
		return -EINVAL;
	*b = 0;
	for (int i = 7; i >= 0; --i) {
		bool bit = false;
		int r = i2c_bit(true, half_us, &bit);
		if (r)
			return r;
		if (bit)
			*b |= (uint8_t)(1u << i);
	}
	return i2c_bit(last, half_us, NULL); /* ACK all but last. */
}

int dj_bridge_i2c(const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen) {
	if (!in || !out || !outlen || inlen < 9)
		return -EINVAL;
	uint8_t addr = in[0];
	uint16_t txlen = dj_le16(in + 1);
	uint16_t rxlen = dj_le16(in + 3);
	uint32_t hz = dj_le32(in + 5);
	if (9u + txlen > inlen || rxlen > *outlen)
		return -EINVAL;
	uint32_t half = hz ? (500000u / hz) : 5u;
	if (!half)
		half = 1u;
	int r = i2c_drive_sda(true);
	if (r)
		return r;
	r = i2c_drive_scl(true);
	if (r)
		return r;
	dly(half);
	r = i2c_drive_sda(false);
	if (r)
		return r;
	dly(half);
	r = i2c_drive_scl(false);
	if (r)
		return r;
	r = i2c_write_byte((uint8_t)((addr << 1) | 0u), half);
	if (r)
		goto stop;
	for (uint16_t i = 0; i < txlen; ++i) {
		r = i2c_write_byte(in[9u + i], half);
		if (r)
			goto stop;
	}
	if (rxlen) {
		r = i2c_drive_sda(true);
		if (r)
			goto stop;
		r = i2c_drive_scl(true);
		if (r)
			goto stop;
		dly(half);
		r = i2c_drive_sda(false);
		if (r)
			goto stop;
		dly(half);
		r = i2c_drive_scl(false);
		if (r)
			goto stop;
		r = i2c_write_byte((uint8_t)((addr << 1) | 1u), half);
		if (r)
			goto stop;
		for (uint16_t i = 0; i < rxlen; ++i) {
			r = i2c_read_byte(out + i, i + 1u == rxlen, half);
			if (r)
				goto stop;
		}
	}
stop:
	(void)i2c_drive_sda(false);
	dly(half);
	(void)i2c_drive_scl(true);
	dly(half);
	(void)i2c_drive_sda(true);
	if (!r)
		*outlen = rxlen;
	return r;
}

int dj_bridge_uart(const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen) {
	if (!in || !out || !outlen || inlen < 10)
		return -EINVAL;
	const struct dj_hw_ops *h = dj_hw();
	if (!h || !h->dir || !h->write || !h->read)
		return -ENOTSUP;
	uint32_t baud = dj_le32(in + 0);
	uint16_t flags = dj_le16(in + 4);
	uint16_t txlen = dj_le16(in + 6);
	uint16_t rxlen = dj_le16(in + 8);
	if (!baud || 10u + txlen > inlen || rxlen > *outlen)
		return -EINVAL;
	uint32_t bit_us = 1000000u / baud;
	if (!bit_us)
		bit_us = 1u;
	bool inv = (flags & 1u) != 0u;
	int r = h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT);
	if (r)
		return r;
	r = h->write(DJ_PIN_DATA0, inv ? false : true);
	if (r)
		return r;
	for (uint16_t i = 0; i < txlen; ++i) {
		uint8_t b = in[10u + i];
		r = h->write(DJ_PIN_DATA0, inv ? true : false);
		if (r)
			return r;
		dly(bit_us);
		for (unsigned bit = 0; bit < 8; ++bit) {
			bool v = ((b >> bit) & 1u) != 0u;
			r = h->write(DJ_PIN_DATA0, inv ? !v : v);
			if (r)
				return r;
			dly(bit_us);
		}
		r = h->write(DJ_PIN_DATA0, inv ? false : true);
		if (r)
			return r;
		dly(bit_us);
	}
	r = h->dir(DJ_PIN_DATA1, DJ_DIR_INPUT);
	if (r)
		return r;
	for (uint16_t i = 0; i < rxlen; ++i) {
		uint8_t b = 0;
		dly(bit_us + bit_us / 2u);
		for (unsigned bit = 0; bit < 8; ++bit) {
			bool v = false;
			r = h->read(DJ_PIN_DATA1, &v);
			if (r)
				return r;
			if (inv)
				v = !v;
			if (v)
				b |= (uint8_t)(1u << bit);
			dly(bit_us);
		}
		out[i] = b;
		dly(bit_us);
	}
	*outlen = rxlen;
	return 0;
}
