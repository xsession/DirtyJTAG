/* SPDX-License-Identifier: MIT */
#include "djprog/dspic_debug.h"
#include "djpk4/device.h"
#include "djpk4/dspic_common.h"
#include "djpk4/icsp.h"
#include "djprog/backend.h"
#include <errno.h>
#include <string.h>

/* Clean-room dsPIC debug control plane.
 *
 * PIC/dsPIC debugging is mediated by a target-resident Debug Executive (DE).
 * Microchip documents the concept/resource reservation publicly, but the exact
 * DE command ABI and binaries are not redistributed here.  This layer therefore
 * supports:
 *   1) mock-dspic targets for CI and host/IDE integration development;
 *   2) a small run-time "debug capsule" descriptor loaded by the host after it
 *      locates local DFP/MPLAB assets.  The capsule unlocks the DJP2 debug API
 *      and may later be extended with per-family transaction scripts.
 *
 * Until a capsule provides real DE transaction scripts, real target run-control
 * returns -ENOTSUP.  That is deliberate; programming ICSP is not the same as a
 * safe CPU debugger.
 */
#define CAPS_MAGIC 0x4544444Au /* JDDE little-endian */
#define CAPS_VERSION 1u
#define CAPS_FLAG_HAS_ATTACH_SCRIPT (1u << 0)
#define CAPS_FLAG_HAS_RUNCTRL (1u << 1)
#define CAPS_FLAG_HAS_REGS (1u << 2)
#define CAPS_FLAG_HAS_BREAKPOINTS (1u << 3)

struct capsule {
	bool loaded;
	uint8_t family;
	uint8_t hw_breakpoints;
	uint8_t register_bytes;
	uint32_t flags;
};

static struct capsule cap;
static bool attached;
static bool halted;
static bool mock;
static uint8_t mock_regs[DJ_DSPIC_DEBUG_REG_BYTES];
static uint32_t mock_bp[4];

static uint32_t le32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool current_is_mock(void) {
	const struct dj_target_cfg *cfg = dj_target_config();
	return cfg && strncmp(cfg->device, "mock-dspic", 10u) == 0;
}

static int require_real_caps(uint32_t need) {
	if (mock)
		return 0;
	if (!cap.loaded)
		return -ENOTSUP;
	if ((cap.flags & need) != need)
		return -ENOTSUP;
	return 0;
}

void dj_dspic_debug_backend_changed(void) {
	attached = false;
	halted = false;
	mock = current_is_mock();
	if (mock) {
		memset(mock_regs, 0, sizeof(mock_regs));
		memset(mock_bp, 0, sizeof(mock_bp));
		mock_regs[32] = 0x00; /* PC low/mid/high begin at 32 */
		mock_regs[33] = 0x01;
		mock_regs[34] = 0x00;
	}
}

int dj_dspic_debug_load_capsule(const uint8_t *data, size_t len) {
	if (!data || len < 16u)
		return -EINVAL;
	if (le32(data + 0) != CAPS_MAGIC)
		return -EBADMSG;
	if (data[4] != CAPS_VERSION)
		return -ENOTSUP;
	if (data[7] > 8u || data[6] > DJ_DSPIC_DEBUG_REG_BYTES)
		return -ERANGE;
	cap.loaded = true;
	cap.family = data[5];
	cap.register_bytes = data[6] ? data[6] : DJ_DSPIC_DEBUG_REG_BYTES;
	cap.hw_breakpoints = data[7];
	cap.flags = le32(data + 8);
	return 0;
}

int dj_dspic_debug_capsule_info(struct dj_dspic_debug_capsule_info *info) {
	if (!info)
		return -EINVAL;
	info->loaded = cap.loaded ? 1u : 0u;
	info->family = cap.family;
	info->hw_breakpoints = cap.hw_breakpoints;
	info->register_bytes = cap.register_bytes;
	info->flags = cap.flags;
	return 0;
}

int dj_dspic_debug_attach(void) {
	mock = current_is_mock();
	int r = require_real_caps(CAPS_FLAG_HAS_ATTACH_SCRIPT);
	if (r)
		return r;
	/* Real DE attach scripts are intentionally not guessed.  A later capsule
	 * revision can embed audited SIX/REGOUT transaction scripts generated from
	 * local DFP/MPLAB metadata. */
	if (!mock && !(cap.flags & CAPS_FLAG_HAS_ATTACH_SCRIPT))
		return -ENOTSUP;
	attached = true;
	halted = mock;
	return 0;
}

int dj_dspic_debug_detach(void) {
	attached = false;
	halted = false;
	return 0;
}

int dj_dspic_debug_status(bool *is_halted) {
	if (!attached)
		return -ENODEV;
	if (is_halted)
		*is_halted = halted;
	return 0;
}

int dj_dspic_debug_halt(void) {
	if (!attached)
		return -ENODEV;
	int r = require_real_caps(CAPS_FLAG_HAS_RUNCTRL);
	if (r)
		return r;
	halted = true;
	return 0;
}

int dj_dspic_debug_run(void) {
	if (!attached)
		return -ENODEV;
	int r = require_real_caps(CAPS_FLAG_HAS_RUNCTRL);
	if (r)
		return r;
	halted = false;
	return 0;
}

int dj_dspic_debug_step(void) {
	if (!attached)
		return -ENODEV;
	int r = require_real_caps(CAPS_FLAG_HAS_RUNCTRL);
	if (r)
		return r;
	halted = true;
	return 0;
}

int dj_dspic_debug_reset(bool halt_after_reset) {
	int r;
	if (mock) {
		attached = true;
		halted = halt_after_reset;
		return 0;
	}
	r = require_real_caps(CAPS_FLAG_HAS_RUNCTRL);
	if (r)
		return r;
	if (dj_icsp_hal() && dj_icsp_hal()->set_mclr_low) {
		r = dj_icsp_hal()->set_mclr_low(true);
		if (r)
			return r;
		dj_icsp_hal()->delay_us(1000);
		r = dj_icsp_hal()->set_mclr_low(false);
		if (r)
			return r;
	}
	halted = halt_after_reset;
	attached = true;
	return 0;
}

int dj_dspic_debug_read_regs(uint8_t *out, size_t *len) {
	if (!out || !len)
		return -EINVAL;
	if (!attached)
		return -ENODEV;
	int r = require_real_caps(CAPS_FLAG_HAS_REGS);
	if (r)
		return r;
	size_t n = mock ? sizeof(mock_regs)
	                : (cap.register_bytes ? cap.register_bytes : DJ_DSPIC_DEBUG_REG_BYTES);
	if (*len < n)
		return -ENOSPC;
	if (mock)
		memcpy(out, mock_regs, n);
	else
		memset(out, 0, n); /* reserved until DE register script is supplied */
	*len = n;
	return 0;
}

int dj_dspic_debug_write_regs(const uint8_t *in, size_t len) {
	if (!in)
		return -EINVAL;
	if (!attached)
		return -ENODEV;
	int r = require_real_caps(CAPS_FLAG_HAS_REGS);
	if (r)
		return r;
	if (mock) {
		if (len != sizeof(mock_regs))
			return -EINVAL;
		memcpy(mock_regs, in, len);
		return 0;
	}
	return -ENOTSUP;
}

int dj_dspic_debug_set_breakpoint(uint32_t address, uint8_t type, uint8_t slot) {
	(void)type;
	if (!attached)
		return -ENODEV;
	int r = require_real_caps(CAPS_FLAG_HAS_BREAKPOINTS);
	if (r)
		return r;
	uint8_t maxbp = mock ? 4u : cap.hw_breakpoints;
	if (slot >= maxbp)
		return -ERANGE;
	if (mock) {
		mock_bp[slot] = address;
		return 0;
	}
	return -ENOTSUP;
}

int dj_dspic_debug_clear_breakpoint(uint8_t slot) {
	if (!attached)
		return -ENODEV;
	int r = require_real_caps(CAPS_FLAG_HAS_BREAKPOINTS);
	if (r)
		return r;
	uint8_t maxbp = mock ? 4u : cap.hw_breakpoints;
	if (slot >= maxbp)
		return -ERANGE;
	if (mock) {
		mock_bp[slot] = 0;
		return 0;
	}
	return -ENOTSUP;
}
