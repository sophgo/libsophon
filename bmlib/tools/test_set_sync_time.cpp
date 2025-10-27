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
  bm_handle_t handle = NULL;
  bm_status_t ret = BM_SUCCESS;
  int timeout = 20000;

  if (argc > 2) {
      printf("invalid arg\n");
      return -1;
  }

  ret = bm_dev_request(&handle, chip_num);
  if (ret != BM_SUCCESS || handle == NULL) {
    printf("bm_dev_request failed, ret = %d\n", ret);
    return -1;
  }

  if (argv[1]) {
	timeout = atoi(argv[1]);
	if (timeout < 0) {
		printf("timeout must be >= 0\n");
		return -1;
	}
  }

  ret = bm_set_sync_timeout(handle, timeout);
  if (ret != BM_SUCCESS) {
	printf("set sync timeout failed, ret = %d\n", ret);
	return -1;
  }

  bm_dev_free(handle);
  return ret;
}