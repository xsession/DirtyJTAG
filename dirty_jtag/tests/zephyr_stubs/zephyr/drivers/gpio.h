#ifndef ZSTUB_GPIO_H
#define ZSTUB_GPIO_H
#include <stdint.h>
#include <zephyr/device.h>
#define GPIO_INPUT 1
#define GPIO_OUTPUT_INACTIVE 2
#define GPIO_OUTPUT_ACTIVE 4
#define GPIO_PULL_UP 8
static inline int gpio_pin_configure(const struct device*d,uint8_t p,int f){(void)d;(void)p;(void)f;return 0;}
static inline int gpio_pin_set(const struct device*d,uint8_t p,int v){(void)d;(void)p;(void)v;return 0;}
static inline int gpio_pin_get(const struct device*d,uint8_t p){(void)d;(void)p;return 1;}
#endif
