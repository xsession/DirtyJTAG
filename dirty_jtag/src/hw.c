/* SPDX-License-Identifier: MIT */
#include "djprog/hw.h"
#include <errno.h>

/* Keep the selected backend and pin map in one place so protocol engines use
 * the same safety sequencing when they manipulate target hardware. */
static const struct dj_hw_ops *g;
static struct dj_pinmap gmap = {{2, 3, 4, 5, 6, 7}};
void dj_hw_bind(const struct dj_hw_ops *ops) {
	g = ops;
}
const struct dj_hw_ops *dj_hw(void) {
	return g;
}
const struct dj_pinmap *dj_hw_pinmap(void) {
	return &gmap;
}
int dj_hw_set_pinmap(const struct dj_pinmap *m) {
	if (!g || !m)
		return -EINVAL;
	gmap = *m;
	return g->configure ? g->configure(&gmap) : 0;
}
int dj_hw_safe_idle(void) {
	if (!g)
		return -ENODEV;
	/* Remove HV first, then target power, then stop driving digital pins. */
	if (g->hv_data0_apply)
		g->hv_data0_apply(false);
	if (g->vpp_apply)
		g->vpp_apply(false);
	if (g->vpp_boost)
		g->vpp_boost(false);
	if (g->write)
		g->write(DJ_PIN_RESET, true); /* release RESET/MCLR sink */
	if (g->power)
		g->power(DJ_PWR_OFF);
	for (int i = 0; i < DJ_PIN_COUNT; i++)
		if (i != DJ_PIN_RESET && g->dir)
			g->dir((enum dj_pin_role)i, DJ_DIR_INPUT);
	return 0;
}
