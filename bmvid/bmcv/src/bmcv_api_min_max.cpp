#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmlib_runtime.h"
#include "bmcv_common_bm1684.h"
#include <cmath>
#include <vector>
#include <thread>
#include <iostream>
#define ONLY_MIN (0)
#define ONLY_MAX (1)
#define BOTH     (2)

bm_status_t bmcv_min_max(bm_handle_t handle,
                         bm_device_mem_t input,
                         float *minVal,
                         float *maxVal,
                         int len) {
    bm_status_t ret = BM_SUCCESS;
    if (minVal == nullptr && maxVal == nullptr)
        return ret;
    bm_device_mem_t minDev, maxDev;
    float minHost[64], maxHost[64];
    if (maxVal != nullptr)
        BM_CHECK_RET(bm_malloc_device_byte(handle, &maxDev, NPU_NUM * 4));
    if (minVal != nullptr)
        BM_CHECK_RET(bm_malloc_device_byte(handle, &minDev, NPU_NUM * 4));
    bm_api_cv_min_max_t api;
    api.inputAddr = bm_mem_get_device_addr(input);
    api.minAddr = bm_mem_get_device_addr(minDev);
    api.maxAddr = bm_mem_get_device_addr(maxDev);
    api.len = len;
    api.mode = maxVal == nullptr ? ONLY_MIN : (minVal == nullptr ? ONLY_MAX : BOTH);
    unsigned int chipid;
    bm_get_chipid(handle, &chipid);
    switch(chipid)
    {
        case 0x1684:
            BM_CHECK_RET(bm_send_api(handle,
                              BM_API_ID_CV_MIN_MAX,
                             (uint8_t *)(&api),
                             sizeof(api)));
            BM_CHECK_RET(bm_sync_api(handle));
            break;

        case BM1684X:
            BM_CHECK_RET(bm_tpu_kernel_launch(handle, "cv_min_max", &api, sizeof(api)));
            break;

        default:
            printf("BM_NOT_SUPPORTED!\n");
            break;
    }

    if (maxVal != nullptr) {
        BM_CHECK_RET(bm_memcpy_d2s(handle, maxHost, maxDev));
        *maxVal = maxHost[0];
        for (int i = 1; i < NPU_NUM; ++i)
            *maxVal = *maxVal > maxHost[i] ? *maxVal : maxHost[i];
        bm_free_device(handle, maxDev);
    }
    if (minVal != nullptr) {
        BM_CHECK_RET(bm_memcpy_d2s(handle, minHost, minDev));
        *minVal = minHost[0];
        for (int i = 1; i < NPU_NUM; ++i)
            *minVal = *minVal < minHost[i] ? *minVal : minHost[i];
        bm_free_device(handle, minDev);
    }
    return ret;
}
