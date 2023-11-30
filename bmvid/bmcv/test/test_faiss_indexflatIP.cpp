#include <cmath>
#include <vector>
#include <memory>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <sys/time.h>
#include "bmcv_api_ext.h"
#include "test_misc.h"
using namespace std;

template <typename T>
class Blob {
   public:
    T * data = NULL;
    int elem_size = 0;
    int byte_size = 0;

    explicit Blob(int size) {
        if (size == 0) {
            data = nullptr;
            byte_size = 0;
            return;
        }
        data = new T[size];
        elem_size = size;
        byte_size = size * sizeof(T);
    }
    ~Blob() {
        if (data) {
            delete[] data;
            elem_size = 0;
            byte_size = 0;
        }
    }
};


template <typename T>
static int matrix_transpose(vector<vector<T>> &matrix) {
    if (matrix.size() == 0)
        return BM_ERR_FAILURE;
    vector<vector<T>> trans_vec(matrix[0].size(), vector<T>());
    for (size_t i = 0; i < matrix.size(); i++) {
        for (size_t j = 0; j < matrix[i].size(); j++) {
            if (trans_vec[j].size() != matrix.size())
                trans_vec[j].resize(matrix.size());
            trans_vec[j][i] = matrix[i][j];
        }
    }
    matrix = trans_vec;
    return BM_SUCCESS;
}

// query matrix * database matrix (FP16 + FP16 ==> FP16)
vector<vector<float>> matrix_mul_ref(vector<vector<fp16>> input_data,
                                     vector<vector<fp16>> db_data) {
    float a = 0;
    int query_vecs_num = input_data.size();
    int database_vecs_num = db_data[0].size();
    int vec_dims = input_data[0].size();

    vector<vector<float>> matrix_mul(query_vecs_num);
    for (int query_cnt = 0; query_cnt < query_vecs_num; query_cnt++) {
        for (int db_cnt = 0; db_cnt < database_vecs_num; db_cnt++) {
            a = 0.0f;
            for (int dims_cnt = 0; dims_cnt < vec_dims; dims_cnt++) {
                a += fp16tofp32(input_data[query_cnt][dims_cnt]) * fp16tofp32(db_data[dims_cnt][db_cnt]);
            }
            matrix_mul[query_cnt].push_back(a);
        }
    }
    return matrix_mul;
}

// query matrix * database matrix (FP32 + FP32 ==> FP32)
vector<vector<float>> matrix_mul_ref(vector<vector<float>> input_data,
                                     vector<vector<float>> db_data) {
    float a = 0;
    int query_vecs_num = input_data.size();
    int database_vecs_num = db_data[0].size();
    int vec_dims = input_data[0].size();

    vector<vector<float>> matrix_mul(query_vecs_num);
    for (int query_cnt = 0; query_cnt < query_vecs_num; query_cnt++) {
        for (int db_cnt = 0; db_cnt < database_vecs_num; db_cnt++) {
            a = 0.0f;
            for (int dims_cnt = 0; dims_cnt < vec_dims; dims_cnt++) {
                a += input_data[query_cnt][dims_cnt] * db_data[dims_cnt][db_cnt];
            }
            matrix_mul[query_cnt].push_back(a);
        }
    }
    return matrix_mul;
}

// INT8 + INT8 ==> INT32
vector<vector<int>> matrix_mul_ref(vector<vector<signed char>> input_data,
                                   vector<vector<signed char>> db_data) {
    int a = 0;
    int query_vecs_num = input_data.size();
    int database_vecs_num = db_data[0].size();
    int vec_dims = input_data[0].size();

    vector<vector<int>> matrix_mul(query_vecs_num);
    for (int query_cnt = 0; query_cnt < query_vecs_num; query_cnt++) {
        for (int db_cnt = 0; db_cnt < database_vecs_num; db_cnt++) {
            a = 0;
            for (int dims_cnt = 0; dims_cnt < vec_dims; dims_cnt++) {
                a += input_data[query_cnt][dims_cnt] * db_data[dims_cnt][db_cnt];
            }
            a = a > 0x7fff ? 0x7fff : a;
            a = a < -0x8000 ? -0x8000 : a;
            matrix_mul[query_cnt].push_back((int)a);
        }
    }
    return matrix_mul;
}

template <typename T>
bool result_compare(T * tpu_result_similarity,
                    int * tpu_result_index,
                    vector<vector<T>> ref_result,
                    int sort_cnt) {
    vector<vector<T>> tmp_ref_result = ref_result;

    for (size_t query_cnt = 0; query_cnt < ref_result.size(); query_cnt++) {
        // std::cout << "\n ==> query_cnt = [" << query_cnt << "]" << std::endl;
        for (int sort_indx = 0; sort_indx < sort_cnt; sort_indx++) {
            int ref_index = std::distance(tmp_ref_result[query_cnt].begin(),
                                          max_element(tmp_ref_result[query_cnt].begin(),
                                                      tmp_ref_result[query_cnt].end()));
            T ref_similarity = tmp_ref_result[query_cnt][ref_index];
            // find the ref index in each of origin vector
            auto tmp_elem = std::find(ref_result[query_cnt].begin(),
                                      ref_result[query_cnt].end(),
                                      ref_similarity);
            int ref_index_origin = std::distance(ref_result[query_cnt].begin(), tmp_elem);
            // std::cout << "\n TPU topk index: [" << tpu_result_index[query_cnt * sort_cnt + sort_indx] << "]" << std::endl;
            // std::cout << " Ref topk index: [" << ref_index_origin << "]" << std::endl;
            // std::cout << " TPU topk distance value: [" << tpu_result_similarity[query_cnt * sort_cnt + sort_indx] << "]" << std::endl;
            // std::cout << " Ref topk distance value: [" << ref_similarity << "]" << std::endl;
            if (fabs((float)tpu_result_similarity[query_cnt * sort_cnt + sort_indx] - (float)ref_similarity) > (1e-2)) {
                cout << "tpu_res[" << query_cnt << "]"
                     << "[" << sort_indx << "]"
                     << "[" << tpu_result_index[query_cnt * sort_cnt + sort_indx] << "] "
                     << tpu_result_similarity[query_cnt * sort_cnt + sort_indx]

                     << " ref_result[" << query_cnt << "]"
                     << "[" << sort_indx << "]"
                     << "[" << ref_index_origin << "] "
                     << tmp_ref_result[query_cnt][ref_index] << endl;
                return false;
            }
            tmp_ref_result[query_cnt].erase(tmp_ref_result[query_cnt].begin() + ref_index);
        }
    }
    return true;
}

bool result_compare_fp16(fp16 * tpu_result_similarity,
                    int * tpu_result_index,
                    vector<vector<float>> ref_result,
                    int sort_cnt) {
    vector<vector<float>> tmp_ref_result = ref_result;

    for (size_t query_cnt = 0; query_cnt < ref_result.size(); query_cnt++) {
        // std::cout << "\n ==> query_cnt = [" << query_cnt << "]" << std::endl;
        for (int sort_indx = 0; sort_indx < sort_cnt; sort_indx++) {
            int ref_index = std::distance(tmp_ref_result[query_cnt].begin(),
                                          max_element(tmp_ref_result[query_cnt].begin(),
                                                      tmp_ref_result[query_cnt].end()));
            float ref_similarity = tmp_ref_result[query_cnt][ref_index];
            // find the ref index in each of origin vector
            auto tmp_elem = std::find(ref_result[query_cnt].begin(),
                                      ref_result[query_cnt].end(),
                                      ref_similarity);
            int ref_index_origin = std::distance(ref_result[query_cnt].begin(), tmp_elem);
            if (fabs((float)fp16tofp32(tpu_result_similarity[query_cnt * sort_cnt + sort_indx]) - (float)fp16tofp32((fp32tofp16(ref_similarity, 1)))) > (3e-1)) {
                cout << "tpu_res[" << query_cnt << "]"
                     << "[" << sort_indx << "]"
                     << "[" << tpu_result_index[query_cnt * sort_cnt + sort_indx] << "] "
                     << fp16tofp32(tpu_result_similarity[query_cnt * sort_cnt + sort_indx])

                     << " ref_result[" << query_cnt << "]"
                     << "[" << sort_indx << "]"
                     << "[" << ref_index_origin << "] " << fp16tofp32((fp32tofp16(ref_similarity, 1))) << " "
                     << ref_similarity << endl;
                return false;
            }
            tmp_ref_result[query_cnt].erase(tmp_ref_result[query_cnt].begin() + ref_index);
        }
    }
    return true;
}

bool result_compare_fp32(float * tpu_result_similarity,
                    int * tpu_result_index,
                    vector<vector<float>> ref_result,
                    int sort_cnt) {
    vector<vector<float>> tmp_ref_result = ref_result;

    for (size_t query_cnt = 0; query_cnt < ref_result.size(); query_cnt++) {
        // std::cout << "\n ==> query_cnt = [" << query_cnt << "]" << std::endl;
        for (int sort_indx = 0; sort_indx < sort_cnt; sort_indx++) {
            int ref_index = std::distance(tmp_ref_result[query_cnt].begin(),
                                          max_element(tmp_ref_result[query_cnt].begin(),
                                                      tmp_ref_result[query_cnt].end()));
            float ref_similarity = tmp_ref_result[query_cnt][ref_index];
            // find the ref index in each of origin vector
            auto tmp_elem = std::find(ref_result[query_cnt].begin(),
                                      ref_result[query_cnt].end(),
                                      ref_similarity);
            int ref_index_origin = std::distance(ref_result[query_cnt].begin(), tmp_elem);
            // std::cout << "\n TPU topk index: [" << tpu_result_index[query_cnt * sort_cnt + sort_indx] << "]" << std::endl;
            // std::cout << " Ref topk index: [" << ref_index_origin << "]" << std::endl;
            // std::cout << " TPU topk distance value: [" << tpu_result_similarity[query_cnt * sort_cnt + sort_indx] << "]" << std::endl;
            // std::cout << " Ref topk distance value: [" << ref_similarity << "]" << std::endl;
            if (fabs((float)tpu_result_similarity[query_cnt * sort_cnt + sort_indx] - (float)ref_similarity ) > (1e-1)) {
                cout << "tpu_res[" << query_cnt << "]"
                     << "[" << sort_indx << "]"
                     << "[" << tpu_result_index[query_cnt * sort_cnt + sort_indx] << "] "
                     << tpu_result_similarity[query_cnt * sort_cnt + sort_indx]

                     << " ref_result[" << query_cnt << "]"
                     << "[" << sort_indx << "]"
                     << "[" << ref_index_origin << "] "
                     << ref_similarity << endl;
                return false;
            }
            tmp_ref_result[query_cnt].erase(tmp_ref_result[query_cnt].begin() + ref_index);
        }
    }
    return true;
}

int32_t faiss_indexflatIP_fp16_single_test(int vec_dims,
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

    std::shared_ptr<Blob<fp16>> input_data = std::make_shared<Blob<fp16>>(query_vecs_num * vec_dims);
    std::shared_ptr<Blob<fp16>> db_data = std::make_shared<Blob<fp16>>(database_vecs_num * vec_dims);
    std::shared_ptr<Blob<float>> output_similarity_fp32 = std::make_shared<Blob<float>>(query_vecs_num * sort_cnt);
    std::shared_ptr<Blob<fp16>> output_similarity_fp16 = std::make_shared<Blob<fp16>>(query_vecs_num * sort_cnt);

    /* assign random data */
    vector<vector<fp16>> db_content_vec(is_transpose ? database_vecs_num : vec_dims);
    if (is_transpose) {
        for (int i = 0; i < database_vecs_num; i++) {
            for (int j = 0; j < vec_dims; j++) {
                #ifdef __linux__
				//fp16 temp_val = fp32tofp16(random() % 120, 1);
                fp16 temp_val = fp32tofp16(j * 0.1 + i * 0.53, 1);
                #else
				fp16 temp_val = fp32tofp16(rand() % 120, 0);
                //fp16 temp_val = fp32tofp16(rand() % 120, 1);
                #endif
                temp_val = fp32tofp16(fp16tofp32(temp_val) / 100, 0);
                db_content_vec[i].push_back(temp_val);
            }
        }
    } else {
        for (int i = 0; i < vec_dims; i++) {
            for (int j = 0; j < database_vecs_num; j++) {
                #ifdef __linux__
				//fp16 temp_val = fp32tofp16(random() % 120, 1);
                fp16 temp_val = fp32tofp16(i * 0.1 + j * 0.53, 1);
                #else
				fp16 temp_val = fp32tofp16(rand() % 120, 0);
                //fp16 temp_val = fp32tofp16(i * 0.1 + j * 0.53, 1);
                #endif
                temp_val = fp32tofp16(fp16tofp32(temp_val) / 100, 0);
                db_content_vec[i].push_back(temp_val);
            }
        }
    }
    vector<vector<fp16>> input_content_vec(query_vecs_num);
    for (int i = 0; i < query_vecs_num; i++) {
        for (int j = 0; j < vec_dims; j++) {
            #ifdef __linux__
            //fp16 temp_val = fp32tofp16(random() % 120, 1);
            fp16 temp_val = fp32tofp16(i * 0.1, 1);
            #else
            fp16 temp_val = fp32tofp16(rand() % 120, 0);
            #endif
            temp_val = fp32tofp16(fp16tofp32(temp_val) / 100, 0);
            input_content_vec[i].push_back(temp_val);
        }
    }
    vector<fp16> db_content_vec_con, input_content_vec_con;
    for (unsigned int i = 0; i < db_content_vec.size(); i++) {
        db_content_vec_con.insert(db_content_vec_con.end(),
                                  db_content_vec[i].begin(),
                                  db_content_vec[i].end());
    }
    for (unsigned int i = 0; i < input_content_vec.size(); i++) {
        input_content_vec_con.insert(input_content_vec_con.end(),
                                     input_content_vec[i].begin(),
                                     input_content_vec[i].end());
    }
    memcpy(input_data.get()->data, input_content_vec_con.data(), query_vecs_num * vec_dims * sizeof(fp16));
    memcpy(db_data.get()->data, db_content_vec_con.data(), database_vecs_num * vec_dims * sizeof(fp16));
    std::shared_ptr<Blob<int>> output_index = std::make_shared<Blob<int>>(query_vecs_num * sort_cnt);

    bm_device_mem_t input_data_global_addr_device,
                    db_data_global_addr_device,
                    buffer_global_addr_device,
                    output_sorted_similarity_global_addr_device,
                    output_sorted_index_global_addr_device;
    bm_malloc_device_byte(handle,
                          &input_data_global_addr_device,
                          dtype_size((data_type_t)input_dtype) * query_vecs_num * vec_dims);
    bm_malloc_device_byte(handle,
                          &db_data_global_addr_device,
                          dtype_size((data_type_t)input_dtype) * database_vecs_num * vec_dims);
    bm_malloc_device_byte(handle,
                          &buffer_global_addr_device,
                          dtype_size((data_type_t)DT_FP32) * query_vecs_num * database_vecs_num);
    bm_malloc_device_byte(handle,
                          &output_sorted_similarity_global_addr_device,
                          dtype_size((data_type_t)output_dtype) * query_vecs_num * sort_cnt);
    bm_malloc_device_byte(handle,
                          &output_sorted_index_global_addr_device,
                          dtype_size(DT_INT32) * query_vecs_num * sort_cnt);
    bm_memcpy_s2d(handle,
                  input_data_global_addr_device,
                  bm_mem_get_system_addr(bm_mem_from_system(input_data.get()->data)));
    bm_memcpy_s2d(handle,
                  db_data_global_addr_device,
                  bm_mem_get_system_addr(bm_mem_from_system(db_data.get()->data)));
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    bmcv_faiss_indexflatIP(handle,
                           input_data_global_addr_device,
                           db_data_global_addr_device,
                           buffer_global_addr_device,
                           output_sorted_similarity_global_addr_device,
                           output_sorted_index_global_addr_device,
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

    if (output_dtype == DT_FP32)
        bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_similarity_fp32.get()->data)),
                  output_sorted_similarity_global_addr_device);
    else
        bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_similarity_fp16.get()->data)),
                  output_sorted_similarity_global_addr_device);
    bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_index.get()->data)),
                  output_sorted_index_global_addr_device);
    if (is_transpose)
        matrix_transpose(db_content_vec);
    vector<vector<float>> ref_result(query_vecs_num);
    ref_result = matrix_mul_ref(input_content_vec, db_content_vec);
    if (output_dtype == DT_FP32) {
        if (false == result_compare_fp32(output_similarity_fp32.get()->data,
                                        output_index.get()->data,
                                        ref_result,
                                        sort_cnt)) {
            std::cout << "-------------faiss_indexflatIP fp16/fp32 compare failed-------" << std::endl;
            exit(-1);
        }
    } else {
        if (false == result_compare_fp16(output_similarity_fp16.get()->data,
                                        output_index.get()->data,
                                        ref_result,
                                        sort_cnt)) {
            std::cout << "-------------faiss_indexflatIP fp16/fp16 compare failed-------" << std::endl;
            exit(-1);
        }
    }

    std::cout << "-------------faiss_indexflatIP fp16 compare succeed-----------" << std::endl;
    bm_free_device(handle, input_data_global_addr_device);
    bm_free_device(handle, db_data_global_addr_device);
    bm_free_device(handle, buffer_global_addr_device);
    bm_free_device(handle, output_sorted_similarity_global_addr_device);
    bm_free_device(handle, output_sorted_index_global_addr_device);
    bm_dev_free(handle);
    return 0;
}

int32_t faiss_indexflatIP_fp32_single_test(int vec_dims,
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

    std::shared_ptr<Blob<float>> input_data = std::make_shared<Blob<float>>(query_vecs_num * vec_dims);
    std::shared_ptr<Blob<float>> db_data = std::make_shared<Blob<float>>(database_vecs_num * vec_dims);
    std::shared_ptr<Blob<float>> output_similarity_fp32 = std::make_shared<Blob<float>>(query_vecs_num * sort_cnt);
    std::shared_ptr<Blob<fp16>> output_similarity_fp16 = std::make_shared<Blob<fp16>>(query_vecs_num * sort_cnt);

    /* assign random data */
    vector<vector<float>> db_content_vec(is_transpose ? database_vecs_num : vec_dims);
    if (is_transpose) {
        for (int i = 0; i < database_vecs_num; i++) {
            for (int j = 0; j < vec_dims; j++) {
                #ifdef __linux__
                float temp_val = random() % 120;
                #else
                float temp_val = rand() % 120;
                #endif
                temp_val = temp_val / 100;
                db_content_vec[i].push_back(temp_val);
            }
        }
    } else {
        for (int i = 0; i < vec_dims; i++) {
            for (int j = 0; j < database_vecs_num; j++) {
                #ifdef __linux__
                float temp_val = random() % 120;
                #else
                float temp_val = rand() % 120;
                #endif
                temp_val = temp_val / 100;
                db_content_vec[i].push_back(temp_val);
            }
        }
    }
    vector<vector<float>> input_content_vec(query_vecs_num);
    for (int i = 0; i < query_vecs_num; i++) {
        for (int j = 0; j < vec_dims; j++) {
            #ifdef __linux__
            float temp_val = random() % 120;
            #else
            float temp_val = rand() % 120;
            #endif
            temp_val = temp_val / 100;
            input_content_vec[i].push_back(temp_val);
        }
    }
    vector<float> db_content_vec_con, input_content_vec_con;
    for (unsigned int i = 0; i < db_content_vec.size(); i++) {
        db_content_vec_con.insert(db_content_vec_con.end(),
                                  db_content_vec[i].begin(),
                                  db_content_vec[i].end());
    }
    for (unsigned int i = 0; i < input_content_vec.size(); i++) {
        input_content_vec_con.insert(input_content_vec_con.end(),
                                     input_content_vec[i].begin(),
                                     input_content_vec[i].end());
    }
    memcpy(input_data.get()->data, input_content_vec_con.data(), query_vecs_num * vec_dims * sizeof(float));
    memcpy(db_data.get()->data, db_content_vec_con.data(), database_vecs_num * vec_dims * sizeof(float));
    std::shared_ptr<Blob<int>> output_index = std::make_shared<Blob<int>>(query_vecs_num * sort_cnt);
    std::cout << "vec_dims: " << vec_dims << std::endl;
    std::cout << "query_vecs_num: " << query_vecs_num << std::endl;
    std::cout << "database_vecs_num: " << database_vecs_num << std::endl;
    std::cout << "sort_cnt: " << sort_cnt << std::endl;
    std::cout << "is_transpose: " << is_transpose << std::endl;

    bm_device_mem_t input_data_global_addr_device,
                    db_data_global_addr_device,
                    buffer_global_addr_device,
                    output_sorted_similarity_global_addr_device,
                    output_sorted_index_global_addr_device;
    bm_malloc_device_byte(handle,
                          &input_data_global_addr_device,
                          dtype_size((data_type_t)input_dtype) * query_vecs_num * vec_dims);
    bm_malloc_device_byte(handle,
                          &db_data_global_addr_device,
                          dtype_size((data_type_t)input_dtype) * database_vecs_num * vec_dims);
    bm_malloc_device_byte(handle,
                          &buffer_global_addr_device,
                          dtype_size((data_type_t)DT_FP32) * query_vecs_num * database_vecs_num);
    bm_malloc_device_byte(handle,
                          &output_sorted_similarity_global_addr_device,
                          dtype_size((data_type_t)output_dtype) * query_vecs_num * sort_cnt);
    bm_malloc_device_byte(handle,
                          &output_sorted_index_global_addr_device,
                          dtype_size(DT_INT32) * query_vecs_num * sort_cnt);
    bm_memcpy_s2d(handle,
                  input_data_global_addr_device,
                  bm_mem_get_system_addr(bm_mem_from_system(input_data.get()->data)));
    bm_memcpy_s2d(handle,
                  db_data_global_addr_device,
                  bm_mem_get_system_addr(bm_mem_from_system(db_data.get()->data)));
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    bmcv_faiss_indexflatIP(handle,
                           input_data_global_addr_device,
                           db_data_global_addr_device,
                           buffer_global_addr_device,
                           output_sorted_similarity_global_addr_device,
                           output_sorted_index_global_addr_device,
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
    if (output_dtype == DT_FP32)
        bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_similarity_fp32.get()->data)),
                  output_sorted_similarity_global_addr_device);
    else
        bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_similarity_fp16.get()->data)),
                  output_sorted_similarity_global_addr_device);
    bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_index.get()->data)),
                  output_sorted_index_global_addr_device);
    if (is_transpose)
        matrix_transpose(db_content_vec);
    vector<vector<float>> ref_result(query_vecs_num);
    ref_result = matrix_mul_ref(input_content_vec, db_content_vec);
    if (output_dtype == DT_FP32) {
        if (false == result_compare_fp32(output_similarity_fp32.get()->data,
                                        output_index.get()->data,
                                        ref_result,
                                        sort_cnt)) {
            std::cout << "-------------faiss_indexflatIP fp32/fp32 compare failed-------" << std::endl;
            exit(-1);
        }
    } else {
        if (false == result_compare_fp16(output_similarity_fp16.get()->data,
                                        output_index.get()->data,
                                        ref_result,
                                        sort_cnt)) {
            std::cout << "-------------faiss_indexflatIP fp32/fp16 compare failed-------" << std::endl;
            exit(-1);
        }
    }
    std::cout << "-------------faiss_indexflatIP fp32 compare succeed-----------" << std::endl;
    bm_free_device(handle, input_data_global_addr_device);
    bm_free_device(handle, db_data_global_addr_device);
    bm_free_device(handle, buffer_global_addr_device);
    bm_free_device(handle, output_sorted_similarity_global_addr_device);
    bm_free_device(handle, output_sorted_index_global_addr_device);
    bm_dev_free(handle);
    return 0;
}

int32_t faiss_indexflatIP_fix8b_single_test(int vec_dims,
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

    std::shared_ptr<Blob<signed char>> input_data = std::make_shared<Blob<signed char>>(query_vecs_num * vec_dims);
    std::shared_ptr<Blob<signed char>> db_data = std::make_shared<Blob<signed char>>(database_vecs_num * vec_dims);
    std::shared_ptr<Blob<int>> output_similarity = std::make_shared<Blob<int>>(query_vecs_num * sort_cnt);
    /* assign random data */
    vector<vector<signed char>> db_content_vec(is_transpose ? database_vecs_num : vec_dims);
    if (is_transpose) {
        for (int i = 0; i < database_vecs_num; i++) {
            for (int j = 0; j < vec_dims; j++) {
                #ifdef __linux__
                signed char temp_val = random() % 20 + 1;
                #else
                signed char temp_val = rand() % 20 - 10;
                #endif
                db_content_vec[i].push_back(temp_val);
            }
        }
    } else {
        for (int i = 0; i < vec_dims; i++) {
            for (int j = 0; j < database_vecs_num; j++) {
                #ifdef __linux__
                signed char temp_val = random() % 20 - 10;
                #else
                signed char temp_val = rand() % 20 - 10;
                #endif
                db_content_vec[i].push_back(temp_val);
            }
        }
    }
    vector<vector<signed char>> input_content_vec(query_vecs_num);
    for (int i = 0; i < query_vecs_num; i++) {
        for (int j = 0; j < vec_dims; j++) {
            #ifdef __linux__
            signed char temp_val = random() % 20 - 10;
            #else
            signed char temp_val = rand() % 20 - 10;
            #endif
            input_content_vec[i].push_back(temp_val);
        }
    }
    vector<signed char> db_content_vec_con, input_content_vec_con;
    for (unsigned int i = 0; i < db_content_vec.size(); i++) {
        db_content_vec_con.insert(db_content_vec_con.end(),
                                  db_content_vec[i].begin(),
                                  db_content_vec[i].end());
    }
    for (unsigned int i = 0; i < input_content_vec.size(); i++) {
        input_content_vec_con.insert(input_content_vec_con.end(),
                                     input_content_vec[i].begin(),
                                     input_content_vec[i].end());
    }
    memcpy(input_data.get()->data, input_content_vec_con.data(), query_vecs_num * vec_dims * sizeof(signed char));
    memcpy(db_data.get()->data, db_content_vec_con.data(), database_vecs_num * vec_dims * sizeof(signed char));

    std::shared_ptr<Blob<int>> output_index = std::make_shared<Blob<int>>(query_vecs_num * sort_cnt);
    std::cout << "vec_dims: " << vec_dims << std::endl;
    std::cout << "query_vecs_num: " << query_vecs_num << std::endl;
    std::cout << "database_vecs_num: " << database_vecs_num << std::endl;
    std::cout << "sort_cnt: " << sort_cnt << std::endl;
    std::cout << "is_transpose: " << is_transpose << std::endl;

    bm_device_mem_t input_data_global_addr_device,
                    db_data_global_addr_device,
                    buffer_global_addr_device,
                    output_sorted_similarity_global_addr_device,
                    output_sorted_index_global_addr_device;
    bm_malloc_device_byte(handle,
                          &input_data_global_addr_device,
                          dtype_size((data_type_t)input_dtype) * query_vecs_num * vec_dims);
    bm_malloc_device_byte(handle,
                          &db_data_global_addr_device,
                          dtype_size((data_type_t)input_dtype) * database_vecs_num * vec_dims);
    bm_malloc_device_byte(handle,
                          &buffer_global_addr_device,
                          dtype_size((data_type_t)DT_FP32) * query_vecs_num * database_vecs_num);
    bm_malloc_device_byte(handle,
                          &output_sorted_similarity_global_addr_device,
                          dtype_size((data_type_t)output_dtype) * query_vecs_num * sort_cnt);
    bm_malloc_device_byte(handle,
                          &output_sorted_index_global_addr_device,
                          dtype_size(DT_INT32) * query_vecs_num * sort_cnt);
    bm_memcpy_s2d(handle,
                  input_data_global_addr_device,
                  bm_mem_get_system_addr(bm_mem_from_system(input_data.get()->data)));
    bm_memcpy_s2d(handle,
                  db_data_global_addr_device,
                  bm_mem_get_system_addr(bm_mem_from_system(db_data.get()->data)));
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    bmcv_faiss_indexflatIP(handle,
                           input_data_global_addr_device,
                           db_data_global_addr_device,
                           buffer_global_addr_device,
                           output_sorted_similarity_global_addr_device,
                           output_sorted_index_global_addr_device,
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
    bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_similarity.get()->data)),
                  output_sorted_similarity_global_addr_device);
    bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_index.get()->data)),
                  output_sorted_index_global_addr_device);
    if (is_transpose)
        matrix_transpose(db_content_vec);
    vector<vector<int>> ref_result(query_vecs_num);
    ref_result = matrix_mul_ref(input_content_vec, db_content_vec);
    if (false == result_compare<int>(output_similarity.get()->data,
                                     output_index.get()->data,
                                     ref_result,
                                     sort_cnt)) {
        std::cout << "------------faiss_indexflatIP FIX8B COMPARE failed-----" << std::endl;
        exit(-1);
    }
    std::cout << "-------------faiss_indexflatIP FIX8B COMPARE succeed-----------" << std::endl;
    bm_free_device(handle, input_data_global_addr_device);
    bm_free_device(handle, db_data_global_addr_device);
    bm_free_device(handle, buffer_global_addr_device);
    bm_free_device(handle, output_sorted_similarity_global_addr_device);
    bm_free_device(handle, output_sorted_index_global_addr_device);
    bm_dev_free(handle);
    return 0;
}

int main(int argc, char* args[]) {
    unsigned int seed = (unsigned)time(NULL);
    srand(seed);
    std::cout << "random seed = " << seed << std::endl;

    int sort_cnt = rand() % 100 + 1;
    int database_vecs_num = rand() % 10000 + 1 + sort_cnt;
    int query_vecs_num = rand() % 64 + 1;
    int vec_dims = rand() % 256 + 1;
    int is_transpose = rand() % 2;
    int input_dtype = rand() % 2 == 0? DT_FP32 : DT_FP16;
    int output_dtype = rand() % 2 == 0? DT_FP32 : DT_FP16;

    if (argc > 1) database_vecs_num = atoi(args[1]);
    if (argc > 2) query_vecs_num = atoi(args[2]);
    if (argc > 3) sort_cnt = atoi(args[3]);
    if (argc > 4) vec_dims = atoi(args[4]);
    if (argc > 5) is_transpose = atoi(args[5]);
    if (argc > 6) input_dtype = atoi(args[6]);
    if (argc > 7) output_dtype = atoi(args[7]);

    std::cout << "------------parameter------------" << std::endl;
    std::cout << "database_num: " << database_vecs_num << std::endl;
    std::cout << "query_num:    " << query_vecs_num << std::endl;
    std::cout << "sort_count:   " << sort_cnt << std::endl;
    std::cout << "data_dims:    " << vec_dims<< std::endl;
    std::cout << "transpose:    " << is_transpose << std::endl;
    std::cout << "input_dtype:  " << (input_dtype == DT_FP32?"fp32":"fp16") << std::endl;
    std::cout << "output_dtype: " << (output_dtype == DT_FP32?"fp32":"fp16") << std::endl;

    if (input_dtype == DT_FP32) {
        if (BM_SUCCESS != faiss_indexflatIP_fp32_single_test(vec_dims, query_vecs_num, database_vecs_num, sort_cnt, is_transpose, input_dtype, output_dtype)) {
            return -1;
        }
    } else if (input_dtype == DT_FP16) {
        if (BM_SUCCESS != faiss_indexflatIP_fp16_single_test(vec_dims, query_vecs_num, database_vecs_num, sort_cnt, is_transpose, input_dtype, output_dtype)) {
            return -1;
        }
    } else {
        if (BM_SUCCESS != faiss_indexflatIP_fix8b_single_test(vec_dims, query_vecs_num, database_vecs_num, sort_cnt, is_transpose, input_dtype, output_dtype)) {
            return -1;
        }
    }
    return 0;
}