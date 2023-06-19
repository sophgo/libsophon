#include "bmcv_api_ext.h"
#include "bmcv_common_bm1684.h"
#include <cstring>
#include <memory>
#include <vector>


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

static bm_status_t bmcv_canny_check(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int aperture_size) {
    if (handle == NULL) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
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
        return BM_NOT_SUPPORTED;
    }
    if (!IS_YUV(src_format) &&
        src_format != FORMAT_GRAY) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "Not supported input image format\n");
        return BM_NOT_SUPPORTED;
    }
    if (dst_format != FORMAT_GRAY) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "Not supported output image format\n");
        return BM_NOT_SUPPORTED;
    }
    if (src_type != DATA_TYPE_EXT_1N_BYTE ||
        dst_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "Not supported image data type\n");
        return BM_NOT_SUPPORTED;
    }
    if (image_sh != image_dh || image_sw != image_dw) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "input and output image size should be same\n");
        return BM_NOT_SUPPORTED;
    }
    if (output.width != stride_o[0]) {
        bmlib_log("CANNY", BMLIB_LOG_ERROR, "output image stride should be equal to width\n");
        return BM_NOT_SUPPORTED;
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

bm_status_t bmcv_image_canny(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        float threshold1,
        float threshold2,
        int aperture_size,
        bool l2gradient) {

    unsigned int chipid = 0x1686;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        ret = bmcv_image_canny_bm1684(handle, input, output, threshold1, threshold2, aperture_size, l2gradient);
        break;

      case 0x1686:
        printf("bm1684x not support\n");
        ret = BM_NOT_SUPPORTED;
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}