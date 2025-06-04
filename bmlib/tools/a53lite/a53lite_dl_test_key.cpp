#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "api.h"

#ifdef __linux__
#define MAX_CHIP_NUM 256

char g_library_file[100];

#define A53LITE_RUNTIME_LOG_TAG "a53lite_runtime"

int main(int argc, char *argv[])
{
    int dev_num;
    int rel_num;
    int arg[MAX_CHIP_NUM];
    tpu_kernel_module_t bm_module;
    bm_handle_t handle;
    tpu_kernel_function_t f_id;
    bm_status_t ret = BM_SUCCESS;
    u64 args = 0;
    char key[80];
    int size;

    if (argc != 4)
    {
        printf("please input param just like: a53lite_load_lib 0 test.so key\n");
        return -1;
    }

    if (BM_SUCCESS != bm_dev_getcount(&dev_num))
    {
        printf("no sophon device found!\n");
        return -1;
    }

    if (dev_num > MAX_CHIP_NUM)
    {
        printf("too many device number %d!\n", dev_num);
        return -1;
    }

    rel_num = atoi(argv[1]);
    ret = bm_dev_request(&handle, rel_num);
    if ((ret != BM_SUCCESS) || (handle == NULL))
    {
        printf("bm_dev_request error, ret = %d\r\n", ret);
        return BM_ERR_FAILURE;
    }

    strcpy(g_library_file, argv[2]);
    strcpy(key, argv[3]);
    size = strlen(key);
    bm_module = tpu_kernel_load_module_file_key(handle, g_library_file, key, size);
    if(bm_module == NULL) {
        printf("bm_module is null!\n");
        return BM_ERR_FAILURE;
    }

    f_id = tpu_kernel_get_function(handle, bm_module, "tpu_kernel_api_multi_crop_resize");

    ret = tpu_kernel_launch(handle, f_id, (void *)&args, sizeof(unsigned long long));
    printf("%s ret is %d\n", __FILE__, ret);
    bm_dev_free(handle);

    return 0;
}
#endif
