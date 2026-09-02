/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_SIMPLELINK_H
#define DJPROG_SIMPLELINK_H
#include <stdint.h>

/* TI SimpleLink CC13xx/CC26xx support.  This is an electrical/debug transport
 * layer for SWD/cJTAG-style bring-up and ROM-bootloader entry sequencing. It is
 * not TI XDS USB protocol emulation. */
enum dj_simplelink_raw_op {
    DJ_SIMPLELINK_RAW_SWD_SEQUENCE = 0x00, /* ARM-SWD style payload: tb,rb,tx */
    DJ_SIMPLELINK_RAW_CJTAG_CLOCK  = 0x01, /* op, cycles, tmsc_out; returns sampled TMSC */
    DJ_SIMPLELINK_RAW_LINES        = 0x02, /* op, nreset, bootloader_backdoor_hint */
};

#define DJ_SIMPLELINK_FLAG_CJTAG      0x0001u
#define DJ_SIMPLELINK_FLAG_BOOTLOADER 0x0002u
#define DJ_SIMPLELINK_FLAG_RF_DEBUG   0x0004u

#endif
