/* SPDX-License-Identifier: MIT */
#include "djpk4/device.h"
#include "djpk4/dspic_common.h"
#include "djpk4/icsp.h"
#include "djpk4/power.h"
#include <errno.h>
#include <string.h>

#define ICSP_HZ 1000000u

static int op(uint32_t x) { return dspic_exec(x); }
static int nop2(void) { int r=op(0); return r ? r : op(0); }

static int start_external_cycle(unsigned delay_us)
{
    const struct dj_icsp_hal *h = dj_icsp_hal();
    int r = op(0xA8E761u); if (r) return r; /* BSET NVMCON,#WR */
    if ((r=nop2())) return r;
    h->delay_us(delay_us);
    if ((r=nop2())) return r;
    if ((r=op(0xA9E761u))) return r;       /* BCLR NVMCON,#WR */
    return nop2();
}

static int unlock30(void)
{
    int r;
    if ((r=op(0x200558u))) return r;
    if ((r=op(0x883B38u))) return r;
    if ((r=op(0x200AA9u))) return r;
    return op(0x883B39u);
}

static int enter30(const struct dj_device *dev)
{
    (void)dev;
    const struct dj_icsp_hal *h = dj_icsp_hal();
    if (!h) return -ENODEV;
    int r = h->set_pgd_output(true); if (r) return r;
    /* Front-end idles PGC/PGD low. VDD is already stable from dj_power_prepare(). */
    if ((r=dj_mclr_low(true))) return r;
    h->delay_us(1000);
    if ((r=dj_mclr_low(false))) return r;
    if ((r=dj_vpp_prepare(dev))) return r;
    if ((r=dj_vpp_apply(true))) return r;
    h->delay_us(4000);
    /* Figure 11-4: 10 us low pulse followed by 4 ms at VIHH. */
    if ((r=dj_vpp_apply(false))) return r;
    if ((r=dj_mclr_low(true))) return r;
    h->delay_us(10);
    if ((r=dj_mclr_low(false))) return r;
    if ((r=dj_vpp_apply(true))) return r;
    h->delay_us(4000);
    if ((r=op(DSPIC_NOP))) return r;
    return op(DSPIC_NOP);
}

static int leave30(const struct dj_device *dev)
{
    (void)dev;
    int r = dj_vpp_apply(false);
    if (r) return r;
    return dj_mclr_low(false);
}

static int read30_words(const struct dj_device *dev, uint32_t addr, uint32_t *words, size_t count)
{
    (void)dev;
    if ((addr & 1u) || !words) return -EINVAL;
    while (count) {
        uint32_t tmp[4];
        int r = dspic_read4_common(addr, tmp, DSPIC_GOTO_100, 0x880190u, 0x883C20u, 2);
        if (r) return r;
        size_t n = count > 4 ? 4 : count;
        for (size_t i=0;i<n;i++) words[i]=tmp[i];
        addr += 8u; words += n; count -= n;
    }
    return 0;
}

static int read30_id(const struct dj_device *dev, uint16_t *devid, uint16_t *rev)
{
    uint32_t w[4];
    int r = read30_words(dev, 0xFF0000u, w, 4);
    if (r) return r;
    if (devid) *devid=(uint16_t)w[0];
    if (rev) *rev=(uint16_t)w[1];
    return 0;
}

static int special_5011_preerase(void)
{
    int r;
    /* DS70102K Table 11-4 steps 2-8: program FBS and RESERVED2 to 0x0000. */
    if ((r=op(0x24008Au))) return r; /* MOV #0x4008,W10 */
    if ((r=op(0x883B0Au))) return r; /* MOV W10,NVMCON */
    if ((r=op(0x200F80u))) return r; /* MOV #0xF8,W0 */
    if ((r=op(0x880190u))) return r; /* MOV W0,TBLPAG */
    if ((r=op(0x200067u))) return r; /* MOV #6,W7 */
    if ((r=op(0xEB0300u))) return r; /* CLR W6 */
    if ((r=op(DSPIC_NOP))) return r;
    for (unsigned i=0;i<2;i++) {
        if ((r=op(0xBB1B86u))) return r; /* TBLWTL W6,[W7++] */
        /* Table order explicitly loads literals then NVMKEY. */
        if ((r=op(0x200558u))) return r;
        if ((r=op(0x200AA9u))) return r;
        if ((r=op(0x883B38u))) return r;
        if ((r=op(0x883B39u))) return r;
        if ((r=start_external_cycle(2000))) return r;
    }
    return 0;
}

static int erase30(const struct dj_device *dev)
{
    int r = dspic_exit_reset(DSPIC_GOTO_100,0,0); if (r) return r;
    if (dev->flags & DJ_DEVF_SPECIAL_ERASE) {
        if ((r=special_5011_preerase())) return r;
    }
    if ((r=op(0x2407FAu))) return r; /* MOV #0x407F,W10 */
    if ((r=op(0x883B0Au))) return r;
    if ((r=unlock30())) return r;
    return start_external_cycle(4000); /* P13a max = 4 ms */
}

static int write30_words(const struct dj_device *dev, uint32_t addr, const uint32_t *words, size_t count)
{
    if (!words || !count || (addr & 1u) || count != dev->row_words || (addr % (2u*dev->row_words)))
        return -EINVAL;
    if (addr + 2u*(uint32_t)count - 2u > dev->user_end_pc) return -ERANGE;
    int r = dspic_exit_reset(DSPIC_GOTO_100,0,0); if (r) return r;
    if ((r=op(0x24001Au))) return r;
    if ((r=op(0x883B0Au))) return r;
    if ((r=dspic_set_tbl_address(addr,7,0x880190u))) return r;
    for (size_t i=0;i<count;i+=4) {
        uint16_t w[6]; dspic_pack4(&words[i],w);
        if ((r=dspic_load_wregs_0_5(w))) return r;
        if ((r=dspic_table_load4(2))) return r;
    }
    if ((r=unlock30())) return r;
    if ((r=start_external_cycle(4000))) return r; /* P12a max = 4 ms */
    return dspic_exit_reset(DSPIC_GOTO_100,0,0);
}

const struct dj_pic_backend dj_backend_dspic30 = {
    .enter=enter30,.leave=leave30,.read_id=read30_id,.erase=erase30,
    .read_words=read30_words,.write_words=write30_words,
};
