/* SPDX-License-Identifier: MIT */
#include "djprog/debug.h"
#include "djprog/dspic_debug.h"
#include "djprog/hw.h"
#include "djprog/swim.h"
#include <errno.h>
#include <string.h>

static enum dj_debug_state openocd_state = DJ_DEBUG_DETACHED;

static enum dj_debug_state stm8_state = DJ_DEBUG_DETACHED;

static enum dj_debug_state dspic_state = DJ_DEBUG_DETACHED;

static int dspic_attach(void) {
	int r = dj_dspic_debug_attach();
	if (!r)
		dspic_state = DJ_DEBUG_ATTACHED;
	return r;
}

static int dspic_detach(void) {
	int r = dj_dspic_debug_detach();
	if (!r)
		dspic_state = DJ_DEBUG_DETACHED;
	return r;
}

static int dspic_status(struct dj_debug_info *info) {
	if (!info)
		return -EINVAL;
	if (dspic_state == DJ_DEBUG_DETACHED) {
		info->state = DJ_DEBUG_DETACHED;
		return 0;
	}
	bool halted = false;
	int r = dj_dspic_debug_status(&halted);
	if (r)
		return r;
	dspic_state = halted ? DJ_DEBUG_HALTED : DJ_DEBUG_RUNNING;
	info->state = dspic_state;
	return 0;
}

static int dspic_halt(void) {
	int r = dj_dspic_debug_halt();
	if (!r)
		dspic_state = DJ_DEBUG_HALTED;
	return r;
}

static int dspic_run(void) {
	int r = dj_dspic_debug_run();
	if (!r)
		dspic_state = DJ_DEBUG_RUNNING;
	return r;
}

static int dspic_step(void) {
	int r = dj_dspic_debug_step();
	if (!r)
		dspic_state = DJ_DEBUG_HALTED;
	return r;
}

static int dspic_reset(bool halt_after_reset) {
	int r = dj_dspic_debug_reset(halt_after_reset);
	if (!r)
		dspic_state = halt_after_reset ? DJ_DEBUG_HALTED : DJ_DEBUG_RESET;
	return r;
}

static int stm8_attach(void) {
	int r = dj_stm8_debug_attach();
	if (!r)
		stm8_state = DJ_DEBUG_ATTACHED;
	return r;
}

static int stm8_detach(void) {
	int r = dj_stm8_debug_detach();
	if (!r)
		stm8_state = DJ_DEBUG_DETACHED;
	return r;
}

static int stm8_status(struct dj_debug_info *info) {
	if (!info)
		return -EINVAL;
	if (stm8_state == DJ_DEBUG_DETACHED) {
		info->state = DJ_DEBUG_DETACHED;
		return 0;
	}
	bool halted = false;
	int r = dj_stm8_debug_status(NULL, NULL, &halted);
	if (r)
		return r;
	stm8_state = halted ? DJ_DEBUG_HALTED : DJ_DEBUG_RUNNING;
	info->state = stm8_state;
	return 0;
}

static int stm8_halt(void) {
	int r = dj_stm8_debug_halt();
	if (!r)
		stm8_state = DJ_DEBUG_HALTED;
	return r;
}

static int stm8_run(void) {
	int r = dj_stm8_debug_run();
	if (!r)
		stm8_state = DJ_DEBUG_RUNNING;
	return r;
}

static int stm8_step(void) {
	int r = dj_stm8_debug_step();
	if (!r)
		stm8_state = DJ_DEBUG_HALTED;
	return r;
}

static int stm8_reset(bool halt_after_reset) {
	int r = dj_stm8_debug_reset(halt_after_reset);
	if (!r)
		stm8_state = halt_after_reset ? DJ_DEBUG_HALTED : DJ_DEBUG_RESET;
	return r;
}

static int openocd_attach(void) {
	const struct dj_backend *b = dj_selected_backend();
	if (!b || !(b->caps & DJ_CAP_DEBUG_OPENOCD))
		return -ENOTSUP;
	openocd_state = DJ_DEBUG_TRANSPORT_READY;
	return 0;
}

static int openocd_detach(void) {
	openocd_state = DJ_DEBUG_DETACHED;
	return 0;
}

static int openocd_status(struct dj_debug_info *info) {
	if (!info)
		return -EINVAL;
	info->state = openocd_state;
	return 0;
}

/* OpenOCD owns target-specific AP/DP, register, breakpoint and run-control
 * semantics.  Keeping these operations unsupported here avoids two hosts
 * racing for the same debug port. */
static const struct dj_debug_ops debug_ops[] = {
    {
        .proto = DJ_PROTO_DSPIC_ICSP,
        .name = "dspic-debug-executive-cleanroom",
        .caps = DJ_CAP_DEBUG_PHY | DJ_CAP_DEBUG_RUNCTRL | DJ_CAP_DEBUG_REGS | DJ_CAP_DEBUG_BREAK |
                DJ_CAP_EXPERIMENTAL,
        .transport = DJ_DEBUG_TRANSPORT_NATIVE,
        .hw_breakpoints = 2,
        .register_width_bits = 16,
        .attach = dspic_attach,
        .detach = dspic_detach,
        .status = dspic_status,
        .halt = dspic_halt,
        .run = dspic_run,
        .step = dspic_step,
        .reset = dspic_reset,
        .read_regs = dj_dspic_debug_read_regs,
        .write_regs = dj_dspic_debug_write_regs,
        .set_breakpoint = dj_dspic_debug_set_breakpoint,
        .clear_breakpoint = dj_dspic_debug_clear_breakpoint,
    },
    {
        .proto = DJ_PROTO_ARM_SWD,
        .name = "arm-swd-openocd",
        .caps = DJ_CAP_DEBUG_PHY | DJ_CAP_DEBUG_OPENOCD,
        .transport = DJ_DEBUG_TRANSPORT_OPENOCD_REMOTE_BITBANG,
        .hw_breakpoints = 0,
        .register_width_bits = 32,
        .attach = openocd_attach,
        .detach = openocd_detach,
        .status = openocd_status,
    },
    {
        .proto = DJ_PROTO_JTAG,
        .name = "jtag-openocd",
        .caps = DJ_CAP_DEBUG_PHY | DJ_CAP_DEBUG_OPENOCD,
        .transport = DJ_DEBUG_TRANSPORT_OPENOCD_REMOTE_BITBANG,
        .hw_breakpoints = 0,
        .register_width_bits = 0,
        .attach = openocd_attach,
        .detach = openocd_detach,
        .status = openocd_status,
    },
    {
        .proto = DJ_PROTO_TI_SIMPLELINK_SWD,
        .name = "ti-simplelink-cc13xx-cc26xx-swd-openocd-transport",
        .caps = DJ_CAP_DEBUG_PHY | DJ_CAP_DEBUG_OPENOCD | DJ_CAP_EXPERIMENTAL,
        .transport = DJ_DEBUG_TRANSPORT_OPENOCD_REMOTE_BITBANG,
        .hw_breakpoints = 0,
        .register_width_bits = 32,
        .attach = openocd_attach,
        .detach = openocd_detach,
        .status = openocd_status,
    },
    {
        .proto = DJ_PROTO_TI_SIMPLELINK_CJTAG,
        .name = "ti-simplelink-cc13xx-cc26xx-cjtag-physical",
        .caps = DJ_CAP_DEBUG_PHY | DJ_CAP_EXPERIMENTAL,
        .transport = DJ_DEBUG_TRANSPORT_NATIVE,
        .hw_breakpoints = 0,
        .register_width_bits = 32,
    },
    {
        .proto = DJ_PROTO_TMS320_C2000_JTAG,
        .name = "tms320-c2000-xds110v3-jtag-openocd-transport",
        .caps = DJ_CAP_DEBUG_PHY | DJ_CAP_DEBUG_OPENOCD | DJ_CAP_EXPERIMENTAL,
        .transport = DJ_DEBUG_TRANSPORT_OPENOCD_REMOTE_BITBANG,
        .hw_breakpoints = 0,
        .register_width_bits = 32,
        .attach = openocd_attach,
        .detach = openocd_detach,
        .status = openocd_status,
    },
    /* The following transports are intentionally advertised as physical-only
     * until their public, family-specific run-control engines are complete. */
    {
        .proto = DJ_PROTO_AVR_UPDI,
        .name = "avr-updi-ocd-physical",
        .caps = DJ_CAP_DEBUG_PHY | DJ_CAP_EXPERIMENTAL,
        .transport = DJ_DEBUG_TRANSPORT_NATIVE,
        .hw_breakpoints = 0,
        .register_width_bits = 8,
    },
    {
        .proto = DJ_PROTO_AVR_PDI,
        .name = "avr-pdi-ocd-physical",
        .caps = DJ_CAP_DEBUG_PHY | DJ_CAP_EXPERIMENTAL,
        .transport = DJ_DEBUG_TRANSPORT_NATIVE,
        .hw_breakpoints = 0,
        .register_width_bits = 8,
    },
    {
        .proto = DJ_PROTO_STM8_SWIM,
        .name = "stm8-swim-native-debug",
        .caps = DJ_CAP_DEBUG_PHY | DJ_CAP_DEBUG_RUNCTRL | DJ_CAP_DEBUG_REGS | DJ_CAP_DEBUG_BREAK |
                DJ_CAP_EXPERIMENTAL,
        .transport = DJ_DEBUG_TRANSPORT_NATIVE,
        .hw_breakpoints = 2,
        .register_width_bits = 8,
        .attach = stm8_attach,
        .detach = stm8_detach,
        .status = stm8_status,
        .halt = stm8_halt,
        .run = stm8_run,
        .step = stm8_step,
        .reset = stm8_reset,
        .read_regs = dj_stm8_debug_read_regs,
        .write_regs = dj_stm8_debug_write_regs,
        .set_breakpoint = dj_stm8_debug_set_breakpoint,
        .clear_breakpoint = dj_stm8_debug_clear_breakpoint,
    },
    {
        .proto = DJ_PROTO_MSP430_SBW,
        .name = "msp430-sbw-debug-physical",
        .caps = DJ_CAP_DEBUG_PHY | DJ_CAP_EXPERIMENTAL,
        .transport = DJ_DEBUG_TRANSPORT_NATIVE,
        .hw_breakpoints = 0,
        .register_width_bits = 16,
    },
    {
        .proto = DJ_PROTO_MSP430_JTAG,
        .name = "msp430-jtag-debug-physical",
        .caps = DJ_CAP_DEBUG_PHY | DJ_CAP_EXPERIMENTAL,
        .transport = DJ_DEBUG_TRANSPORT_NATIVE,
        .hw_breakpoints = 0,
        .register_width_bits = 16,
    },
    {
        .proto = DJ_PROTO_SILABS_C2,
        .name = "silabs-c2-debug-physical",
        .caps = DJ_CAP_DEBUG_PHY | DJ_CAP_EXPERIMENTAL,
        .transport = DJ_DEBUG_TRANSPORT_NATIVE,
        .hw_breakpoints = 0,
        .register_width_bits = 8,
    },
};

const struct dj_debug_ops *dj_debug_ops_for(enum dj_proto_id proto) {
	for (size_t i = 0; i < sizeof(debug_ops) / sizeof(debug_ops[0]); ++i) {
		if (debug_ops[i].proto == proto)
			return &debug_ops[i];
	}
	return NULL;
}

static const struct dj_debug_ops *selected_ops(void) {
	const struct dj_backend *b = dj_selected_backend();
	return b ? dj_debug_ops_for(b->id) : NULL;
}

int dj_debug_get_info(struct dj_debug_info *info) {
	if (!info)
		return -EINVAL;
	memset(info, 0, sizeof(*info));
	const struct dj_backend *b = dj_selected_backend();
	if (!b)
		return -ENODEV;
	const struct dj_debug_ops *ops = dj_debug_ops_for(b->id);
	info->proto = b->id;
	info->state = DJ_DEBUG_DETACHED;
	if (!ops)
		return 0;
	info->transport = ops->transport;
	info->caps = ops->caps;
	info->hw_breakpoints = ops->hw_breakpoints;
	info->register_width_bits = ops->register_width_bits;
	if (ops->status)
		return ops->status(info);
	return 0;
}

int dj_debug_attach(void) {
	const struct dj_debug_ops *ops = selected_ops();
	return ops && ops->attach ? ops->attach() : -ENOTSUP;
}

int dj_debug_detach(void) {
	const struct dj_debug_ops *ops = selected_ops();
	return ops && ops->detach ? ops->detach() : -ENOTSUP;
}

int dj_debug_halt(void) {
	const struct dj_debug_ops *ops = selected_ops();
	return ops && ops->halt ? ops->halt() : -ENOTSUP;
}

int dj_debug_run(void) {
	const struct dj_debug_ops *ops = selected_ops();
	return ops && ops->run ? ops->run() : -ENOTSUP;
}

int dj_debug_step(void) {
	const struct dj_debug_ops *ops = selected_ops();
	return ops && ops->step ? ops->step() : -ENOTSUP;
}

int dj_debug_reset(bool halt_after_reset) {
	const struct dj_debug_ops *ops = selected_ops();
	return ops && ops->reset ? ops->reset(halt_after_reset) : -ENOTSUP;
}

int dj_debug_read_regs(uint8_t *out, size_t *len) {
	const struct dj_debug_ops *ops = selected_ops();
	return ops && ops->read_regs ? ops->read_regs(out, len) : -ENOTSUP;
}

int dj_debug_write_regs(const uint8_t *in, size_t len) {
	const struct dj_debug_ops *ops = selected_ops();
	return ops && ops->write_regs ? ops->write_regs(in, len) : -ENOTSUP;
}

int dj_debug_set_breakpoint(uint32_t address, uint8_t type, uint8_t slot) {
	const struct dj_debug_ops *ops = selected_ops();
	return ops && ops->set_breakpoint ? ops->set_breakpoint(address, type, slot) : -ENOTSUP;
}

int dj_debug_clear_breakpoint(uint8_t slot) {
	const struct dj_debug_ops *ops = selected_ops();
	return ops && ops->clear_breakpoint ? ops->clear_breakpoint(slot) : -ENOTSUP;
}

void dj_debug_backend_changed(void) {
	openocd_state = DJ_DEBUG_DETACHED;
	stm8_state = DJ_DEBUG_DETACHED;
	dspic_state = DJ_DEBUG_DETACHED;
	dj_dspic_debug_backend_changed();
}
