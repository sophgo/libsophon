#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include <stdio.h>

static bm_status_t bmcv_quantify_check(
        bm_handle_t handle,
        bm_image input,
        bm_image output) {
    if (handle == NULL) {
        bmlib_log("QUANTIFY", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    bm_image_format_ext src_format = input.image_format;
    bm_image_data_format_ext src_type = input.data_type;
    bm_image_format_ext dst_format = output.image_format;
    bm_image_data_format_ext dst_type = output.data_type;
    int src_h = input.height;
    int src_w = input.width;
    int dst_h = output.height;
    int dst_w = output.width;
    if (src_format != dst_format) {
        bmlib_log("QUANTIFY", BMLIB_LOG_ERROR, "input and output image format is NOT same");
        return BM_ERR_PARAM;
    }
    if (src_format != FORMAT_RGB_PLANAR &&
        src_format != FORMAT_BGR_PLANAR) {
        bmlib_log("QUANTIFY", BMLIB_LOG_ERROR, "Not supported image format");
        return BM_NOT_SUPPORTED;
    }
    if (src_type != DATA_TYPE_EXT_FLOAT32 ||
        dst_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("QUANTIFY", BMLIB_LOG_ERROR, "Not supported image data type");
        return BM_NOT_SUPPORTED;
    }
    if (src_h != dst_h || src_w != dst_w) {
        bmlib_log("QUANTIFY", BMLIB_LOG_ERROR, "inputs and output image size should be same");
        return BM_ERR_PARAM;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_quantify(
        bm_handle_t handle,
        bm_image input,
        bm_image output) {
    bm_status_t ret = BM_SUCCESS;

    bm_handle_check_2(handle, input, output);
    ret = bmcv_quantify_check(handle, input, output);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    bool output_alloc_flag = false;
    if (!bm_image_is_attached(output)) {
        ret = bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY);
        if (ret != BM_SUCCESS) {
            return ret;
        }
        output_alloc_flag = true;
    }
    // construct and send api
    int channel = bm_image_get_plane_num(input);
    int input_str[3], output_str[3];
    bm_image_get_stride(input, input_str);
    bm_image_get_stride(output, output_str);

    bm_device_mem_t input_mem[3];
    bm_image_get_device_mem(input, input_mem);
    bm_device_mem_t output_mem[3];
    bm_image_get_device_mem(output, output_mem);

    bm_api_cv_quantify_t api;
    api.channel = channel;
    for (int i = 0; i < channel; i++) {
        api.input_addr[i] = bm_mem_get_device_addr(input_mem[i]);
        api.output_addr[i] = bm_mem_get_device_addr(output_mem[i]);
        api.width[i] = input.image_private->memory_layout[i].W;
        api.height[i] = input.image_private->memory_layout[i].H;
        api.input_str[i] = input_str[i];
        api.output_str[i] = output_str[i];
    }
    // rgb-planar format's channel is 1
    if (input.image_format == FORMAT_RGB_PLANAR ||
        input.image_format == FORMAT_BGR_PLANAR) {
        api.height[0] *= 3;
    }

    unsigned int chipid;
    bm_get_chipid(handle, &chipid);

    switch (chipid)
    {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_quantify", (u8 *)&api, sizeof(api));
            if(BM_SUCCESS != ret){
                bmlib_log("QUANTIFY", BMLIB_LOG_ERROR, "quantify sync api error\n");
                if (output_alloc_flag) {
                    for (int i = 0; i < channel; i++) {
                        bm_free_device(handle, output_mem[i]);
                    }
                }
                return ret;
            }
            break;

        default:
            printf("ChipID is NOT supported\n");
            break;
    }

    return ret;
}
