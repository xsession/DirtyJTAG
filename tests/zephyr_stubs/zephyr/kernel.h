#ifndef ZSTUB_KERNEL_H
#define ZSTUB_KERNEL_H
#include <stdint.h>
#define K_MSEC(x) (x)
static inline void k_busy_wait(uint32_t us){(void)us;}
static inline void k_sleep(int t){(void)t;}
#endif
