/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_DEBUG_BITBANG_H
#define DJPROG_DEBUG_BITBANG_H
#include <stddef.h>
#include <stdint.h>

/*
 * Execute a block of OpenOCD remote_bitbang protocol bytes directly on the
 * currently-selected ARM SWD or generic JTAG backend.  The output contains
 * one ASCII '0'/'1' byte for every sample/read request in the input.
 */
int dj_debug_remote_bitbang(const uint8_t *ops, size_t op_len, uint8_t *samples,
                            size_t *sample_len);
#endif
