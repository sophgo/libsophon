#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "string.h"
#ifdef __linux
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
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

#ifdef __linux__
void test_msleep(int n_ms)
{
	int i = 0;
	for (i = 0; i < n_ms; i++)
		usleep(1000);
}

void test_sleep(int n_ms)
{
	int loop = n_ms / 1000;
	int res = n_ms % 1000;
	int i = 0;

	test_msleep(res);
	for (i = 0; i < loop; i++)
		test_msleep(1000);
}
#endif

int test_cdma_stod_transfer(int chip_num, int transfer_size)
{
	bm_handle_t handle = NULL;
	bm_status_t ret = BM_SUCCESS;
	unsigned char *sys_send_buffer, *sys_recieve_buffer;
	int cmp_ret = 0;
	unsigned long consume_sys = 0;
	unsigned long consume_real = 0;
	unsigned long consume = 0;
	struct timespec tp;
	bm_device_mem_t dev_buffer;
	bm_profile_t profile_start, profile_end;
	struct timeval tv_start;
	struct timeval tv_end;
	struct timeval timediff;
	u64 paddr;
#ifdef __linux__
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
#else
	clock_gettime(0, &tp);
#endif
	srand(tp.tv_nsec);

	if (transfer_size == 0x0)
		transfer_size = 1024 * 1024 * 4;

	sys_send_buffer = (unsigned char *)malloc(transfer_size);
	sys_recieve_buffer = (unsigned char *)malloc(transfer_size);
	if (!sys_send_buffer || !sys_recieve_buffer)
	{
		printf("malloc buffer for test failed\n");
		return -1;
	}

	for (int i = 0; i < transfer_size; i++)
	{
		sys_send_buffer[i] = rand() % 0xff;
		sys_recieve_buffer[i] = 0x0;
	}

	ret = bm_dev_request(&handle, chip_num);
	if (ret != BM_SUCCESS || handle == NULL)
	{
		printf("bm_dev_request failed, ret = %d\n", ret);
		if (sys_send_buffer)
			free(sys_send_buffer);
		if (sys_recieve_buffer)
			free(sys_recieve_buffer);
		return -1;
	}

	bm_malloc_device_mem(handle, &paddr, 0, transfer_size);
	dev_buffer = bm_mem_from_device(paddr, transfer_size);

	for (int i = 0; i < 10; i++)
	{
		bm_trace_enable(handle);
		gettimeofday(&tv_start, NULL);
		bm_get_profile(handle, &profile_start);
		ret = bm_memcpy_s2d(handle, dev_buffer, sys_send_buffer);
		if (ret != BM_SUCCESS)
		{
			printf("CDMA transfer from system to device failed, ret = %d\n", ret);
			return -1;
		}

		gettimeofday(&tv_end, NULL);
		timersub(&tv_end, &tv_start, &timediff);
		consume = timediff.tv_sec * 1000000 + timediff.tv_usec;
		consume_sys += consume;

		bm_get_profile(handle, &profile_end);
		consume = profile_end.cdma_out_time - profile_start.cdma_out_time;
		bm_trace_disable(handle);

		consume_real += consume;
	}
	consume = consume_sys / 10;
	if (consume > 0)
	{
		float bandwidth = (float)transfer_size / (1024.0 * 1024.0) / (consume / 1000000.0);
		printf("S2D sys:Transfer size:0x%x byte. Cost time:%ld us, Write Bandwidth:%.2f MB/s\n",
			   transfer_size,
			   consume,
			   bandwidth);
	}
	consume = consume_real / 10;
	if (consume > 0)
	{
		float bandwidth = (float)transfer_size / (1024.0 * 1024.0) / (consume / 1000000.0);
		printf("S2D real:Transfer size:0x%x byte. Cost time:%ld us, Write Bandwidth:%.2f MB/s\n",
			   transfer_size,
			   consume,
			   bandwidth);
	}

	consume_sys = 0x0;
	consume_real = 0x0;

	for (int i = 0; i < 10; i++)
	{
		bm_trace_enable(handle);
		gettimeofday(&tv_start, NULL);
		bm_get_profile(handle, &profile_start);
		ret = bm_memcpy_d2s(handle, sys_recieve_buffer, dev_buffer);
		if (ret != BM_SUCCESS)
		{
			printf("CDMA transfer from system to device failed, ret = %d\n", ret);
			return -1;
		}

		gettimeofday(&tv_end, NULL);
		timersub(&tv_end, &tv_start, &timediff);
		consume = timediff.tv_sec * 1000000 + timediff.tv_usec;
		consume_sys += consume;
		bm_get_profile(handle, &profile_end);
		consume = profile_end.cdma_in_time - profile_start.cdma_in_time;
		consume_real += consume;
		bm_trace_disable(handle);
	}
	consume = consume_sys / 10;
	if (consume > 0)
	{
		float bandwidth = (float)transfer_size / (1024.0 * 1024.0) / (consume / 1000000.0);
		printf("D2S sys:Transfer size:0x%x byte. Cost time:%ld us, Write Bandwidth:%.2f MB/s\n",
			   transfer_size,
			   consume,
			   bandwidth);
	}
	consume = consume_real / 10;
	if (consume > 0)
	{
		float bandwidth = (float)transfer_size / (1024.0 * 1024.0) / (consume / 1000000.0);
		printf("D2S real:Transfer size:0x%x byte. Cost time:%ld us, Write Bandwidth:%.2f MB/s\n",
			   transfer_size,
			   consume,
			   bandwidth);
	}
	cmp_ret = array_cmp_int(sys_send_buffer, sys_recieve_buffer, transfer_size, "cdma test");
	printf("dev = %d, cdma transfer test %s.\n", chip_num, cmp_ret ? "Failed" : "Success");

	if (sys_send_buffer)
		free(sys_send_buffer);
	if (sys_recieve_buffer)
		free(sys_recieve_buffer);

	bm_free_device_mem(handle, paddr);
	bm_dev_free(handle);
	return cmp_ret;
}

struct cdma_process_para
{
	int dev_id;
	int size;
	int launch_num;
	int dir;
};

#ifdef __linux__
void *test_cdma_thread(void *arg)
{
#else
DWORD WINAPI test_cdma_thread(LPVOID arg)
{
#endif
	bm_handle_t handle;
	bm_status_t ret = BM_SUCCESS;
	struct cdma_process_para *ppara = (struct cdma_process_para *)arg;
	unsigned char *sys_buffer;
	bm_device_mem_t dev_buffer;
	int i = 0x0;
	u64 paddr;

	sys_buffer = (unsigned char *)malloc(ppara->size);

	ret = bm_dev_request(&handle, ppara->dev_id);
	if (BM_SUCCESS != ret)
	{
		printf("request dev %d failed, ret = %d\n", ppara->dev_id, ret);
		return NULL;
	}

	bm_malloc_device_mem(handle, &paddr, 0, ppara->size);
	dev_buffer = bm_mem_from_device(paddr, ppara->size);

	for (i = 0; i < ppara->launch_num; i++)
	{
		if (ppara->dir == 0x0)
		{
			ret = bm_memcpy_s2d(handle, dev_buffer, sys_buffer);
			if (ret != BM_SUCCESS)
			{
				printf("CDMA transfer from system to device failed, ret = %d\n", ret);
			}
		}
		else
		{
			ret = bm_memcpy_d2s(handle, sys_buffer, dev_buffer);
			if (ret != BM_SUCCESS)
			{
				printf("CDMA transfer from device to sys failed, ret = %d\n", ret);
			}
		}
	}

	bm_free_device_mem(handle, paddr);
	bm_dev_free(handle);
	return NULL;
}

#define THREAD_NUM 64
// dir = 0, s2d; dir = 1, d2s
int test_cmda_perf_mutithread(int thread_num, int dir, int dev_id, int size, int launch_num)
{
#ifdef __linux__
	pthread_t threads[THREAD_NUM];
#else
	DWORD dwThreadIdArray[THREAD_NUM];
	HANDLE hThreadArray[THREAD_NUM];
#endif

	struct cdma_process_para para;
	int i = 0x0;
	int ret = 0x0;
	unsigned long long total_size = size * thread_num * launch_num;
	float sys_bandwidth = 0;
	unsigned long long sys_trans_time_us = 0;
	struct timeval tv_start;
	struct timeval tv_end;
	struct timeval timediff;

	if (thread_num > THREAD_NUM)
	{
		printf("thread num = %d is too much\n", thread_num);
		return -1;
	}
	para.dev_id = dev_id;
	para.size = size;
	para.launch_num = launch_num;
	para.dir = dir;

	gettimeofday(&tv_start, NULL);
	for (i = 0; i < thread_num; i++)
	{
#ifdef __linux__
		ret = pthread_create(&threads[i], NULL, test_cdma_thread, &para);
		if (ret < 0)
		{
			printf("pthread_create %d error: error_code = %d\n", i, ret);
			return -1;
		}
#else
		hThreadArray[i] =
			CreateThread(NULL,				   // default security attributes
						 0,					   // use default stack size
						 test_cdma_thread,	   // thread function name
						 &para,				   // argument to thread function
						 0,					   // use default creation flags
						 &dwThreadIdArray[i]); // returns the thread identifier
		if (hThreadArray[i] == NULL)
		{
			printf("creatthread %d and thread_id 0x%08lx failed\n", i, dwThreadIdArray[i]);
			// ExitProcess(3);
			return -1;
		}
#endif
	}
#ifdef __linux__
	for (i = 0; i < thread_num; i++)
	{
		ret = pthread_join(threads[i], NULL);
		if (ret < 0)
		{
			printf("pthread_join %d error: error_code = %d\n", i, ret);
			return -1;
		}
	}
#endif
#ifdef _WIN32
	for (i = 0; i < thread_num; i++)
	{
		DWORD dwWaitResult = WaitForSingleObject(hThreadArray[i], INFINITE);
		switch (dwWaitResult)
		{
		case WAIT_OBJECT_0:
			ret = 0;
			break;
		case WAIT_FAILED:
			ret = -1;
			break;
		case WAIT_ABANDONED:
			ret = -2;
			break;
		case WAIT_TIMEOUT:
			ret = -3;
			break;
		default:
			ret = 0;
			break;
		}
		if (ret < 0)
		{
			printf("WaitForSingleObject %d error: error_code = %d\n", i, ret);
			return -1;
		}
	}

	for (i = 0; i < thread_num; i++)
		CloseHandle(hThreadArray[i]);
#endif

	gettimeofday(&tv_end, NULL);
	timersub(&tv_end, &tv_start, &timediff);
	sys_trans_time_us = timediff.tv_sec * 1000000 + timediff.tv_usec;

	if (sys_trans_time_us > 0)
	{
		sys_bandwidth = (float)(total_size / (1024.0 * 1024.0)) / (sys_trans_time_us / 1000000.0);
	}
	else
	{
		return -1;
	}

	if (dir == 0x0)
	{
		printf("cdma s2d test use %d thread bandwidth : %.2f MB/s\n", thread_num, sys_bandwidth);
	}
	else
	{
		printf("cdma d2s test use %d thread bandwidth : %.2f MB/s\n", thread_num, sys_bandwidth);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int chip_num = 0;
	int transfer_size = 0;
	int loop_num = 0;
	int interval = 0;
	int ret = 0;
	int count = 0;
	int i = 0;
	int j = 0;

	bm_dev_getcount(&count);
	for (i = 0; i < count; i++)
	{
		ret = test_cdma_stod_transfer(i, transfer_size);
		if (ret != 0)
			break;
		test_cmda_perf_mutithread(4, 0, i, (1024 * 1024 * 4), 20);
		test_cmda_perf_mutithread(4, 1, i, (1024 * 1024 * 4), 20);
		test_cmda_perf_mutithread(8, 0, i, (1024 * 1024 * 4), 20);
		test_cmda_perf_mutithread(8, 1, i, (1024 * 1024 * 4), 20);
		test_cmda_perf_mutithread(16, 0, i, (1024 * 1024 * 4), 20);
		test_cmda_perf_mutithread(16, 1, i, (1024 * 1024 * 4), 20);
	}

	return ret;
}
