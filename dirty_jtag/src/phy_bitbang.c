/* SPDX-License-Identifier: MIT */
#include "djprog/hw.h"
#include "djprog/phy.h"
#include <errno.h>
#include <string.h>

static void dly(uint32_t us) {
	if (dj_hw() && dj_hw()->delay_us)
		dj_hw()->delay_us(us ? us : 1);
}
static uint32_t half_period(uint32_t hz) {
	if (!hz)
		hz = 100000u;
	uint32_t x = 500000u / hz;
	return x ? x : 1u;
}
static unsigned parity8(uint8_t x) {
	x ^= x >> 4;
	x ^= x >> 2;
	x ^= x >> 1;
	return x & 1u;
}

int dj_spi_xfer(const uint8_t *tx, uint8_t *rx, size_t n, uint32_t hz) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	uint32_t half = half_period(hz);
	h->dir(DJ_PIN_CLK, DJ_DIR_OUTPUT);
	h->dir(DJ_PIN_DATA1, DJ_DIR_OUTPUT);
	h->dir(DJ_PIN_DATA2, DJ_DIR_INPUT);
	for (size_t i = 0; i < n; i++) {
		uint8_t r = 0, t = tx ? tx[i] : 0xff;
		for (int b = 7; b >= 0; b--) {
			h->write(DJ_PIN_CLK, false);
			h->write(DJ_PIN_DATA1, ((t >> b) & 1u) != 0);
			dly(half);
			h->write(DJ_PIN_CLK, true);
			bool v = false;
			h->read(DJ_PIN_DATA2, &v);
			r = (uint8_t)((r << 1) | (v ? 1u : 0u));
			dly(half);
		}
		if (rx)
			rx[i] = r;
	}
	h->write(DJ_PIN_CLK, false);
	return 0;
}

static int uart_send_bit(const struct dj_hw_ops *h, bool bit, bool invert, uint32_t period) {
	int r = h->write(DJ_PIN_DATA0, invert ? !bit : bit);
	if (r)
		return r;
	dly(period);
	return 0;
}

int dj_uart_1wire_txrx_ex(const uint8_t *tx, size_t tn, uint8_t *rx, size_t rn, uint32_t baud,
                          bool inv, enum dj_uart_parity parity, unsigned stop_bits) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h || !baud || stop_bits < 1 || stop_bits > 2)
		return -EINVAL;
	uint32_t bit = (1000000u + baud / 2u) / baud;
	if (!bit)
		bit = 1;
	int r = h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT);
	if (r)
		return r;
	r = h->write(DJ_PIN_DATA0, inv ? false : true);
	if (r)
		return r; /* logical idle = 1 */
	for (size_t i = 0; i < tn; i++) {
		if ((r = uart_send_bit(h, false, inv, bit)))
			return r;
		for (unsigned b = 0; b < 8; b++)
			if ((r = uart_send_bit(h, ((tx[i] >> b) & 1u) != 0, inv, bit)))
				return r;
		if (parity != DJ_PARITY_NONE) {
			bool p = (parity8(tx[i]) != 0); /* XOR of data bits gives even-parity bit. */
			if (parity == DJ_PARITY_ODD)
				p = !p;
			if ((r = uart_send_bit(h, p, inv, bit)))
				return r;
		}
		for (unsigned s = 0; s < stop_bits; s++)
			if ((r = uart_send_bit(h, true, inv, bit)))
				return r;
	}
	if (rn == 0)
		return h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
	r = h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
	if (r)
		return r;
	for (size_t i = 0; i < rn; i++) {
		uint32_t wait = bit * 30u;
		bool physical = true, found_start = false;
		while (wait > 0) {
			if ((r = h->read(DJ_PIN_DATA0, &physical)))
				return r;
			bool logical = inv ? !physical : physical;
			if (!logical) {
				found_start = true;
				break;
			}
			--wait;
			dly(1);
		}
		if (!found_start)
			return -ETIMEDOUT;
		dly(bit + bit / 2u); /* center of D0 */
		uint8_t x = 0;
		for (unsigned b = 0; b < 8; b++) {
			if ((r = h->read(DJ_PIN_DATA0, &physical)))
				return r;
			bool logical = inv ? !physical : physical;
			if (logical)
				x |= (uint8_t)(1u << b);
			dly(bit);
		}
		if (parity != DJ_PARITY_NONE) {
			if ((r = h->read(DJ_PIN_DATA0, &physical)))
				return r;
			bool p = inv ? !physical : physical;
			bool want = (parity8(x) != 0);
			if (parity == DJ_PARITY_ODD)
				want = !want;
			if (p != want)
				return -EBADMSG;
			dly(bit);
		}
		for (unsigned s = 0; s < stop_bits; s++) {
			if ((r = h->read(DJ_PIN_DATA0, &physical)))
				return r;
			bool logical = inv ? !physical : physical;
			if (!logical)
				return -EIO;
			dly(bit);
		}
		if (rx)
			rx[i] = x;
	}
	return 0;
}

int dj_uart_1wire_txrx(const uint8_t *tx, size_t txlen, uint8_t *rx, size_t rxlen, uint32_t baud,
                       bool invert) {
	return dj_uart_1wire_txrx_ex(tx, txlen, rx, rxlen, baud, invert, DJ_PARITY_NONE, 1);
}

static int sync_bit(bool output, bool value, bool *sample, uint32_t half) {
	const struct dj_hw_ops *h = dj_hw();
	int r;
	if (output) {
		if ((r = h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT)))
			return r;
		if ((r = h->write(DJ_PIN_DATA0, value)))
			return r;
	} else if ((r = h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT)))
		return r;
	if ((r = h->write(DJ_PIN_CLK, false)))
		return r;
	dly(half);
	if ((r = h->write(DJ_PIN_CLK, true)))
		return r;
	if (sample) {
		bool b = false;
		if ((r = h->read(DJ_PIN_DATA0, &b)))
			return r;
		*sample = b;
	}
	dly(half);
	return 0;
}

static int sync_send_8e2(uint8_t x, uint32_t half) {
	int r = sync_bit(true, false, NULL, half);
	if (r)
		return r; /* start */
	for (unsigned b = 0; b < 8; b++) {
		r = sync_bit(true, ((x >> b) & 1u) != 0, NULL, half);
		if (r)
			return r;
	}
	r = sync_bit(true, parity8(x) != 0, NULL, half);
	if (r)
		return r;
	r = sync_bit(true, true, NULL, half);
	if (r)
		return r;
	return sync_bit(true, true, NULL, half);
}
static int sync_recv_8e2(uint8_t *x, uint32_t half) {
	if (!x)
		return -EINVAL;
	bool b = false;
	int r;
	r = sync_bit(false, false, &b, half);
	if (r)
		return r;
	if (b)
		return -EIO; /* start */
	uint8_t v = 0;
	for (unsigned i = 0; i < 8; i++) {
		r = sync_bit(false, false, &b, half);
		if (r)
			return r;
		if (b)
			v |= (uint8_t)(1u << i);
	}
	r = sync_bit(false, false, &b, half);
	if (r)
		return r;
	if (b != (parity8(v) != 0))
		return -EBADMSG;
	r = sync_bit(false, false, &b, half);
	if (r)
		return r;
	if (!b)
		return -EIO;
	r = sync_bit(false, false, &b, half);
	if (r)
		return r;
	if (!b)
		return -EIO;
	*x = v;
	return 0;
}
int dj_sync_1wire_txrx_8e2(const uint8_t *tx, size_t tn, uint8_t *rx, size_t rn, uint32_t hz) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	uint32_t half = half_period(hz);
	int r = h->dir(DJ_PIN_CLK, DJ_DIR_OUTPUT);
	if (r)
		return r;
	h->write(DJ_PIN_CLK, false);
	for (size_t i = 0; i < tn; i++) {
		r = sync_send_8e2(tx[i], half);
		if (r)
			return r;
	}
	for (size_t i = 0; i < rn; i++) {
		r = sync_recv_8e2(&rx[i], half);
		if (r)
			return r;
	}
	h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
	h->write(DJ_PIN_CLK, false);
	return 0;
}
int dj_sync_1wire_break(uint32_t hz, unsigned bit_times) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h || bit_times < 12)
		return -EINVAL;
	uint32_t half = half_period(hz);
	int r;
	for (unsigned i = 0; i < bit_times; i++) {
		r = sync_bit(true, false, NULL, half);
		if (r)
			return r;
	}
	r = sync_bit(true, true, NULL, half);
	h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
	return r;
}

static void c2strobe(void) {
	const struct dj_hw_ops *h = dj_hw();
	h->write(DJ_PIN_CLK, false);
	dly(1);
	h->write(DJ_PIN_CLK, true);
	dly(1);
}
int dj_c2_addr_write(uint8_t a) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT);
	h->write(DJ_PIN_CLK, true);
	h->write(DJ_PIN_DATA0, true);
	c2strobe();
	h->write(DJ_PIN_DATA0, true);
	c2strobe();
	for (int i = 0; i < 8; i++) {
		h->write(DJ_PIN_DATA0, ((a >> i) & 1u) != 0);
		c2strobe();
	}
	h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
	return 0;
}
int dj_c2_addr_read(uint8_t *a) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h || !a)
		return -EINVAL;
	h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT);
	h->write(DJ_PIN_DATA0, true);
	c2strobe();
	h->write(DJ_PIN_DATA0, false);
	c2strobe();
	h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
	uint8_t x = 0;
	for (int i = 0; i < 8; i++) {
		bool v = false;
		c2strobe();
		h->read(DJ_PIN_DATA0, &v);
		if (v)
			x |= (uint8_t)(1u << i);
	}
	*a = x;
	return 0;
}
int dj_c2_data_write(uint8_t v) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT);
	h->write(DJ_PIN_DATA0, false);
	c2strobe();
	h->write(DJ_PIN_DATA0, true);
	c2strobe();
	for (int i = 0; i < 8; i++) {
		h->write(DJ_PIN_DATA0, ((v >> i) & 1u) != 0);
		c2strobe();
	}
	h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
	return 0;
}
int dj_c2_data_read(uint8_t *v) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h || !v)
		return -EINVAL;
	h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT);
	h->write(DJ_PIN_DATA0, false);
	c2strobe();
	h->write(DJ_PIN_DATA0, false);
	c2strobe();
	h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
	for (int t = 0; t < 100; t++) {
		bool ack = false;
		c2strobe();
		h->read(DJ_PIN_DATA0, &ack);
		if (!ack)
			break;
		if (t == 99)
			return -ETIMEDOUT;
	}
	uint8_t x = 0;
	for (int i = 0; i < 8; i++) {
		bool b = false;
		c2strobe();
		h->read(DJ_PIN_DATA0, &b);
		if (b)
			x |= (uint8_t)(1u << i);
	}
	*v = x;
	return 0;
}

int dj_swd_sequence(const uint8_t *tx, size_t tb, uint8_t *rx, size_t rb, uint32_t hz) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	uint32_t half = half_period(hz);
	h->dir(DJ_PIN_CLK, DJ_DIR_OUTPUT);
	h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT);
	for (size_t i = 0; i < tb; i++) {
		h->write(DJ_PIN_CLK, false);
		h->write(DJ_PIN_DATA0, ((tx[i / 8] >> (i & 7)) & 1u) != 0);
		dly(half);
		h->write(DJ_PIN_CLK, true);
		dly(half);
	}
	h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT);
	if (rx)
		memset(rx, 0, (rb + 7) / 8);
	for (size_t i = 0; i < rb; i++) {
		bool b = false;
		h->write(DJ_PIN_CLK, false);
		dly(half);
		h->write(DJ_PIN_CLK, true);
		h->read(DJ_PIN_DATA0, &b);
		if (rx && b)
			rx[i / 8] |= (uint8_t)(1u << (i & 7));
		dly(half);
	}
	return 0;
}
int dj_jtag_shift(const uint8_t *tx, size_t bits, uint8_t *rx, uint32_t hz) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	uint32_t half = half_period(hz);
	h->dir(DJ_PIN_CLK, DJ_DIR_OUTPUT);
	h->dir(DJ_PIN_DATA1, DJ_DIR_OUTPUT);
	h->dir(DJ_PIN_DATA2, DJ_DIR_INPUT);
	if (rx)
		memset(rx, 0, (bits + 7) / 8);
	for (size_t i = 0; i < bits; i++) {
		h->write(DJ_PIN_CLK, false);
		h->write(DJ_PIN_DATA1, ((tx[i / 8] >> (i & 7)) & 1u) != 0);
		dly(half);
		bool b = false;
		h->read(DJ_PIN_DATA2, &b);
		h->write(DJ_PIN_CLK, true);
		if (rx && b)
			rx[i / 8] |= (uint8_t)(1u << (i & 7));
		dly(half);
	}
	return 0;
}
