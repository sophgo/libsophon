#include <cmath>
#include <vector>
#include <cstring>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <sys/time.h>
#include "bmcv_api_ext.h"
typedef unsigned int u32;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long long u64;

typedef enum {
    DT_INT8   = (0 << 1) | 1,
    DT_UINT8  = (0 << 1) | 0,
    DT_INT16  = (3 << 1) | 1,
    DT_UINT16 = (3 << 1) | 0,
    DT_FP16   = (1 << 1) | 1,
    DT_BFP16  = (5 << 1) | 1,
    DT_INT32  = (4 << 1) | 1,
    DT_UINT32 = (4 << 1) | 0,
    DT_FP32   = (2 << 1) | 1
} data_type_t;

typedef struct {
    int L_row_num;
    int L_col_num;
    int R_col_num;
    int transpose;
    data_type_t L_dtype;
    data_type_t R_dtype;
    data_type_t Y_dtype;
} matmul_param_t;

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

bool MallocWrap(bm_handle_t& bm_handle, bm_device_mem_t* dev_mem, unsigned long long* host_mem, u32 size) {
    bm_status_t status = bm_malloc_device_byte(bm_handle, dev_mem, size);
    assert(BM_SUCCESS == status);
#ifdef SOC_MODE
    status = bm_mem_mmap_device_mem(bm_handle, dev_mem, host_mem);
    assert(BM_SUCCESS == status);
#else
    *host_mem = (unsigned long long)malloc(size);
#endif
    return BM_SUCCESS == status;
}

void FreeWrap(bm_handle_t& bm_handle, bm_device_mem_t* dev_mem, void* host_mem) {
#ifdef SOC_MODE
    bm_status_t status = bm_mem_unmap_device_mem(bm_handle, host_mem,
                                                bm_mem_get_device_size(*dev_mem));
    assert(BM_SUCCESS == status);
#else
    free(host_mem);
#endif
    bm_free_device(bm_handle, *dev_mem);
}

void matrix_gen_data(float* data, u32 len) {
    for (u32 i = 0; i < len; i++) {
        data[i] = ((float)rand() / (float)RAND_MAX) * 3.3;
    }
}

void fvec_norm_L2sqr_ref(float* vec, float* matrix, int row_num, int col_num) {
    for (int i = 0; i < row_num; i++)
        for (int j = 0; j < col_num; j++)
            vec[i] += matrix[i * col_num + j] * matrix[i * col_num + j];
}

void matrix_trans(void* src, void* dst, int row_num, int col_num, data_type_t dtype) {
    for (int i = 0; i < row_num; i++) {
        for (int j = 0; j < col_num; j++) {
            if (dtype_size(dtype) == 1) {
                ((u8*)dst)[j * row_num + i] = ((u8*)src)[i * col_num + j];
            } else if (dtype_size(dtype) == 2) {
                ((u16*)dst)[j * row_num + i] = ((u16*)src)[i * col_num + j];
            } else if (dtype_size(dtype) == 4) {
                // ((u32*)dst)[j * row_num + i] = ((u32*)src)[i * col_num + j];
                ((float*)dst)[j * row_num + i] = ((float*)src)[i * col_num + j];
            }
        }
    }
}

// calc squared L2 distance matrix
// Y = (-2) * L * R + db_norm_L2sqr_vec + query_norm_L2sqr_vec
template<typename T, typename M, typename K>
void native_calc_L2distance_matrix(const matmul_param_t* param,
                                   T* L_ptr,
                                   M* R_ptr,
                                   K* Y_ptr,
                                   K* vec_query,
                                   K* vec_db) {
    memset(Y_ptr, 0, sizeof(K) * param->L_row_num * param->R_col_num);
    for (int kidx = 0; kidx < param->L_col_num; kidx++) {
        M* cur_R = &R_ptr[kidx * param->R_col_num];
        for (int ridx = 0; ridx < param->L_row_num; ridx++) {
            K L_val = (K)L_ptr[ridx * param->L_col_num + kidx];
            for (int cidx = 0; cidx < param->R_col_num; cidx++) {
                Y_ptr[ridx * param->R_col_num + cidx] += L_val * (K)cur_R[cidx];
            }
        }
    }

    for (int i = 0; i < param->L_row_num; i++) {
        for (int j = 0; j < param->R_col_num; j++) {
            Y_ptr[i * param->R_col_num + j] = (-2) * Y_ptr[i * param->R_col_num + j];
            Y_ptr[i * param->R_col_num + j] += vec_query[i];
            Y_ptr[i * param->R_col_num + j] += vec_db[j];
        }
    }
}

bool GreaterSort(std::pair<int, float>a, std::pair<int, float>b) {
    return (a.second > b.second);
}

bool LessSort(std::pair<int, float>a, std::pair<int, float>b) {
    return (a.second < b.second);
}

void gen_topk_reference(float* input_data,
                        float* top_data_ref,
                        int* top_index_ref,
                        int query_vecs_num,
                        int database_vecs_num,
                        int sort_cnt,
                        int descending) {
    for (int b = 0; b < query_vecs_num; b++) {
        std::vector<std::pair<int, float>> bottom_vec(database_vecs_num);
        for (int i = 0; i < database_vecs_num; i++) {
            int offset = b * database_vecs_num + i;
            float data = ((float*)input_data)[offset];
            bottom_vec[i] = std::make_pair(i, data);
        }
        std::stable_sort(bottom_vec.begin(), bottom_vec.end(), descending ? GreaterSort : LessSort);
        for (int i = 0; i < sort_cnt; i++) {
            top_index_ref[b * sort_cnt + i] = bottom_vec[i].first;
            ((float*)top_data_ref)[b * sort_cnt + i] = bottom_vec[i].second;
        }
    }
}

static int result_compare(float* tpu_result_similarity,
                          int* tpu_result_index,
                          float* ref_similarity,
                          int* ref_index,
                          int sort_cnt,
                          int query_vecs_num) {
    for (int query_cnt = 0; query_cnt < query_vecs_num; query_cnt++) {
        // std::cout << "\n ==> query_cnt = [" << query_cnt << "]" << std::endl;
        for (int sort_indx = 0; sort_indx < sort_cnt; sort_indx++) {
            // std::cout << "\n TPU topk index: [" << tpu_result_index[query_cnt * sort_cnt + sort_indx] << "]" << std::endl;
            // std::cout << " Ref topk index: [" << ref_index[query_cnt * sort_cnt + sort_indx] << "]" << std::endl;
            // std::cout << " TPU topk distance value: [" << tpu_result_similarity[query_cnt * sort_cnt + sort_indx] << "]" << std::endl;
            // std::cout << " Ref topk distance value: [" << ref_similarity[query_cnt * sort_cnt + sort_indx] << "]" << std::endl;
            if (fabs((float)tpu_result_similarity[query_cnt * sort_cnt + sort_indx] - ref_similarity[query_cnt * sort_cnt + sort_indx]) > (1e-2)) {
                std::cout << "tpu_res[" << query_cnt << "]"
                          << "[" << sort_indx << "]"
                          << "[" << tpu_result_index[query_cnt * sort_cnt + sort_indx] << "] "
                          << tpu_result_similarity[query_cnt * sort_cnt + sort_indx]

                          << " ref_result[" << query_cnt << "]"
                          << "[" << sort_indx << "]"
                          << "[" << ref_index[query_cnt * sort_cnt + sort_indx] << "] "
                          << ref_similarity[query_cnt * sort_cnt + sort_indx] << std::endl;
                return -1;
            }
        }
    }
    return 0;
}

bm_status_t test_faiss_indexflatL2(int vec_dims,
                                   int query_vecs_num,
                                   int database_vecs_num,
                                   int sort_cnt,
                                   int is_transpose,
                                   int input_dtype,
                                   int output_dtype) {
    bm_handle_t handle;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_dev_request(&handle, 0);
    if (BM_SUCCESS != ret) {
        std::cout << "request dev failed" << std::endl;
        return BM_ERR_FAILURE;
    }
    std::cout << "vec_dims: " << vec_dims << std::endl;
    std::cout << "query_vecs_num: " << query_vecs_num << std::endl;
    std::cout << "database_vecs_num: " << database_vecs_num << std::endl;
    std::cout << "sort_cnt: " << sort_cnt << std::endl;
    std::cout << "is_transpose: " << is_transpose << std::endl;

    float *blob_L , *blob_R, *vec_query, *vec_db;
    bm_device_mem_t query_data_dev_mem, db_data_dev_mem, query_L2norm_dev_mem, db_L2norm_dev_mem;
    MallocWrap(handle, &query_data_dev_mem, (unsigned long long*)&blob_L, query_vecs_num * vec_dims * dtype_size((data_type_t)input_dtype));
    MallocWrap(handle, &db_data_dev_mem, (unsigned long long*)&blob_R, vec_dims * database_vecs_num * dtype_size((data_type_t)input_dtype));
    MallocWrap(handle, &query_L2norm_dev_mem, (unsigned long long*)&vec_query, 1 * query_vecs_num * dtype_size((data_type_t)input_dtype));
    MallocWrap(handle, &db_L2norm_dev_mem, (unsigned long long*)&vec_db, 1 * database_vecs_num * dtype_size((data_type_t)input_dtype));
    float* blob_R_trans = new float[vec_dims * database_vecs_num];

    matrix_gen_data(blob_L, query_vecs_num * vec_dims);
    matrix_gen_data(blob_R, vec_dims * database_vecs_num);
    matrix_trans(blob_R, blob_R_trans, database_vecs_num, vec_dims, (data_type_t)input_dtype);

    fvec_norm_L2sqr_ref(vec_query, blob_L, query_vecs_num, vec_dims);
    fvec_norm_L2sqr_ref(vec_db, blob_R, database_vecs_num, vec_dims);

    bm_memcpy_s2d(handle, query_data_dev_mem, blob_L);
    bm_memcpy_s2d(handle, db_data_dev_mem, blob_R);
    bm_memcpy_s2d(handle, query_L2norm_dev_mem, vec_query);
    bm_memcpy_s2d(handle, db_L2norm_dev_mem, vec_db);

    float *blob_Y, *output_dis;
    int *output_inx;
    bm_device_mem_t buffer_dev_mem, sorted_similarity_dev_mem, sorted_index_dev_mem;
    MallocWrap(handle, &buffer_dev_mem, (unsigned long long*)&blob_Y, query_vecs_num * database_vecs_num * dtype_size((data_type_t)output_dtype));
    MallocWrap(handle, &sorted_similarity_dev_mem, (unsigned long long*)&output_dis, query_vecs_num * sort_cnt * dtype_size((data_type_t)output_dtype));
    MallocWrap(handle, &sorted_index_dev_mem, (unsigned long long*)&output_inx, query_vecs_num * sort_cnt * dtype_size(DT_INT32));

    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    bmcv_faiss_indexflatL2(handle,
                           query_data_dev_mem,
                           db_data_dev_mem,
                           query_L2norm_dev_mem,
                           db_L2norm_dev_mem,
                           buffer_dev_mem,
                           sorted_similarity_dev_mem,
                           sorted_index_dev_mem,
                           vec_dims,
                           query_vecs_num,
                           database_vecs_num,
                           sort_cnt,
                           is_transpose,
                           input_dtype,
                           output_dtype);
    gettimeofday(&t2, NULL);
    std::cout << "TPU using time(ms): " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) / 1000 << "(ms)" << std::endl;
    std::cout << "TPU using time(us): " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "(us)" << std::endl;
    bm_memcpy_d2s(handle, output_dis, sorted_similarity_dev_mem);
    bm_memcpy_d2s(handle, output_inx, sorted_index_dev_mem);

    matmul_param_t param;
    memset(&param, 0, sizeof(matmul_param_t));

    param.L_row_num = query_vecs_num,
    param.L_col_num = vec_dims;
    param.R_col_num = database_vecs_num;
    param.transpose = is_transpose;
    param.L_dtype = (data_type_t)input_dtype;
    param.R_dtype = (data_type_t)input_dtype;
    param.Y_dtype = (data_type_t)output_dtype;

    int diff = 0;
    float* blob_Y_ref = new float[query_vecs_num * database_vecs_num * dtype_size((data_type_t)output_dtype)];
    float* blob_dis_ref = new float[query_vecs_num * sort_cnt * dtype_size((data_type_t)output_dtype)];
    int* blob_inx_ref = new int[query_vecs_num * sort_cnt * dtype_size(DT_INT32)];
    native_calc_L2distance_matrix<float, float, float>(&param, (float*)blob_L, is_transpose ? (float*)blob_R_trans : (float*)blob_R, (float*)blob_Y_ref, (float*)vec_query, (float*)vec_db);
    gen_topk_reference(blob_Y_ref, blob_dis_ref, blob_inx_ref, query_vecs_num, database_vecs_num, sort_cnt, 0);

    diff = result_compare((float*)output_dis, output_inx, (float*)blob_dis_ref, blob_inx_ref, sort_cnt, query_vecs_num);
    if (diff)
        ret = BM_ERR_DATA;
    delete[] blob_R_trans;
    delete[] blob_Y_ref;
    delete[] blob_dis_ref;
    delete[] blob_inx_ref;
    FreeWrap(handle, &query_data_dev_mem, blob_L);
    FreeWrap(handle, &db_data_dev_mem, blob_R);
    FreeWrap(handle, &query_L2norm_dev_mem, vec_query);
    FreeWrap(handle, &db_L2norm_dev_mem, vec_db);
    FreeWrap(handle, &buffer_dev_mem, blob_Y);
    FreeWrap(handle, &sorted_similarity_dev_mem, output_dis);
    FreeWrap(handle, &sorted_index_dev_mem, output_inx);
    if (ret != 0) {
          std::cout << "\n-----Test faiss_indexflatL2 failed-----" << std::endl;
          exit(-1);
    } else {
          std::cout << "\n----------Test faiss_indexflatL2 succeed-----------" << std::endl;
    }

    bm_dev_free(handle);
    return ret;
}

int main(int argc, char *argv[]) {
    unsigned int seed = (unsigned)time(NULL);
    srand(seed);
    std::cout << "random seed = " << seed << std::endl;

    // int sort_cnt = 100;
    // int vec_dims = 256;
    // int query_vecs_num = 1;
    // int database_vecs_num = 2000000;
    // int is_transpose = 1;
    int sort_cnt = rand() % 100 + 1;
    int vec_dims = rand() % 256 + 1;
    int query_vecs_num = rand() % 64 + 1;
    int database_vecs_num = rand() % 10000 + 1 + sort_cnt;
    int is_transpose = rand() % 2;
    int input_dtype = DT_FP32;
    int output_dtype = DT_FP32;
    if (argc > 1) vec_dims = atoi(argv[1]);
    if (argc > 2) query_vecs_num = atoi(argv[2]);
    if (argc > 3) database_vecs_num = atoi(argv[3]);
    if (argc > 4) sort_cnt = atoi(argv[4]);
    if (argc > 5) input_dtype = atoi(argv[5]);
    if (argc > 6) output_dtype = atoi(argv[6]);
    if (argc > 7) is_transpose = atoi(argv[7]);

    if (BM_SUCCESS != test_faiss_indexflatL2(vec_dims, query_vecs_num, database_vecs_num, sort_cnt, is_transpose, input_dtype, output_dtype)) {
        return -1;
    }
    return 0;
}
