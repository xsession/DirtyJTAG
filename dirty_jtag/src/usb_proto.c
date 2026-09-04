/* SPDX-License-Identifier: MIT */
#include "djprog/usb_proto.h"
#include "djpk4/device.h"
#include "djprog/backend.h"
#include "djprog/bridge.h"
#include "djprog/common.h"
#include "djprog/debug.h"
#include "djprog/debug_bitbang.h"
#include "djprog/hw.h"
#include "djprog/power_trace.h"
#include "djprog/rtt.h"
#include "djprog/safety.h"
#include "djprog/script_vm.h"
#include "djprog/swim_phy.h"
#include "djprog/swo.h"
#include <errno.h>
#include <string.h>

/* Decode and validate DJP2 before dispatch so malformed frames cannot reach a
 * backend or alter target power state. */
size_t djp2_encode(const struct djp2_frame *f, uint8_t *out, size_t cap) {
	if (!f || !out || f->len > DJP2_MAX_PAYLOAD || cap < DJP2_HDR_SIZE + f->len)
		return 0;
	dj_put_le32(out, DJP2_MAGIC);
	dj_put_le16(out + 4, DJP2_VERSION);
	dj_put_le16(out + 6, f->cmd);
	dj_put_le16(out + 8, f->seq);
	dj_put_le16(out + 10, f->status);
	dj_put_le16(out + 12, f->flags);
	dj_put_le32(out + 14, f->len);
	dj_put_le16(out + 18, 0);
	if (f->len)
		memcpy(out + 20, f->payload, f->len);
	uint32_t crc = dj_crc32(out, 18, 0);
	crc = dj_crc32(f->payload, f->len, crc);
	dj_put_le16(out + 18, (uint16_t)(crc ^ (crc >> 16)));
	return 20 + f->len;
}

int djp2_decode(const uint8_t *b, size_t n, struct djp2_frame *f) {
	if (!b || !f || n < 20 || dj_le32(b) != DJP2_MAGIC || dj_le16(b + 4) != DJP2_VERSION)
		return -EINVAL;
	uint32_t l = dj_le32(b + 14);
	if (l > DJP2_MAX_PAYLOAD || n != 20 + l)
		return -EMSGSIZE;
	uint16_t got = dj_le16(b + 18);
	uint8_t h[18];
	memcpy(h, b, 18);
	uint32_t crc = dj_crc32(h, 18, 0);
	crc = dj_crc32(b + 20, l, crc);
	if (got != (uint16_t)(crc ^ (crc >> 16)))
		return -EBADMSG;
	memset(f, 0, sizeof(*f));
	f->cmd = dj_le16(b + 6);
	f->seq = dj_le16(b + 8);
	f->status = dj_le16(b + 10);
	f->flags = dj_le16(b + 12);
	f->len = l;
	if (l)
		memcpy(f->payload, b + 20, l);
	return 0;
}

static uint16_t maperr(int r) {
	if (r == 0)
		return DJP2_OK;
	if (r == -EINVAL)
		return DJP2_E_ARG;
	if (r == -ENOTSUP || r == -ENOSYS || r == -ENODEV)
		return DJP2_E_UNSUPPORTED;
	if (r == -ERANGE || r == -EMSGSIZE)
		return DJP2_E_RANGE;
	if (r == -EBUSY)
		return DJP2_E_BUSY;
	if (r == -EPERM || r == -ETIMEDOUT)
		return DJP2_E_POWER;
	return DJP2_E_IO;
}

static int emit_measurement(uint8_t *p, size_t cap, size_t *len) {
	if (!p || !len || cap < 16)
		return -ENOSPC;
	if (!dj_hw() || !dj_hw()->measure)
		return -ENOTSUP;
	struct dj_measurement m = {0};
	int rc = dj_hw()->measure(&m);
	if (rc)
		return rc;
	dj_put_le32(p + 0, m.vtarget_mv);
	dj_put_le32(p + 4, m.vpp_mv);
	dj_put_le32(p + 8, m.itarget_ma);
	dj_put_le32(p + 12, m.power_fault ? 1u : 0u);
	*len = 16;
	return 0;
}

int djp2_dispatch(const struct djp2_frame *q, struct djp2_frame *r) {
	if (!q || !r)
		return -EINVAL;
	memset(r, 0, sizeof(*r));
	r->cmd = q->cmd;
	r->seq = q->seq;
	int rc = 0;
	const struct dj_backend *b = dj_selected_backend();

	switch (q->cmd) {
	case DJP2_HELLO: {
		const char *s = "DirtyJTAG Universal "
		                "Programmer+Debugger;proto=2;board=rpi_pico;power=tps630702+tps2553";
		r->len = (uint32_t)strlen(s);
		memcpy(r->payload, s, r->len);
		break;
	}
	case DJP2_LIST_PROTOCOLS: {
		size_t p = 0;
		for (size_t i = 0; i < dj_backend_count(); ++i) {
			const struct dj_backend *x = dj_backend_at(i);
			size_t nl = strlen(x->name);
			if (p + 12 + nl > DJP2_MAX_PAYLOAD)
				break;
			dj_put_le16(r->payload + p, (uint16_t)x->id);
			dj_put_le16(r->payload + p + 2, (uint16_t)nl);
			dj_put_le32(r->payload + p + 4, x->caps);
			dj_put_le32(r->payload + p + 8, x->max_hz);
			memcpy(r->payload + p + 12, x->name, nl);
			p += 12 + nl;
		}
		r->len = (uint32_t)p;
		break;
	}
	case DJP2_LIST_DEVICES: {
		size_t p = 0;
		for (size_t i = 0; i < dj_device_count(); ++i) {
			const struct dj_device *d = dj_device_at(i);
			size_t nl = strlen(d->name);
			/* family:u8 name_len:u8 row:u16 page:u16 flags:u32 user_end:u32 name */
			if (nl > 255 || p + 14 + nl > DJP2_MAX_PAYLOAD)
				break;
			r->payload[p + 0] = (uint8_t)d->family;
			r->payload[p + 1] = (uint8_t)nl;
			dj_put_le16(r->payload + p + 2, d->row_words);
			dj_put_le16(r->payload + p + 4, d->page_words);
			dj_put_le32(r->payload + p + 6, d->flags);
			dj_put_le32(r->payload + p + 10, d->user_end_pc);
			memcpy(r->payload + p + 14, d->name, nl);
			p += 14 + nl;
		}
		r->len = (uint32_t)p;
		break;
	}
	case DJP2_CONFIG: {
		if (q->len < 19) {
			rc = -EINVAL;
			break;
		}
		struct dj_target_cfg c = {0};
		c.proto = (enum dj_proto_id)dj_le16(q->payload);
		c.power = (enum dj_power_mode)q->payload[2];
		c.flags = q->payload[3];
		c.clock_hz = dj_le32(q->payload + 4);
		c.vtarget_mv = dj_le32(q->payload + 8);
		c.flash_size = dj_le32(q->payload + 12);
		c.page_size = dj_le16(q->payload + 16);
		size_t nl = q->payload[18];
		if (nl > sizeof(c.device) - 1 || 19 + nl > q->len) {
			rc = -EINVAL;
			break;
		}
		memcpy(c.device, q->payload + 19, nl);
		c.device[nl] = 0;
		rc = dj_select_backend(&c);
		break;
	}
	case DJP2_STATUS: {
		const struct dj_target_cfg *c = dj_target_config();
		struct dj_measurement m = {0};
		if (dj_hw() && dj_hw()->measure)
			(void)dj_hw()->measure(&m);
		size_t nl = (c && c->device[0]) ? strlen(c->device) : 0u;
		if (nl > 47u)
			nl = 47u;
		dj_put_le16(r->payload + 0, b ? (uint16_t)b->id : 0u);
		r->payload[2] = c ? (uint8_t)c->power : 0u;
		r->payload[3] = (uint8_t)nl;
		dj_put_le32(r->payload + 4, c ? c->clock_hz : 0u);
		dj_put_le32(r->payload + 8, b ? b->caps : 0u);
		dj_put_le32(r->payload + 12, m.vtarget_mv);
		dj_put_le32(r->payload + 16, m.vpp_mv);
		dj_put_le32(r->payload + 20, m.itarget_ma);
		dj_put_le32(r->payload + 24, m.power_fault ? 1u : 0u);
		dj_put_le32(r->payload + 28, c ? c->flash_size : 0u);
		dj_put_le16(r->payload + 32, c ? c->page_size : 0u);
		dj_put_le16(r->payload + 34, c ? c->flags : 0u);
		if (nl)
			memcpy(r->payload + 36, c->device, nl);
		r->len = (uint32_t)(36u + nl);
		break;
	}
	case DJP2_ENTER:
		rc = b && b->enter ? b->enter() : -ENOTSUP;
		break;
	case DJP2_LEAVE:
		rc = b && b->leave ? b->leave() : -ENOTSUP;
		break;
	case DJP2_IDENTIFY: {
		if (!b || !b->identify) {
			rc = -ENOTSUP;
			break;
		}
		size_t outn = DJP2_MAX_PAYLOAD;
		rc = b->identify(r->payload, &outn);
		if (!rc)
			r->len = (uint32_t)outn;
		break;
	}
	case DJP2_ERASE:
		rc = dj_safety_require(DJ_SAFETY_ERASE);
		if (!rc)
			rc = b && b->erase ? b->erase() : -ENOTSUP;
		break;
	case DJP2_READ: {
		if (!b || !b->read_mem || q->len < 8) {
			rc = -ENOTSUP;
			break;
		}
		uint32_t a = dj_le32(q->payload), n = dj_le32(q->payload + 4);
		if (n > DJP2_MAX_PAYLOAD) {
			rc = -ERANGE;
			break;
		}
		rc = b->read_mem(a, r->payload, n);
		if (!rc)
			r->len = n;
		break;
	}
	case DJP2_WRITE: {
		rc = dj_safety_require(DJ_SAFETY_WRITE);
		if (rc)
			break;
		if (!b || !b->write_mem || q->len < 8) {
			rc = -ENOTSUP;
			break;
		}
		uint32_t a = dj_le32(q->payload), n = dj_le32(q->payload + 4);
		if (n + 8u != q->len) {
			rc = -EINVAL;
			break;
		}
		rc = b->write_mem(a, q->payload + 8, n);
		break;
	}
	case DJP2_RAW_XFER: {
		if (!b || !b->raw_xfer) {
			rc = -ENOTSUP;
			break;
		}
		size_t outn = DJP2_MAX_PAYLOAD;
		rc = b->raw_xfer(q->payload, q->len, r->payload, &outn);
		if (!rc)
			r->len = (uint32_t)outn;
		break;
	}
	case DJP2_POWER:
		rc = dj_safety_require(DJ_SAFETY_POWER);
		if (rc)
			break;
		if (!dj_hw() || !dj_hw()->power || q->len < 1) {
			rc = -EINVAL;
			break;
		}
		rc = dj_hw()->power((enum dj_power_mode)q->payload[0]);
		break;
	case DJP2_MEASURE: {
		size_t n = 0;
		rc = emit_measurement(r->payload, DJP2_MAX_PAYLOAD, &n);
		if (!rc)
			r->len = (uint32_t)n;
		break;
	}
	case DJP2_SET_PINMAP: {
		if (q->len != DJ_PIN_COUNT) {
			rc = -EINVAL;
			break;
		}
		struct dj_pinmap pm = {0};
		for (unsigned i = 0; i < DJ_PIN_COUNT; ++i)
			pm.gpio[i] = q->payload[i];
		rc = dj_hw_set_pinmap(&pm);
		break;
	}
	case DJP2_VPP: {
		rc = dj_safety_require(DJ_SAFETY_VPP);
		if (rc)
			break;
		if (q->len < 2 || !dj_hw()) {
			rc = -EINVAL;
			break;
		}
		bool on = q->payload[1] != 0;
		if (q->payload[0] == 0)
			rc = dj_hw()->vpp_boost ? dj_hw()->vpp_boost(on) : -ENOTSUP;
		else if (q->payload[0] == 1)
			rc = dj_hw()->vpp_apply ? dj_hw()->vpp_apply(on) : -ENOTSUP;
		else if (q->payload[0] == 2)
			rc = dj_hw()->hv_data0_apply ? dj_hw()->hv_data0_apply(on) : -ENOTSUP;
		else
			rc = -EINVAL;
		break;
	}

	case DJP2_PHY_INFO: {
		const char *name = dj_swim_phy_name();
		size_t nl = name ? strlen(name) : 0u;
		if (nl > 63u)
			nl = 63u;
		r->payload[0] = dj_swim_phy_active() ? 1u : 0u;
		r->payload[1] = (uint8_t)nl;
		r->payload[2] = 0u;
		r->payload[3] = 0u;
		if (nl)
			memcpy(r->payload + 4, name, nl);
		r->len = (uint32_t)(4u + nl);
		break;
	}
	case DJP2_SCRIPT_XFER: {
		rc = dj_safety_require(DJ_SAFETY_SCRIPT);
		if (rc)
			break;
		size_t outn = DJP2_MAX_PAYLOAD;
		rc = dj_script_execute(q->payload, q->len, r->payload, &outn);
		if (!rc)
			r->len = (uint32_t)outn;
		break;
	}

	case DJP2_BRIDGE_INFO: {
		size_t outn = DJP2_MAX_PAYLOAD;
		rc = dj_bridge_info(r->payload, &outn);
		if (!rc)
			r->len = (uint32_t)outn;
		break;
	}
	case DJP2_BRIDGE_GPIO: {
		rc = dj_safety_require(DJ_SAFETY_BRIDGE);
		if (rc)
			break;
		size_t outn = DJP2_MAX_PAYLOAD;
		rc = dj_bridge_gpio(q->payload, q->len, r->payload, &outn);
		if (!rc)
			r->len = (uint32_t)outn;
		break;
	}
	case DJP2_BRIDGE_SPI: {
		rc = dj_safety_require(DJ_SAFETY_BRIDGE);
		if (rc)
			break;
		size_t outn = DJP2_MAX_PAYLOAD;
		rc = dj_bridge_spi(q->payload, q->len, r->payload, &outn);
		if (!rc)
			r->len = (uint32_t)outn;
		break;
	}
	case DJP2_BRIDGE_I2C: {
		rc = dj_safety_require(DJ_SAFETY_BRIDGE);
		if (rc)
			break;
		size_t outn = DJP2_MAX_PAYLOAD;
		rc = dj_bridge_i2c(q->payload, q->len, r->payload, &outn);
		if (!rc)
			r->len = (uint32_t)outn;
		break;
	}
	case DJP2_BRIDGE_UART: {
		rc = dj_safety_require(DJ_SAFETY_BRIDGE);
		if (rc)
			break;
		size_t outn = DJP2_MAX_PAYLOAD;
		rc = dj_bridge_uart(q->payload, q->len, r->payload, &outn);
		if (!rc)
			r->len = (uint32_t)outn;
		break;
	}
	case DJP2_POWER_TRACE: {
		size_t outn = DJP2_MAX_PAYLOAD;
		rc = dj_power_trace_capture(q->payload, q->len, r->payload, &outn);
		if (!rc)
			r->len = (uint32_t)outn;
		break;
	}
	case DJP2_SAFETY_STATUS: {
		struct dj_safety_state st = dj_safety_get();
		dj_put_le32(r->payload + 0, st.armed_flags);
		dj_put_le32(r->payload + 4, st.remaining_uses);
		r->len = 8;
		break;
	}
	case DJP2_SAFETY_ARM: {
		if (q->len < 9u) {
			rc = -EINVAL;
			break;
		}
		uint32_t flags = dj_le32(q->payload + 0);
		uint32_t uses = dj_le32(q->payload + 4);
		uint8_t phrase_len = q->payload[8];
		if ((size_t)9u + phrase_len != q->len) {
			rc = -EINVAL;
			break;
		}
		rc = dj_safety_arm(flags, uses, (const char *)(q->payload + 9), phrase_len);
		break;
	}
	case DJP2_SAFETY_DISARM:
		dj_safety_reset();
		break;
	case DJP2_DEBUG_BITBANG: {
		size_t outn = DJP2_MAX_PAYLOAD;
		rc = dj_debug_remote_bitbang(q->payload, q->len, r->payload, &outn);
		if (!rc)
			r->len = (uint32_t)outn;
		break;
	}
	case DJP2_DEBUG_INFO: {
		struct dj_debug_info i = {0};
		rc = dj_debug_get_info(&i);
		if (!rc) {
			dj_put_le16(r->payload + 0, (uint16_t)i.proto);
			r->payload[2] = (uint8_t)i.state;
			r->payload[3] = (uint8_t)i.transport;
			dj_put_le32(r->payload + 4, i.caps);
			dj_put_le16(r->payload + 8, i.hw_breakpoints);
			r->payload[10] = i.register_width_bits;
			r->payload[11] = 0;
			r->len = 12;
		}
		break;
	}
	case DJP2_DEBUG_ATTACH:
		rc = dj_debug_attach();
		break;
	case DJP2_DEBUG_DETACH:
		rc = dj_debug_detach();
		break;
	case DJP2_DEBUG_HALT:
		rc = dj_debug_halt();
		break;
	case DJP2_DEBUG_RUN:
		rc = dj_debug_run();
		break;
	case DJP2_DEBUG_STEP:
		rc = dj_debug_step();
		break;
	case DJP2_DEBUG_RESET:
		rc = dj_debug_reset(q->len && q->payload[0] != 0);
		break;
	case DJP2_DEBUG_REG_READ: {
		size_t outn = DJP2_MAX_PAYLOAD;
		rc = dj_debug_read_regs(r->payload, &outn);
		if (!rc)
			r->len = (uint32_t)outn;
		break;
	}
	case DJP2_DEBUG_REG_WRITE:
		rc = dj_debug_write_regs(q->payload, q->len);
		break;
	case DJP2_DEBUG_BP_SET:
		if (q->len != 6)
			rc = -EINVAL;
		else
			rc = dj_debug_set_breakpoint(dj_le32(q->payload), q->payload[4], q->payload[5]);
		break;
	case DJP2_DEBUG_BP_CLEAR:
		if (q->len != 1)
			rc = -EINVAL;
		else
			rc = dj_debug_clear_breakpoint(q->payload[0]);
		break;

	case DJP2_RTT_SCAN: {
		if (q->len < 8) {
			rc = -EINVAL;
			break;
		}
		uint32_t start = dj_le32(q->payload + 0);
		uint32_t end = dj_le32(q->payload + 4);
		uint32_t cb = 0u;
		rc = dj_rtt_scan(start, end, &cb);
		if (!rc) {
			dj_put_le32(r->payload, cb);
			r->len = 4;
		}
		break;
	}
	case DJP2_RTT_INFO: {
		if (q->len < 4) {
			rc = -EINVAL;
			break;
		}
		struct dj_rtt_info info;
		rc = dj_rtt_get_info(dj_le32(q->payload), &info);
		if (!rc) {
			dj_put_le32(r->payload + 0, info.cb_addr);
			dj_put_le32(r->payload + 4, info.max_up);
			dj_put_le32(r->payload + 8, info.max_down);
			dj_put_le32(r->payload + 12, info.up_name);
			dj_put_le32(r->payload + 16, info.up_buffer);
			dj_put_le32(r->payload + 20, info.up_size);
			dj_put_le32(r->payload + 24, info.up_wr);
			dj_put_le32(r->payload + 28, info.up_rd);
			dj_put_le32(r->payload + 32, info.up_flags);
			dj_put_le32(r->payload + 36, info.down_name);
			dj_put_le32(r->payload + 40, info.down_buffer);
			dj_put_le32(r->payload + 44, info.down_size);
			dj_put_le32(r->payload + 48, info.down_wr);
			dj_put_le32(r->payload + 52, info.down_rd);
			dj_put_le32(r->payload + 56, info.down_flags);
			r->len = 60;
		}
		break;
	}
	case DJP2_RTT_READ: {
		if (q->len < 7) {
			rc = -EINVAL;
			break;
		}
		uint32_t cb = dj_le32(q->payload + 0);
		uint8_t channel = q->payload[4];
		uint16_t want = dj_le16(q->payload + 5);
		size_t outn = want;
		if (outn > DJP2_MAX_PAYLOAD) {
			rc = -ERANGE;
			break;
		}
		rc = dj_rtt_read_up(cb, channel, r->payload, &outn);
		if (!rc)
			r->len = (uint32_t)outn;
		break;
	}
	case DJP2_RTT_WRITE: {
		if (q->len < 5) {
			rc = -EINVAL;
			break;
		}
		uint32_t cb = dj_le32(q->payload + 0);
		uint8_t channel = q->payload[4];
		size_t written = 0u;
		rc = dj_rtt_write_down(cb, channel, q->payload + 5, q->len - 5u, &written);
		if (!rc) {
			dj_put_le32(r->payload, (uint32_t)written);
			r->len = 4;
		}
		break;
	}

	case DJP2_RTT_CHANNEL_INFO: {
		if (q->len < 6) {
			rc = -EINVAL;
			break;
		}
		struct dj_rtt_channel_info ci;
		rc = dj_rtt_get_channel_info(dj_le32(q->payload), q->payload[4], q->payload[5], &ci);
		if (!rc) {
			r->payload[0] = ci.direction;
			r->payload[1] = ci.channel;
			r->payload[2] = ci.name_len;
			r->payload[3] = 0u;
			dj_put_le32(r->payload + 4, ci.name_addr);
			dj_put_le32(r->payload + 8, ci.buffer_addr);
			dj_put_le32(r->payload + 12, ci.size);
			dj_put_le32(r->payload + 16, ci.wr_off);
			dj_put_le32(r->payload + 20, ci.rd_off);
			dj_put_le32(r->payload + 24, ci.flags);
			dj_put_le32(r->payload + 28, ci.used);
			dj_put_le32(r->payload + 32, ci.free_space);
			if (ci.name_len)
				memcpy(r->payload + 36, ci.name, ci.name_len);
			r->len = (uint32_t)(36u + ci.name_len);
		}
		break;
	}
	case DJP2_SWO_CONFIG: {
		if (q->len < 8) {
			rc = -EINVAL;
			break;
		}
		rc = dj_swo_configure(dj_le32(q->payload + 0), dj_le32(q->payload + 4));
		break;
	}
	case DJP2_SWO_START:
		rc = dj_swo_start();
		break;
	case DJP2_SWO_STOP:
		rc = dj_swo_stop();
		break;
	case DJP2_SWO_STATUS: {
		struct dj_swo_status st;
		rc = dj_swo_status(&st);
		if (!rc) {
			dj_put_le32(r->payload + 0, st.baud);
			dj_put_le32(r->payload + 4, st.flags);
			dj_put_le32(r->payload + 8, st.available);
			dj_put_le32(r->payload + 12, st.dropped);
			r->payload[16] = st.active ? 1u : 0u;
			r->payload[17] = 0u;
			r->payload[18] = 0u;
			r->payload[19] = 0u;
			r->len = 20;
		}
		break;
	}
	case DJP2_SWO_READ: {
		if (q->len < 2) {
			rc = -EINVAL;
			break;
		}
		size_t outn = dj_le16(q->payload);
		if (outn > DJP2_MAX_PAYLOAD) {
			rc = -ERANGE;
			break;
		}
		rc = dj_swo_read(r->payload, &outn);
		if (!rc)
			r->len = (uint32_t)outn;
		break;
	}
	case DJP2_SAFE_IDLE:
	case DJP2_ABORT:
		dj_safety_reset();
		rc = dj_hw_safe_idle();
		break;
	default:
		rc = -ENOSYS;
		break;
	}

	r->status = maperr(rc);
	return rc;
}
