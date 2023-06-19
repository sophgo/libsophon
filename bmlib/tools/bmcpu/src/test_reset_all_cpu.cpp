#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <bmlib_runtime.h>

#ifdef __linux__
#define MAX_CHIP_NUM 256

void *bmcpu_all_reset(void *arg) {
    bm_handle_t handle;
    bm_status_t ret;
    int dev_id = *(int *)arg;

    printf("bm-sophon%d reset cpu run!\n", dev_id);
    ret = bm_dev_request(&handle, dev_id);
    if ((ret != BM_SUCCESS) || (handle == NULL)) {
        printf("bm_dev_request error, ret = %d\r\n", ret);
        return (void *)BM_ERR_FAILURE;
    }

    ret = bmcpu_reset_cpu(handle);
    if (ret != BM_SUCCESS) {
        printf("reset cpu %d failed!\r\n", dev_id);
        bm_dev_free(handle);
        return (void *)BM_ERR_FAILURE;
    }

    bm_dev_free(handle);

    return (void *)BM_SUCCESS;
}

int main(void) {
    int    dev_num;
    int    i;
    int    arg[MAX_CHIP_NUM];
    pthread_t threads[MAX_CHIP_NUM];
    int    ret;

    if (BM_SUCCESS != bm_dev_getcount(&dev_num)) {
        printf("no sophon device found! do not need reset sophon device!\n");
        return -0;
    }

    if (dev_num > MAX_CHIP_NUM) {
        printf("too many device number %d!\n", dev_num);
        return -1;
    }

    for (i = 0; i < dev_num; i++) {
        arg[i] = i;
        ret = pthread_create(&threads[i], NULL, bmcpu_all_reset, (void *)&arg[i]);
        if (ret != 0) {
            printf("create thread %d error!\n", i);
            return -1;
        }
    }

    for (i = 0; i < dev_num; i++) {
        ret = pthread_join(threads[i], NULL);
        if (ret < 0) {
            printf("join thread %d error!\n", i);
            return -1;
        }
    }

    return 0;
}
#endif
