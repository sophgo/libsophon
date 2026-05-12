#include "stdlib.h"
#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"
#include <string.h>

static bm_status_t bmcv_remap_check(bm_handle_t handle, bm_image input, bm_image output, int interpolation_mode) {
    bm_image_format_ext src_format = input.image_format;
    bm_image_data_format_ext src_type = input.data_type;
    bm_image_format_ext dst_format = output.image_format;
    bm_image_data_format_ext dst_type = output.data_type;
    int image_sh = input.height;
    int image_sw = input.width;
    int image_dh = output.height;
    int image_dw = output.width;
    if (handle == NULL) {
        bmlib_log("REMAP", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    if (image_sh < 8 || image_sw < 8) {
        bmlib_log("REMAP", BMLIB_LOG_ERROR, "Input image min size:8x8\n");
        return BM_NOT_SUPPORTED;
    }
    if (image_sh > 8192 || image_sw > 8192) {
        bmlib_log("REMAP", BMLIB_LOG_ERROR, "Input image max size:8192x8192\n");
        return BM_NOT_SUPPORTED;
    }
    if (image_dh < 8 || image_dw < 8) {
        bmlib_log("REMAP", BMLIB_LOG_ERROR, "Output image min size:8x8\n");
        return BM_NOT_SUPPORTED;
    }
    if(interpolation_mode == 1) {
        if (image_dh > 4096 || image_dw > 4096) {
            bmlib_log("REMAP", BMLIB_LOG_ERROR, "interpolation_mode:BILINEAR, Output image max size: 4096x4096\n");
            return BM_NOT_SUPPORTED;
        }
    } else {
        if (image_dh > 8192 || image_dw > 8192) {
            bmlib_log("REMAP", BMLIB_LOG_ERROR, "interpolation_mode:NEAREST, Output image max size: 8192x8192\n");
            return BM_NOT_SUPPORTED;
        }
    }
    if (src_format != FORMAT_YUV444P &&
        src_format != FORMAT_RGB_PLANAR &&
        src_format != FORMAT_BGR_PLANAR &&
        src_format != FORMAT_BGRP_SEPARATE &&
        src_format != FORMAT_RGBP_SEPARATE &&
        src_format != FORMAT_GRAY) {
        bmlib_log("REMAP", BMLIB_LOG_ERROR, "Not supported input image format!\n");
        return BM_NOT_SUPPORTED;
    }
    if (dst_format != src_format) {
        bmlib_log("REMAP", BMLIB_LOG_ERROR, "Input and output image format should be same!\n");
        return BM_NOT_SUPPORTED;
    }
    if (src_type != DATA_TYPE_EXT_1N_BYTE ||
        dst_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("REMAP", BMLIB_LOG_ERROR, "Not supported image data type\n");
        return BM_NOT_SUPPORTED;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_remap(bm_handle_t handle, bm_image input, bm_image output, bm_device_mem_t mapx_data_global_addr,
                             bm_device_mem_t mapy_data_global_addr, int interpolation_mode) {
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_remap_check(handle, input, output, interpolation_mode);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    int channel = bm_image_get_plane_num(input);
    bm_device_mem_t input_mem[3], output_mem[3];
    bm_image_get_device_mem(input, input_mem);
    bm_image_get_device_mem(output, output_mem);
    sg_api_cv_remap_t api;

    api.channel = channel;
    api.interpolation_mode = interpolation_mode;
    for (int i = 0; i < channel; i++) {
        api.input_addr[i] = bm_mem_get_device_addr(input_mem[i]);
        api.output_addr[i] = bm_mem_get_device_addr(output_mem[i]);
    }
    if (input.image_format == FORMAT_RGB_PLANAR ||
        input.image_format == FORMAT_BGR_PLANAR) {
        api.channel = 3;
        for (int i = 0; i < 3; i++) {
            api.input_addr[i] = bm_mem_get_device_addr(input_mem[0]) + input.height * input.width * i;
            api.output_addr[i] = bm_mem_get_device_addr(output_mem[0]) + output.height * output.width * i;
        }
    }
    api.input_width = input.width;
    api.input_height = input.height;
    api.output_width = output.width;
    api.output_height = output.height;
    api.mapx_addr = bm_mem_get_device_addr(mapx_data_global_addr);
    api.mapy_addr = bm_mem_get_device_addr(mapy_data_global_addr);
    unsigned int chipid;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret) {
        bmlib_log("remap", BMLIB_LOG_ERROR, "remap bm_get_chipid error\n");
        return ret;
    }
    switch (chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_remap", (u8 *)&api, sizeof(api));
            if (BM_SUCCESS != ret) {
                bmlib_log("remap", BMLIB_LOG_ERROR, "remap sync api error\n");
                return ret;
            }
            break;
        default:
            ret = BM_NOT_SUPPORTED;
            break;
    }
    return ret;
}