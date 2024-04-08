#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include "uart.h"
#include "bm_timer.h"

struct console_ctx {
	struct uart_ctx uart;
};

/**
 * Open an MCU UART console
 *
 * @param ctx console context
 * @param uart_port name of uart port, on linux usually /dev/ttyUSBx if USB to
 * RS232 is used. This parameter will be passed to uart_open without any
 * modification, see uart_open for detailed information.
 *
 * @return 0 on success, other values for failure.
 */
int console_open(struct console_ctx *ctx, char *uart_port);

/**
 * Execute a command in MCU UART console
 *
 * @param ctx console context
 * @param cmd command to be executed
 * @param output command output, donnot pass NULL to this parameter, this
 * interface donnot deal with it.
 * @param outlen command output buffer length, if it is smaller than command
 * output, an error will occur, but the command has been executed.
 * @param timeout timeout from command execute start to command execute end.
 *
 * @return 0 on success, over values for failure.
 */
int console_exec(struct console_ctx *ctx, char *cmd,
		 char *output, int outlen, int timeout);

/**
 * Close a console
 *
 * @param ctx console context
 */
void console_close(struct console_ctx *ctx);

/**
 * Get SN from MCU. SN will terminated by 0.
 *
 * @param ctx console context
 * @param sn buffer that SN will store
 *
 * @return on success, length of SN, not including zero terminator. on failure,
 * a nagetive error code will return.
 */
int console_cmd_get_sn(struct console_ctx *ctx, char *sn);

/**
 * Set SN to MCU, SN should should termiate by 0
 *
 * @param ctx console context
 * @param sn buffer which stores SN
 *
 * @return 0 on success, other values for failure.
 */
int console_cmd_set_sn(struct console_ctx *ctx, char *sn);

/**
 * Download data to MCU program flash
 * Usually used to upgrade MCU firmware
 *
 * @param ctx console context
 * @param offset offset in MCU program flash
 * @param data data to be programmed into MCU program flash
 * @param size size of data
 *
 * @return 0 on success, other values for failure.
 */
int console_cmd_download(struct console_ctx *ctx, int offset,
			 void *data, int size);

int console_cmd_sc7p_set_tpu_vol(struct console_ctx *ctx, char *vol);
int console_cmd_sc7p_set_vddc_vol(struct console_ctx *ctx, char *vol);
int console_cmd_sc7p_set_mon_mode(struct console_ctx *ctx, char *mode);
int console_cmd_sc7p_poweroff(struct console_ctx *ctx, int chip_index);

#endif
