#include "boot_test.h"
#include "eu_cmd.h"

extern int testcase_main(void);

int main(void)
{
#if defined(RUN_ALONE) && !defined(RUN_IN_DDR) && !defined(NO_DDR_INIT)
  ddr_init();
#endif

  return testcase_main();
}
