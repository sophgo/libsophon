#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <bmlib_runtime.h>

#ifdef __linux__
#define MAX_CHIP_NUM 256

bm_status_t bmcpu_pre_start(int dev_id) {
    bm_handle_t handle;
    bm_status_t ret;
    char* dev = "/dev/bm-sophon";
    char*       kernel_path = "/opt/sophon/libsophon-current/data";
    char fip_path[100];
    char ramdisk_path[100];
    char dev_path[30];

    sprintf(dev_path, "%s%d", dev, dev_id);
    sprintf(fip_path, "%s%s", kernel_path, "/fip.bin");
    sprintf(ramdisk_path, "%s%s", kernel_path, "/ramboot_rootfs.itb");

    while (access(dev_path, F_OK) < 0) {
        sleep(1);
    }

    printf("bm-sophon%d fip path: %s; \nkernel path: %s\n", dev_id, fip_path, ramdisk_path);
    ret = bm_dev_request(&handle, dev_id);
    if ((ret != BM_SUCCESS) || (handle == NULL)) {
        printf("bm_dev_request error, ret = %d\r\n", ret);
        return BM_ERR_FAILURE;
    }

    ret = bmcpu_start_cpu(handle, fip_path, ramdisk_path);
    if ((ret != BM_SUCCESS) && (ret != BM_NOT_SUPPORTED)) {
        printf("start cpu %d failed!\r\n", dev_id);
        bm_dev_free(handle);
        return BM_ERR_FAILURE;
    }

    printf("bm-sophon%d start a53 success!\n", dev_id);

    bm_dev_free(handle);

    return BM_SUCCESS;
}

int main(int argc, char* argv[]) {
    int    dev_num;
    int    rel_num;
    int    i;
    int    arg[MAX_CHIP_NUM];
    pthread_t threads[MAX_CHIP_NUM];
    int    ret;

    if (argc != 2) {
        printf("please input param just like: test_pre_start_one bm-sophon0\n");
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
    printf("%s %d: input param is %s\n", __func__, __LINE__, argv[1]);
    rel_num = atoi(argv[1]);
    ret = bmcpu_pre_start(rel_num);
    if (ret != BM_SUCCESS) {
        printf("bm-sophon%d start a53 failed!\n");
        return -1;
    }

    return 0;
}
#endif
