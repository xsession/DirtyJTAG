/* SPDX-License-Identifier: MIT */
#ifndef DJPK4_COMMON_H
#define DJPK4_COMMON_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DJ_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define DJ_WORD24_MASK 0x00FFFFFFu

static inline uint16_t dj_le16(const uint8_t *p) {
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t dj_le32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline void dj_put_le16(uint8_t *p, uint16_t v) {
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
}
static inline void dj_put_le32(uint8_t *p, uint32_t v) {
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}
#endif
