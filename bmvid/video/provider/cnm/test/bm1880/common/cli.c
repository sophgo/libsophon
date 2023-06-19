#include <stdio.h>
#include <string.h>

#include "command.h"
#include "uart.h"

#if defined(CONFIG_CHIP_BM1682)
#define CLI_PROMPT              "BM1682> "
#elif defined(CONFIG_CHIP_BM1880)
#define CLI_PROMPT              "BM1880> "
#else
#define CLI_PROMPT              "UNKNOWN> "
#endif
#define CLI_CBSIZE              31
#define CLI_MAXARGS             4

#define isblank(c)              ((uint8_t)c == 0x20)

int cli_readline(char *prompt, char *cmdbuf)
{
  int     n = 0;
  char   *p = cmdbuf;
  char   *p_buf = p;
  cmdbuf[0] = '\0';

  if (prompt)
    uart_puts(prompt);

  for (;;) {
    char c = uart_getc();
    switch (c) {
    case '\r':                      /* Enter                */
    case '\n':
      *p = '\0';
      uart_puts("\n");
      return p - p_buf;
    case '\0':                      /* nul                  */
      continue;
    case 0x03:                      /* ^C - break           */
      p_buf[0] = '\0';              /* discard input */
      return -1;
    case 0x15:                      /* ^U - erase line      */
    case 0x17:                      /* ^W - erase word      */
    case 0x08:                      /* ^H  - backspace      */
    case 0x7F:                      /* DEL - backspace      */
      continue;
    default:
      /*
       * Must be a normal character then
       */
      if (n < CLI_CBSIZE-2) {
        uart_putc(c);
        *p++ = c;
        ++n;
      } else {
        return p - p_buf;
      }
    }
  }
  return 0;
}

int cli_simple_parse_line(char *line, char *argv[])
{
  int nargs = 0;

  //debug_parser("%s: \"%s\"\n", __func__, line);
  while (nargs < CLI_MAXARGS) {
    /* skip any white space */
    while (isblank(*line))
      ++line;

    if (*line == '\0') {    /* end of line, no more args    */
      argv[nargs] = NULL;
      //debug_parser("%s: nargs=%d\n", __func__, nargs);
      return nargs;
    }

    argv[nargs++] = line;   /* begin of argument string     */

    /* find end of string */
    while (*line && !isblank(*line))
      ++line;

    if (*line == '\0') {    /* end of line, no more args    */
      argv[nargs] = NULL;
      //debug_parser("parse_line: nargs=%d\n", nargs);
      return nargs;
    }

    *line++ = '\0';         /* terminate current arg         */
  }

  printf("** Too many args (max. %d) **\n", CLI_MAXARGS);

  //debug_parser("%s: nargs=%d\n", __func__, nargs);
  return nargs;
}

int cli_run_command(char *cmd)
{
  char cmdbuf[CLI_CBSIZE];
  int rc = 0;

  if (strlen(cmd) >= CLI_CBSIZE) {
    uart_puts("## Command too long!\n");
    return -1;
  }
  strcpy(cmdbuf, cmd);

  if (strcmp(cmdbuf, "exit") == 0) {
    printf("exit command\n");
    rc = 1;
  } else if (strcmp(cmdbuf, "test") == 0) {
    printf("test command\n");
    rc = 0;
  } else {
    char *argv[CLI_MAXARGS + 1];
    int argc = cli_simple_parse_line(cmdbuf, argv);
    rc = command_main(argc, argv);
  }
  return rc;
}

void delay(int a)
{
    int b = 100000, c=100000;
    for (; a>0; a--)
        for (; b>0; b--)
            for (; c>0; c--)
                __asm__ ("nop");
}

int cli_simple_loop(void)
{
  uint8_t c;
  static char lastcommand[CLI_CBSIZE + 1] = { 0, };
  static char cli_command[CLI_CBSIZE + 1] = { 0, };

  if (!uart_tstc())
    return 0;

  c = (uint8_t)uart_getc();
  uart_putc(c);
  if (c == 'q')
    return -1;
  if (c != 'c')
    return 0;

  uart_puts("Enter CLI\n");

  while (1) {
    int len = cli_readline(CLI_PROMPT, cli_command);

    if (len > 0) {
      strncpy(lastcommand, cli_command, CLI_CBSIZE + 1);
    } else if (len == 0) {
      // repeat
    } else if (len == -1) {
      // interrupt
    } else if (len == -2) {
      // timeout
      uart_puts("\nTimed out waiting for command\n");
      return 0;
    }

    int rc = cli_run_command(lastcommand);

    if (rc <= 0) {
      /* invalid command or not repeatable, forget it */
      lastcommand[0] = 0;
    }

    if (rc == 1) {
      /* exit */
      uart_puts("Exit CLI\n");
      return 0;
    }
  }
  return 0;
}
