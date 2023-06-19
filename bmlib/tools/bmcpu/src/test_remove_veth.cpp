#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <bmlib_runtime.h>
#include <bmcpu_internal.h>

int main(int argc, char *argv[]) {
#if !defined(USING_CMODEL) && !defined(SOC_MODE)
    bm_handle_t handle;
    bm_status_t ret;

    if (argc < 2) {
        printf("Usage: test_remove_veth dev_id\n");
        return -1;
    }

    ret = bm_dev_request(&handle, atoi(argv[1]));
    if ((ret != BM_SUCCESS) || (handle == NULL)) {
        printf("bm_dev_request error, ret = %d\n", ret);
        return -1;
    }

    ret = bm_remove_veth(handle);
    if (ret != BM_SUCCESS)
    {
        bmlib_log(BMCPU_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "remove virtual ethernet error, ret %d\n",
                  ret);
        return BM_ERR_FAILURE;
    } else {
        printf("remove veth success!\n");
    }

    bm_dev_free(handle);
#else
    argc = argc;
    argv = argv;
    printf("This test case is only valid in PCIe mode!\n");
#endif
    return 0;
}
