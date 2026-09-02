/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_RTT_H
#define DJPROG_RTT_H
#include <stddef.h>
#include <stdint.h>

#define DJ_RTT_ID "SEGGER RTT"
#define DJ_RTT_ID_LEN 10u
#define DJ_RTT_CB_ID_SIZE 16u
#define DJ_RTT_CB_HEADER_SIZE 24u
#define DJ_RTT_BUFFER_DESC_SIZE 24u
#define DJ_RTT_MAX_SCAN_CHUNK 128u
#define DJ_RTT_MAX_CHANNELS 16u

struct dj_rtt_info {
    uint32_t cb_addr;
    uint32_t max_up;
    uint32_t max_down;
    uint32_t up_name;
    uint32_t up_buffer;
    uint32_t up_size;
    uint32_t up_wr;
    uint32_t up_rd;
    uint32_t up_flags;
    uint32_t down_name;
    uint32_t down_buffer;
    uint32_t down_size;
    uint32_t down_wr;
    uint32_t down_rd;
    uint32_t down_flags;
};

struct dj_rtt_channel_info {
    uint32_t name_addr;
    uint32_t buffer_addr;
    uint32_t size;
    uint32_t wr_off;
    uint32_t rd_off;
    uint32_t flags;
    uint32_t used;
    uint32_t free_space;
    uint8_t direction;
    uint8_t channel;
    uint8_t name_len;
    char name[32];
};

int dj_rtt_scan(uint32_t start, uint32_t end, uint32_t *cb_addr);
int dj_rtt_get_info(uint32_t cb_addr, struct dj_rtt_info *info);
int dj_rtt_get_channel_info(uint32_t cb_addr, uint8_t direction, uint8_t channel, struct dj_rtt_channel_info *info);
int dj_rtt_read_up(uint32_t cb_addr, uint8_t channel, uint8_t *out, size_t *len);
int dj_rtt_write_down(uint32_t cb_addr, uint8_t channel, const uint8_t *data, size_t len, size_t *written);

#endif
