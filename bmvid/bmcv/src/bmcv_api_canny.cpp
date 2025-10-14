#include "bmcv_api_ext.h"
#include "bmcv_common_bm1684.h"
#include "bmcv_bm1684x.h"
#include "bmcv_internal.h"
#include <cstring>
#include <memory>
#include <vector>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>

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
    signed char* kerI = new signed char[(std::max)(ksizeX, ksizeY) + 1];
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

static void get_sobel_kernel_1684x(float* kernel, int dx, int dy, int _ksize)
{
    if (_ksize == 3) {
        if ((dx == 1) && (dy == 0)) {
            kernel[0] = -1; kernel[1] = 0; kernel[2] = 1;
            kernel[3] = -2; kernel[4] = 0; kernel[5] = 2;
            kernel[6] = -1; kernel[7] = 0; kernel[8] = 1;
        } else if ((dx == 0) && (dy == 1)) {
            kernel[0] = -1; kernel[1] = -2; kernel[2] = -1;
            kernel[3] = 0; kernel[4] = 0; kernel[5] = 0;
            kernel[6] = 1; kernel[7] = 2; kernel[8] = 1;
        }
    }else {
        printf("Ksize only support 3 !\n");
    }
}

static bm_status_t bmcv_canny_check(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int aperture_size) {
    if (handle == NULL) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_DEVNOTREADY;
    }
    if (aperture_size != 3) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "Only support the aperture size is 3!\n" );
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
    int stride_o[3];
    bm_image_get_stride(output, stride_o);
    if (image_sw + aperture_size - 1 >= 2048) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "image width is too large!\r\n");
        return BM_ERR_DATA;
    }
    if (src_format != FORMAT_GRAY) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "Not supported input image format\n");
        return BM_ERR_DATA;
    }
    if (dst_format != FORMAT_GRAY) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "Not supported output image format\n");
        return BM_ERR_DATA;
    }
    if (src_type != DATA_TYPE_EXT_1N_BYTE ||
        dst_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "Not supported image data type\n");
        return BM_ERR_DATA;
    }
    if (image_sh != image_dh || image_sw != image_dw) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "input and output image size should be same\n");
        return BM_ERR_DATA;
    }
    if (output.width != stride_o[0]) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "output image stride should be equal to width\n");
        return BM_ERR_DATA;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_canny_bm1684(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        float threshold1,
        float threshold2,
        int aperture_size,
        bool l2gradient) {
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_canny_check(handle, input, output, aperture_size);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    float* kernel = new float [aperture_size * aperture_size * 2];
    get_sobel_kernel(kernel, 1, 0, aperture_size, 1);
    get_sobel_kernel(kernel + aperture_size * aperture_size, 0, 1, aperture_size, 1);
    bm_device_mem_t kernel_mem;
    ret = bm_malloc_device_byte(handle, &kernel_mem, aperture_size * aperture_size * 2 * sizeof(float));
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
    bm_device_mem_t map_mem;
    // not include the first line
    ret = bm_malloc_device_byte(handle, &map_mem, (input.width + 2) * (input.height + 1));
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
            bm_free_device(handle, map_mem);
            delete [] kernel;
            return ret;
        }
        output_alloc_flag = true;
    }
    // construct and send api
    int stride_i[3];
    bm_image_get_stride(input, stride_i);
    bm_device_mem_t input_mem[3];
    bm_image_get_device_mem(input, input_mem);
    bm_device_mem_t output_mem;
    bm_image_get_device_mem(output, &output_mem);
    bm_api_cv_canny_t api;
    api.kernel_addr = bm_mem_get_device_addr(kernel_mem);
    api.ks = aperture_size;
    api.low_thresh = threshold1 > threshold2 ? threshold2 : threshold1;
    api.high_thresh = threshold1 > threshold2 ? threshold1 : threshold2;
    if (l2gradient) {
        #ifndef __linux__
          api.low_thresh = (std::min)(float(32767), api.low_thresh);
          api.high_thresh = (std::min)(float(32767), api.high_thresh);
        #else
          api.low_thresh = std::min(float(32767), api.low_thresh);
          api.high_thresh = std::min(float(32767), api.high_thresh);
        #endif
        if (api.low_thresh > 0)
            api.low_thresh *= api.low_thresh;
        if (api.high_thresh > 0)
            api.high_thresh *= api.high_thresh;
    }
    api.input_addr = bm_mem_get_device_addr(input_mem[0]);
    api.output_addr = bm_mem_get_device_addr(map_mem);
    api.width = input.width;
    api.height = input.height;
    api.stride_i = stride_i[0];
    api.stride_o = output.width + 2;
    api.l2gradient = l2gradient ? 1 : 0;
    ret = bm_send_api(handle,  BM_API_ID_CV_CANNY, (uint8_t*)&api, sizeof(api));
    if (BM_SUCCESS != ret) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "canny send api error\n");
        bm_free_device(handle, kernel_mem);
        bm_free_device(handle, map_mem);
        if (output_alloc_flag) {
            bm_free_device(handle, output_mem);
        }
        delete [] kernel;
        return ret;
    }
    ret = bm_sync_api(handle);
    if (BM_SUCCESS != ret) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "canny sync api error\n");
        bm_free_device(handle, kernel_mem);
        bm_free_device(handle, map_mem);
        if (output_alloc_flag) {
            bm_free_device(handle, output_mem);
        }
        delete [] kernel;
        return ret;
    }
    unsigned char* map_i = new unsigned char [(input.height + 2) * (input.width + 2)];
    unsigned char* map_o = new unsigned char [input.height * input.width];
    // the first line memset not be edge
    memset(map_i, 1, input.width + 2);
    ret = bm_memcpy_d2s(handle, map_i + input.width + 2, map_mem);
    if (BM_SUCCESS != ret) {
        bm_free_device(handle, kernel_mem);
        bm_free_device(handle, map_mem);
        if (output_alloc_flag) {
            bm_free_device(handle, output_mem);
        }
        delete [] map_i;
        delete [] map_o;
        delete [] kernel;
        return ret;
    }
    unsigned char** stack = new unsigned char* [input.height * input.width];
    unsigned char **stack_top = &stack[0];
    unsigned char **stack_bottom = &stack[0];
    int mapstep = input.width + 2;
    #define CANNY_PUSH(d)    *(d) = (unsigned char)(2), *stack_top++ = (d)
    #define CANNY_POP(d)     (d) = *--stack_top
    for (int i = input.width + 2; i < (input.height + 1) * (input.width + 2); i++) {
        if (map_i[i] == 2)
            CANNY_PUSH(map_i + i);
    }
    while (stack_top > stack_bottom) {
        unsigned char* m;
        CANNY_POP(m);
        if (!m[-1])         CANNY_PUSH(m - 1);
        if (!m[1])          CANNY_PUSH(m + 1);
        if (!m[-mapstep-1]) CANNY_PUSH(m - mapstep - 1);
        if (!m[-mapstep])   CANNY_PUSH(m - mapstep);
        if (!m[-mapstep+1]) CANNY_PUSH(m - mapstep + 1);
        if (!m[mapstep-1])  CANNY_PUSH(m + mapstep - 1);
        if (!m[mapstep])    CANNY_PUSH(m + mapstep);
        if (!m[mapstep+1])  CANNY_PUSH(m + mapstep + 1);
    }
    // the final pass, form the final image
    for (int i = 0; i < input.height; i++) {
        for (int j = 0; j < input.width; j++)
            map_o[i * (mapstep - 2) + j] = (unsigned char)-(map_i[i * mapstep + j + mapstep + 1] >> 1);
    }
    ret = bm_memcpy_s2d(handle, output_mem, map_o);
    if (BM_SUCCESS != ret) {
        bm_free_device(handle, kernel_mem);
        bm_free_device(handle, map_mem);
        if (output_alloc_flag) {
            bm_free_device(handle, output_mem);
        }
        delete [] map_i;
        delete [] map_o;
        delete [] kernel;
        delete [] stack;
        return ret;
    }
    delete [] map_i;
    delete [] map_o;
    delete [] kernel;
    delete [] stack;
    bm_free_device(handle, kernel_mem);
    bm_free_device(handle, map_mem);
    return BM_SUCCESS;
}

static bm_status_t bmcv_canny_sobel_check_bm1684x(bm_handle_t handle, bm_image input, bm_image output, int dx, int dy, int ksize)
{
    if (handle == NULL) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_DEVNOTREADY;
    }
    if (ksize % 2 == 0 || ksize > 7) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "The kernel size must be odd and not greater than 7\n" );
        return BM_ERR_PARAM;
    }
    if (dx < 0 || dy < 0 || dx + dy <= 0) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "dx or dy is illegal!\r\n");
        return BM_ERR_PARAM;
    }
    bm_image_format_ext src_format = input.image_format;
    // bm_image_data_format_ext src_type = input.data_type;
    bm_image_format_ext dst_format = output.image_format;
    // bm_image_data_format_ext dst_type = output.data_type;
    int image_sh = input.height;
    int image_sw = input.width;
    int image_dh = output.height;
    int image_dw = output.width;
    if ((image_sw < 8) || (image_sh < 8)) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "min_width: 8, min_height: 8\r\n");
        return BM_ERR_PARAM;
    }
    if ((ksize == 3) && (image_sw + ksize - 1 > 8194)) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "when ksize = 3, image max_width: 8192!\r\n");
        return BM_ERR_PARAM;
    }
    if ((ksize == 5) && (image_sw + ksize - 1 > 4100)) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "when ksize = 5, image max_width: 4096!\r\n");
        return BM_ERR_PARAM;
    }
    if ((ksize == 7) && (image_sw + ksize - 1 > 2054)) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "when ksize = 7, image max_width: 2048!\r\n");
        return BM_ERR_PARAM;
    }
    if (src_format != FORMAT_GRAY) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "Not supported input image format\n");
        return BM_ERR_DATA;
    }
    if (dst_format != FORMAT_GRAY) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "Not supported output image format\n");
        return BM_ERR_DATA;
    }

    if (image_sh != image_dh || image_sw != image_dw) {
        bmlib_log("SOBEL", BMLIB_LOG_ERROR, "input and output image size should be same\n");
        return BM_ERR_DATA;
    }
    return BM_SUCCESS;
}

static bm_status_t bmcv_canny_sobel_bm1684x(bm_handle_t handle, bm_image input, bm_image output, int dx, int dy, int ksize)
{
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_canny_sobel_check_bm1684x(handle, input, output, dx, dy, ksize);
    if (BM_SUCCESS != ret) {
        return ret;
    }

    float* kernel = (float*)malloc(ksize * ksize * sizeof(float));
    get_sobel_kernel_1684x(kernel, dx, dy, ksize);
    bm_device_mem_t kernel_mem;
    ret = bm_malloc_device_byte(handle, &kernel_mem, ksize * ksize * sizeof(float));
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
    bool output_alloc_flag = false;
    if (!bm_image_is_attached(output)) {
        ret = bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY);
        if (ret != BM_SUCCESS) {
            bm_free_device(handle, kernel_mem);
            free(kernel);
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
    sg_api_cv_canny_sobel_t api;
    api.kernel_addr = bm_mem_get_device_addr(kernel_mem);
    api.kw = ksize;
    api.kh = ksize;
    api.delta = 0;
    api.is_packed = (input.image_format == FORMAT_RGB_PACKED ||
                     input.image_format == FORMAT_BGR_PACKED);

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
               input.image_format == FORMAT_BGRP_SEPARATE) {
        api.channel = 3;
        for (int i = 0; i < 3; i++) {
            api.input_addr[i] = bm_mem_get_device_addr(input_mem[i]);
            api.output_addr[i] = bm_mem_get_device_addr(output_mem[i]);
            api.width[i] = input.width;
            api.height[i] = input.height;
            api.stride_i[i] = stride_i[i];
            api.stride_o[i] = stride_o[i];
        }
    } else if (input.image_format == FORMAT_RGB_PACKED ||
               input.image_format == FORMAT_BGR_PACKED){
        api.channel = 1;
        for (int i = 0; i < 3; i++) {
            api.input_addr[i] = bm_mem_get_device_addr(input_mem[0]);
            api.output_addr[i] = bm_mem_get_device_addr(output_mem[0]);
            api.width[i] = input.width;
            api.height[i] = input.height;
            api.stride_i[i] = stride_i[0] / 3;
            api.stride_o[i] = stride_o[0] / 12;

        }
    } else {
        api.channel = 1;
        for (int i = 0; i < 3; i++) {
            api.input_addr[i] = bm_mem_get_device_addr(input_mem[0]);
            api.output_addr[i] = bm_mem_get_device_addr(output_mem[0]);
            api.width[i] = input.width;
            api.height[i] = input.height;
            api.stride_i[i] = stride_i[0];
            api.stride_o[i] = stride_o[0] / 4;
        }
    }

    unsigned int chipid;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
        return ret;
    switch (chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_canny_sobel", &api, sizeof(api));
            if (BM_SUCCESS != ret) {
                bmlib_log("CANNY SOBEL", BMLIB_LOG_ERROR, "bm_tpu_kernel_launch error\n");
                bm_free_device(handle, kernel_mem);
                if (output_alloc_flag) {
                    bm_free_device(handle, output_mem[0]);
                }
                free(kernel);
                return ret;
            }
            break;
        default:
            ret = BM_NOT_SUPPORTED;
            break;
    }
    bm_free_device(handle, kernel_mem);
    free(kernel);
    return BM_SUCCESS;
}

static void non_maximum_suppression(float* magnitude, float* direction, int height, int width)
{
    float* suppressed = (float*)malloc(width * height * sizeof(float));
    memset(suppressed, 0, width * height * sizeof(float));
    int x, y, index;
    float angle;

    for (y = 1; y < height - 1; y++) {
        for (x = 1; x < width - 1; x++) {
            index = y * width + x;
            angle = direction[index];

            if (((angle >= 0) && (angle < 22.5)) || ((angle >= 157.5) && (angle <= 180))) {
                if ((magnitude[index] >= magnitude[index - 1]) && (magnitude[index] >= magnitude[index + 1])) {
                    suppressed[index] = magnitude[index];
                }
            } else if ((angle >= 22.5) && (angle < 67.5)) {
                if ((magnitude[index] >= magnitude[index + width + 1]) && (magnitude[index] >= magnitude[index - width - 1])) {
                    suppressed[index] = magnitude[index];
                }
            } else if ((angle >= 67.5) && (angle < 112.5)) {
                if ((magnitude[index] >= magnitude[index - width]) && (magnitude[index] >= magnitude[index + width])) {
                    suppressed[index] = magnitude[index];
                }
            } else if ((angle >= 112.5) && (angle < 157.5)) {
                if ((magnitude[index] >= magnitude[index - width + 1]) && (magnitude[index] >= magnitude[index + width - 1])) {
                    suppressed[index] = magnitude[index];
                }
            }
        }
    }

    memcpy(magnitude, suppressed, width * height * sizeof(float));
    free(suppressed);
}

static void double_threshold(float* magnitude, float low_threshold, float high_threshold, int height, int width)
{
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int index = y * width + x;

            if (magnitude[index] >= high_threshold) {
                magnitude[index] = 255;
            } else if (magnitude[index] >= low_threshold) {
                magnitude[index] = 128;
            } else {
                magnitude[index] = 0;
            }
        }
    }
}

static void edge_linking(float* edges, int height, int width)
{
    int x, y, index;

    for (y = 1; y < height - 1; y++) {
        for (x = 1; x < width - 1; x++) {
            index = y * width + x;

            if (edges[index] == 128) {
                if (edges[index - 1] == 255 || edges[index + 1] == 255 ||
                    edges[index - width - 1] == 255 || edges[index - width + 1] == 255 ||
                    edges[index - width] == 255 || edges[index + width] == 255 ||
                    edges[index + width - 1] == 255 || edges[index + width + 1] == 255) {
                    edges[index] = 255;
                } else {
                    edges[index] = 0;
                }
            }
        }
    }
}

static bm_status_t bmcv_canny_mag(bm_handle_t handle, bm_image dx, bm_image dy, float* G, int width, int height, bool l2gradient)
{
    bm_device_mem_t dx_dev, dy_dev, d_dev;
    bm_status_t ret = BM_SUCCESS;
    sg_api_cv_canny_mag_t api;
    unsigned int chipid;

    ret = bm_malloc_device_byte(handle, &d_dev, width * height * sizeof(float));
    if (ret != BM_SUCCESS) {
        return ret;
    }

    ret = bm_image_get_device_mem(dx, &dx_dev);
    if (ret != BM_SUCCESS) {
        printf("bm_image_get_device_mem dx_dev failed!\n");
        goto exit0;
    }

    ret = bm_image_get_device_mem(dy, &dy_dev);
    if (ret != BM_SUCCESS) {
        printf("bm_image_get_device_mem dy_dev failed!\n");
        goto exit0;
    }

    api.dx_addr = bm_mem_get_device_addr(dx_dev);
    api.dy_addr = bm_mem_get_device_addr(dy_dev);
    api.d_addr = bm_mem_get_device_addr(d_dev);
    api.l2gradient = l2gradient;
    api.size = width * height;

    ret = bm_get_chipid(handle, &chipid);
    if (ret != BM_SUCCESS) {
        printf("bm_get_chipid failed!\n");
        goto exit0;
    }

    switch (chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_canny_mag", &api, sizeof(api));
            if (BM_SUCCESS != ret) {
                bmlib_log("CANNY_MAG", BMLIB_LOG_ERROR, "cv_canny_mag bm_tpu_kernel_launch error\n");
                goto exit0;
            }
            break;
        default:
            ret = BM_NOT_SUPPORTED;
            break;
    }

    ret = bm_memcpy_d2s(handle, G, d_dev);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s failed!\n");
        goto exit0;
    }

exit0:
    bm_free_device(handle, d_dev);
    return ret;
}

static bm_status_t bmcv_canny_double_thred(float* magnitude, float low_threshold, float high_threshold, int size, bm_handle_t handle)
{
    bm_device_mem_t mag_dev;
    bm_status_t ret = BM_SUCCESS;
    sg_api_cv_canny_double_thred_t api;
    unsigned int chipid;

    ret = bm_malloc_device_byte(handle, &mag_dev, size * sizeof(float));
    if (ret != BM_SUCCESS) {
        return ret;
    }

    ret = bm_memcpy_s2d(handle, mag_dev, magnitude);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d failed!\n");
        goto exit0;
    }

    api.mag_addr = bm_mem_get_device_addr(mag_dev);
    api.low_threshold = low_threshold;
    api.high_threshold = high_threshold;
    api.size = size;

    ret = bm_get_chipid(handle, &chipid);
    if (ret != BM_SUCCESS) {
        printf("bm_get_chipid failed!\n");
        goto exit0;
    }

    switch (chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_canny_double_thred", &api, sizeof(api));
            if (BM_SUCCESS != ret) {
                bmlib_log("CANNY_MAG", BMLIB_LOG_ERROR, "cv_canny_double_thred bm_tpu_kernel_launch error\n");
                goto exit0;
            }
            break;
        default:
            ret = BM_NOT_SUPPORTED;
            break;
    }

    ret = bm_memcpy_d2s(handle, magnitude, mag_dev);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s failed!\n");
        goto exit0;
    }

exit0:
    bm_free_device(handle, mag_dev);
    return ret;
}

static bm_status_t bmcv_image_canny_bm1684x(bm_handle_t handle, bm_image input, bm_image output, float low_thresh,
                            float high_thresh, int aperture_size, bool l2gradient)
{
    int width = output.width;
    int height = output.height;
    bm_image img_dx, img_dy;
    float* out_dx = (float*)malloc(width * height * sizeof(float));
    float* out_dy = (float*)malloc(width * height * sizeof(float));
    float* G = (float*)malloc(width * height * sizeof(float));
    float* direction = (float*)malloc(width * height * sizeof(float));
    unsigned char* res = (unsigned char*)malloc(width * height * sizeof(unsigned char));
    int i, j, index;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_FLOAT32, &img_dx);
    if (ret != BM_SUCCESS) {
        printf("bm_image_create img_dx failed!\n");
        goto exit0;
    }

    ret = bm_image_alloc_dev_mem(img_dx);
    if (ret != BM_SUCCESS) {
        printf("bm_image_alloc_dev_mem img_dx failed!\n");
        goto exit0;
    }

    ret = bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_FLOAT32, &img_dy);
    if (ret != BM_SUCCESS) {
        printf("bm_image_create img_dy failed!\n");
        goto exit0;
    }
    ret = bm_image_alloc_dev_mem(img_dy);
    if (ret != BM_SUCCESS) {
        printf("bm_image_alloc_dev_mem img_dy failed!\n");
        goto exit1;
    }

    ret = bmcv_canny_sobel_bm1684x(handle, input, img_dx, 1, 0, aperture_size);
    if (ret != BM_SUCCESS) {
        printf("bmcv_canny_sobel_bm1684x failed!\n");
        goto exit2;
    }

    ret = bm_image_copy_device_to_host(img_dx, (void**)(&out_dx));
    if (ret != BM_SUCCESS) {
        printf("bm_image_copy_device_to_host img_dx failed!\n");
        goto exit2;
    }

    ret = bmcv_canny_sobel_bm1684x(handle, input, img_dy, 0, 1, aperture_size);
    if (ret != BM_SUCCESS) {
        printf("bmcv_canny_sobel_bm1684x failed!\n");
        goto exit2;
    }

    ret = bm_image_copy_device_to_host(img_dy, (void**)(&out_dy));
    if (ret != BM_SUCCESS) {
        printf("bm_image_copy_device_to_host img_dy failed!\n");
        goto exit2;
    }

    ret = bmcv_canny_mag(handle, img_dx, img_dy, G, width, height, l2gradient);
    if (ret != BM_SUCCESS) {
        printf("bmcv_canny_mag failed!\n");
        goto exit2;
    }

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            index = i * width + j;

            // if (l2gradient) {
            //     float g = sqrt(out_dx[index] * out_dx[index] + out_dy[index] * out_dy[index]);
            //     if (g >= 255) {
            //         G[index] = 255;
            //     } else {
            //         G[index]= g;
            //     }
            // }
            // else {
            //     float g = fabs(out_dx[index]) + fabs(out_dy[index]);
            //     if (g > 255) {
            //         G[index] = 255;
            //     } else {
            //         G[index]= g;
            //     }
            // }

            direction[index] = atan2((out_dy[index]), out_dx[index]) * 180.0 / M_PI;
                if (direction[index] < 0) {
                    direction[index] += 180;
                }
            }
        }

    non_maximum_suppression(G, direction, height, width);

    ret = bmcv_canny_double_thred(G, low_thresh, high_thresh, height * width, handle);
    if (ret != BM_SUCCESS) {
        printf("bmcv_canny_double_thred failed!\n");
        goto exit2;
    }

    edge_linking(G, height, width);

    for (i = 0; i < width * height ; ++i) {
        res[i] = (unsigned char)round(G[i]);
    }

    ret = bm_image_copy_host_to_device(output, (void**)(&res));
    if (ret != BM_SUCCESS) {
        printf("bm_image_copy_host_to_device output failed!\n");
        goto exit2;
    }

exit2:
    bm_image_destroy(img_dy);
exit1:
    bm_image_destroy(img_dx);
exit0:
    free(out_dx);
    free(out_dy);
    free(G);
    free(direction);
    free(res);
    return ret;
}

bm_status_t bmcv_image_canny(bm_handle_t handle, bm_image input, bm_image output, float threshold1,
                            float threshold2, int aperture_size, bool l2gradient)

{
    unsigned int chipid = 0x1686;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_handle_check_2(handle, input, output);
    if (ret != BM_SUCCESS) {
        printf("bm_handle_check_2 failed!\n");
        return ret;
    }

    ret = bm_get_chipid(handle, &chipid);
    if (ret != BM_SUCCESS) {
        printf("bm_get_chipid failed!\n");
        return ret;
    }

    switch(chipid) {
      case 0x1684:
        ret = bmcv_image_canny_bm1684(handle, input, output, threshold1, threshold2, aperture_size, l2gradient);
        if (ret != BM_SUCCESS) {
            printf("bmcv_image_canny_bm1684 failed!\n");
        }
        break;

      case BM1684X:
        ret = bmcv_image_canny_bm1684x(handle, input, output, threshold1, threshold2, aperture_size, l2gradient);
        if (ret != BM_SUCCESS) {
            printf("bmcv_image_canny_bm1684x failed!\n");
        }
        break;

      default:
        printf("ChipID is NOT supported\n");
        ret = BM_ERR_NOFEATURE;
        break;
    }

    return ret;
}