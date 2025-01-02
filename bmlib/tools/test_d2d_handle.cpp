#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "bmlib_memory.h"
#include "api.h"
#ifdef __linux
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#else
#pragma comment(lib, "libbmlib-static.lib")
#endif

int array_cmp_int(
	unsigned char *p_exp,
	unsigned char *p_got,
	int len,
	const char *info_label)
{
	int idx;
	for (idx = 0; idx < len; idx++)
	{
		if (p_exp[idx] != p_got[idx])
		{
			printf("%s error at index %d exp %x got %x\n",
				   info_label, idx, p_exp[idx], p_got[idx]);
			return -1;
		}
	}
	return 0;
}

static bool test_device_mem_range_valid(bm_handle_t handle, bm_device_mem_t mem)
{
#ifdef USING_CMODEL
	UNUSED(handle);
	UNUSED(mem);
#else
	u64 saddr = bm_mem_get_device_addr(mem);
	u64 eaddr = bm_mem_get_size(mem) + saddr;

	if (handle->misc_info.chipid == 0x1684 || handle->misc_info.chipid == 0x1686)
	{
		if (((saddr >= 0x100000000 && saddr <= 0x4ffffffff) || (saddr >= 0x0 && saddr <= 0x103fffff)) && ((eaddr >= 0x100000000 && eaddr <= 0x500000000) || (eaddr >= 0x0 && eaddr <= 0x10400000)))
		{
			return true;
		}
		else
		{
			bmlib_log("bmlib_memory", BMLIB_LOG_ERROR,
					  "%s saddr=0x%llx eaddr=0x%llx out of range\n", __func__, saddr, eaddr);
			return false;
		}
	}

	if (handle->misc_info.chipid == 0x1682)
	{
		if (saddr >= 0x100000000 && saddr <= 0x2ffffffff && eaddr >= 0x100000000 && eaddr <= 0x300000000)
		{
			return true;
		}
		else
		{
			bmlib_log("bmlib_memory", BMLIB_LOG_ERROR,
					  "%s saddr=0x%llx eaddr=0x%llx out of range\n", __func__, saddr, eaddr);
			return false;
		}
	}
#endif
	return true;
}

bm_status_t test_memcpy_d2d_byte(bm_handle_t handle, bm_device_mem_t dst,
								 size_t dst_offset, bm_device_mem_t src,
								 size_t src_offset, size_t size)
{
	bm_status_t ret = BM_SUCCESS;
	tpu_kernel_module_t bm_module;
	tpu_kernel_function_t f_id;
	const char lib_path[80] = "/opt/sophon/libsophon-current/lib/dyn_load/memory_op.so";
	const char key[64] = "memory_op.so";
	int key_size = strlen(key);
	bm_profile_t profile_start, profile_end;
	unsigned long long time = 0;
	struct timeval t1, t2;

	if (!test_device_mem_range_valid(handle, src))
	{
		return BM_ERR_PARAM;
	}

	if (!test_device_mem_range_valid(handle, dst))
	{
		return BM_ERR_PARAM;
	}

	bm_api_memcpy_byte_t api = {bm_mem_get_device_addr(src) + src_offset,
								bm_mem_get_device_addr(dst) + dst_offset, size};
#ifdef USING_CMODEL
	if (fun_id != 0)
	{
#else
	if (handle->misc_info.chipid == 0x1686)
	{
		bm_module = tpu_kernel_load_module_file_key(handle, lib_path, key, key_size);
		if (bm_module == NULL)
		{
			printf("bm_module is null!\n");
			return BM_ERR_FAILURE;
		}
#endif

		f_id = tpu_kernel_get_function(handle, bm_module, "sg_api_memcpy_byte");
		gettimeofday(&t1, NULL);
		ret = tpu_kernel_launch(handle, f_id, (void *)(&api), sizeof(bm_api_memcpy_byte_t));
		gettimeofday(&t2, NULL);
		if (bm_module != NULL)
		{
			free(bm_module);
			bm_module = NULL;
		}
		time = ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec);
		if (time > 0)
		{
			float bandwidth = (float)size / (1024.0 * 1024.0) / (time / 1000000.0);
			printf("Src:%llx, Dst:%llx, Transfer size:0x%lx byte. Cost time:%lld us, Write Bandwidth:%.2f MB/s\n",
				   bm_mem_get_device_addr(src),
				   bm_mem_get_device_addr(dst),
				   size,
				   time,
				   bandwidth);
		}
	}
	else
	{
		bm_trace_enable(handle);
		bm_get_profile(handle, &profile_start);
		ret = bm_send_api(handle, BM_API_ID_MEMCPY_BYTE, (u8 *)(&api), sizeof(api));
		ret = bm_sync_api(handle);
		bm_get_profile(handle, &profile_end);
		bm_trace_disable(handle);
	}
	return ret;
}

struct gdma_process_para
{
	bm_handle_t handle;
	int dev_id;
};

void *test_gdma_thread(void *arg)
{
	bm_status_t ret = BM_SUCCESS;
	int i = 0;
	int transfer_size = 1024 * 1024 * 64;
	int rand_num = rand();
	unsigned long long time = 0;
	struct gdma_process_para *ppara = (struct gdma_process_para *)arg;
	bm_device_mem_t src_device_buffer;
	bm_device_mem_t dst_device_buffer;
	unsigned char *sys_send_buffer = (unsigned char *)malloc(transfer_size);
	unsigned char *sys_receive_buffer = (unsigned char *)malloc(transfer_size);
	bm_handle_t handle = NULL;

	if (ppara->handle == NULL)
	{
		// printf("multi handle\n");
		ret = bm_dev_request(&handle, ppara->dev_id);
		if (BM_SUCCESS != ret)
		{
			printf("request dev %d failed, ret = %d\n", ppara->dev_id, ret);
			return NULL;
		}
	}
	else
	{
		handle = ppara->handle;
		// printf("single handle\n");
	}

	if (!sys_send_buffer || !sys_receive_buffer)
	{
		printf("malloc buffer for test failed\n");
		return NULL;
	}

	for (i = 0; i < transfer_size; i++)
		*(sys_send_buffer + i) = i + rand_num;

	ret = bm_malloc_device_dword(handle, &src_device_buffer, transfer_size / 4);
	if (ret != BM_SUCCESS)
	{
		printf("malloc device memory size = %d failed, ret = %d\n", transfer_size, ret);
		return NULL;
	}

	ret = bm_malloc_device_dword(handle, &dst_device_buffer, transfer_size / 4);
	if (ret != BM_SUCCESS)
	{
		printf("malloc device memory size = %d failed, ret = %d\n", transfer_size, ret);
		return NULL;
	}

	ret = bm_memcpy_s2d(handle,
						src_device_buffer,
						sys_send_buffer);
	if (ret != BM_SUCCESS)
	{
		if (sys_send_buffer)
			free(sys_send_buffer);
		if (sys_receive_buffer)
			free(sys_receive_buffer);
		printf("CDMA transfer from system to device failed, ret = %d\n", ret);
		return NULL;
	}

	ret = test_memcpy_d2d_byte(handle,
							   dst_device_buffer, 0, src_device_buffer, 0,
							   transfer_size);

	if (ret != BM_SUCCESS)
	{
		printf("GDMA transfer from system to system failed, ret = %d\n", ret);
		return NULL;
	}

	ret = bm_memcpy_d2s(handle, sys_receive_buffer, dst_device_buffer);

	if (ret != BM_SUCCESS)
	{
		if (sys_send_buffer)
			free(sys_send_buffer);
		if (sys_receive_buffer)
			free(sys_receive_buffer);
		printf("CDMA transfer from system to device failed, ret = %d\n", ret);
		return NULL;
	}

	if (array_cmp_int(sys_send_buffer, sys_receive_buffer, transfer_size, "test_gdma"))
	{
		if (sys_send_buffer)
			free(sys_send_buffer);
		if (sys_receive_buffer)
			free(sys_receive_buffer);
		printf("Src:%llx, Dst:%llx, Transfer size:0x%x byte fail\n",
			   bm_mem_get_device_addr(src_device_buffer),
			   bm_mem_get_device_addr(dst_device_buffer),
			   transfer_size);
		return NULL;
	}

	if (sys_send_buffer)
		free(sys_send_buffer);
	if (sys_receive_buffer)
		free(sys_receive_buffer);

	if (ppara->handle == NULL)
	{
		bm_dev_free(handle);
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	bm_handle_t handle = NULL;
	int ret = 0x0;
	struct timespec tp;
	int count = 0x0;
	int chip_num = 0x0;
	int transfer_size = 0;
	unsigned long long src_addr = 0;
	unsigned long long dst_addr = 0;
	int i;
	struct gdma_process_para para;
	int thread_num = 5;
	pthread_t threads[64];

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
	srand(tp.tv_nsec);

	ret = bm_dev_request(&handle, 0);
	if (ret != BM_SUCCESS || handle == NULL)
	{
		printf("bm_dev_request failed, ret = %d\n", ret);
		return -1;
	}

	printf("multi handle\n");
	para.handle = NULL;
	para.dev_id = 0;

	for (i = 0; i < thread_num; i++)
	{
		ret = pthread_create(&threads[i], NULL, test_gdma_thread, &para);
		if (ret < 0)
		{
			printf("pthread_create %d error: error_code = %d\n", i, ret);
			return -1;
		}
	}

	for (i = 0; i < thread_num; i++)
	{
		ret = pthread_join(threads[i], NULL);
		if (ret < 0)
		{
			printf("pthread_join %d error: error_code = %d\n", i, ret);
			return -1;
		}
	}

	printf("single handle\n");
	para.handle = handle;
	para.dev_id = 0;

	for (i = 0; i < thread_num; i++)
	{
		ret = pthread_create(&threads[i], NULL, test_gdma_thread, &para);
		if (ret < 0)
		{
			printf("pthread_create %d error: error_code = %d\n", i, ret);
			return -1;
		}
	}

	for (i = 0; i < thread_num; i++)
	{
		ret = pthread_join(threads[i], NULL);
		if (ret < 0)
		{
			printf("pthread_join %d error: error_code = %d\n", i, ret);
			return -1;
		}
	}

	bm_dev_free(handle);

	return 0;
}
