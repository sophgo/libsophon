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
#include <io.h>
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#endif
#define THREAD_NUM 10
typedef struct gdma_d2d_st {
	bm_handle_t handle;
	int transfer_size;
	unsigned long long src_device_addr;
	unsigned long long dst_device_addr;
	int core_id;
	int loop_num;
}gdma_d2d_t;

int array_cmp_int(
		unsigned char *p_exp,
		unsigned char *p_got,
		int len,
		const char *info_label)
{
	int idx;
	for (idx = 0; idx < len; idx++) {
		if (p_exp[idx] != p_got[idx]) {
			printf("%s error at index %d exp %x got %x\n",
				info_label, idx, p_exp[idx], p_got[idx]);
			return -1;
		}
	}
	return 0;
}

void *test_gdma_d2d(void *arg)
{
	gdma_d2d_t *param = (gdma_d2d_t *)arg;
	bm_handle_t handle = param->handle;
	int transfer_size = param->transfer_size;
	unsigned long long src_device_addr = param->src_device_addr;
	unsigned long long dst_device_addr = param->dst_device_addr;
	int core_id = param->core_id;
	int loop_num = param->loop_num;

	bm_status_t ret = BM_SUCCESS;
	int i = 0;
	int rand_num = rand();
	unsigned long long time = 0;
	bm_device_mem_t src_device_buffer;
	bm_device_mem_t dst_device_buffer;
	unsigned char* sys_send_buffer = (unsigned char *)malloc(transfer_size);
	unsigned char* sys_receive_buffer = (unsigned char *)malloc(transfer_size);
	struct timespec start_time, end_time;
	unsigned int duration = 0;

	if (!sys_send_buffer || !sys_receive_buffer) {
		printf("malloc buffer for test failed\n");
		return NULL;
	}

	for (i = 0; i < transfer_size; i++)
		*(sys_send_buffer + i) = i + rand_num;

	if (src_device_addr == 0x0) {
		ret = bm_malloc_device_byte_heap_mask(handle, &src_device_buffer, 3, transfer_size);
		if (ret != BM_SUCCESS) {
			printf("malloc device memory size = %d failed, ret = %d\n", transfer_size, ret);
			return NULL;
		}
	} else {
		src_device_buffer = bm_mem_from_device(src_device_addr, transfer_size);
	}

	if (dst_device_addr == 0x0) {
		ret = bm_malloc_device_byte_heap_mask(handle, &dst_device_buffer, 3, transfer_size);
		if (ret != BM_SUCCESS) {
			printf("malloc device memory size = %d failed, ret = %d\n", transfer_size, ret);
			return NULL;
		}
	} else {
		dst_device_buffer = bm_mem_from_device(dst_device_addr, transfer_size);
	}

  	printf("bm_memcpy_s2d\n");
	ret = bm_memcpy_s2d(handle, src_device_buffer, sys_send_buffer);
	if (ret != BM_SUCCESS) {
		if (sys_send_buffer)
			free(sys_send_buffer);
		if (sys_receive_buffer)
			free(sys_receive_buffer);
		printf("CDMA transfer from system to device failed, ret = %d\n", ret);
		return NULL;
	}

	printf("bm_memcpy_d2d\n");
	if (core_id == 2) {
		for (i = 0; i < loop_num; i++) {
			ret = bm_memcpy_d2d_byte_with_core(handle, dst_device_buffer, 0, src_device_buffer, 0, transfer_size, 0);
			ret = bm_memcpy_d2d_byte_with_core(handle, dst_device_buffer, 0, src_device_buffer, 0, transfer_size, 1);
		}
	} else {
		clock_gettime(CLOCK_MONOTONIC, &start_time);
		for (i = 0; i < loop_num; i++) {
			ret = bm_memcpy_d2d_byte_with_core(handle, dst_device_buffer, 0, src_device_buffer, 0, transfer_size, core_id);
		}
		clock_gettime(CLOCK_MONOTONIC, &end_time);
		duration = (end_time.tv_sec - start_time.tv_sec)*1000000 + (end_time.tv_nsec - start_time.tv_nsec)/1000;
	}
	if (ret != BM_SUCCESS) {
		printf("GDMA transfer from system to system failed, ret = %d\n", ret);
		return NULL;
	} else {
		printf("Src:%llx, Dst:%llx, size:0x%x cost:%dus core_id:0x%x, loop_num:%d success\n",
				bm_mem_get_device_addr(src_device_buffer),
				bm_mem_get_device_addr(dst_device_buffer),
				transfer_size,
				duration,
				core_id,
				loop_num);
	}

	printf("bm_memcpy_d2s\n");
	ret = bm_memcpy_d2s(handle, sys_receive_buffer, dst_device_buffer);

	if (ret != BM_SUCCESS) {
		if(sys_send_buffer) free(sys_send_buffer);
		if(sys_receive_buffer) free(sys_receive_buffer);
		printf("CDMA transfer from system to device failed, ret = %d\n", ret);
		return NULL;
	}

	printf("array_cmp_int\n");
	if (array_cmp_int(sys_send_buffer, sys_receive_buffer, transfer_size, "test_gdma")) {
		if (sys_send_buffer)
			free(sys_send_buffer);
		if (sys_receive_buffer)
			free(sys_receive_buffer);
		printf("Src:%llx, Dst:%llx, size:0x%x core_id:0x%x, fail\n",
				bm_mem_get_device_addr(src_device_buffer),
				bm_mem_get_device_addr(dst_device_buffer),
				transfer_size,
				core_id);
		return NULL;
	} else {
		printf("Src:%llx, Dst:%llx, size:0x%x core_id:0x%x, success\n",
			bm_mem_get_device_addr(src_device_buffer),
			bm_mem_get_device_addr(dst_device_buffer),
			transfer_size,
			core_id);
	}

	if (sys_send_buffer)
		free(sys_send_buffer);
	if (sys_receive_buffer)
		free(sys_receive_buffer);

  	return NULL;
}

int main(int argc, char *argv[])
{
	bm_handle_t handle = NULL;
	gdma_d2d_t gdma_d2d;
	bm_status_t ret = BM_SUCCESS;
	struct timespec tp;
	int count = 0x0;
	int chip_num = 0x0;
	int core_id = 0;
	int transfer_size = 0;
	unsigned long long src_addr = 0;
	unsigned long long dst_addr = 0;
	int loop_num = 0;
	int i = 0;
	printf("I'm starting gdma_perf\n");

	#ifdef __linux__
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
	#else
	clock_gettime(0, &tp);
	#endif
	srand(tp.tv_nsec);

	if (argv[1]) {
		if (strcmp("d2d", argv[1]) == 0) {
			if (argc != 7) {
				printf("invalid arg\n");
				printf("Uasge: test_gdma_perf d2d chip_num core_id size src_addr dst_addr\n");
				printf("Example: test_gdma_perf d2d 0 0 0x100000 0x180000000 0x190000000\n");
				return -1;
			}
			chip_num = atoi(argv[2]);
			core_id = atoi(argv[3]);
			transfer_size = (int)strtol(argv[4], NULL, 16);
			src_addr = strtoll(argv[5], NULL, 16);
			dst_addr = strtoll(argv[6], NULL, 16);
			printf("test chip num = 0x%x, core_id = %d, transfer_size = 0x%x, src_addr = 0x%llx, dst_addr = 0x%llx\n",
				chip_num, core_id, transfer_size, src_addr, dst_addr);

			printf("bm_dev_request\n");
			ret = bm_dev_request(&handle, chip_num);
			if (ret != BM_SUCCESS || handle == NULL) {
				printf("bm_dev_request failed, ret = %d\n", ret);
				return -1;
			}

			gdma_d2d.handle = handle;
			gdma_d2d.transfer_size = transfer_size;
			gdma_d2d.src_device_addr = src_addr;
			gdma_d2d.dst_device_addr = dst_addr;
			gdma_d2d.core_id = core_id;
			gdma_d2d.loop_num = 1;
			test_gdma_d2d(&gdma_d2d);
			bm_dev_free(handle);
			return 0;
		} else if (strcmp("stress", argv[1]) == 0) {
			if (argc != 8) {
				printf("invalid arg\n");
				printf("Usage: test_gdma_perf stress chip_num core_id size src_addr dst_addr loopnum\n");
				printf("Example: test_gdma_perf stress 0 0 0x100000 0x180000000 0x190000000 10000\n");
				return -1;
			}
			chip_num = atoi(argv[2]);
			core_id = atoi(argv[3]);
			transfer_size = (int)strtol(argv[4], NULL, 16);
			src_addr = strtoll(argv[5], NULL, 16);
			dst_addr = strtoll(argv[6], NULL, 16);
			loop_num = atoi(argv[7]);
			printf("test chip num = 0x%x, core_id = %d, transfer_size = 0x%x, src_addr = 0x%llx, dst_addr = 0x%llx, loop_num = %lld\n",
				chip_num, core_id, transfer_size, src_addr, dst_addr, loop_num);

			ret = bm_dev_request(&handle, chip_num);
			if (ret != BM_SUCCESS || handle == NULL) {
				printf("bm_dev_request failed, ret = %d\n", ret);
				return -1;
			}
			gdma_d2d.handle = handle;
			gdma_d2d.transfer_size = transfer_size;
			gdma_d2d.src_device_addr = src_addr;
			gdma_d2d.dst_device_addr = dst_addr;
			gdma_d2d.core_id = core_id;
			gdma_d2d.loop_num = loop_num;
			test_gdma_d2d(&gdma_d2d);
			bm_dev_free(handle);
			return 0;
		} else {
			printf("invalid arg\n");
			printf("test_gdma_perf d2d chip_num core_id size src_addr dst_addr\n");
			printf("test_gdma_perf d2d 0 0 0x100000 0x180000000 0x190000000\n");
			return -1;
		}
	} else {
		bm_dev_getcount(&count);
		for (i = 0; i < count; i++) {
			ret = bm_dev_request(&handle, i);
			if (ret != BM_SUCCESS || handle == NULL) {
				printf("bm_dev_request failed, ret = %d\n", ret);
				return -1;
			}

			pthread_t threads[THREAD_NUM];
			//gdma_d2d_t arg[]={{handle, 256 * 1024, 0, 0, 0, 1},
			//					{handle, 256 * 1024, 0, 0, 1, 1},
			//					{handle, 256 * 1024, 0, 0, 0, 1},
			//					{handle, 256 * 1024, 0, 0, 1, 1},
			//					{handle, 256 * 1024, 0, 0, 0, 1},
			//					{handle, 256 * 1024, 0, 0, 1, 1},
			//					{handle, 256 * 1024, 0, 0, 0, 1},
			//					{handle, 256 * 1024, 0, 0, 1, 1},
			//					{handle, 256 * 1024, 0, 0, 0, 1},
			//					{handle, 256 * 1024, 0, 0, 1, 1}};
			gdma_d2d_t arg[]={{handle, 256 * 1024, 0, 0, 0, 1},
								{handle, 256 * 1024, 0, 0, 0, 1},
								{handle, 256 * 1024, 0, 0, 0, 1},
								{handle, 256 * 1024, 0, 0, 0, 1},
								{handle, 256 * 1024, 0, 0, 0, 1},
								{handle, 256 * 1024, 0, 0, 0, 1},
								{handle, 256 * 1024, 0, 0, 0, 1},
								{handle, 256 * 1024, 0, 0, 0, 1},
								{handle, 256 * 1024, 0, 0, 0, 1},
								{handle, 256 * 1024, 0, 0, 0, 1}};

			for (int thread_cnt = 0; thread_cnt < THREAD_NUM; thread_cnt++) {
				if (pthread_create(&threads[thread_cnt], NULL, test_gdma_d2d, (void *)&arg[thread_cnt])) {
					printf("create thread %d error!\n", thread_cnt);
					return -1;
				}
			}

			for (int thread_cnt = 0; thread_cnt < THREAD_NUM; thread_cnt++) {
				if (pthread_join(threads[thread_cnt], NULL)) {
					printf("join thread %d error!\n", thread_cnt);
					return -1;
				}
			}

			bm_dev_free(handle);
		}
	}
	return 0;
}
