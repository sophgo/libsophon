/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * Add to readline cmdline-editing by
 * (C) Copyright 2005
 * JinHua Luo, GuangDong Linux Center, <luo.jinhua@gd-linux.com>
 *
 * SPDX-License-Identifier:    GPL-2.0+
 */

#include <cli.h>
#include <console.h>
#include <debug.h>
#include <string.h>
#include <stdlib.h>
#include <uart_loader.h>
#include <platform_def.h>
#include <mmio.h>

/******** environment workaround ********/

#if DEBUG_PARSER
#define debug_parser tf_printf
#else
#define debug_parser(fmt, args...)
#endif
#define puts tf_printf

/*
 * Rather than doubling the size of the _ctype lookup table to hold a 'blank'
 * flag, just check for space or tab.
 */
#define isblank(c)    (c == ' ' || c == '\t')

/**
 * strcpy - Copy a %NUL terminated string
 * @dest: Where to copy the string to
 * @src: Where to copy the string from
 */
char * strcpy(char * dest,const char *src)
{
    char *tmp = dest;

    while ((*dest++ = *src++) != '\0')
        /* nothing */;
    return tmp;
}

/**
 * strlcpy - Copy a C-string into a sized buffer
 * @dest: Where to copy the string to
 * @src: Where to copy the string from
 * @size: size of destination buffer
 *
 * Compatible with *BSD: the result is always a valid
 * NUL-terminated string that fits in the buffer (unless,
 * of course, the buffer size is zero). It does not pad
 * out the result like strncpy() does.
 */
size_t strlcpy(char *dest, const char *src, size_t size)
{
    size_t ret = strlen(src);

    if (size) {
        size_t len = (ret >= size) ? size - 1 : ret;
        memcpy(dest, src, len);
        dest[len] = '\0';
    }
    return ret;
}

extern char console_buffer[];

/**** workaround end ****/

#define TEST_CMD(a) strcmp(argv[0], a) == 0

int cmd_process(int flag, int argc, char * const argv[])
{
    if (argc < 1)
        return -1;

    if (TEST_CMD("exit")) {
        return 0xE0F;
    } else if (TEST_CMD("loadf")) {
        load_serial_ymodem(BM_FLASH0_BASE, xyzModem_ymodem); // fip.bin
    } else if (TEST_CMD("loadk")) {
        load_serial_ymodem(0x100080000, xyzModem_ymodem); // kernel
    } else if (TEST_CMD("loadd")) {
        load_serial_ymodem(0x102000000, xyzModem_ymodem); // dtb
    } else if (TEST_CMD("loadr")) {
        load_serial_ymodem(0x104000000, xyzModem_ymodem); // ramdisk
    } else if (TEST_CMD("mw")) {
        unsigned long addr, value;

        if (argc < 3)
            return -1;
        addr = simple_strtoul(argv[1], NULL, 16);
        value = simple_strtoul(argv[2], NULL, 16);
        mmio_write_32(addr, value);
        INFO("set address 0x%lx to 0x%lx\n", addr, value);
    } else if (TEST_CMD("mr")) {
        unsigned long addr;

        if (argc < 2)
            return -1;
        addr = simple_strtoul(argv[1], NULL, 16);
        INFO("address 0x%lx is 0x%x\n", addr, mmio_read_32(addr));
    }
    return 0;

}

int cli_simple_parse_line(char *line, char *argv[])
{
    int nargs = 0;

    debug_parser("%s: \"%s\"\n", __func__, line);
    while (nargs < CONFIG_SYS_MAXARGS) {
        /* skip any white space */
        while (isblank(*line))
            ++line;

        if (*line == '\0') {    /* end of line, no more args    */
            argv[nargs] = NULL;
            debug_parser("%s: nargs=%d\n", __func__, nargs);
            return nargs;
        }

        argv[nargs++] = line;    /* begin of argument string    */

        /* find end of string */
        while (*line && !isblank(*line))
            ++line;

        if (*line == '\0') {    /* end of line, no more args    */
            argv[nargs] = NULL;
            debug_parser("parse_line: nargs=%d\n", nargs);
            return nargs;
        }

        *line++ = '\0';        /* terminate current arg     */
    }

    ERROR("** Too many args (max. %d) **\n", CONFIG_SYS_MAXARGS);

    debug_parser("%s: nargs=%d\n", __func__, nargs);
    return nargs;
}

 /*
 * WARNING:
 *
 * We must create a temporary copy of the command since the command we get
 * may be the result from getenv(), which returns a pointer directly to
 * the environment data, which may change magicly when the command we run
 * creates or modifies environment variables (like "bootp" does).
 */
int cli_simple_run_command(const char *cmd, int flag)
{
    char cmdbuf[CONFIG_SYS_CBSIZE];    /* working copy of cmd        */
    char *token;            /* start of token in cmdbuf    */
    char *sep;            /* end of token (separator) in cmdbuf */
    char *str = cmdbuf;
    char *argv[CONFIG_SYS_MAXARGS + 1];    /* NULL terminated    */
    int argc, inquotes;
    int rc = 0;

    debug_parser("[RUN_COMMAND] cmd[%p]=\"", cmd);
    if (DEBUG_PARSER) {
        /* use puts - string may be loooong */
        puts(cmd ? cmd : "NULL");
        puts("\"\n");
    }

    if (!cmd || !*cmd)
        return -1;    /* empty command */

    if (strlen(cmd) >= CONFIG_SYS_CBSIZE) {
        puts("## Command too long!\n");
        return -1;
    }

    strcpy(cmdbuf, cmd);

    /* Process separators and check for invalid
     * repeatable commands
     */

    debug_parser("[PROCESS_SEPARATORS] %s\n", cmd);
    while (*str) {
        /*
         * Find separator, or string end
         * Allow simple escape of ';' by writing "\;"
         */
        for (inquotes = 0, sep = str; *sep; sep++) {
            if ((*sep == '\'') &&
                (*(sep - 1) != '\\'))
                inquotes = !inquotes;

            if (!inquotes &&
                (*sep == ';') &&    /* separator        */
                (sep != str) &&    /* past string start    */
                (*(sep - 1) != '\\'))    /* and NOT escaped */
                break;
        }

        /*
         * Limit the token to data between separators
         */
        token = str;
        if (*sep) {
            str = sep + 1;    /* start of command for next pass */
            *sep = '\0';
        } else {
            str = sep;    /* no more commands for next pass */
        }
        debug_parser("token: \"%s\"\n", token);

        /* Extract arguments */
        argc = cli_simple_parse_line(token, argv);
        if (argc == 0) {
            rc = -1;    /* no command at all */
            continue;
        }

        rc = cmd_process(flag, argc, argv);
    }

    return rc;
}

void cli_simple_loop(void)
{
    int len;
    int flag;
    int rc = 1;

    for (;;) {
        len = cli_readline(CONFIG_SYS_PROMPT);

        flag = 0;    /* assume no special flags for now */

        if (len == -1)
            puts("<INTERRUPT>\n");
        else
            rc = cli_simple_run_command(console_buffer, flag);

        if (rc == 0xE0F) {
            puts("<CLI EXIT>\n");
            break;
        }
    }
}

