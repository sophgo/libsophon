#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <bmlib_runtime.h>
#include <bmcpu_internal.h>
#ifdef __linux__
#include <sys/syscall.h>
#else
#include <windows.h>
#endif

int main(int argc, char *argv[]) {
#if !defined(USING_CMODEL) && !defined(SOC_MODE)
    bm_handle_t handle;
    bm_status_t ret;

    if (argc < 4) {
        printf("Usage: test_start_mix_cpu dev_id fip_file itb_file\n");
        return -1;
    }

    ret = bm_dev_request(&handle, atoi(argv[1]));
    if ((ret != BM_SUCCESS) || (handle == NULL)) {
        printf("bm_dev_request error, ret = %d\n", ret);
        return -1;
    }

    ret = bmcpu_start_mix_cpu(handle, argv[2],  argv[3]);
    if (ret != BM_SUCCESS) {
        printf("ERROR!!! start mix cpu error!\n");
        bm_dev_free(handle);
        return -1;
    } else {
        printf("Start mix cpu success!\n");
    }

    bm_dev_free(handle);
#else
    argc = argc;
    argv = argv;
    printf("This test case is only valid in PCIe mode!\n");
#endif
    return 0;
}
