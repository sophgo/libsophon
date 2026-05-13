#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"
#include <stdio.h>
bm_status_t bmcv_image_count_nonzero_check(bm_handle_t handle, int width, int height, int channel) {
    if (handle == NULL) {
        bmlib_log("COUNT_NONZERO", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    if (width > 8192 || height > 8192) {
        bmlib_log("COUNT_NONZERO", BMLIB_LOG_ERROR, "Max image size: 8192x8192!\r\n");
        return BM_ERR_PARAM;
    }
    if (channel != 1 && channel != 3) {
        bmlib_log("COUNT_NONZERO", BMLIB_LOG_ERROR, "channel must be 1 or 3\r\n");
        return BM_ERR_PARAM;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_count_nonzero(bm_handle_t handle, bm_device_mem_t input_addr, bm_device_mem_t nonzero_idx_addr,
                               int width, int height, int channel, int* nonzero_count) {
    bm_status_t ret = BM_SUCCESS;
    bm_device_mem_t nonzero_count_addr;
    bm_malloc_device_byte(handle, &nonzero_count_addr, sizeof(int));
    ret = bmcv_image_count_nonzero_check(handle, width, height, channel);
    if (BM_SUCCESS != ret) {
        bmlib_log("count_nonzero", BMLIB_LOG_ERROR, "bmcv_image_count_nonzero_check error\r\n");
        return ret;
    }
    sg_api_cv_count_nonzero_t api;
    api.channel = channel;
    api.width = width;
    api.height = height;
    api.input_addr = bm_mem_get_device_addr(input_addr);
    api.nonzero_count_addr = bm_mem_get_device_addr(nonzero_count_addr);
    api.nonzero_idx_addr = bm_mem_get_device_addr(nonzero_idx_addr);
    unsigned int chipid;
    bm_get_chipid(handle, &chipid);
    switch (chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_count_nonzero", &api, sizeof(api));
            if(BM_SUCCESS != ret){
                bmlib_log("COUNT_NONZERO", BMLIB_LOG_ERROR, "count_nonzero sync api error\n");
                return BM_ERR_FAILURE;
            }
            break;
        default:
            printf("ChipID is NOT supported\n");
            break;
    }
    bm_memcpy_d2s(handle, nonzero_count, nonzero_count_addr);
    bm_free_device(handle, nonzero_count_addr);
    return BM_SUCCESS;
}