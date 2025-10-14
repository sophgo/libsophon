#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"
#include <stdio.h>
#include <memory>

bm_status_t bmcv_rotate_check(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int rotation_angle) {
    if (handle == NULL) {
        bmlib_log("ROTATE", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    bm_image_format_ext src_format = input.image_format;
    bm_image_data_format_ext src_type = input.data_type;
    bm_image_format_ext dst_format = output.image_format;
    bm_image_data_format_ext dst_type = output.data_type;

    if (src_format != dst_format) {
        bmlib_log("ROTATE", BMLIB_LOG_ERROR, "input and output image format is NOT same\n");
        return BM_ERR_PARAM;
    }
    if (src_type != DATA_TYPE_EXT_1N_BYTE ||
        dst_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("ROTATE", BMLIB_LOG_ERROR, "Not supported image data type\n");
        return BM_NOT_SUPPORTED;
    }
    if(input.width > 8192 || input.height > 8192) {
        printf("Unsupported size : size_max = 8192 x 8192\n");
        return BM_NOT_SUPPORTED;
    }
    if(input.width < 8 || input.height < 8) {
        printf("Unsupported size : size_min = 8 x 8\n");
        return BM_NOT_SUPPORTED;
    }
    if(rotation_angle != 90 && rotation_angle != 180 && rotation_angle != 270) {
        bmlib_log("ROTATE", BMLIB_LOG_ERROR, "Not supported rotation_angle\n");
        return BM_NOT_SUPPORTED;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_rotate2(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int rotation_angle) {
    bm_status_t ret = BM_SUCCESS;
    bm_handle_check_2(handle, input, output);
    ret = bmcv_rotate_check(handle, input, output, rotation_angle);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    sg_api_cv_rotate_t api;
    bool out_mem_alloc_flag = 0;
    if (!bm_image_is_attached(output)) {
        if (BM_SUCCESS != bm_image_alloc_dev_mem(output)) {
            BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");
            return BM_ERR_FAILURE;
        }
        out_mem_alloc_flag = 1;
    }
    bm_device_mem_t input_mem[3], output_mem[3];
    bm_image_get_device_mem(output, output_mem);
    bm_image_get_device_mem(input, input_mem);
    int channel = bm_image_get_plane_num(input);
    if(input.image_format == FORMAT_RGB_PLANAR || input.image_format == FORMAT_BGR_PLANAR) {
        channel = 3;
    }
    api.channel = channel;
    api.rotation_angle = rotation_angle;
    for (int i = 0; i < channel; i++) {
        api.input_addr[i] = bm_mem_get_device_addr(input_mem[i]);
        api.output_addr[i] = bm_mem_get_device_addr(output_mem[i]);
    }

    api.width = input.width;
    api.height = input.height;

    unsigned int chipid;
    bm_get_chipid(handle, &chipid);

    switch (chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_rotate", (u8 *)&api, sizeof(api));
            if(BM_SUCCESS != ret){
                bmlib_log("ROTATE", BMLIB_LOG_ERROR, "rotate sync api error\n");
                if (out_mem_alloc_flag) {
                    for(int i = 0; i < channel; i++) {
                        bm_free_device(handle, output_mem[i]);
                    }
                }
                return BM_ERR_FAILURE;
            }
            break;

        default:
            printf("ChipID is NOT supported\n");
            break;
    }

    return BM_SUCCESS;
}

bm_status_t bmcv_image_rotate(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int rotation_angle) {
    bm_status_t ret = BM_SUCCESS;
    bm_image temp_in, temp_out;
    ret = bmcv_rotate_check(handle, input, output, rotation_angle);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    switch(input.image_format) {
        case FORMAT_GRAY:
        case FORMAT_RGBP_SEPARATE:
        case FORMAT_BGRP_SEPARATE:
        case FORMAT_YUV444P:
            ret = bmcv_image_rotate2(handle, input, output, rotation_angle);
            break;
        default:
            bm_image_create(handle, input.height, input.width, FORMAT_RGBP_SEPARATE, DATA_TYPE_EXT_1N_BYTE, &temp_in, NULL);
            bm_image_create(handle, output.height, output.width, FORMAT_RGBP_SEPARATE, DATA_TYPE_EXT_1N_BYTE, &temp_out, NULL);
            bm_image_alloc_dev_mem(temp_in, BMCV_HEAP_ANY);
            bm_image_alloc_dev_mem(temp_out, BMCV_HEAP_ANY);
            ret = bmcv_image_vpp_csc_matrix_convert(handle, 1, input, &temp_in, CSC_MAX_ENUM, NULL, BMCV_INTER_LINEAR, NULL);  //NV12->RGBP
            if (ret != BM_SUCCESS) {
                printf("input vpp_csc_matrix_convert error\n");
                bm_image_destroy(temp_in);
                bm_image_destroy(temp_out);
                return ret;
            }
            ret = bmcv_image_rotate2(handle,temp_in,temp_out,rotation_angle);
            if (ret != BM_SUCCESS) {
                printf("bmcv_image_rotate2 error\n");
                bm_image_destroy(temp_in);
                bm_image_destroy(temp_out);
                return ret;
            }
            ret = bmcv_image_vpp_csc_matrix_convert(handle, 1, temp_out, &output, CSC_MAX_ENUM, NULL, BMCV_INTER_LINEAR, NULL);  //RGBP->NV12
            if (ret != BM_SUCCESS) {
                printf("output vpp_csc_matrix_convert error\n");
                bm_image_destroy(temp_in);
                bm_image_destroy(temp_out);
                return ret;
            }
            bm_image_destroy(temp_in);
            bm_image_destroy(temp_out);
            break;
    }

    return ret;
}