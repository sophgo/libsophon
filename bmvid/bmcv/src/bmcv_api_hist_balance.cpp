#include "bmcv_api_ext_c.h"
#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

#define GRAY_SERIES 256
#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))
#define HIST_BALANCE_LEN_MAX 32640

static bm_status_t bmcv_param_check(int len)
{
    if(len < 0 || len > HIST_BALANCE_LEN_MAX) {
        printf("the bmcv_param_check failed, the len sholud larger than 0 & smaller than 32640\n");
        return BM_ERR_PARAM;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_hist_balance(bm_handle_t handle, bm_device_mem_t input, bm_device_mem_t output,
                            int H, int W, int batch)
{
    bm_status_t ret = BM_SUCCESS;
    bm_api_cv_hist_balance_t api;
    int imageSize = H * W;
    bm_device_mem_t cdf, cdf_index;
    uint8_t* cdf_sys = NULL;
    int i;

    ret = bmcv_param_check(imageSize);
    if (ret != BM_SUCCESS) {
        printf("the param check failed!\n");
        return ret;
    }

    cdf_sys = (uint8_t*)malloc(GRAY_SERIES * sizeof(uint8_t));
    memset(cdf_sys, 0, GRAY_SERIES * sizeof(uint8_t));

    for (i = 0; i < GRAY_SERIES; ++i) {
        cdf_sys[i] = i;
    }

    ret = bm_malloc_device_byte(handle, &cdf, GRAY_SERIES * sizeof(uint8_t));
    if (ret != BM_SUCCESS) {
        printf("the bm_malloc_device_byte failed!\n");
        free(cdf_sys);
        return ret;
    }

    ret = bm_malloc_device_byte(handle, &cdf_index, GRAY_SERIES * sizeof(uint32_t));
    if (ret != BM_SUCCESS) {
        printf("the bm_malloc_device_byte failed!\n");
        free(cdf_sys);
        bm_free_device(handle, cdf);
        return ret;
    }

    ret = bm_memcpy_s2d(handle, cdf, cdf_sys);
    if (ret) {
        printf("bm_memcpy_s2d failed!\n");
        goto exit;
    }

    for (i = 0; i < batch ; ++i) {
        api.Xaddr = bm_mem_get_device_addr(input) + i * imageSize * sizeof(uint8_t);
        api.len = imageSize;
        api.cdf_addr = bm_mem_get_device_addr(cdf);
        api.cdf_index_addr = bm_mem_get_device_addr(cdf_index);
        api.Yaddr = bm_mem_get_device_addr(output) + i * imageSize * sizeof(uint8_t);
        ret = bm_tpu_kernel_launch(handle, "cv_hist_balance", (u8*)(&api), sizeof(api));
        if (ret != BM_SUCCESS) {
            bmlib_log("HIST_BALANCE", BMLIB_LOG_ERROR, "hist_balance sync api error\n");
            goto exit;
        }
    }

exit:
    free(cdf_sys);
    bm_free_device(handle, cdf);
    bm_free_device(handle, cdf_index);

    return ret;
}