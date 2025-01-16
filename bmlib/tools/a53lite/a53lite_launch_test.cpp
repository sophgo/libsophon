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
#include "bmlib_memory.h"
#include "api.h"

#ifdef __linux__
#define MAX_CHIP_NUM 256

char g_library_file[100];

#define A53LITE_RUNTIME_LOG_TAG "a53lite_runtime"

typedef struct gdma_d2d_st {
	bm_handle_t handle;
	int transfer_size;
	unsigned long long src_device_addr;
	unsigned long long dst_device_addr;
	int core_id;
	int loop_num;
}gdma_d2d_t;

bm_basic_func_t g_basic_func[] = {
  {.func_name = (char*)"sg_api_memset", 0, 0},
  {.func_name = (char*)"sg_api_memcpy", 0, 0},
  {.func_name = (char*)"sg_api_memcpy_wstride", 0, 0},
  {.func_name = (char*)"sg_api_memcpy_byte", 0, 0},
};

int main(int argc, char *argv[])
{
    int dev_num;
    int rel_num;
    int arg[MAX_CHIP_NUM];
    tpu_kernel_module_t bm_module;
    bm_handle_t handle;
	int core_id;
    tpu_kernel_function_t f_id0;
	tpu_kernel_function_t f_id1;
	void *param_data;
	unsigned int param_size;
	tpu_launch_param_t param_list[2];
	bm_device_mem_t src_device_buffer;
	bm_device_mem_t dst_device_buffer;
	int transfer_size = 0x100000;
	unsigned long long src_device_addr = 0x180000000;
	unsigned long long dst_device_addr = 0x190000000;
	bm_status_t ret = BM_SUCCESS;

    if (argc != 2)
    {
        printf("please input param just like: a53lite_lanuch_test 0 \n");
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

	core_id = 0;

#define KERNEL_MODULE_PATH "/lib/firmware/libbm1688_kernel_module.so"
	tpu_kernel_module_t bm_module0 = NULL;
  	tpu_kernel_module_t bm_module1 = NULL;
    bm_module0 = tpu_kernel_load_module_file_to_core(handle, KERNEL_MODULE_PATH, 0);
    if(bm_module0 == NULL) {
        printf("bm_module0 is null!\n");
        return BM_ERR_FAILURE;
    }
    bm_module1 = tpu_kernel_load_module_file_to_core(handle, KERNEL_MODULE_PATH, 1);
    if(bm_module1 == NULL) {
        printf("bm_module1 is null!\n");
        return BM_ERR_FAILURE;
    }
    for (int i=0; i < sizeof(g_basic_func) / sizeof(g_basic_func[0]); i++) {
    	f_id0 = tpu_kernel_get_function_from_core(handle, bm_module0, g_basic_func[i].func_name, 0);
    	f_id1 = tpu_kernel_get_function_from_core(handle, bm_module1, g_basic_func[i].func_name, 1);
    }
    free(bm_module0);
    bm_module0 = NULL;
    free(bm_module1);
    bm_module1 = NULL;

	src_device_buffer = bm_mem_from_device(src_device_addr, transfer_size);
	dst_device_buffer = bm_mem_from_device(dst_device_addr, transfer_size);
	bm_api_memcpy_byte_t api = {bm_mem_get_device_addr(src_device_buffer),
                                bm_mem_get_device_addr(dst_device_buffer), (u64)transfer_size};

	param_list[0].core_id = core_id;
	param_list[0].func_id = f_id0;
	param_list[0].param_data = &api;
	param_list[0].param_size = sizeof(api);

	core_id = 1;
	src_device_addr = 0x200000000;
	dst_device_addr = 0x210000000;
	src_device_buffer = bm_mem_from_device(src_device_addr, transfer_size);
	dst_device_buffer = bm_mem_from_device(dst_device_addr, transfer_size);
	api = {bm_mem_get_device_addr(src_device_buffer),
                                bm_mem_get_device_addr(dst_device_buffer), (u64)transfer_size};

	param_list[1].core_id = core_id;
	param_list[1].func_id = f_id1;
	param_list[1].param_data = &api;
	param_list[1].param_size = sizeof(api);

	printf("runing tpu_kernel_launch_async_multicores ...\n");
    u32 *data = (u32 *)param_list[0].param_data;
    for (int i =0; i< param_list[0].param_size/4; i++)
        printf("[%s: %d] 0x%x\n", __func__, __LINE__, *(data+i));
    data = (u32 *)param_list[1].param_data;
    for (int i =0; i< param_list[1].param_size/4; i++)
        printf("[%s: %d]  0x%x\n", __func__, __LINE__, *(data+i));
    ret = tpu_kernel_launch_async_multicores(handle, &param_list[0], 2);

    printf("run tpu_kernel_launch_async_multicores end !\n");
}
#endif