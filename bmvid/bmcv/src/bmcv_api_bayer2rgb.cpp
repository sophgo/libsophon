#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"
#include "bmcv_common_bm1684.h"
#include "bmlib_runtime.h"
#include <memory>

#define KERNEL_SIZE 3 * 3 * 3 * 4 * 64

bm_status_t bmcv_image_bayer2rgb(
        bm_handle_t handle,
        unsigned char* convd_kernel,
        bm_image input,
        bm_image output) {
    bm_status_t ret = BM_SUCCESS;
    if (!bm_image_is_attached(output)) {
        ret = bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY);
        if (ret != BM_SUCCESS) {
            return ret;
        }
    }

    bm_device_mem_t input_mem;
    bm_device_mem_t output_mem;
    bm_image_get_device_mem(input, &input_mem);
    bm_image_get_device_mem(output, &output_mem);

    bm_device_mem_t input_dev_buffer_padding_ul;
    bm_device_mem_t input_dev_buffer_padding_br;
    bm_device_mem_t sys_addr_ul;
    bm_device_mem_t sys_addr_br;
    bm_device_mem_t convd_kernel_data;
    bm_device_mem_t sys_addr_temp_g;

    bm_api_cv_bayer2rgb_t param;
    param.height = input.height;
    param.width  = input.width;
    param.input_addr  = bm_mem_get_device_addr(input_mem);
    param.output_addr = bm_mem_get_device_addr(output_mem);

    bm_malloc_device_byte(handle, &sys_addr_ul, input.height * input.width * 4);
    param.sys_mem_addr_temp_ul = bm_mem_get_device_addr(sys_addr_ul);
    bm_malloc_device_byte(handle, &sys_addr_br, input.width * input.height * 4);
    param.sys_mem_addr_temp_br = bm_mem_get_device_addr(sys_addr_br);
    bm_malloc_device_byte(handle, &sys_addr_temp_g, input.width * input.height * 4);
    param.sys_mem_addr_temp_g = bm_mem_get_device_addr(sys_addr_temp_g);

    bm_malloc_device_byte(handle, &convd_kernel_data, KERNEL_SIZE * sizeof(unsigned char));
    bm_memcpy_s2d(handle, convd_kernel_data, convd_kernel);
    param.kernel_addr = bm_mem_get_device_addr(convd_kernel_data);

    bm_malloc_device_byte(handle, &input_dev_buffer_padding_ul, (input.width + 2) * (input.height + 2) * sizeof(char));
    bm_malloc_device_byte(handle, &input_dev_buffer_padding_br, (input.width + 2) * (input.height + 2) * sizeof(char));
    param.input_addr_padding_up_left = bm_mem_get_device_addr(input_dev_buffer_padding_ul);
    param.input_addr_padding_bottom_right = bm_mem_get_device_addr(input_dev_buffer_padding_br);

    unsigned int chipid;
    bm_get_chipid(handle, &chipid);

    switch(chipid)
    {
        case 0x1684:
            ret = bm_send_api(handle, BM_API_ID_CV_BAYER2RGB, (uint8_t*)&param, sizeof(param));
            if (BM_SUCCESS != ret) {
                bmlib_log("BAYER2RGB", BMLIB_LOG_ERROR, "bayer2rgb send api error\n");
                goto FREEMEM;
            }
            ret = bm_sync_api(handle);
            if (BM_SUCCESS != ret) {
                bmlib_log("BAYER2RGB", BMLIB_LOG_ERROR, "bayer2rgb sync api error\n");
                goto FREEMEM;
            }
            break;

        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_bayer2rgb", (u8 *)&param, sizeof(param));
            if(BM_SUCCESS != ret){
                bmlib_log("BAYER2RGB", BMLIB_LOG_ERROR, "bayer2rgb sync api error\n");
                goto FREEMEM;
            }
            break;

        default:
            printf("BM_NOT_SUPPORTED!\n");
            break;
    }

FREEMEM:
    bm_free_device(handle, sys_addr_ul);
    bm_free_device(handle, sys_addr_br);
    bm_free_device(handle, sys_addr_temp_g);
    bm_free_device(handle, convd_kernel_data);
    bm_free_device(handle, input_dev_buffer_padding_ul);
    bm_free_device(handle, input_dev_buffer_padding_br);

    return ret;
}