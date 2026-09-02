/* SPDX-License-Identifier: MIT */
#include "djprog/usb_proto.h"
#include "djprog/hw.h"
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <string.h>
extern int dj_rpi_pico_hal_init(void);
static const struct device *cdc=DEVICE_DT_GET(DT_NODELABEL(dj_cdc));
static int getb(uint8_t*b){for(;;){int r=uart_poll_in(cdc,b);if(r==0)return 0;k_sleep(K_MSEC(1));}}
static void putn(const uint8_t*b,size_t n){for(size_t i=0;i<n;i++)uart_poll_out(cdc,b[i]);}
int main(void){if(dj_rpi_pico_hal_init())return 0;dj_hw_safe_idle();if(!device_is_ready(cdc))return 0;if(usb_enable(NULL))return 0;uint32_t dtr=0;while(!dtr){uart_line_ctrl_get(cdc,UART_LINE_CTRL_DTR,&dtr);k_sleep(K_MSEC(50));}static uint8_t ibuf[DJP2_HDR_SIZE+DJP2_MAX_PAYLOAD],obuf[DJP2_HDR_SIZE+DJP2_MAX_PAYLOAD];for(;;){/* hunt for DJP2 magic to recover after line noise */uint32_t shift=0;do{uint8_t b;getb(&b);shift=(shift>>8)|((uint32_t)b<<24);}while(shift!=DJP2_MAGIC);ibuf[0]='D';ibuf[1]='J';ibuf[2]='P';ibuf[3]='2';for(size_t i=4;i<DJP2_HDR_SIZE;i++)getb(&ibuf[i]);uint32_t plen=(uint32_t)ibuf[14]|((uint32_t)ibuf[15]<<8)|((uint32_t)ibuf[16]<<16)|((uint32_t)ibuf[17]<<24);if(plen>DJP2_MAX_PAYLOAD){dj_hw_safe_idle();continue;}for(size_t i=0;i<plen;i++)getb(&ibuf[DJP2_HDR_SIZE+i]);struct djp2_frame q,r;if(djp2_decode(ibuf,DJP2_HDR_SIZE+plen,&q)){dj_hw_safe_idle();continue;}djp2_dispatch(&q,&r);size_t n=djp2_encode(&r,obuf,sizeof(obuf));if(n)putn(obuf,n);}return 0;}
