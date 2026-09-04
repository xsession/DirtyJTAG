/* SPDX-License-Identifier: MIT */
#include "djprog/debug_bitbang.h"
#include "djprog/backend.h"
#include "djprog/hw.h"
#include <errno.h>
#include <stdbool.h>

static int sample_pin(enum dj_pin_role role, uint8_t *samples, size_t cap, size_t *used) {
	if (*used >= cap)
		return -ENOSPC;
	bool v = false;
	int r = dj_hw()->read(role, &v);
	if (r)
		return r;
	samples[(*used)++] = v ? '1' : '0';
	return 0;
}

static int set_reset(bool trst, bool srst, bool jtag) {
	const struct dj_hw_ops *h = dj_hw();
	int r;
	/* RESET is an open-drain sink: true=released, false=asserted. */
	r = h->write(DJ_PIN_RESET, !srst);
	if (r)
		return r;
	if (!jtag)
		return 0;

	/* AUX is used as nTRST for the OpenOCD bridge.  Prefer open-drain style. */
	if (trst) {
		r = h->write(DJ_PIN_AUX, false);
		if (r)
			return r;
		return h->dir(DJ_PIN_AUX, DJ_DIR_OUTPUT);
	}
	return h->dir(DJ_PIN_AUX, DJ_DIR_INPUT);
}

int dj_debug_remote_bitbang(const uint8_t *ops, size_t op_len, uint8_t *samples,
                            size_t *sample_len) {
	if (!ops || !sample_len)
		return -EINVAL;
	const struct dj_hw_ops *h = dj_hw();
	const struct dj_backend *b = dj_selected_backend();
	if (!h || !b)
		return -ENODEV;
	if (b->id != DJ_PROTO_ARM_SWD && b->id != DJ_PROTO_JTAG && b->id != DJ_PROTO_TMS320_C2000_JTAG)
		return -ENOTSUP;

	const bool jtag = (b->id == DJ_PROTO_JTAG || b->id == DJ_PROTO_TMS320_C2000_JTAG);
	const size_t cap = *sample_len;
	size_t used = 0;
	bool swdio_output = true;
	int r = 0;

	if (jtag) {
		if ((r = h->dir(DJ_PIN_CLK, DJ_DIR_OUTPUT)))
			return r;
		if ((r = h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT)))
			return r; /* TMS */
		if ((r = h->dir(DJ_PIN_DATA1, DJ_DIR_OUTPUT)))
			return r; /* TDI */
		if ((r = h->dir(DJ_PIN_DATA2, DJ_DIR_INPUT)))
			return r; /* TDO */
	} else {
		if ((r = h->dir(DJ_PIN_CLK, DJ_DIR_OUTPUT)))
			return r;
		if ((r = h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT)))
			return r;
	}

	for (size_t i = 0; i < op_len; ++i) {
		const uint8_t c = ops[i];
		if (c == 'B' || c == 'b')
			continue; /* optional activity LED */
		if (c == 'Q')
			continue; /* socket close handled by host bridge */

		if (c >= 'r' && c <= 'u') {
			const unsigned v = (unsigned)(c - 'r');
			/* r=00, s=01, t=10, u=11 => bit1 TRST, bit0 SRST. */
			if ((r = set_reset((v & 2u) != 0, (v & 1u) != 0, jtag)))
				return r;
			continue;
		}
		if (c == 'Z') {
			h->delay_us(1000);
			continue;
		}
		if (c == 'z') {
			h->delay_us(1);
			continue;
		}

		if (jtag) {
			if (c >= '0' && c <= '7') {
				const unsigned v = (unsigned)(c - '0');
				if ((r = h->write(DJ_PIN_CLK, (v & 4u) != 0)))
					return r;
				if ((r = h->write(DJ_PIN_DATA0, (v & 2u) != 0)))
					return r;
				if ((r = h->write(DJ_PIN_DATA1, (v & 1u) != 0)))
					return r;
				continue;
			}
			if (c == 'R') {
				if ((r = sample_pin(DJ_PIN_DATA2, samples, cap, &used)))
					return r;
				continue;
			}
			return -EINVAL;
		}

		/* SWD additions used by modern OpenOCD remote_bitbang. */
		if (c == 'O') {
			swdio_output = true;
			if ((r = h->dir(DJ_PIN_DATA0, DJ_DIR_OUTPUT)))
				return r;
			continue;
		}
		if (c == 'o') {
			swdio_output = false;
			if ((r = h->dir(DJ_PIN_DATA0, DJ_DIR_INPUT)))
				return r;
			continue;
		}
		if (c == 'c') {
			if ((r = sample_pin(DJ_PIN_DATA0, samples, cap, &used)))
				return r;
			continue;
		}
		if (c >= 'd' && c <= 'g') {
			const unsigned v = (unsigned)(c - 'd');
			if ((r = h->write(DJ_PIN_CLK, (v & 2u) != 0)))
				return r;
			if (swdio_output && (r = h->write(DJ_PIN_DATA0, (v & 1u) != 0)))
				return r;
			continue;
		}
		return -EINVAL;
	}

	*sample_len = used;
	return 0;
}
