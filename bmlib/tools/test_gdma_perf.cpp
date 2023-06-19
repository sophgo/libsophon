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

#define TRANSFER_SIZE 1024*1024*64*8

unsigned long long DDR0_BASE = 0X100000000;
unsigned long long DDR0_END  = 0X100000000 + 8*1024*1024*1024UL;
unsigned long long DDR1_BASE = 0X300000000 + 512*1024*1024;
unsigned long long DDR1_END  = 0X300000000 + 2*1024*1024*1024UL;
unsigned long long DDR2_BASE = 0X400000000 + 512*1024*1024;
unsigned long long DDR2_END  = 0X400000000 + 2*1024*1024*1024UL;
bm_profile_t profile_start, profile_end;

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

static bool test_device_mem_range_valid(bm_handle_t handle, bm_device_mem_t mem) {
#ifdef USING_CMODEL
  UNUSED(handle);
  UNUSED(mem);
#else
  u64 saddr = bm_mem_get_device_addr(mem);
  u64 eaddr = bm_mem_get_size(mem) + saddr;

  if (handle->misc_info.chipid == 0x1684 || handle->misc_info.chipid == 0x1686) {
    if (((saddr >= 0x100000000 && saddr <= 0x4ffffffff) || (saddr >= 0x0 && saddr <= 0x103fffff))
        && ((eaddr >= 0x100000000 && eaddr <= 0x500000000) || (eaddr >= 0x0 && eaddr <= 0x10400000))) {
      return true;
    } else {
      bmlib_log("bmlib_memory", BMLIB_LOG_ERROR,
      "%s saddr=0x%llx eaddr=0x%llx out of range\n", __func__, saddr, eaddr);
      return false;
    }
  }

  if (handle->misc_info.chipid == 0x1682) {
    if (saddr >= 0x100000000 && saddr <= 0x2ffffffff
        && eaddr >= 0x100000000 && eaddr <= 0x300000000) {
      return true;
    } else {
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
                               size_t src_offset, size_t size) {
  bm_status_t ret = BM_SUCCESS;
  tpu_kernel_module_t bm_module;
  tpu_kernel_function_t f_id;
  const char lib_path[80] = "/opt/sophon/libsophon-current/lib/tpu_module/libbm1684x_kernel_module.so";
  const char key[64] = "libbm1684x_kernel_module.so";
  int key_size = strlen(key);

  if (!test_device_mem_range_valid(handle, src)) {
    return BM_ERR_PARAM;
  }

  if (!test_device_mem_range_valid(handle, dst)) {
    return BM_ERR_PARAM;
  }

  bm_api_memcpy_byte_t api = {bm_mem_get_device_addr(src) + src_offset,
                              bm_mem_get_device_addr(dst) + dst_offset, size};
#ifdef USING_CMODEL
  if (fun_id != 0) {
#else
  if (handle->misc_info.chipid == 0x1686) {
    bm_module = tpu_kernel_load_module_file_key(handle, lib_path, key, key_size);
    if(bm_module == NULL) {
        printf("bm_module is null!\n");
        return BM_ERR_FAILURE;
    }
#endif

    f_id = tpu_kernel_get_function(handle, bm_module, "sg_api_memcpy_byte");
    bm_trace_enable(handle);
    bm_get_profile(handle, &profile_start);
    ret = tpu_kernel_launch(handle, f_id, (void *)(&api), sizeof(bm_api_memcpy_byte_t));
    if (bm_module != NULL) {
      free(bm_module);
      bm_module = NULL;
    }
    bm_get_profile(handle, &profile_end);
    bm_trace_disable(handle);
  } else {
    bm_trace_enable(handle);
    bm_get_profile(handle, &profile_start);
    ret = bm_send_api(handle, BM_API_ID_MEMCPY_BYTE, (u8 *)(&api), sizeof(api));
    ret = bm_sync_api(handle);
    bm_get_profile(handle, &profile_end);
    bm_trace_disable(handle);
  }
  return ret;
}

int test_gdma_d2d(bm_handle_t handle, int transfer_size,
                       unsigned long long src_device_addr,
                       unsigned long long dst_device_addr)
{
  bm_status_t ret = BM_SUCCESS;
  int i = 0;
  int rand_num = rand();
  unsigned long long time = 0;
  bm_device_mem_t src_device_buffer;
  bm_device_mem_t dst_device_buffer;
  unsigned char* sys_send_buffer = (unsigned char *)malloc(transfer_size);
  unsigned char* sys_receive_buffer = (unsigned char *)malloc(transfer_size);

  if (!sys_send_buffer || !sys_receive_buffer) {
    printf("malloc buffer for test failed\n");
    return -1;
  }

  for (i = 0; i < transfer_size; i++)
    *(sys_send_buffer + i) = i + rand_num;

  if (src_device_addr == 0x0) {
    ret = bm_malloc_device_dword(handle, &src_device_buffer, transfer_size/4);
    if (ret != BM_SUCCESS) {
      printf("malloc device memory size = %d failed, ret = %d\n", transfer_size, ret);
      return -1;
    }
  } else {
    src_device_buffer = bm_mem_from_device(src_device_addr, transfer_size);
  }

  if (dst_device_addr == 0x0) {
    ret = bm_malloc_device_dword(handle, &dst_device_buffer, transfer_size/4);
    if (ret != BM_SUCCESS) {
      printf("malloc device memory size = %d failed, ret = %d\n", transfer_size, ret);
      return -1;
    }
  } else {
    dst_device_buffer = bm_mem_from_device(dst_device_addr, transfer_size);
  }

  ret = bm_memcpy_s2d(handle,
		src_device_buffer,
		sys_send_buffer);
	if (ret != BM_SUCCESS) {
		if(sys_send_buffer) free(sys_send_buffer);
		if(sys_receive_buffer) free(sys_receive_buffer);
		printf("CDMA transfer from system to device failed, ret = %d\n", ret);
		return -1;
	}

  ret = test_memcpy_d2d_byte(handle,
        dst_device_buffer, 0, src_device_buffer, 0,
        transfer_size);

  if (ret != BM_SUCCESS) {
    printf("GDMA transfer from system to system failed, ret = %d\n", ret);
    return -1;
  }

  time = profile_end.tpu_process_time - profile_start.tpu_process_time;
  if (time > 0) {
     float bandwidth = (float)transfer_size / (1024.0*1024.0) / (time / 1000000.0);
     printf("Src:%llx, Dst:%llx, Transfer size:0x%x byte. Cost time:%lld us, Write Bandwidth:%.2f MB/s\n",
             bm_mem_get_device_addr(src_device_buffer),
             bm_mem_get_device_addr(dst_device_buffer),
             transfer_size,
             time,
             bandwidth);
   }

  ret = bm_memcpy_d2s(handle, sys_receive_buffer, dst_device_buffer);

  if (ret != BM_SUCCESS) {
    if(sys_send_buffer) free(sys_send_buffer);
    if(sys_receive_buffer) free(sys_receive_buffer);
    printf("CDMA transfer from system to device failed, ret = %d\n", ret);
    return -1;
  }

  if(array_cmp_int(sys_send_buffer, sys_receive_buffer, transfer_size, "test_gdma")) {
    if(sys_send_buffer) free(sys_send_buffer);
    if(sys_receive_buffer) free(sys_receive_buffer);
    printf("Src:%llx, Dst:%llx, Transfer size:0x%x byte fail\n",
             bm_mem_get_device_addr(src_device_buffer),
             bm_mem_get_device_addr(dst_device_buffer),
             transfer_size);
    return -1;
  }


  if(sys_send_buffer) free(sys_send_buffer);
  if(sys_receive_buffer) free(sys_receive_buffer);

  return 0;
}

int test_gdma_ddr0(bm_handle_t handle)
{
	unsigned long long src_device_addr = DDR0_BASE + 16*1024*1024;
	unsigned long long dst_device_addr = DDR0_BASE + 16*1024*1024 + TRANSFER_SIZE;

	if(test_gdma_d2d(handle, TRANSFER_SIZE, src_device_addr, dst_device_addr))
	{
		printf("%s, error, src_device_addr=0x%llx, dst_device_addr=0x%llx\n", __func__, src_device_addr, dst_device_addr);
		return -1;
	}

	return 0;
}

int test_gdma_ddr0_2_ddr1(bm_handle_t handle)
{
	unsigned long long src_device_addr = DDR0_BASE + 16*1024*1024;
	unsigned long long dst_device_addr = DDR1_BASE;

	if(test_gdma_d2d(handle, TRANSFER_SIZE, src_device_addr, dst_device_addr))
	{
		printf("%s, error, src_device_addr=0x%llx, dst_device_addr=0x%llx\n", __func__, src_device_addr, dst_device_addr);
		return -1;
	}

	return 0;
}

int test_gdma_ddr1_2_ddr0(bm_handle_t handle)
{
	unsigned long long dst_device_addr = DDR0_BASE + 16*1024*1024;
	unsigned long long src_device_addr = DDR1_BASE;

	if(test_gdma_d2d(handle, TRANSFER_SIZE, src_device_addr, dst_device_addr))
	{
		printf("%s, error, src_device_addr=0x%llx, dst_device_addr=0x%llx\n", __func__, src_device_addr, dst_device_addr);
		return -1;
	}

	return 0;
}

int test_gdma_ddr0_2_ddr2(bm_handle_t handle)
{
	unsigned long long src_device_addr = DDR0_BASE + 16*1024*1024;
	unsigned long long dst_device_addr = DDR2_BASE;

	if(test_gdma_d2d(handle, TRANSFER_SIZE, src_device_addr, dst_device_addr))
	{
		printf("%s, error, src_device_addr=0x%llx, dst_device_addr=0x%llx\n", __func__, src_device_addr, dst_device_addr);
		return -1;
	}

	return 0;
}

int test_gdma_ddr2_2_ddr0(bm_handle_t handle)
{
	unsigned long long dst_device_addr = DDR0_BASE + 16*1024*1024;
	unsigned long long src_device_addr = DDR2_BASE;

	if(test_gdma_d2d(handle, TRANSFER_SIZE, src_device_addr, dst_device_addr))
	{
		printf("%s, error, src_device_addr=0x%llx, dst_device_addr=0x%llx\n", __func__, src_device_addr, dst_device_addr);
		return -1;
	}

	return 0;
}

int test_gdma_ddr1(bm_handle_t handle)
{
	unsigned long long src_device_addr = DDR1_BASE;
	unsigned long long dst_device_addr = DDR1_BASE + TRANSFER_SIZE;

	if(test_gdma_d2d(handle, TRANSFER_SIZE, src_device_addr, dst_device_addr))
	{
		printf("%s, error, src_device_addr=0x%llx, dst_device_addr=0x%llx\n", __func__, src_device_addr, dst_device_addr);
		return -1;
	}

	return 0;
}

int test_gdma_ddr2(bm_handle_t handle)
{
	unsigned long long src_device_addr = DDR2_BASE;
	unsigned long long dst_device_addr = DDR2_BASE + TRANSFER_SIZE;

	if(test_gdma_d2d(handle, TRANSFER_SIZE, src_device_addr, dst_device_addr))
	{
		printf("%s, error, src_device_addr=0x%llx, dst_device_addr=0x%llx\n", __func__, src_device_addr, dst_device_addr);
		return -1;
	}

	return 0;
}

int test_gdma_ddr1_2_ddr2(bm_handle_t handle)
{
	unsigned long long src_device_addr = DDR1_BASE;
	unsigned long long dst_device_addr = DDR2_BASE;

	if(test_gdma_d2d(handle, TRANSFER_SIZE, src_device_addr, dst_device_addr))
	{
		printf("%s, error, src_device_addr=0x%llx, dst_device_addr=0x%llx\n", __func__, src_device_addr, dst_device_addr);
		return -1;
	}

	return 0;
}

int test_gdma_ddr2_2_ddr1(bm_handle_t handle)
{
	unsigned long long src_device_addr = DDR2_BASE;
	unsigned long long dst_device_addr = DDR1_BASE;

	if(test_gdma_d2d(handle, TRANSFER_SIZE, src_device_addr, dst_device_addr))
	{
		printf("%s, error, src_device_addr=0x%llx, dst_device_addr=0x%llx\n", __func__, src_device_addr, dst_device_addr);
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
  bm_handle_t handle = NULL;
  bm_status_t ret = BM_SUCCESS;
  struct timespec tp;
  int count = 0x0;
  int chip_num = 0x0;
  int transfer_size = 0;
  unsigned long long src_addr = 0;
  unsigned long long dst_addr = 0;
  int i = 0;

  #ifdef __linux__
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
  #else
  clock_gettime(0, &tp);
  #endif
  srand(tp.tv_nsec);

  if (argv[1]) {
     if (strcmp("chip", argv[1])== 0) {
       if (argc != 3) {
           printf("invalid arg\n");
           printf("example test_gdma_perf chip chip_num \n");
           printf("like test_gdma_perf chip 0 \n");
           return -1;
       }
       chip_num = atoi(argv[2]);
       ret = bm_dev_request(&handle, chip_num);
      if (ret != BM_SUCCESS || handle == NULL) {
        printf("bm_dev_request failed, ret = %d\n", ret);
        return -1;
      }

      if(test_gdma_ddr0(handle)) return -1;
      if(test_gdma_ddr1(handle)) return -1;
      if(test_gdma_ddr2(handle)) return -1;

      if(test_gdma_ddr0_2_ddr1(handle)) return -1;
      if(test_gdma_ddr1_2_ddr0(handle)) return -1;
      if(test_gdma_ddr0_2_ddr2(handle)) return -1;
      if(test_gdma_ddr2_2_ddr0(handle)) return -1;
      if(test_gdma_ddr1_2_ddr2(handle)) return -1;
      if(test_gdma_ddr2_2_ddr1(handle)) return -1;
      printf("test gdma traversal pass\n");
    } else if (strcmp("d2d", argv[1]) == 0) {
       if (argc != 6) {
           printf("invalid arg\n");
           printf("example test_gdma_perf d2d chip_num size  src_addr dst_addr \n");
           printf("like test_gdma_perf d2d 0 0x40000000  0x150000000  0x160000000\n");
           return -1;
       }
       chip_num = atoi(argv[2]);
       transfer_size = (int)strtol(argv[3], NULL, 16);

       src_addr = strtoll(argv[4], NULL, 16);
       dst_addr = strtoll(argv[5], NULL, 16);
       printf(
           "test chip num = 0x%x, transfer_size = 0x%x, src_addr = 0x%llx, " "dst_addr = 0x%llx\n",
           chip_num, transfer_size, src_addr, dst_addr);


       ret = bm_dev_request(&handle, chip_num);
       if (ret != BM_SUCCESS || handle == NULL) {
         printf("bm_dev_request failed, ret = %d\n", ret);
         return -1;
       }
      if (test_gdma_d2d(handle, transfer_size, src_addr, dst_addr)) return -1;
      bm_dev_free(handle);
      return 0;
    }
  }
  else {
    bm_dev_getcount(&count);
    for (i = 0; i < count; i++) {
      ret = bm_dev_request(&handle, i);
      if (ret != BM_SUCCESS || handle == NULL) {
        printf("bm_dev_request failed, ret = %d\n", ret);
        return -1;
      }
      if (test_gdma_d2d(handle, 1024*1024*256, 0, 0)) return -1;
      bm_dev_free(handle);
    }
  }
  return 0;
}
