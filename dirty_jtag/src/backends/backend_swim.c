/* SPDX-License-Identifier: MIT */
#include "djprog/backend.h"
#include "djprog/hw.h"
#include "djprog/swim.h"
#include <errno.h>
#include <string.h>

static struct dj_target_cfg cfg;

static int select_swim(const struct dj_target_cfg *c) {
	if (!c)
		return -EINVAL;
	cfg = *c;
	bool mock = strncmp(cfg.device, "mock-stm8", 9u) == 0;
	return dj_swim_select(cfg.clock_hz ? cfg.clock_hz : 363000u, mock);
}

static int enter_swim(void) {
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	if (h->power) {
		int r = h->power(cfg.power);
		if (r)
			return r;
	}
	return dj_swim_enter();
}

static int leave_swim(void) {
	return dj_swim_leave();
}

static int identify(uint8_t *out, size_t *len) {
	if (!out || !len || *len < 2u)
		return -EINVAL;
	/* Most STM8 devices expose a device ID in this area; exact family decoding
	 * remains host-side/table driven. */
	int r = dj_swim_read_mem(0x00487Eu, out, 2u);
	if (!r)
		*len = 2u;
	return r;
}

static int read_mem(uint32_t addr, uint8_t *data, size_t len) {
	return dj_swim_read_mem(addr, data, len);
}

static int write_mem(uint32_t addr, const uint8_t *data, size_t len) {
	return dj_swim_write_mem(addr, data, len);
}

/* Raw SWIM is now a transport-level operation:
 *   op 0x01: read  [01 addr24 len16] -> data
 *   op 0x02: write [02 addr24 len16 data...] -> empty
 *   op 0x03: srst  [03] -> empty
 * Legacy pulse-triplet raw mode is intentionally retired from the high-level
 * backend because it bypassed protocol acknowledgements and could not debug. */
static int raw(const uint8_t *tx, size_t n, uint8_t *rx, size_t *rn) {
	if (!tx || !rn || n < 1u)
		return -EINVAL;
	if (tx[0] == 0x03u) {
		*rn = 0;
		return dj_swim_system_reset();
	}
	if (n < 6u)
		return -EINVAL;
	uint32_t addr = ((uint32_t)tx[1] << 16) | ((uint32_t)tx[2] << 8) | tx[3];
	uint16_t len = (uint16_t)tx[4] | ((uint16_t)tx[5] << 8);
	if (tx[0] == 0x01u) {
		if (!rx || *rn < len)
			return -ENOSPC;
		int r = dj_swim_read_mem(addr, rx, len);
		if (!r)
			*rn = len;
		return r;
	}
	if (tx[0] == 0x02u) {
		if (n != 6u + len)
			return -EINVAL;
		*rn = 0;
		return dj_swim_write_mem(addr, tx + 6, len);
	}
	return -EINVAL;
}

const struct dj_backend dj_backend_swim = {
    DJ_PROTO_STM8_SWIM,
    "stm8-swim",
    DJ_CAP_IDENTIFY | DJ_CAP_READ | DJ_CAP_WRITE | DJ_CAP_RAW_XFER | DJ_CAP_DEBUG_PHY |
        DJ_CAP_DEBUG_RUNCTRL | DJ_CAP_DEBUG_REGS | DJ_CAP_DEBUG_BREAK | DJ_CAP_EXPERIMENTAL,
    363000,
    8000000,
    select_swim,
    enter_swim,
    leave_swim,
    identify,
    0,
    read_mem,
    write_mem,
    raw};
