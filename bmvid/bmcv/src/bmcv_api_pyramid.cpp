#include "bmcv_api_ext.h"
#include "bmcv_common_bm1684.h"
#include "bmcv_internal.h"
#include <stdlib.h>
#include <memory>
#include <vector>


static bm_status_t bmcv_pyramid_check(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        bool is_down) {
    if (handle == NULL) {
        bmlib_log("PYRAMID", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
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
    if (image_sw + 5 - 1 >= 2048) {
        bmlib_log("PYRAMID", BMLIB_LOG_ERROR, "image width is too large!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (src_format != FORMAT_GRAY || dst_format != FORMAT_GRAY) {
        bmlib_log("PYRAMID", BMLIB_LOG_ERROR, "Image format only support GRAY\n");
        return BM_NOT_SUPPORTED;
    }
    if (src_type != DATA_TYPE_EXT_1N_BYTE ||
        dst_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("PYRAMID", BMLIB_LOG_ERROR, "Not supported image data type\n");
        return BM_NOT_SUPPORTED;
    }
    if (is_down && (abs(image_sh - image_dh * 2) > 1 || abs(image_sw - image_dw * 2) > 1)) {
        bmlib_log("PYRAMID", BMLIB_LOG_ERROR, "input and output image size not valid\n");
        return BM_NOT_SUPPORTED;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_pyramid_down(
        bm_handle_t handle,
        bm_image input,
        bm_image output) {
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_pyramid_check(handle, input, output, true);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    int kw = 5, kh = 5;
    float kernel[25] = {
            1,  4,  6,  4, 1,
            4, 16, 24, 16, 4,
            6, 24, 36, 24, 6,
            4, 16, 24, 16, 4,
            1,  4,  6,  4, 1};
    bm_device_mem_t kernel_mem;
    ret = bm_malloc_device_byte(handle, &kernel_mem, kw * kh * sizeof(float));
    if (BM_SUCCESS != ret) {
        return ret;
    }
    ret = bm_memcpy_s2d(handle, kernel_mem, kernel);
    if (BM_SUCCESS != ret) {
        bm_free_device(handle, kernel_mem);
        return ret;
    }
    bool output_alloc_flag = false;
    if (!bm_image_is_attached(output)) {
        ret = bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY);
        if (ret != BM_SUCCESS) {
            bm_free_device(handle, kernel_mem);
            return ret;
        }
        output_alloc_flag = true;
    }
    // construct and send api
    int stride_i, stride_o;
    bm_image_get_stride(input, &stride_i);
    bm_image_get_stride(output, &stride_o);
    bm_device_mem_t input_mem;
    bm_image_get_device_mem(input, &input_mem);
    bm_device_mem_t output_mem;
    bm_image_get_device_mem(output, &output_mem);
    bm_api_cv_pyramid_t api;
    api.kernel_addr = bm_mem_get_device_addr(kernel_mem);
    api.input_addr = bm_mem_get_device_addr(input_mem);
    api.output_addr = bm_mem_get_device_addr(output_mem);
    api.width = input.width;
    api.height = input.height;
    api.ow = output.width;
    api.oh = output.height;
    api.stride_i = stride_i;
    api.stride_o = stride_o;

    unsigned int chipid = 0x1686;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {
      case 0x1684:{
        ret = bm_send_api(handle,  BM_API_ID_CV_PYRAMID, (uint8_t*)&api, sizeof(api));
        if (BM_SUCCESS != ret) {
            bmlib_log("PYRAMID", BMLIB_LOG_ERROR, "pyramid send api error\n");
            bm_free_device(handle, kernel_mem);
            if (output_alloc_flag) {
                bm_free_device(handle, output_mem);
            }
            return ret;
        }
        ret = bm_sync_api(handle);
        if (BM_SUCCESS != ret) {
            bmlib_log("PYRAMID", BMLIB_LOG_ERROR, "pyramid sync api error\n");
            bm_free_device(handle, kernel_mem);
            if (output_alloc_flag) {
                bm_free_device(handle, output_mem);
            }
            return ret;
        }
        break;
      }

      case 0x1686:{
        ret = bm_tpu_kernel_launch(handle, "cv_pyramid", &api, sizeof(api));
        if (BM_SUCCESS != ret) {
            bmlib_log("PYRAMID", BMLIB_LOG_ERROR, "pyramid sync api error\n");
            bm_free_device(handle, kernel_mem);
            if (output_alloc_flag) {
                bm_free_device(handle, output_mem);
            }
            return ret;
        }
        break;
      }
      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }
    bm_free_device(handle, kernel_mem);
    return ret;
}