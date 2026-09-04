/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_SWO_H
#define DJPROG_SWO_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DJ_SWO_RING_SIZE 2048u

struct dj_swo_status {
	uint32_t baud;
	uint32_t flags;
	uint32_t available;
	uint32_t dropped;
	bool active;
};

int dj_swo_configure(uint32_t baud, uint32_t flags);
int dj_swo_start(void);
int dj_swo_stop(void);
int dj_swo_status(struct dj_swo_status *status);
int dj_swo_read(uint8_t *out, size_t *len);
int dj_swo_mock_feed(const uint8_t *data, size_t len);

#endif
