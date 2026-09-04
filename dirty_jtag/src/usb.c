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

#include <zephyr/device.h>
#include <zephyr/drivers/usb/udc.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/usb_ch9.h>
#include <zephyr/usb/usbd.h>

#include "cmd.h"
#include "delay.h"
#include "usb.h"

#define DIRTYJTAG_USB_VID 0x1209
#define DIRTYJTAG_USB_PID 0xC0CA
#define DIRTYJTAG_USB_MAX_POWER 50

#define DIRTYJTAG_USB_ENABLED 0

struct dirtyjtag_usb_desc {
	struct usb_if_descriptor iface;
	struct usb_ep_descriptor ep_out;
	struct usb_ep_descriptor ep_in;
	struct usb_desc_header nil_desc;
} __packed;

struct dirtyjtag_usb_data {
	struct dirtyjtag_usb_desc *desc;
	const struct usb_desc_header **fs_desc;
	atomic_t state;
};

static struct usbd_class_data *dirtyjtag_class;

USBD_DEVICE_DEFINE(dirtyjtag_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), DIRTYJTAG_USB_VID,
                   DIRTYJTAG_USB_PID);

USBD_DESC_LANG_DEFINE(dirtyjtag_lang);
USBD_DESC_MANUFACTURER_DEFINE(dirtyjtag_mfr, "Jean THOMAS");
USBD_DESC_PRODUCT_DEFINE(dirtyjtag_product, "DirtyJTAG");
USBD_DESC_STRING_DEFINE(dirtyjtag_sn, "000000000000000000000000", USBD_DUT_STRING_SERIAL_NUMBER);
USBD_DESC_CONFIG_DEFINE(dirtyjtag_cfg_desc, "DirtyJTAG");
USBD_CONFIGURATION_DEFINE(dirtyjtag_fs_config, 0, DIRTYJTAG_USB_MAX_POWER, &dirtyjtag_cfg_desc);

static struct dirtyjtag_usb_desc dirtyjtag_desc = {
    .iface =
        {
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
    .ep_out =
        {
            .bLength = sizeof(struct usb_ep_descriptor),
            .bDescriptorType = USB_DESC_ENDPOINT,
            .bEndpointAddress = DIRTYJTAG_READ_ENDPOINT,
            .bmAttributes = USB_EP_TYPE_BULK,
            .wMaxPacketSize = sys_cpu_to_le16(DIRTYJTAG_USB_BUFFER_SIZE),
            .bInterval = 0,
        },
    .ep_in =
        {
            .bLength = sizeof(struct usb_ep_descriptor),
            .bDescriptorType = USB_DESC_ENDPOINT,
            .bEndpointAddress = DIRTYJTAG_WRITE_ENDPOINT,
            .bmAttributes = USB_EP_TYPE_BULK,
            .wMaxPacketSize = sys_cpu_to_le16(DIRTYJTAG_USB_BUFFER_SIZE),
            .bInterval = 0,
        },
    .nil_desc =
        {
            .bLength = 0,
            .bDescriptorType = 0,
        },
};

static const struct usb_desc_header *dirtyjtag_fs_desc[] = {
    (struct usb_desc_header *)&dirtyjtag_desc.iface,
    (struct usb_desc_header *)&dirtyjtag_desc.ep_in,
    (struct usb_desc_header *)&dirtyjtag_desc.ep_out,
    (struct usb_desc_header *)&dirtyjtag_desc.nil_desc,
};

static struct dirtyjtag_usb_data dirtyjtag_usb_data = {
    .desc = &dirtyjtag_desc,
    .fs_desc = dirtyjtag_fs_desc,
};

static int dirtyjtag_enqueue_out(struct usbd_class_data *c_data) {
	struct usbd_context *ctx = usbd_class_get_ctx(c_data);
	struct net_buf *buf;

	buf = usbd_ep_buf_alloc(c_data, DIRTYJTAG_READ_ENDPOINT, DIRTYJTAG_USB_BUFFER_SIZE);
	if (buf == NULL) {
		return -ENOMEM;
	}

	if (usbd_ep_enqueue(c_data, buf) != 0) {
		(void)usbd_ep_buf_free(ctx, buf);
		return -EIO;
	}

	return 0;
}

static int dirtyjtag_request(struct usbd_class_data *c_data, struct net_buf *buf, int err) {
	struct usbd_context *ctx = usbd_class_get_ctx(c_data);
	struct dirtyjtag_usb_data *data = usbd_class_get_private(c_data);
	struct udc_buf_info *bi = udc_get_buf_info(buf);

	if (!atomic_test_bit(&data->state, DIRTYJTAG_USB_ENABLED) || err != 0) {
		(void)usbd_ep_buf_free(ctx, buf);
		return 0;
	}

	if (bi->ep == DIRTYJTAG_READ_ENDPOINT) {
		if (buf->len > 0U) {
			const struct dirtyjtag_usb_transfer transfer = {
			    .buffer = buf->data,
			    .transferred = buf->len,
			};

			cmd_handle(&transfer);
		}

		(void)usbd_ep_buf_free(ctx, buf);
		(void)dirtyjtag_enqueue_out(c_data);
	} else {
		(void)usbd_ep_buf_free(ctx, buf);
	}

	return 0;
}

static void *dirtyjtag_get_desc(struct usbd_class_data *c_data, const enum usbd_speed speed) {
	struct dirtyjtag_usb_data *data = usbd_class_get_private(c_data);

	ARG_UNUSED(speed);

	return data->fs_desc;
}

static void dirtyjtag_enable(struct usbd_class_data *c_data) {
	struct dirtyjtag_usb_data *data = usbd_class_get_private(c_data);

	dirtyjtag_class = c_data;

	if (!atomic_test_and_set_bit(&data->state, DIRTYJTAG_USB_ENABLED)) {
		(void)dirtyjtag_enqueue_out(c_data);
	}
}

static void dirtyjtag_disable(struct usbd_class_data *c_data) {
	struct dirtyjtag_usb_data *data = usbd_class_get_private(c_data);

	atomic_clear_bit(&data->state, DIRTYJTAG_USB_ENABLED);
	dirtyjtag_class = NULL;
}

static int dirtyjtag_usb_class_init(struct usbd_class_data *c_data) {
	ARG_UNUSED(c_data);

	return 0;
}

static const struct usbd_class_api dirtyjtag_api = {
    .request = dirtyjtag_request,
    .get_desc = dirtyjtag_get_desc,
    .enable = dirtyjtag_enable,
    .disable = dirtyjtag_disable,
    .init = dirtyjtag_usb_class_init,
};

USBD_DEFINE_CLASS(dirtyjtag, &dirtyjtag_api, &dirtyjtag_usb_data, NULL);

void usb_read_serial(void) {}

int usb_init(void) {
	int err;

	err = usbd_add_descriptor(&dirtyjtag_usbd, &dirtyjtag_lang);
	if (err != 0) {
		return err;
	}

	err = usbd_add_descriptor(&dirtyjtag_usbd, &dirtyjtag_mfr);
	if (err != 0) {
		return err;
	}

	err = usbd_add_descriptor(&dirtyjtag_usbd, &dirtyjtag_product);
	if (err != 0) {
		return err;
	}

	err = usbd_add_descriptor(&dirtyjtag_usbd, &dirtyjtag_sn);
	if (err != 0) {
		return err;
	}

	err = usbd_add_configuration(&dirtyjtag_usbd, USBD_SPEED_FS, &dirtyjtag_fs_config);
	if (err != 0) {
		return err;
	}

	err = usbd_register_class(&dirtyjtag_usbd, "dirtyjtag", USBD_SPEED_FS, 1);
	if (err != 0) {
		return err;
	}

	(void)usbd_device_set_code_triple(&dirtyjtag_usbd, USBD_SPEED_FS, 0, 0, 0);

	err = usbd_init(&dirtyjtag_usbd);
	if (err != 0) {
		return err;
	}

	return usbd_enable(&dirtyjtag_usbd);
}

void usb_reenumerate(void) {
	delay_us(20000);
}

void usb_send(uint8_t *sent_buffer, uint8_t sent_size) {
	struct net_buf *buf;

	if (dirtyjtag_class == NULL || sent_size == 0U) {
		return;
	}

	if (sent_size > DIRTYJTAG_USB_BUFFER_SIZE) {
		sent_size = DIRTYJTAG_USB_BUFFER_SIZE;
	}

	buf = usbd_ep_buf_alloc(dirtyjtag_class, DIRTYJTAG_WRITE_ENDPOINT, sent_size);
	if (buf == NULL) {
		return;
	}

	net_buf_add_mem(buf, sent_buffer, sent_size);

	if (usbd_ep_enqueue(dirtyjtag_class, buf) != 0) {
		struct usbd_context *ctx = usbd_class_get_ctx(dirtyjtag_class);

		(void)usbd_ep_buf_free(ctx, buf);
	}
}
