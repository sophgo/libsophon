#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "string.h"
#include "bmlib_memory.h"
#ifdef __linux
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
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

static bool bm_device_mem_range_valid(bm_handle_t handle, bm_device_mem_t mem)
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
			printf("%s saddr=0x%llx eaddr=0x%llx out of range\n", __func__, saddr, eaddr);
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
			printf("%s saddr=0x%llx eaddr=0x%llx out of range\n", __func__, saddr, eaddr);
			return false;
		}
	}
#endif
	return true;
}

bm_status_t bm_mem_mmap_device_mem_mix(bm_handle_t handle, bm_device_mem_t *dmem,
									   u64 *vmem)
{
#ifndef USING_CMODEL
	void *ret = 0;
	u64 addr, size, addr_end;
	u64 aligned_size, aligned_addr;

	if (handle->misc_info.pcie_soc_mode == 0)
	{
		printf("bmlib not support mmap in pcie mode\n");
		return BM_ERR_FAILURE;
	}
#ifdef __linux__
	// if (!bm_device_mem_page_aligned(*dmem)) {
	//   bmlib_log(BMLIB_MEMORY_LOG_TAG, BMLIB_LOG_ERROR,
	//          "bm_mem_mmap_device_mem device_mem_addr = 0x%llx is illegal\n",
	//          bm_mem_get_device_addr(*dmem));
	//   return BM_ERR_PARAM;
	// }

	if (!bm_device_mem_range_valid(handle, *dmem))
	{
		return BM_ERR_PARAM;
	}
	size = bm_mem_get_device_size(*dmem);
	addr = bm_mem_get_device_addr(*dmem);
	addr_end = addr + size;

	aligned_addr = bm_mem_get_device_addr(*dmem) & (~(PAGE_SIZE - 1));
	aligned_size = ((addr_end + (PAGE_SIZE - 1)) & (~(PAGE_SIZE - 1))) - aligned_addr;

	ret = mmap(0, aligned_size, PROT_READ | PROT_WRITE, MAP_SHARED,
			   handle->dev_fd, aligned_addr);
	// printf("mmap, addr: 0x%llx, size: 0x%llx\n", (u64)ret);
	if (MAP_FAILED != ret)
	{
		// ret = (((u64)ret) + (addr - aligned_addr))
		*vmem = (u64)ret + addr - aligned_addr;
		// printf("PAGE_SIZE: 0x%llx, aligned_addr: 0x%llx, aligned_size: 0x%llx, mmap_vaddr: 0x%llx, final_vmem: 0x%llx\n", PAGE_SIZE, aligned_addr, aligned_size, (u64)ret, *vmem);
		return BM_SUCCESS;
	}
	else
	{
		return BM_ERR_FAILURE;
	}
#endif
#else
#define GLOBAL_MEM_START_ADDR 0x100000000
	// handle->bm_dev->get_global_memaddr_(handle->dev_id);
	*vmem = (u64)((u8 *)handle->bm_dev->get_global_memaddr_(handle->dev_id) +
				  bm_mem_get_device_addr(*dmem) - GLOBAL_MEM_START_ADDR);
#endif
	return BM_SUCCESS;
}

bm_status_t bm_mem_unmap_device_mem_mix(bm_handle_t handle, void *vmem, int size)
{
#ifndef USING_CMODEL
	if (handle->misc_info.pcie_soc_mode == 0)
	{
		printf("bmlib not support unmap in pcie mode\n");
		return BM_ERR_FAILURE;
	}
#ifdef __linux__
	// unsigned int aligned_size = (size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));
	(void)munmap(vmem, size);
	// printf("unmap, addr: 0x%llx, size: 0x%llx\n", (u64)vmem, size);
#endif
#else
	UNUSED(handle);
	UNUSED(vmem);
	UNUSED(size);
#endif
	return BM_SUCCESS;
}

bm_status_t bm_memcpy_s2d_fast_mix(bm_handle_t handle, bm_device_mem_t dst, void *src)
{
#ifndef USING_CMODEL
	u64 dst_vaddr = 0;
	bm_status_t ret;
	u64 addr, size, addr_end;
	u64 aligned_size, aligned_addr;

	if (handle->misc_info.pcie_soc_mode == 0)
	{
		printf("bmlib not support s2d fast in pcie mode\n");
		return BM_ERR_FAILURE;
	}
	ret = bm_mem_mmap_device_mem_mix(handle, &dst, &dst_vaddr);
	if (ret != BM_SUCCESS)
	{
		printf("bmlib mmap in s2d fast failed\n");
		return BM_ERR_FAILURE;
	}

	memcpy((void *)dst_vaddr, src, bm_mem_get_device_size(dst));

	ret = bm_mem_flush_device_mem(handle, &dst);
	if (ret != BM_SUCCESS)
	{
		printf("bmlib invalidate device mem in s2d fast failed\n");
		return BM_ERR_FAILURE;
	}

	size = bm_mem_get_device_size(dst);
	addr = bm_mem_get_device_addr(dst);
	addr_end = addr + size;

	aligned_addr = addr & (~(PAGE_SIZE - 1));
	aligned_size = ((addr_end + (PAGE_SIZE - 1)) & (~(PAGE_SIZE - 1))) - aligned_addr;

	dst_vaddr = dst_vaddr - (addr - aligned_addr);
	bm_mem_unmap_device_mem_mix(handle, (void *)dst_vaddr, aligned_size);
#else
	UNUSED(handle);
	UNUSED(dst);
	UNUSED(src);
#endif
	return BM_SUCCESS;
}

bm_status_t bm_memcpy_d2s_fast_mix(bm_handle_t handle, void *dst, bm_device_mem_t src)
{
#ifndef USING_CMODEL
	u64 src_vaddr = 0;
	bm_status_t ret;
	u64 addr, size, addr_end;
	u64 aligned_size, aligned_addr;

	if (handle->misc_info.pcie_soc_mode == 0)
	{
		printf("bmlib not support d2s fast in pcie mode\n");
		return BM_ERR_FAILURE;
	}
	ret = bm_mem_mmap_device_mem_mix(handle, &src, &src_vaddr);
	if (ret != BM_SUCCESS)
	{
		printf("bmlib mmap in d2s fast failed\n");
		return BM_ERR_FAILURE;
	}

	ret = bm_mem_invalidate_device_mem(handle, &src);
	if (ret != BM_SUCCESS)
	{
		printf("bmlib invalidate device mem in d2s fast failed\n");
		return BM_ERR_FAILURE;
	}

	memcpy(dst, (void *)src_vaddr, bm_mem_get_device_size(src));

	size = bm_mem_get_device_size(src);
	addr = bm_mem_get_device_addr(src);
	addr_end = addr + size;

	aligned_addr = addr & (~(PAGE_SIZE - 1));
	aligned_size = ((addr_end + (PAGE_SIZE - 1)) & (~(PAGE_SIZE - 1))) - aligned_addr;

	src_vaddr = src_vaddr - (addr - aligned_addr);

	bm_mem_unmap_device_mem_mix(handle, (void *)src_vaddr, aligned_size);
#else
	UNUSED(handle);
	UNUSED(dst);
	UNUSED(src);
#endif
	return BM_SUCCESS;
}

bm_status_t bm_memcpy_d2s_cdma(bm_handle_t handle, void *dst, bm_device_mem_t src)
{
#ifndef USING_CMODEL
	bm_memcpy_info_t bm_mem_d2s;

#ifdef __linux__
#ifdef USING_INT_CDMA
	bm_mem_d2s.intr = true;
#else
	bm_mem_d2s.intr = false;
#endif
	bm_mem_d2s.host_addr = dst;
#else
	bm_mem_d2s.intr = 1;
	bm_mem_d2s.host_addr = (u64)dst;
#endif

	bm_mem_d2s.device_addr = bm_mem_get_device_addr(src);
	bm_mem_d2s.size = bm_mem_get_size(src);
	bm_mem_d2s.dir = CHIP2HOST;
	bm_mem_d2s.src_device_addr = 0;
	bm_mem_d2s.cdma_iommu_mode = handle->cdma_iommu_mode;

	union
	{
		void *ptr;
		u64 val;
	} ptr_to_u64;
	ptr_to_u64.ptr = dst;
	bm_profile_record_memcpy_begin(handle);
	auto res = platform_ioctl(handle, BMDEV_MEMCPY, &bm_mem_d2s);
	bm_profile_record_memcpy_end(handle, bm_mem_d2s.device_addr, ptr_to_u64.val, bm_mem_d2s.size, bm_mem_d2s.dir);
	if (0 != res)
		return BM_ERR_FAILURE;
#else
	UNUSED(handle);
	UNUSED(dst);
	UNUSED(src);
#endif
	return BM_SUCCESS;
}

bm_status_t bm_memcpy_s2d_cdma(bm_handle_t handle, bm_device_mem_t dst, void *src)
{
#ifdef USING_CMODEL
	return handle->bm_dev->bm_device_memcpy_s2d(dst, src);
#else
	if (handle == nullptr)
	{
		printf("handle is nullptr %s: %s: %d\n", __FILE__, __func__, __LINE__);
		return BM_ERR_DEVNOTREADY;
	}

	if (!bm_device_mem_range_valid(handle, dst))
	{
		return BM_ERR_PARAM;
	}

	bm_memcpy_info_t bm_mem_s2d;

#ifdef USING_INT_CDMA
	bm_mem_s2d.intr = true;
#else
	bm_mem_s2d.intr = false;
#endif
	bm_mem_s2d.host_addr = src;

	bm_mem_s2d.device_addr = bm_mem_get_device_addr(dst);
	bm_mem_s2d.size = bm_mem_get_size(dst);
	bm_mem_s2d.dir = HOST2CHIP;
	bm_mem_s2d.src_device_addr = 0;
	bm_mem_s2d.cdma_iommu_mode = handle->cdma_iommu_mode;

	union
	{
		void *ptr;
		u64 val;
	} ptr_to_u64;
	ptr_to_u64.ptr = src;
	bm_profile_record_memcpy_begin(handle);
	auto res = platform_ioctl(handle, BMDEV_MEMCPY, &bm_mem_s2d);
	bm_profile_record_memcpy_end(handle, ptr_to_u64.val, bm_mem_s2d.device_addr, bm_mem_s2d.size, bm_mem_s2d.dir);
	return (0 != res) ? BM_ERR_FAILURE : BM_SUCCESS;
#endif
}

int test_cdma_stod_transfer(int chip_num, int transfer_size, unsigned long long dst_addr, int use_cdma)
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

	if (dst_addr == 0x0)
	{
		ret = bm_malloc_device_dword(handle, &dev_buffer, transfer_size / 4);
		if (ret != BM_SUCCESS)
		{
			printf("malloc device memory size = %d failed, ret = %d\n", transfer_size, ret);
			return -1;
		}
	}
	else
	{
		dev_buffer = bm_mem_from_device(dst_addr, transfer_size);
	}
	for (int i = 0; i < 10; i++)
	{
		bm_trace_enable(handle);
		gettimeofday(&tv_start, NULL);
		bm_get_profile(handle, &profile_start);
		if (use_cdma == 1)
		{
			ret = bm_memcpy_s2d_cdma(handle, dev_buffer, sys_send_buffer);
			if (ret != BM_SUCCESS)
			{
				printf("CDMA transfer from system to device failed, ret = %d\n", ret);
				return -1;
			}
		}
		else
		{
			ret = bm_memcpy_s2d_fast_mix(handle, dev_buffer, sys_send_buffer);
			if (ret != BM_SUCCESS)
			{
				printf("memcpy transfer from system to device failed, ret = %d\n", ret);
				return -1;
			}
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

	if (use_cdma == 1)
	{
		consume = consume_real / 10;
		if (consume > 0)
		{
			float bandwidth = (float)transfer_size / (1024.0 * 1024.0) / (consume / 1000000.0);
			printf("S2D real:Transfer size:0x%x byte. Cost time:%ld us, Write Bandwidth:%.2f MB/s\n",
				   transfer_size,
				   consume,
				   bandwidth);
		}
	}
	consume_sys = 0x0;
	consume_real = 0x0;

	for (int i = 0; i < 10; i++)
	{
		bm_trace_enable(handle);
		gettimeofday(&tv_start, NULL);
		bm_get_profile(handle, &profile_start);
		if (use_cdma == 1)
		{
			ret = bm_memcpy_d2s_cdma(handle, sys_recieve_buffer, dev_buffer);
			if (ret != BM_SUCCESS)
			{
				printf("CDMA transfer from system to device failed, ret = %d\n", ret);
				return -1;
			}
		}
		else
		{
			ret = bm_memcpy_d2s_fast_mix(handle, sys_recieve_buffer, dev_buffer);
			if (ret != BM_SUCCESS)
			{
				printf("CDMA transfer from system to device failed, ret = %d\n", ret);
				return -1;
			}
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
	if (use_cdma == 1)
	{
		consume = consume_real / 10;
		if (consume > 0)
		{
			float bandwidth = (float)transfer_size / (1024.0 * 1024.0) / (consume / 1000000.0);
			printf("D2S real:Transfer size:0x%x byte. Cost time:%ld us, Write Bandwidth:%.2f MB/s\n",
				   transfer_size,
				   consume,
				   bandwidth);
		}
	}
	cmp_ret = array_cmp_int(sys_send_buffer, sys_recieve_buffer, transfer_size, "cdma test");
	printf("dev = %d, transfer test %s.\n", chip_num, cmp_ret ? "Failed" : "Success");

	if (sys_send_buffer)
		free(sys_send_buffer);
	if (sys_recieve_buffer)
		free(sys_recieve_buffer);
	if (dst_addr == 0x0)
	{
		bm_free_device(handle, dev_buffer);
	}
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

	sys_buffer = (unsigned char *)malloc(ppara->size);

	ret = bm_dev_request(&handle, ppara->dev_id);
	if (BM_SUCCESS != ret)
	{
		printf("request dev %d failed, ret = %d\n", ppara->dev_id, ret);
		return NULL;
	}

	ret = bm_malloc_device_byte(handle, &dev_buffer, ppara->size);
	if (ret != BM_SUCCESS)
	{
		printf("malloc device memory size = %d failed, ret = %d\n", ppara->size, ret);
		free(sys_buffer);
		return NULL;
	}

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
#if defined(SOC_MODE)
	int chip_num = 0;
	int transfer_size = 0;
	unsigned long long src_addr = 0;
	unsigned long long dst_addr = 0;
	int loop_num = 0;
	int interval = 0;
	int ret = 0;
	int count = 0;
	int i = 0;
	int j = 0;

	printf("cdma test:\n");
	test_cdma_stod_transfer(0, transfer_size, dst_addr, 1);

	printf("memcpy test:\n");
	test_cdma_stod_transfer(0, transfer_size, dst_addr, 0);
#else
	printf("This test case is only valid in SOC mode!\n");
#endif
	return 0;
}
