#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"
#include "bmlib_runtime.h"

static bm_status_t bmcv_matrix_prune_check(
        bm_handle_t handle,
        int matrix_dims,
        float p) {
    if (handle == NULL) {
        bmlib_log("matrix_prune", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    if (matrix_dims > 6000) {
        bmlib_log("matrix_prune", BMLIB_LOG_ERROR, "Unsupported size : max_matrix_dims: 6000! \r\n");
        return BM_ERR_PARAM;
    }
    if (matrix_dims < 8) {
        bmlib_log("matrix_prune", BMLIB_LOG_ERROR, "Unsupported size : min_matrix_dims: 8 \r\n");
        return BM_ERR_PARAM;
    }
    if (p < 0.0 || p > 1.0) {
        bmlib_log("matrix_prune", BMLIB_LOG_ERROR, "Unsupported size : p range: 0.0f~1.0f \r\n");
        return BM_ERR_PARAM;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_matrix_prune(bm_handle_t handle,
                              bm_device_mem_t input_data_global_addr,
                              bm_device_mem_t output_data_global_addr,
                              bm_device_mem_t sort_index_global_addr,
                              int matrix_dims,
                              float p) {
    bm_status_t ret = BM_SUCCESS;
    bm_device_mem_t sort_data_global_addr, sort_trans_global_addr;

    ret = bmcv_matrix_prune_check(handle, matrix_dims, p);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    ret = bm_malloc_device_byte(handle, &sort_data_global_addr, sizeof(float) * matrix_dims * matrix_dims);
    if (BM_SUCCESS != ret) {
        bmlib_log("MATRIX_PRUNE", BMLIB_LOG_ERROR, "malloc sort_data_global_addr failed!\r\n");
        return BM_ERR_FAILURE;
    }
    ret = bm_malloc_device_byte(handle, &sort_trans_global_addr, sizeof(float) * matrix_dims * matrix_dims);
    if (BM_SUCCESS != ret) {
        bmlib_log("MATRIX_PRUNE", BMLIB_LOG_ERROR, "malloc sort_trans_global_addr failed!\r\n");
        bm_free_device(handle, sort_data_global_addr);
        return BM_ERR_FAILURE;
    }
    sg_api_cv_matrix_prune_t api;
    api.input_data_global_addr = bm_mem_get_device_addr(input_data_global_addr);
    api.output_data_global_addr = bm_mem_get_device_addr(output_data_global_addr);
    api.sort_data_global_addr = bm_mem_get_device_addr(sort_data_global_addr);
    api.sort_trans_global_addr = bm_mem_get_device_addr(sort_trans_global_addr);
    api.sort_index_global_addr = bm_mem_get_device_addr(sort_index_global_addr);
    api.matrix_dims = matrix_dims;
    api.p = p;
    unsigned int chipid;
    bm_get_chipid(handle, &chipid);

    switch (chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_matrix_prune", &api, sizeof(api));
            if(BM_SUCCESS != ret){
                bmlib_log("matrix_prune", BMLIB_LOG_ERROR, "matrix_prune sync api error\n");
                return BM_ERR_FAILURE;
            }
            break;
        default:
            bmlib_log("matrix_prune", BMLIB_LOG_ERROR, "ChipID is NOT supported\n");
            break;
    }
    bm_free_device(handle, sort_data_global_addr);
    bm_free_device(handle, sort_trans_global_addr);
    return BM_SUCCESS;
}
