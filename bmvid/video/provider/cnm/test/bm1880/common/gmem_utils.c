#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "system_common.h"
#include "gmem_utils.h"

#define SEED 10 //rand seed
void globalmem_fill_rand_ival(u64 ga, u32 count)
{
  //uartlog("%s, ga:0x%lx\n", __func__, ga);

  u64 base = GLOBAL_MEM_START_ADDR + ga;
  srand(SEED);
  for (ulong m = 0; m < count * 4; m+=4) {
    ulong value = rand();
    writel(base + m, value);
  }
}

void globalmem_fill_u32_const(u64 ga, u32 val, u32 count)
{
  //uartlog("%s\n", __func__);

  u64 base = GLOBAL_MEM_START_ADDR + ga;

  for (u32 m = 0; m < count * 4; m+=4) {
    writel(base + m, val);
  }
}

int globalmem_cmp_epsilon(u64 ga_exp, u64 ga_got, u32 size, int exact_match, float epsilon)
{
  int err = 0;

  debug("%s\n", __func__);
  float *base_exp = GA_TO_PTR(ga_exp);
  float *base_got = GA_TO_PTR(ga_got);
  for (int i = 0; i < size; i++) {
    if (exact_match) {
      if (base_exp[i] != base_got[i]) {
        printf("ERR: idx %d, exp %f (0x%08x), got %f (0x%08x)\n",
                    i, base_exp[i], (u32)base_exp[i], base_got[i], (u32)base_got[i]);
        err++;
#ifndef DEBUG
      return err;
#endif
      }
    } else {
      if (fabs(base_exp[i] - base_got[i]) > epsilon) {
        printf("ERR: idx %d, exp %f, got %f, allowed epsilon %f\n",
                    i, base_exp[i], base_got[i], epsilon);
        err++;
#ifndef DEBUG
      return err;
#endif
      }
    }

    if (isinf(base_got[i]) && !isinf(base_exp[i])) {
      printf("ERR: idx %d, exp %.10f, got %.10f\n",
                  i, base_exp[i], base_got[i]);
      err++;
#ifndef DEBUG
      return err;
#endif
    }
  }

  return err;
}

int globalmem_cmp_u32_rand(u64 ga, u32 size)
{
  int err = 0;

  //uartlog("%s: ga = %lx\n", __func__, ga);
  u64 src = GLOBAL_MEM_START_ADDR + ga;

  u32 m;
  srand(SEED);
  for (m = 0; m < size * 4; m+=4) {
    u32 expdata = rand();
    u32 rdata = readl(src + m);
    if (rdata != expdata) {
      uartlog("check error\n");
      err++;
#ifndef DEBUG
      return err;
#endif
    }
  }

  return err;
}

int globalmem_cmp_u32_const(u64 ga, u32 val, u32 size)
{
  int err = 0;

  //uartlog("%s: ga = %lx\n", __func__, ga);

  u64 base = GLOBAL_MEM_START_ADDR + ga;
  for (int i = 0; i < size * 4; i+=4) {
    u32 rdata = readl(base + i);
    if (rdata != val) {
      err++;
#ifndef DEBUG
      return err;
#endif
    }
  }

  return err;
}