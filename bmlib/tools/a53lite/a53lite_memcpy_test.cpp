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
    int transfer_size = 0x100000;
    bm_status_t ret = BM_SUCCESS;
    const char *module_name = "liba53_memcpy.so";
    bm_device_mem_t device_buffer;
    u64 args = 0;

    if (argc != 3)
    {
        printf("please input param just like: a53lite_load_lib 0 test.so\n");
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
    int fd = open((const char *)g_library_file, O_RDONLY);
    struct stat fileStat;
    u8 *file_buffer;
    if (-1 == fstat(fd, &fileStat))
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "stat file %s error!!\n",
                  module_name);
        close(fd);
        return BM_ERR_PARAM;
    }
    file_buffer = (u8 *)malloc(fileStat.st_size);
    if (read(fd, file_buffer, fileStat.st_size) != fileStat.st_size)
    {
        bmlib_log(A53LITE_RUNTIME_LOG_TAG,
                  BMLIB_LOG_ERROR,
                  "read file %s error\n",
                  module_name);
        free(file_buffer);
        close(fd);
        return BM_ERR_FAILURE;
    }

    bm_module = tpu_kernel_load_module(handle, (const char *)file_buffer, fileStat.st_size);
    f_id = tpu_kernel_get_function(handle, bm_module, "test_memcpy");

    ret = bm_malloc_device_dword(handle, &device_buffer, transfer_size / 4);
    if (ret != BM_SUCCESS)
    {
        printf("malloc device memory size = %llu failed, ret = %d\n", transfer_size, ret);
        return -1;
    }

    args = bm_mem_get_device_addr(device_buffer);
    printf("device memory addr is 0x%llx\n", args);
    ret = tpu_kernel_launch(handle, f_id, (void *)&args, sizeof(unsigned long long));
    bm_free_device(handle, device_buffer);
    ret = tpu_kernel_unload_module(handle, bm_module);

    bm_dev_free(handle);

    return 0;
}
#endif
