#include <linux/delay.h>
#include <linux/device.h>
#include "bm_common.h"
#include "uart.h"

static inline int bm_check_uart_reg_bits(struct bm_device_info *bmdi, u32 uart_index, u64 offset, u32 mask, u32 wait_loop)
{
	u32 try_loop = 0;
	u32 reg_val;

	while (1) {
		reg_val = uart_reg_read(bmdi, uart_index, offset);
		if ((reg_val & mask) != 0)
			return 1;

		if (wait_loop != 0) {
			try_loop++;		//wait_loop == 0 means wait for ever
			udelay(1);
			if (try_loop >= wait_loop) {
				pr_info("bmsophon=%d, uart=%d check_uart_reg_bits timeout \n", bmdi->dev_index, uart_index);
				return -1;
			}
		}
	}
	return -1;
}

static inline int bm_check_uart_reg_bits_zero(struct bm_device_info *bmdi, u32 uart_index, u64 offset, u32 mask, u32 wait_loop)
{
	u32 try_loop = 0;
	u32 reg_val;

	while (1) {
		reg_val = uart_reg_read(bmdi, uart_index, offset);
		if ((reg_val & mask) == 0)
			return 1;

		if (wait_loop != 0) {
			try_loop++;		//wait_loop == 0 means wait for ever
			udelay(1);
			if (try_loop >= wait_loop) {
				pr_info("bmsophon=%d, uart=%d check_uart_reg_bits_zero timeout \n", bmdi->dev_index, uart_index);
				return -1;
			}
		}
	}
	return 0;
}

void bmdrv_uart_init(struct bm_device_info *bmdi, u32 uart_index, u32 baudrate)
{
	int uart_clock = 500000000;
	int divisor = uart_clock / (16 * baudrate);
	int value = 0x0;

	if (bmdi->cinfo.platform == PALLADIUM)
		divisor = 1;

	if (uart_index == 0x2) {
		value = top_reg_read(bmdi, 0x484);
		value &= ~(0x11 << 20);
		value |= (0x1 << 20);
		top_reg_write(bmdi, 0x484, value);

		value = top_reg_read(bmdi, 0x488);
		value &= ~(0x11 << 4);
		value |= (0x1 << 4);
		top_reg_write(bmdi, 0x488, value);
	}

	uart_reg_write(bmdi, uart_index, UARTLCR, uart_reg_read(bmdi, uart_index, UARTLCR) | UARTLCR_DLAB); /* enable DLL, DLLM programming */
	uart_reg_write(bmdi, uart_index, UARTDLL, divisor & 0xff); /* program DLL */
	uart_reg_write(bmdi, uart_index, UARTDLLM, (divisor >> 8) & 0xff); /* program DLL */
	uart_reg_write(bmdi, uart_index, UARTLCR, uart_reg_read(bmdi, uart_index, UARTLCR) & ~UARTLCR_DLAB); /* disable DLL, DLLM programming */

	uart_reg_write(bmdi, uart_index, UARTLCR, UARTLCR_WORDSZ_8); /* 8n1 */
	uart_reg_write(bmdi, uart_index, UARTIER, 0); /* no interrupt */
	uart_reg_write(bmdi, uart_index, UARTFCR, UARTFCR_TXCLR | UARTFCR_RXCLR | UARTFCR_FIFOEN | UARTFCR_DMAEN); /* enable fifo, DMA */
	uart_reg_write(bmdi, uart_index, UARTMCR, 0x1); /* DTR + RTS */
}

static void uart_put_c(struct bm_device_info *bmdi, u32 uart_index, unsigned char *data)
{
	uart_reg_write(bmdi, uart_index, UARTTX, *data);
}

static void uart_get_c(struct bm_device_info *bmdi, u32 uart_index, unsigned char *data)
{
	*data = (unsigned char)uart_reg_read(bmdi, uart_index, UARTTX);
}

static int uart_send_byte(struct bm_device_info *bmdi, u32 uart_index, unsigned char *data)
{
	int ret = 0;

	ret = bm_check_uart_reg_bits(bmdi, uart_index, UARTLSR, UARTLSR_THRE, 10000);
	if (ret < 0) {
		pr_info("bmsophon=%d, uart=%d uart_send_byte fail \n", bmdi->dev_index, uart_index);
		return -1;
	}
	uart_put_c(bmdi, uart_index, data);

	return ret;
}

static int uart_get_byte(struct bm_device_info *bmdi, u32 uart_index, unsigned char *data)
{
	int ret = 0;

	ret = bm_check_uart_reg_bits(bmdi, uart_index, UARTLSR, UARTLSR_RDR, 1000000);
	if (ret < 0) {
		pr_info("bmsophon=%d, uart=%d uart_get_byte fail \n", bmdi->dev_index, uart_index);
		return -1;
	}
	uart_get_c(bmdi, uart_index, data);

	return ret;
}

int uart_putc(struct uart_ctx *ctx, unsigned char ch)
{
	return uart_send_byte(ctx->bmdi, ctx->uart_index, &ch);
}

int uart_getc(struct uart_ctx *ctx)
{
	unsigned char data = 0x0;
	int ret = 0x0;

	ret = uart_get_byte(ctx->bmdi, ctx->uart_index, &data);

	if (ret < 0)
		return -1;

	return (int)data;
}
