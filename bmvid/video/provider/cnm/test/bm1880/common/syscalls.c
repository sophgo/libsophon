#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdint.h>

#include "system_common.h"
#include "uart.h"

#define HEAP_SIZE      CONFIG_HEAP_SIZE

static char heap[HEAP_SIZE] __attribute__((aligned (8)));

int __io_putchar(int ch)
{
    uart_putc((uint8_t)ch);

    return ch;
}

int __io_getchar(void)
{
    return uart_getc();
}

caddr_t _sbrk(int incr)
{
    static char *heap_end;
    char *prev_heap_end;

    if (heap_end == 0)
        heap_end = heap;

    prev_heap_end = heap_end;
    if (heap_end - heap > HEAP_SIZE - incr) {
        errno = ENOMEM;
/*
 * Enable while(1) below will help to debug heap problems.
 */
#if 1
        uart_puts("\n_sbrk: error, no enough memory!\n");
        while (1)
            ;
#endif
        return (caddr_t) NULL;
    }

    heap_end += incr;
    return (caddr_t) prev_heap_end;
}

int _write(int file, char *ptr, int len)
{
    int DataIdx;

    for (DataIdx = 0; DataIdx < len; DataIdx++)
        __io_putchar(*ptr++);

    return len;
}

int _close(int file)
{
    return -EPERM;
}

int _kill(int pid, int sig)
{
    uart_puts("firmware kill\n");
    while (1)
        ;
}

int _getpid()
{
    return 1;
}

void _exit(int status)
{
    uart_puts("firmware exit\n");
    while (1)
        ;
}

int _fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    return 0;
}

int _read(int file, char *ptr, int len)
{
    int DataIdx;

    for (DataIdx = 0; DataIdx < len; DataIdx++)
        *ptr++ = __io_getchar();

    return len;
}
