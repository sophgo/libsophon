#include "uart.h"
#include "bm_timer.h"

#define TIMEOUT          (200)

/* ASCII control codes: */
#define SOH (0x01)      /* start of 128-byte data packet */
#define STX (0x02)      /* start of 1024-byte data packet */
#define EOT (0x04)      /* end of transmission */
#define ACK (0x06)      /* receive OK */
#define NAK (0x15)      /* receiver error; retry */
#define CAN (0x18)      /* two of these in succession aborts transfer */
#define CRC (0x43)      /* use in place of first NAK for CRC mode */

#define TMO	(0xff)		/* no data */
#define UND	(0xfe)		/* undefined error */

/* Number of consecutive receive errors before giving up: */
#define MAX_ERRORS    (100)

/* http://www.ccsinfo.com/forum/viewtopic.php?t=24977 */
static unsigned short crc16(const unsigned char *buf, unsigned long count)
{
	unsigned short crc = 0;
	int i;

	while(count--) {
		crc = crc ^ *buf++ << 8;

		for (i=0; i<8; i++) {
			if (crc & 0x8000) {
				crc = crc << 1 ^ 0x1021;
			} else {
				crc = crc << 1;
			}
		}
	}
	return crc;
}

static int uart_wait(struct uart_ctx *uart, char ch, int timeout)
{
	struct timer_ctx timer;
	int err = -1;

	bmdrv_timer_start(uart->bmdi, &timer, timeout);
	do {
		err = uart_getc(uart);
		if (bmdrv_timer_remain(uart->bmdi, &timer) == 0)
			break;
	} while (err != ch);
	bmdrv_timer_stop(uart->bmdi, &timer);

	return err == ch;
}

static int send_package(struct uart_ctx *uart, void *data, long len, int seq)
{
	uint8_t sxx;
	uint16_t crc;
	int i;

	if (len == 128)
		sxx = SOH;
	else if (len == 1024)
		sxx = STX;
	else
		return false;

	crc = crc16(data, len);

	uart_putc(uart, sxx);
	uart_putc(uart, seq);
	uart_putc(uart, 255 - seq);

	for (i = 0; i < len; ++i)
		uart_putc(uart, ((unsigned char *)data)[i]);

	uart_putc(uart, (crc >> 8) & 0xff);
	uart_putc(uart, crc & 0xff);

	/* wait ack or nack */
	return uart_wait(uart, ACK, 100);
}

int xmodem_send(struct uart_ctx *uart, void *data, long len)
{
	int err;
	long i, total;
	struct timer_ctx timer;

	if (len % 128) {
		pr_info("only 128 bytes aligned data is supported\n");
		return -1;
	}

	/* wait start condition 'C' */
	if (!uart_wait(uart, CRC, 5000)) {
		pr_info("no start condition found\n");
		return -1;
	}

	total = len / 128;
	err = 0;
	for (i = 0; i < total; ++i) {

		bmdrv_timer_start(uart->bmdi, &timer, 3000);
		while (!send_package(uart, ((char *)data) + i * 128, 128, i)) {
			pr_info("%ldth package send failed, try again\n", i);
			if (bmdrv_timer_remain(uart->bmdi, &timer) == 0) {
				err = -1;
				pr_info("%ldth package send timeout\n", i);
				break;
			}
		}
		bmdrv_timer_stop(uart->bmdi, &timer);
		if (err)
			return -1;
		pr_info("xmodem upgrading %ld%%\r", (i * 100 / total));
	}

	/* end of transmission */
	uart_putc(uart, EOT);

	/* wait receiver to stop */
	uart_wait(uart, ACK, 1000);
	pr_info("xmodem upgrading 100%%\n");

	return 0;
}
