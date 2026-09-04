/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_HW_H
#define DJPROG_HW_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum dj_pin_role {
	DJ_PIN_CLK = 0,
	DJ_PIN_DATA0,
	DJ_PIN_DATA1,
	DJ_PIN_DATA2,
	DJ_PIN_RESET,
	DJ_PIN_AUX,
	DJ_PIN_COUNT
};
enum dj_dir { DJ_DIR_INPUT = 0, DJ_DIR_OUTPUT = 1, DJ_DIR_OD_LOW = 2, DJ_DIR_RELEASE = 3 };
enum dj_power_mode { DJ_PWR_OFF = 0, DJ_PWR_EXTERNAL = 1, DJ_PWR_3V3 = 2, DJ_PWR_5V = 3 };

struct dj_measurement {
	uint32_t vtarget_mv;
	uint32_t vpp_mv;
	uint32_t itarget_ma;
	bool power_fault;
};
struct dj_pinmap {
	uint8_t gpio[DJ_PIN_COUNT];
};
struct dj_hw_ops {
	int (*init)(void);
	int (*configure)(const struct dj_pinmap *map);
	int (*dir)(enum dj_pin_role role, enum dj_dir d);
	int (*write)(enum dj_pin_role role, bool value);
	int (*read)(enum dj_pin_role role, bool *value);
	int (*clock_bits)(enum dj_pin_role clk, enum dj_pin_role data_out, enum dj_pin_role data_in,
	                  const uint8_t *tx, uint8_t *rx, size_t bits, bool lsb_first, uint32_t hz);
	int (*power)(enum dj_power_mode mode);
	int (*vpp_boost)(bool on);
	int (*vpp_apply)(bool on);
	int (*hv_data0_apply)(bool on);
	int (*measure)(struct dj_measurement *m);
	void (*delay_us)(uint32_t us);
};
void dj_hw_bind(const struct dj_hw_ops *ops);
const struct dj_hw_ops *dj_hw(void);
int dj_hw_safe_idle(void);
int dj_hw_set_pinmap(const struct dj_pinmap *map);
const struct dj_pinmap *dj_hw_pinmap(void);
#endif
