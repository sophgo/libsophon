#include "bmcv_api_ext.h"
#include "bmcv_internal.h"

#pragma pack(push, 1)
typedef struct {
    unsigned long long input_query_global_addr;
    unsigned long long database_global_addr;
    unsigned long long query_L2norm_global_addr;
    unsigned long long db_L2norm_global_addr;
    unsigned long long buffer_global_addr;
    unsigned long long output_sorted_similarity_global_addr;
    unsigned long long output_sorted_index_global_addr;
    int vec_dims;
    int query_vecs_num;
    int database_vecs_num;
    int k;
    int transpose;
    int input_dtype;
    int output_dtype;
} faiss_api_indexflatL2_t;
#pragma pack(pop)

bm_status_t bmcv_faiss_indexflatL2(bm_handle_t handle,
                                   bm_device_mem_t input_data_global_addr,
                                   bm_device_mem_t db_data_global_addr,
                                   bm_device_mem_t query_L2norm_global_addr,
                                   bm_device_mem_t db_L2norm_global_addr,
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
    faiss_api_indexflatL2_t api;
    api.input_query_global_addr = bm_mem_get_device_addr(input_data_global_addr);
    api.database_global_addr = bm_mem_get_device_addr(db_data_global_addr);
    api.query_L2norm_global_addr = bm_mem_get_device_addr(query_L2norm_global_addr);
    api.db_L2norm_global_addr = bm_mem_get_device_addr(db_L2norm_global_addr);
    api.buffer_global_addr = bm_mem_get_device_addr(buffer_global_addr);
    api.output_sorted_similarity_global_addr = bm_mem_get_device_addr(output_sorted_similarity_global_addr);
    api.output_sorted_index_global_addr = bm_mem_get_device_addr(output_sorted_index_global_addr);
    api.vec_dims = vec_dims;
    api.query_vecs_num = query_vecs_num;
    api.database_vecs_num = database_vecs_num;
    api.k = sort_cnt;
    api.transpose = is_transpose;
    api.input_dtype = input_dtype;
    api.output_dtype = output_dtype;

    if(database_vecs_num < sort_cnt){
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_api_indexflatL2 database_vecs_num(%d) < sort_cnt(%d), %s: %s: %d\n",
                                        database_vecs_num, sort_cnt, filename(__FILE__), __func__, __LINE__);
        return BM_NOT_SUPPORTED;
    }

    bm_status_t ret = BM_SUCCESS;
    unsigned int chipid;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
        return ret;

    switch (chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "faiss_api_indexflatL2", &api, sizeof(api));
            if (BM_SUCCESS != ret) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_api_indexflatL2 launch_sync api error!, %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
            }
            break;
        default:
            ret = BM_NOT_SUPPORTED;
            break;
    }
    return ret;
}