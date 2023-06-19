#ifndef __YMODEM_H__
#define __YMODEM_H__

#include "uart.h"

/**
 * Send data through XMODEM with a block size of 128 bytes.
 * console_cmd_download uses this function. calling this interface by customer
 * may not necessary.
 *
 * @param uart uart context, see uart.h
 * @param data data to be downloaded
 * @param len length of data
 *
 * @return 0 on success, other values for failure
 */
int xmodem_send(struct uart_ctx *uart, void *data, long len);

#endif
