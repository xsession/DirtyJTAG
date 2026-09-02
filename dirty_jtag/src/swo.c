/* SPDX-License-Identifier: MIT */
#include "djprog/swo.h"
#include <errno.h>
#include <string.h>

static uint8_t swo_ring[DJ_SWO_RING_SIZE];
static uint32_t swo_baud;
static uint32_t swo_flags;
static uint32_t swo_wr;
static uint32_t swo_rd;
static uint32_t swo_dropped;
static bool swo_active;

static uint32_t ring_available(void)
{
    if (swo_wr >= swo_rd) return swo_wr - swo_rd;
    return DJ_SWO_RING_SIZE - swo_rd + swo_wr;
}

static uint32_t ring_free(void)
{
    return DJ_SWO_RING_SIZE - 1u - ring_available();
}

int dj_swo_configure(uint32_t baud, uint32_t flags)
{
    if (baud < 9600u || baud > 12000000u) return -ERANGE;
    if (swo_active) return -EBUSY;
    swo_baud = baud;
    swo_flags = flags;
    swo_wr = 0u;
    swo_rd = 0u;
    swo_dropped = 0u;
    memset(swo_ring, 0, sizeof(swo_ring));
    return 0;
}

int dj_swo_start(void)
{
    if (!swo_baud) return -EINVAL;
    swo_active = true;
    return 0;
}

int dj_swo_stop(void)
{
    swo_active = false;
    return 0;
}

int dj_swo_status(struct dj_swo_status *status)
{
    if (!status) return -EINVAL;
    status->baud = swo_baud;
    status->flags = swo_flags;
    status->available = ring_available();
    status->dropped = swo_dropped;
    status->active = swo_active;
    return 0;
}

int dj_swo_read(uint8_t *out, size_t *len)
{
    size_t want;
    size_t got = 0u;
    if (!out || !len) return -EINVAL;
    want = *len;
    while (got < want && swo_rd != swo_wr) {
        out[got++] = swo_ring[swo_rd];
        swo_rd = (swo_rd + 1u) % DJ_SWO_RING_SIZE;
    }
    *len = got;
    return 0;
}

int dj_swo_mock_feed(const uint8_t *data, size_t len)
{
    if (!data && len) return -EINVAL;
    for (size_t i = 0; i < len; ++i) {
        if (ring_free() == 0u) {
            swo_dropped++;
            continue;
        }
        swo_ring[swo_wr] = data[i];
        swo_wr = (swo_wr + 1u) % DJ_SWO_RING_SIZE;
    }
    return 0;
}
