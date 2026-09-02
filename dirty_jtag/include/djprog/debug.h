/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_DEBUG_H
#define DJPROG_DEBUG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "djprog/backend.h"

/* High-level debug state exposed over DJP2.  The OpenOCD transport owns the
 * actual CPU run state for SWD/JTAG, so TRANSPORT_READY is intentionally
 * distinct from HALTED/RUNNING. */
enum dj_debug_state {
    DJ_DEBUG_DETACHED = 0,
    DJ_DEBUG_TRANSPORT_READY = 1,
    DJ_DEBUG_ATTACHED = 2,
    DJ_DEBUG_RUNNING = 3,
    DJ_DEBUG_HALTED = 4,
    DJ_DEBUG_RESET = 5,
};

enum dj_debug_transport {
    DJ_DEBUG_TRANSPORT_NONE = 0,
    DJ_DEBUG_TRANSPORT_OPENOCD_REMOTE_BITBANG = 1,
    DJ_DEBUG_TRANSPORT_NATIVE = 2,
};

struct dj_debug_info {
    enum dj_proto_id proto;
    enum dj_debug_state state;
    enum dj_debug_transport transport;
    uint32_t caps;
    uint16_t hw_breakpoints;
    uint8_t register_width_bits;
    uint8_t reserved;
};

struct dj_debug_ops {
    enum dj_proto_id proto;
    const char *name;
    uint32_t caps;
    enum dj_debug_transport transport;
    uint16_t hw_breakpoints;
    uint8_t register_width_bits;
    int (*attach)(void);
    int (*detach)(void);
    int (*status)(struct dj_debug_info *info);
    int (*halt)(void);
    int (*run)(void);
    int (*step)(void);
    int (*reset)(bool halt_after_reset);
    int (*read_regs)(uint8_t *out, size_t *len);
    int (*write_regs)(const uint8_t *in, size_t len);
    int (*set_breakpoint)(uint32_t address, uint8_t type, uint8_t slot);
    int (*clear_breakpoint)(uint8_t slot);
};

const struct dj_debug_ops *dj_debug_ops_for(enum dj_proto_id proto);
int dj_debug_get_info(struct dj_debug_info *info);
int dj_debug_attach(void);
int dj_debug_detach(void);
int dj_debug_halt(void);
int dj_debug_run(void);
int dj_debug_step(void);
int dj_debug_reset(bool halt_after_reset);
int dj_debug_read_regs(uint8_t *out, size_t *len);
int dj_debug_write_regs(const uint8_t *in, size_t len);
int dj_debug_set_breakpoint(uint32_t address, uint8_t type, uint8_t slot);
int dj_debug_clear_breakpoint(uint8_t slot);
void dj_debug_backend_changed(void);

#endif
