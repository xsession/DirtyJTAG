/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_POWER_TRACE_H
#define DJPROG_POWER_TRACE_H
#include <stddef.h>
#include <stdint.h>

/* Response records are 16 bytes:
 * t_ms, vtarget_mv, vpp_mv, itarget_ma. Power-fault is ORed into bit31 of
 * itarget_ma so hosts can log compact samples without another field. */
int dj_power_trace_capture(const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen);

#endif
