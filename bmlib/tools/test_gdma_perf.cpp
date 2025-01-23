#include "api.h"
#include "bmlib_internal.h"
#include "bmlib_memory.h"
#include "bmlib_runtime.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#define MAX_CHIP_NUM 256

#define BMDEV_SET_IOMAP_TPYE _IOWR('p', 0x99, u_int)
#define DEVICE_FILE "/dev/bm-tpu0"
#define TPU_GDMA_SIZE 0x10000 // gdma size
#define TPU_SYS_SIZE 0x30000  // sys size
#define TPU_REG_SIZE 0x10000  // reg size
#define TPU_SMEM_SIZE 0x1000  // smem size aligned to 4096
#define TPU_LMEM_SIZE 0x80000 // lmem size

static void *mmap_gdma;
static void *mmap_sys;
static void *mmap_reg;
static void *mmap_smem;
static void *mmap_lmem;

char g_library_file[100];

#define A53LITE_RUNTIME_LOG_TAG "a53lite_runtime"

jmp_buf jmp_env;

void *map_physical_memory(uint64_t phys_addr, size_t length) {
  int fd;
  void *mapped_addr;

  fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (fd < 0) {
    perror("open /dev/mem");
    return MAP_FAILED;
  }

  printf("phys_addr: 0x%lx, length: 0x%lx\n", phys_addr, length);
  mapped_addr =
      mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, phys_addr);
  if (mapped_addr == MAP_FAILED) {
    perror("mmap");
    close(fd);
    return MAP_FAILED;
  }

  close(fd);
  return mapped_addr;
}

size_t virtual_to_physical(size_t addr) {
  int fd = open("/proc/self/pagemap", O_RDONLY);
  if (fd < 0) {
    printf("open '/proc/self/pagemap' failed!\n");
    return 0;
  }
  size_t pagesize = getpagesize();
  size_t offset = (addr / pagesize) * sizeof(uint64_t);
  if (lseek(fd, offset, SEEK_SET) < 0) {
    printf("lseek() failed!\n");
    close(fd);
    return 0;
  }
  uint64_t info;
  if (read(fd, &info, sizeof(uint64_t)) != sizeof(uint64_t)) {
    printf("read() failed!\n");
    close(fd);
    return 0;
  }
  if ((info & (((uint64_t)1) << 63)) == 0) {
    printf("page is not present!\n");
    close(fd);
    return 0;
  }
  size_t frame = info & ((((uint64_t)1) << 55) - 1);
  size_t phy = frame * pagesize + addr % pagesize;
  close(fd);
  return phy;
}

static void *mmap_paddr_to_vaddr(int type, size_t size, int fd) {
  int ret = ioctl(fd, BMDEV_SET_IOMAP_TPYE, type);
  if (ret != 0) {
    printf("ioctl failed\n");
  }

  void *mapped_memory =
      mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped_memory == MAP_FAILED) {
    perror("mmap failed");
    close(fd);
    return NULL;
  }
  // printf("mapped_memory: %p\n", mapped_memory);

  // printf("The value of the register is: %x\n", *((unsigned char
  // *)mapped_memory));

  // *((int *)mapped_memory) = 0xff;
  // printf("-------------------\n");
  // printf("The value of the register is: %x\n", *((unsigned char
  // *)mapped_memory));

  // if (munmap(mapped_memory, size) < 0) {
  // 		perror("munmap failed");
  // }

  // mapped_memory_list.emplace_back(mapped_memory, size);
  return mapped_memory;
}

static int mmap_tpu_sys() {
  // void *mmap;
  int result = 0;

  int fd = open(DEVICE_FILE, O_RDWR);
  if (fd < 0) {
    perror("Failed to open device");
    return -1;
  }

  mmap_gdma = mmap_paddr_to_vaddr(0, TPU_GDMA_SIZE, fd);
  if (mmap == NULL) {
    printf("GDMA mmap failed\n");
    return -1;
  }

  mmap_sys = mmap_paddr_to_vaddr(1, TPU_SYS_SIZE, fd);
  if (mmap == NULL) {
    printf("SYS mmap failed\n");
    return -1;
  }

  mmap_reg = mmap_paddr_to_vaddr(2, TPU_REG_SIZE, fd);
  if (mmap == NULL) {
    printf("REG mmap failed\n");
    return -1;
  }

  mmap_smem = mmap_paddr_to_vaddr(3, TPU_SMEM_SIZE, fd);
  if (mmap == NULL) {
    printf("SMEM mmap failed\n");
    return -1;
  }

  mmap_lmem = mmap_paddr_to_vaddr(4, TPU_LMEM_SIZE, fd);
  if (mmap == NULL) {
    printf("LMEM mmap failed\n");
    return -1;
  }

  close(fd);
  return 0;
}

static int mmap_addr_to_firmwarecore(void *handle) {
  // void *mmap;
  int result = 0;
  dlerror();
  int (*CallPhysicalToVirtual)(void *, int) =
      (int (*)(void *, int))dlsym(handle, "PhysicalToVirtual");
  const char *dlsym_error = dlerror();
  if (dlsym_error) {
    fprintf(stderr, "Cannot load symbol 'CallPhysicalToVirtual': %s\n",
            dlsym_error);
    dlclose(handle);
    return -1;
  }

  result = CallPhysicalToVirtual(mmap_gdma, 0);
  if (result != 0) {
    printf("CallPhysicalToVirtual failed\n");
    return -1;
  }

  result = CallPhysicalToVirtual(mmap_sys, 1);
  if (result != 0) {
    printf("CallPhysicalToVirtual failed\n");
    return -1;
  }

  result = CallPhysicalToVirtual(mmap_reg, 2);
  if (result != 0) {
    printf("CallPhysicalToVirtual failed\n");
    return -1;
  }

  result = CallPhysicalToVirtual(mmap_smem, 3);
  if (result != 0) {
    printf("CallPhysicalToVirtual failed\n");
    return -1;
  }

  result = CallPhysicalToVirtual(mmap_lmem, 4);
  if (result != 0) {
    printf("CallPhysicalToVirtual failed\n");
    return -1;
  }

  return 0;
}

static int load_table_to_firmwarecore(void *handle) {
  dlerror();
  void (*CallFunc)() = (void (*)())dlsym(handle, "load_lookup_tables");
  const char *dlsym_error = dlerror();
  if (dlsym_error) {
    fprintf(stderr, "Cannot load symbol 'CallPhysicalToVirtual': %s\n",
            dlsym_error);
    dlclose(handle);
    return -1;
  }
  try {
    CallFunc();
  } catch (const std::exception &e) {
    std::cout << "load_lookup_tables failed: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}

typedef struct {
  bm_handle_t handle;
  bm_device_mem_t *mem_need_free;
  size_t mem_need_free_size;
  unsigned char **ptr_need_free;
  size_t ptr_need_free_size;
  struct {
    bm_device_mem_t *keys;
    bm_device_mem_t *values;
    size_t size;
  } post_copy_map;
} DeviceMemAllocator;

void init_allocator(DeviceMemAllocator *allocator, bm_handle_t handle) {
  allocator->handle = handle;
  allocator->mem_need_free = NULL;
  allocator->mem_need_free_size = 0;
  allocator->ptr_need_free = NULL;
  allocator->ptr_need_free_size = 0;
  allocator->post_copy_map.keys = NULL;
  allocator->post_copy_map.values = NULL;
  allocator->post_copy_map.size = 0;
}

void destroy_mem(DeviceMemAllocator *allocator) {
  for (size_t i = 0; i < allocator->ptr_need_free_size; ++i) {
    free(allocator->ptr_need_free[i]);
  }
  free(allocator->ptr_need_free);
  allocator->ptr_need_free = NULL;
  allocator->ptr_need_free_size = 0;

  for (size_t i = 0; i < allocator->mem_need_free_size; ++i) {
    bm_free_device(allocator->handle, allocator->mem_need_free[i]);
  }
  free(allocator->mem_need_free);
  allocator->mem_need_free = NULL;
  allocator->mem_need_free_size = 0;

  free(allocator->post_copy_map.keys);
  free(allocator->post_copy_map.values);
  allocator->post_copy_map.keys = NULL;
  allocator->post_copy_map.values = NULL;
  allocator->post_copy_map.size = 0;
}

bm_device_mem_t alloc_on_device(DeviceMemAllocator *allocator, size_t elem_size,
                                void (*init_func)(float *, size_t)) {
  bm_device_mem_t mem;
  size_t byte_size = elem_size;
  int ret = bm_malloc_device_byte(allocator->handle, &mem, byte_size);
  if (ret != BM_SUCCESS) {
    destroy_mem(allocator);
    longjmp(jmp_env, ret);
  }
  if (init_func) {
    float *ptr = (float *)malloc(byte_size);
    if (!ptr) {
      destroy_mem(allocator);
      longjmp(jmp_env, BM_ERR_NOMEM);
    }
    init_func(ptr, 1);
    // ret = bm_memcpy_s2d_partial(allocator->handle, mem, ptr, byte_size);
    free(ptr);
    if (ret != BM_SUCCESS) {
      destroy_mem(allocator);
      longjmp(jmp_env, ret);
    }
  }
  allocator->mem_need_free = (bm_device_mem_t *)realloc(
      allocator->mem_need_free,
      (allocator->mem_need_free_size + 1) * sizeof(bm_device_mem_t));
  allocator->mem_need_free[allocator->mem_need_free_size++] = mem;
  return mem;
}

void dealloc(DeviceMemAllocator *allocator, const bm_device_mem_t *mem) {
  if (bm_mem_get_type(*mem) == BM_MEM_TYPE_SYSTEM) {
    for (size_t i = 0; i < allocator->mem_need_free_size; ++i) {
      if (allocator->mem_need_free[i].u.device.device_addr ==
          mem->u.device.device_addr) {
        bm_free_device(allocator->handle, *mem);
        allocator->mem_need_free[i] =
            allocator->mem_need_free[--allocator->mem_need_free_size];
        return;
      }
    }
    destroy_mem(allocator);
    longjmp(jmp_env, BM_ERR_FAILURE);
  } else {
    unsigned char *ptr = (unsigned char *)bm_mem_get_system_addr(*mem);
    for (size_t i = 0; i < allocator->ptr_need_free_size; ++i) {
      if (allocator->ptr_need_free[i] == ptr) {
        free(ptr);
        allocator->ptr_need_free[i] =
            allocator->ptr_need_free[--allocator->ptr_need_free_size];
        return;
      }
    }
    destroy_mem(allocator);
    longjmp(jmp_env, BM_ERR_FAILURE);
  }
}

bm_device_mem_t alloc_on_system(DeviceMemAllocator *allocator, size_t elem_size,
                                size_t elem_count,
                                void (*init_func)(float *, size_t)) {
  size_t byte_size = elem_size * elem_count;
  unsigned char *ptr = (unsigned char *)malloc(byte_size);
  if (!ptr) {
    destroy_mem(allocator);
    longjmp(jmp_env, BM_ERR_NOMEM);
  }
  if (init_func) {
    init_func((float *)ptr, elem_count);
  }
  allocator->ptr_need_free = (unsigned char **)realloc(
      allocator->ptr_need_free,
      (allocator->ptr_need_free_size + 1) * sizeof(unsigned char *));
  allocator->ptr_need_free[allocator->ptr_need_free_size++] = ptr;
  bm_device_mem_t mem;
  bm_mem_set_system_addr(&mem, ptr);
  return mem;
}

bm_device_mem_t map_input_to_device(DeviceMemAllocator *allocator,
                                    const bm_device_mem_t *raw_mem,
                                    size_t elem_size) {
  if (bm_mem_get_type(*raw_mem) == BM_MEM_TYPE_SYSTEM) {
    bm_device_mem_t new_mem = alloc_on_device(allocator, elem_size, NULL);

    bm_memcpy_s2d(allocator->handle, new_mem, bm_mem_get_system_addr(*raw_mem));
    // int ret=bm_mem_write_data_to_ion(allocator->handle, &new_mem,
    // bm_mem_get_system_addr(*raw_mem), elem_size* sizeof(uint32_t));
    // // int ret=bm_vAddr_to_pAddr(allocator->handle, (void
    // *)bm_mem_get_system_addr(*raw_mem),&new_mem.u.device.device_addr); if
    // (ret != BM_SUCCESS) { 	destroy_mem(allocator);
    //     longjmp(jmp_env, ret);
    // }
    // else{
    // 	printf( "bm_mem_write_data_to_ion success!
    // %lu\n",new_mem.u.device.device_addr);
    // }

    void *virtual_addr =
        map_physical_memory(bm_mem_get_device_addr(new_mem), elem_size);
    // printf all data uint8_t
    for (int i = 0; i < elem_size; i++) {
      printf("%u ", *(uint8_t *)(virtual_addr + i));
    }
    printf("\n");

    return new_mem;
  }
  return *raw_mem;
}

bm_device_mem_t map_output_to_device(DeviceMemAllocator *allocator,
                                     const bm_device_mem_t *raw_mem,
                                     size_t elem_size, int is_inplace) {
  if (bm_mem_get_type(*raw_mem) == BM_MEM_TYPE_SYSTEM) {
    bm_device_mem_t new_mem = alloc_on_device(allocator, elem_size, NULL);
    allocator->post_copy_map.keys = (bm_device_mem_t *)realloc(
        allocator->post_copy_map.keys,
        (allocator->post_copy_map.size + 1) * sizeof(bm_device_mem_t));
    allocator->post_copy_map.values = (bm_device_mem_t *)realloc(
        allocator->post_copy_map.values,
        (allocator->post_copy_map.size + 1) * sizeof(bm_device_mem_t));
    allocator->post_copy_map.keys[allocator->post_copy_map.size] = *raw_mem;
    allocator->post_copy_map.values[allocator->post_copy_map.size++] = new_mem;
    if (is_inplace) {
      int ret = bm_memcpy_s2d(allocator->handle, new_mem,
                              bm_mem_get_system_addr(*raw_mem));
      if (ret != BM_SUCCESS) {
        destroy_mem(allocator);
        longjmp(jmp_env, ret);
      }
    }
    return new_mem;
  }
  return *raw_mem;
}

unsigned long long map_input_to_device_addr(DeviceMemAllocator *allocator,
                                            void *raw_ptr, size_t elem_size) {
  bm_device_mem_t raw_mem = bm_mem_from_system(raw_ptr);
  bm_device_mem_t mem = map_input_to_device(allocator, &raw_mem, elem_size);
  return bm_mem_get_device_addr(mem);
}

unsigned long long map_output_to_device_addr(DeviceMemAllocator *allocator,
                                             const bm_device_mem_t *raw_mem,
                                             size_t elem_size, int is_inplace) {
  bm_device_mem_t mem =
      map_output_to_device(allocator, raw_mem, elem_size, is_inplace);
  return bm_mem_get_device_addr(mem);
}

unsigned long long map_buffer_to_device_addr(DeviceMemAllocator *allocator,
                                             size_t buffer_size) {
  if (buffer_size == 0)
    return (unsigned long long)-1;
  bm_device_mem_t mem = alloc_on_device(allocator, buffer_size, NULL);
  return bm_mem_get_device_addr(mem);
}

void flush_output(DeviceMemAllocator *allocator) {
  for (size_t i = 0; i < allocator->post_copy_map.size; ++i) {
    bm_memcpy_d2s(allocator->handle,
                  bm_mem_get_system_addr(allocator->post_copy_map.keys[i]),
                  allocator->post_copy_map.values[i]);
  }
  free(allocator->post_copy_map.keys);
  free(allocator->post_copy_map.values);
  allocator->post_copy_map.keys = NULL;
  allocator->post_copy_map.values = NULL;
  allocator->post_copy_map.size = 0;
}

void destroy_allocator(DeviceMemAllocator *allocator) {
  // flush_output(allocator);
  destroy_mem(allocator);
}

void init_input(uint32_t *ptr, size_t len) {
  for (size_t i = 0; i < len; i++) {
    ptr[i] = 1.0f;
  }
}

#define BM_ERR_PARAM -1
#define BMLIB_MEMORY_LOG_TAG "MemoryLog"

void bmlib_log(const char *tag, const char *level, const char *msg) {
  printf("[%s] %s: %s\n", tag, level, msg);
}

typedef enum {
  BM_DTYPE_INT,
  BM_DTYPE_FLOAT,
  BM_DTYPE_UINT32,
} bm_dtype_t;

int bm_mem_write_data_to_sys_ion(bm_handle_t handle, bm_device_mem_t *dmem,
                                 bm_dtype_t dtype, size_t size,
                                 const char *dump_filename) {

  void *mapped_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                             dmem->u.device.dmabuf_fd, 0);
  if (mapped_memory == MAP_FAILED) {
    bmlib_log(BMLIB_MEMORY_LOG_TAG, "ERROR", "mmap failed");
    return BM_ERR_PARAM;
  }

  FILE *file = fopen(dump_filename, "w");
  if (!file) {
    bmlib_log(BMLIB_MEMORY_LOG_TAG, "ERROR",
              "Failed to open file for dumping data");
    munmap(mapped_memory, size);
    return BM_ERR_PARAM;
  }

  size_t count = size;

  size_t print_count = count < 20 ? count : 20;

  switch (dtype) {
  case BM_DTYPE_INT: {
    int *data = (int *)mapped_memory;
    for (size_t i = 0; i < print_count; i++) {
      printf("Data[%zu]: %d\n", i, data[i]);
    }

    for (size_t i = 0; i < count; i++) {
      fprintf(file, "Data[%zu]: %d\n", i, data[i]);
    }
    break;
  }
  case BM_DTYPE_FLOAT: {
    float *data = (float *)mapped_memory;
    for (size_t i = 0; i < print_count; i++) {
      printf("Data[%zu]: %f\n", i, data[i]);
    }

    for (size_t i = 0; i < count; i++) {
      fprintf(file, "Data[%zu]: %f\n", i, data[i]);
    }
    break;
  }
  case BM_DTYPE_UINT32: {
    uint32_t *data = (uint32_t *)mapped_memory;
    for (size_t i = 0; i < print_count; i++) {
      printf("Data[%zu]: %u\n", i, data[i]);
    }

    for (size_t i = 0; i < count; i++) {
      fprintf(file, "Data[%zu]: %u\n", i, data[i]);
    }
    break;
  }
  default:
    bmlib_log(BMLIB_MEMORY_LOG_TAG, "ERROR", "Unsupported data type");
    fclose(file);
    munmap(mapped_memory, size);
    return BM_ERR_PARAM;
  }

  fclose(file);
  munmap(mapped_memory, size);
  return 0;
}

typedef struct sg_api_memcpy {
  unsigned long long src_global_offset;
  unsigned long long dst_global_offset;
  int input_n;
  int src_nstride;
  int dst_nstride;
  int count;
} __attribute__((packed)) sg_api_memcpy_t;

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

typedef struct sg_api_1d_memcpy {
  unsigned long long src_global_offset;
  unsigned long long dst_global_offset;
  int w_bytes;
  int stride_bytes_src;
  int stride_bytes_dst;
  data_type_t data_type;
} __attribute__((packed)) sg_api_1d_memcpy_t;

typedef struct sg_api_2d_memcpy {
  unsigned long long src_global_offset;
  unsigned long long dst_global_offset;
  int w_bytes;
  int h;
  int stride_bytes_src;
  int stride_bytes_dst;
  data_type_t data_type;
} __attribute__((packed)) sg_api_2d_memcpy_t;

bm_status_t test_sg_api_1d_memcpy(void *lib_handle, bm_handle_t handle,
                                  DeviceMemAllocator allocator) {
  int (*f_ptr)(void *, unsigned int);
  size_t shape_cnt = 100;
  f_ptr = (int (*)(void *, unsigned int))dlsym(lib_handle, "sg_api_1d_memcpy");
  if (!f_ptr) {
    fprintf(stderr, "Failed to load sg_api_1d_memcpy\n");
    return BM_ERR_FAILURE;
  }

  sg_api_1d_memcpy api_mem_param;
  api_mem_param.data_type = DT_INT8;
  api_mem_param.w_bytes = shape_cnt;
  size_t sg_dtype_len = sizeof(uint8_t);

  // init src data
  uint8_t *input = (uint8_t *)malloc(shape_cnt * sg_dtype_len);
  if (!input) {
    fprintf(stderr, "Failed to allocate memory for input\n");
    return BM_ERR_FAILURE;
  }
  // init src data value
  for (size_t i = 0; i < shape_cnt * sg_dtype_len; i++) {
    input[i] = 3;
  }

  // init dst data
  uint32_t *output = (uint32_t *)malloc(shape_cnt * sg_dtype_len);
  if (!output) {
    fprintf(stderr, "Failed to allocate memory for output\n");
    free(input);
    bm_dev_free(handle);
    return BM_ERR_FAILURE;
  }

  bm_device_mem_t input_mem = bm_mem_from_system(input);
  bm_device_mem_t input_device_mem =
      map_input_to_device(&allocator, &input_mem, shape_cnt * sg_dtype_len);
  bm_device_mem_t output_mem = bm_mem_from_system(output);
  bm_device_mem_t output_device_mem = map_output_to_device(
      &allocator, &output_mem, shape_cnt * sg_dtype_len, 0);
  api_mem_param.src_global_offset = bm_mem_get_device_addr(input_device_mem);
  api_mem_param.dst_global_offset = bm_mem_get_device_addr(output_device_mem);

  printf("=====sg_api_1d_memcpy start=====\n");
  int ret = f_ptr(&api_mem_param, sizeof(api_mem_param));
  if (ret != 0) {
    printf("sg_api_1d_memcpy failed!\n");
  }
  printf("=====sg_api_1d_memcpy over output data=====\n");
  void *virtual_addr = map_physical_memory(api_mem_param.dst_global_offset,
                                           shape_cnt * sg_dtype_len);
  // printf all data uint8_t
  for (int i = 0; i < shape_cnt * sg_dtype_len; i++) {
    printf("%u ", *(uint8_t *)(virtual_addr + i));
  }
  printf("\n");

  free(input);
  free(output);

  return BM_SUCCESS;
}

bm_status_t test_sg_api_2d_memcpy(void *lib_handle, bm_handle_t handle,
                                  DeviceMemAllocator allocator) {
  int (*f_ptr)(void *, unsigned int);
  size_t shape_cnt = 100 * 2;
  f_ptr = (int (*)(void *, unsigned int))dlsym(lib_handle, "sg_api_2d_memcpy");

  sg_api_2d_memcpy api_mem_param;
  api_mem_param.data_type = DT_INT8;
  api_mem_param.w_bytes = 1;
  api_mem_param.h = 50;
  size_t sg_dtype_len = sizeof(uint8_t);

  // init src data
  uint8_t *input = (uint8_t *)malloc(shape_cnt * sg_dtype_len);
  if (!input) {
    fprintf(stderr, "Failed to allocate memory for input\n");
    return BM_ERR_FAILURE;
  }
  // init src data value
  for (size_t i = 0; i < shape_cnt * sg_dtype_len; i++) {
    input[i] = 4;
  }

  // init dst data
  uint32_t *output = (uint32_t *)malloc(shape_cnt * sg_dtype_len);
  if (!output) {
    fprintf(stderr, "Failed to allocate memory for output\n");
    free(input);
    return BM_ERR_FAILURE;
  }

  bm_device_mem_t input_mem = bm_mem_from_system(input);
  bm_device_mem_t input_device_mem =
      map_input_to_device(&allocator, &input_mem, shape_cnt * sg_dtype_len);
  bm_device_mem_t output_mem = bm_mem_from_system(output);
  bm_device_mem_t output_device_mem = map_output_to_device(
      &allocator, &output_mem, shape_cnt * sg_dtype_len, 0);
  api_mem_param.src_global_offset = bm_mem_get_device_addr(input_device_mem);
  api_mem_param.dst_global_offset =
      bm_mem_get_device_addr(output_device_mem) + 1;
  api_mem_param.stride_bytes_src = 2;
  api_mem_param.stride_bytes_dst = 2;

  printf("=====sg_api_2d_memcpy start=====\n");
  int ret = f_ptr(&api_mem_param, sizeof(api_mem_param));
  if (ret != 0) {
    printf("sg_api_1d_memcpy failed!\n");
  }

  printf("=====sg_api_2d_memcpy over output data=====\n");
  void *virtual_addr = map_physical_memory(
      bm_mem_get_device_addr(output_device_mem), shape_cnt * sg_dtype_len);
  // printf all data uint8_t
  for (int i = 0; i < shape_cnt * sg_dtype_len; i++) {
    printf("%u ", *(uint8_t *)(virtual_addr + i));
  }
  printf("\n");

  free(input);
  free(output);

  return BM_SUCCESS;
}

int main(int argc, char *argv[]) {
  int dev_num;
  int rel_num;
  int arg[MAX_CHIP_NUM];
  tpu_kernel_module_t bm_module;
  bm_handle_t handle;
  DeviceMemAllocator allocator;
  tpu_kernel_function_t f_id;
  bm_status_t ret = BM_SUCCESS;

  int i = 0;
  if (argc != 3) {
    printf("please input param just like: a53lite_load_lib 0 test.so\n");
    return -1;
  }

  if (BM_SUCCESS != bm_dev_getcount(&dev_num)) {
    printf("no sophon device found!\n");
    return -1;
  }

  if (dev_num > MAX_CHIP_NUM) {
    printf("too many device number %d!\n", dev_num);
    return -1;
  }

  rel_num = atoi(argv[1]);
  ret = bm_dev_request(&handle, rel_num);
  if ((ret != BM_SUCCESS) || (handle == NULL)) {
    printf("bm_dev_request error, ret = %d\r\n", ret);
    return BM_ERR_FAILURE;
  }
  init_allocator(&allocator, handle);

  // open libfirmware_core_1.so
  void *lib_handle = dlopen(argv[2], RTLD_NOW);

  int ret_1 = mmap_tpu_sys();
  if (ret_1 != 0) {
    std::cout << "mmap_tpu_sys fail" << std::endl;
  }

  if (mmap_addr_to_firmwarecore(lib_handle)) {
    std::cout << "mmap_addr_to_firmwarecore failed" << std::endl;
  } else {
    std::cout << "mmap_addr_to_firmwarecore success" << std::endl;
  }

  // if (load_table_to_firmwarecore(lib_handle)) {
  //   std::cout << "load_table_to_firmwarecore failed" << std::endl;
  // } else {
  //   std::cout << "load_table_to_firmwarecore success" << std::endl;
  // }

  ret_1 = test_sg_api_1d_memcpy(lib_handle, handle, allocator);
  if (ret_1 != 0) {
    std::cout << "test_sg_api_1d_memcpy fail" << std::endl;
  }
  ret_1 = test_sg_api_2d_memcpy(lib_handle, handle, allocator);
  if (ret_1 != 0) {
    std::cout << "test_sg_api_2d_memcpy fail" << std::endl;
  }

  bm_dev_free(handle);
  dlclose(lib_handle);

  // destroy_allocator(&allocator);
  return 0;
}
