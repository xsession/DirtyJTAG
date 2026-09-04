#ifndef ZSTUB_UART_H
#define ZSTUB_UART_H
#include <stdint.h>
#include <zephyr/device.h>
#define UART_LINE_CTRL_DTR 1
static inline int uart_poll_in(const struct device *d, unsigned char *b) {
	(void)d;
	(void)b;
	return -1;
}
static inline void uart_poll_out(const struct device *d, unsigned char b) {
	(void)d;
	(void)b;
}
static inline int uart_line_ctrl_get(const struct device *d, uint32_t c, uint32_t *v) {
	(void)d;
	(void)c;
	*v = 1;
	return 0;
}
#endif
