/* SPDX-License-Identifier: MIT */
#ifndef DJPK4_ICSP_H
#define DJPK4_ICSP_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum dj_target_power {
    DJ_POWER_OFF = 0,
    DJ_POWER_EXTERNAL = 1,
    DJ_POWER_3V3 = 2,
    DJ_POWER_5V = 3,
};

struct dj_power_measurement {
    uint32_t vtarget_mv;
    uint32_t vpp_mv;
};

struct dj_icsp_hal {
    int (*init)(void);
    int (*set_pgd_output)(bool output);
    int (*set_mclr_low)(bool low);
    int (*set_vpp_boost)(bool on);
    int (*set_vpp_apply)(bool on);
    int (*set_target_power)(enum dj_target_power mode);
    int (*measure)(struct dj_power_measurement *m);
    int (*write_bits)(uint32_t value, unsigned bits, bool msb_first, uint32_t hz);
    int (*read_bits)(uint32_t *value, unsigned bits, bool lsb_first, uint32_t hz);
    void (*delay_us)(uint32_t us);
};

void dj_icsp_bind(const struct dj_icsp_hal *hal);
const struct dj_icsp_hal *dj_icsp_hal(void);
int dj_icsp_six(uint32_t instruction, uint32_t hz);
int dj_icsp_regout(uint16_t *value, uint32_t hz);
int dj_icsp_idle_clocks(unsigned clocks, uint32_t hz);
#endif
