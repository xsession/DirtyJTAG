/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_DSPIC_DEBUG_H
#define DJPROG_DSPIC_DEBUG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DJ_DSPIC_DEBUG_REG_BYTES 42u /* W0..W15, PC24, SR, WREG shadow/alignment room */

struct dj_dspic_debug_capsule_info {
	uint8_t loaded;
	uint8_t family;
	uint8_t hw_breakpoints;
	uint8_t register_bytes;
	uint32_t flags;
};

void dj_dspic_debug_backend_changed(void);
int dj_dspic_debug_load_capsule(const uint8_t *data, size_t len);
int dj_dspic_debug_capsule_info(struct dj_dspic_debug_capsule_info *info);
int dj_dspic_debug_attach(void);
int dj_dspic_debug_detach(void);
int dj_dspic_debug_status(bool *halted);
int dj_dspic_debug_halt(void);
int dj_dspic_debug_run(void);
int dj_dspic_debug_step(void);
int dj_dspic_debug_reset(bool halt_after_reset);
int dj_dspic_debug_read_regs(uint8_t *out, size_t *len);
int dj_dspic_debug_write_regs(const uint8_t *in, size_t len);
int dj_dspic_debug_set_breakpoint(uint32_t address, uint8_t type, uint8_t slot);
int dj_dspic_debug_clear_breakpoint(uint8_t slot);
#endif
