#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <bmlib_runtime.h>

int main(int argc, char *argv[]) {
#if !defined(USING_CMODEL) && !defined(SOC_MODE)
    bm_handle_t handle;
    bm_status_t ret;

    if (argc < 2) {
        printf("Usage: test_reset_cpu dev_id\n");
        return -1;
    }

    ret = bm_dev_request(&handle, atoi(argv[1]));
    if ((ret != BM_SUCCESS) || (handle == NULL)) {
        printf("bm_dev_request error, ret = %d\n", ret);
        return -1;
    }

    ret = bmcpu_reset_cpu(handle);
    if (ret != BM_SUCCESS) {
        printf("ERROR!!! reset cpu error!\n");
        bm_dev_free(handle);
        return -1;
    } else {
        printf("reset cpu success!\n");
    }

    bm_dev_free(handle);
#else
    argc = argc;
    argv = argv;
    printf("This test case is only valid in PCIe mode!\n");
#endif
    return 0;
}
