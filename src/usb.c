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
#include <string.h>

#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/usb_ch9.h>
#include <zephyr/usb/usb_device.h>

#include "cmd.h"
#include "delay.h"
#include "usb.h"

struct dirtyjtag_usb_desc {
  struct usb_if_descriptor iface;
  struct usb_ep_descriptor ep_out;
  struct usb_ep_descriptor ep_in;
} __packed;

static uint8_t rx_usb_buffer[DIRTYJTAG_USB_BUFFER_SIZE];
static uint8_t tx_usb_buffer[DIRTYJTAG_USB_BUFFER_SIZE];

static void dirtyjtag_out_cb(uint8_t ep, enum usb_dc_ep_cb_status_code status) {
  uint32_t read = 0U;

  ARG_UNUSED(status);

  if (usb_read(ep, rx_usb_buffer, sizeof(rx_usb_buffer), &read) != 0) {
    return;
  }

  if (read > 0U) {
    const struct dirtyjtag_usb_transfer transfer = {
      .buffer = rx_usb_buffer,
      .transferred = read,
    };

    cmd_handle(&transfer);
  }
}

static void dirtyjtag_in_cb(uint8_t ep, enum usb_dc_ep_cb_status_code status) {
  ARG_UNUSED(ep);
  ARG_UNUSED(status);
}

static void dirtyjtag_status_cb(struct usb_cfg_data *cfg,
                                enum usb_dc_status_code status,
                                const uint8_t *param) {
  ARG_UNUSED(cfg);
  ARG_UNUSED(status);
  ARG_UNUSED(param);
}

static struct usb_ep_cfg_data dirtyjtag_ep_cfg[] = {
  {
    .ep_cb = dirtyjtag_out_cb,
    .ep_addr = DIRTYJTAG_READ_ENDPOINT,
  },
  {
    .ep_cb = dirtyjtag_in_cb,
    .ep_addr = DIRTYJTAG_WRITE_ENDPOINT,
  },
};

USBD_CLASS_DESCR_DEFINE(primary, 0) struct dirtyjtag_usb_desc dirtyjtag_desc = {
  .iface = {
    .bLength = sizeof(struct usb_if_descriptor),
    .bDescriptorType = USB_DESC_INTERFACE,
    .bInterfaceNumber = 0,
    .bAlternateSetting = 0,
    .bNumEndpoints = 2,
    .bInterfaceClass = USB_BCC_VENDOR,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0,
    .iInterface = 0,
  },
  .ep_out = {
    .bLength = sizeof(struct usb_ep_descriptor),
    .bDescriptorType = USB_DESC_ENDPOINT,
    .bEndpointAddress = DIRTYJTAG_READ_ENDPOINT,
    .bmAttributes = USB_DC_EP_BULK,
    .wMaxPacketSize = sys_cpu_to_le16(DIRTYJTAG_USB_BUFFER_SIZE),
    .bInterval = 0,
  },
  .ep_in = {
    .bLength = sizeof(struct usb_ep_descriptor),
    .bDescriptorType = USB_DESC_ENDPOINT,
    .bEndpointAddress = DIRTYJTAG_WRITE_ENDPOINT,
    .bmAttributes = USB_DC_EP_BULK,
    .wMaxPacketSize = sys_cpu_to_le16(DIRTYJTAG_USB_BUFFER_SIZE),
    .bInterval = 0,
  },
};

USBD_DEFINE_CFG_DATA(dirtyjtag_config) = {
  .usb_device_description = NULL,
  .interface_descriptor = &dirtyjtag_desc.iface,
  .cb_usb_status = dirtyjtag_status_cb,
  .interface = {
    .vendor_handler = NULL,
    .class_handler = NULL,
    .custom_handler = NULL,
  },
  .num_endpoints = ARRAY_SIZE(dirtyjtag_ep_cfg),
  .endpoint = dirtyjtag_ep_cfg,
};

void usb_read_serial(void) {
}

int usb_init(void) {
  memset(rx_usb_buffer, 0, sizeof(rx_usb_buffer));
  memset(tx_usb_buffer, 0, sizeof(tx_usb_buffer));

  return usb_enable(NULL);
}

void usb_reenumerate(void) {
  delay_us(20000);
}

void usb_send(uint8_t *sent_buffer, uint8_t sent_size) {
  uint32_t wrote = 0U;

  if (sent_size > sizeof(tx_usb_buffer)) {
    sent_size = sizeof(tx_usb_buffer);
  }

  memcpy(tx_usb_buffer, sent_buffer, sent_size);
  (void)usb_write(DIRTYJTAG_WRITE_ENDPOINT, tx_usb_buffer, sent_size, &wrote);
}
