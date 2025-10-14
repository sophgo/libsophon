#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"

bm_status_t bmcv_knn_match(
        bm_handle_t handle,
        bm_device_mem_t ref_addr,
        bm_device_mem_t test_addr,
        bm_device_mem_t distance_addr,
        bm_device_mem_t good_match_addr,
        bm_device_mem_t match_index_addr,
        int n_ref,
        int n_ref_feat,
        int n_test_feat,
        int n_descriptor,
        float ratio_thresh) {
    bm_device_mem_t ref_data_dev_mem;       // [n_ref x n_ref_feat, n_descriptor]
    bm_device_mem_t test_data_dev_mem;      // [n_test_feat, n_descriptor]
    bm_device_mem_t buffer_dist_to_sort_dev_mem;   // [n_ref_feat]
    bm_device_mem_t distance_tpu_dev_mem;   // [n_ref x n_test_feat, k]
    bm_device_mem_t good_match_dev_mem;     // [2 * n_ref]
    bm_device_mem_t index_sorted_dev_mem;   // [n_ref]
    int k = 2;

    bm_status_t ret = BM_SUCCESS;
    bm_api_cv_knn_match_t api;
    unsigned int chipid = BM1684X;

    if (bm_mem_get_type(ref_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                &ref_data_dev_mem,
                                                n_ref * n_ref_feat * n_descriptor * sizeof(int))) {
            BMCV_ERR_LOG("bm_malloc_device_byte ref_data_dev_mem failed.\r\n");
            ret = BM_ERR_NOMEM;
            goto exit0;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          ref_data_dev_mem,
                          bm_mem_get_system_addr(ref_addr))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");
            ret = BM_ERR_NOMEM;
            goto exit1;
        }
    } else {
        ref_data_dev_mem = ref_addr;
    }

    if (bm_mem_get_type(test_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                &test_data_dev_mem,
                                                n_test_feat * n_descriptor * sizeof(int))) {
            BMCV_ERR_LOG("bm_malloc_device_byte teste_data_dev_mem failed.\r\n");
            ret = BM_ERR_NOMEM;
            goto exit1;
        }
        if (BM_SUCCESS !=
            bm_memcpy_s2d(handle,
                          test_data_dev_mem,
                          bm_mem_get_system_addr(test_addr))) {
            BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");
            ret = BM_ERR_NOMEM;
            goto exit2;
        }
    } else {
        test_data_dev_mem = test_addr;
    }

    ret = bm_malloc_device_byte(handle, &buffer_dist_to_sort_dev_mem, n_ref * n_ref_feat * n_test_feat * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_btye dist_to_sort failed!\n");
        goto exit3;
    }

    if (bm_mem_get_type(distance_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                &distance_tpu_dev_mem,
                                                n_ref * n_test_feat * k * sizeof(float))) {
            BMCV_ERR_LOG("bm_malloc_device_byte distance_tpu_dev_mem failed.\r\n");
            ret = BM_ERR_NOMEM;
            goto exit3;
        }
    } else {
        distance_tpu_dev_mem = distance_addr;
    }

    if (bm_mem_get_type(good_match_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                &good_match_dev_mem,
                                                2 * n_ref * sizeof(int))) {
            BMCV_ERR_LOG("bm_malloc_device_byte good_match_dev_mem failed.\r\n");
            ret = BM_ERR_NOMEM;
            goto exit4;
        }
    } else {
        good_match_dev_mem = good_match_addr;
    }

    if (bm_mem_get_type(match_index_addr) == BM_MEM_TYPE_SYSTEM) {
        if (BM_SUCCESS != bm_malloc_device_byte(handle,
                                                &index_sorted_dev_mem,
                                                n_ref * sizeof(int))) {
            BMCV_ERR_LOG("bm_malloc_device_byte match_index_dev_mem failed.\r\n");
            ret = BM_ERR_NOMEM;
            goto exit5;
        }
    } else {
        index_sorted_dev_mem = match_index_addr;
    }

    ret = bm_get_chipid(handle, &chipid);
    if (ret != BM_SUCCESS) {
        printf("bm_get_chipid failed\n");
        goto exit6;
    }

    api.ref_data_dev_mem = bm_mem_get_device_addr(ref_data_dev_mem);
    api.test_data_dev_mem = bm_mem_get_device_addr(test_data_dev_mem);
    api.dist_to_sort_dev_mem = bm_mem_get_device_addr(buffer_dist_to_sort_dev_mem);
    api.distance_tpu_dev_mem = bm_mem_get_device_addr(distance_tpu_dev_mem);
    api.good_match_dev_mem = bm_mem_get_device_addr(good_match_dev_mem);
    api.index_sorted_dev_mem = bm_mem_get_device_addr(index_sorted_dev_mem);
    api.n_ref = n_ref;
    api.n_ref_feat = n_ref_feat;
    api.n_test_feat = n_test_feat;
    api.n_descriptor = n_descriptor;
    api.k = k;
    api.ratio_thresh = ratio_thresh;


    switch(chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "sg_cv_knn_match", &api, sizeof(api));
            if (ret != BM_SUCCESS) {
                bmlib_log("KNN_match", BMLIB_LOG_ERROR, "bm_tpu_kernel_launch error\n");
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
            goto exit6;
        }
    }
    if (bm_mem_get_type(good_match_addr) == BM_MEM_TYPE_SYSTEM) {
        ret = bm_memcpy_d2s(handle, bm_mem_get_system_addr(good_match_addr), good_match_dev_mem);
        if (ret != BM_SUCCESS) {
            bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_memcpy_d2s error\n");
            goto exit6;
        }
    }
    if (bm_mem_get_type(match_index_addr) == BM_MEM_TYPE_SYSTEM) {
        ret = bm_memcpy_d2s(handle, bm_mem_get_system_addr(match_index_addr), index_sorted_dev_mem);
        if (ret != BM_SUCCESS) {
            bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_memcpy_d2s error\n");
            goto exit6;
        }
    }

exit6:
    if (bm_mem_get_type(match_index_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, index_sorted_dev_mem);
    }
exit5:
    if (bm_mem_get_type(good_match_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, good_match_dev_mem);
    }
exit4:
    if (bm_mem_get_type(distance_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, distance_tpu_dev_mem);
    }
exit3:
    bm_free_device(handle, buffer_dist_to_sort_dev_mem);
exit2:
    if (bm_mem_get_type(test_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, test_data_dev_mem);
    }
exit1:
    if (bm_mem_get_type(ref_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, ref_data_dev_mem);
    }
exit0:
    return ret;
}
