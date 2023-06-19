#include <stdio.h>
#include "debug.h"

void print_trace(void)
{
  printf("%s not supported\n", __func__);
}

void firmware_hang(int ret)
{
  FW_ERR("firmware hang, ret = %d\n", ret);
  while(1)
    ;
}

void hang(int ret)
{
  firmware_hang(ret);
}
