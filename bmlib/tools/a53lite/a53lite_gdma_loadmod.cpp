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

int array_cmp_int(
    unsigned char *p_exp,
    unsigned char *p_got,
    int len,
    const char *info_label)
{
    int idx;
    for (idx = 0; idx < len; idx++)
    {
        if (p_exp[idx] != p_got[idx])
        {
            printf("%s error at index %d exp %x got %x\n",
                   info_label, idx, p_exp[idx], p_got[idx]);
            return -1;
        }
    }
    return 0;
}

bm_status_t a53lite_test_gdma_d2d(bm_handle_t handle, u64 transfer_size,
                                  unsigned long long src_device_addr,
                                  unsigned long long dst_device_addr, tpu_kernel_function_t f_id)
{
    bm_status_t ret = BM_SUCCESS;
    bm_profile_t profile_start, profile_end;
    int i = 0;
    int rand_num = rand();
    unsigned long long time = 0;
    bm_device_mem_t src_device_buffer;
    bm_device_mem_t dst_device_buffer;
    unsigned char *sys_send_buffer = (unsigned char *)malloc(transfer_size);
    unsigned char *sys_receive_buffer = (unsigned char *)malloc(transfer_size);

    struct timeval tv_start;
    struct timeval tv_end;
    struct timeval timediff;
    unsigned long consume = 0;

    if (!sys_send_buffer || !sys_receive_buffer)
    {
        printf("malloc buffer for test failed\n");
        return BM_ERR_NOMEM;
    }

    for (i = 0; i < transfer_size; i++)
        *(sys_send_buffer + i) = i + rand_num;

    if (src_device_addr == 0x0)
    {
        ret = bm_malloc_device_dword(handle, &src_device_buffer, transfer_size / 4);
        if (ret != BM_SUCCESS)
        {
            printf("malloc device memory size = %llu failed, ret = %d\n", transfer_size, ret);
            return BM_ERR_NOMEM;
        }
    }
    else
    {
        src_device_buffer = bm_mem_from_device(src_device_addr, transfer_size);
    }

    if (dst_device_addr == 0x0)
    {
        ret = bm_malloc_device_dword(handle, &dst_device_buffer, transfer_size / 4);
        if (ret != BM_SUCCESS)
        {
            printf("malloc device memory size = %llu failed, ret = %d\n", transfer_size, ret);
            return BM_ERR_NOMEM;
        }
    }
    else
    {
        dst_device_buffer = bm_mem_from_device(dst_device_addr, transfer_size);
    }

    // printf("%s %d: src device addr is 0x%llx, dst device addr is 0x%llx\n", __FILE__, __LINE__, bm_mem_get_device_addr(src_device_buffer), bm_mem_get_device_addr(dst_device_buffer));
    ret = bm_memcpy_s2d(handle,
                        src_device_buffer,
                        sys_send_buffer);
    if (ret != BM_SUCCESS)
    {
        if (sys_send_buffer)
            free(sys_send_buffer);
        if (sys_receive_buffer)
            free(sys_receive_buffer);
        printf("CDMA transfer from system to device failed, ret = %d\n", ret);
        return BM_ERR_FAILURE;
    }

    bm_api_memcpy_byte_t arg = {bm_mem_get_device_addr(src_device_buffer),
                                bm_mem_get_device_addr(dst_device_buffer), transfer_size};

    gettimeofday(&tv_start, NULL);
    ret = tpu_kernel_launch(handle, f_id, (void *)&arg, sizeof(bm_api_memcpy_byte_t));
    gettimeofday(&tv_end, NULL);
    timersub(&tv_end, &tv_start, &timediff);
    if (ret != BM_SUCCESS)
    {
        printf("GDMA transfer from system to system failed, ret = %d\n", ret);
        return BM_ERR_FAILURE;
    }
    consume = timediff.tv_sec * 1000000 + timediff.tv_usec;
    float bandwidth = (double)transfer_size / (1024.0 * 1024.0) / (consume / 1000000.0);
    printf("Src:%llx, Dst:%llx, Transfer size:0x%llx byte. Cost time:%lu us, Write Bandwidth:%.2f MB/s\n",
           bm_mem_get_device_addr(src_device_buffer),
           bm_mem_get_device_addr(dst_device_buffer),
           transfer_size,
           consume,
           bandwidth);

    ret = bm_memcpy_d2s(handle, sys_receive_buffer, dst_device_buffer);

    if (ret != BM_SUCCESS)
    {
        if (sys_send_buffer)
            free(sys_send_buffer);
        if (sys_receive_buffer)
            free(sys_receive_buffer);
        printf("CDMA transfer from system to device failed, ret = %d\n", ret);
        return BM_ERR_FAILURE;
    }

    if (array_cmp_int(sys_send_buffer, sys_receive_buffer, transfer_size, "test_gdma"))
    {
        if (sys_send_buffer)
            free(sys_send_buffer);
        if (sys_receive_buffer)
            free(sys_receive_buffer);
        printf("Src:%llx, Dst:%llx, Transfer size:0x%llx byte fail\n",
               bm_mem_get_device_addr(src_device_buffer),
               bm_mem_get_device_addr(dst_device_buffer),
               transfer_size);
        return BM_ERR_FAILURE;
    }

    if (dst_device_addr == 0x0)
    {
        bm_free_device(handle, dst_device_buffer);
    }
    if (src_device_addr == 0x0)
    {
        bm_free_device(handle, src_device_buffer);
    }
    if (sys_send_buffer)
        free(sys_send_buffer);
    if (sys_receive_buffer)
        free(sys_receive_buffer);

    return BM_SUCCESS;
}

#define A53LITE_RUNTIME_LOG_TAG "a53lite_runtime"

int main(int argc, char *argv[])
{
    int dev_num;
    int rel_num;
    int arg[MAX_CHIP_NUM];
    tpu_kernel_module_t bm_module;
    bm_handle_t handle;
    tpu_kernel_function_t f_id, cnt = 0;
    int transfer_size = 0x40000000;
    bm_status_t ret = BM_SUCCESS;
    const char *module_name = "libgdma.so";

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
    f_id = tpu_kernel_get_function(handle, bm_module, "sg_api_memcpy_byte");

    while (cnt != 10)
    {
        ret = a53lite_test_gdma_d2d(handle, transfer_size, 0, 0, f_id);
        cnt++;
    }

    bm_dev_free(handle);

    return 0;
}
#endif
