/* SPDX-License-Identifier: MIT */
#include "djprog/backend.h"
#include "djprog/phy.h"
#include "djprog/hw.h"
#include <errno.h>
#define C2_DEVICEID 0x00u
#define C2_REVID    0x01u
#define C2_FPCTL    0x02u
#define C2_FPDAT    0xB4u
static struct dj_target_cfg cfg;static int sel(const struct dj_target_cfg*c){cfg=*c;return 0;}
static int poll(uint8_t mask,bool set){for(int i=0;i<10000;i++){uint8_t a=0;int r=dj_c2_addr_read(&a);if(r)return r;if(((a&mask)!=0)==set)return 0;dj_hw()->delay_us(2);}return-ETIMEDOUT;}
static int fpwrite(uint8_t x){int r=dj_c2_data_write(x);if(r)return r;return poll(0x02,false);}static int fpread(uint8_t*x){int r=poll(0x01,true);if(r)return r;return dj_c2_data_read(x);}
static int enter(void){const struct dj_hw_ops*h=dj_hw();if(!h)return-ENODEV;if(h->power){int r=h->power(cfg.power);if(r)return r;}h->dir(DJ_PIN_CLK,DJ_DIR_OUTPUT);h->write(DJ_PIN_CLK,false);h->delay_us(25);h->write(DJ_PIN_CLK,true);h->delay_us(3);int r=dj_c2_addr_write(C2_FPCTL);if(r)return r;if((r=dj_c2_data_write(0x02)))return r;if((r=dj_c2_data_write(0x04)))return r;if((r=dj_c2_data_write(0x01)))return r;h->delay_us(20000);return 0;}static int leave(void){if(dj_hw()){dj_hw()->dir(DJ_PIN_CLK,DJ_DIR_INPUT);dj_hw()->dir(DJ_PIN_DATA0,DJ_DIR_INPUT);}return 0;}
static int identify(uint8_t*out,size_t*len){if(!out||!len||*len<2)return-ENOSPC;int r=dj_c2_addr_write(C2_DEVICEID);if(r)return r;if((r=dj_c2_data_read(&out[0])))return r;if((r=dj_c2_addr_write(C2_REVID)))return r;if((r=dj_c2_data_read(&out[1])))return r;*len=2;return 0;}
static int c2cmd(uint8_t c){int r=dj_c2_addr_write(C2_FPDAT);if(r)return r;if((r=fpwrite(c)))return r;uint8_t s=0;if((r=fpread(&s)))return r;return s==0x0D?0:-EIO;}
static int erase(void){int r=c2cmd(0x03);if(r)return r;uint8_t key[3]={0xDE,0xAD,0xA5};for(int i=0;i<3;i++)if((r=fpwrite(key[i])))return r;uint8_t s=0;if((r=fpread(&s)))return r;return s==0x0D?0:-EIO;}
static int readm(uint32_t a,uint8_t*d,size_t n){if(!d||a>0xFFFFu||a+n>0x10000u)return-ERANGE;while(n){size_t k=n>256?256:n;int r=c2cmd(0x06);if(r)return r;if((r=fpwrite((uint8_t)(a>>8))))return r;if((r=fpwrite((uint8_t)a)))return r;if((r=fpwrite((uint8_t)(k==256?0:k))))return r;for(size_t i=0;i<k;i++)if((r=fpread(&d[i])))return r;a+=k;d+=k;n-=k;}return 0;}
static int writem(uint32_t a,const uint8_t*d,size_t n){if(!d||a>0xFFFFu||a+n>0x10000u)return-ERANGE;while(n){size_t k=n>256?256:n;int r=c2cmd(0x07);if(r)return r;if((r=fpwrite((uint8_t)(a>>8))))return r;if((r=fpwrite((uint8_t)a)))return r;if((r=fpwrite((uint8_t)(k==256?0:k))))return r;for(size_t i=0;i<k;i++)if((r=fpwrite(d[i])))return r;uint8_t s=0;if((r=fpread(&s)))return r;if(s!=0x0D)return-EIO;a+=k;d+=k;n-=k;}return 0;}
static int raw(const uint8_t*t,size_t n,uint8_t*r,size_t*rn){if(!t||n<1||!rn)return-EINVAL;switch(t[0]){case 0:if(n<2)return-EINVAL;*rn=0;return dj_c2_addr_write(t[1]);case 1:if(*rn<1)return-ENOSPC;*rn=1;return dj_c2_addr_read(r);case 2:if(n<2)return-EINVAL;*rn=0;return dj_c2_data_write(t[1]);case 3:if(*rn<1)return-ENOSPC;*rn=1;return dj_c2_data_read(r);default:return-ENOSYS;}}
const struct dj_backend dj_backend_c2={DJ_PROTO_SILABS_C2,"silabs-c2",DJ_CAP_IDENTIFY|DJ_CAP_ERASE|DJ_CAP_READ|DJ_CAP_WRITE|DJ_CAP_RAW_XFER|DJ_CAP_DEBUG_PHY|DJ_CAP_EXPERIMENTAL,100000,1000000,sel,enter,leave,identify,erase,readm,writem,raw};
