/*
  Copyright (c) 2017-2022 The DirtyJTAG authors.

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the Software
  is furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
  OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "jtag.h"

#define DIRTYJTAG_NODE DT_PATH(zephyr_user)

BUILD_ASSERT(DT_NODE_HAS_PROP(DIRTYJTAG_NODE, tck_gpios),
             "zephyr,user must define tck-gpios");
BUILD_ASSERT(DT_NODE_HAS_PROP(DIRTYJTAG_NODE, tdi_gpios),
             "zephyr,user must define tdi-gpios");
BUILD_ASSERT(DT_NODE_HAS_PROP(DIRTYJTAG_NODE, tdo_gpios),
             "zephyr,user must define tdo-gpios");
BUILD_ASSERT(DT_NODE_HAS_PROP(DIRTYJTAG_NODE, tms_gpios),
             "zephyr,user must define tms-gpios");

static const struct gpio_dt_spec tck = GPIO_DT_SPEC_GET(DIRTYJTAG_NODE, tck_gpios);
static const struct gpio_dt_spec tdi = GPIO_DT_SPEC_GET(DIRTYJTAG_NODE, tdi_gpios);
static const struct gpio_dt_spec tdo = GPIO_DT_SPEC_GET(DIRTYJTAG_NODE, tdo_gpios);
static const struct gpio_dt_spec tms = GPIO_DT_SPEC_GET(DIRTYJTAG_NODE, tms_gpios);

#if DT_NODE_HAS_PROP(DIRTYJTAG_NODE, trst_gpios)
static const struct gpio_dt_spec trst = GPIO_DT_SPEC_GET(DIRTYJTAG_NODE, trst_gpios);
#endif

#if DT_NODE_HAS_PROP(DIRTYJTAG_NODE, srst_gpios)
static const struct gpio_dt_spec srst = GPIO_DT_SPEC_GET(DIRTYJTAG_NODE, srst_gpios);
#endif

static bool max_frequency = true;
static uint32_t half_period_us = 1U;

static void wait_half_period(void) {
  if (!max_frequency) {
    k_busy_wait(half_period_us);
  }
}

static void pulse_tck(void) {
  gpio_pin_set_dt(&tck, 1);
  wait_half_period();
  gpio_pin_set_dt(&tck, 0);
  wait_half_period();
}

void jtag_init(void) {
  if (!gpio_is_ready_dt(&tck) || !gpio_is_ready_dt(&tdi) ||
      !gpio_is_ready_dt(&tdo) || !gpio_is_ready_dt(&tms)) {
    return;
  }

  gpio_pin_configure_dt(&tck, GPIO_OUTPUT_INACTIVE);
  gpio_pin_configure_dt(&tdi, GPIO_OUTPUT_INACTIVE);
  gpio_pin_configure_dt(&tms, GPIO_OUTPUT_INACTIVE);
  gpio_pin_configure_dt(&tdo, GPIO_INPUT | GPIO_PULL_DOWN);

#if DT_NODE_HAS_PROP(DIRTYJTAG_NODE, trst_gpios)
  if (gpio_is_ready_dt(&trst)) {
    gpio_pin_configure_dt(&trst, GPIO_OUTPUT_ACTIVE);
  }
#endif

#if DT_NODE_HAS_PROP(DIRTYJTAG_NODE, srst_gpios)
  if (gpio_is_ready_dt(&srst)) {
    gpio_pin_configure_dt(&srst, GPIO_OUTPUT_ACTIVE);
  }
#endif
}

void jtag_set_frequency(uint32_t frequency) {
  if (frequency == 0U) {
    frequency = 1U;
  } else if (frequency > 1500U) {
    frequency = 1500U;
  }

  max_frequency = (frequency == 1500U);
  half_period_us = 500U / frequency;
  if (half_period_us == 0U) {
    half_period_us = 1U;
  }
}

void jtag_set_tck(uint8_t value) {
  gpio_pin_set_dt(&tck, value ? 1 : 0);
}

void jtag_set_tms(uint8_t value) {
  gpio_pin_set_dt(&tms, value ? 1 : 0);
}

void jtag_set_tdi(uint8_t value) {
  gpio_pin_set_dt(&tdi, value ? 1 : 0);
}

uint8_t jtag_get_tdo(void) {
  return gpio_pin_get_dt(&tdo) > 0 ? 1 : 0;
}

void jtag_set_trst(uint8_t value) {
#if DT_NODE_HAS_PROP(DIRTYJTAG_NODE, trst_gpios)
  gpio_pin_set_dt(&trst, value ? 1 : 0);
#else
  (void)value;
#endif
}

void jtag_set_srst(uint8_t value) {
#if DT_NODE_HAS_PROP(DIRTYJTAG_NODE, srst_gpios)
  gpio_pin_set_dt(&srst, value ? 1 : 0);
#else
  (void)value;
#endif
}

void jtag_transfer(uint16_t length, const uint8_t *in, uint8_t *out) {
  uint32_t xfer_i = 0;

  jtag_set_tms(0);

  while (xfer_i < length) {
    uint8_t bitmask = 0x80 >> (xfer_i % 8);

    jtag_set_tdi(in[xfer_i / 8] & bitmask);
    wait_half_period();
    jtag_set_tck(1);

    if (jtag_get_tdo()) {
      out[xfer_i / 8] |= bitmask;
    }

    xfer_i++;
    wait_half_period();
    jtag_set_tck(0);
  }
}

bool jtag_strobe(uint8_t pulses, bool tms_value, bool tdi_value) {
  bool ret;

  jtag_set_tms(tms_value);
  jtag_set_tdi(tdi_value);

  if (!pulses) {
    return jtag_get_tdo();
  }

  while (--pulses) {
    pulse_tck();
  }

  gpio_pin_set_dt(&tck, 1);
  wait_half_period();
  ret = jtag_get_tdo();
  gpio_pin_set_dt(&tck, 0);
  wait_half_period();

  return ret;
}
