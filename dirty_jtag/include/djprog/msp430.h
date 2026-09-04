/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_MSP430_H
#define DJPROG_MSP430_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Firmware-side clean-room raw operation envelope for MSP430 4-wire JTAG and
 * 2-wire Spy-Bi-Wire.  These commands deliberately expose TAP operations, not
 * destructive flash algorithms.  Host-side device descriptors decide whether a
 * sequence is safe to promote to erase/write for a given family. */
enum dj_msp430_raw_op {
	DJ_MSP430_RAW_TAP_RESET = 0x00, /* payload: op, cycles; returns empty */
	DJ_MSP430_RAW_SHIFT_IR = 0x01,  /* payload: op, bits_le16, txbytes; returns rxbytes */
	DJ_MSP430_RAW_SHIFT_DR = 0x02,  /* payload: op, bits_le16, txbytes; returns rxbytes */
	DJ_MSP430_RAW_CLOCK = 0x03,     /* payload: op, cycles, tms, tdi; returns packed TDO bits */
};

#endif
