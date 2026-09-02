/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_BACKEND_H
#define DJPROG_BACKEND_H
#include <stddef.h>
#include <stdint.h>
#include "djprog/hw.h"

#define DJ_CAP_IDENTIFY      (1u<<0)
#define DJ_CAP_ERASE         (1u<<1)
#define DJ_CAP_READ          (1u<<2)
#define DJ_CAP_WRITE         (1u<<3)
#define DJ_CAP_RAW_XFER      (1u<<4)
#define DJ_CAP_HV            (1u<<5)
#define DJ_CAP_DEBUG_PHY     (1u<<6)
#define DJ_CAP_TARGET_POWER  (1u<<7)
#define DJ_CAP_DEBUG_RUNCTRL (1u<<8)
#define DJ_CAP_DEBUG_REGS    (1u<<9)
#define DJ_CAP_DEBUG_BREAK   (1u<<10)
#define DJ_CAP_DEBUG_OPENOCD (1u<<11)
#define DJ_CAP_BRIDGE        (1u<<12)
#define DJ_CAP_POWER_TRACE   (1u<<13)
#define DJ_CAP_BOOTLOADER    (1u<<14)
#define DJ_CAP_EXPERIMENTAL  (1u<<31)

enum dj_proto_id {
    DJ_PROTO_NONE=0,
    DJ_PROTO_DSPIC_ICSP=1,
    DJ_PROTO_PIC24_ICSP=2,
    DJ_PROTO_PIC_ICSP_RAW=3,
    DJ_PROTO_AVR_ISP=10,
    DJ_PROTO_AVR_UPDI=11,
    DJ_PROTO_AVR_TPI=12,
    DJ_PROTO_AVR_PDI=13,
    DJ_PROTO_STM8_SWIM=20,
    DJ_PROTO_MSP430_SBW=30,
    DJ_PROTO_MSP430_JTAG=31,
    DJ_PROTO_SILABS_C2=40,
    DJ_PROTO_RENESAS_RL78=50,
    DJ_PROTO_ARM_SWD=60,
    DJ_PROTO_JTAG=61,
    DJ_PROTO_TMS320_C2000_JTAG=70,
    DJ_PROTO_TI_SIMPLELINK_SWD=80,
    DJ_PROTO_TI_SIMPLELINK_CJTAG=81,
};

struct dj_target_cfg {
    enum dj_proto_id proto;
    char device[48];
    uint32_t clock_hz;
    enum dj_power_mode power;
    uint32_t vtarget_mv;
    uint32_t flash_size;
    uint16_t page_size;
    uint16_t flags;
};
struct dj_backend {
    enum dj_proto_id id;
    const char *name;
    uint32_t caps;
    uint32_t default_hz;
    uint32_t max_hz;
    int (*select)(const struct dj_target_cfg *cfg);
    int (*enter)(void);
    int (*leave)(void);
    int (*identify)(uint8_t *out,size_t *len);
    int (*erase)(void);
    int (*read_mem)(uint32_t addr,uint8_t *data,size_t len);
    int (*write_mem)(uint32_t addr,const uint8_t *data,size_t len);
    int (*raw_xfer)(const uint8_t *tx,size_t txlen,uint8_t *rx,size_t *rxlen);
};
size_t dj_backend_count(void);
const struct dj_backend *dj_backend_at(size_t i);
const struct dj_backend *dj_backend_by_id(enum dj_proto_id id);
int dj_select_backend(const struct dj_target_cfg *cfg);
const struct dj_backend *dj_selected_backend(void);
const struct dj_target_cfg *dj_target_config(void);
#endif
