/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_SWIM_PHY_H
#define DJPROG_SWIM_PHY_H

#include <stdbool.h>
#include <stdint.h>

/* Pluggable STM8 SWIM physical layer.
 *
 * The upper layer in src/swim.c owns UM0470 command semantics (SRST/ROTF/WOTF,
 * parity, ACK/NACK and STM8 debug-module register access).  A PHY may replace
 * only the timing-critical packet send/receive primitives.  This keeps the
 * debugger ABI and mock test target identical across GPIO, RP2040 PIO and any
 * later timer/DMA implementation.
 */
struct dj_swim_phy_ops {
	const char *name;
	bool (*available)(void);
	int (*select)(uint32_t requested_hz);
	int (*enter)(void);
	int (*leave)(void);
	int (*send_packet)(uint32_t value, unsigned bits); /* bits: 3 or 8 */
	int (*recv_packet)(uint8_t *value);                /* 8-bit data packet */
	int (*set_speed)(bool high_speed, uint32_t hz);
};

int dj_swim_phy_register(const struct dj_swim_phy_ops *ops);
const char *dj_swim_phy_name(void);
bool dj_swim_phy_active(void);

/* Board-specific optional binder.  Non-RP2040/native builds provide a weak
 * no-op in src/swim.c, so board HALs may call this unconditionally.
 */
int dj_swim_rp2040_pio_try_bind(uint8_t data0_gpio, uint32_t initial_hz);

#endif
