/* SPDX-License-Identifier: MIT */
#include "djprog/rtt.h"
#include "djprog/backend.h"
#include "djprog/common.h"
#include <errno.h>
#include <string.h>

struct rtt_desc {
    uint32_t name;
    uint32_t buffer;
    uint32_t size;
    uint32_t wr;
    uint32_t rd;
    uint32_t flags;
};

static const struct dj_backend *mem_backend(void)
{
    const struct dj_backend *b = dj_selected_backend();
    if (!b || !b->read_mem || !b->write_mem) return NULL;
    return b;
}

static int read_target(uint32_t addr, void *data, size_t len)
{
    const struct dj_backend *b = mem_backend();
    if (!b) return -ENOTSUP;
    if (!data || len == 0u) return -EINVAL;
    return b->read_mem(addr, (uint8_t *)data, len);
}

static int write_target(uint32_t addr, const void *data, size_t len)
{
    const struct dj_backend *b = mem_backend();
    if (!b) return -ENOTSUP;
    if (!data || len == 0u) return -EINVAL;
    return b->write_mem(addr, (const uint8_t *)data, len);
}

static int read_u32(uint32_t addr, uint32_t *v)
{
    uint8_t b[4];
    int r;
    if (!v) return -EINVAL;
    r = read_target(addr, b, sizeof(b));
    if (r) return r;
    *v = dj_le32(b);
    return 0;
}

static int write_u32(uint32_t addr, uint32_t v)
{
    uint8_t b[4];
    dj_put_le32(b, v);
    return write_target(addr, b, sizeof(b));
}

static int read_desc(uint32_t addr, struct rtt_desc *d)
{
    uint8_t b[DJ_RTT_BUFFER_DESC_SIZE];
    int r;
    if (!d) return -EINVAL;
    r = read_target(addr, b, sizeof(b));
    if (r) return r;
    d->name = dj_le32(b + 0);
    d->buffer = dj_le32(b + 4);
    d->size = dj_le32(b + 8);
    d->wr = dj_le32(b + 12);
    d->rd = dj_le32(b + 16);
    d->flags = dj_le32(b + 20);
    return 0;
}

static int read_header(uint32_t cb_addr, uint32_t *max_up, uint32_t *max_down)
{
    uint8_t id[DJ_RTT_CB_ID_SIZE];
    int r = read_target(cb_addr, id, sizeof(id));
    if (r) return r;
    if (memcmp(id, DJ_RTT_ID, DJ_RTT_ID_LEN) != 0) return -ENOENT;
    r = read_u32(cb_addr + 16u, max_up);
    if (r) return r;
    r = read_u32(cb_addr + 20u, max_down);
    if (r) return r;
    if (*max_up > DJ_RTT_MAX_CHANNELS || *max_down > DJ_RTT_MAX_CHANNELS) return -ERANGE;
    return 0;
}

static uint32_t up_desc_addr(uint32_t cb_addr, uint32_t channel)
{
    return cb_addr + DJ_RTT_CB_HEADER_SIZE + channel * DJ_RTT_BUFFER_DESC_SIZE;
}

static uint32_t down_desc_addr(uint32_t cb_addr, uint32_t max_up, uint32_t channel)
{
    return cb_addr + DJ_RTT_CB_HEADER_SIZE + max_up * DJ_RTT_BUFFER_DESC_SIZE + channel * DJ_RTT_BUFFER_DESC_SIZE;
}

int dj_rtt_scan(uint32_t start, uint32_t end, uint32_t *cb_addr)
{
    uint8_t chunk[DJ_RTT_MAX_SCAN_CHUNK];
    uint32_t addr;
    const uint32_t step = DJ_RTT_MAX_SCAN_CHUNK - DJ_RTT_ID_LEN + 1u;
    if (!cb_addr || end <= start) return -EINVAL;
    if (!mem_backend()) return -ENOTSUP;
    *cb_addr = 0u;
    addr = start;
    while (addr < end) {
        size_t n = DJ_MIN((size_t)DJ_RTT_MAX_SCAN_CHUNK, (size_t)(end - addr));
        int r = read_target(addr, chunk, n);
        if (r) return r;
        if (n >= DJ_RTT_ID_LEN) {
            for (size_t i = 0; i <= n - DJ_RTT_ID_LEN; ++i) {
                if (memcmp(chunk + i, DJ_RTT_ID, DJ_RTT_ID_LEN) == 0) {
                    *cb_addr = addr + (uint32_t)i;
                    return 0;
                }
            }
        }
        if (n < DJ_RTT_MAX_SCAN_CHUNK) break;
        addr += step;
    }
    return -ENOENT;
}

int dj_rtt_get_info(uint32_t cb_addr, struct dj_rtt_info *info)
{
    uint32_t max_up = 0u, max_down = 0u;
    struct rtt_desc up = {0}, down = {0};
    int r;
    if (!info) return -EINVAL;
    memset(info, 0, sizeof(*info));
    r = read_header(cb_addr, &max_up, &max_down);
    if (r) return r;
    if (max_up) {
        r = read_desc(up_desc_addr(cb_addr, 0u), &up);
        if (r) return r;
    }
    if (max_down) {
        r = read_desc(down_desc_addr(cb_addr, max_up, 0u), &down);
        if (r) return r;
    }
    info->cb_addr = cb_addr;
    info->max_up = max_up;
    info->max_down = max_down;
    info->up_name = up.name;
    info->up_buffer = up.buffer;
    info->up_size = up.size;
    info->up_wr = up.wr;
    info->up_rd = up.rd;
    info->up_flags = up.flags;
    info->down_name = down.name;
    info->down_buffer = down.buffer;
    info->down_size = down.size;
    info->down_wr = down.wr;
    info->down_rd = down.rd;
    info->down_flags = down.flags;
    return 0;
}

static size_t desc_used(const struct rtt_desc *d)
{
    if (!d || d->size == 0u || d->wr >= d->size || d->rd >= d->size) return 0u;
    if (d->wr >= d->rd) return (size_t)(d->wr - d->rd);
    return (size_t)(d->size - d->rd + d->wr);
}

static size_t desc_free(const struct rtt_desc *d)
{
    if (!d || d->size < 2u || d->wr >= d->size || d->rd >= d->size) return 0u;
    if (d->rd > d->wr) return (size_t)(d->rd - d->wr - 1u);
    return (size_t)(d->size - d->wr + d->rd - 1u);
}

static void safe_name(uint32_t addr, char *out, size_t cap, uint8_t *name_len)
{
    uint8_t tmp[32];
    size_t n = 0u;
    if (!out || cap == 0u || !name_len) return;
    out[0] = 0;
    *name_len = 0u;
    if (!addr) return;
    if (read_target(addr, tmp, sizeof(tmp)) != 0) return;
    while (n + 1u < cap && n < sizeof(tmp) && tmp[n] != 0u) {
        uint8_t c = tmp[n];
        out[n] = (c >= 32u && c < 127u) ? (char)c : '?';
        ++n;
    }
    out[n] = 0;
    *name_len = (uint8_t)n;
}

int dj_rtt_get_channel_info(uint32_t cb_addr, uint8_t direction, uint8_t channel, struct dj_rtt_channel_info *info)
{
    uint32_t max_up = 0u, max_down = 0u;
    uint32_t daddr;
    struct rtt_desc d;
    int r;
    if (!info) return -EINVAL;
    memset(info, 0, sizeof(*info));
    r = read_header(cb_addr, &max_up, &max_down);
    if (r) return r;
    if (direction == 0u) {
        if ((uint32_t)channel >= max_up) return -ERANGE;
        daddr = up_desc_addr(cb_addr, channel);
    } else if (direction == 1u) {
        if ((uint32_t)channel >= max_down) return -ERANGE;
        daddr = down_desc_addr(cb_addr, max_up, channel);
    } else {
        return -EINVAL;
    }
    r = read_desc(daddr, &d);
    if (r) return r;
    info->direction = direction;
    info->channel = channel;
    info->name_addr = d.name;
    info->buffer_addr = d.buffer;
    info->size = d.size;
    info->wr_off = d.wr;
    info->rd_off = d.rd;
    info->flags = d.flags;
    info->used = (uint32_t)desc_used(&d);
    info->free_space = (uint32_t)desc_free(&d);
    safe_name(d.name, info->name, sizeof(info->name), &info->name_len);
    return 0;
}

int dj_rtt_read_up(uint32_t cb_addr, uint8_t channel, uint8_t *out, size_t *len)
{
    uint32_t max_up = 0u, max_down = 0u;
    struct rtt_desc d;
    size_t cap, available, first;
    int r;
    if (!out || !len) return -EINVAL;
    cap = *len;
    *len = 0u;
    r = read_header(cb_addr, &max_up, &max_down);
    if (r) return r;
    (void)max_down;
    if ((uint32_t)channel >= max_up) return -ERANGE;
    r = read_desc(up_desc_addr(cb_addr, channel), &d);
    if (r) return r;
    if (d.size == 0u || d.wr >= d.size || d.rd >= d.size) return -ERANGE;
    if (d.wr >= d.rd) available = (size_t)(d.wr - d.rd);
    else available = (size_t)(d.size - d.rd + d.wr);
    if (available > cap) available = cap;
    if (available == 0u) return 0;
    first = DJ_MIN(available, (size_t)(d.size - d.rd));
    r = read_target(d.buffer + d.rd, out, first);
    if (r) return r;
    if (first < available) {
        r = read_target(d.buffer, out + first, available - first);
        if (r) return r;
    }
    d.rd = (d.rd + (uint32_t)available) % d.size;
    r = write_u32(up_desc_addr(cb_addr, channel) + 16u, d.rd);
    if (r) return r;
    *len = available;
    return 0;
}

int dj_rtt_write_down(uint32_t cb_addr, uint8_t channel, const uint8_t *data, size_t len, size_t *written)
{
    uint32_t max_up = 0u, max_down = 0u;
    struct rtt_desc d;
    uint32_t daddr;
    size_t free_space, first;
    int r;
    if (!data || !written) return -EINVAL;
    *written = 0u;
    r = read_header(cb_addr, &max_up, &max_down);
    if (r) return r;
    if ((uint32_t)channel >= max_down) return -ERANGE;
    daddr = down_desc_addr(cb_addr, max_up, channel);
    r = read_desc(daddr, &d);
    if (r) return r;
    if (d.size < 2u || d.wr >= d.size || d.rd >= d.size) return -ERANGE;
    if (d.rd > d.wr) free_space = (size_t)(d.rd - d.wr - 1u);
    else free_space = (size_t)(d.size - d.wr + d.rd - 1u);
    if (free_space > len) free_space = len;
    if (free_space == 0u) return 0;
    first = DJ_MIN(free_space, (size_t)(d.size - d.wr));
    r = write_target(d.buffer + d.wr, data, first);
    if (r) return r;
    if (first < free_space) {
        r = write_target(d.buffer, data + first, free_space - first);
        if (r) return r;
    }
    d.wr = (d.wr + (uint32_t)free_space) % d.size;
    r = write_u32(daddr + 12u, d.wr);
    if (r) return r;
    *written = free_space;
    return 0;
}
