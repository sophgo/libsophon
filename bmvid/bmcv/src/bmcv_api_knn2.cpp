#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"

bm_status_t bmcv_knn2(
    bm_handle_t handle,
    bm_device_mem_t ref_data_addr,
    bm_device_mem_t test_data_addr,
    bm_device_mem_t distance_addr,
    bm_device_mem_t indices_addr,
    int n_test,
    int n_ref,
    int n_feat,
    int k) {
    if (k < 0 || k > n_ref) {
        printf("[ERROR] k must be between 1 and number of ref data\n");
        return BM_ERR_PARAM;
    }
    bm_device_mem_t ref_data_dev_mem;       // [n_ref, n_feat]
    bm_device_mem_t test_data_dev_mem;      // [n_test, n_feat]
    bm_device_mem_t buffer_dist_to_sort_dev_mem;   // [n_test, n_ref]
    bm_device_mem_t distance_tpu_dev_mem;   // [n_test, k]
    bm_device_mem_t indices_tpu_dev_mem;    // [n_test, k]

    bm_api_cv_knn2_t api;
    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    if (bm_mem_get_type(ref_data_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                &ref_data_dev_mem,
                                                n_ref * n_feat * sizeof(float))) {
            BMCV_ERR_LOG("bm_malloc_device_byte ref_data_dev_mem failed.\r\n");
            ret = BM_ERR_NOMEM;
            goto exit0;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          ref_data_dev_mem,
                          bm_mem_get_system_addr(ref_data_addr))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");
            ret = BM_ERR_NOMEM;
            goto exit1;
        }
    } else {
        ref_data_dev_mem = ref_data_addr;
    }

    if (bm_mem_get_type(test_data_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                &test_data_dev_mem,
                                                n_test * n_feat * sizeof(float))) {
            BMCV_ERR_LOG("bm_malloc_device_byte test_data_dev_mem failed.\r\n");
            ret = BM_ERR_NOMEM;
            goto exit1;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          test_data_dev_mem,
                          bm_mem_get_system_addr(test_data_addr))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");
            ret = BM_ERR_NOMEM;
            goto exit2;
        }
    } else {
        test_data_dev_mem = test_data_addr;
    }

    ret = bm_malloc_device_byte(handle, &buffer_dist_to_sort_dev_mem, n_test * n_ref * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_btye dist_to_sort failed!\n");
        goto exit2;
    }

    if (bm_mem_get_type(distance_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                &distance_tpu_dev_mem,
                                                n_test * k * sizeof(float))) {
            BMCV_ERR_LOG("bm_malloc_device_byte distance_tpu_dev_mem failed.\r\n");
            ret = BM_ERR_NOMEM;
            goto exit3;
        }
    } else {
        distance_tpu_dev_mem = distance_addr;
    }

    if (bm_mem_get_type(indices_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                &indices_tpu_dev_mem,
                                                n_test * k * sizeof(int))) {
            BMCV_ERR_LOG("bm_malloc_device_byte indices_tpu_dev_mem failed.\r\n");
            ret = BM_ERR_NOMEM;
            goto exit4;
        }
    } else {
        indices_tpu_dev_mem = indices_addr;
    }

    ret = bm_get_chipid(handle, &chipid);
    if (ret != BM_SUCCESS) {
        printf("bm_get_chipid failed\n");
        goto exit5;
    }

    api.ref_data_dev_mem = bm_mem_get_device_addr(ref_data_dev_mem);
    api.test_data_dev_mem = bm_mem_get_device_addr(test_data_dev_mem);
    api.dist_to_sort_dev_mem = bm_mem_get_device_addr(buffer_dist_to_sort_dev_mem);
    api.distance_tpu_dev_mem = bm_mem_get_device_addr(distance_tpu_dev_mem);
    api.indices_tpu_dev_mem = bm_mem_get_device_addr(indices_tpu_dev_mem);
    api.n_ref = n_ref;
    api.n_test = n_test;
    api.n_feat = n_feat;
    api.k = k;

    switch(chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "sg_cv_knn2", &api, sizeof(api));
            if (ret != BM_SUCCESS) {
                bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_tpu_kernel_launch error\n");
                return ret;
            }
            break;

        default:
            printf("BM_NOT_SUPPORTED!\n");
            ret = BM_NOT_SUPPORTED;
            break;
    }

    if (bm_mem_get_type(distance_addr) == BM_MEM_TYPE_SYSTEM) {
        ret = bm_memcpy_d2s(handle, bm_mem_get_system_addr(distance_addr), distance_tpu_dev_mem);
        if (ret != BM_SUCCESS) {
            bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_memcpy_d2s error\n");
            goto exit5;
        }
    }

    if (bm_mem_get_type(indices_addr) == BM_MEM_TYPE_SYSTEM) {
        ret = bm_memcpy_d2s(handle, bm_mem_get_system_addr(indices_addr), indices_tpu_dev_mem);
        if (ret != BM_SUCCESS) {
            bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_memcpy_d2s error\n");
            goto exit5;
        }
    }

exit5:
    if (bm_mem_get_type(indices_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, indices_tpu_dev_mem);
    }
exit4:
    if (bm_mem_get_type(distance_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, distance_tpu_dev_mem);
    }
exit3:
    bm_free_device(handle, buffer_dist_to_sort_dev_mem);
exit2:
    if (bm_mem_get_type(test_data_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, test_data_dev_mem);
    }
exit1:
    if (bm_mem_get_type(ref_data_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, ref_data_dev_mem);
    }
exit0:
    return ret;
}