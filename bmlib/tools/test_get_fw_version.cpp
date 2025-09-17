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
	unsigned core_num = 0;

	ret = bm_dev_request(&handle, chip_num);
	if (ret != BM_SUCCESS || handle == NULL) {
		printf("bm_dev_request failed, ret = %d\n", ret);
		return -1;
	}

	ret = bm_get_tpu_scalar_num(handle, &core_num);

	if (core_num == 1) {
		ret = tpu_kernel_get_fw_version(handle, 0);
	} else if (core_num == 2) {
		ret = tpu_kernel_get_fw_version(handle, 0);
		ret = tpu_kernel_get_fw_version(handle, 1);
	}

	if (ret != BM_SUCCESS) {
		printf("bm_get_core_to_send failed, ret = %d\n", ret);
		return -1;
	}

	bm_dev_free(handle);
	return ret;
}