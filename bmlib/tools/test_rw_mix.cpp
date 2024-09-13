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

#if defined(SOC_MODE)
int main(int argc, char *argv[])
{
	int chip_num = 0;
	struct bm_rw reg;
	bm_handle_t handle = NULL;
	bm_status_t ret = BM_SUCCESS;

	if (argv[1])
	{
		if (strcmp("r", argv[1]) == 0)
		{
			reg.op = BM_READ;
			reg.paddr = (u64)strtol(argv[2], NULL, 16);
		}
		else if (strcmp("w", argv[1]) == 0)
		{
			reg.op = BM_WRITE;
			reg.paddr = (u64)strtol(argv[2], NULL, 16);
			reg.value = strtol(argv[3], NULL, 16);
		}
		else
		{
			printf("invalid arg\n");
			return -1;
		}
	}

	ret = bm_dev_request(&handle, chip_num);
	if (ret != BM_SUCCESS || handle == NULL)
	{
		printf("bm_dev_request failed, ret = %d\n", ret);
		return -1;
	}

	ret = bm_rw_mix(handle, &reg);
	if (ret != BM_SUCCESS)
	{
		printf("bm_rw_mix failed, ret = %d\n", ret);
		return -1;
	}

	if (reg.op == BM_READ)
		printf("read addr = 0x%llx, value = 0x%x\n", reg.paddr, reg.value);
	else if (reg.op == BM_WRITE)
		printf("write addr = 0x%llx, value = 0x%x\n", reg.paddr, reg.value);

	bm_dev_free(handle);
	return ret;
}
#else
int main()
{
	printf("only support in mix mode!\n");

	return 0;
}
#endif