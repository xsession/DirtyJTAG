/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_SCRIPT_VM_H
#define DJPROG_SCRIPT_VM_H
#include <stddef.h>
#include <stdint.h>

/* Tiny USB script VM for niche MCU protocols whose public algorithms are best
 * kept host-side.  The VM is intentionally electrical, not semantic: it can
 * power the target, set directions, clock bits, delay and sample pins.  Device
 * algorithms remain in host scripts/profiles so new MCU families can be added
 * without reflashing the Pico firmware.
 */
int dj_script_execute(const uint8_t *script, size_t script_len, uint8_t *out, size_t *out_len);
#endif
