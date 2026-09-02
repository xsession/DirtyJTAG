/* SPDX-License-Identifier: MIT */
#include "djprog/backend.h"
#include "djprog/phy.h"
#include "djprog/hw.h"
#include <errno.h>

static struct dj_target_cfg cfg;
static int sel(const struct dj_target_cfg*c){cfg=*c;return 0;}
static uint32_t baud(void){uint32_t b=cfg.clock_hz?cfg.clock_hz:115200u;return b>225000u?225000u:b;}
static int enter(void){
    const struct dj_hw_ops*h=dj_hw();if(!h)return-ENODEV;
    if(h->power){int r=h->power(cfg.power);if(r)return r;}
    if((cfg.flags&1u)!=0){
        if(!h->vpp_boost||!h->hv_data0_apply)return-ENOTSUP;
        int r=h->vpp_boost(true);if(r)return r;
        if((r=h->hv_data0_apply(true)))return r;
        h->delay_us(1000);
        h->hv_data0_apply(false);
        h->delay_us(50); /* begin UPDI synchronization/key traffic well inside vendor activation window */
    }
    h->dir(DJ_PIN_DATA0,DJ_DIR_OUTPUT);h->write(DJ_PIN_DATA0,true);h->delay_us(2000);
    /* UPDI uses UART-like 8E2. A long low BREAK re-synchronizes the PHY. */
    uint32_t b=baud(); uint32_t bit=(1000000u+b/2u)/b;if(!bit)bit=1;
    h->write(DJ_PIN_DATA0,false);h->delay_us(24u*bit);
    h->write(DJ_PIN_DATA0,true);h->delay_us(2u*bit);
    h->dir(DJ_PIN_DATA0,DJ_DIR_INPUT);
    return 0;
}
static int leave(void){if(dj_hw())dj_hw()->dir(DJ_PIN_DATA0,DJ_DIR_INPUT);return 0;}
static int raw(const uint8_t*tx,size_t tn,uint8_t*rx,size_t*rn){
    if(!tx||tn<4||!rn)return-EINVAL;
    uint16_t txc=(uint16_t)tx[0]|((uint16_t)tx[1]<<8),rxc=(uint16_t)tx[2]|((uint16_t)tx[3]<<8);
    if(4u+(size_t)txc>tn||(size_t)rxc>*rn)return-EINVAL;
    int r=dj_uart_1wire_txrx_ex(tx+4,txc,rx,rxc,baud(),false,DJ_PARITY_EVEN,2);
    if(!r)*rn=rxc;
    return r;
}
const struct dj_backend dj_backend_updi={
    DJ_PROTO_AVR_UPDI,"avr-updi-8e2-raw",
    DJ_CAP_RAW_XFER|DJ_CAP_DEBUG_PHY|DJ_CAP_TARGET_POWER|DJ_CAP_EXPERIMENTAL,
    115200,225000,sel,enter,leave,0,0,0,0,raw
};
