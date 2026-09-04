/* SPDX-License-Identifier: MIT */
#include "djprog/script_vm.h"
#include "djprog/common.h"
#include "djprog/hw.h"
#include "djprog/phy.h"
#include <errno.h>
#include <string.h>

#define VM_OP_END 0x00u
#define VM_OP_DIR 0x01u      /* role:u8 dir:u8 */
#define VM_OP_WRITE 0x02u    /* role:u8 value:u8 */
#define VM_OP_READ 0x03u     /* role:u8 -> u8 */
#define VM_OP_DELAY_US 0x04u /* u32 */
#define VM_OP_POWER 0x05u    /* mode:u8 */
#define VM_OP_VPP 0x06u      /* path:u8 on:u8 */
#define VM_OP_CLOCK_BITS                                                                           \
	0x07u               /* clk,out,in,bits:u8 flags:u8 hz:u32 tx:ceil(bits/8) -> rx if flags bit1 */
#define VM_OP_SPI 0x08u /* hz:u32 len:u16 bytes -> bytes */
#define VM_OP_UART1W 0x09u /* baud:u32 tx:u16 rx:u16 flags:u8 bytes -> rx */
#define VM_OP_SAFE_IDLE 0x0Au

static uint32_t le32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t le16(const uint8_t *p) {
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int append(uint8_t *out, size_t *pos, size_t cap, const uint8_t *data, size_t len) {
	if (!out || !pos || !data)
		return -EINVAL;
	if (*pos + len > cap)
		return -ENOSPC;
	memcpy(out + *pos, data, len);
	*pos += len;
	return 0;
}

static enum dj_dir vm_dir(uint8_t d) {
	switch (d) {
	case 0:
		return DJ_DIR_INPUT;
	case 1:
		return DJ_DIR_OUTPUT;
	case 2:
		return DJ_DIR_OD_LOW;
	default:
		return DJ_DIR_RELEASE;
	}
}

int dj_script_execute(const uint8_t *s, size_t n, uint8_t *out, size_t *out_len) {
	if (!s || !out_len)
		return -EINVAL;
	const struct dj_hw_ops *h = dj_hw();
	if (!h)
		return -ENODEV;
	size_t ip = 0u, op = 0u, cap = *out_len;
	while (ip < n) {
		uint8_t code = s[ip++];
		int r = 0;
		switch (code) {
		case VM_OP_END:
			*out_len = op;
			return 0;
		case VM_OP_DIR:
			if (ip + 2u > n || !h->dir)
				return -EINVAL;
			r = h->dir((enum dj_pin_role)s[ip], vm_dir(s[ip + 1]));
			ip += 2u;
			break;
		case VM_OP_WRITE:
			if (ip + 2u > n || !h->write)
				return -EINVAL;
			r = h->write((enum dj_pin_role)s[ip], s[ip + 1] != 0u);
			ip += 2u;
			break;
		case VM_OP_READ: {
			if (ip + 1u > n || !h->read)
				return -EINVAL;
			bool v = false;
			r = h->read((enum dj_pin_role)s[ip++], &v);
			if (!r) {
				uint8_t b = v ? 1u : 0u;
				r = append(out, &op, cap, &b, 1u);
			}
			break;
		}
		case VM_OP_DELAY_US:
			if (ip + 4u > n || !h->delay_us)
				return -EINVAL;
			h->delay_us(le32(s + ip));
			ip += 4u;
			break;
		case VM_OP_POWER:
			if (ip + 1u > n || !h->power)
				return -EINVAL;
			r = h->power((enum dj_power_mode)s[ip++]);
			break;
		case VM_OP_VPP:
			if (ip + 2u > n)
				return -EINVAL;
			if (s[ip] == 0u)
				r = h->vpp_boost ? h->vpp_boost(s[ip + 1] != 0u) : -ENOTSUP;
			else if (s[ip] == 1u)
				r = h->vpp_apply ? h->vpp_apply(s[ip + 1] != 0u) : -ENOTSUP;
			else if (s[ip] == 2u)
				r = h->hv_data0_apply ? h->hv_data0_apply(s[ip + 1] != 0u) : -ENOTSUP;
			else
				r = -EINVAL;
			ip += 2u;
			break;
		case VM_OP_CLOCK_BITS: {
			if (ip + 9u > n || !h->clock_bits)
				return -EINVAL;
			enum dj_pin_role clk = (enum dj_pin_role)s[ip++];
			enum dj_pin_role dout = (enum dj_pin_role)s[ip++];
			enum dj_pin_role din = (enum dj_pin_role)s[ip++];
			uint8_t bits = s[ip++];
			uint8_t flags = s[ip++];
			uint32_t hz = le32(s + ip);
			ip += 4u;
			if (bits == 0u || bits > 32u)
				return -EINVAL;
			size_t bytes = (bits + 7u) / 8u;
			if (ip + bytes > n)
				return -EINVAL;
			uint8_t rx[4] = {0};
			bool want_rx = (flags & 2u) != 0u;
			r = h->clock_bits(clk, dout, din, s + ip, want_rx ? rx : NULL, bits, (flags & 1u) != 0u,
			                  hz ? hz : 100000u);
			ip += bytes;
			if (!r && want_rx)
				r = append(out, &op, cap, rx, bytes);
			break;
		}
		case VM_OP_SPI: {
			if (ip + 6u > n)
				return -EINVAL;
			uint32_t hz = le32(s + ip);
			ip += 4u;
			uint16_t len = le16(s + ip);
			ip += 2u;
			if (ip + len > n || op + len > cap)
				return -EINVAL;
			r = dj_spi_xfer(s + ip, out + op, len, hz ? hz : 125000u);
			ip += len;
			if (!r)
				op += len;
			break;
		}
		case VM_OP_UART1W: {
			if (ip + 9u > n)
				return -EINVAL;
			uint32_t baud = le32(s + ip);
			ip += 4u;
			uint16_t txc = le16(s + ip);
			ip += 2u;
			uint16_t rxc = le16(s + ip);
			ip += 2u;
			uint8_t flags = s[ip++];
			if (ip + txc > n || op + rxc > cap)
				return -EINVAL;
			enum dj_uart_parity parity = (flags & 1u) ? DJ_PARITY_EVEN : DJ_PARITY_NONE;
			uint8_t stop_bits = (flags & 2u) ? 2u : 1u;
			r = dj_uart_1wire_txrx_ex(s + ip, txc, out + op, rxc, baud ? baud : 115200u, false,
			                          parity, stop_bits);
			ip += txc;
			if (!r)
				op += rxc;
			break;
		}
		case VM_OP_SAFE_IDLE:
			r = dj_hw_safe_idle();
			break;
		default:
			return -ENOSYS;
		}
		if (r)
			return r;
	}
	*out_len = op;
	return 0;
}
