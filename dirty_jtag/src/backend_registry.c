/* SPDX-License-Identifier: MIT */
#include "djprog/backend.h"
#include "djprog/debug.h"
#include <errno.h>
#include <string.h>

extern const struct dj_backend dj_backend_avr_isp,dj_backend_updi,dj_backend_tpi,dj_backend_pdi;
extern const struct dj_backend dj_backend_swim,dj_backend_sbw,dj_backend_msp430_jtag,dj_backend_c2,dj_backend_rl78;
extern const struct dj_backend dj_backend_swd,dj_backend_jtag,dj_backend_dspic,dj_backend_pic24_raw,dj_backend_pic_raw;
extern const struct dj_backend dj_backend_tms320_jtag;

static const struct dj_backend *all[]={
    &dj_backend_dspic,&dj_backend_pic24_raw,&dj_backend_pic_raw,
    &dj_backend_avr_isp,&dj_backend_updi,&dj_backend_tpi,&dj_backend_pdi,
    &dj_backend_swim,&dj_backend_sbw,&dj_backend_msp430_jtag,&dj_backend_c2,&dj_backend_rl78,
    &dj_backend_swd,&dj_backend_jtag,&dj_backend_tms320_jtag
};
static const struct dj_backend *sel;
static struct dj_target_cfg cfg;
size_t dj_backend_count(void){return sizeof(all)/sizeof(all[0]);}
const struct dj_backend*dj_backend_at(size_t i){return i<dj_backend_count()?all[i]:0;}
const struct dj_backend*dj_backend_by_id(enum dj_proto_id id){for(size_t i=0;i<dj_backend_count();i++)if(all[i]->id==id)return all[i];return 0;}
int dj_select_backend(const struct dj_target_cfg*c){
    if(!c)return -EINVAL;
    const struct dj_backend*b=dj_backend_by_id(c->proto);
    if(!b)return -ENODEV;
    int r=dj_hw_safe_idle();if(r&&r!=-ENODEV)return r;
    cfg=*c;if(!cfg.clock_hz)cfg.clock_hz=b->default_hz;if(cfg.clock_hz>b->max_hz)return -ERANGE;
    r=b->select?b->select(&cfg):0;if(r)return r;sel=b;dj_debug_backend_changed();return 0;
}
const struct dj_backend*dj_selected_backend(void){return sel;}
const struct dj_target_cfg*dj_target_config(void){return &cfg;}
