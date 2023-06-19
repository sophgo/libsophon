#include <stdint.h>

#define REG_BASE_UART (0x09000000)

struct pl01x_regs {
    volatile uint32_t    dr;        /* 0x00 Data register */
    volatile uint32_t    ecr;        /* 0x04 Error clear register (Write) */
    volatile uint32_t    pl010_lcrh;    /* 0x08 Line control register, high byte */
    volatile uint32_t    pl010_lcrm;    /* 0x0C Line control register, middle byte */
    volatile uint32_t    pl010_lcrl;    /* 0x10 Line control register, low byte */
    volatile uint32_t    pl010_cr;    /* 0x14 Control register */
    volatile uint32_t    fr;        /* 0x18 Flag register (Read only) */
    volatile uint32_t    reserved;
};

#define UART_PL01x_FR_TXFE              0x80
#define UART_PL01x_FR_RXFF              0x40
#define UART_PL01x_FR_TXFF              0x20
#define UART_PL01x_FR_RXFE              0x10
#define UART_PL01x_FR_BUSY              0x08
#define UART_PL01x_FR_TMSK              (UART_PL01x_FR_TXFF + UART_PL01x_FR_BUSY)

static struct pl01x_regs *uart = (struct pl01x_regs *)REG_BASE_UART;

void uart_init(void)
{
}

void _uart_putc(uint8_t ch)
{
    while (uart->fr & UART_PL01x_FR_TXFF)
        ;
    uart->dr = ch;
}

void uart_putc(uint8_t ch)
{
    if (ch == '\n') {
        _uart_putc('\r');
    }
    _uart_putc(ch);
}

void uart_puts(char *str)
{
    if (!str)
        return;

    while (*str) {
        uart_putc(*str++);
    }
}

int uart_getc(void)
{
    while (uart->fr & UART_PL01x_FR_RXFE)
        ;
    return (int)uart->dr;
}

int uart_tstc(void)
{
    return (!(uart->fr & UART_PL01x_FR_RXFE));
}
