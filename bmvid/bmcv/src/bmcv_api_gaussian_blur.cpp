#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bmcv_bm1684x.h"
#include <memory>
#include <float.h>
#include <stdio.h>
#include <math.h>

static int get_gaussian_sep_kernel(int n, float sigma, float *k_sep) {
    const int SMALL_GAUSSIAN_SIZE = 3;
    static const float small_gaussian_tab[3] = {0.25f, 0.5f, 0.25f};
    const float* fixed_kernel = n % 2 == 1 && n <= SMALL_GAUSSIAN_SIZE && sigma <= 0 ? small_gaussian_tab : 0;
    float sigmaX = sigma > 0 ? sigma : ((n - 1) * 0.5 - 1) * 0.3 + 0.8;
    float scale2X = -0.5 / (sigmaX * sigmaX);
    float sum = 0;
    int i;

    for (i = 0; i < n; i++) {
        float x = i - (n - 1) * 0.5;
        float t = fixed_kernel ? fixed_kernel[i] : exp(scale2X * x * x);
        k_sep[i] = t;
        sum += k_sep[i];
    }
    sum = 1./sum;
    for (i = 0; i < n; i++) {
        k_sep[i] = k_sep[i] * sum;
    }
    return 0;
}

static void create_gaussian_kernel(float* kernel, int kw, int kh, float sigma1, float sigma2) {
    float* k_sep_x = (float* )malloc(sizeof(float) * kw);
    float* k_sep_y = (float* )malloc(sizeof(float) * kh);

    if(sigma2 <= 0) sigma2 = sigma1;
    // automatic detection of kernel size from sigma
    if (kw <= 0 && sigma1 > 0 ) kw = (int)round(sigma1 * 3 * 2 + 1) | 1;
    if (kh <= 0 && sigma2 > 0 ) kh = (int)round(sigma2 * 3 * 2 + 1) | 1;
    sigma1 = sigma1 < 0 ? 0 : sigma1;
    sigma2 = sigma2 < 0 ? 0 : sigma2;
    get_gaussian_sep_kernel(kw, sigma1, k_sep_x);
    if (kh == kw && abs(sigma1 - sigma2) < DBL_EPSILON) {
        get_gaussian_sep_kernel(kw, sigma1, k_sep_y);
    } else {
        get_gaussian_sep_kernel(kh, sigma2, k_sep_y);
    }
    for (int i = 0; i < kh; i++) {
        for (int j = 0; j < kw; j++) {
            kernel[i * kw + j] = k_sep_y[i] * k_sep_x[j];
        }
    }
    free(k_sep_x);
    free(k_sep_y);
}


static bm_status_t bmcv_gaussian_blur_check_bm1684(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int kw,
        int kh) {
    if (handle == NULL) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    if (kw > 31 || kh > 31) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "The kernel size must be not greater than 31\n");
        return BM_ERR_PARAM;
    }
    bm_image_format_ext src_format = input.image_format;
    bm_image_data_format_ext src_type = input.data_type;
    bm_image_format_ext dst_format = output.image_format;
    bm_image_data_format_ext dst_type = output.data_type;
    int image_sh = input.height;
    int image_sw = input.width;
    int image_dh = output.height;
    int image_dw = output.width;
    if (image_sw + kw - 1 >= 2048) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "image width is too large!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (src_format != FORMAT_RGB_PLANAR &&
        src_format != FORMAT_BGR_PLANAR &&
        src_format != FORMAT_RGB_PACKED &&
        src_format != FORMAT_BGR_PACKED &&
        src_format != FORMAT_BGRP_SEPARATE &&
        src_format != FORMAT_RGBP_SEPARATE &&
        src_format != FORMAT_YUV420P &&
        src_format != FORMAT_YUV422P &&
        src_format != FORMAT_YUV444P &&
        src_format != FORMAT_GRAY) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "Not supported input image format!\n");
        return BM_NOT_SUPPORTED;
    }
    if (dst_format != src_format) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "input and output image format should be same!\n");
        return BM_NOT_SUPPORTED;
    }
    if (src_type != DATA_TYPE_EXT_1N_BYTE ||
        dst_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "Not supported image data type\n");
        return BM_NOT_SUPPORTED;
    }
    if (image_sh != image_dh || image_sw != image_dw) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "input and output image size should be same\n");
        return BM_NOT_SUPPORTED;
    }
    return BM_SUCCESS;
}

static bm_status_t bmcv_gaussian_blur_check_bm1684x(bm_handle_t handle, bm_image input, bm_image output,
                                            int kw, int kh) {
    if (handle == NULL) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    if (kw > 7 || kh > 7) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "The kernel max size: 7!\n");
        return BM_ERR_PARAM;
    }
    if (kw != 3 && kw != 5 && kw != 7) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "The kernel size only support 3, 5, 7!\n");
        return BM_ERR_PARAM;
    }
    if (kh != 3 && kh != 5 && kh != 7) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "The kernel size only support 3, 5, 7!\n");
        return BM_ERR_PARAM;
    }
    if (kw != kh) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "The kernel size onlt support 3*3, 5*5, 7*7!\n");
        return BM_ERR_PARAM;
    }
    bm_image_format_ext src_format = input.image_format;
    bm_image_data_format_ext src_type = input.data_type;
    bm_image_format_ext dst_format = output.image_format;
    bm_image_data_format_ext dst_type = output.data_type;
    int image_sh = input.height;
    int image_sw = input.width;
    int image_dh = output.height;
    int image_dw = output.width;

    if ((kw == 3) && (image_sw > 8192)) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "when ksize = 3, image max_width: 8192!\r\n");
        return BM_ERR_PARAM;
    }
    if ((kw == 5) && (image_sw > 4096)) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "when ksize = 5, image max_width: 4096!\r\n");
        return BM_ERR_PARAM;
    }
    if ((kw == 7) && (image_sw > 2048)) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "when ksize = 7, image max_width: 2048!\r\n");
        return BM_ERR_PARAM;
    }
    if (src_format != FORMAT_RGB_PLANAR &&
        src_format != FORMAT_BGR_PLANAR &&
        src_format != FORMAT_RGB_PACKED &&
        src_format != FORMAT_BGR_PACKED &&
        src_format != FORMAT_BGRP_SEPARATE &&
        src_format != FORMAT_RGBP_SEPARATE &&
        src_format != FORMAT_GRAY) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "Not supported input image format!\n");
        return BM_NOT_SUPPORTED;
    }
    if (dst_format != src_format) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "input and output image format should be same!\n");
        return BM_NOT_SUPPORTED;
    }
    if (src_type != DATA_TYPE_EXT_1N_BYTE ||
        dst_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "Not supported image data type\n");
        return BM_NOT_SUPPORTED;
    }
    if (image_sh != image_dh || image_sw != image_dw) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "input and output image size should be same\n");
        return BM_NOT_SUPPORTED;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_gaussian_blur_bm1684(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int kw,
        int kh,
        float sigmaX,
        float sigmaY) {
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_gaussian_blur_check_bm1684(handle, input, output, kw, kh);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    float* kernel = new float [kw * kh];
    create_gaussian_kernel(kernel, kw, kh, sigmaX, sigmaY);
    bm_device_mem_t kernel_mem;
    ret = bm_malloc_device_byte(handle, &kernel_mem, kw * kh * sizeof(float));
    if (BM_SUCCESS != ret) {
        delete [] kernel;
        return ret;
    }
    ret = bm_memcpy_s2d(handle, kernel_mem, kernel);
    if (BM_SUCCESS != ret) {
        bm_free_device(handle, kernel_mem);
        delete [] kernel;
        return ret;
    }
    bool output_alloc_flag = false;
    if (!bm_image_is_attached(output)) {
        ret = bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY);
        if (ret != BM_SUCCESS) {
            bm_free_device(handle, kernel_mem);
            delete [] kernel;
            return ret;
        }
        output_alloc_flag = true;
    }
    // construct and send api
    int stride_i[3], stride_o[3];
    bm_image_get_stride(input, stride_i);
    bm_image_get_stride(output, stride_o);
    bm_device_mem_t input_mem[3];
    bm_image_get_device_mem(input, input_mem);
    bm_device_mem_t output_mem[3];
    bm_image_get_device_mem(output, output_mem);
    int channel = bm_image_get_plane_num(input);
    bm_api_cv_filter_t api;
    api.channel = channel;
    api.kernel_addr = bm_mem_get_device_addr(kernel_mem);
    api.kh = kh;
    api.kw = kw;
    api.delta = 0;
    api.is_packed = (input.image_format == FORMAT_RGB_PACKED ||
                     input.image_format == FORMAT_BGR_PACKED);
    api.out_type = 0;   // 0-uint8  1-uint16
    for (int i = 0; i < channel; i++) {
        api.input_addr[i] = bm_mem_get_device_addr(input_mem[i]);
        api.output_addr[i] = bm_mem_get_device_addr(output_mem[i]);
        api.width[i] = input.image_private->memory_layout[i].W / (api.is_packed ? 3 : 1);
        api.height[i] = input.image_private->memory_layout[i].H;
        api.stride_i[i] = stride_i[i];
        api.stride_o[i] = stride_o[i];
    }
    if (input.image_format == FORMAT_RGB_PLANAR ||
        input.image_format == FORMAT_BGR_PLANAR) {
        api.channel = 3;
        api.stride_i[1] = api.stride_i[0];
        api.stride_i[2] = api.stride_i[0];
        api.stride_o[1] = api.stride_o[0];
        api.stride_o[2] = api.stride_o[0];
        api.width[1] = api.width[0];
        api.width[2] = api.width[0];
        api.height[1] = api.height[0];
        api.height[2] = api.height[0];
        api.input_addr[1] = api.input_addr[0] + api.height[0] * api.stride_i[0];
        api.input_addr[2] = api.input_addr[0] + api.height[0] * api.stride_i[0] * 2;
        api.output_addr[1] = api.output_addr[0] + api.height[0] * api.stride_i[0];
        api.output_addr[2] = api.output_addr[0] + api.height[0] * api.stride_i[0] * 2;
    }
    ret = bm_send_api(handle,  BM_API_ID_CV_FILTER, (uint8_t*)&api, sizeof(api));
    if (BM_SUCCESS != ret) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "gaussian_blur send api error\n");
        bm_free_device(handle, kernel_mem);
        if (output_alloc_flag) {
            bm_free_device(handle, output_mem[0]);
        }
        delete [] kernel;
        return ret;
    }
    ret = bm_sync_api(handle);
    if (BM_SUCCESS != ret) {
        bmlib_log("GAUSSIAN_BLUR", BMLIB_LOG_ERROR, "gaussian_blur sync api error\n");
        bm_free_device(handle, kernel_mem);
        if (output_alloc_flag) {
            bm_free_device(handle, output_mem[0]);
        }
        delete [] kernel;
        return ret;
    }
    bm_free_device(handle, kernel_mem);
    delete [] kernel;
    return BM_SUCCESS;
}

bm_status_t bmcv_image_gaussian_blur_bm1684x(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int kw,
        int kh,
        float sigmaX,
        float sigmaY) {
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_gaussian_blur_check_bm1684x(handle, input, output, kw, kh);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    float *kernel = (float*)malloc(kw * kh * sizeof(float));
    memset(kernel, 0, kw * kh * sizeof(float));
    create_gaussian_kernel(kernel, kw, kh, sigmaX, sigmaY);
    bm_device_mem_t kernel_mem;
    ret = bm_malloc_device_byte(handle, &kernel_mem, kw * kh * sizeof(float));
    if (BM_SUCCESS != ret) {
        free(kernel);
        return ret;
    }
    ret = bm_memcpy_s2d(handle, kernel_mem, kernel);
    if (BM_SUCCESS != ret) {
        bm_free_device(handle, kernel_mem);
        free(kernel);
        return ret;
    }
    if (!bm_image_is_attached(output)) {
        ret = bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY);
        if (ret != BM_SUCCESS) {
            bm_free_device(handle, kernel_mem);
            free(kernel);
            return ret;
        }
    }
    // construct and send api
    int stride_i[3], stride_o[3];
    bm_image_get_stride(input, stride_i);
    bm_image_get_stride(output, stride_o);
    bm_device_mem_t input_mem[3];
    bm_image_get_device_mem(input, input_mem);
    bm_device_mem_t output_mem[3];
    bm_image_get_device_mem(output, output_mem);
    int channel = bm_image_get_plane_num(input);
    sg_api_cv_gaussian_blur_t api;
    api.channel = channel;
    api.kernel_addr = bm_mem_get_device_addr(kernel_mem);
    api.kh = kh;
    api.kw = kw;
    api.delta = 0;
    api.is_packed = (input.image_format == FORMAT_RGB_PACKED ||
                     input.image_format == FORMAT_BGR_PACKED);
    api.out_type = 0;   // 0-uint8  1-uint16
    for (int i = 0; i < channel; i++) {
        api.input_addr[i] = bm_mem_get_device_addr(input_mem[i]);
        api.output_addr[i] = bm_mem_get_device_addr(output_mem[i]);
        api.width = input.image_private->memory_layout[i].W / (api.is_packed ? 3 : 1);
        api.height = input.image_private->memory_layout[i].H;
        api.stride_i = stride_i[i];
        api.stride_o = stride_o[i];
    }
    if (input.image_format == FORMAT_RGB_PLANAR ||
        input.image_format == FORMAT_BGR_PLANAR) {
        api.channel = 3;
        for (int i = 0; i < 3; i++) {
            api.stride_i = stride_i[0];
            api.stride_o = stride_o[0];
            api.width = input.width;
            api.height = input.height;
            api.input_addr[i] = bm_mem_get_device_addr(input_mem[0]) + input.height * stride_i[0] * i;
            api.output_addr[i] = bm_mem_get_device_addr(output_mem[0]) + input.height * stride_i[0] * i;
        }
    }
    ret = bm_tpu_kernel_launch(handle, "cv_gaussian_blur", (u8 *)&api, sizeof(api));
    if (BM_SUCCESS != ret) {
        bmlib_log("gaussinan_blur", BMLIB_LOG_ERROR, "gaussinan_blur sync api error\n");
    }
    bm_free_device(handle, kernel_mem);
    free(kernel);
    return ret;
}

bm_status_t bmcv_image_gaussian_blur(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int kw,
        int kh,
        float sigmaX,
        float sigmaY) {

    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;
    bm_handle_check_2(handle, input, output);
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;
    switch(chipid)
    {
      case 0x1684:
        ret = bmcv_image_gaussian_blur_bm1684(handle, input, output, kw, kh, sigmaX, sigmaY);
        break;
      case BM1684X:
        ret = bmcv_image_gaussian_blur_bm1684x(handle, input, output, kw, kh, sigmaX, sigmaY);
        break;
      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }
    return ret;
}
