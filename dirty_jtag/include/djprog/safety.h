/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_SAFETY_H
#define DJPROG_SAFETY_H
#include <stddef.h>
#include <stdint.h>

#define DJ_SAFETY_ERASE (1u << 0)
#define DJ_SAFETY_WRITE (1u << 1)
#define DJ_SAFETY_VPP (1u << 2)
#define DJ_SAFETY_SCRIPT (1u << 3)
#define DJ_SAFETY_BRIDGE (1u << 4)
#define DJ_SAFETY_POWER (1u << 5)
#define DJ_SAFETY_DEBUG (1u << 6)
#define DJ_SAFETY_ALL                                                                              \
	(DJ_SAFETY_ERASE | DJ_SAFETY_WRITE | DJ_SAFETY_VPP | DJ_SAFETY_SCRIPT | DJ_SAFETY_BRIDGE |     \
	 DJ_SAFETY_POWER | DJ_SAFETY_DEBUG)

#define DJ_SAFETY_CONFIRM_PHRASE "I understand this can damage hardware"
#define DJ_SAFETY_MAX_USES 1000000u

struct dj_safety_state {
	uint32_t armed_flags;
	uint32_t remaining_uses;
};

void dj_safety_reset(void);
int dj_safety_arm(uint32_t flags, uint32_t uses, const char *phrase, size_t phrase_len);
int dj_safety_require(uint32_t flags);
struct dj_safety_state dj_safety_get(void);
const char *dj_safety_flags_text(uint32_t flags);

#endif
