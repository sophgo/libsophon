#include "string.h"
#include <math.h>
#include "bmcv_api_ext.h"
#include <stdio.h>
#include <float.h>
#include "bmcv_common_bm1684.h"
#include "bmcv_bm1684x.h"
#include "bmcv_internal.h"

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

static bm_status_t check(
    bm_handle_t handle,
    int width,
    int height,
    int win_size,
    bm_image bm_im1,
    bm_image bm_im2,
    float data_range) {
    if (handle == NULL) {
        bmlib_log("ssim", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    if (width - win_size < 0 || height - win_size < 0) {
        bmlib_log("ssim", BMLIB_LOG_ERROR, "win_size exceeds image extent!\r\n");
        return BM_ERR_FAILURE;
    }
    if (win_size % 2 == 0) {
        bmlib_log("ssim", BMLIB_LOG_ERROR, "Window size must be odd!\r\n");
        return BM_ERR_FAILURE;
    }

    bm_image_format_ext fmt1 = bm_im1.image_format;
    bm_image_data_format_ext type1 = bm_im1.data_type;
    bm_image_format_ext fmt2 = bm_im2.image_format;
    bm_image_data_format_ext type2 = bm_im2.data_type;
    if (fmt1 != FORMAT_GRAY && fmt1 != FORMAT_RGB_PLANAR && fmt1 != FORMAT_BGR_PLANAR) {
        bmlib_log("ssim", BMLIB_LOG_ERROR, "image format NOT supported!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (type1 != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("ssim", BMLIB_LOG_ERROR, "data type NOT supported!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (fmt1 != fmt2 || type1 != type2) {
        bmlib_log("ssim", BMLIB_LOG_ERROR, "two input images should be with same format and data type!\r\n");
        return BM_ERR_FAILURE;
    }
    if (data_range <= 0.0f) {
        return BM_NOT_SUPPORTED;
    }

    return BM_SUCCESS;
}

/**
 * @brief Calculate the SSIM (Structural Similarity Index) of two images on TPU
 * @param handle BM device handle
 * @param win_size Window size (must be odd)
 * @param gradient Whether to compute gradient (unused)
 * @param data_range Pixel value range (e.g., 255.0)
 * @param gaussian_weights Whether to use Gaussian weights
 * @param full Whether to output the full difference map
 * @param bm_im1 Image 1
 * @param bm_im2 Image 2
 * @param diff_map_tpu Output difference map (optional, caller is responsible for release)
 * @param mssim_tpu Output average SSIM value
 * @return BM_SUCCESS on success, other error codes on failure
 */
bm_status_t bmcv_image_ssim(
    bm_handle_t handle,
    int win_size,
    // bool gradient,
    float data_range,
    bool gaussian_weights,
    bool full,
    bm_image bm_im1,
    bm_image bm_im2,
    float** diff_map_tpu,
    float* mssim_tpu
) {
    bm_status_t ret = check(handle, bm_im1.width, bm_im1.height, win_size, bm_im1, bm_im2, data_range);
    if (ret != BM_SUCCESS)
        return ret;

    unsigned int chipid;
    int kernel_len = win_size;
    float *kernel = NULL;
    float sigma = 1.5f;
    int width = bm_im1.width;
    int height = bm_im1.height;
    int channel = bm_image_get_plane_num(bm_im1);

    int pad = (win_size - 1) / 2;
    double mssim = 0.0f;
    double total_ssim = 0.0f;

    bm_api_cv_ssim_t api;
    int stride1[3], stride2[3];
    bm_image_get_stride(bm_im1, stride1);
    bm_image_get_stride(bm_im2, stride2);
    bm_device_mem_t im1_mem[3], im2_mem[3];
    bm_image_get_device_mem(bm_im1, im1_mem);
    bm_image_get_device_mem(bm_im2, im2_mem);
    if (bm_im1.image_format == FORMAT_RGB_PLANAR || bm_im1.image_format == FORMAT_BGR_PLANAR) {
        channel = 3;
    }

    float* diffmap_tpu = (float*)malloc(width * height * channel * sizeof(float));
    if (!diffmap_tpu) {
        ret = BM_ERR_FAILURE;
        goto exit0;
    }

    api.channel = channel;
    api.data_range = data_range;
    api.width = bm_im1.width;
    api.height = bm_im1.height;
    api.win_size = win_size;

    for (int i = 0; i < channel; i++) {
        api.im1_addr[i] = bm_mem_get_device_addr(im1_mem[0]) + bm_im1.height * stride1[0] * i;
        api.im2_addr[i] = bm_mem_get_device_addr(im2_mem[0]) + bm_im2.height * stride2[0] * i;
    }

    if (gaussian_weights) {
        int r = (int)(3.5f * sigma + 0.5f);
        kernel_len = 2 * r + 1;
        kernel = (float*)malloc(kernel_len * kernel_len * sizeof(float));
        if (!kernel) {
            ret = BM_ERR_FAILURE;
            goto exit0;
        }
        create_gaussian_kernel(kernel, kernel_len, kernel_len, sigma, sigma);
    } else {
        kernel = (float*)malloc(kernel_len * kernel_len * sizeof(float));
        if (!kernel) {
            ret = BM_ERR_FAILURE;
            goto exit0;
        }
        for (int i = 0; i < kernel_len * kernel_len; i++) {
            kernel[i] = 1.0f / (kernel_len * kernel_len);
        }
    }
    api.kernel_len = kernel_len;

    bm_device_mem_t kernel_mem;
    ret = bm_malloc_device_byte(handle, &kernel_mem, kernel_len * kernel_len * sizeof(float));
    if (BM_SUCCESS != ret) {
        goto exit0;
    }
    ret = bm_memcpy_s2d(handle, kernel_mem, kernel);
    if (BM_SUCCESS != ret) {
        goto exit1;
    }
    api.kernel_addr = bm_mem_get_device_addr(kernel_mem);

    bm_device_mem_t diff_map_mem;
    ret = bm_malloc_device_byte(handle, &diff_map_mem, width * height * channel * sizeof(float));
    if (BM_SUCCESS != ret) {
        goto exit2;
    }
    api.diff_map_addr = bm_mem_get_device_addr(diff_map_mem);

    bm_device_mem_t ux_mem;
    ret = bm_malloc_device_byte(handle, &ux_mem, width * height * sizeof(float));
    if (BM_SUCCESS != ret) {
        goto exit3;
    }
    api.ux_addr = bm_mem_get_device_addr(ux_mem);

    bm_device_mem_t uy_mem;
    ret = bm_malloc_device_byte(handle, &uy_mem, width * height * sizeof(float));
    if (BM_SUCCESS != ret) {
        goto exit4;
    }
    api.uy_addr = bm_mem_get_device_addr(uy_mem);

    bm_device_mem_t u_vxx_mem;
    ret = bm_malloc_device_byte(handle, &u_vxx_mem, width * height * sizeof(float));
    if (BM_SUCCESS != ret) {
        goto exit5;
    }
    api.u_vxx_addr = bm_mem_get_device_addr(u_vxx_mem);

    bm_device_mem_t u_vyy_mem;
    ret = bm_malloc_device_byte(handle, &u_vyy_mem, width * height * sizeof(float));
    if (BM_SUCCESS != ret) {
        goto exit6;
    }
    api.u_vyy_addr = bm_mem_get_device_addr(u_vyy_mem);

    bm_device_mem_t u_vxy_mem;
    ret = bm_malloc_device_byte(handle, &u_vxy_mem, width * height * sizeof(float));
    if (BM_SUCCESS != ret) {
        goto exit7;
    }
    api.u_vxy_addr = bm_mem_get_device_addr(u_vxy_mem);

    ret = bm_get_chipid(handle, &chipid);
    if (ret) {
        printf("get chipid is error !\n");
        return BM_ERR_FAILURE;
    }

    switch(chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_ssim", &api, sizeof(api));
            if(ret != BM_SUCCESS){
                bmlib_log("SSIM", BMLIB_LOG_ERROR, "cv_ssim sync api error\n");
                goto exit7;
            }
            break;
        default:
            printf("BMCV_SSIM BM_NOT_SUPPORTED!\n");
            ret = BM_NOT_SUPPORTED;
            break;
    }

    if (ret != BM_SUCCESS) {
        printf("tpu_kernel_launch failed");
        goto exit7;
    }

    ret = bm_memcpy_d2s(handle, diffmap_tpu, diff_map_mem);
    if (BM_SUCCESS != ret) {
        goto exit7;
    }

    for (int c = 0; c < channel; c++) {
        double ssim_c = 0.0f;
        int count_c = 0;
        for (int y = pad; y < height - pad; y++) {
            for (int x = pad; x < width - pad; x++) {
                ssim_c += diffmap_tpu[c * width * height + y * width + x];
                count_c++;
            }
        }
        ssim_c /= count_c;
        total_ssim += ssim_c;
    }
    mssim = total_ssim / channel;

    *mssim_tpu = mssim;
    if (full && diff_map_tpu) {
        *diff_map_tpu = (float*)malloc(width * height * channel * sizeof(float));
        if (*diff_map_tpu == NULL) {
            ret = BM_ERR_FAILURE;
            goto exit7;
        }
        memcpy(*diff_map_tpu, diffmap_tpu, width * height * channel * sizeof(float));
    }

exit7:
    bm_free_device(handle, u_vxy_mem);
exit6:
    bm_free_device(handle, u_vyy_mem);
exit5:
    bm_free_device(handle, u_vxx_mem);
exit4:
    bm_free_device(handle, uy_mem);
exit3:
    bm_free_device(handle, ux_mem);
exit2:
    bm_free_device(handle, diff_map_mem);
exit1:
    bm_free_device(handle, kernel_mem);
exit0:
    free(kernel);
    free(diffmap_tpu);
    return ret;
}