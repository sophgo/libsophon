#include "bmcv_api_ext.h"
#include "bmcv_common_bm1684.h"
#include "bmcv_bm1684x.h"
#include "bmcv_internal.h"
#include <cmath>
#include <vector>
#include <thread>

typedef data_type_t bm_data_type_t;

bm_status_t bmcv_distance_1684x(bm_handle_t handle,
                          bm_device_mem_t input,
                          bm_device_mem_t output,
                          int dim,
                          const void *pnt,
                          int len,
                          int dtype) {
    if (dim <= 0 || dim > 8)
        return BM_ERR_PARAM;
    bm_status_t ret = BM_SUCCESS;
    bm_api_cv_distance_1684x_t api;
    api.Xaddr = bm_mem_get_device_addr(input);
    api.Yaddr = bm_mem_get_device_addr(output);
    api.dim = dim;
    api.len = len;
    if(dtype != DT_FP32 && dtype != DT_FP16)
        api.dtype = DT_FP32;
    else
        api.dtype = dtype;
    if(dtype == DT_FP16) {
        fp16 *pnt16 = (fp16 *)pnt;
        for (int i = 0; i < dim; ++i)
            api.pnt[i] = fp16tofp32(pnt16[i]);
    } else {
        float *pnt32 = (float *)pnt;
        for (int i = 0; i < dim; ++i)
            api.pnt[i] = pnt32[i];
    }
    BM_CHECK_RET(bm_tpu_kernel_launch(handle, "cv_distance", &api, sizeof(api)));
    return ret;
}

bm_status_t bmcv_distance_1684(bm_handle_t handle,
                          bm_device_mem_t input,
                          bm_device_mem_t output,
                          int dim,
                          const float *pnt,
                          int len,
                          int dtype) {
    if (dim <= 0 || dim > 8)
        return BM_ERR_PARAM;
    if (dtype != DT_FP32){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "bm1684 only support fp32, dtype(%d), %s: %s: %d\n", dtype, filename(__FILE__), __func__, __LINE__);
        return BM_ERR_PARAM;
    }
    bm_status_t ret = BM_SUCCESS;
    bm_api_cv_distance_t api;
    api.Xaddr = bm_mem_get_device_addr(input);
    api.Yaddr = bm_mem_get_device_addr(output);
    api.dim = dim;
    api.len = len;
    for (int i = 0; i < dim; ++i)
        api.pnt[i] = pnt[i];

    BM_CHECK_RET(bm_send_api(handle,
                        BM_API_ID_CV_DISTANCE,
                        reinterpret_cast<uint8_t *>(&api),
                        sizeof(api)));
    BM_CHECK_RET(bm_sync_api(handle));

    return ret;
}

bm_status_t bmcv_distance_ext(bm_handle_t handle,
                          bm_device_mem_t input,
                          bm_device_mem_t output,
                          int dim,
                          const void *pnt,
                          int len,
                          int dtype) {

    bm_status_t ret = BM_SUCCESS;
    unsigned int chipid = 0x1686;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
        return ret;
    switch (chipid)
    {
        case 0x1684:{
            BM_CHECK_RET(bmcv_distance_1684(handle, input, output, dim, (const float *)pnt, len, dtype));
            break;
        }
        case 0x1686:{
            BM_CHECK_RET(bmcv_distance_1684x(handle, input, output, dim, pnt, len, dtype));
            break;
        }
        default:{
            ret = BM_NOT_SUPPORTED;
            printf("ChipID is NOT supported\n");
            break;
        }
    }
    return ret;
}

bm_status_t bmcv_distance(bm_handle_t handle,
                          bm_device_mem_t input,
                          bm_device_mem_t output,
                          int dim,
                          const float *pnt,
                          int len) {
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_distance_ext(handle, input, output, dim, (const void *)pnt, len, DT_FP32);
    return ret;
}