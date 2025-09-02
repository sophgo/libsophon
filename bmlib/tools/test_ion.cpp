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

int main() {
    int ret=0;
    bm_handle_t handle = nullptr; // Assume handle is initialized properly
    bm_device_mem_t pmem;
    ret=bm_dev_request(&handle,0);
    if(ret!=0){
        printf("bm_dev_request failed\n");
        return -1;
    }
    printf("starting memory test\n");
    for(int i=0;i<1000;i++){
        bm_malloc_device_byte(handle, &pmem, 1024 * 10);
        usleep(1000*1000);
        bm_free_device(handle, pmem);
    }
    bm_dev_free(handle);

    return 0;
}
