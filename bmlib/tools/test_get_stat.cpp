#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "string.h"

#define BMLIB_TEST_CASE_TAG "bmlib_test_case"

int main(int argc, char *argv[]) {
#if !defined(USING_CMODEL)
    bm_handle_t handle;
    bm_status_t ret;
    bm_dev_stat_t stat;
    char cmd[100];

    ret = bm_dev_request(&handle, 0);
    if ((ret != BM_SUCCESS) || (handle == NULL)) {
        printf("bm_dev_request error, ret = %d\n", ret);
        return -1;
    }

    ret = bm_get_stat(handle, &stat);
    if (ret != BM_SUCCESS)
    {
        bmlib_log(BMLIB_TEST_CASE_TAG,
                  BMLIB_LOG_ERROR,
                  "get heap stat, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    }

    printf("heap mem_total: %d MB, mem_used: %d MB, tpu_util: %d MB, heap_num:%d\n", stat.mem_total, stat.mem_used, stat.tpu_util, stat.heap_num);
    printf("heap 0 mem_total: %d MB, mem_avail: %d MB, mem_used:%d MB\n", stat.heap_stat[0].mem_total, stat.heap_stat[0].mem_avail, stat.heap_stat[0].mem_used);
    printf("heap 1 mem_total: %d MB, mem_avail: %d MB, mem_used:%d MB\n", stat.heap_stat[1].mem_total, stat.heap_stat[1].mem_avail, stat.heap_stat[1].mem_used);
    //printf("cpu mem(MB): \n");
    sprintf(cmd, "free -m");
    system(cmd);
    bm_dev_free(handle);
#else
    argc = argc;
    argv = argv;
    printf("This test case is only valid in PCIe mode!\n");
#endif
    return 0;
}