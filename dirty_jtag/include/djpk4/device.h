/* SPDX-License-Identifier: MIT */
#ifndef DJPK4_DEVICE_H
#define DJPK4_DEVICE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "djpk4/icsp.h"

enum dj_family {
    DJ_FAMILY_DSPIC30,
    DJ_FAMILY_DSPIC33F,
    DJ_FAMILY_DSPIC33E,
    DJ_FAMILY_DSPIC33CK,
    DJ_FAMILY_DSPIC33A,
};

struct dj_device;
struct dj_pic_backend {
    int (*enter)(const struct dj_device *dev);
    int (*leave)(const struct dj_device *dev);
    int (*read_id)(const struct dj_device *dev, uint16_t *devid, uint16_t *rev);
    int (*erase)(const struct dj_device *dev);
    int (*read_words)(const struct dj_device *dev, uint32_t pc_addr, uint32_t *words, size_t count);
    int (*write_words)(const struct dj_device *dev, uint32_t pc_addr, const uint32_t *words, size_t count);
};

#define DJ_DEVF_FULL_PROGRAM  (1u << 0)
#define DJ_DEVF_SPECIAL_ERASE (1u << 1)
#define DJ_DEVF_EXPERIMENTAL  (1u << 2)

struct dj_device {
    const char *name;
    enum dj_family family;
    uint16_t devid[3];
    uint8_t devid_count;
    uint32_t user_end_pc;
    uint16_t row_words;
    uint16_t page_words;
    enum dj_target_power preferred_power;
    uint32_t vtarget_min_mv;
    uint32_t vtarget_max_mv;
    bool needs_vpp;
    uint32_t flags;
    const struct dj_pic_backend *backend;
};

const struct dj_device *dj_device_find_name(const char *name);
const struct dj_device *dj_device_find_id(uint16_t devid);
size_t dj_device_count(void);
const struct dj_device *dj_device_at(size_t index);

extern const struct dj_pic_backend dj_backend_dspic30;
extern const struct dj_pic_backend dj_backend_dspic33f;
extern const struct dj_pic_backend dj_backend_dspic33e;
extern const struct dj_pic_backend dj_backend_dspic33ck;
#endif
