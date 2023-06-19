#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include <memory>
#include <vector>

static bm_status_t bmcv_threshold_check(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        bm_thresh_type_t type) {
    if (handle == NULL) {
        bmlib_log("THRESHOLD", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    if (type >= BM_THRESH_TYPE_MAX) {
        bmlib_log("THRESHOLD", BMLIB_LOG_ERROR, "Threshold type is error!\r\n");
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
        bmlib_log("THRESHOLD", BMLIB_LOG_ERROR, "input and output image format is NOT same");
        return BM_ERR_PARAM;
    }
    if (src_format != FORMAT_YUV420P &&
        src_format != FORMAT_YUV422P &&
        src_format != FORMAT_YUV444P &&
        src_format != FORMAT_NV12 &&
        src_format != FORMAT_NV21 &&
        src_format != FORMAT_NV61 &&
        src_format != FORMAT_NV16 &&
        src_format != FORMAT_NV24 &&
        src_format != FORMAT_GRAY &&
        src_format != FORMAT_RGB_PLANAR &&
        src_format != FORMAT_BGR_PLANAR &&
        src_format != FORMAT_RGB_PACKED &&
        src_format != FORMAT_BGR_PACKED &&
        src_format != FORMAT_RGBP_SEPARATE &&
        src_format != FORMAT_BGRP_SEPARATE) {
        bmlib_log("THRESHOLD", BMLIB_LOG_ERROR, "Not supported image format");
        return BM_NOT_SUPPORTED;
    }
    if (src_type != DATA_TYPE_EXT_1N_BYTE ||
        dst_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("THRESHOLD", BMLIB_LOG_ERROR, "Not supported image data type");
        return BM_NOT_SUPPORTED;
    }
    if (src_h != dst_h || src_w != dst_w) {
        bmlib_log("THRESHOLD", BMLIB_LOG_ERROR, "inputs and output image size should be same");
        return BM_ERR_PARAM;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_threshold(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        unsigned char thresh,
        unsigned char max_value,
        bm_thresh_type_t type) {
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_threshold_check(handle, input, output, type);
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

    bm_api_cv_threshold_t api;
    api.channel = channel;
    api.thresh = thresh;
    api.max_value = max_value;
    api.type = type;
    for (int i = 0; i < channel; i++) {
        api.input_addr[i] = bm_mem_get_device_addr(input_mem[i]);
        api.output_addr[i] = bm_mem_get_device_addr(output_mem[i]);
        api.width[i] = input.image_private->memory_layout[i].W;
        api.height[i] = input.image_private->memory_layout[i].H;
        api.input_str[i] = input_str[i];
        api.output_str[i] = output_str[i];
    }
    // if rgb-planar, its channel is 3
    if (input.image_format == FORMAT_RGB_PLANAR ||
        input.image_format == FORMAT_BGR_PLANAR) {
        api.height[0] *= 3;
    }

    unsigned int chipid;
    bm_get_chipid(handle, &chipid);

    switch (chipid)
    {
        case 0x1684:
            ret = bm_send_api(handle,  BM_API_ID_CV_THRESHOLD, (uint8_t*)&api, sizeof(api));
            if (BM_SUCCESS != ret) {
                bmlib_log("THRESHOLD", BMLIB_LOG_ERROR, "threshold send api error\n");
                if (output_alloc_flag) {
                    for (int i = 0; i < channel; i++) {
                        bm_free_device(handle, output_mem[i]);
                    }
                }
                return ret;
            }
            ret = bm_sync_api(handle);
            if (BM_SUCCESS != ret) {
                bmlib_log("THRESHOLD", BMLIB_LOG_ERROR, "threshold sync api error\n");
                if (output_alloc_flag) {
                    for (int i = 0; i < channel; i++) {
                        bm_free_device(handle, output_mem[i]);
                    }
                }
                return ret;
            }
            break;

        case BM1684X:
            BM_CHECK_RET(bm_tpu_kernel_launch(handle, "cv_threshold", &api, sizeof(api)));
            break;

        default:
            printf("ChipID is NOT supported\n");
            break;
    }

    return BM_SUCCESS;
}

