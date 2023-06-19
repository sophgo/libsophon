#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system_common.h"
#include "uart.h"

typedef int (*cmd_func_t)(int argc, char **argv);

int do_help(int argc, char **argv);
int do_rm(int argc, char **argv);
int do_wm(int argc, char **argv);

struct cmd_entry {
  char name[8];
  cmd_func_t func;
  int arg_count;
  char usage[32];
};

static struct cmd_entry cmd_list[] = {
  {"help", do_help, 0, ""},
  {"rm", do_rm, 1, "address"},
  {"wm", do_wm, 2, "address value"},
};

int do_help(int argc, char **argv)
{
  int i;
  printf("Command List:\n");
  for (i = 0; i < sizeof(cmd_list)/sizeof(cmd_list[0]); i++) {
    printf("  %s %s\n", cmd_list[i].name, cmd_list[i].usage);
  }
  return 0;
}

int do_rm(int argc, char **argv)
{
  void *addr;
  unsigned int val;
  printf("%s %s\n", argv[0], argv[1]);
  addr = (void *)strtoul(argv[1], NULL, 16);
  printf("read addr = 0x%p, ", addr);
  val = __raw_readl(addr);
  printf("value = 0x%x\n", val);
  return 0;
}

int do_wm(int argc, char **argv)
{
  void *addr;
  unsigned int val;
  printf("%s %s %s\n", argv[0], argv[1], argv[2]);
  addr =(void *)strtoul(argv[1], NULL, 16);
  val = strtoul(argv[2], NULL, 16);
  printf("write addr = 0x%p, value = 0x%x\n", addr, val);
  __raw_writel(addr, val);
  return 0;
}

static int find_cmd(char *cmd)
{
  int i;
  for (i = 0; i < sizeof(cmd_list)/sizeof(cmd_list[0]); i++) {
    if (strcmp(cmd, cmd_list[i].name) == 0)
      return i;
  }
  return -1;
}

int command_main(int argc, char **argv)
{
  int cmd_id = find_cmd(argv[0]);
  if (cmd_id < 0) {
     printf("unknown command %s\n", argv[0]);
     return -1;
  }
  if (argc != (cmd_list[cmd_id].arg_count + 1)) {
    printf("%s with wrong argument, usage:\n", cmd_list[cmd_id].name);
    printf("  %s %s\n", cmd_list[cmd_id].name, cmd_list[cmd_id].usage);
    return -1;
  }
  return cmd_list[cmd_id].func(argc, argv);
}
