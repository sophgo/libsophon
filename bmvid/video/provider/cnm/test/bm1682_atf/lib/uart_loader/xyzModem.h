/*
 *==========================================================================
 *
 *      xyzModem.h
 *
 *      RedBoot stream handler for xyzModem protocol
 *
 *==========================================================================
 * SPDX-License-Identifier:    eCos-2.0
 *==========================================================================
 *#####DESCRIPTIONBEGIN####
 *
 * Author(s):    gthomas
 * Contributors: gthomas
 * Date:         2000-07-14
 * Purpose:
 * Description:
 *
 * This code is part of RedBoot (tm).
 *
 *####DESCRIPTIONEND####
 *
 *==========================================================================
 */

#ifndef _XYZMODEM_H_
#define _XYZMODEM_H_

/******** environment workaround ********/
#include <delay_timer.h>
#include <console.h>
#include <stdlib.h>
#include <stdio.h>
#include <debug.h>
#include <sys/stdarg.h>
#include <string.h>

//#define USE_DBG_BUF
#undef EOF /* defined in stdio.h */

typedef    int bool;
typedef unsigned long ulong;
#define    false 0
#define    true 1
#define getc console_getc
#define putc console_putc
#define tstc console_tstc

/* 16 bit CRC with polynomial x^16+x^12+x^5+1 (CRC-CCITT) */
uint16_t crc16_ccitt(uint16_t crc_start, unsigned char *s, int len);

/******** workaround end ********/

#define xyzModem_xmodem 1
#define xyzModem_ymodem 2
/* Don't define this until the protocol support is in place */
/*#define xyzModem_zmodem 3 */

#define xyzModem_access   -1
#define xyzModem_noZmodem -2
#define xyzModem_timeout  -3
#define xyzModem_eof      -4
#define xyzModem_cancel   -5
#define xyzModem_frame    -6
#define xyzModem_cksum    -7
#define xyzModem_sequence -8

#define xyzModem_close 1
#define xyzModem_abort 2


#define CYGNUM_CALL_IF_SET_COMM_ID_QUERY_CURRENT
#define CYGACC_CALL_IF_SET_CONSOLE_COMM(x)

#define diag_vprintf vprintf
#define diag_printf tf_printf
#define diag_vsprintf vsprintf

#define CYGACC_CALL_IF_DELAY_US(x) udelay(x)

typedef struct {
    char *filename;
    int   mode;
    int   chan;
} connection_info_t;



int   xyzModem_stream_open(connection_info_t *info, int *err);
void  xyzModem_stream_close(int *err);
void  xyzModem_stream_terminate(bool method, int (*getc)(void));
int   xyzModem_stream_read(char *buf, int size, int *err);
char *xyzModem_error(int err);

#endif /* _XYZMODEM_H_ */
