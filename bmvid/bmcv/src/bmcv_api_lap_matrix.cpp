#include <stdint.h>
#include <stdio.h>
#include <vector>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"

bm_status_t bmcv_lap_matrix(bm_handle_t handle, bm_device_mem_t input, bm_device_mem_t output,
                            int row, int col)
{
    bm_device_mem_t input_tpu, output_tpu;
    bm_api_cv_lap_matrix api;
    bm_status_t ret = BM_SUCCESS;
    unsigned int chipid;
    bm_device_mem_t res_addr, const_val_addr;
    float const_value = 1.f;

    ret = bm_get_chipid(handle, &chipid);
    if (ret != BM_SUCCESS) {
        printf("bm_get_chipid failed\n");
        goto err0;
    }

    if (bm_mem_get_type(input) == BM_MEM_TYPE_SYSTEM) {
        ret = bm_malloc_device_byte(handle, &input_tpu, sizeof(float) * row * col);
        if (ret != BM_SUCCESS) {
            bmlib_log("LAP_MATRIX", BMLIB_LOG_ERROR, "bm_malloc_device_byte error\n");
            goto err0;
        }

        ret = bm_memcpy_s2d(handle, input_tpu, bm_mem_get_system_addr(input));
        if (ret != BM_SUCCESS) {
            bmlib_log("LAP_MATRIX", BMLIB_LOG_ERROR, "bm_memcpy_s2d error\n");
            goto err1;
        }
    } else {
        input_tpu = input;
    }

    if (bm_mem_get_type(output) == BM_MEM_TYPE_SYSTEM) {
        ret = bm_malloc_device_byte(handle, &output_tpu, sizeof(float) * row * col);
        if (ret != BM_SUCCESS) {
            bmlib_log("LAP_MATRIX", BMLIB_LOG_ERROR, "bm_malloc_device_byte error\n");
            goto err1;
        }

        ret = bm_memcpy_s2d(handle, output_tpu, bm_mem_get_system_addr(output));
        if (ret != BM_SUCCESS) {
            bmlib_log("LAP_MATRIX", BMLIB_LOG_ERROR, "bm_memcpy_s2d error\n");
            goto err2;
        }
    } else {
        output_tpu = output;
    }

    ret = bm_malloc_device_byte(handle, &res_addr, sizeof(float) * row);
    if (ret != BM_SUCCESS) {
        bmlib_log("LAP_MATRIX", BMLIB_LOG_ERROR, "bm_malloc_device_byte error\n");
        goto err2;
    }

    ret = bm_malloc_device_byte(handle, &const_val_addr, sizeof(float) * col);
    if (ret != BM_SUCCESS) {
        bmlib_log("LAP_MATRIX", BMLIB_LOG_ERROR, "bm_malloc_device_byte error\n");
        goto err3;
    }

    ret = bm_memset_device_ext(handle, &const_value, 4, const_val_addr);
    if (ret != BM_SUCCESS) {
        bmlib_log("LAP_MATRIX", BMLIB_LOG_ERROR, "bm_memset_device_ext error\n");
        goto err4;
    }

    api.input_device_addr = bm_mem_get_device_addr(input_tpu);
    api.output_device_addr = bm_mem_get_device_addr(output_tpu);
    api.res_addr = bm_mem_get_device_addr(res_addr);
    api.const_val_addr = bm_mem_get_device_addr(const_val_addr);
    api.row = row;
    api.col = col;

    switch(chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_lap_matrix", &api, sizeof(api));
            if (ret != BM_SUCCESS) {
                bmlib_log("LAP_MATRIX", BMLIB_LOG_ERROR, "bm_tpu_kernel_launch error\n");
                goto err4;
            }
            break;

        default:
            printf("BM_NOT_SUPPORTED!\n");
            break;
    }

    /* copy and free */
    if (bm_mem_get_type(output) == BM_MEM_TYPE_SYSTEM) {
        ret = bm_memcpy_d2s(handle, bm_mem_get_system_addr(output), output_tpu);
        if (ret != BM_SUCCESS) {
            bmlib_log("LAP_MATRIX", BMLIB_LOG_ERROR, "bm_memcpy_d2s error\n");
            goto err4;
        }
    }

err4:
    bm_free_device(handle, const_val_addr);
err3:
    bm_free_device(handle, res_addr);
err2:
    if (bm_mem_get_type(output) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, output_tpu);
    }
err1:
    if (bm_mem_get_type(input) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_tpu);
    }
err0:
    return ret;
}