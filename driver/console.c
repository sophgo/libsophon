#include "console.h"
#include "xmodem.h"
#include "bm_timer.h"
#include "uart.h"
#include "linux/ctype.h"

#define CONSOLE_ENTER	'\r'
#define CONSOLE_PROMPT '#'

static int wait_prompt(struct console_ctx *ctx, int timeout)
{
	/* read out all input data */
	struct timer_ctx timer;
	struct uart_ctx *uart;

	uart = &ctx->uart;
	bmdrv_timer_start(uart->bmdi, &timer, timeout);
	while (uart_getc(&ctx->uart) != CONSOLE_PROMPT) {
		/* timeout */
		if (bmdrv_timer_remain(uart->bmdi, &timer) == 0) {
			bmdrv_timer_stop(uart->bmdi, &timer);
			return -1;
		}
	}
	bmdrv_timer_stop(uart->bmdi, &timer);

	/* we should receive an space after console prompt */
	bmdrv_timer_start(uart->bmdi, &timer, 1);
	while (uart_getc(&ctx->uart) != ' ') {
		if (bmdrv_timer_remain(uart->bmdi, &timer) == 0) {
			/* we may lost that character, it's all okay */
			break;
		}
	}
	bmdrv_timer_stop(uart->bmdi, &timer);

	return 0;
}

/*
 * reset mcu console to clear mode
 */
static int reset_console(struct console_ctx *ctx)
{
	/* try newline */
	uart_putc(&ctx->uart, CONSOLE_ENTER);

	/* wait prompt */
	return wait_prompt(ctx, 1000);
}

static int send_single(struct console_ctx *ctx, char ch, int timeout)
{
	struct timer_ctx timer;
	int err;
	struct uart_ctx *uart;

	uart = &ctx->uart;
	uart_putc(&ctx->uart, ch);
	/* wait echo back */
	bmdrv_timer_start(uart->bmdi, &timer, timeout);
	while (uart_getc(&ctx->uart) != ch) {
		if (bmdrv_timer_remain(uart->bmdi, &timer) == 0) {
			err = -1;
			goto timeout;
		}
	}
	err = 0;
timeout:
	bmdrv_timer_stop(uart->bmdi, &timer);
	return err;
}

static int type_in_command(struct console_ctx *ctx, char *cmd)
{
	char *p;

	/* type into command, without ENTER */
	for (p = cmd; *p; ++p) {
		if (send_single(ctx, *p, 1000)) {
			pr_info("error with command %s when sending %c\n",
			      cmd, *p);
			return -1;
		}
	}

	/* type into newline */
	uart_putc(&ctx->uart, CONSOLE_ENTER);
	return 0;
}

int console_exec(struct console_ctx *ctx, char *cmd,
		 char *output, int outlen, int timeout)
{
	int err, i, is_leading_char;
	struct timer_ctx timer;
	struct uart_ctx *uart;

	uart = &ctx->uart;
	err = reset_console(ctx);
	if (err) {
		pr_info("cannot reset mcu console\n");
		return -1;
	}

	if (type_in_command(ctx, cmd))
		return -1;

	/* get all output except prompt */
	bmdrv_timer_start(uart->bmdi, &timer, timeout);

	is_leading_char = true;
	err = -1;
	for (i = 0; i < outlen - 1;) {
		err = uart_getc(&ctx->uart);
		if (err >= 0) {
			/* skip leading \r\n */
			if (is_leading_char) {
				if (isprint(err))
				       is_leading_char = false;
				else
					continue;
			}

			if (err == CONSOLE_PROMPT) {
				err = 0;
				break;
			}
			output[i] = err;
			++i;
		}
		if (bmdrv_timer_remain(uart->bmdi, &timer) == 0) {
			pr_info("command %s not done in %d ms\n",
				cmd, timeout);
			err = -1;
			break;
		}
	}
	bmdrv_timer_stop(uart->bmdi, &timer);

	if (i == outlen - 1) {
		pr_info("no enough output buffer for command %s\n",
			cmd);
		return -1;
	}

	if (err)
		return -1;

	/* append tailing null */
	output[i] = 0;

	/* prompt received */
	/* we should receive an space after console prompt */
	bmdrv_timer_start(uart->bmdi, &timer, 1);
	while (uart_getc(&ctx->uart) != ' ') {
		if (bmdrv_timer_remain(uart->bmdi, &timer) == 0) {
			/* we may lost that character, it's all okay */
			break;
		}
	}
	bmdrv_timer_stop(uart->bmdi, &timer);

	return 0;
}

int console_cmd_get_sn(struct console_ctx *ctx, char *sn)
{
	int err;
	char cmdout[64];
	char *p;

	err = console_exec(ctx, "sn", cmdout, sizeof(cmdout), 1000);
	if (err)
		return -1;

	for (err = 0, p = cmdout; isprint(*p); ++p, ++err)
		((char *)sn)[err] = *p;

	/* terminate sn */
	((char *)sn)[err] = 0;
	return err;
}

int console_cmd_set_sn(struct console_ctx *ctx, char *sn)
{
	int err;
	char cmd[64];
	char cmdout[64];

	snprintf(cmd, sizeof(cmd), "sn %s", sn);

	err = console_exec(ctx, cmd, cmdout, sizeof(cmdout), 1000);
	if (err)
		return -1;

	return 0;
}

int console_cmd_download(struct console_ctx *ctx, int offset,
			 void *data, int size)
{
	int err;
	char cmdout[128];

	reset_console(ctx);

	err = console_exec(ctx, "upgrade", cmdout, sizeof(cmdout), 1000);
	if (err)
		return -1;

	reset_console(ctx);

	snprintf(cmdout, sizeof(cmdout), "download 0x%x", offset);

	err = type_in_command(ctx, cmdout);

	if (err)
		return err;

	err = xmodem_send(&ctx->uart, data, size);

	if (err)
		return err;

	reset_console(ctx);

	return 0;
}

int console_cmd_sc7p_set_tpu_vol(struct console_ctx *ctx, char *vol)
{
	int err;
	char cmd[64];
	char cmdout[64];

	snprintf(cmd, sizeof(cmd), "tpu %s", vol);

	err = console_exec(ctx, cmd, cmdout, sizeof(cmdout), 1000);
	if (err)
		return -1;

	return 0;
}

int console_cmd_sc7p_set_vddc_vol(struct console_ctx *ctx, char *vol)
{
	int err;
	char cmd[64];
	char cmdout[64];

	snprintf(cmd, sizeof(cmd), "vddc %s", vol);

	err = console_exec(ctx, cmd, cmdout, sizeof(cmdout), 1000);
	if (err)
		return -1;

	return 0;
}

int console_cmd_sc7p_set_mon_mode(struct console_ctx *ctx, char *mode)
{
	int err;
	char cmd[64];
	char cmdout[64];

	snprintf(cmd, sizeof(cmd), "monmode %s", mode);

	err = console_exec(ctx, cmd, cmdout, sizeof(cmdout), 1000);
	if (err)
		return -1;

	return 0;
}

int console_cmd_sc7p_poweroff(struct console_ctx *ctx, int chip_index)
{
	int err;
	char cmd[64];
	char cmdout[64];

	snprintf(cmd, sizeof(cmd), "poweroff %d", chip_index);

	err = console_exec(ctx, cmd, cmdout, sizeof(cmdout), 1000);
	if (err)
		return -1;

	return 0;
}
