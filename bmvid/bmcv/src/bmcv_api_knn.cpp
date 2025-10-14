#include <stdint.h>
#include <stdio.h>
#include <vector>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"

bm_status_t bmcv_knn(
    bm_handle_t handle,
    //output
    bm_device_mem_t centroids_global_addr,//[m_code,  n_feat]
    bm_device_mem_t labels_global_addr,   //[m_obs]
    //input nxn
    bm_device_mem_t input_global_addr,    //[m_obs,   n_feat]
    bm_device_mem_t weight_global_addr,   //[m_code,  n_feat]
    //buffer 2 * max(m_obs, m_code) * 4
    bm_device_mem_t buffer_global_addr,
    int* Shape_Input,
    int* Shape_Weight,
    int  dims_Input,
    int  dims_Weight,
    int  n_feat,
    int  k,
    int  num_iter,
    int  buffer_coeff,
    unsigned int buffer_max_cnt,
    int dtype)
{
    //output
    bm_device_mem_t centroids_global_addr_tpu;
    bm_device_mem_t labels_global_addr_tpu;
    //input nxn
    bm_device_mem_t input_global_addr_tpu;
    bm_device_mem_t weight_global_addr_tpu;
    //buffer 2 * max(m_obs, m_code) * 4
    bm_device_mem_t buffer_global_addr_tpu;

    unsigned int shape_cnt_mobs_nfeat = 1;
    for (int i = 0; i < dims_Input; i++)
    {
        shape_cnt_mobs_nfeat *= Shape_Input[i];
    }
    unsigned int shape_cnt_mcode_nfeat = 1;
    for (int i = 0; i < dims_Weight; i++)
    {
        shape_cnt_mcode_nfeat *= Shape_Weight[i];
    }
    unsigned int shape_labels = shape_cnt_mobs_nfeat/n_feat;

    bm_api_cv_knn api;
    bm_status_t ret = BM_SUCCESS;
    unsigned int chipid;

    ret = bm_get_chipid(handle, &chipid);
    if (ret != BM_SUCCESS) {
        printf("bm_get_chipid failed\n");
        goto err0;
    }

    if (bm_mem_get_type(centroids_global_addr) == BM_MEM_TYPE_SYSTEM) {
            ret = bm_malloc_device_byte(handle, &centroids_global_addr_tpu, sizeof(float) * shape_cnt_mobs_nfeat);
            if (ret != BM_SUCCESS) {
                    bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_malloc_device_byte error\n");
                    goto err1;
            }

            ret = bm_memcpy_s2d(handle, centroids_global_addr_tpu, bm_mem_get_system_addr(centroids_global_addr));
            if ( ret != BM_SUCCESS) {               bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_memcpy_s2d error\n");
                    goto err2;
            }
    } else {
            centroids_global_addr_tpu = centroids_global_addr;
    }

    if (bm_mem_get_type(labels_global_addr) == BM_MEM_TYPE_SYSTEM) {
            ret = bm_malloc_device_byte(handle, &labels_global_addr_tpu, sizeof(float) * shape_labels);
            if (ret != BM_SUCCESS) {
                    bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_malloc_device_byte error\n");
                    goto err1;
            }

            ret = bm_memcpy_s2d(handle, labels_global_addr_tpu, bm_mem_get_system_addr(labels_global_addr));
            if ( ret != BM_SUCCESS) {               bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_memcpy_s2d error\n");
                    goto err2;
            }
    } else {
            labels_global_addr_tpu = labels_global_addr;
    }

    if (bm_mem_get_type(input_global_addr) == BM_MEM_TYPE_SYSTEM) {
            ret = bm_malloc_device_byte(handle, &input_global_addr_tpu, sizeof(float) * shape_cnt_mobs_nfeat);
            if (ret != BM_SUCCESS) {
                    bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_malloc_device_byte error\n");
                    goto err0;
            }

            ret = bm_memcpy_s2d(handle, input_global_addr_tpu, bm_mem_get_system_addr(input_global_addr));
            if ( ret != BM_SUCCESS) {               bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_memcpy_s2d error\n");
                    goto err1;
            }
    } else {
            input_global_addr_tpu = input_global_addr;
    }

    if (bm_mem_get_type(weight_global_addr) == BM_MEM_TYPE_SYSTEM) {
            ret = bm_malloc_device_byte(handle, &weight_global_addr_tpu, sizeof(float) * shape_cnt_mcode_nfeat);
            if (ret != BM_SUCCESS) {
                    bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_malloc_device_byte error\n");
                    goto err0;
            }

            ret = bm_memcpy_s2d(handle, weight_global_addr_tpu, bm_mem_get_system_addr(weight_global_addr));
            if ( ret != BM_SUCCESS) {               bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_memcpy_s2d error\n");
                    goto err1;
            }
    } else {
            weight_global_addr_tpu = weight_global_addr;
    }

    if (bm_mem_get_type(buffer_global_addr) == BM_MEM_TYPE_SYSTEM) {
            ret = bm_malloc_device_byte(handle, &buffer_global_addr_tpu, sizeof(float) * buffer_coeff * buffer_max_cnt);
            if (ret != BM_SUCCESS) {
                    bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_malloc_device_byte error\n");
                    goto err0;
            }

            ret = bm_memcpy_s2d(handle, buffer_global_addr_tpu, bm_mem_get_system_addr(buffer_global_addr));
            if ( ret != BM_SUCCESS) {               bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_memcpy_s2d error\n");
                    goto err1;
            }
    } else {
            buffer_global_addr_tpu = buffer_global_addr;
    }

    api.centroids_global_addr = bm_mem_get_device_addr(centroids_global_addr_tpu);
    api.labels_global_addr    = bm_mem_get_device_addr(labels_global_addr_tpu);
    api.input_global_addr     = bm_mem_get_device_addr(input_global_addr_tpu);
    api.weight_global_addr    = bm_mem_get_device_addr(weight_global_addr_tpu);
    api.buffer_global_addr    = bm_mem_get_device_addr(buffer_global_addr_tpu);
    memcpy(api.Shape_Input,  Shape_Input,  sizeof(int) * dims_Input);
    memcpy(api.Shape_Weight, Shape_Weight, sizeof(int) * dims_Weight);
    api.dims_Input  = dims_Input;
    api.dims_Weight = dims_Weight;
    api.k           = k;
    api.num_iter    = num_iter;
    api.dtype       = dtype;

    switch(chipid)
    {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_knn", &api, sizeof(api));
            if (ret != BM_SUCCESS) {
                bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_tpu_kernel_launch error\n");
                goto err2;
            }
            break;

        default:
            printf("BM_NOT_SUPPORTED!\n");
            break;
    }

    /* copy and free */
    if (bm_mem_get_type(centroids_global_addr) == BM_MEM_TYPE_SYSTEM) {
            ret = bm_memcpy_d2s(handle, bm_mem_get_system_addr(centroids_global_addr), centroids_global_addr_tpu);
            if (ret != BM_SUCCESS) {
                    bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_memcpy_d2s error\n");
                    goto err2;
            }
    }

    if (bm_mem_get_type(labels_global_addr) == BM_MEM_TYPE_SYSTEM) {
            ret = bm_memcpy_d2s(handle, bm_mem_get_system_addr(labels_global_addr), labels_global_addr_tpu);
            if (ret != BM_SUCCESS) {
                    bmlib_log("KNN", BMLIB_LOG_ERROR, "bm_memcpy_d2s error\n");
                    goto err2;
            }
    }

err2:
    if (bm_mem_get_type(centroids_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, centroids_global_addr_tpu);
    }
    if (bm_mem_get_type(labels_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, labels_global_addr_tpu);
    }
err1:
    if (bm_mem_get_type(input_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, input_global_addr_tpu);
    }
    if (bm_mem_get_type(weight_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, weight_global_addr_tpu);
    }
    if (bm_mem_get_type(buffer_global_addr) == BM_MEM_TYPE_SYSTEM) {
        bm_free_device(handle, buffer_global_addr_tpu);
    }
err0:
    return ret;
}