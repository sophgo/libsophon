#include "bmcv_api_ext.h"
#include "bmcv_common_bm1684.h"
#include "bmcv_internal.h"
#include <memory>
#include <vector>
#include <cstring>
#include <stdio.h>


#define IS_YUV(a) (a == FORMAT_NV12 || a == FORMAT_NV21 || a == FORMAT_NV16 ||     \
                   a == FORMAT_NV61 || a == FORMAT_NV24 || a == FORMAT_YUV420P ||  \
                   a == FORMAT_YUV422P || a == FORMAT_YUV444P)
#define IS_RGB(a) (a == FORMAT_RGB_PACKED || a == FORMAT_RGB_PLANAR ||     \
                   a == FORMAT_BGR_PACKED || a == FORMAT_BGR_PLANAR ||  \
                   a == FORMAT_RGBP_SEPARATE || a == FORMAT_BGRP_SEPARATE)

static void get_sobel_kernel(
        float* kernel,
        int dx,
        int dy,
        int _ksize,
        float scale) {
    int i, j, ksizeX = _ksize, ksizeY = _ksize;
    if( ksizeX == 1 && dx > 0 )
        ksizeX = 3;
    if( ksizeY == 1 && dy > 0 )
        ksizeY = 3;

    signed char* kx = new signed char [ksizeX];
    signed char* ky = new signed char [ksizeY];
    #ifdef __linux__
    signed char* kerI = new signed char [std::max(ksizeX, ksizeY) + 1];
    #else
    signed char* kerI = new signed char [(std::max)(ksizeX, ksizeY) + 1];
    #endif

    for(int k = 0; k < 2; k++) {
        signed char* tmp = k == 0 ? kx : ky;
        int order = k == 0 ? dx : dy;
        int ksize = k == 0 ? ksizeX : ksizeY;

        if (ksize == 1)
            kerI[0] = 1;
        else if(ksize == 3) {
            if (order == 0)
                kerI[0] = 1, kerI[1] = 2, kerI[2] = 1;
            else if(order == 1)
                kerI[0] = -1, kerI[1] = 0, kerI[2] = 1;
            else
                kerI[0] = 1, kerI[1] = -2, kerI[2] = 1;
        } else {
            int oldval, newval;
            kerI[0] = 1;
            for( i = 0; i < ksize; i++ )
                kerI[i+1] = 0;

            for(i = 0; i < ksize - order - 1; i++) {
                oldval = kerI[0];
                for(j = 1; j <= ksize; j++) {
                    newval = kerI[j]+kerI[j-1];
                    kerI[j-1] = oldval;
                    oldval = newval;
                }
            }
            for(i = 0; i < order; i++) {
                oldval = -kerI[0];
                for(j = 1; j <= ksize; j++) {
                    newval = kerI[j-1] - kerI[j];
                    kerI[j-1] = oldval;
                    oldval = newval;
                }
            }
        }
        memcpy(tmp, kerI, ksize * sizeof(char));
    }
    for (int i = 0; i < ksizeY; i++) {
        for (int j = 0; j < ksizeX; j++) {
            kernel[i * ksizeX + j] = ky[i] * kx[j] * scale;
        }
    }
    delete [] kx;
    delete [] ky;
    delete [] kerI;
}

static bm_status_t bmcv_sobel_check(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int dx,
        int dy,
        int ksize) {
    if (handle == NULL) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_DEVNOTREADY;
    }
    if (ksize % 2 == 0 || ksize > 31) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "The kernel size must be odd and not greater than 31\n" );
        return BM_ERR_PARAM;
    }
    if (dx < 0 || dy < 0 || dx + dy <= 0) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "dx or dy is illegal!\r\n");
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
    if (image_sw + ksize - 1 >= 2048) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "image width is too large!\r\n");
        return BM_ERR_PARAM;
    }
    if (ksize > 9) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "ksize is too large!\r\n");
        return BM_ERR_PARAM;
    }
    if (!IS_YUV(src_format) &&
        !IS_RGB(src_format) &&
        src_format != FORMAT_GRAY) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "Not supported input image format\n");
        return BM_ERR_DATA;
    }
    if ((IS_YUV(src_format) && dst_format != FORMAT_GRAY) &&
        (dst_format != src_format)) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "Not supported output image format\n");
        return BM_ERR_DATA;
    }
    if (src_type != DATA_TYPE_EXT_1N_BYTE ||
        dst_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "Not supported image data type\n");
        return BM_ERR_DATA;
    }
    if (image_sh != image_dh || image_sw != image_dw) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "input and output image size should be same\n");
        return BM_ERR_DATA;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_sobel_bm1684(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int dx,
        int dy,
        int ksize,
        float scale,
        float delta) {
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_sobel_check(handle, input, output, dx, dy, ksize);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    int kw = (ksize == 1 && dx > 0) ? 3 : ksize;
    int kh = (ksize == 1 && dy > 0) ? 3 : ksize;
    float* kernel = new float [kw * kh];
    get_sobel_kernel(kernel, dx, dy, ksize, scale);
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
    bm_api_cv_filter_t api;
    api.kernel_addr = bm_mem_get_device_addr(kernel_mem);
    api.kw = kw;
    api.kh = kh;
    api.delta = delta;
    api.is_packed = (input.image_format == FORMAT_RGB_PACKED ||
                     input.image_format == FORMAT_BGR_PACKED);
    api.out_type = 0;    // 0-uint8   1-uint16
    if (input.image_format == FORMAT_RGB_PLANAR ||
        input.image_format == FORMAT_BGR_PLANAR) {
        api.channel = 3;
        for (int i = 0; i < 3; i++) {
            api.stride_i[i] = stride_i[0];
            api.stride_o[i] = stride_o[0];
            api.width[i] = input.width;
            api.height[i] = input.height;
            api.input_addr[i] = bm_mem_get_device_addr(input_mem[0]) + input.height * stride_i[0] * i;
            api.output_addr[i] = bm_mem_get_device_addr(output_mem[0]) + input.height * stride_i[0] * i;
        }
    } else if (input.image_format == FORMAT_RGBP_SEPARATE ||
               input.image_format == FORMAT_RGBP_SEPARATE) {
        api.channel = 3;
        for (int i = 0; i < 3; i++) {
            api.input_addr[i] = bm_mem_get_device_addr(input_mem[i]);
            api.output_addr[i] = bm_mem_get_device_addr(output_mem[i]);
            api.width[i] = input.width;
            api.height[i] = input.height;
            api.stride_i[i] = stride_i[i];
            api.stride_o[i] = stride_o[i];
        }
    } else {
        api.channel = 1;
        api.input_addr[0] = bm_mem_get_device_addr(input_mem[0]);
        api.output_addr[0] = bm_mem_get_device_addr(output_mem[0]);
        api.width[0] = input.width;
        api.height[0] = input.height;
        api.stride_i[0] = stride_i[0];
        api.stride_o[0] = stride_o[0];
    }
    ret = bm_send_api(handle,  BM_API_ID_CV_FILTER, (uint8_t*)&api, sizeof(api));
    if (BM_SUCCESS != ret) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "sobel send api error\n");
        bm_free_device(handle, kernel_mem);
        if (output_alloc_flag) {
            bm_free_device(handle, output_mem[0]);
        }
        delete [] kernel;
        return ret;
    }
    ret = bm_sync_api(handle);
    if (BM_SUCCESS != ret) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "sobel sync api error\n");
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


bm_status_t bmcv_image_sobel(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int dx,
        int dy,
        int ksize,
        float scale,
        float delta) {
    unsigned int chipid = 0x1686;
    bm_status_t ret = BM_SUCCESS;
    bm_handle_check_2(handle, input, output);
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        ret = bmcv_image_sobel_bm1684(handle, input, output, dx, dy, ksize, scale, delta);
        break;

      case 0x1686:
        printf("current card not support\n");
        ret = BM_ERR_NOFEATURE;
        break;

      default:
        ret = BM_ERR_NOFEATURE;
        break;
    }

    return ret;
}
