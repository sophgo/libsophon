
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"
#include "bmlib_runtime.h"

static bm_status_t bmcv_cos_similarity_check(
        bm_handle_t handle,
        int vec_num,
        int vec_dims) {
    if (handle == NULL) {
        bmlib_log("cos_similarity", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    if (vec_dims > 256 || vec_num > 6000) {
        bmlib_log("cos_similarity", BMLIB_LOG_ERROR, "Unsupported size : max_vec_dims: 256  max_vec_num:6000!\r\n");
        return BM_ERR_PARAM;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_cos_similarity(bm_handle_t handle,
                                bm_device_mem_t input_data_global_addr,
                                bm_device_mem_t normalized_data_global_addr,
                                bm_device_mem_t output_data_global_addr,
                                int vec_num,
                                int vec_dims) {
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_cos_similarity_check(handle, vec_num, vec_dims);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    sg_api_cv_cos_similarity_t api;
    api.input_data_global_addr = bm_mem_get_device_addr(input_data_global_addr);
    api.normalized_data_global_addr = bm_mem_get_device_addr(normalized_data_global_addr);
    api.output_data_global_addr = bm_mem_get_device_addr(output_data_global_addr);
    api.vec_dims = vec_dims;
    api.vec_num = vec_num;
    unsigned int chipid;
    bm_get_chipid(handle, &chipid);

    switch (chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_cos_similarity", &api, sizeof(api));
            if(BM_SUCCESS != ret){
                bmlib_log("cos_similarity", BMLIB_LOG_ERROR, "cos_similarity sync api error\n");
                return BM_ERR_FAILURE;
            }
            break;

        default:
            bmlib_log("cos_similarity", BMLIB_LOG_ERROR, "ChipID is NOT supported\n");
            break;
    }

    return BM_SUCCESS;
}

