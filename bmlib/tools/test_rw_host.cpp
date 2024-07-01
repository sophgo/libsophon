#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "string.h"

#ifdef WIN32
#include <io.h>
#include <time.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif

int main(int argc, char *argv[])
{
#ifndef SOC_MODE
	int chip_num = 0;
	struct bm_rw reg;
	bm_handle_t handle = NULL;
	bm_status_t ret = BM_SUCCESS;
	enum bm_rw_op option;

	if (argv[1]) {
		if (strcmp("r", argv[1]) == 0)
			reg.op = BM_READ;
		else if (strcmp("w", argv[1]) == 0) {
			reg.op = BM_WRITE;
			reg.value = strtol(argv[2], NULL, 16);
		} else if (strcmp("m", argv[1]) == 0)
			reg.op = BM_MALLOC;
		else if (strcmp("f", argv[1]) == 0)
			reg.op = BM_FREE;
		else {
			printf("invalid arg\n");
			return -1;
		}
	}

	ret = bm_dev_request(&handle, chip_num);
	if (ret != BM_SUCCESS || handle == NULL) {
		printf("bm_dev_request failed, ret = %d\n", ret);
		return -1;
	}

	option = reg.op;
	ret = bm_rw_host(handle, &reg);
	if (ret != BM_SUCCESS) {
		printf("bm_rw_mix failed, ret = %d\n", ret);
		return -1;
	}

	if (option == BM_READ)
		printf("read addr = 0x%llx, value = 0x%x\n", reg.paddr, reg.value);
	else if (option == BM_WRITE)
		printf("write addr = 0x%llx, value = 0x%x\n", reg.paddr, reg.value);
	else if (option == BM_MALLOC)
		printf("malloc addr = 0x%llx\n", reg.paddr);
	else if (option == BM_FREE)
		printf("free addr = 0x%llx\n", reg.paddr);

	bm_dev_free(handle);
	return ret;
#else

	printf("only support in pcie mode!\n");

	return 0;
#endif
}
