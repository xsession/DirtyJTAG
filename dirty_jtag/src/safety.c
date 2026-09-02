/* SPDX-License-Identifier: MIT */
#include "djprog/safety.h"
#include <errno.h>
#include <string.h>

static struct dj_safety_state g_safety;

void dj_safety_reset(void)
{
    g_safety.armed_flags = 0u;
    g_safety.remaining_uses = 0u;
}

int dj_safety_arm(uint32_t flags, uint32_t uses, const char *phrase, size_t phrase_len)
{
    const char expected[] = DJ_SAFETY_CONFIRM_PHRASE;
    const size_t expected_len = sizeof(expected) - 1u;
    if ((flags == 0u) || ((flags & ~DJ_SAFETY_ALL) != 0u) || !phrase) return -EINVAL;
    if (uses == 0u || uses > DJ_SAFETY_MAX_USES) return -ERANGE;
    if (phrase_len != expected_len || memcmp(phrase, expected, expected_len) != 0) return -EPERM;
    g_safety.armed_flags = flags;
    g_safety.remaining_uses = uses;
    return 0;
}

int dj_safety_require(uint32_t flags)
{
    if ((flags == 0u) || ((flags & ~DJ_SAFETY_ALL) != 0u)) return -EINVAL;
    if ((g_safety.armed_flags & flags) != flags || g_safety.remaining_uses == 0u) return -EPERM;
    g_safety.remaining_uses--;
    if (g_safety.remaining_uses == 0u) g_safety.armed_flags = 0u;
    return 0;
}

struct dj_safety_state dj_safety_get(void)
{
    return g_safety;
}

const char *dj_safety_flags_text(uint32_t flags)
{
    if (flags == DJ_SAFETY_ERASE) return "erase";
    if (flags == DJ_SAFETY_WRITE) return "write";
    if (flags == DJ_SAFETY_VPP) return "vpp";
    if (flags == DJ_SAFETY_SCRIPT) return "script";
    if (flags == DJ_SAFETY_BRIDGE) return "bridge";
    if (flags == DJ_SAFETY_POWER) return "power";
    if (flags == DJ_SAFETY_DEBUG) return "debug";
    return "multiple";
}
