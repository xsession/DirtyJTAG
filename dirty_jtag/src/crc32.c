#include "djprog/usb_proto.h"
uint32_t dj_crc32(const void *data,size_t len,uint32_t seed){const uint8_t*p=data;uint32_t c=~seed;while(len--){c^=*p++;for(int i=0;i<8;i++)c=(c>>1)^(0xEDB88320u&(0u-(c&1u)));}return ~c;}
