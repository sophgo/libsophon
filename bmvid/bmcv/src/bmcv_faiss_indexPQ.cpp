#include "bmcv_api_ext.h"
#include "bmcv_internal.h"

#pragma pack(push, 1)
typedef struct
{
    unsigned long long centroids_global_addr;
    unsigned long long nxquery_global_addr;
    unsigned long long nycodes_global_addr;
    unsigned long long output_distance_global_addr;
    unsigned long long output_index_global_addr;
    int d;
    int m;
    int ksub;
    int ny;
    int nx;
    int k;
    int IP_metric;
} faiss_api_indexPQ_ADC_t;
typedef struct
{
    unsigned long long sdc_table_global_addr;
    unsigned long long nxcodes_global_addr;
    unsigned long long nycodes_global_addr;
    unsigned long long output_distance_global_addr;
    unsigned long long output_index_global_addr;
    int m;
    int ksub;
    int ny;
    int nx;
    int k;
    int IP_metric;
} faiss_api_indexPQ_SDC_t;
typedef struct
{
    unsigned long long vector_input_global_addr;
    unsigned long long centroids_global_addr;
    unsigned long long buffer_table_global_addr;
    unsigned long long codes_output_global_addr;
    int nv;
    int d;
    int m;
    int ksub;
    int IP_metric;
} faiss_api_indexPQ_encode_t;
#pragma pack(pop)

bm_status_t bmcv_faiss_indexPQ_ADC(bm_handle_t handle,
                                   bm_device_mem_t centroids_input_dev,
                                   bm_device_mem_t nxquery_input_dev,
                                   bm_device_mem_t nycodes_input_dev,
                                   bm_device_mem_t distance_output_dev,
                                   bm_device_mem_t index_output_dev,
                                   int vec_dims,
                                   int slice_num,
                                   int centroids_num,
                                   int database_num,
                                   int query_num,
                                   int sort_cnt,
                                   int IP_metric)
{
    faiss_api_indexPQ_ADC_t api;
    api.centroids_global_addr = bm_mem_get_device_addr(centroids_input_dev);
    api.nxquery_global_addr = bm_mem_get_device_addr(nxquery_input_dev);
    api.nycodes_global_addr = bm_mem_get_device_addr(nycodes_input_dev);
    api.output_distance_global_addr = bm_mem_get_device_addr(distance_output_dev);
    api.output_index_global_addr = bm_mem_get_device_addr(index_output_dev);
    api.d = vec_dims;
    api.m = slice_num;
    api.ksub = centroids_num;
    api.ny = database_num;
    api.nx = query_num;
    api.k = sort_cnt;
    api.IP_metric = IP_metric;

    bm_status_t ret = BM_SUCCESS;
    unsigned int chipid;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
        return ret;

    switch (chipid)
    {
    case BM1684X:
        ret = bm_tpu_kernel_launch(handle, "faiss_api_indexPQ_ADCsearch", &api, sizeof(api));
        if (BM_SUCCESS != ret)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_indexPQ_ADC launch_sync api error!, %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
        }
        break;
    default:
        ret = BM_NOT_SUPPORTED;
        break;
    }
    return ret;
}

bm_status_t bmcv_faiss_indexPQ_SDC(bm_handle_t handle,
                                   bm_device_mem_t sdc_table_input_dev,
                                   bm_device_mem_t nxcodes_input_dev,
                                   bm_device_mem_t nycodes_input_dev,
                                   bm_device_mem_t distance_output_dev,
                                   bm_device_mem_t index_output_dev,
                                   int slice_num,
                                   int centroids_num,
                                   int database_num,
                                   int query_num,
                                   int sort_cnt,
                                   int IP_metric)
{
    faiss_api_indexPQ_SDC_t api;
    api.sdc_table_global_addr = bm_mem_get_device_addr(sdc_table_input_dev);
    api.nxcodes_global_addr = bm_mem_get_device_addr(nxcodes_input_dev);
    api.nycodes_global_addr = bm_mem_get_device_addr(nycodes_input_dev);
    api.output_distance_global_addr = bm_mem_get_device_addr(distance_output_dev);
    api.output_index_global_addr = bm_mem_get_device_addr(index_output_dev);
    api.m = slice_num;
    api.ksub = centroids_num;
    api.ny = database_num;
    api.nx = query_num;
    api.k = sort_cnt;
    api.IP_metric = IP_metric;

    bm_status_t ret = BM_SUCCESS;
    unsigned int chipid;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
        return ret;

    switch (chipid)
    {
    case BM1684X:
        ret = bm_tpu_kernel_launch(handle, "faiss_api_indexPQ_SDCsearch", &api, sizeof(api));
        if (BM_SUCCESS != ret)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_indexPQ_SDC launch_sync api error!, %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
        }
        break;
    default:
        ret = BM_NOT_SUPPORTED;
        break;
    }
    return ret;
}


bm_status_t bmcv_faiss_indexPQ_encode(bm_handle_t handle,
                                      bm_device_mem_t vector_input_dev,
                                      bm_device_mem_t centroids_input_dev,
                                      bm_device_mem_t buffer_table_dev,
                                      bm_device_mem_t nvcodes_output_dev,
                                      int encode_vec_num,
                                      int vec_dims,
                                      int slice_num,
                                      int centroids_num,
                                      int IP_metric)
{
    faiss_api_indexPQ_encode_t api;
    api.vector_input_global_addr = bm_mem_get_device_addr(vector_input_dev);
    api.centroids_global_addr = bm_mem_get_device_addr(centroids_input_dev);
    api.buffer_table_global_addr = bm_mem_get_device_addr(buffer_table_dev);
    api.codes_output_global_addr = bm_mem_get_device_addr(nvcodes_output_dev);
    api.nv = encode_vec_num;
    api.d = vec_dims;
    api.m = slice_num;
    api.ksub = centroids_num;
    api.IP_metric = IP_metric;

    bm_status_t ret = BM_SUCCESS;
    unsigned int chipid;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
        return ret;

    switch (chipid)
    {
    case BM1684X:
        ret = bm_tpu_kernel_launch(handle, "faiss_api_indexPQ_encode", &api, sizeof(api));
        if (BM_SUCCESS != ret)
        {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_indexPQ_encode launch_sync api error!, %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
        }
        break;
    default:
        ret = BM_NOT_SUPPORTED;
        break;
    }
    return ret;
}