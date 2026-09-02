/* SPDX-License-Identifier: MIT */
#include "djprog/backend.h"
#include "djprog/hw.h"
#include "djprog/phy.h"
#include "djpk4/device.h"
#include "djpk4/icsp.h"
#include "djpk4/dspic_common.h"
#include "djpk4/power.h"
#include "djprog/dspic_debug.h"
#include <errno.h>
#include <string.h>
static const struct dj_device *dev; static struct dj_target_cfg cfg;
static int pgdout(bool o){return dj_hw()->dir(DJ_PIN_DATA0,o?DJ_DIR_OUTPUT:DJ_DIR_INPUT);}static int mclr(bool low){return dj_hw()->write(DJ_PIN_RESET,!low);}static int vb(bool x){return dj_hw()->vpp_boost?dj_hw()->vpp_boost(x):-ENOTSUP;}static int va(bool x){return dj_hw()->vpp_apply?dj_hw()->vpp_apply(x):-ENOTSUP;}static int pwr(enum dj_target_power p){enum dj_power_mode m=p==DJ_POWER_5V?DJ_PWR_5V:p==DJ_POWER_3V3?DJ_PWR_3V3:p==DJ_POWER_EXTERNAL?DJ_PWR_EXTERNAL:DJ_PWR_OFF;return dj_hw()->power?dj_hw()->power(m):-ENODEV;}static int meas(struct dj_power_measurement*m){struct dj_measurement x={0};int r=dj_hw()->measure?dj_hw()->measure(&x):-ENOTSUP;if(!r){m->vtarget_mv=x.vtarget_mv;m->vpp_mv=x.vpp_mv;}return r;}
static int wb(uint32_t v,unsigned bits,bool msb,uint32_t hz){size_t bytes=(bits+7)/8;uint8_t b[4]={0};for(size_t i=0;i<bytes&&i<4;i++)b[i]=(uint8_t)(v>>(8*i));/* dspic common passes false for LSB-first control/data */if(msb){uint8_t rev[4]={0};for(unsigned i=0;i<bits;i++)if((v>>(bits-1-i))&1)rev[i/8]|=1u<<(i&7);return dj_swd_sequence(rev,bits,0,0,hz);}return dj_swd_sequence(b,bits,0,0,hz);}static int rb(uint32_t*v,unsigned bits,bool lsb,uint32_t hz){uint8_t r[4]={0};int e=dj_swd_sequence(0,0,r,bits,hz);if(e)return e;uint32_t x=0;if(lsb){for(unsigned i=0;i<bits;i++)if(r[i/8]&(1u<<(i&7)))x|=1u<<i;}else{for(unsigned i=0;i<bits;i++)if(r[i/8]&(1u<<(i&7)))x|=1u<<(bits-1-i);}*v=x;return 0;}static void du(uint32_t u){dj_hw()->delay_us(u);}static const struct dj_icsp_hal hal={0,pgdout,mclr,vb,va,pwr,meas,wb,rb,du};
static int sel(const struct dj_target_cfg*c){cfg=*c;dev=dj_device_find_name(c->device);if(!dev&&strncmp(c->device,"mock-dspic",10u)!=0)return-ENODEV;dj_icsp_bind(&hal);dj_dspic_debug_backend_changed();return 0;}static int enter(void){if(!dev)return-ENODEV;int r=dj_power_prepare(dev);if(r)return r;dj_hw()->dir(DJ_PIN_CLK,DJ_DIR_OUTPUT);dj_hw()->dir(DJ_PIN_DATA0,DJ_DIR_OUTPUT);return dev->backend->enter(dev);}static int leave(void){if(!dev)return-ENODEV;int r=dev->backend->leave(dev);dj_power_shutdown();return r;}static int ident(uint8_t*out,size_t*len){if(!dev||!out||!len||*len<4)return-EINVAL;uint16_t id=0,rv=0;int r=dev->backend->read_id(dev,&id,&rv);if(r)return r;out[0]=id;out[1]=id>>8;out[2]=rv;out[3]=rv>>8;*len=4;return 0;}static int erase(void){return dev?dev->backend->erase(dev):-ENODEV;}
static int readm(uint32_t a,uint8_t*d,size_t n){if(!dev||!d||n%3)return-EINVAL;size_t words=n/3;uint32_t tmp[64];while(words){size_t k=words>64?64:words;int r=dev->backend->read_words(dev,a,tmp,k);if(r)return r;for(size_t i=0;i<k;i++){d[3*i]=(uint8_t)tmp[i];d[3*i+1]=(uint8_t)(tmp[i]>>8);d[3*i+2]=(uint8_t)(tmp[i]>>16);}a+=(uint32_t)(2*k);d+=3*k;words-=k;}return 0;}
static int writem(uint32_t a,const uint8_t*d,size_t n){if(!dev||!d||n!=(size_t)dev->row_words*3u)return-EINVAL;uint32_t w[128];if(dev->row_words>128)return-ENOSPC;for(size_t i=0;i<dev->row_words;i++)w[i]=(uint32_t)d[3*i]|((uint32_t)d[3*i+1]<<8)|((uint32_t)d[3*i+2]<<16);return dev->backend->write_words(dev,a,w,dev->row_words);}static int raw(const uint8_t*t,size_t n,uint8_t*r,size_t*rn){
    if(!t||!rn||n<1)return-EINVAL;
    switch(t[0]){
    case 0x80: *rn=0; return dj_dspic_debug_load_capsule(t+1,n-1);
    case 0x81:{ struct dj_dspic_debug_capsule_info i={0}; if(!r||*rn<8)return-ENOSPC; int e=dj_dspic_debug_capsule_info(&i); if(e)return e; r[0]=i.loaded; r[1]=i.family; r[2]=i.hw_breakpoints; r[3]=i.register_bytes; r[4]=(uint8_t)i.flags; r[5]=(uint8_t)(i.flags>>8); r[6]=(uint8_t)(i.flags>>16); r[7]=(uint8_t)(i.flags>>24); *rn=8; return 0;}
    case 0x82:{ if(n<4)return-EINVAL; uint32_t ins=(uint32_t)t[1]|((uint32_t)t[2]<<8)|((uint32_t)t[3]<<16); *rn=0; return dspic_exec(ins);}
    case 0x83:{ if(!r||*rn<2)return-ENOSPC; uint16_t v=0; int e=dj_icsp_regout(&v,1000000u); if(e)return e; r[0]=(uint8_t)v; r[1]=(uint8_t)(v>>8); *rn=2; return 0;}
    default:return-ENOSYS;
    }
}
const struct dj_backend dj_backend_dspic={DJ_PROTO_DSPIC_ICSP,"dspic30f-dspic33f-icsp",DJ_CAP_IDENTIFY|DJ_CAP_ERASE|DJ_CAP_READ|DJ_CAP_WRITE|DJ_CAP_HV|DJ_CAP_RAW_XFER|DJ_CAP_DEBUG_PHY|DJ_CAP_DEBUG_RUNCTRL|DJ_CAP_DEBUG_REGS|DJ_CAP_DEBUG_BREAK|DJ_CAP_EXPERIMENTAL,1000000,5000000,sel,enter,leave,ident,erase,readm,writem,raw};
