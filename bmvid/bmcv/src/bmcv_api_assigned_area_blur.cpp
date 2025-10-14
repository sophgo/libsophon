#include <math.h>
#include <stdio.h>
#include "bmcv_api_ext_c.h"
#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"
#include "bmlib_runtime.h"

bm_status_t assigned_area_blur_check(bm_handle_t handle, bm_image input, bm_image output, int ksize,
                                     int assigned_area_num, float center_weight_scale, bmcv_rect_t *assigned_area) {
    if (handle == NULL) {
        bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    bm_image_format_ext src_format = input.image_format;
    bm_image_format_ext dst_format = output.image_format;
    bm_image_data_format_ext src_type = input.data_type;
    bm_image_data_format_ext dst_type = output.data_type;
    if (src_format != dst_format) {
        bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "Input and output image format should be same!\n");
        return BM_ERR_PARAM;
    }
    if (input.width != output.width) {
        bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "Input and output width should be same!\n");
        return BM_ERR_PARAM;
    }
    if (input.height != output.height) {
        bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "Input and output height should be same!\n");
        return BM_ERR_PARAM;
    }
    if (src_format != FORMAT_YUV420P &&
        src_format != FORMAT_YUV444P &&
        src_format != FORMAT_BGR_PLANAR &&
        src_format != FORMAT_RGB_PLANAR &&
        src_format != FORMAT_RGBP_SEPARATE &&
        src_format != FORMAT_BGRP_SEPARATE &&
        src_format != FORMAT_GRAY) {
        bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "Unsupported input format!\n");
        return BM_ERR_PARAM;
    }
    if (src_type != DATA_TYPE_EXT_1N_BYTE ||
        dst_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "Unsupported image data type!\n");
        return BM_NOT_SUPPORTED;
    }
    if(input.width > 4096 || input.height > 4096) {
        bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "Original image unsupported size : size_max = 4096 x 4096!\n");
        return BM_NOT_SUPPORTED;
    }
    if(input.width < 8 || input.height < 8) {
        bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "Original image unsupported size : size_min = 8 x 8!\n");
        return BM_NOT_SUPPORTED;
    }
    if ((ksize != 3) && (ksize != 5) && (ksize != 7) && (ksize != 9) && (ksize != 11)) {
        bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "Ksize only support 3, 5, 7, 9, 11!\n");
        return BM_ERR_PARAM;
    }
    if (assigned_area_num < 1 || assigned_area_num > 200) {
        bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "Unsupported assigned_area_num, valid value range:1~100!\n");
        return BM_ERR_PARAM;
    }
    if (center_weight_scale < 0 || center_weight_scale > 1) {
        bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "Unsupported center_weight_scale, valid value range:0~1!\n");
        return BM_ERR_PARAM;
    }
    for(int i = 0; i < assigned_area_num; i++) {
        if (assigned_area[i].start_x < 0 || (assigned_area[i].start_x > (input.width - assigned_area[i].crop_w))) {
            bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "Unsupported assigned_area[%d].start_x, valid value range:0~input.width-assigned_area[%d].crop_w!\n", i, i);
            return BM_ERR_PARAM;
        }
        if (assigned_area[i].start_y < 0 || (assigned_area[i].start_y > (input.height - assigned_area[i].crop_h))) {
            bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "Unsupported assigned_area[%d].start_y, valid value range:0~input.height-assigned_area[%d].crop_h!\n", i, i);
            return BM_ERR_PARAM;
        }
        if (assigned_area[i].crop_w < 1 || assigned_area[i].crop_w > input.width) {
            bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "Unsupported assigned_area[%d].crop_w, valid value range:1~input.width!\n", i);
            return BM_ERR_PARAM;
        }
        if (assigned_area[i].crop_h < 1 || assigned_area[i].crop_h > input.height) {
            bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "Unsupported assigned_area[%d].crop_h, valid value range:1~input.height!\n", i);
            return BM_ERR_PARAM;
        }
    }
    return BM_SUCCESS;
}

static void create_fp16_kernel(fp16* kernel, int ksize, float center_weight_scale) {
    int half = ksize / 2;
    float max_distance = sqrt(2) * half;
    float weight_sum = 0.0f;
    float* fp32_kernel = (float *)malloc(ksize * ksize * 4);
    for (int i = 0; i < ksize; i++) {
        for (int j = 0; j < ksize; j++) {
            int x = i - half;
            int y = j - half;
            float distance = sqrt((float)(x * x + y * y));

            float weight = distance / max_distance;

            if (distance <= half * 0.5f) {
                weight *= center_weight_scale;
            }

            fp32_kernel[i * ksize + j] = weight;
            weight_sum += weight;
        }
    }
    for (int i = 0; i < ksize * ksize; i++) {
        fp32_kernel[i] /= weight_sum;
        kernel[i] = fp32tofp16(fp32_kernel[i], 1);
    }
    free(fp32_kernel);
}

bm_status_t bmcv_image_assigned_area_blur2(bm_handle_t handle, bm_image input, bm_image output, int ksize,
                                          int assigned_area_num, float center_weight_scale, bmcv_rect_t *assigned_area) {
    bm_status_t ret;
    fp16 *kernel = (fp16*)malloc(ksize * ksize * 2);
    bm_device_mem_t kernel_mem;
    create_fp16_kernel(kernel, ksize, center_weight_scale);
    bm_malloc_device_byte(handle, &kernel_mem, ksize * ksize * 2);
    ret = bm_memcpy_s2d(handle, kernel_mem, kernel);
    if (BM_SUCCESS != ret) {
        bm_free_device(handle, kernel_mem);
        free(kernel);
        return ret;
    }
    bm_device_mem_t input_mem[3], output_mem[3];
    bm_image_get_device_mem(input, input_mem);
    bm_image_get_device_mem(output, output_mem);

    sg_api_cv_assigned_area_blur_t api;
    int channel;
    api.base_width = input.width;
    api.assigned_area_num = assigned_area_num;
    for (int i = 0; i < assigned_area_num; i++) {
        api.assigned_area_width[i] = assigned_area[i].crop_w;
        api.assigned_area_height[i] = assigned_area[i].crop_h;
        api.start_x[i] = assigned_area[i].start_x;
        api.start_y[i] = assigned_area[i].start_y;
    }

    if (input.image_format == FORMAT_GRAY) {
        channel = 1;
    } else {
        channel = 3;
    }
    if (input.image_format == FORMAT_RGB_PLANAR || input.image_format == FORMAT_BGR_PLANAR) {
        for (int i = 0; i < channel; i++) {
            api.base_addr[i] = bm_mem_get_device_addr(input_mem[0]) + input.height * input.width * i;
            api.output_addr[i] = bm_mem_get_device_addr(output_mem[0]) + input.height * input.width * i;
        }
    } else {
        for (int i = 0; i < channel; i++) {
            api.base_addr[i] = bm_mem_get_device_addr(input_mem[i]);
            api.output_addr[i] = bm_mem_get_device_addr(output_mem[i]);
        }
    }
    api.ksize = ksize;
    api.channel = channel;
    api.kernel_sys_addr = bm_mem_get_device_addr(kernel_mem);
    unsigned int chipid;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret) {
        bm_free_device(handle, kernel_mem);
        free(kernel);
        return ret;
    }

    switch (chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_assigned_area_blur", (u8 *)&api, sizeof(api));
            if (BM_SUCCESS != ret) {
                bmlib_log("ASSIGNED AREA BLUR", BMLIB_LOG_ERROR, "sync api error!\n");
                bm_free_device(handle, kernel_mem);
                free(kernel);
                return BM_ERR_FAILURE;
            }
            break;
        default:
            printf("Chipid is not supported!\n");
            break;
    }
    bm_free_device(handle, kernel_mem);
    free(kernel);
    return ret;
}

bm_status_t bmcv_image_assigned_area_blur(bm_handle_t handle, bm_image input, bm_image output, int ksize,
                                          int assigned_area_num, float center_weight_scale, bmcv_rect_t *assigned_area) {
    bm_status_t ret;
    ret = assigned_area_blur_check(handle, input, output, ksize, assigned_area_num, center_weight_scale, assigned_area);
    if (BM_SUCCESS != ret) {
        printf("bmcv_image_assigned_area_blur_check failed!\n");
        return ret;
    }
    bm_image img_rgb_input, img_rgb_output;
    if(input.image_format == FORMAT_YUV420P) {
        bm_image_create(handle, input.height, input.width, FORMAT_RGBP_SEPARATE, DATA_TYPE_EXT_1N_BYTE, &img_rgb_input, NULL);
        bm_image_create(handle, output.height, output.width, FORMAT_RGBP_SEPARATE, DATA_TYPE_EXT_1N_BYTE, &img_rgb_output, NULL);
        bm_image_alloc_dev_mem(img_rgb_input, 2);
        bm_image_alloc_dev_mem(img_rgb_output, 2);
        ret = bmcv_image_vpp_csc_matrix_convert(handle, 1, input, &img_rgb_input, CSC_YCbCr2RGB_BT709, NULL, BMCV_INTER_LINEAR, NULL);
        if (ret != 0) {
            printf("bmcv_image_vpp_csc_matrix_convert yuv to rgb failed\n");
            bm_image_destroy(img_rgb_input);
            bm_image_destroy(img_rgb_output);
            return ret;
        }
        ret = bmcv_image_vpp_csc_matrix_convert(handle, 1, input, &img_rgb_output, CSC_YCbCr2RGB_BT709, NULL, BMCV_INTER_LINEAR, NULL);
        if (ret != 0) {
            printf("bmcv_image_vpp_csc_matrix_convert yuv to rgb failed\n");
            bm_image_destroy(img_rgb_input);
            bm_image_destroy(img_rgb_output);
            return ret;
        }
        ret = bmcv_image_assigned_area_blur2(handle, img_rgb_input, img_rgb_output, ksize, assigned_area_num, center_weight_scale, assigned_area);
        if (ret != 0) {
            printf("bmcv_image_assigned_area_blur2 failed\n");
            bm_image_destroy(img_rgb_input);
            bm_image_destroy(img_rgb_output);
            return ret;
        }
    } else {
        ret = bmcv_image_assigned_area_blur2(handle, input, output, ksize, assigned_area_num, center_weight_scale, assigned_area);
        if (ret != 0) {
            printf("bmcv_image_assigned_area_blur2 failed\n");
            bm_image_destroy(img_rgb_input);
            bm_image_destroy(img_rgb_output);
            return ret;
        }
    }
    if (input.image_format == FORMAT_YUV420P) {
        ret = bmcv_image_vpp_csc_matrix_convert(handle, 1, img_rgb_output, &output, CSC_RGB2YCbCr_BT709, NULL, BMCV_INTER_LINEAR, NULL);
        if (ret != 0) {
            printf("bmcv_image_vpp_csc_matrix_convert rgb to yuv failed\n");
            bm_image_destroy(img_rgb_input);
            bm_image_destroy(img_rgb_output);
            return ret;
        }
    }
    return ret;
}