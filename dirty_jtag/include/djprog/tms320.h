/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_TMS320_H
#define DJPROG_TMS320_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Clean-room raw operation envelope for TI TMS320/C2000 JTAG bring-up.
 * This is intentionally not TI's proprietary XDS USB protocol.  It exposes the
 * electrical IEEE 1149.1 operations needed by a host bridge or bench tool while
 * keeping CCS/XDS emulation as a separate, future compatibility layer. */
enum dj_tms320_raw_op {
	DJ_TMS320_RAW_TAP_RESET = 0x00, /* payload: op, cycles; returns empty */
	DJ_TMS320_RAW_SHIFT_IR = 0x01,  /* payload: op, bits_le16, txbytes; returns rxbytes */
	DJ_TMS320_RAW_SHIFT_DR = 0x02,  /* payload: op, bits_le16, txbytes; returns rxbytes */
	DJ_TMS320_RAW_CLOCK = 0x03,     /* payload: op, cycles, tms, tdi; returns packed TDO */
	DJ_TMS320_RAW_LINES = 0x04,     /* payload: op, nreset, ntrst_or_aux, emu_hint; returns empty */
};

#define DJ_TMS320_FLAG_AUX_IS_NTRST 0x0001u
#define DJ_TMS320_FLAG_AUX_IS_EMU0 0x0002u
#define DJ_TMS320_FLAG_CJTAG 0x0004u

#endif
