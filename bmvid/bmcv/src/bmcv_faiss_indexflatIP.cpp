#include "bmcv_api_ext.h"
#include "bmcv_internal.h"

typedef struct {
    unsigned long long input_query_global_addr;
    unsigned long long database_global_addr;
    unsigned long long buffer_global_addr;
    unsigned long long output_sorted_similarity_global_addr;
    unsigned long long output_sorted_index_global_addr;
    unsigned long long output_sorted_buffer_fp32_similarity_global_addr;
    int vec_dims;
    int query_vecs_num;
    int database_vecs_num;
    int k;
    int transpose;
    int input_dtype;
    int output_dtype;
} __attribute__((packed)) faiss_api_indexflatIP_t;


bm_status_t bmcv_faiss_indexflatIP_check(int database_vecs_num, int sort_cnt, int query_vecs_num, int input_dtype, int output_dtype) {
    switch (input_dtype) {
        case 5:
            if (output_dtype != 3 && output_dtype != 5) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_api_indexflatIP when input_dtype = fp32, output_dtype should be fp16/fp32!\n%s: %s: %d\n",
                        filename(__FILE__), __func__, __LINE__);
                return BM_NOT_SUPPORTED;
            }
            break;
        case 3:
            if (output_dtype != 3 && output_dtype != 5) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_api_indexflatIP when input_dtype = fp16, output_dtype should be fp16/fp32!\n%s: %s: %d\n",
                        filename(__FILE__), __func__, __LINE__);
                return BM_NOT_SUPPORTED;
            }
            break;
        case 1:
            if (output_dtype != 9) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_api_indexflatIP when input_dtype = char, output_dtype should be int!\n%s: %s: %d\n",
                        filename(__FILE__), __func__, __LINE__);
                return BM_NOT_SUPPORTED;
            }
            break;
        default:
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_api_indexflatIP input_dtype should be fp32/fp16/char!\n%s: %s: %d\n",
                        filename(__FILE__), __func__, __LINE__);
            return BM_NOT_SUPPORTED;
    }

    if (database_vecs_num < sort_cnt) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_api_indexflatIP database_vecs_num(%d) < sort_cnt(%d), %s: %s: %d\n",
                                        database_vecs_num, sort_cnt, filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
    if (database_vecs_num < query_vecs_num) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_api_indexflatIP database_vecs_num(%d) < query_vecs_num(%d), %s: %s: %d\n",
                                        database_vecs_num, query_vecs_num, filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_faiss_indexflatIP(bm_handle_t handle,
                                   bm_device_mem_t input_data_global_addr,
                                   bm_device_mem_t db_data_global_addr,
                                   bm_device_mem_t buffer_global_addr,
                                   bm_device_mem_t output_sorted_similarity_global_addr,
                                   bm_device_mem_t output_sorted_index_global_addr,
                                   int vec_dims,
                                   int query_vecs_num,
                                   int database_vecs_num,
                                   int sort_cnt,
                                   int is_transpose,
                                   int input_dtype,
                                   int output_dtype) {
    faiss_api_indexflatIP_t api;
    bm_status_t ret = BM_SUCCESS;
    bm_device_mem_t output_sorted_buffer_fp32_similarity_global_addr;
    if(input_dtype == DT_FP32 && output_dtype == DT_FP16) {
        ret = bm_malloc_device_byte(handle, &output_sorted_buffer_fp32_similarity_global_addr, sort_cnt * query_vecs_num * sizeof(float));
        if(ret != BM_SUCCESS) {
            bmlib_log("bmcv_faiss_indexflatIP", BM_ERR_FAILURE, "bm_malloc_device_byte output_sorted_buffer_fp32_similarity_global_addr failed\n");
            return ret;
        }
    }
    api.input_query_global_addr = bm_mem_get_device_addr(input_data_global_addr);
    api.database_global_addr = bm_mem_get_device_addr(db_data_global_addr);
    api.buffer_global_addr = bm_mem_get_device_addr(buffer_global_addr);
    api.output_sorted_similarity_global_addr = bm_mem_get_device_addr(output_sorted_similarity_global_addr);
    api.output_sorted_index_global_addr = bm_mem_get_device_addr(output_sorted_index_global_addr);
    api.output_sorted_buffer_fp32_similarity_global_addr = bm_mem_get_device_addr(output_sorted_buffer_fp32_similarity_global_addr);
    api.vec_dims = vec_dims;
    api.query_vecs_num = query_vecs_num;
    api.database_vecs_num = database_vecs_num;
    api.k = sort_cnt;
    api.transpose = is_transpose;
    api.input_dtype = input_dtype;
    api.output_dtype = output_dtype;

    ret = bmcv_faiss_indexflatIP_check(database_vecs_num, sort_cnt, query_vecs_num, input_dtype, output_dtype);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "bmcv_faiss_indexflatIP_check failed, %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return ret;
    }
    unsigned int chipid;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_api_indexflatIP bm_get_chipid failed!, %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return ret;
    }

    switch (chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "faiss_api_indexflatIP", &api, sizeof(api));
            if (BM_SUCCESS != ret) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_indexflatIP launch_sync api error!, %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
            }
            break;
        default:
            ret = BM_NOT_SUPPORTED;
            break;
    }
    if(input_dtype == DT_FP32 && output_dtype == DT_FP16) {
        bm_free_device(handle, output_sorted_buffer_fp32_similarity_global_addr);
    }
    return ret;
}

bm_status_t bmcv_faiss_indexflatIP_u64(bm_handle_t handle,
                                   bm_device_mem_u64_t input_data_global_addr,
                                   bm_device_mem_u64_t db_data_global_addr,
                                   bm_device_mem_u64_t buffer_global_addr,
                                   bm_device_mem_u64_t output_sorted_similarity_global_addr,
                                   bm_device_mem_u64_t output_sorted_index_global_addr,
                                   int vec_dims,
                                   int query_vecs_num,
                                   int database_vecs_num,
                                   int sort_cnt,
                                   int is_transpose,
                                   int input_dtype,
                                   int output_dtype) {
    faiss_api_indexflatIP_t api;
    bm_status_t ret = BM_SUCCESS;
    bm_device_mem_u64_t output_sorted_buffer_fp32_similarity_global_addr;
    if(input_dtype == DT_FP32 && output_dtype == DT_FP16) {
        ret = bm_malloc_device_byte_u64(handle, &output_sorted_buffer_fp32_similarity_global_addr, sort_cnt * query_vecs_num * sizeof(float));
        if(ret != BM_SUCCESS) {
            bmlib_log("bmcv_faiss_indexflatIP_u64", BM_ERR_FAILURE, "bm_malloc_device_byte_u64 output_sorted_buffer_fp32_similarity_global_addr failed\n");
            return ret;
        }
    }
    api.input_query_global_addr = bm_mem_get_device_addr_u64(input_data_global_addr);
    api.database_global_addr = bm_mem_get_device_addr_u64(db_data_global_addr);
    api.buffer_global_addr = bm_mem_get_device_addr_u64(buffer_global_addr);
    api.output_sorted_similarity_global_addr = bm_mem_get_device_addr_u64(output_sorted_similarity_global_addr);
    api.output_sorted_index_global_addr = bm_mem_get_device_addr_u64(output_sorted_index_global_addr);
    api.output_sorted_buffer_fp32_similarity_global_addr = bm_mem_get_device_addr_u64(output_sorted_buffer_fp32_similarity_global_addr);
    api.vec_dims = vec_dims;
    api.query_vecs_num = query_vecs_num;
    api.database_vecs_num = database_vecs_num;
    api.k = sort_cnt;
    api.transpose = is_transpose;
    api.input_dtype = input_dtype;
    api.output_dtype = output_dtype;

    ret = bmcv_faiss_indexflatIP_check(database_vecs_num, sort_cnt, query_vecs_num, input_dtype, output_dtype);
    if (ret != BM_SUCCESS) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "bmcv_faiss_indexflatIP_check failed, %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return ret;
    }
    unsigned int chipid;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_api_indexflatIP_u64 bm_get_chipid failed!, %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
        return ret;
    }

    switch (chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "faiss_api_indexflatIP", &api, sizeof(api));
            if (BM_SUCCESS != ret) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_indexflatIP_u64 launch_sync api error!, %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
            }
            break;
        default:
            ret = BM_NOT_SUPPORTED;
            break;
    }
    if(input_dtype == DT_FP32 && output_dtype == DT_FP16) {
        bm_free_device_u64(handle, output_sorted_buffer_fp32_similarity_global_addr);
    }
    return ret;
}