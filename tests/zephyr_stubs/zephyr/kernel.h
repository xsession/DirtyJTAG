#ifndef ZSTUB_KERNEL_H
#define ZSTUB_KERNEL_H
#include <stdint.h>
#define K_MSEC(x) (x)
#define K_FOREVER (-1)
#define ARG_UNUSED(x) (void)(x)
#define K_THREAD_DEFINE(name, stack_size, entry, p1, p2, p3, prio, options, delay)
static inline void k_busy_wait(uint32_t us){(void)us;}
static inline void k_sleep(int t){(void)t;}
static inline int64_t k_uptime_get(void){return 0;}
#endif
