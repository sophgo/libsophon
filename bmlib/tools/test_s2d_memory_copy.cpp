#include "bmlib_ioctl.h"
#include "bmlib_runtime.h"
#include "bmlib_memory.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char** argv) {
    bm_handle_t bm_handle;
    bm_status_t status = bm_dev_request(&bm_handle, 0);
    assert(status == BM_SUCCESS);

    size_t size = 4096;//4k表示一个页大小
    bm_device_mem_t vari_mem;
    bm_malloc_device_byte(bm_handle, &vari_mem, size);
    printf("mem addr:0x%llx\n", vari_mem.u.device.device_addr);
    printf("size: %zu\n", vari_mem.size);

    void *virtual_addr;
    int fd;

    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
      perror("open /dev/mem");
    }

    virtual_addr =
        mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, vari_mem.u.device.device_addr);
    if (virtual_addr == MAP_FAILED) {
      perror("mmap");
      close(fd);
    }
    close(fd);

    //auto vari_mem = bm_mem_from_device(addr, size);
    /*1.testcase for bmmemcpy_s2d*/
    uint8_t* vari_data = new uint8_t[size];
    memset(vari_data, 'H', size);
    // ((uint32_t*)vari_data)[0] = 0xeee;
    // ((uint32_t*)vari_data)[1] = 0xffff;
    // ((uint32_t*)vari_data)[2] = 0x100000;
    // ((uint32_t*)vari_data)[4] = 0x9300;

    status = bm_memcpy_s2d(bm_handle, vari_mem, vari_data);
    printf("============print bm_memcpy_s2d===========\n");
    for (int i = 0; i < size/10; i++) {
      printf("%c",*(reinterpret_cast<uint8_t *>(virtual_addr) + i));
      if (i % 16 == 15) {
        printf("\n");
      } else {
        printf(" ");
      }
    }
    printf("\n");
    /*2.testcase for bmmemcpy_s2d for phy offset*/
    uint64_t addr = vari_mem.u.device.device_addr + 128;
    auto pmem = bm_mem_from_device(addr, 32);
    memset(vari_data, 'T', size);
    status = bm_memcpy_s2d(bm_handle, pmem, vari_data);
    printf("============print bm_memcpy_s2d for phy offset===========\n");
    for (int i = 0; i < size/10; i++) {
      printf("%c",*(reinterpret_cast<uint8_t *>(virtual_addr) + i));
      if (i % 16 == 15) {
        printf("\n");
      } else {
        printf(" ");
      }
    }
    printf("\n");
    assert( status == BM_SUCCESS);
    {
      /*3.testcase for bmmemcpy_s2d_partial_offset*/
      memset(vari_data, 'J', size);
      status = bm_memcpy_s2d_partial_offset(bm_handle, vari_mem, vari_data, 64, 64);
      printf("============print bm_memcpy_s2d_partial_offset===========\n");
      for (int i = 0; i < size/10; i++) {
        printf("%c",*(reinterpret_cast<uint8_t *>(virtual_addr) + i));
        if (i % 16 == 15) {
          printf("\n");
        } else {
          printf(" ");
        }
      }
      printf("\n");
      assert( status == BM_SUCCESS);
    }

    bm_thread_sync(bm_handle);
    delete[] vari_data;

    bm_free_device(bm_handle, vari_mem);
    bm_dev_free(bm_handle);
    return 0;
}