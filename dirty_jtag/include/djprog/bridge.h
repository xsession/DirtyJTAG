/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_BRIDGE_H
#define DJPROG_BRIDGE_H
#include <stddef.h>
#include <stdint.h>

/* Clean-room bridge command helpers inspired by vendor probe bridge modes
 * (STLINK-V3, KitProg3, MCU-Link Pro, PICkit 5 VCOM, etc.). These are generic
 * electrical transactions over the DirtyJTAG role pins, not vendor protocol
 * emulation. */
enum dj_bridge_gpio_op {
    DJ_BRIDGE_GPIO_READ  = 0,
    DJ_BRIDGE_GPIO_WRITE = 1,
    DJ_BRIDGE_GPIO_DIR   = 2,
};

int dj_bridge_info(uint8_t *out, size_t *len);
int dj_bridge_gpio(const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen);
int dj_bridge_spi(const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen);
int dj_bridge_i2c(const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen);
int dj_bridge_uart(const uint8_t *in, size_t inlen, uint8_t *out, size_t *outlen);

#endif
