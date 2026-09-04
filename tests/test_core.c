#include "djpk4/dspic_common.h"
#include "djprog/backend.h"
#include "djprog/common.h"
#include "djprog/debug.h"
#include "djprog/dspic_debug.h"
#include "djprog/hw.h"
#include "djprog/msp430.h"
#include "djprog/safety.h"
#include "djprog/swim.h"
#include "djprog/swo.h"
#include "djprog/tms320.h"
#include "djprog/usb_proto.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static int zinit(void) {
	return 0;
}
static int zcfg(const struct dj_pinmap *m) {
	(void)m;
	return 0;
}
static int zdir(enum dj_pin_role r, enum dj_dir d) {
	(void)r;
	(void)d;
	return 0;
}
static int zw(enum dj_pin_role r, bool v) {
	(void)r;
	(void)v;
	return 0;
}
static int zr(enum dj_pin_role r, bool *v) {
	(void)r;
	*v = false;
	return 0;
}
static int zclk(enum dj_pin_role a, enum dj_pin_role b, enum dj_pin_role c, const uint8_t *d,
                uint8_t *e, size_t f, bool g, uint32_t h) {
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	if (e)
		memset(e, 0, (f + 7) / 8);
	(void)g;
	(void)h;
	return 0;
}
static int zp(enum dj_power_mode m) {
	(void)m;
	return 0;
}
static int zb(bool v) {
	(void)v;
	return 0;
}
static int zm(struct dj_measurement *m) {
	m->vtarget_mv = 3300;
	m->vpp_mv = 12100;
	m->itarget_ma = 42;
	m->power_fault = false;
	return 0;
}
static void zd(uint32_t u) {
	(void)u;
}

static void arm_safety(uint32_t flags, uint32_t uses) {
	const char phrase[] = DJ_SAFETY_CONFIRM_PHRASE;
	struct djp2_frame q = {.cmd = DJP2_SAFETY_ARM, .seq = 99}, r = {0};
	dj_put_le32(q.payload + 0, flags);
	dj_put_le32(q.payload + 4, uses);
	q.payload[8] = (uint8_t)(sizeof(phrase) - 1u);
	memcpy(q.payload + 9, phrase, sizeof(phrase) - 1u);
	q.len = 9u + (uint32_t)(sizeof(phrase) - 1u);
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.status == DJP2_OK);
}

static const struct dj_hw_ops ops = {.init = zinit,
                                     .configure = zcfg,
                                     .dir = zdir,
                                     .write = zw,
                                     .read = zr,
                                     .clock_bits = zclk,
                                     .power = zp,
                                     .vpp_boost = zb,
                                     .vpp_apply = zb,
                                     .hv_data0_apply = zb,
                                     .measure = zm,
                                     .delay_us = zd};

int main(void) {
	dj_hw_bind(&ops);
	struct dj_pinmap pm = {{2, 3, 4, 5, 6, 7}};
	assert(dj_hw_set_pinmap(&pm) == 0);

	assert(dj_backend_count() >= 17);
	assert(dj_backend_by_id(DJ_PROTO_DSPIC_ICSP));
	assert(dj_backend_by_id(DJ_PROTO_PIC24_ICSP));
	assert(dj_backend_by_id(DJ_PROTO_PIC_ICSP_RAW));
	assert(dj_backend_by_id(DJ_PROTO_AVR_ISP));
	assert(dj_backend_by_id(DJ_PROTO_AVR_PDI));
	assert(dj_backend_by_id(DJ_PROTO_MSP430_SBW));
	assert(dj_backend_by_id(DJ_PROTO_MSP430_JTAG));
	assert(dj_backend_by_id(DJ_PROTO_TMS320_C2000_JTAG));
	assert(dj_backend_by_id(DJ_PROTO_TI_SIMPLELINK_SWD));
	assert(dj_backend_by_id(DJ_PROTO_TI_SIMPLELINK_CJTAG));
	assert(dj_backend_by_id(DJ_PROTO_SILABS_C2));

	struct dj_target_cfg c = {
	    .proto = DJ_PROTO_AVR_ISP, .clock_hz = 125000, .power = DJ_PWR_5V, .page_size = 128};
	assert(dj_select_backend(&c) == 0);
	assert(dj_selected_backend()->id == DJ_PROTO_AVR_ISP);

	struct djp2_frame q = {.cmd = DJP2_HELLO, .seq = 7}, r = {0}, dec = {0};
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.status == 0 && r.len > 10);

	q.cmd = DJP2_STATUS;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 36);

	q.cmd = DJP2_MEASURE;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 16);
	assert(r.payload[0] == 0xe4 && r.payload[1] == 0x0c); /* 3300 LE */

	q.cmd = DJP2_LIST_DEVICES;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len > 20);

	q.cmd = DJP2_SET_PINMAP;
	q.len = DJ_PIN_COUNT;
	for (unsigned i = 0; i < DJ_PIN_COUNT; i++)
		q.payload[i] = (uint8_t)(18 + i);
	assert(djp2_dispatch(&q, &r) == 0);
	assert(dj_hw_pinmap()->gpio[0] == 18);
	assert(dj_hw_set_pinmap(&pm) == 0);

	q.cmd = DJP2_VPP;
	q.len = 2;
	q.payload[0] = 0;
	q.payload[1] = 1;
	assert(djp2_dispatch(&q, &r) == -EPERM);
	assert(r.status == DJP2_E_POWER);
	arm_safety(DJ_SAFETY_VPP, 1);
	assert(djp2_dispatch(&q, &r) == 0);

	q.cmd = DJP2_PHY_INFO;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len >= 4);

	/* Electrical script VM: read DATA0 through DJP2_SCRIPT_XFER. */
	q.cmd = DJP2_SCRIPT_XFER;
	q.len = 3;
	q.payload[0] = 0x03;
	q.payload[1] = DJ_PIN_DATA0;
	q.payload[2] = 0x00;
	assert(djp2_dispatch(&q, &r) == -EPERM);
	arm_safety(DJ_SAFETY_SCRIPT, 1);
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 1 && r.payload[0] == 0);

	/* dsPIC Debug-Executive control plane: mock mode exercises the native
	 * DJP2 debugger API without bundling proprietary Microchip DE binaries. */
	struct dj_target_cfg md = {
	    .proto = DJ_PROTO_DSPIC_ICSP, .clock_hz = 1000000, .power = DJ_PWR_EXTERNAL};
	strcpy(md.device, "mock-dspic30f5011");
	assert(dj_select_backend(&md) == 0);
	q.cmd = DJP2_DEBUG_INFO;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 12);
	assert(r.payload[3] == DJ_DEBUG_TRANSPORT_NATIVE);
	q.cmd = DJP2_DEBUG_ATTACH;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_DEBUG_HALT;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_DEBUG_REG_READ;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == DJ_DSPIC_DEBUG_REG_BYTES);
	q.cmd = DJP2_DEBUG_BP_SET;
	q.len = 6;
	q.payload[0] = 0x00;
	q.payload[1] = 0x02;
	q.payload[2] = 0x01;
	q.payload[3] = 0x00;
	q.payload[4] = 0;
	q.payload[5] = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_DEBUG_RUN;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_DEBUG_DETACH;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);

	/* OpenOCD remote_bitbang transport: verify both SWD and JTAG paths. */
	struct dj_target_cfg ds = {
	    .proto = DJ_PROTO_ARM_SWD, .clock_hz = 1000000, .power = DJ_PWR_EXTERNAL};
	assert(dj_select_backend(&ds) == 0);
	q.cmd = DJP2_DEBUG_BITBANG;
	q.len = 5;
	memcpy(q.payload, "Odczo", 5); /* drive, clocks, sample, delay, input */
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 1 && r.payload[0] == '0');

	struct dj_target_cfg jt = {
	    .proto = DJ_PROTO_JTAG, .clock_hz = 1000000, .power = DJ_PWR_EXTERNAL};
	assert(dj_select_backend(&jt) == 0);
	q.cmd = DJP2_DEBUG_BITBANG;
	q.len = 4;
	memcpy(q.payload, "0R7R", 4);
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 2 && r.payload[0] == '0' && r.payload[1] == '0');

	/* First-class debugger control API: OpenOCD owns ARM/JTAG run control,
	 * while DJP2 reports transport readiness without falsely claiming halt/step. */
	q.cmd = DJP2_DEBUG_INFO;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 12);
	assert(r.payload[2] == DJ_DEBUG_DETACHED);
	assert(r.payload[3] == DJ_DEBUG_TRANSPORT_OPENOCD_REMOTE_BITBANG);
	q.cmd = DJP2_DEBUG_ATTACH;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_DEBUG_INFO;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.payload[2] == DJ_DEBUG_TRANSPORT_READY);
	q.cmd = DJP2_DEBUG_HALT;
	assert(djp2_dispatch(&q, &r) == -ENOTSUP);
	assert(r.status == DJP2_E_UNSUPPORTED);
	q.cmd = DJP2_DEBUG_DETACH;
	assert(djp2_dispatch(&q, &r) == 0);

	/* MSP430 2-wire and 4-wire raw TAP paths: these are intentionally low-level
	 * physical/debug primitives, not destructive memory algorithms. */
	struct dj_target_cfg sbw = {
	    .proto = DJ_PROTO_MSP430_SBW, .clock_hz = 100000, .power = DJ_PWR_EXTERNAL};
	strcpy(sbw.device, "msp430g2553");
	assert(dj_select_backend(&sbw) == 0);
	q.cmd = DJP2_ENTER;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_RAW_XFER;
	q.len = 2;
	q.payload[0] = DJ_MSP430_RAW_TAP_RESET;
	q.payload[1] = 8;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 0);
	q.cmd = DJP2_RAW_XFER;
	q.len = 4;
	q.payload[0] = DJ_MSP430_RAW_SHIFT_IR;
	q.payload[1] = 8;
	q.payload[2] = 0;
	q.payload[3] = 0x91;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 1);
	q.cmd = DJP2_DEBUG_INFO;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 12 && r.payload[3] == DJ_DEBUG_TRANSPORT_NATIVE);

	struct dj_target_cfg mspj = {
	    .proto = DJ_PROTO_MSP430_JTAG, .clock_hz = 200000, .power = DJ_PWR_EXTERNAL};
	strcpy(mspj.device, "msp430f5529");
	assert(dj_select_backend(&mspj) == 0);
	q.cmd = DJP2_ENTER;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_RAW_XFER;
	q.len = 5;
	q.payload[0] = DJ_MSP430_RAW_SHIFT_DR;
	q.payload[1] = 16;
	q.payload[2] = 0;
	q.payload[3] = 0xaa;
	q.payload[4] = 0x55;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 2);

	/* TI TMS320/C2000 XDS110v3-style JTAG path: raw TAP operations and
	 * OpenOCD remote_bitbang transport are separate from MSP430. */
	struct dj_target_cfg c2k = {
	    .proto = DJ_PROTO_TMS320_C2000_JTAG, .clock_hz = 1000000, .power = DJ_PWR_EXTERNAL};
	strcpy(c2k.device, "TMS320F2800137");
	assert(dj_select_backend(&c2k) == 0);
	q.cmd = DJP2_ENTER;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_IDENTIFY;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 4);
	q.cmd = DJP2_RAW_XFER;
	q.len = 2;
	q.payload[0] = DJ_TMS320_RAW_TAP_RESET;
	q.payload[1] = 8;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 0);
	q.cmd = DJP2_RAW_XFER;
	q.len = 4;
	q.payload[0] = DJ_TMS320_RAW_SHIFT_IR;
	q.payload[1] = 6;
	q.payload[2] = 0;
	q.payload[3] = 0x3f;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 1);
	q.cmd = DJP2_DEBUG_INFO;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 12 && r.payload[3] == DJ_DEBUG_TRANSPORT_OPENOCD_REMOTE_BITBANG);
	q.cmd = DJP2_DEBUG_ATTACH;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_DEBUG_BITBANG;
	q.len = 4;
	memcpy(q.payload, "0R7R", 4);
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 2);
	q.cmd = DJP2_DEBUG_DETACH;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);

	/* STM8 native debug mock: verifies the non-ARM run-control path without
	 * pretending to have real target hardware in CI. */
	struct dj_target_cfg sw = {
	    .proto = DJ_PROTO_STM8_SWIM, .clock_hz = 363000, .power = DJ_PWR_EXTERNAL};
	strcpy(sw.device, "mock-stm8s003f3");
	assert(dj_select_backend(&sw) == 0);
	q.cmd = DJP2_ENTER;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_DEBUG_INFO;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 12);
	assert(r.payload[3] == DJ_DEBUG_TRANSPORT_NATIVE);
	assert(((uint32_t)r.payload[4] | ((uint32_t)r.payload[5] << 8) |
	        ((uint32_t)r.payload[6] << 16) | ((uint32_t)r.payload[7] << 24)) &
	       DJ_CAP_DEBUG_RUNCTRL);
	q.cmd = DJP2_DEBUG_ATTACH;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_DEBUG_HALT;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_DEBUG_INFO;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.payload[2] == DJ_DEBUG_HALTED);
	q.cmd = DJP2_DEBUG_REG_READ;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == STM8_CPU_REG_COUNT);
	for (size_t i = 0; i < STM8_CPU_REG_COUNT; i++)
		q.payload[i] = (uint8_t)(0x10 + i);
	q.cmd = DJP2_DEBUG_REG_WRITE;
	q.len = STM8_CPU_REG_COUNT;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_DEBUG_BP_SET;
	q.len = 6;
	q.payload[0] = 0x34;
	q.payload[1] = 0x12;
	q.payload[2] = 0x80;
	q.payload[3] = 0x00;
	q.payload[4] = 0;
	q.payload[5] = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	uint8_t bp[3] = {0};
	assert(dj_swim_mock_read(STM8_DM_BK1E, bp, 3) == 0);
	assert(bp[0] == 0x80 && bp[1] == 0x12 && bp[2] == 0x34);
	q.cmd = DJP2_DEBUG_STEP;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_DEBUG_RUN;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_DEBUG_DETACH;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);

	q.cmd = DJP2_ENTER;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);

	/* SEGGER RTT control block support: use the STM8 mock memory as a safe
	 * target RAM model.  This verifies scanning, up-buffer reads with RdOff
	 * update, and down-buffer writes with WrOff update. */
	uint8_t rtt[72];
	memset(rtt, 0, sizeof(rtt));
	memcpy(rtt, "SEGGER RTT", 10);
	dj_put_le32(rtt + 16, 1);          /* MaxNumUpBuffers */
	dj_put_le32(rtt + 20, 1);          /* MaxNumDownBuffers */
	dj_put_le32(rtt + 24 + 0, 0x0180); /* up0 sName */
	dj_put_le32(rtt + 24 + 4, 0x0200); /* up0 pBuffer */
	dj_put_le32(rtt + 24 + 8, 16);     /* up0 SizeOfBuffer */
	dj_put_le32(rtt + 24 + 12, 5);     /* up0 WrOff */
	dj_put_le32(rtt + 24 + 16, 0);     /* up0 RdOff */
	dj_put_le32(rtt + 48 + 0, 0x0190); /* down0 sName */
	dj_put_le32(rtt + 48 + 4, 0x0300); /* down0 pBuffer */
	dj_put_le32(rtt + 48 + 8, 8);      /* down0 SizeOfBuffer */
	assert(dj_swim_mock_write(0x0100, rtt, sizeof(rtt)) == 0);
	assert(dj_swim_mock_write(0x0180, (const uint8_t *)"Terminal", 9) == 0);
	assert(dj_swim_mock_write(0x0190, (const uint8_t *)"Input", 6) == 0);
	assert(dj_swim_mock_write(0x0200, (const uint8_t *)"hello", 5) == 0);
	q.cmd = DJP2_RTT_SCAN;
	q.len = 8;
	dj_put_le32(q.payload + 0, 0x0000);
	dj_put_le32(q.payload + 4, 0x0800);
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 4 && dj_le32(r.payload) == 0x0100);
	q.cmd = DJP2_RTT_INFO;
	q.len = 4;
	dj_put_le32(q.payload, 0x0100);
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 60 && dj_le32(r.payload + 4) == 1 && dj_le32(r.payload + 8) == 1);

	q.cmd = DJP2_RTT_CHANNEL_INFO;
	q.len = 6;
	dj_put_le32(q.payload + 0, 0x0100);
	q.payload[4] = 0;
	q.payload[5] = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 44 && r.payload[0] == 0 && r.payload[1] == 0 && r.payload[2] == 8);
	assert(dj_le32(r.payload + 8) == 0x0200 && memcmp(r.payload + 36, "Terminal", 8) == 0);
	q.cmd = DJP2_RTT_READ;
	q.len = 7;
	dj_put_le32(q.payload + 0, 0x0100);
	q.payload[4] = 0;
	dj_put_le16(q.payload + 5, 16);
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 5 && memcmp(r.payload, "hello", 5) == 0);
	uint8_t rdchk[4];
	assert(dj_swim_mock_read(0x0100 + 24 + 16, rdchk, 4) == 0);
	assert(dj_le32(rdchk) == 5);
	q.cmd = DJP2_RTT_WRITE;
	q.len = 8;
	dj_put_le32(q.payload + 0, 0x0100);
	q.payload[4] = 0;
	memcpy(q.payload + 5, "cmd", 3);
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 4 && dj_le32(r.payload) == 3);
	uint8_t downchk[4];
	assert(dj_swim_mock_read(0x0300, downchk, 3) == 0);
	assert(memcmp(downchk, "cmd", 3) == 0);

	/* SWO/ITM capture control plane: Rev O keeps hardware capture isolated from
	 * target semantics, while providing the same USB/host contract that a PIO
	 * receiver will use. */
	q.cmd = DJP2_SWO_CONFIG;
	q.len = 8;
	dj_put_le32(q.payload + 0, 2000000u);
	dj_put_le32(q.payload + 4, 0u);
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_SWO_START;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(dj_swo_mock_feed((const uint8_t *)"ITM", 3) == 0);
	q.cmd = DJP2_SWO_STATUS;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 20 && dj_le32(r.payload + 8) == 3 && r.payload[16] == 1);
	q.cmd = DJP2_SWO_READ;
	q.len = 2;
	dj_put_le16(q.payload, 16);
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 3 && memcmp(r.payload, "ITM", 3) == 0);
	q.cmd = DJP2_SWO_STOP;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);

	/* Vendor feature implementation pack: generic bridge modes and power trace. */
	q.cmd = DJP2_BRIDGE_INFO;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len > 10);
	q.cmd = DJP2_BRIDGE_GPIO;
	q.len = 2;
	q.payload[0] = 0;
	q.payload[1] = DJ_PIN_DATA0;
	assert(djp2_dispatch(&q, &r) == -EPERM);
	arm_safety(DJ_SAFETY_BRIDGE, 2);
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 1);
	q.cmd = DJP2_BRIDGE_SPI;
	q.len = 12;
	q.payload[0] = 0;
	q.payload[1] = DJ_PIN_RESET;
	dj_put_le16(q.payload + 2, 0);
	dj_put_le32(q.payload + 4, 100000);
	dj_put_le16(q.payload + 8, 2);
	q.payload[10] = 0x9f;
	q.payload[11] = 0x00;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 2);
	q.cmd = DJP2_POWER_TRACE;
	q.len = 8;
	dj_put_le16(q.payload + 0, 3);
	dj_put_le16(q.payload + 2, 0);
	dj_put_le32(q.payload + 4, 0);
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 48);
	assert(dj_le32(r.payload + 4) == 3300u && dj_le32(r.payload + 8) == 12100u);

	/* TI SimpleLink CC13xx/CC26xx support: SWD OpenOCD transport plus cJTAG
	 * physical profile, both kept separate from C2000 and MSP430. */
	struct dj_target_cfg sl = {
	    .proto = DJ_PROTO_TI_SIMPLELINK_SWD, .clock_hz = 1000000, .power = DJ_PWR_EXTERNAL};
	strcpy(sl.device, "CC2652R");
	assert(dj_select_backend(&sl) == 0);
	q.cmd = DJP2_ENTER;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_IDENTIFY;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len > 20);
	q.cmd = DJP2_DEBUG_INFO;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 12 && r.payload[3] == DJ_DEBUG_TRANSPORT_OPENOCD_REMOTE_BITBANG);
	q.cmd = DJP2_DEBUG_ATTACH;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_DEBUG_DETACH;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	struct dj_target_cfg slc = {
	    .proto = DJ_PROTO_TI_SIMPLELINK_CJTAG, .clock_hz = 500000, .power = DJ_PWR_EXTERNAL};
	strcpy(slc.device, "CC1352P7");
	assert(dj_select_backend(&slc) == 0);
	q.cmd = DJP2_ENTER;
	q.len = 0;
	assert(djp2_dispatch(&q, &r) == 0);
	q.cmd = DJP2_RAW_XFER;
	q.len = 3;
	q.payload[0] = 0x01;
	q.payload[1] = 8;
	q.payload[2] = 0xff;
	assert(djp2_dispatch(&q, &r) == 0);
	assert(r.len == 1);

	uint8_t buf[4096];
	size_t n = djp2_encode(&r, buf, sizeof(buf));
	assert(n == DJP2_HDR_SIZE + r.len);
	assert(djp2_decode(buf, n, &dec) == 0);
	assert(dec.seq == 7 && dec.len == r.len && !memcmp(dec.payload, r.payload, r.len));

	uint32_t in[4] = {0x123456, 0xabcdef, 0x010203, 0xfefdfc}, out[4] = {0};
	uint16_t packed[6];
	dspic_pack4(in, packed);
	dspic_unpack4(packed, out);
	assert(!memcmp(in, out, sizeof(in)));

	puts("test_core: PASS");
	return 0;
}
