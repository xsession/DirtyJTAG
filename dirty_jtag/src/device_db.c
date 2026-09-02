/* SPDX-License-Identifier: MIT */
#include "djpk4/device.h"
#include "djpk4/common.h"
#include <string.h>

static const struct dj_device devices[] = {
    {
        .name="dsPIC30F4011", .family=DJ_FAMILY_DSPIC30,
        .devid={0x0101}, .devid_count=1, .user_end_pc=0x007FFE,
        .row_words=32, .page_words=0, .preferred_power=DJ_POWER_5V,
        .vtarget_min_mv=4500, .vtarget_max_mv=5500, .needs_vpp=true,
        .flags=DJ_DEVF_FULL_PROGRAM, .backend=&dj_backend_dspic30,
    },
    {
        .name="dsPIC30F5011", .family=DJ_FAMILY_DSPIC30,
        .devid={0x0080}, .devid_count=1, .user_end_pc=0x00AFFE,
        .row_words=32, .page_words=0, .preferred_power=DJ_POWER_5V,
        .vtarget_min_mv=4500, .vtarget_max_mv=5500, .needs_vpp=true,
        .flags=DJ_DEVF_FULL_PROGRAM | DJ_DEVF_SPECIAL_ERASE, .backend=&dj_backend_dspic30,
    },
    {
        .name="dsPIC33FJ128GP802", .family=DJ_FAMILY_DSPIC33F,
        .devid={0x062D}, .devid_count=1, .user_end_pc=0x0157FE,
        .row_words=64, .page_words=512, .preferred_power=DJ_POWER_3V3,
        .vtarget_min_mv=3000, .vtarget_max_mv=3600, .needs_vpp=false,
        .flags=DJ_DEVF_FULL_PROGRAM, .backend=&dj_backend_dspic33f,
    },
};

const struct dj_device *dj_device_find_name(const char *name)
{
    for (size_t i = 0; i < DJ_ARRAY_SIZE(devices); ++i)
        if (!strcmp(devices[i].name, name)) return &devices[i];
    return NULL;
}
const struct dj_device *dj_device_find_id(uint16_t id)
{
    for (size_t i = 0; i < DJ_ARRAY_SIZE(devices); ++i)
        for (unsigned j = 0; j < devices[i].devid_count; ++j)
            if (devices[i].devid[j] == id) return &devices[i];
    return NULL;
}
size_t dj_device_count(void) { return DJ_ARRAY_SIZE(devices); }
const struct dj_device *dj_device_at(size_t i) { return i < DJ_ARRAY_SIZE(devices) ? &devices[i] : NULL; }
