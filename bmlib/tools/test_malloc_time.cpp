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

int test_alloc_time(unsigned long long transfer_size, int test_num)
{
    bm_handle_t handle = NULL;
    bm_status_t ret = BM_SUCCESS;
    int i = 0;
    struct timeval t1, t2, timediff;
    unsigned long consume_alloc = 0;
    unsigned long consume_free = 0;
    u64 paddr[test_num];
    bm_device_mem_u64_t dev_buffer[test_num];

    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS || handle == NULL) {
        printf("bm_dev_request failed, ret = %d\n", ret);
        return -1;
    }

    for (i=0; i<test_num; i++) {
        gettimeofday(&t1, NULL);
        ret = bm_malloc_device_byte_u64(handle, &(dev_buffer[i]), transfer_size);
        gettimeofday(&t2, NULL);
        timersub(&t2, &t1, &timediff);
        consume_alloc += timediff.tv_sec * 1000000 + timediff.tv_usec;
    }

    for (i=0; i<test_num; i++) {
        gettimeofday(&t1, NULL);
        bm_free_device_u64(handle, dev_buffer[i]);
        gettimeofday(&t2, NULL);
        timersub(&t2, &t1, &timediff);
        consume_free += timediff.tv_sec * 1000000 + timediff.tv_usec;
    }

    consume_alloc = consume_alloc/test_num;
    consume_free = consume_free/test_num;
    printf("bm_device_mem malloc size: 0x%-10llx num: %-5d malloc time: %-3lu free time: %-3lu\n",
            transfer_size, test_num, consume_alloc, consume_free);

    consume_alloc = 0;
    consume_free = 0;
    for (i=0; i<test_num; i++) {
        gettimeofday(&t1, NULL);
        bm_malloc_device_mem(handle, &(paddr[i]), 0, transfer_size);
        gettimeofday(&t2, NULL);
        timersub(&t2, &t1, &timediff);
        consume_alloc += timediff.tv_sec * 1000000 + timediff.tv_usec;
    }

    for (i=0; i<test_num; i++) {
        gettimeofday(&t1, NULL);
        bm_free_device_mem(handle, paddr[i]);
        gettimeofday(&t2, NULL);
        timersub(&t2, &t1, &timediff);
        consume_free += timediff.tv_sec * 1000000 + timediff.tv_usec;
    }

    consume_alloc = consume_alloc/test_num;
    consume_free = consume_free/test_num;
    printf("paddr         malloc size: 0x%-10llx num: %-5d malloc time: %-3lu free time: %-3lu\n",
            transfer_size, test_num, consume_alloc, consume_free);

    bm_dev_free(handle);

    return 0;
}

int main(int argc, char *argv[])
{
    test_alloc_time(0x1000, 10000);
    test_alloc_time(0x10000, 1000);
    test_alloc_time(0x100000, 100);
    test_alloc_time(0x1000000, 10);

    return 0;
}
