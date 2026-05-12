#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bmcv_bm1684x.h"
#include "bmlib_runtime.h"
#include <memory>
#include <vector>
#include <stdio.h>

static bm_status_t add_mask_to_image_check(bm_image mask, bm_image image, int mask_num)
{
    if (mask.height != image.height || mask.width != image.width) {
        bmlib_log("ADD_MASK_TO_IMAGE", BMLIB_LOG_ERROR, "mask and image image size should be same\n");
        return BM_ERR_PARAM;
    }

    if (mask.data_type  != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("ADD_MASK_TO_IMAGE", BMLIB_LOG_ERROR, "mask not supported data type\n");
        return BM_NOT_SUPPORTED;
    }

    if (mask.image_format != FORMAT_GRAY) {
        bmlib_log("ADD_MASK_TO_IMAGE", BMLIB_LOG_ERROR, "mask not supported format type\n");
        return BM_NOT_SUPPORTED;
    }

    if (image.data_type != DATA_TYPE_EXT_FLOAT32) {
        bmlib_log("ADD_MASK_TO_IMAGE", BMLIB_LOG_ERROR, "image not supported data type\n");
        return BM_NOT_SUPPORTED;
    }

    if (image.image_format != FORMAT_RGB_PACKED) {
        bmlib_log("ADD_MASK_TO_IMAGE", BMLIB_LOG_ERROR, "image not supported format type\n");
        return BM_NOT_SUPPORTED;
    }

    if (mask_num < 0 && mask_num > 4) {
        bmlib_log("ADD_MASK_TO_IMAGE", BMLIB_LOG_ERROR, "mask_num max value is 4\n");
        return BM_ERR_PARAM;
    }

    return BM_SUCCESS;
}


bm_status_t bmcv_add_mask_to_image(
    bm_handle_t  handle,
    bm_image     mask,
    bm_image     image,
    int          num_mask,
    mask_info_t *mask_config)
{
    bm_status_t ret = BM_SUCCESS;

    ret = add_mask_to_image_check(mask, image, num_mask);
    if (ret != BM_SUCCESS) {
        return ret;
    }

    bool image_alloc_flag = false;
    if (!bm_image_is_attached(image)) {
        ret = bm_image_alloc_dev_mem(image, BMCV_HEAP_ANY);
        if (ret != BM_SUCCESS) {
            return ret;
        }
        image_alloc_flag = true;
    }

    sg_api_add_mask_to_image_t api;
    memset(&api, 0, sizeof(sg_api_add_mask_to_image_t));

    bm_device_mem_t mask_mem, image_mem;
    bm_image_get_device_mem(mask, &mask_mem);
    bm_image_get_device_mem(image, &image_mem);

    api.input_addr = bm_mem_get_device_addr(mask_mem);
    api.output_addr = bm_mem_get_device_addr(image_mem);
    api.width = mask.width;
    api.height = mask.height;
    api.mask_num = num_mask;

    for (int i = 0; i < num_mask; i++) {
        api.mask_info[i] = mask_config[i];
    }

    /* load tpu module */
    ret = bm_tpu_kernel_launch(handle, "add_mask_to_image_device", (unsigned char *)&api, sizeof(api));
    if (ret != BM_SUCCESS) {
        printf("add mask to image launch failed\n");
        if (image_alloc_flag)
            bm_free_device(handle, image_mem);

        return ret;
    }

    return ret;
}

