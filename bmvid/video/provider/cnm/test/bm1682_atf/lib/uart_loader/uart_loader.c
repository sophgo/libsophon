#include <console.h>
#include <debug.h>
#include <uart_loader.h>
#include <platform_def.h>
#include "xyzModem.h"

#define BUF_SIZE 1024

static int getcxmodem(void) {
    if (tstc())
        return (getc());
    return -1;
}

uint32_t load_serial_ymodem(ulong dst_base, uint32_t mode)
{
    int size;
    int err;
    int res;
    connection_info_t info;
    char ymodemBuf[BUF_SIZE];
    ulong store_addr = ~0;
    ulong addr = 0;

    NOTICE("## Ready for binary (ymodem) download to 0x%lx at %d bps...\n",
        dst_base, PLAT_BM_CONSOLE_BAUDRATE);

    size = 0;
    info.mode = mode;
    res = xyzModem_stream_open(&info, &err);
    if (!res) {
        while ((res = xyzModem_stream_read(ymodemBuf, BUF_SIZE, &err)) > 0) {
            store_addr = addr + dst_base;
            size += res;
            addr += res;
            memcpy((char *)(store_addr), ymodemBuf, res);
        }
    } else {
        ERROR("%s\n", xyzModem_error(err));
    }

    xyzModem_stream_close(&err);
    xyzModem_stream_terminate(false, &getcxmodem);

    NOTICE("## Total Size @ 0x%lx = %d Bytes\n", dst_base, size);
    return 0;
}

