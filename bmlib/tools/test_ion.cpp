#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <atomic>

#include "bmlib_ioctl.h"
#include "bmlib_runtime.h"
#include "bmlib_memory.h"
#include "ion.h"


void *map_physical_memory(uint64_t phys_addr, size_t length) {
  int fd;
  void *mapped_addr;

  fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (fd < 0) {
    perror("open /dev/mem");
    return MAP_FAILED;
  }

//   printf("phys_addr: 0x%lx, length: 0x%lx\n", phys_addr, length);
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

void test_ion_write_read(bm_handle_t handle, bm_device_mem_t ion_mem, size_t byte_size, char c) {
    uint8_t *input = reinterpret_cast<uint8_t *>(malloc(byte_size));
    bm_device_mem_t input_mem = bm_mem_from_system(input);

    for (size_t i = 0; i < byte_size; i++) {
        input[i] = c;
    }
    bm_mem_write_data_to_ion(handle, &ion_mem, input, byte_size);

    uint8_t *output = reinterpret_cast<uint8_t *>(malloc(byte_size));

    bm_mem_read_data_from_ion(handle, &ion_mem, output, byte_size);

    printf("=====TEST write ion data:%c=====\n", c);
    void *virtual_addr = map_physical_memory(bm_mem_get_device_addr(ion_mem),
                                            byte_size);
    printf("read from /dev/mem:\n");
    for (int i = 0; i < byte_size; i++) {
        printf("%c ", *(reinterpret_cast<uint8_t *>(virtual_addr) + i));
    }
    printf("\n");

    printf("read from ion driver:\n");
    for (int i = 0; i < byte_size; i++) {
        printf("%c ", output[i]);
    }
    printf("\n");
}


struct sys_ion_data {
	__u32 size;
	__u32 cached;
	__u64 addr_p;
	__u8 name[32];
};

void test_get_ion_heap_info(bm_handle_t handle, struct bm_heap_info *heap_info){
    bm_get_ion_head_info(handle, heap_info);
    printf("ION heap info: start_addr=0x%llx, size=0x%llx\n", heap_info->mem_start_addr, heap_info->mem_size);
}


int main() {
    bm_handle_t handle;
    bm_dev_request(&handle, 0);
    struct bm_heap_info heap_info;
    test_get_ion_heap_info(handle, &heap_info);
    bm_device_mem_t ion_mem;
    size_t sg_dtype_len = sizeof(uint8_t);
    size_t shape_cnt = 256;
    size_t byte_size = shape_cnt * sg_dtype_len;
    int ret = bm_malloc_device_byte(handle, &ion_mem, byte_size);
    for (size_t i = 0; i < 50; i++)
    {
        printf("======================================TEST %d:%c====================================\n", i, 'a' + i%26);
        test_ion_write_read(handle, ion_mem, byte_size, 'a' + i%26);
    }
    bm_dev_free(handle);
    return 0;
}
