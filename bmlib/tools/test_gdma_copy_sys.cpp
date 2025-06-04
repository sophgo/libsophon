// Copyright 2025 sophgon Inc. All rights reserved.
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "bmlib_ioctl.h"
#include "bmlib_runtime.h"
#include "ion.h"

typedef unsigned int u_int;
static uint32_t gdma_pio_seq;
pthread_mutex_t gdma_pio_seq_lock = PTHREAD_MUTEX_INITIALIZER;

struct bm_tdma_wait_arg {
  unsigned int seq_no;
  int ret;
};
typedef enum {
  DT_INT8 = (0 << 1) | 1,
  DT_UINT8 = (0 << 1) | 0,
  DT_INT16 = (3 << 1) | 1,
  DT_UINT16 = (3 << 1) | 0,
  DT_FP16 = (1 << 1) | 1,
  DT_BFP16 = (5 << 1) | 1,
  DT_INT32 = (4 << 1) | 1,
  DT_UINT32 = (4 << 1) | 0,
  DT_FP32 = (2 << 1) | 1,
  DT_INT4 = (6 << 1) | 1,
  DT_UINT4 = (6 << 1) | 0,
  DT_FP8E5M2 = (0 << 5) | (7 << 1) | 1,
  DT_FP8E4M3 = (1 << 5) | (7 << 1) | 1,
  DT_FP20 = (8 << 1) | 1,
  DT_TF32 = (9 << 1) | 1,
} data_type_t;



uint32_t platform_sys_tdmacopy_1d(uint64_t u64PhyDst, uint64_t u64PhySrc,
                                  uint32_t u32Len) {
  int fd = open("/dev/bm-tpu0", O_RDWR);

  if (fd < 0) {
    perror("Failed to open device");
    return -1;
  }

  // construct api_mem_param
  sg_api_1d_memcpy_t api_mem_param;
  bm_tdma_wait_arg wait_arg;

  pthread_mutex_lock(&gdma_pio_seq_lock);
  gdma_pio_seq++;
  // api_mem_param.seq_no = gdma_pio_seq;
  pthread_mutex_unlock(&gdma_pio_seq_lock);

  api_mem_param.w_bytes = u32Len;
  api_mem_param.src_global_offset = u64PhySrc;
  api_mem_param.dst_global_offset = u64PhyDst;

  printf("api_mem_param: w_bytes: %d\n", api_mem_param.w_bytes);
  printf("api_mem_param: src_global_offset: 0x%llx\n", api_mem_param.src_global_offset);
  printf("api_mem_param: dst_global_offset: 0x%llx\n", api_mem_param.dst_global_offset);
  // printf("api_mem_param: seq_no: %d\n", api_mem_param.seq_no);

  // call sg_api_1d_memcpy
  int ret = ioctl(fd, BMDEV_MEMCPY, &api_mem_param);
  if (ret != 0) {
    printf("sg_api_1d_memcpy failed!\n");
    return -1;
  }
  printf("sg_api_1d_memcpy success! wait for sync......\n");
  // wait_arg.seq_no = api_mem_param.seq_no;

  ret = ioctl(fd, BMDEV_MEMCPY_ASYNC, &wait_arg);
  if (ret != 0) {
    printf("sg_api_1d_memcpy failed!\n");
    return -1;
  }

  return 0;
}

static void *map_physical_memory(uint64_t phys_addr, size_t length) {
  int fd;
  void *mapped_addr;

  fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (fd < 0) {
    perror("open /dev/mem");
    return NULL;
  }

  printf("phys_addr: 0x%lx, length: 0x%lx\n", phys_addr, length);
  mapped_addr =
      mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, phys_addr);
  if (mapped_addr == MAP_FAILED) {
    perror("mmap");
    close(fd);
    return NULL;
  }

  close(fd);
  return mapped_addr;
}

static int test_gdma(void) {
  int fd = open("/dev/ion", O_RDWR);

  if (fd < 0) {
    printf("open /dev/ion fail\n");
    return -1;
  }
  // allocate memory
  int u32Len = 100;
  uint32_t u32Flags = 0;
  uint32_t u32HeapID = 1;
  uint32_t ret;
  struct ion_allocation_data alloc_data_src;
  struct ion_allocation_data alloc_data_dst;

  memset(&alloc_data_src, 0, sizeof(alloc_data_src));
  alloc_data_src.len = u32Len;
  alloc_data_src.heap_id_mask = u32HeapID;
  alloc_data_src.flags = u32Flags;

  memset(&alloc_data_dst, 0, sizeof(alloc_data_dst));
  alloc_data_dst.len = u32Len;
  alloc_data_dst.heap_id_mask = u32HeapID;
  alloc_data_src.flags = u32Flags;

  int ret_ = ioctl(fd, ION_IOC_ALLOC, &alloc_data_src);

  if (ret_ < 0) {
    printf("ion alloc fail\n");
    return -1;
  }
  // fill data value 5 to alloc_data_src.paddr
  void *virtual_addr = map_physical_memory(alloc_data_src.paddr, u32Len);

  for (int i = 0; i < u32Len; i++) {
    *reinterpret_cast<uint8_t *>(static_cast<uint8_t *>(virtual_addr) + i) = 5;
  }
  void *virtual_addr_ = map_physical_memory(alloc_data_src.paddr, u32Len);

  for (int i = 0; i < 100; i++) {
    printf("%u ", *(reinterpret_cast<uint8_t *>(virtual_addr) + i));
  }
  printf("\n");

  ret_ = ioctl(fd, ION_IOC_ALLOC, &alloc_data_dst);
  if (ret_ < 0) {
    printf("ion alloc fail\n");
    return -1;
  }

  ret = platform_sys_tdmacopy_1d(alloc_data_dst.paddr, alloc_data_src.paddr,
                                 u32Len);
  if (ret != 0) {
    printf("sys gdma test fail\n");
  }

  void *virtual_addr2 = map_physical_memory(alloc_data_dst.paddr, u32Len);
  // printf all data uint8_t
  for (int i = 0; i < 100; i++) {
    printf("%u ", *(reinterpret_cast<uint8_t *>(virtual_addr2) + i));
  }

  close(fd);

  return 0;
}

int main(int argc, char *argv[]) {
  test_gdma();
  return 0;
}