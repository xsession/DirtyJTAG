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

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#include "cmd.h"
#include "delay.h"
#include "usb.h"

#define DIRTYJTAG_UART_NODE DT_CHOSEN(zephyr_console)
#define DIRTYJTAG_UART_STACK_SIZE 1024
#define DIRTYJTAG_UART_PRIORITY 5

#ifndef CONFIG_DIRTYJTAG_LEGACY_UART_RX_IDLE_MS
#define CONFIG_DIRTYJTAG_LEGACY_UART_RX_IDLE_MS 2
#endif

static const struct device *const dirtyjtag_uart =
	DEVICE_DT_GET(DIRTYJTAG_UART_NODE);

static void dirtyjtag_uart_worker(void *a, void *b, void *c)
{
	uint8_t rx_buffer[DIRTYJTAG_USB_BUFFER_SIZE];
	size_t rx_len = 0;
	int64_t last_rx_ms = 0;

	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	for (;;) {
		uint8_t ch;

		if (uart_poll_in(dirtyjtag_uart, &ch) == 0) {
			if (rx_len < sizeof(rx_buffer)) {
				rx_buffer[rx_len++] = ch;
			}
			last_rx_ms = k_uptime_get();
		} else {
			if (rx_len > 0 &&
			    k_uptime_get() - last_rx_ms >=
				    CONFIG_DIRTYJTAG_LEGACY_UART_RX_IDLE_MS) {
				const struct dirtyjtag_usb_transfer transfer = {
					.buffer = rx_buffer,
					.transferred = rx_len,
				};

				(void)cmd_handle(&transfer);
				rx_len = 0;
			}
			k_sleep(K_MSEC(1));
		}

		if (rx_len == sizeof(rx_buffer)) {
			const struct dirtyjtag_usb_transfer transfer = {
				.buffer = rx_buffer,
				.transferred = rx_len,
			};

			(void)cmd_handle(&transfer);
			rx_len = 0;
		}
	}
}

K_THREAD_DEFINE(dirtyjtag_uart_thread, DIRTYJTAG_UART_STACK_SIZE,
		dirtyjtag_uart_worker, NULL, NULL, NULL,
		DIRTYJTAG_UART_PRIORITY, 0, 0);

void usb_read_serial(void)
{
}

int usb_init(void)
{
	return device_is_ready(dirtyjtag_uart) ? 0 : -ENODEV;
}

void usb_reenumerate(void)
{
	delay_us(20000);
}

void usb_send(uint8_t *sent_buffer, uint8_t sent_size)
{
	for (uint8_t i = 0; i < sent_size; ++i) {
		uart_poll_out(dirtyjtag_uart, sent_buffer[i]);
	}
}
