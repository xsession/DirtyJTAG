/* SPDX-License-Identifier: MIT */
#include "djprog/hw.h"
#include "djprog/usb_proto.h"

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>

extern int dj_rpi_pico_hal_init(void);

static const struct device *const cdc = DEVICE_DT_GET(DT_NODELABEL(dj_cdc));
static uint8_t input_buffer[DJP2_HDR_SIZE + DJP2_MAX_PAYLOAD];
static uint8_t output_buffer[DJP2_HDR_SIZE + DJP2_MAX_PAYLOAD];
static struct djp2_frame request;
static struct djp2_frame response;

static uint8_t read_byte(void) {
	uint8_t value;

	while (uart_poll_in(cdc, &value) != 0) {
		k_sleep(K_MSEC(1));
	}

	return value;
}

static void write_bytes(const uint8_t *data, size_t length) {
	for (size_t i = 0; i < length; ++i) {
		uart_poll_out(cdc, data[i]);
	}
}

static void wait_for_host(void) {
	uint32_t dtr = 0;

	while (dtr == 0U) {
		(void)uart_line_ctrl_get(cdc, UART_LINE_CTRL_DTR, &dtr);
		k_sleep(K_MSEC(50));
	}
}

static void read_frame(void) {
	uint32_t window = 0;

	/* Scan for the magic so a truncated or noisy frame does not desync USB. */
	do {
		window = (window >> 8) | ((uint32_t)read_byte() << 24);
	} while (window != DJP2_MAGIC);

	input_buffer[0] = 'D';
	input_buffer[1] = 'J';
	input_buffer[2] = 'P';
	input_buffer[3] = '2';
	for (size_t i = 4; i < DJP2_HDR_SIZE; ++i) {
		input_buffer[i] = read_byte();
	}
}

int main(void) {
	if (dj_rpi_pico_hal_init() != 0) {
		return 0;
	}

	(void)dj_hw_safe_idle();
	if (!device_is_ready(cdc) || usb_enable(NULL) != 0) {
		return 0;
	}
	wait_for_host();

	for (;;) {
		read_frame();
		uint32_t payload_length = (uint32_t)input_buffer[14] | ((uint32_t)input_buffer[15] << 8) |
		                          ((uint32_t)input_buffer[16] << 16) |
		                          ((uint32_t)input_buffer[17] << 24);

		if (payload_length > DJP2_MAX_PAYLOAD) {
			(void)dj_hw_safe_idle();
			continue;
		}
		for (size_t i = 0; i < payload_length; ++i) {
			input_buffer[DJP2_HDR_SIZE + i] = read_byte();
		}

		if (djp2_decode(input_buffer, DJP2_HDR_SIZE + payload_length, &request) != 0) {
			(void)dj_hw_safe_idle();
			continue;
		}

		(void)djp2_dispatch(&request, &response);
		size_t encoded_length = djp2_encode(&response, output_buffer, sizeof(output_buffer));
		if (encoded_length != 0U) {
			write_bytes(output_buffer, encoded_length);
		}
	}

	return 0;
}
