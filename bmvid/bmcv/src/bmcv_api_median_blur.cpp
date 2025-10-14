#include "stdlib.h"
#include "bmcv_internal.h"
#include "bmcv_api_ext.h"

typedef struct sg_api_cv_pad0 {
    unsigned long long input_data_global_addr;
    unsigned long long padded_input_data_global_addr;
    int channel;
    int width;
    int height;
    int ksize;
}__attribute__((packed)) sg_api_cv_pad0_t;

static bm_status_t bmcv_median_blur_check(bm_handle_t handle, int width, int height, int format, int ksize) {
    if (handle == NULL) {
        bmlib_log("median_blur", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    if (ksize > 9) {
        bmlib_log("median_blur", BMLIB_LOG_ERROR, "The kernel max size: 9!\n");
        return BM_ERR_PARAM;
    }
    if (ksize != 3 && ksize != 5 && ksize != 7 && ksize != 9) {
        bmlib_log("median_blur", BMLIB_LOG_ERROR, "The kernel size only supports 3, 5, 7, 9!\n");
        return BM_ERR_PARAM;
    }
    if ((width > 4096) || (height > 3000)) {
        bmlib_log("median_blur", BMLIB_LOG_ERROR, "image max_size: 4096 * 3000!\r\n");
        return BM_ERR_PARAM;
    }
    if (format != FORMAT_GRAY &&
        format != FORMAT_RGBP_SEPARATE &&
        format != FORMAT_BGRP_SEPARATE &&
        format != FORMAT_YUV444P) {
        bmlib_log("median_blur", BMLIB_LOG_ERROR, "image format only supports FORMAT_GRAY, FORMAT_RGBP_SEPARATE, FORMAT_BGRP_SEPARATE and FORMAT_YUV444P\r\n");
        return BM_ERR_PARAM;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_median_blur(bm_handle_t handle, bm_device_mem_t input_data_global_addr, bm_device_mem_t padded_input_data_global_addr,
                                   unsigned char *output, int width, int height, int format, int ksize) {
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_median_blur_check(handle, width, height, format, ksize);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    int channel = format == FORMAT_GRAY ? 1 : 3;
    int padd_width = ksize - 1 + width;
    int padd_height = ksize - 1 + height;
    unsigned int chipid;
    sg_api_cv_pad0_t api;
    api.input_data_global_addr = bm_mem_get_device_addr(input_data_global_addr);
    api.padded_input_data_global_addr = bm_mem_get_device_addr(padded_input_data_global_addr);
    api.channel = channel;
    api.ksize = ksize;
    api.width = width;
    api.height = height;

    ret = bm_get_chipid(handle, &chipid);
    switch (chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_pad0", &api, sizeof(api));
            if (BM_SUCCESS != ret) {
                bmlib_log("median_blur", BMLIB_LOG_ERROR, "median_blur sync api error\n");
                return ret;
            }
            break;
        default:
            ret = BM_NOT_SUPPORTED;
            break;
    }
    int edge = ksize / 2;
    const int GRAY_LEVELS = 256;
    unsigned char *padded_input = (unsigned char *)malloc(padd_width * padd_height * channel);
    if (padded_input == NULL) {
        bmlib_log("median_blur", BMLIB_LOG_ERROR, "Failed to allocate memory for padded_input\n");
        return BM_ERR_PARAM;
    }
    unsigned char *histogram = (unsigned char *)malloc(sizeof(unsigned char) * GRAY_LEVELS);
    if (histogram == NULL) {
        bmlib_log("median_blur", BMLIB_LOG_ERROR, "Failed to allocate memory for histogram\n");
        free(padded_input);
        return BM_ERR_PARAM;
    }
    ret = bm_memcpy_d2s(handle, bm_mem_get_system_addr(bm_mem_from_system(padded_input)), padded_input_data_global_addr);
    if(ret != BM_SUCCESS) {
        bmlib_log("median_blur", BMLIB_LOG_ERROR, "bm_memcpy_d2s padded_input_data_global_addr to padded_input error\n");
        free(padded_input);
        free(histogram);
        return BM_ERR_PARAM;
    }
    for (int c = 0; c < channel; c++) {
        int channel_offset = c * padd_width * padd_height;
        for (int y = edge; y < padd_height - edge; y++) {
            for (int x = edge; x < padd_width - edge; x++) {
                memset(histogram, 0, sizeof(unsigned char) * GRAY_LEVELS);
                int window_area_idx = 0;
                for (int j = -edge; j <= edge; j++) {
                    int idx_y = y + j;
                    for (int i = -edge; i <= edge; i++) {
                        int idx_x = x + i;
                        unsigned char pixel = padded_input[idx_y * padd_width + idx_x + channel_offset];
                        histogram[pixel]++;
                        window_area_idx++;
                    }
                }
                int medianPos = (window_area_idx - 1) / 2;
                unsigned char count = 0;
                unsigned char median = 0;
                for (int k = 0; k < GRAY_LEVELS; k++) {
                    count += histogram[k];
                    if (count > medianPos) {
                        median = (unsigned char)k;
                        break;
                    }
                }
                output[(y - edge) * width + (x - edge) + c * width * height] = median;
            }
        }
    }
    free(padded_input);
    free(histogram);
    return ret;
}