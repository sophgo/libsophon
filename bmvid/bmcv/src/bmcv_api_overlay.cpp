#include <stdio.h>
#include <stdlib.h>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"
#include <sys/time.h>
#include <memory>
#include "bmcv_common_bm1684.h"
#include <vector>
#include <iostream>
#include "bmlib_runtime.h"

bm_status_t bmcv_overlay_check(bm_handle_t handle, bm_image input_base_img, bm_image input_overlay_img,
                               int pos_x, int pos_y) {
    if (handle == NULL) {
        bmlib_log("OVERLAY", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    bm_image_format_ext input_base_format = input_base_img.image_format;
    bm_image_format_ext input_overlay_format = input_overlay_img.image_format;
    bm_image_data_format_ext input_base_dtype = input_base_img.data_type;
    bm_image_data_format_ext input_overlay_dtype = input_overlay_img.data_type;
    int base_width = input_base_img.image_private->memory_layout[0].W / 3;
    int base_height = input_base_img.image_private->memory_layout[0].H;
    int overlay_width = input_overlay_img.image_private->memory_layout[0].W / 4;
    int overlay_height = input_overlay_img.image_private->memory_layout[0].H;

    if (input_base_format != FORMAT_RGB_PACKED) {
        bmlib_log("OVERLAY", BMLIB_LOG_ERROR, "Not supported base_img format");
        return BM_NOT_SUPPORTED;
    }
    if (input_overlay_format != FORMAT_ABGR_PACKED &&
        input_overlay_format != FORMAT_ARGB4444_PACKED &&
        input_overlay_format != FORMAT_ARGB1555_PACKED) {
        bmlib_log("OVERLAY", BMLIB_LOG_ERROR, "Not supported overlay_img format");
        return BM_NOT_SUPPORTED;
    }
    if (input_base_dtype != DATA_TYPE_EXT_1N_BYTE ||
        input_overlay_dtype != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("OVERLAY", BMLIB_LOG_ERROR, "Not supported image data type");
        return BM_NOT_SUPPORTED;
    }
    if (pos_x + overlay_width > base_width || pos_y + overlay_height > base_height) {
        bmlib_log("OVERLAY", BMLIB_LOG_ERROR, "The specified position of the overlay image is out of bounds");
        return BM_ERR_PARAM;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_overlay(bm_handle_t handle, bm_image input_base_img, int overlay_num,
                            bmcv_rect_t* overlay_info, bm_image* input_overlay_img)
{
    bm_status_t ret = BM_SUCCESS;
    for (int i = 0; i < overlay_num; i++) {
        ret = bmcv_overlay_check(handle, input_base_img, input_overlay_img[i], overlay_info[i].start_x, overlay_info[i].start_y);
        if (BM_SUCCESS != ret) {
            printf("bmcv_overlay_check failed!\n");
            return ret;
        }
    }

    bm_device_mem_t input_base_mem;
    bm_device_mem_t input_overlay_mem[overlay_num];
    bm_image_get_device_mem(input_base_img, &input_base_mem);
    unsigned long long overlay_addr[overlay_num];

    for (int k = 0; k < overlay_num; k++) {
        bm_image_get_device_mem(input_overlay_img[k], input_overlay_mem + k);
        overlay_addr[k] = bm_mem_get_device_addr(input_overlay_mem[k]);
    }

    sg_api_cv_overlay_t api;

    api.overlay_num = overlay_num;
    for (int i = 0; i < overlay_num; i++) {
        api.overlay_addr[i] = overlay_addr[i];;
        api.overlay_width[i] = overlay_info[i].crop_w;
        api.overlay_height[i] = overlay_info[i].crop_h;
        api.pos_x[i] = overlay_info[i].start_x;
        api.pos_y[i] = overlay_info[i].start_y;
    }
    api.base_addr = bm_mem_get_device_addr(input_base_mem);
    api.base_width = input_base_img.image_private->memory_layout[0].W / 3;
    api.base_height = input_base_img.image_private->memory_layout[0].H;
    api.format = input_overlay_img->image_format;

    unsigned int chipid;
    bm_get_chipid(handle, &chipid);
    switch (chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_overlay", (u8 *)&api, sizeof(api));
            if(BM_SUCCESS != ret){
                bmlib_log("OVERLAY", BMLIB_LOG_ERROR, "overlay sync api error\n");
                return BM_ERR_FAILURE;
            }
            break;
        default:
            printf("ChipID is NOT supported\n");
            break;
    }

    return BM_SUCCESS;
};