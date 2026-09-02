/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_SWIM_H
#define DJPROG_SWIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "djprog/swim_phy.h"

/* STM8 SWIM/DM public register map from ST UM0470. */
#define STM8_CPU_REG_A        0x007F00u
#define STM8_CPU_REG_PCE      0x007F01u
#define STM8_CPU_REG_PCH      0x007F02u
#define STM8_CPU_REG_PCL      0x007F03u
#define STM8_CPU_REG_XH       0x007F04u
#define STM8_CPU_REG_XL       0x007F05u
#define STM8_CPU_REG_YH       0x007F06u
#define STM8_CPU_REG_YL       0x007F07u
#define STM8_CPU_REG_SPH      0x007F08u
#define STM8_CPU_REG_SPL      0x007F09u
#define STM8_CPU_REG_CC       0x007F0Au
#define STM8_CPU_REG_COUNT    11u

#define STM8_SWIM_CSR         0x007F80u
#define STM8_DM_BK1E          0x007F90u
#define STM8_DM_BK1H          0x007F91u
#define STM8_DM_BK1L          0x007F92u
#define STM8_DM_BK2E          0x007F93u
#define STM8_DM_BK2H          0x007F94u
#define STM8_DM_BK2L          0x007F95u
#define STM8_DM_CR1           0x007F96u
#define STM8_DM_CR2           0x007F97u
#define STM8_DM_CSR1          0x007F98u
#define STM8_DM_CSR2          0x007F99u
#define STM8_DM_ENFCTR        0x007F9Au

#define STM8_SWIM_CSR_SWIM_DM 0x20u
#define STM8_SWIM_CSR_HS      0x10u
#define STM8_SWIM_CSR_NOACC   0x40u

#define STM8_DM_CSR1_STE      0x40u
#define STM8_DM_CSR1_STF      0x20u
#define STM8_DM_CSR1_RST      0x10u
#define STM8_DM_CSR1_BK2F     0x04u
#define STM8_DM_CSR1_BK1F     0x02u

#define STM8_DM_CSR2_SWBKE    0x20u
#define STM8_DM_CSR2_SWBKF    0x10u
#define STM8_DM_CSR2_STALL    0x08u
#define STM8_DM_CSR2_FLUSH    0x01u

#define STM8_DM_CR1_WDGOFF    0x80u
#define STM8_DM_CR1_BC_IFETCH_RANGE 0x08u
#define STM8_DM_CR1_BC_IFETCH_OR    0x28u
#define STM8_DM_CR1_BC_IFETCH_SEQ   0x80u

struct stm8_cpu_regs {
    uint8_t a;
    uint32_t pc; /* 24-bit, stored in bits 23:0. */
    uint16_t x;
    uint16_t y;
    uint16_t sp;
    uint8_t cc;
};

int dj_swim_select(uint32_t requested_hz, bool mock_target);
int dj_swim_enter(void);
int dj_swim_leave(void);
int dj_swim_system_reset(void);
int dj_swim_read_mem(uint32_t addr, uint8_t *data, size_t len);
int dj_swim_write_mem(uint32_t addr, const uint8_t *data, size_t len);
int dj_swim_set_speed(bool high_speed);
const char *dj_swim_selected_phy_name(void);

int dj_stm8_debug_attach(void);
int dj_stm8_debug_detach(void);
int dj_stm8_debug_status(uint8_t *csr1, uint8_t *csr2, bool *halted);
int dj_stm8_debug_halt(void);
int dj_stm8_debug_run(void);
int dj_stm8_debug_step(void);
int dj_stm8_debug_reset(bool halt_after_reset);
int dj_stm8_debug_read_regs(uint8_t *out, size_t *len);
int dj_stm8_debug_write_regs(const uint8_t *in, size_t len);
int dj_stm8_debug_set_breakpoint(uint32_t address, uint8_t type, uint8_t slot);
int dj_stm8_debug_clear_breakpoint(uint8_t slot);

/* Native-test helper.  It is inert unless the selected target name starts with
 * "mock-stm8"; production paths always use SWIM WOTF/ROTF on DATA0. */
void dj_swim_mock_reset(void);
int dj_swim_mock_read(uint32_t addr, uint8_t *data, size_t len);
int dj_swim_mock_write(uint32_t addr, const uint8_t *data, size_t len);

#endif
