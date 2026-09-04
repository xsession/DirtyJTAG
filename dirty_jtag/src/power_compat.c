/* SPDX-License-Identifier: MIT */
#include "djpk4/power.h"
#include "djprog/hw.h"
#include <errno.h>
static enum dj_power_mode conv(enum dj_target_power p) {
	switch (p) {
	case DJ_POWER_3V3:
		return DJ_PWR_3V3;
	case DJ_POWER_5V:
		return DJ_PWR_5V;
	case DJ_POWER_EXTERNAL:
		return DJ_PWR_EXTERNAL;
	default:
		return DJ_PWR_OFF;
	}
}
int dj_power_init(void) {
	return dj_hw() ? 0 : -ENODEV;
}
int dj_power_prepare(const struct dj_device *d) {
	if (!d || !dj_hw() || !dj_hw()->power)
		return -ENODEV;
	return dj_hw()->power(conv(d->preferred_power));
}
int dj_power_shutdown(void) {
	return dj_hw_safe_idle();
}
int dj_power_force(enum dj_target_power m) {
	return dj_hw() && dj_hw()->power ? dj_hw()->power(conv(m)) : -ENODEV;
}
int dj_vpp_prepare(const struct dj_device *d) {
	if (!d || !dj_hw())
		return -EINVAL;
	if (!d->needs_vpp)
		return 0;
	if (!dj_hw()->vpp_boost)
		return -ENOTSUP;
	int r = dj_hw()->vpp_boost(true);
	if (r)
		return r;
	for (int i = 0; i < 100; i++) {
		struct dj_measurement m = {0};
		if (dj_hw()->measure && !dj_hw()->measure(&m) && m.vpp_mv >= 11000 && m.vpp_mv <= 13500)
			return 0;
		dj_hw()->delay_us(1000);
	}
	return -ETIMEDOUT;
}
int dj_vpp_apply(bool on) {
	return dj_hw() && dj_hw()->vpp_apply ? dj_hw()->vpp_apply(on) : -ENODEV;
}
int dj_mclr_low(bool low) {
	if (!dj_hw())
		return -ENODEV;
	return dj_hw()->write(DJ_PIN_RESET, !low);
}
int dj_power_measure(struct dj_power_measurement *m) {
	if (!m || !dj_hw() || !dj_hw()->measure)
		return -EINVAL;
	struct dj_measurement x = {0};
	int r = dj_hw()->measure(&x);
	m->vtarget_mv = x.vtarget_mv;
	m->vpp_mv = x.vpp_mv;
	return r;
}
