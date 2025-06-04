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

  if (argc != 1) {
      printf("invalid arg\n");
      return -1;
  }

  ret = bm_dev_request(&handle, chip_num);
  if (ret != BM_SUCCESS || handle == NULL) {
    printf("bm_dev_request failed, ret = %d\n", ret);
    return -1;
  }

  ret = bm_reset_tpu(handle);
  if (ret != BM_SUCCESS) {
      printf("bm_reset_tpu failed, ret = %d\n", ret);
  return -1;
  }

  bm_dev_free(handle);
  return ret;
}
