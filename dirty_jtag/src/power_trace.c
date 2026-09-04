/* SPDX-License-Identifier: MIT */
#include "djprog/power_trace.h"
#include "djprog/common.h"
#include "djprog/hw.h"
#include <errno.h>

int dj_power_trace_capture(const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen) {
	if (!in || !out || !outlen || inlen < 8)
		return -EINVAL;
	const struct dj_hw_ops *h = dj_hw();
	if (!h || !h->measure)
		return -ENOTSUP;
	uint16_t samples = dj_le16(in + 0);
	uint16_t interval_ms = dj_le16(in + 2);
	uint32_t flags = dj_le32(in + 4);
	(void)flags;
	if (!samples)
		samples = 1;
	size_t need = (size_t)samples * 16u;
	if (need > *outlen)
		return -ENOSPC;
	for (uint16_t i = 0; i < samples; ++i) {
		struct dj_measurement m = {0};
		int r = h->measure(&m);
		if (r)
			return r;
		uint32_t cur = m.itarget_ma | (m.power_fault ? 0x80000000u : 0u);
		dj_put_le32(out + 16u * i + 0, (uint32_t)i * (uint32_t)interval_ms);
		dj_put_le32(out + 16u * i + 4, m.vtarget_mv);
		dj_put_le32(out + 16u * i + 8, m.vpp_mv);
		dj_put_le32(out + 16u * i + 12, cur);
		if (interval_ms && i + 1u < samples && h->delay_us)
			h->delay_us((uint32_t)interval_ms * 1000u);
	}
	*outlen = need;
	return 0;
}
