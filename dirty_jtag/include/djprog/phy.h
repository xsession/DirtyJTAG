/* SPDX-License-Identifier: MIT */
#ifndef DJPROG_PHY_H
#define DJPROG_PHY_H
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

enum dj_uart_parity { DJ_PARITY_NONE=0, DJ_PARITY_EVEN=1, DJ_PARITY_ODD=2 };

int dj_spi_xfer(const uint8_t *tx,uint8_t *rx,size_t len,uint32_t hz);
int dj_uart_1wire_txrx_ex(const uint8_t *tx,size_t txlen,uint8_t *rx,size_t rxlen,
                          uint32_t baud,bool invert,enum dj_uart_parity parity,unsigned stop_bits);
int dj_uart_1wire_txrx(const uint8_t *tx,size_t txlen,uint8_t *rx,size_t rxlen,uint32_t baud,bool invert);
int dj_sync_1wire_txrx_8e2(const uint8_t *tx,size_t txlen,uint8_t *rx,size_t rxlen,uint32_t hz);
int dj_sync_1wire_break(uint32_t hz,unsigned bit_times);
int dj_c2_addr_write(uint8_t addr);
int dj_c2_addr_read(uint8_t *addr);
int dj_c2_data_write(uint8_t value);
int dj_c2_data_read(uint8_t *value);
int dj_swd_sequence(const uint8_t *tx,size_t tx_bits,uint8_t *rx,size_t rx_bits,uint32_t hz);
int dj_jtag_shift(const uint8_t *tx,size_t bits,uint8_t *rx,uint32_t hz);
#endif
