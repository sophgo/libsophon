#include "boot_test.h"

void pll_init()
{
  //unsigned int div = (0x18 * CONFIG_SYS_CLOCK) / 300;
  //unsigned int reg_config = 0x001102 | (div << 16);
  //cpu_write32(0x500080e8, 0x00301102);
  //while(cpu_read32(0x50008100) != 0);

  //600MHz
  //uartlog("set main clk to 600MHz\n");
  //cpu_write32(0x500080e8, 0x00301102);
  //while(cpu_read32(0x50008100) != 0);

  //687.5MHz
  uartlog("set main clk to 687.5MHz\n");
  cpu_write32(0x500080e8, 0x00371102);
  while(cpu_read32(0x50008100) != 0);

  //700MHz
  //uartlog("set main clk to 700MHz\n");
  //cpu_write32(0x500080e8, 0x00381102);
  //while(cpu_read32(0x50008100) != 0);
}
