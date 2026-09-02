/* SPDX-License-Identifier: MIT */
#include "djpk4/device.h"
#include "djpk4/dspic_common.h"
#include "djpk4/icsp.h"
#include "djpk4/power.h"
#include <errno.h>

#define ICSP_HZ 1000000u

static int enter33f(const struct dj_device *dev)
{
    (void)dev;
    const struct dj_icsp_hal *h = dj_icsp_hal();
    if (!h) return -ENODEV;
    int r = h->set_pgd_output(true);
    if (r) return r;
    if ((r = dj_mclr_low(false))) return r;
    h->delay_us(1000);
    if ((r = dj_mclr_low(true))) return r;
    h->delay_us(1000);
    if ((r = h->write_bits(0x4D434851u, 32, true, ICSP_HZ))) return r;
    h->delay_us(10);
    if ((r = dj_mclr_low(false))) return r;
    h->delay_us(1000);
    return dj_icsp_idle_clocks(5, ICSP_HZ);
}

static int leave33f(const struct dj_device *dev)
{
    (void)dev;
    return dj_mclr_low(true);
}

static int read33f_words(const struct dj_device *dev, uint32_t a, uint32_t *w, size_t n)
{
    (void)dev;
    if ((a & 1u) || !w) return -EINVAL;
    while (n) {
        uint32_t t[4];
        int r = dspic_read4_common(a, t, DSPIC_GOTO_200, 0x880190u, 0x883C20u, 2);
        if (r) return r;
        size_t k = n > 4 ? 4 : n;
        for (size_t i = 0; i < k; i++) w[i] = t[i];
        a += 8;
        w += k;
        n -= k;
    }
    return 0;
}

static int read33f_id(const struct dj_device *d, uint16_t *id, uint16_t *rev)
{
    uint32_t w[4];
    int r = read33f_words(d, 0xFF0000u, w, 4);
    if (r) return r;
    if (id) *id = (uint16_t)w[0];
    if (rev) *rev = (uint16_t)w[1];
    return 0;
}

static int erase33f(const struct dj_device *d)
{
    (void)d;
    const struct dj_icsp_hal *h = dj_icsp_hal();
    int r = dspic_exit_reset(DSPIC_GOTO_200, 0, 0);
    if (r) return r;
    if ((r = dspic_exec(0x2404FAu))) return r;
    if ((r = dspic_exec(0x883B0Au))) return r;
    if ((r = dspic_exec(0xA8E761u))) return r;
    for (int i = 0; i < 4; i++) {
        if ((r = dspic_exec(0))) return r;
    }
    h->delay_us(330000);
    return dspic_poll_wr(0x803B00u, 0x883C20u, DSPIC_GOTO_200, 50, 1);
}

static int write33f_words(const struct dj_device *d, uint32_t a, const uint32_t *w, size_t n)
{
    if (!w || n != d->row_words || (a & 1u) || (a % (2u * d->row_words))) return -EINVAL;
    if (a + 2u * (uint32_t)n - 2u > d->user_end_pc) return -ERANGE;
    int r = dspic_exit_reset(DSPIC_GOTO_200, 0, 0);
    if (r) return r;
    if ((r = dspic_exec(0x24001Au))) return r;
    if ((r = dspic_exec(0x883B0Au))) return r;
    if ((r = dspic_set_tbl_address(a, 7, 0x880190u))) return r;
    for (size_t i = 0; i < n; i += 4) {
        uint16_t p[6];
        dspic_pack4(&w[i], p);
        if ((r = dspic_load_wregs_0_5(p))) return r;
        if ((r = dspic_table_load4(2))) return r;
    }
    if ((r = dspic_exec(0xA8E761u))) return r;
    for (int i = 0; i < 4; i++) {
        if ((r = dspic_exec(0))) return r;
    }
    dj_icsp_hal()->delay_us(2000);
    return dspic_poll_wr(0x803B00u, 0x883C20u, DSPIC_GOTO_200, 20, 1);
}

const struct dj_pic_backend dj_backend_dspic33f = {
    .enter = enter33f,
    .leave = leave33f,
    .read_id = read33f_id,
    .erase = erase33f,
    .read_words = read33f_words,
    .write_words = write33f_words,
};
