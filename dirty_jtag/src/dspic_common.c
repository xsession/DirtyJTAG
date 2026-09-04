/* SPDX-License-Identifier: MIT */
#include "djpk4/dspic_common.h"
#include "djpk4/common.h"
#include "djpk4/icsp.h"
#include <errno.h>

static const struct dj_icsp_hal *g_hal;

void dj_icsp_bind(const struct dj_icsp_hal *hal) {
	g_hal = hal;
}
const struct dj_icsp_hal *dj_icsp_hal(void) {
	return g_hal;
}

int dj_icsp_six(uint32_t instruction, uint32_t hz) {
	if (!g_hal || !g_hal->write_bits)
		return -ENODEV;
	int rc = g_hal->write_bits(0u, 4, false, hz);
	if (rc)
		return rc;
	return g_hal->write_bits(instruction & DJ_WORD24_MASK, 24, false, hz);
}

int dj_icsp_regout(uint16_t *value, uint32_t hz) {
	if (!g_hal || !g_hal->write_bits || !g_hal->read_bits || !g_hal->set_pgd_output)
		return -ENODEV;
	int rc = g_hal->set_pgd_output(true);
	if (rc)
		return rc;
	rc = g_hal->write_bits(1u, 4, false, hz); /* REGOUT control code = 0001, LSB first */
	if (rc)
		return rc;
	rc = g_hal->set_pgd_output(false);
	if (rc)
		return rc;
	uint32_t discard = 0;
	rc = g_hal->read_bits(&discard, 8, true, hz);
	if (rc)
		return rc;
	uint32_t v = 0;
	rc = g_hal->read_bits(&v, 16, true, hz);
	if (rc)
		return rc;
	*value = (uint16_t)v;
	return g_hal->set_pgd_output(true);
}

int dj_icsp_idle_clocks(unsigned clocks, uint32_t hz) {
	if (!g_hal || !g_hal->write_bits)
		return -ENODEV;
	while (clocks) {
		unsigned n = clocks > 32 ? 32 : clocks;
		int rc = g_hal->write_bits(0u, n, false, hz);
		if (rc)
			return rc;
		clocks -= n;
	}
	return 0;
}

uint32_t dspic_mov_lit(unsigned wreg, uint16_t literal) {
	/* dsPIC MOV #lit16,Wn encoding: 0010 kkkk kkkk kkkk kkkk 000n */
	return 0x200000u | ((uint32_t)literal << 4) | (wreg & 0x0fu);
}

int dspic_exec_hz(uint32_t instruction, uint32_t hz) {
	return dj_icsp_six(instruction, hz);
}
int dspic_exec(uint32_t instruction) {
	return dj_icsp_six(instruction, 1000000u);
}

int dspic_exit_reset(uint32_t goto_insn, unsigned nops_before, unsigned nops_after) {
	int rc;
	if (nops_before == 0 && nops_after == 0) {
		/* Classic dsPIC30F/dsPIC33F tables require GOTO, GOTO, NOP. */
		if ((rc = dspic_exec(goto_insn)))
			return rc;
		if ((rc = dspic_exec(goto_insn)))
			return rc;
		return dspic_exec(DSPIC_NOP);
	}
	for (unsigned i = 0; i < nops_before; ++i)
		if ((rc = dspic_exec(DSPIC_NOP)))
			return rc;
	if ((rc = dspic_exec(goto_insn)))
		return rc;
	for (unsigned i = 0; i < nops_after; ++i)
		if ((rc = dspic_exec(DSPIC_NOP)))
			return rc;
	return 0;
}

int dspic_set_tbl_address(uint32_t pc_addr, unsigned ptr_wreg, uint32_t mov_tblpag_opcode) {
	/* Program-space PC addresses are even; TBLPAG gets the upper byte and pointer gets low 16 bits.
	 */
	int rc = dspic_exec(dspic_mov_lit(0, (uint16_t)(pc_addr >> 16)));
	if (rc)
		return rc;
	rc = dspic_exec(mov_tblpag_opcode); /* MOV W0,TBLPAG */
	if (rc)
		return rc;
	return dspic_exec(dspic_mov_lit(ptr_wreg, (uint16_t)pc_addr));
}

void dspic_pack4(const uint32_t in[4], uint16_t w[6]) {
	w[0] = (uint16_t)in[0];
	w[1] = (uint16_t)(((in[1] >> 16) & 0xffu) << 8) | (uint16_t)((in[0] >> 16) & 0xffu);
	w[2] = (uint16_t)in[1];
	w[3] = (uint16_t)in[2];
	w[4] = (uint16_t)(((in[3] >> 16) & 0xffu) << 8) | (uint16_t)((in[2] >> 16) & 0xffu);
	w[5] = (uint16_t)in[3];
}

void dspic_unpack4(const uint16_t w[6], uint32_t out[4]) {
	out[0] = (uint32_t)w[0] | ((uint32_t)(w[1] & 0x00ffu) << 16);
	out[1] = (uint32_t)w[2] | ((uint32_t)(w[1] & 0xff00u) << 8);
	out[2] = (uint32_t)w[3] | ((uint32_t)(w[4] & 0x00ffu) << 16);
	out[3] = (uint32_t)w[5] | ((uint32_t)(w[4] & 0xff00u) << 8);
}

int dspic_load_wregs_0_5(const uint16_t w[6]) {
	for (unsigned i = 0; i < 6; ++i) {
		int rc = dspic_exec(dspic_mov_lit(i, w[i]));
		if (rc)
			return rc;
	}
	return 0;
}

int dspic_table_load4(unsigned nop_count) {
	/* Common four-instruction latch pattern used by classic dsPIC30/33F/33E. */
	static const uint32_t tbl[] = {0xBB0BB6u, 0xBBDBB6u, 0xBBEBB6u, 0xBB1BB6u,
	                               0xBB0BB6u, 0xBBDBB6u, 0xBBEBB6u, 0xBB1BB6u};
	int rc = dspic_exec(0xEB0300u); /* CLR W6 */
	if (rc)
		return rc;
	if ((rc = dspic_exec(DSPIC_NOP)))
		return rc;
	for (size_t i = 0; i < DJ_ARRAY_SIZE(tbl); ++i) {
		if ((rc = dspic_exec(tbl[i])))
			return rc;
		for (unsigned n = 0; n < nop_count; ++n)
			if ((rc = dspic_exec(DSPIC_NOP)))
				return rc;
	}
	return 0;
}

int dspic_read4_common(uint32_t pc_addr, uint32_t out[4], uint32_t goto_insn,
                       uint32_t mov_tblpag_opcode, uint32_t mov_visi_base, unsigned tbl_nops) {
	static const uint32_t rd[] = {0xBA1B96u, 0xBADBB6u, 0xBADBD6u, 0xBA1BB6u,
	                              0xBA1B96u, 0xBADBB6u, 0xBADBD6u, 0xBA0BB6u};
	int rc = dspic_exit_reset(goto_insn, goto_insn == DSPIC_GOTO_200 ? 3 : 0,
	                          goto_insn == DSPIC_GOTO_200 ? 3 : 0);
	if (rc)
		return rc;
	rc = dspic_set_tbl_address(pc_addr, 6, mov_tblpag_opcode);
	if (rc)
		return rc;
	if ((rc = dspic_exec(0xEB0380u)))
		return rc; /* CLR W7 */
	if (goto_insn == DSPIC_GOTO_200 && (rc = dspic_exec(DSPIC_NOP)))
		return rc;
	for (size_t i = 0; i < DJ_ARRAY_SIZE(rd); ++i) {
		if ((rc = dspic_exec(rd[i])))
			return rc;
		for (unsigned n = 0; n < tbl_nops; ++n)
			if ((rc = dspic_exec(DSPIC_NOP)))
				return rc;
	}
	uint16_t w[6];
	for (unsigned i = 0; i < 6; ++i) {
		if ((rc = dspic_exec(mov_visi_base + i)))
			return rc;
		if ((rc = dspic_exec(DSPIC_NOP)))
			return rc;
		if ((rc = dj_icsp_regout(&w[i], 1000000u)))
			return rc;
		if ((rc = dspic_exec(DSPIC_NOP)))
			return rc;
	}
	dspic_unpack4(w, out);
	return 0;
}

int dspic_poll_wr(uint32_t mov_nvmcon_w0, uint32_t mov_w0_visi, uint32_t goto_insn,
                  unsigned timeout_ms, unsigned extra_nops) {
	const struct dj_icsp_hal *hal = dj_icsp_hal();
	if (!hal)
		return -ENODEV;
	for (unsigned ms = 0; ms < timeout_ms; ++ms) {
		int rc = dspic_exec(DSPIC_NOP);
		if (rc)
			return rc;
		rc = dspic_exec(mov_nvmcon_w0);
		if (rc)
			return rc;
		rc = dspic_exec(DSPIC_NOP);
		if (rc)
			return rc;
		rc = dspic_exec(mov_w0_visi);
		if (rc)
			return rc;
		rc = dspic_exec(DSPIC_NOP);
		if (rc)
			return rc;
		uint16_t nvmcon = 0;
		rc = dj_icsp_regout(&nvmcon, 1000000u);
		if (rc)
			return rc;
		for (unsigned n = 0; n < extra_nops; ++n)
			if ((rc = dspic_exec(DSPIC_NOP)))
				return rc;
		rc = dspic_exit_reset(goto_insn, goto_insn == DSPIC_GOTO_200 ? 3 : 0,
		                      goto_insn == DSPIC_GOTO_200 ? 3 : 0);
		if (rc)
			return rc;
		if ((nvmcon & 0x8000u) == 0u)
			return 0;
		hal->delay_us(1000);
	}
	return -ETIMEDOUT;
}
