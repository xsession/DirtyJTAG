/* SPDX-License-Identifier: MIT */
#ifndef DJPK4_POWER_H
#define DJPK4_POWER_H
#include <stdbool.h>
#include "djpk4/device.h"
int dj_power_init(void);
int dj_power_prepare(const struct dj_device *dev);
int dj_power_shutdown(void);
int dj_power_force(enum dj_target_power mode);
int dj_vpp_prepare(const struct dj_device *dev);
int dj_vpp_apply(bool on);
int dj_mclr_low(bool low);
int dj_power_measure(struct dj_power_measurement *m);
#endif
