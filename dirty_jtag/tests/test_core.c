#include "djprog/backend.h"
#include "djprog/hw.h"
#include "djprog/usb_proto.h"
#include "djprog/debug.h"
#include "djprog/swim.h"
#include "djprog/dspic_debug.h"
#include "djprog/msp430.h"
#include "djprog/tms320.h"
#include "djpk4/dspic_common.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static int zinit(void){return 0;}
static int zcfg(const struct dj_pinmap*m){(void)m;return 0;}
static int zdir(enum dj_pin_role r,enum dj_dir d){(void)r;(void)d;return 0;}
static int zw(enum dj_pin_role r,bool v){(void)r;(void)v;return 0;}
static int zr(enum dj_pin_role r,bool*v){(void)r;*v=false;return 0;}
static int zclk(enum dj_pin_role a,enum dj_pin_role b,enum dj_pin_role c,const uint8_t*d,uint8_t*e,size_t f,bool g,uint32_t h){(void)a;(void)b;(void)c;(void)d;if(e)memset(e,0,(f+7)/8);(void)g;(void)h;return 0;}
static int zp(enum dj_power_mode m){(void)m;return 0;}
static int zb(bool v){(void)v;return 0;}
static int zm(struct dj_measurement*m){m->vtarget_mv=3300;m->vpp_mv=12100;m->itarget_ma=42;m->power_fault=false;return 0;}
static void zd(uint32_t u){(void)u;}
static const struct dj_hw_ops ops={
    .init=zinit,.configure=zcfg,.dir=zdir,.write=zw,.read=zr,.clock_bits=zclk,
    .power=zp,.vpp_boost=zb,.vpp_apply=zb,.hv_data0_apply=zb,.measure=zm,.delay_us=zd
};

int main(void){
    dj_hw_bind(&ops);
    struct dj_pinmap pm={{2,3,4,5,6,7}};
    assert(dj_hw_set_pinmap(&pm)==0);

    assert(dj_backend_count()>=14);
    assert(dj_backend_by_id(DJ_PROTO_DSPIC_ICSP));
    assert(dj_backend_by_id(DJ_PROTO_PIC24_ICSP));
    assert(dj_backend_by_id(DJ_PROTO_PIC_ICSP_RAW));
    assert(dj_backend_by_id(DJ_PROTO_AVR_ISP));
    assert(dj_backend_by_id(DJ_PROTO_AVR_PDI));
    assert(dj_backend_by_id(DJ_PROTO_MSP430_SBW));
    assert(dj_backend_by_id(DJ_PROTO_MSP430_JTAG));
    assert(dj_backend_by_id(DJ_PROTO_TMS320_C2000_JTAG));
    assert(dj_backend_by_id(DJ_PROTO_SILABS_C2));

    struct dj_target_cfg c={.proto=DJ_PROTO_AVR_ISP,.clock_hz=125000,.power=DJ_PWR_5V,.page_size=128};
    assert(dj_select_backend(&c)==0);
    assert(dj_selected_backend()->id==DJ_PROTO_AVR_ISP);

    struct djp2_frame q={.cmd=DJP2_HELLO,.seq=7},r={0},dec={0};
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.status==0 && r.len>10);

    q.cmd=DJP2_STATUS; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==36);

    q.cmd=DJP2_MEASURE;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==16);
    assert(r.payload[0]==0xe4 && r.payload[1]==0x0c); /* 3300 LE */

    q.cmd=DJP2_LIST_DEVICES;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len>20);

    q.cmd=DJP2_SET_PINMAP; q.len=DJ_PIN_COUNT;
    for(unsigned i=0;i<DJ_PIN_COUNT;i++) q.payload[i]=(uint8_t)(18+i);
    assert(djp2_dispatch(&q,&r)==0);
    assert(dj_hw_pinmap()->gpio[0]==18);
    assert(dj_hw_set_pinmap(&pm)==0);

    q.cmd=DJP2_VPP; q.len=2; q.payload[0]=0; q.payload[1]=1;
    assert(djp2_dispatch(&q,&r)==0);

    q.cmd=DJP2_PHY_INFO; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len >= 4);



    /* Electrical script VM: read DATA0 through DJP2_SCRIPT_XFER. */
    q.cmd=DJP2_SCRIPT_XFER; q.len=3; q.payload[0]=0x03; q.payload[1]=DJ_PIN_DATA0; q.payload[2]=0x00;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==1 && r.payload[0]==0);

    /* dsPIC Debug-Executive control plane: mock mode exercises the native
     * DJP2 debugger API without bundling proprietary Microchip DE binaries. */
    struct dj_target_cfg md={.proto=DJ_PROTO_DSPIC_ICSP,.clock_hz=1000000,.power=DJ_PWR_EXTERNAL};
    strcpy(md.device, "mock-dspic30f5011");
    assert(dj_select_backend(&md)==0);
    q.cmd=DJP2_DEBUG_INFO; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==12);
    assert(r.payload[3]==DJ_DEBUG_TRANSPORT_NATIVE);
    q.cmd=DJP2_DEBUG_ATTACH; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    q.cmd=DJP2_DEBUG_HALT; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    q.cmd=DJP2_DEBUG_REG_READ; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==DJ_DSPIC_DEBUG_REG_BYTES);
    q.cmd=DJP2_DEBUG_BP_SET; q.len=6;
    q.payload[0]=0x00; q.payload[1]=0x02; q.payload[2]=0x01; q.payload[3]=0x00; q.payload[4]=0; q.payload[5]=0;
    assert(djp2_dispatch(&q,&r)==0);
    q.cmd=DJP2_DEBUG_RUN; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    q.cmd=DJP2_DEBUG_DETACH; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);

    /* OpenOCD remote_bitbang transport: verify both SWD and JTAG paths. */
    struct dj_target_cfg ds={.proto=DJ_PROTO_ARM_SWD,.clock_hz=1000000,.power=DJ_PWR_EXTERNAL};
    assert(dj_select_backend(&ds)==0);
    q.cmd=DJP2_DEBUG_BITBANG; q.len=5;
    memcpy(q.payload,"Odczo",5); /* drive, clocks, sample, delay, input */
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==1 && r.payload[0]=='0');

    struct dj_target_cfg jt={.proto=DJ_PROTO_JTAG,.clock_hz=1000000,.power=DJ_PWR_EXTERNAL};
    assert(dj_select_backend(&jt)==0);
    q.cmd=DJP2_DEBUG_BITBANG; q.len=4;
    memcpy(q.payload,"0R7R",4);
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==2 && r.payload[0]=='0' && r.payload[1]=='0');

    /* First-class debugger control API: OpenOCD owns ARM/JTAG run control,
     * while DJP2 reports transport readiness without falsely claiming halt/step. */
    q.cmd=DJP2_DEBUG_INFO; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==12);
    assert(r.payload[2]==DJ_DEBUG_DETACHED);
    assert(r.payload[3]==DJ_DEBUG_TRANSPORT_OPENOCD_REMOTE_BITBANG);
    q.cmd=DJP2_DEBUG_ATTACH; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    q.cmd=DJP2_DEBUG_INFO;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.payload[2]==DJ_DEBUG_TRANSPORT_READY);
    q.cmd=DJP2_DEBUG_HALT;
    assert(djp2_dispatch(&q,&r)==-ENOTSUP);
    assert(r.status==DJP2_E_UNSUPPORTED);
    q.cmd=DJP2_DEBUG_DETACH;
    assert(djp2_dispatch(&q,&r)==0);



    /* MSP430 2-wire and 4-wire raw TAP paths: these are intentionally low-level
     * physical/debug primitives, not destructive memory algorithms. */
    struct dj_target_cfg sbw={.proto=DJ_PROTO_MSP430_SBW,.clock_hz=100000,.power=DJ_PWR_EXTERNAL};
    strcpy(sbw.device, "msp430g2553");
    assert(dj_select_backend(&sbw)==0);
    q.cmd=DJP2_ENTER; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    q.cmd=DJP2_RAW_XFER; q.len=2; q.payload[0]=DJ_MSP430_RAW_TAP_RESET; q.payload[1]=8;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==0);
    q.cmd=DJP2_RAW_XFER; q.len=4; q.payload[0]=DJ_MSP430_RAW_SHIFT_IR; q.payload[1]=8; q.payload[2]=0; q.payload[3]=0x91;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==1);
    q.cmd=DJP2_DEBUG_INFO; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==12 && r.payload[3]==DJ_DEBUG_TRANSPORT_NATIVE);

    struct dj_target_cfg mspj={.proto=DJ_PROTO_MSP430_JTAG,.clock_hz=200000,.power=DJ_PWR_EXTERNAL};
    strcpy(mspj.device, "msp430f5529");
    assert(dj_select_backend(&mspj)==0);
    q.cmd=DJP2_ENTER; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    q.cmd=DJP2_RAW_XFER; q.len=5; q.payload[0]=DJ_MSP430_RAW_SHIFT_DR; q.payload[1]=16; q.payload[2]=0; q.payload[3]=0xaa; q.payload[4]=0x55;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==2);

    /* TI TMS320/C2000 XDS110v3-style JTAG path: raw TAP operations and
     * OpenOCD remote_bitbang transport are separate from MSP430. */
    struct dj_target_cfg c2k={.proto=DJ_PROTO_TMS320_C2000_JTAG,.clock_hz=1000000,.power=DJ_PWR_EXTERNAL};
    strcpy(c2k.device, "TMS320F2800137");
    assert(dj_select_backend(&c2k)==0);
    q.cmd=DJP2_ENTER; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    q.cmd=DJP2_IDENTIFY; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==4);
    q.cmd=DJP2_RAW_XFER; q.len=2; q.payload[0]=DJ_TMS320_RAW_TAP_RESET; q.payload[1]=8;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==0);
    q.cmd=DJP2_RAW_XFER; q.len=4; q.payload[0]=DJ_TMS320_RAW_SHIFT_IR; q.payload[1]=6; q.payload[2]=0; q.payload[3]=0x3f;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==1);
    q.cmd=DJP2_DEBUG_INFO; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==12 && r.payload[3]==DJ_DEBUG_TRANSPORT_OPENOCD_REMOTE_BITBANG);
    q.cmd=DJP2_DEBUG_ATTACH; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    q.cmd=DJP2_DEBUG_BITBANG; q.len=4;
    memcpy(q.payload,"0R7R",4);
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==2);
    q.cmd=DJP2_DEBUG_DETACH; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);

    /* STM8 native debug mock: verifies the non-ARM run-control path without
     * pretending to have real target hardware in CI. */
    struct dj_target_cfg sw={.proto=DJ_PROTO_STM8_SWIM,.clock_hz=363000,.power=DJ_PWR_EXTERNAL};
    strcpy(sw.device, "mock-stm8s003f3");
    assert(dj_select_backend(&sw)==0);
    q.cmd=DJP2_ENTER; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    q.cmd=DJP2_DEBUG_INFO; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==12);
    assert(r.payload[3]==DJ_DEBUG_TRANSPORT_NATIVE);
    assert((r.payload[4] | (r.payload[5]<<8) | (r.payload[6]<<16) | (r.payload[7]<<24)) & DJ_CAP_DEBUG_RUNCTRL);
    q.cmd=DJP2_DEBUG_ATTACH; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    q.cmd=DJP2_DEBUG_HALT; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    q.cmd=DJP2_DEBUG_INFO; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.payload[2]==DJ_DEBUG_HALTED);
    q.cmd=DJP2_DEBUG_REG_READ; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    assert(r.len==STM8_CPU_REG_COUNT);
    for(size_t i=0;i<STM8_CPU_REG_COUNT;i++) q.payload[i]=(uint8_t)(0x10+i);
    q.cmd=DJP2_DEBUG_REG_WRITE; q.len=STM8_CPU_REG_COUNT;
    assert(djp2_dispatch(&q,&r)==0);
    q.cmd=DJP2_DEBUG_BP_SET; q.len=6;
    q.payload[0]=0x34; q.payload[1]=0x12; q.payload[2]=0x80; q.payload[3]=0x00; q.payload[4]=0; q.payload[5]=0;
    assert(djp2_dispatch(&q,&r)==0);
    uint8_t bp[3]={0};
    assert(dj_swim_mock_read(STM8_DM_BK1E,bp,3)==0);
    assert(bp[0]==0x80 && bp[1]==0x12 && bp[2]==0x34);
    q.cmd=DJP2_DEBUG_STEP; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    q.cmd=DJP2_DEBUG_RUN; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);
    q.cmd=DJP2_DEBUG_DETACH; q.len=0;
    assert(djp2_dispatch(&q,&r)==0);

    uint8_t buf[4096];
    size_t n=djp2_encode(&r,buf,sizeof(buf));
    assert(n==DJP2_HDR_SIZE+r.len);
    assert(djp2_decode(buf,n,&dec)==0);
    assert(dec.seq==7 && dec.len==r.len && !memcmp(dec.payload,r.payload,r.len));

    uint32_t in[4]={0x123456,0xabcdef,0x010203,0xfefdfc},out[4]={0};
    uint16_t packed[6];
    dspic_pack4(in,packed); dspic_unpack4(packed,out);
    assert(!memcmp(in,out,sizeof(in)));

    puts("test_core: PASS");
    return 0;
}
