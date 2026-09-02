/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_USB_PROTO_H
#define DJPROG_USB_PROTO_H
#include <stddef.h>
#include <stdint.h>
#define DJP2_MAGIC 0x32504A44u /* DJP2 */
#define DJP2_VERSION 2u
#define DJP2_MAX_PAYLOAD 2048u
#define DJP2_HDR_SIZE 20u
enum djp2_cmd {
    DJP2_HELLO=1, DJP2_LIST_PROTOCOLS=2, DJP2_CONFIG=3, DJP2_STATUS=4,
    DJP2_LIST_DEVICES=5,
    DJP2_ENTER=0x10, DJP2_LEAVE=0x11, DJP2_IDENTIFY=0x12,
    DJP2_ERASE=0x20, DJP2_READ=0x21, DJP2_WRITE=0x22,
    DJP2_RAW_XFER=0x30, DJP2_POWER=0x40, DJP2_MEASURE=0x41,
    DJP2_SET_PINMAP=0x42, DJP2_VPP=0x43, DJP2_PHY_INFO=0x44,
    DJP2_SCRIPT_XFER=0x45,
    DJP2_DEBUG_BITBANG=0x50,
    DJP2_DEBUG_INFO=0x51, DJP2_DEBUG_ATTACH=0x52, DJP2_DEBUG_DETACH=0x53,
    DJP2_DEBUG_HALT=0x54, DJP2_DEBUG_RUN=0x55, DJP2_DEBUG_STEP=0x56,
    DJP2_DEBUG_RESET=0x57, DJP2_DEBUG_REG_READ=0x58, DJP2_DEBUG_REG_WRITE=0x59,
    DJP2_DEBUG_BP_SET=0x5a, DJP2_DEBUG_BP_CLEAR=0x5b,
    DJP2_SAFE_IDLE=0x7e, DJP2_ABORT=0x7f
};
enum djp2_status {
    DJP2_OK=0, DJP2_E_FRAME=1, DJP2_E_CRC=2, DJP2_E_CMD=3,
    DJP2_E_ARG=4, DJP2_E_UNSUPPORTED=5, DJP2_E_IO=6,
    DJP2_E_POWER=7, DJP2_E_RANGE=8, DJP2_E_BUSY=9
};
struct djp2_frame {
    uint16_t cmd,seq,status,flags;
    uint32_t len;
    uint8_t payload[DJP2_MAX_PAYLOAD];
};
uint32_t dj_crc32(const void *data,size_t len,uint32_t seed);
size_t djp2_encode(const struct djp2_frame *f,uint8_t *out,size_t cap);
int djp2_decode(const uint8_t *buf,size_t len,struct djp2_frame *f);
int djp2_dispatch(const struct djp2_frame *req,struct djp2_frame *rsp);
#endif
