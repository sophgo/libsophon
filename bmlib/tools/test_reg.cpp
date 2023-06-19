#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "string.h"

int main(int argc, char *argv[])
{
  int chip_num = 0;
  int op = 0;
  struct bm_reg reg;
  bm_handle_t handle = NULL;
  bm_status_t ret = BM_SUCCESS;

  if (argv[1]) {
    if (strcmp("regr", argv[1])== 0) {
      op = 0;
      chip_num = atoi(argv[2]);
      reg.reg_addr = (int)strtol(argv[3], NULL, 16);
    } else if(strcmp("regw", argv[1])== 0) {
      op = 1;
      chip_num = atoi(argv[2]);
      reg.reg_addr = (int)strtol(argv[3], NULL, 16);
      reg.reg_value = strtol(argv[4], NULL, 16);
    } else {
      printf("invalid arg\n");
      return -1;
    }
  }

  ret = bm_dev_request(&handle, chip_num);
  if (ret != BM_SUCCESS || handle == NULL) {
    printf("bm_dev_request failed, ret = %d\n", ret);
    return -1;
  }

  if (op == 0) {
    ret = bm_get_reg(handle, &reg);
    if (ret != BM_SUCCESS) {
      printf("bm_get_reg failed, ret = %d\n", ret);
      return -1;
    }
    printf("read reg = 0x%x, value = 0x%x\n", reg.reg_addr, reg.reg_value);
  } else {
    ret = bm_set_reg(handle, &reg);
    if (ret != BM_SUCCESS) {
    printf("bm_set_reg failed, ret = %d\n", ret);
    return -1;
    }
    printf("write reg = 0x%x, value = 0x%x\n", reg.reg_addr, reg.reg_value);
  }

  bm_dev_free(handle);
  return ret;
}
