#include "bmcv_api_ext.h"
#include "bmcv_internal.h"

#pragma pack(push, 1)
typedef struct
{
    unsigned long long centroids_global_addr;
    unsigned long long nxquery_global_addr;
    unsigned long long nycodes_global_addr;
    unsigned long long output_distance_global_addr;
    unsigned long long output_distance_global_tmp_addr;
    unsigned long long output_index_global_addr;
    int d;
    int m;
    int ksub;
    int ny;
    int nx;
    int k;
    int IP_metric;
    int input_dtype;
    int output_dtype;
} faiss_api_indexPQ_ADC_t;
typedef struct
{
    unsigned long long sdc_table_global_addr;
    unsigned long long nxcodes_global_addr;
    unsigned long long nycodes_global_addr;
    unsigned long long output_distance_global_addr;
    unsigned long long output_distance_global_tmp_addr;
    unsigned long long output_index_global_addr;
    int m;
    int ksub;
    int ny;
    int nx;
    int k;
    int IP_metric;
    int input_dtype;
    int output_dtype;
} faiss_api_indexPQ_SDC_t;

static inline int dtype_size(data_type_t data_type) {
    int size = 0;
    switch (data_type) {
        case DT_FP32:   size = 4; break;
        case DT_UINT32: size = 4; break;
        case DT_INT32:  size = 4; break;
        case DT_FP16:   size = 2; break;
        case DT_BFP16:  size = 2; break;
        case DT_INT16:  size = 2; break;
        case DT_UINT16: size = 2; break;
        case DT_INT8:   size = 1; break;
        case DT_UINT8:  size = 1; break;
        default: break;
    }
    return size;
}
#pragma pack(pop)

typedef struct {
    unsigned long long vector_input_global_addr;
    unsigned long long centroids_global_addr;
    unsigned long long buffer_table_global_addr;
    unsigned long long buffer_table_global_tmp_addr;
    unsigned long long topk_data_global_addr;
    unsigned long long codes_output_global_addr;
    unsigned long long output_int32_global_addr;
    int                encode_num;
    int                d;
    int                m;
    int                ksub;
    int                IP_metric;
    int                input_dtype;
    int                output_dtype;
} __attribute__((packed)) faiss_api_indexPQ_encode_t;

bm_status_t bmcv_faiss_indexPQ_ADC_ext(bm_handle_t handle,
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
                                   int IP_metric,
                                   int in_dtype,
                                   int out_dtype)
{
    faiss_api_indexPQ_ADC_t api;
    bm_device_mem_t distance_output_dev_fp32;

    if (bm_mem_get_type(distance_output_dev) == BM_MEM_TYPE_DEVICE) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle, &distance_output_dev_fp32, query_num * database_num * sizeof(float))) {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_indexPQ_ADC bm_malloc_device_byte error!, %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
            return BM_ERR_FAILURE;
        }
    }

    api.centroids_global_addr = bm_mem_get_device_addr(centroids_input_dev);
    api.nxquery_global_addr = bm_mem_get_device_addr(nxquery_input_dev);
    api.nycodes_global_addr = bm_mem_get_device_addr(nycodes_input_dev);
    api.output_distance_global_addr = bm_mem_get_device_addr(distance_output_dev);
    api.output_distance_global_tmp_addr = bm_mem_get_device_addr(distance_output_dev_fp32);
    api.output_index_global_addr = bm_mem_get_device_addr(index_output_dev);
    api.d = vec_dims;
    api.m = slice_num;
    api.ksub = centroids_num;
    api.ny = database_num;
    api.nx = query_num;
    api.k = sort_cnt;
    api.IP_metric = IP_metric;
    api.input_dtype = in_dtype;
    api.output_dtype = out_dtype;
    bm_status_t ret = BM_SUCCESS;
    unsigned int chipid;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret) {
        if (bm_mem_get_type(distance_output_dev) == BM_MEM_TYPE_DEVICE) {
            bm_free_device(handle, distance_output_dev_fp32);
        }
        return ret;
    }

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
    if (bm_mem_get_type(distance_output_dev) == BM_MEM_TYPE_DEVICE) {
        bm_free_device(handle, distance_output_dev_fp32);
    }
    return ret;
}

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
    return bmcv_faiss_indexPQ_ADC_ext(handle,
                                       centroids_input_dev,
                                       nxquery_input_dev,
                                       nycodes_input_dev,
                                       distance_output_dev,
                                       index_output_dev,
                                       vec_dims,
                                       slice_num,
                                       centroids_num,
                                       database_num,
                                       query_num,
                                       sort_cnt,
                                       IP_metric,
                                       DT_FP32,
                                       DT_FP32);
}

bm_status_t bmcv_faiss_indexPQ_SDC_ext(bm_handle_t handle,
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
                                   int IP_metric,
                                   int in_dtype,
                                   int out_dtype)
{
    faiss_api_indexPQ_SDC_t api;
    bm_device_mem_t distance_output_dev_fp32;

    if (bm_mem_get_type(distance_output_dev) == BM_MEM_TYPE_DEVICE) {
        if (BM_SUCCESS !=
            bm_malloc_device_byte(handle, &distance_output_dev_fp32, query_num * database_num * sizeof(float))) {
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_indexPQ_SDC bm_malloc_device_byte error!, %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
            return BM_ERR_FAILURE;
        }
    }

    api.sdc_table_global_addr = bm_mem_get_device_addr(sdc_table_input_dev);
    api.nxcodes_global_addr = bm_mem_get_device_addr(nxcodes_input_dev);
    api.nycodes_global_addr = bm_mem_get_device_addr(nycodes_input_dev);
    api.output_distance_global_addr = bm_mem_get_device_addr(distance_output_dev);
    api.output_distance_global_tmp_addr = bm_mem_get_device_addr(distance_output_dev_fp32);
    api.output_index_global_addr = bm_mem_get_device_addr(index_output_dev);
    api.m = slice_num;
    api.ksub = centroids_num;
    api.ny = database_num;
    api.nx = query_num;
    api.k = sort_cnt;
    api.IP_metric = IP_metric;
    api.input_dtype = in_dtype;
    api.output_dtype = out_dtype;
    bm_status_t ret = BM_SUCCESS;
    unsigned int chipid;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret) {
        if (bm_mem_get_type(distance_output_dev) == BM_MEM_TYPE_DEVICE) {
            bm_free_device(handle, distance_output_dev_fp32);
        }
        return ret;
    }

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
    if (bm_mem_get_type(distance_output_dev) == BM_MEM_TYPE_DEVICE) {
        bm_free_device(handle, distance_output_dev_fp32);
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
    return bmcv_faiss_indexPQ_SDC_ext(handle,
                                   sdc_table_input_dev,
                                   nxcodes_input_dev,
                                   nycodes_input_dev,
                                   distance_output_dev,
                                   index_output_dev,
                                   slice_num,
                                   centroids_num,
                                   database_num,
                                   query_num,
                                   sort_cnt,
                                   IP_metric,
                                   DT_FP32,
                                   DT_FP32);
}

bm_status_t bmcv_faiss_indexPQ_encode_ext(bm_handle_t handle,
                                          bm_device_mem_t vector_input_dev,
                                          bm_device_mem_t centroids_input_dev,
                                          bm_device_mem_t buffer_table_dev,
                                          bm_device_mem_t nvcodes_output_dev,
                                          int encode_vec_num,
                                          int vec_dims,
                                          int slice_num,
                                          int centroids_num,
                                          int IP_metric,
                                          int input_dtype,
                                          int output_dtype) {
    faiss_api_indexPQ_encode_t api;
    bm_device_mem_t buffer_table_dev_32byte, topk_data_global_addr, output_int32_global_addr;

    if (BM_SUCCESS != bm_malloc_device_byte(handle, &buffer_table_dev_32byte, encode_vec_num * slice_num * centroids_num * dtype_size((data_type_t)input_dtype))) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_indexPQ_SDC bm_malloc_device_byte buffer_table_dev_32byte error!, %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bm_malloc_device_byte(handle, &topk_data_global_addr, encode_vec_num * slice_num * dtype_size((data_type_t)input_dtype))) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_indexPQ_SDC bm_malloc_device_byte topk_data_global_addr error!, %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
        bm_free_device(handle, buffer_table_dev_32byte);
        return BM_ERR_FAILURE;
    }
    if (BM_SUCCESS != bm_malloc_device_byte(handle, &output_int32_global_addr, encode_vec_num * slice_num * dtype_size((data_type_t)input_dtype))) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_indexPQ_SDC bm_malloc_device_byte output_int32_global_addr error!, %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
        bm_free_device(handle, buffer_table_dev_32byte);
        bm_free_device(handle, topk_data_global_addr);
        return BM_ERR_FAILURE;
    }
    api.vector_input_global_addr = bm_mem_get_device_addr(vector_input_dev);
    api.centroids_global_addr = bm_mem_get_device_addr(centroids_input_dev);
    api.topk_data_global_addr = bm_mem_get_device_addr(topk_data_global_addr);
    api.buffer_table_global_addr = bm_mem_get_device_addr(buffer_table_dev);
    api.buffer_table_global_tmp_addr = bm_mem_get_device_addr(buffer_table_dev_32byte);
    api.output_int32_global_addr = bm_mem_get_device_addr(output_int32_global_addr);
    api.codes_output_global_addr = bm_mem_get_device_addr(nvcodes_output_dev);
    api.encode_num = encode_vec_num;
    api.d = vec_dims;
    api.m = slice_num;
    api.ksub = centroids_num;
    api.IP_metric = IP_metric;
    api.input_dtype = input_dtype;
    api.output_dtype = output_dtype;

    bm_status_t ret = BM_SUCCESS;
    unsigned int chipid;
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret) {
        bm_free_device(handle, buffer_table_dev_32byte);
        bm_free_device(handle, topk_data_global_addr);
        bm_free_device(handle, output_int32_global_addr);
        return ret;
    }

    switch (chipid) {
        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "faiss_api_indexPQ_encode", &api, sizeof(api));
            if (BM_SUCCESS != ret) {
                bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "faiss_api_indexPQ_encode launch_sync api error!, %s: %s: %d\n", filename(__FILE__), __func__, __LINE__);
            }
            break;
        default:
            ret = BM_NOT_SUPPORTED;
            break;
    }

    bm_free_device(handle, buffer_table_dev_32byte);
    bm_free_device(handle, topk_data_global_addr);
    bm_free_device(handle, output_int32_global_addr);
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
                                      int IP_metric) {
    return bmcv_faiss_indexPQ_encode_ext(handle,
                                         vector_input_dev,
                                         centroids_input_dev,
                                         buffer_table_dev,
                                         nvcodes_output_dev,
                                         encode_vec_num,
                                         vec_dims,
                                         slice_num,
                                         centroids_num,
                                         IP_metric,
                                         DT_FP32,
                                         DT_FP32);
}