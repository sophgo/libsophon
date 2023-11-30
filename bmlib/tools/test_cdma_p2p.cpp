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
  for (idx = 0; idx < len; idx++) {
    if (p_exp[idx] != p_got[idx]) {
      printf("%s error at index 0x%x exp %x got %x\n",
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
    for (i = 0; i < loop; i ++)
        test_msleep(1000);
}
#endif

int test_cdma_ptop_transfer(unsigned long long src_addr, int src_num,
            unsigned long long dst_addr, int dst_num, int transfer_size)
{
  bm_handle_t handle_src = NULL;
  bm_handle_t handle_dst = NULL;
  bm_status_t ret = BM_SUCCESS;
  unsigned char *sys_send_buffer, *sys_recieve_buffer;
  int cmp_ret = 0;
  unsigned long consume_sys = 0;
  unsigned long consume_real = 0;
  unsigned long consume = 0;
  struct timespec tp;
  bm_device_mem_t dev_buffer_src, dev_buffer_dst;
  u64 src, dst;
  bm_profile_t profile_start, profile_end;
  struct timeval tv_start;
  struct timeval tv_end;
  struct timeval timediff;
  #ifdef __linux__
  clock_gettime(CLOCK_THREAD_CPUTIME_ID,&tp);
  #else
  clock_gettime(0, &tp);
  #endif
  srand(tp.tv_nsec);

  if (transfer_size == 0x0)
    transfer_size = 1024*1024*4;

  sys_send_buffer = (unsigned char *)malloc(transfer_size);
  sys_recieve_buffer = (unsigned char *)malloc(transfer_size);
  if (!sys_send_buffer || !sys_recieve_buffer) {
    printf("malloc buffer for test failed\n");
    return -1;
  }

  for (int i = 0; i < transfer_size; i++) {
    sys_send_buffer[i] = rand()%0xff;
    sys_recieve_buffer[i] = 0x0;
  }

  ret = bm_dev_request(&handle_src, src_num);
  if (ret != BM_SUCCESS || handle_src == NULL) {
    printf("bm_dev_request failed, ret = %d\n", ret);
    if (sys_send_buffer) free(sys_send_buffer);
    if (sys_recieve_buffer) free(sys_recieve_buffer);
    return -1;
  }

  if (src_addr == 0x0) {
    ret = bm_malloc_device_dword(handle_src, &dev_buffer_src, transfer_size/4);
    if (ret != BM_SUCCESS) {
      printf("malloc device memory size = %d failed, ret = %d\n", transfer_size, ret);
      return -1;
    }
  } else {
      dev_buffer_src = bm_mem_from_device(src_addr, transfer_size);
  }

  for (int i = 0; i < 10; i++) {
    bm_trace_enable(handle_src);
    gettimeofday(&tv_start, NULL);
    bm_get_profile(handle_src, &profile_start);
    ret = bm_memcpy_s2d(handle_src, dev_buffer_src, sys_send_buffer);
    if (ret != BM_SUCCESS) {
      printf("CDMA transfer from system to device failed, ret = %d\n", ret);
      return -1;
    }

    gettimeofday(&tv_end, NULL);
    timersub(&tv_end, &tv_start, &timediff);
    consume = timediff.tv_sec * 1000000 + timediff.tv_usec;
    consume_sys += consume;

    bm_get_profile(handle_src, &profile_end);
    consume = profile_end.cdma_out_time - profile_start.cdma_out_time;
    bm_trace_disable(handle_src);

    consume_real += consume;
  }
  consume = consume_sys / 10;
  if (consume > 0) {
    float bandwidth = (float)transfer_size / (1024.0*1024.0) / (consume / 1000000.0);
    // printf("S2D sys:Transfer size:0x%x byte. Cost time:%ld us, Write Bandwidth:%.2f MB/s\n",
    //         transfer_size,
    //         consume,
    //         bandwidth);
  }
  consume = consume_real / 10;
  if (consume > 0) {
    float bandwidth = (float)transfer_size / (1024.0*1024.0) / (consume / 1000000.0);
    // printf("S2D real:Transfer size:0x%x byte. Cost time:%ld us, Write Bandwidth:%.2f MB/s\n",
    //         transfer_size,
    //         consume,
    //         bandwidth);
  }

  consume_sys = 0x0;
  consume_real = 0x0;

  ret = bm_dev_request(&handle_dst, dst_num);

  if (dst_addr == 0x0) {
    ret = bm_malloc_device_dword(handle_dst, &dev_buffer_dst, transfer_size/4);
    if (ret != BM_SUCCESS) {
      printf("malloc device memory size = %d failed, ret = %d\n", transfer_size, ret);
      return -1;
    }
  } else {
      dev_buffer_dst = bm_mem_from_device(dst_addr, transfer_size);
  }

  for (int i = 0; i < 10; i++) {
    gettimeofday(&tv_start, NULL);
    ret = bm_memcpy_p2p(handle_src, dev_buffer_src, handle_dst, dev_buffer_dst);
    gettimeofday(&tv_end, NULL);
    timersub(&tv_end, &tv_start, &timediff);
    consume = timediff.tv_sec * 1000000 + timediff.tv_usec;
    consume_sys += consume;
  }

  consume = consume_sys / 10;
  if (consume > 0) {
    float bandwidth = (float)transfer_size / (1024.0*1024.0) / (consume / 1000000.0);
    printf("P2P sys:Transfer size:0x%x byte. Cost time:%ld us, Write Bandwidth:%.2f MB/s\n",
            transfer_size,
            consume,
            bandwidth);
  }

  for (int i = 0; i < 10; i++) {
    bm_trace_enable(handle_dst);
    gettimeofday(&tv_start, NULL);
    bm_get_profile(handle_dst, &profile_start);
    ret = bm_memcpy_d2s(handle_dst, sys_recieve_buffer, dev_buffer_dst);
    if (ret != BM_SUCCESS) {
      printf("CDMA transfer from system to device failed, ret = %d\n", ret);
      return -1;
    }

    gettimeofday(&tv_end, NULL);
    timersub(&tv_end, &tv_start, &timediff);
    consume = timediff.tv_sec * 1000000 + timediff.tv_usec;
    consume_sys += consume;
    bm_get_profile(handle_dst, &profile_end);
    consume = profile_end.cdma_in_time - profile_start.cdma_in_time;
    consume_real += consume;
    bm_trace_disable(handle_dst);
  }
  consume = consume_sys / 10;
  if (consume > 0) {
    float bandwidth = (float)transfer_size / (1024.0*1024.0) / (consume / 1000000.0);
    // printf("D2S sys:Transfer size:0x%x byte. Cost time:%ld us, Write Bandwidth:%.2f MB/s\n",
    //         transfer_size,
    //         consume,
    //         bandwidth);
  }
  consume = consume_real / 10;
  if (consume > 0) {
    float bandwidth = (float)transfer_size / (1024.0*1024.0) / (consume / 1000000.0);
    // printf("D2S real:Transfer size:0x%x byte. Cost time:%ld us, Write Bandwidth:%.2f MB/s\n",
    //         transfer_size,
    //         consume,
    //         bandwidth);
  }

  if (src_addr == 0x0) {
    src = bm_mem_get_device_addr(dev_buffer_src);
  } else {
    src = src_addr;
  }

  if (dst_addr == 0x0) {
    dst = bm_mem_get_device_addr(dev_buffer_dst);
  } else {
    dst = dst_addr;
  }

  cmp_ret = array_cmp_int(sys_send_buffer, sys_recieve_buffer, transfer_size, "cdma test");
  printf("src = %d, src_addr = 0x%llx, dst = %d, dst_addr = 0x%llx, cdma transfer test %s.\n", src_num, src, dst_num, dst, cmp_ret ? "Failed" : "Success");

  if (sys_send_buffer) free(sys_send_buffer);
  if (sys_recieve_buffer) free(sys_recieve_buffer);
  if (src_addr == 0x0) {
    bm_free_device(handle_src, dev_buffer_src);
  }

  if (dst_addr == 0x0) {
    bm_free_device(handle_dst, dev_buffer_dst);
  }
  bm_dev_free(handle_src);
  bm_dev_free(handle_dst);
  return cmp_ret;
}

void *test_cdma_ptop_transfer_mutithread(void *arg) {
  int src_num;
  int dst_num;
  unsigned long long src_addr = 0;
  unsigned long long dst_addr = 0;
  int transfer_size = 0;
  bm_handle_t handle_src = NULL;
  bm_handle_t handle_dst = NULL;
  bm_status_t ret = BM_SUCCESS;
  unsigned char *sys_send_buffer, *sys_recieve_buffer;
  int cmp_ret = 0;
  struct timespec tp;
  bm_device_mem_t dev_buffer_src, dev_buffer_dst;
  u64 src, dst;

  clock_gettime(CLOCK_THREAD_CPUTIME_ID,&tp);
  srand(tp.tv_nsec);

  src_num = rand() % 8;
  dst_num = rand() % 8;
  if (src_num == dst_num) {
      src_num = (dst_num + 1) % 8;
  }

  if (transfer_size == 0x0)
    transfer_size = 1024*1024*4;

  sys_send_buffer = (unsigned char *)malloc(transfer_size);
  sys_recieve_buffer = (unsigned char *)malloc(transfer_size);
  if (!sys_send_buffer || !sys_recieve_buffer) {
    printf("malloc buffer for test failed\n");
    return NULL;
  }

  for (int i = 0; i < transfer_size; i++) {
    sys_send_buffer[i] = rand()%0xff;
    sys_recieve_buffer[i] = 0x0;
  }

  ret = bm_dev_request(&handle_src, src_num);
  if (ret != BM_SUCCESS || handle_src == NULL) {
    printf("bm_dev_request failed, ret = %d\n", ret);
    if (sys_send_buffer) free(sys_send_buffer);
    if (sys_recieve_buffer) free(sys_recieve_buffer);
    return NULL;
  }

  if (src_addr == 0x0) {
    ret = bm_malloc_device_dword(handle_src, &dev_buffer_src, transfer_size/4);
    if (ret != BM_SUCCESS) {
      printf("malloc device memory size = %d failed, ret = %d\n", transfer_size, ret);
      return NULL;
    }
  } else {
      dev_buffer_src = bm_mem_from_device(src_addr, transfer_size);
  }

  ret = bm_memcpy_s2d(handle_src, dev_buffer_src, sys_send_buffer);
  if (ret != BM_SUCCESS) {
    printf("CDMA transfer from system to device failed, ret = %d\n", ret);
    return NULL;
  }

  ret = bm_dev_request(&handle_dst, dst_num);

  if (dst_addr == 0x0) {
    ret = bm_malloc_device_dword(handle_dst, &dev_buffer_dst, transfer_size/4);
    if (ret != BM_SUCCESS) {
      printf("malloc device memory size = %d failed, ret = %d\n", transfer_size, ret);
      return NULL;
    }
  } else {
      dev_buffer_dst = bm_mem_from_device(dst_addr, transfer_size);
  }

  ret = bm_memcpy_p2p(handle_src, dev_buffer_src, handle_dst, dev_buffer_dst);
  if (ret != BM_SUCCESS) {
    printf("CDMA transfer p2p failed, ret = %d\n", ret);
    return NULL;
  }

  ret = bm_memcpy_d2s(handle_dst, sys_recieve_buffer, dev_buffer_dst);
  if (ret != BM_SUCCESS) {
    printf("CDMA transfer from system to device failed, ret = %d\n", ret);
    return NULL;
  }

  if (src_addr == 0x0) {
    src = bm_mem_get_device_addr(dev_buffer_src);
  } else {
    src = src_addr;
  }

  if (dst_addr == 0x0) {
    dst = bm_mem_get_device_addr(dev_buffer_dst);
  } else {
    dst = dst_addr;
  }

  cmp_ret = array_cmp_int(sys_send_buffer, sys_recieve_buffer, transfer_size, "cdma test");
  printf("src = %d, src_addr = 0x%llx, dst = %d, dst_addr = 0x%llx, size: 0x%x, cdma transfer test %s.\n", src_num, src, dst_num, dst, transfer_size, cmp_ret ? "Failed" : "Success");

  if (sys_send_buffer) free(sys_send_buffer);
  if (sys_recieve_buffer) free(sys_recieve_buffer);
  if (src_addr == 0x0) {
    bm_free_device(handle_src, dev_buffer_src);
  }

  if (dst_addr == 0x0) {
    bm_free_device(handle_dst, dev_buffer_dst);
  }
  bm_dev_free(handle_src);
  bm_dev_free(handle_dst);
  return NULL;
}

#define THREAD_NUM 64

int test_cmda_perf_mutithread(int thread_num)
{
  pthread_t threads[THREAD_NUM];

  int i = 0x0;
  int ret = 0x0;

  if (thread_num > THREAD_NUM) {
    printf("thread num = %d is too much\n", thread_num);
    return -1;
  }

  for (i = 0; i < thread_num; i++) {

    ret = pthread_create(&threads[i], NULL, test_cdma_ptop_transfer_mutithread, NULL);
    if (ret < 0) {
      printf("pthread_create %d error: error_code = %d\n", i, ret);
      return -1;
    }
  }

  for (i = 0; i < thread_num; i++) {
    ret = pthread_join(threads[i], NULL);
    if (ret < 0) {
      printf("pthread_join %d error: error_code = %d\n", i, ret);
      return -1;
    }
  }

  return 0;
}

int main(int argc, char *argv[])
{
  unsigned long long src_addr;
  int src_num;
  unsigned long long dst_addr;
  int dst_num;
  int transfer_size;
  int ret;
  int thread_num;
  int test_num;
  int i;

  if (argc == 1) {
    ret = test_cdma_ptop_transfer(0x130000000, 0, 0x140000000, 1, 0x1000);
  } else if (strcmp("muti", argv[1]) == 0) {
    thread_num = atoi(argv[2]);
    test_num = atoi(argv[3]);
    printf("p2p muti test, thread_num: %d, test_num: %d\n", thread_num, test_num);
    for (i = 1; i <= test_num; i++) {
      printf("test num: %d\n", i);
      test_cmda_perf_mutithread(thread_num);
    }
  } else if (argc == 6) {
    src_addr = strtoll(argv[1], NULL, 16);
    src_num = strtoll(argv[2], NULL, 16);
    dst_addr = strtoll(argv[3], NULL, 16);
    dst_num = strtoll(argv[4], NULL, 16);
    transfer_size = strtoll(argv[5], NULL, 16);
    printf("test_cdma_p2p src_addr = 0x%llx, src_num = 0x%d, dst_addr = 0x%llx, dst_num = %d, transfer_size = 0x%x\n",\
           src_addr, src_num, dst_addr, dst_num, transfer_size);

    ret = test_cdma_ptop_transfer(src_addr, src_num, dst_addr, dst_num, transfer_size);
  } else {
    printf("invalid arg\n");
    printf("example test_cdma_p2p src_addr src_num dst_addr dst_num  transfer_size\n");
    printf("like test_cdma_p2p 0x130000000 0 0x140000000 1 0x1000\n");
  }
  return ret;
}
