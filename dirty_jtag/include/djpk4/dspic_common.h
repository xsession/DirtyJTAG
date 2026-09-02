/* SPDX-License-Identifier: MIT */
#ifndef DJPK4_DSPIC_COMMON_H
#define DJPK4_DSPIC_COMMON_H
#include <stddef.h>
#include <stdint.h>
#include "djpk4/device.h"

#define DSPIC_NOP 0x000000u
#define DSPIC_GOTO_100 0x040100u
#define DSPIC_GOTO_200 0x040200u

uint32_t dspic_mov_lit(unsigned wreg, uint16_t literal);
int dspic_exec(uint32_t instruction);
int dspic_exec_hz(uint32_t instruction, uint32_t hz);
int dspic_exit_reset(uint32_t goto_insn, unsigned nops_before, unsigned nops_after);
int dspic_set_tbl_address(uint32_t pc_addr, unsigned ptr_wreg, uint32_t mov_tblpag_opcode);
void dspic_pack4(const uint32_t in[4], uint16_t w[6]);
void dspic_unpack4(const uint16_t w[6], uint32_t out[4]);
int dspic_load_wregs_0_5(const uint16_t w[6]);
int dspic_table_load4(unsigned nop_count);
int dspic_read4_common(uint32_t pc_addr, uint32_t out[4], uint32_t goto_insn,
                       uint32_t mov_tblpag_opcode, uint32_t mov_visi_base,
                       unsigned tbl_nops);
int dspic_poll_wr(uint32_t mov_nvmcon_w0, uint32_t mov_w0_visi, uint32_t goto_insn,
                  unsigned timeout_ms, unsigned extra_nops);
#endif
