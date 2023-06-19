#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include <cmath>
#include <vector>
#include <thread>

bm_status_t bmcv_cmulp(bm_handle_t handle,
                       bm_device_mem_t inputReal,
                       bm_device_mem_t inputImag,
                       bm_device_mem_t pointReal,
                       bm_device_mem_t pointImag,
                       bm_device_mem_t outputReal,
                       bm_device_mem_t outputImag,
                       int batch,
                       int len) {
    bm_status_t ret = BM_SUCCESS;
    bm_api_cv_cmulp_t api;
    api.XR = bm_mem_get_device_addr(inputReal);
    api.XI = bm_mem_get_device_addr(inputImag);
    api.PR = bm_mem_get_device_addr(pointReal);
    api.PI = bm_mem_get_device_addr(pointImag);
    api.YR = bm_mem_get_device_addr(outputReal);
    api.YI = bm_mem_get_device_addr(outputImag);
    api.batch = batch;
    api.len = len;

    unsigned int chipid;
    bm_get_chipid(handle, &chipid);

    switch (chipid)
    {
        case 0x1684:
            BM_CHECK_RET(bm_send_api(handle,
                                     BM_API_ID_CV_CMULP,
                                     reinterpret_cast<uint8_t *>(&api),
                                     sizeof(api)));
            BM_CHECK_RET(bm_sync_api(handle));
            break;

        case BM1684X:
            BM_CHECK_RET(bm_tpu_kernel_launch(handle, "cv_cmulp", &api, sizeof(api)));
            // tpu_kernel_launch_sync(handle, "cv_cmulp", &api, sizeof(api));
            break;

        default:
            printf("ChipID is NOT supported\n");
            break;
    }

    return ret;
}
