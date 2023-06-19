#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bmlib_runtime.h"
#include <memory>
#include <vector>

static bm_status_t bmcv_absdiff_check(
        bm_handle_t handle,
        bm_image input1,
        bm_image input2,
        bm_image output) {
    if (handle == NULL) {
        bmlib_log("ABSDIFF", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }

    bm_image_format_ext src1_format = input1.image_format;
    bm_image_data_format_ext src1_type = input1.data_type;
    bm_image_format_ext src2_format = input2.image_format;
    bm_image_data_format_ext src2_type = input2.data_type;
    bm_image_format_ext dst_format = output.image_format;
    bm_image_data_format_ext dst_type = output.data_type;
    int src1_h = input1.height;
    int src1_w = input1.width;
    int src2_h = input2.height;
    int src2_w = input2.width;
    int dst_h = output.height;
    int dst_w = output.width;
    if (src1_format != src2_format &&
        src1_format != dst_format) {
        bmlib_log("ABSDIFF", BMLIB_LOG_ERROR, "input and output image format is NOT same");
        return BM_ERR_PARAM;
    }
    if (src1_format != FORMAT_YUV420P &&
        src1_format != FORMAT_YUV422P &&
        src1_format != FORMAT_YUV444P &&
        src1_format != FORMAT_NV12 &&
        src1_format != FORMAT_NV21 &&
        src1_format != FORMAT_NV61 &&
        src1_format != FORMAT_NV16 &&
        src1_format != FORMAT_NV24 &&
        src1_format != FORMAT_GRAY &&
        src1_format != FORMAT_RGB_PLANAR &&
        src1_format != FORMAT_BGR_PLANAR &&
        src1_format != FORMAT_RGB_PACKED &&
        src1_format != FORMAT_BGR_PACKED &&
        src1_format != FORMAT_RGBP_SEPARATE &&
        src1_format != FORMAT_BGRP_SEPARATE) {
        bmlib_log("ABSDIFF", BMLIB_LOG_ERROR, "Not supported image format");
        return BM_NOT_SUPPORTED;
    }
    if (src1_type != DATA_TYPE_EXT_1N_BYTE ||
        src2_type != DATA_TYPE_EXT_1N_BYTE ||
        dst_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("ABSDIFF", BMLIB_LOG_ERROR, "Not supported image data type");
        return BM_NOT_SUPPORTED;
    }
    if (src1_h != src2_h || src1_w != src2_w ||
        src1_h != dst_h || src1_w != dst_w) {
        bmlib_log("ABSDIFF", BMLIB_LOG_ERROR, "inputs and output image size should be same");
        return BM_ERR_PARAM;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_absdiff(
        bm_handle_t handle,
        bm_image input1,
        bm_image input2,
        bm_image output) {
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_absdiff_check(handle, input1, input2, output);
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
    int channel = bm_image_get_plane_num(input1);
    int input1_str[3], input2_str[3], output_str[3];
    bm_image_get_stride(input1, input1_str);
    bm_image_get_stride(input2, input2_str);
    bm_image_get_stride(output, output_str);
    bm_device_mem_t input1_mem[3];
    bm_image_get_device_mem(input1, input1_mem);
    bm_device_mem_t input2_mem[3];
    bm_image_get_device_mem(input2, input2_mem);
    bm_device_mem_t output_mem[3];
    bm_image_get_device_mem(output, output_mem);
    bm_api_cv_absdiff_t api;
    api.channel = channel;
    for (int i = 0; i < channel; i++) {
        api.input1_addr[i] = bm_mem_get_device_addr(input1_mem[i]);
        api.input2_addr[i] = bm_mem_get_device_addr(input2_mem[i]);
        api.output_addr[i] = bm_mem_get_device_addr(output_mem[i]);
        api.width[i] = input1.image_private->memory_layout[i].W;
        api.height[i] = input1.image_private->memory_layout[i].H;
        api.input1_str[i] = input1_str[i];
        api.input2_str[i] = input2_str[i];
        api.output_str[i] = output_str[i];
    }
    // if rgb-planar, its channel is 3
    if (input1.image_format == FORMAT_RGB_PLANAR ||
        input1.image_format == FORMAT_BGR_PLANAR) {
        api.height[0] *= 3;
    }

    unsigned int chipid;
    bm_get_chipid(handle, &chipid);

    switch(chipid)
    {
        case 0x1684:
            ret = bm_send_api(handle, BM_API_ID_CV_ABSDIFF, (uint8_t*)&api, sizeof(api));
            if (BM_SUCCESS != ret) {
                bmlib_log("ABSDIFF", BMLIB_LOG_ERROR, "absdiff send api error\n");
                if (output_alloc_flag) {
                    for (int i = 0; i < channel; i++) {
                        bm_free_device(handle, output_mem[i]);
                    }
                }
                return ret;
            }
            ret = bm_sync_api(handle);
            if (BM_SUCCESS != ret) {
                bmlib_log("ABSDIFF", BMLIB_LOG_ERROR, "absdiff sync api error\n");
                if (output_alloc_flag) {
                    for (int i = 0; i < channel; i++) {
                        bm_free_device(handle, output_mem[i]);
                    }
                }
            return ret;
            }
            break;

        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_absdiff", (u8 *)&api, sizeof(api));
            if(BM_SUCCESS != ret){
                bmlib_log("ABSDIFF", BMLIB_LOG_ERROR, "absdiff sync api error\n");
                if (output_alloc_flag) {
                    for (int i = 0; i < channel; i++) {
                        bm_free_device(handle, output_mem[i]);
                    }
                }
            }
            // tpu_kernel_launch_sync(handle, "cv_absdiff", &api, sizeof(api));
            break;

        default:
            printf("BM_NOT_SUPPORTED!\n");
            break;
    }

    return BM_SUCCESS;
}