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
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#define INT_MAX 2147483647

int main(int argc, char *argv[])
{
    bm_handle_t handle = NULL;
    bm_status_t ret = BM_SUCCESS;
    int transfer_size = 0x100;
    int i = 0;
    bm_device_mem_t dev_buffer;
    int test_num = INT_MAX;

    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS || handle == NULL) {
        printf("bm_dev_request failed, ret = %d\n", ret);
        return -1;
    }

    for (i=0; i<test_num; i++) {
        ret = bm_malloc_device_byte(handle, &dev_buffer, transfer_size);
        if (ret != BM_SUCCESS) {
            break;
        }
    }

    printf("test success! malloc device memory max num is %d\n", i);
    bm_dev_free(handle);

    return 0;
}
