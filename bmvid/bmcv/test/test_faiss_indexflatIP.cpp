#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif
#include <stdint.h>
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include <pthread.h>

#define MAX(a,b)	((a) > (b) ? (a) : (b))

typedef struct {
    int loop;
    int database_vecs_num;
    int query_vecs_num;
    int sort_cnt;
    int vec_dims;
    int is_transpose;
    int input_dtype;
    int output_dtype;
    bm_handle_t handle;
} faiss_indexflatIP_thread_arg_t;

int matrix_mul_ref_int(signed char** input_data_vec, signed char** db_data, int** ref_result, int query_vecs_num,
                          int database_vecs_num, int vec_dims) {
    int a = 0;
    for (int query_cnt = 0; query_cnt < query_vecs_num; query_cnt++) {
        for (int db_cnt = 0; db_cnt < database_vecs_num; db_cnt++) {
            a = 0;
            for (int dims_cnt = 0; dims_cnt < vec_dims; dims_cnt++) {

                a += input_data_vec[query_cnt][dims_cnt] * db_data[dims_cnt][db_cnt];
            }
            a = a > 0x7fff ? 0x7fff : a;
            a = a < -0x8000 ? -0x8000 : a;
            ref_result[query_cnt][db_cnt] = a;
        }
    }
    return 0;
}

int matrix_mul_ref_fp16(fp16** input_data_vec, fp16** db_data, float** ref_result, int query_vecs_num,
                          int database_vecs_num, int vec_dims) {
    float a = 0.0f;
    for (int query_cnt = 0; query_cnt < query_vecs_num; query_cnt++) {
        for (int db_cnt = 0; db_cnt < database_vecs_num; db_cnt++) {
            a = 0.0f;
            for (int dims_cnt = 0; dims_cnt < vec_dims; dims_cnt++) {
                a += fp16tofp32(input_data_vec[query_cnt][dims_cnt]) * fp16tofp32(db_data[dims_cnt][db_cnt]);
            }
            ref_result[query_cnt][db_cnt] = a;
        }
    }
    return 0;
}

int matrix_mul_ref_fp32(float** input_data_vec, float** db_data, float** ref_result, int query_vecs_num,
                          int database_vecs_num, int vec_dims) {
    float a = 0.0f;
    for (int query_cnt = 0; query_cnt < query_vecs_num; query_cnt++) {
        for (int db_cnt = 0; db_cnt < database_vecs_num; db_cnt++) {
            a = 0.0f;
            for (int dims_cnt = 0; dims_cnt < vec_dims; dims_cnt++) {
                a += input_data_vec[query_cnt][dims_cnt] * db_data[dims_cnt][db_cnt];
            }
            ref_result[query_cnt][db_cnt] = a;
        }
    }
    return 0;
}

bm_status_t result_compare_int(int *tpu_result_similarity,
                        int *tpu_result_index,
                        int **ref_result,
                        int query_vecs_num,
                        int database_vecs_num,
                        int sort_cnt) {
    int **tmp_ref_result = (int **)malloc(query_vecs_num * sizeof(int*));
    for (int i = 0; i < query_vecs_num; i++) {
        tmp_ref_result[i] = (int*)malloc(database_vecs_num * sizeof(int));
        for (int j = 0; j < database_vecs_num; j++) {
            tmp_ref_result[i][j] = ref_result[i][j];
        }
    }
    for (int query_cnt = 0; query_cnt < query_vecs_num; query_cnt++) {
        for (int sort_indx = 0; sort_indx < sort_cnt; sort_indx++) {
            // Find the index of the largest element
            int ref_index = 0;
            int max_similarity = tmp_ref_result[query_cnt][0];
            for (int i = 1; i < database_vecs_num - sort_indx; i++) {
                if (tmp_ref_result[query_cnt][i] > max_similarity) {
                    max_similarity = tmp_ref_result[query_cnt][i];
                    ref_index = i;
                }
            }
            int ref_similarity = tmp_ref_result[query_cnt][ref_index];
            // Find index in original array
            int ref_index_origin = -1;
            for (int i = 0; i < database_vecs_num; i++) {
                if (fabs(ref_result[query_cnt][i] - ref_similarity) < 1e-6) {
                    ref_index_origin = i;
                    break;
                }
            }

            if (fabs((float)tpu_result_similarity[query_cnt * sort_cnt + sort_indx] - (float)ref_similarity) > (1e-2)) {
                printf("tpu_res[%d][%d][%d] %d ref_result[%d][%d][%d] %d\n",
                       query_cnt, sort_indx, tpu_result_index[query_cnt * sort_cnt + sort_indx],
                       tpu_result_similarity[query_cnt * sort_cnt + sort_indx],
                       query_cnt, sort_indx, ref_index_origin, ref_similarity);
                for (int i = 0; i < query_vecs_num; i++) {
                    free(tmp_ref_result[i]);
                }
                free(tmp_ref_result);
                return BM_ERR_FAILURE;
            }
            // Remove the largest element found
            for (int i = ref_index; i < database_vecs_num - 1; i++) {
                tmp_ref_result[query_cnt][i] = tmp_ref_result[query_cnt][i + 1];
            }
        }
    }

    for (int i = 0; i < query_vecs_num; i++) {
        free(tmp_ref_result[i]);
    }
    free(tmp_ref_result);

    return BM_SUCCESS;
}

bm_status_t result_compare_fp16(fp16 *tpu_result_similarity,
                         int *tpu_result_index,
                         float **ref_result,
                         int query_vecs_num,
                         int database_vecs_num,
                         int sort_cnt) {
    float **tmp_ref_result = (float **)malloc(query_vecs_num * sizeof(float *));
    for (int i = 0; i < query_vecs_num; i++) {
        tmp_ref_result[i] = (float *)malloc(database_vecs_num * sizeof(float));
        for (int j = 0; j < database_vecs_num; j++) {
            tmp_ref_result[i][j] = ref_result[i][j];
        }
    }
    for (int query_cnt = 0; query_cnt < query_vecs_num; query_cnt++) {
        for (int sort_indx = 0; sort_indx < sort_cnt; sort_indx++) {
            int ref_index = 0;
            float max_similarity = tmp_ref_result[query_cnt][0];
            for (int i = 1; i < database_vecs_num - sort_indx; i++) {
                if (tmp_ref_result[query_cnt][i] > max_similarity) {
                    max_similarity = tmp_ref_result[query_cnt][i];
                    ref_index = i;
                }
            }
            float ref_similarity = tmp_ref_result[query_cnt][ref_index];

            int ref_index_origin = -1;
            for (int i = 0; i < database_vecs_num; i++) {
                if (ref_result[query_cnt][i] == ref_similarity) {
                    ref_index_origin = i;
                    break;
                }
            }
            if (fabs((float)fp16tofp32(tpu_result_similarity[query_cnt * sort_cnt + sort_indx]) - (float)fp16tofp32((fp32tofp16(ref_similarity, 1)))) > 3e-1) {
                printf("cpu&&tpu result compare failed!\n");
                printf("tpu_res[%d][%d][%d] %f ref_result[%d][%d][%d] %f %f\n",
                       query_cnt, sort_indx, tpu_result_index[query_cnt * sort_cnt + sort_indx],
                       fp16tofp32(tpu_result_similarity[query_cnt * sort_cnt + sort_indx]),
                       query_cnt, sort_indx, ref_index_origin, fp16tofp32((fp32tofp16(ref_similarity, 1))), ref_similarity);
                for (int i = 0; i < query_vecs_num; i++) {
                    free(tmp_ref_result[i]);
                }
                free(tmp_ref_result);
                return BM_ERR_FAILURE;
            }
            for (int i = ref_index; i < database_vecs_num - 1; i++) {
                tmp_ref_result[query_cnt][i] = tmp_ref_result[query_cnt][i + 1];
            }
        }
    }

    for (int i = 0; i < query_vecs_num; i++) {
        free(tmp_ref_result[i]);
    }
    free(tmp_ref_result);
    return BM_SUCCESS;
}
bm_status_t result_compare_fp32(float *tpu_result_similarity,
                         int *tpu_result_index,
                         float **ref_result,
                         int query_vecs_num,
                         int database_vecs_num,
                         int sort_cnt) {
    float **tmp_ref_result = (float **)malloc(query_vecs_num * sizeof(float *));
    for (int i = 0; i < query_vecs_num; i++) {
        tmp_ref_result[i] = (float *)malloc(database_vecs_num * sizeof(float));
        for (int j = 0; j < database_vecs_num; j++) {
            tmp_ref_result[i][j] = ref_result[i][j];
        }
    }
    for (int query_cnt = 0; query_cnt < query_vecs_num; query_cnt++) {
        for (int sort_indx = 0; sort_indx < sort_cnt; sort_indx++) {
            int ref_index = 0;
            float max_similarity = tmp_ref_result[query_cnt][0];
            for (int i = 1; i < database_vecs_num - sort_indx; i++) {
                if (tmp_ref_result[query_cnt][i] > max_similarity) {
                    max_similarity = tmp_ref_result[query_cnt][i];
                    ref_index = i;
                }
            }
            float ref_similarity = tmp_ref_result[query_cnt][ref_index];
            int ref_index_origin = -1;
            for (int i = 0; i < database_vecs_num; i++) {
                if (fabs(ref_result[query_cnt][i] - ref_similarity) < 1e-6) {
                    ref_index_origin = i;
                    break;
                }
            }
            const float abs_tol = 0.2f;
            const float rel_tol = 0.002f;
            if (fabs(tpu_result_similarity[query_cnt * sort_cnt + sort_indx] - ref_similarity) > MAX(abs_tol, rel_tol * fabs(ref_similarity))) {
                printf("tpu_res[%d][%d][%d] %f ref_result[%d][%d][%d] %f\n",
                       query_cnt, sort_indx, tpu_result_index[query_cnt * sort_cnt + sort_indx],
                       tpu_result_similarity[query_cnt * sort_cnt + sort_indx],
                       query_cnt, sort_indx, ref_index_origin, ref_similarity);
                for (int i = 0; i < query_vecs_num; i++) {
                    free(tmp_ref_result[i]);
                }
                free(tmp_ref_result);
                return BM_ERR_FAILURE;
            }
            for (int i = ref_index; i < database_vecs_num - 1; i++) {
                tmp_ref_result[query_cnt][i] = tmp_ref_result[query_cnt][i + 1];
            }
        }
    }
    for (int i = 0; i < query_vecs_num; i++) {
        free(tmp_ref_result[i]);
    }
    free(tmp_ref_result);
    return BM_SUCCESS;
}

bm_status_t faiss_indexflatIP_fix8b_single_test(bm_handle_t handle,
                                                int vec_dims,
                                                int query_vecs_num,
                                                int database_vecs_num,
                                                int sort_cnt,
                                                int is_transpose,
                                                int input_dtype,
                                                int output_dtype) {
    printf("database_vecs_num: %d\n", database_vecs_num);
    printf("query_num:         %d\n", query_vecs_num);
    printf("sort_count:        %d\n", sort_cnt);
    printf("data_dims:         %d\n", vec_dims);
    printf("transpose:         %d\n", is_transpose);
    printf("input_dtype:       %d\n", input_dtype);
    printf("output_dtype:      %d\n", output_dtype);
    int i, j;
    bm_status_t ret = BM_SUCCESS;
    signed char* input_data = (signed char*)malloc(query_vecs_num * vec_dims * sizeof(signed char));
    signed char* db_data = (signed char*)malloc(database_vecs_num * vec_dims * sizeof(signed char));
    int* output_similarity = (int*)malloc(query_vecs_num * sort_cnt * sizeof(int));
    signed char** input_content_vec = (signed char**)malloc(query_vecs_num * sizeof(signed char*));
    for (i = 0; i < query_vecs_num; i++) {
        input_content_vec[i] = (signed char*)malloc(vec_dims * sizeof(signed char));
        for (j = 0; j < vec_dims; j++) {
            #ifdef __linux__
            signed char temp_val = random() % 20 - 10;
            #else
            signed char temp_val = rand() % 20 - 10;
            #endif
            input_content_vec[i][j] = temp_val;
        }
    }
    signed char** db_content_vec = (signed char**)malloc((is_transpose ? database_vecs_num : vec_dims) * sizeof(signed char*));
    signed char** db_content_vec_trans = (signed char**)malloc((is_transpose ? vec_dims : database_vecs_num) * sizeof(signed char*));

    if (is_transpose) {
        for(i = 0; i < vec_dims; i++) {
            db_content_vec_trans[i] = (signed char*)malloc(database_vecs_num * sizeof(signed char));
        }
        for (i = 0; i < database_vecs_num; i++) {
            db_content_vec[i] = (signed char*)malloc(vec_dims * sizeof(signed char));
            for (j = 0; j < vec_dims; j++) {
                #ifdef __linux__
                signed char temp_val = random() % 20 + 1;
                #else
                signed char temp_val = rand() % 20 - 10;
                #endif
                db_content_vec[i][j] = temp_val;
                db_content_vec_trans[j][i] = temp_val;
            }
        }
    } else {
        for(i = 0; i < database_vecs_num; i++) {
            db_content_vec_trans[i] = (signed char*)malloc(vec_dims * sizeof(signed char));
        }
        for (i = 0; i < vec_dims; i++) {
            db_content_vec[i] = (signed char*)malloc(database_vecs_num * sizeof(signed char));
            for (j = 0; j < database_vecs_num; j++) {
                #ifdef __linux__
                signed char temp_val = random() % 20 - 10;
                #else
                signed char temp_val = rand() % 20 - 10;
                #endif
                db_content_vec[i][j] = temp_val;
                db_content_vec_trans[j][i] = temp_val;
            }
        }
    }

    for (i = 0; i < query_vecs_num; ++i) {
        for (j = 0; j < vec_dims; ++j) {
            input_data[i * vec_dims + j] = input_content_vec[i][j];
        }
    }
    if (is_transpose) {
        for (i = 0; i < database_vecs_num; ++i) {
            for (j = 0; j < vec_dims; ++j) {
                db_data[i * vec_dims + j] = db_content_vec[i][j];
            }
        }
    } else {
        for (i = 0; i < vec_dims; ++i) {
            for (j = 0; j < database_vecs_num; ++j) {
                db_data[i * database_vecs_num + j] = db_content_vec[i][j];
            }
        }
    }
    int* output_index = (int*)malloc(query_vecs_num * sort_cnt * sizeof(int));
    int** ref_result = (int**)calloc(query_vecs_num, sizeof(int*));
    for (i = 0; i < query_vecs_num; i++) {
        ref_result[i] = (int*)calloc(database_vecs_num, sizeof(int));
    }
    bm_device_mem_t input_data_global_addr_device,
                    db_data_global_addr_device,
                    buffer_global_addr_device,
                    output_sorted_similarity_global_addr_device,
                    output_sorted_index_global_addr_device;
    ret = bm_malloc_device_byte(handle,
                        &input_data_global_addr_device,
                        dtype_size((bm_data_type_t)input_dtype) * query_vecs_num * vec_dims);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte input_data_global_addr_device failed!\n");
        input_data_global_addr_device = BM_MEM_NULL;
        goto free_mem1;
    }
    ret = bm_malloc_device_byte(handle,
                            &db_data_global_addr_device,
                            dtype_size((bm_data_type_t)input_dtype) * database_vecs_num * vec_dims);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte output_sorted_index_global_addr_device failed!\n");
        bm_free_device(handle, input_data_global_addr_device);
        db_data_global_addr_device = BM_MEM_NULL;
        goto free_mem1;
    }
    ret = bm_malloc_device_byte(handle,
                            &buffer_global_addr_device,
                            dtype_size((bm_data_type_t)DT_FP32) * query_vecs_num * database_vecs_num);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte output_sorted_index_global_addr_device failed!\n");
        bm_free_device(handle, input_data_global_addr_device);
        bm_free_device(handle, db_data_global_addr_device);
        buffer_global_addr_device = BM_MEM_NULL;
        goto free_mem1;
    }
    ret = bm_malloc_device_byte(handle,
                            &output_sorted_similarity_global_addr_device,
                            dtype_size((bm_data_type_t)output_dtype) * query_vecs_num * sort_cnt);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte output_sorted_index_global_addr_device failed!\n");
        bm_free_device(handle, input_data_global_addr_device);
        bm_free_device(handle, db_data_global_addr_device);
        bm_free_device(handle, buffer_global_addr_device);
        output_sorted_similarity_global_addr_device = BM_MEM_NULL;
        goto free_mem1;
    }
    ret = bm_malloc_device_byte(handle,
                            &output_sorted_index_global_addr_device,
                            dtype_size(DT_INT32) * query_vecs_num * sort_cnt);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte output_sorted_index_global_addr_device failed!\n");
        bm_free_device(handle, input_data_global_addr_device);
        bm_free_device(handle, db_data_global_addr_device);
        bm_free_device(handle, buffer_global_addr_device);
        bm_free_device(handle, output_sorted_similarity_global_addr_device);
        output_sorted_index_global_addr_device = BM_MEM_NULL;
        goto free_mem1;
    }
    ret = bm_memcpy_s2d(handle,
                  input_data_global_addr_device,
                  bm_mem_get_system_addr(bm_mem_from_system(input_data)));
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d input_data_global_addr_device failed!\n");
        goto free_mem;
    }
    ret = bm_memcpy_s2d(handle,
                  db_data_global_addr_device,
                  bm_mem_get_system_addr(bm_mem_from_system(db_data)));
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d db_data_global_addr_device failed!\n");
        goto free_mem;
    }
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    ret = bmcv_faiss_indexflatIP(handle,
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
    printf("TPU using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    if (BM_SUCCESS != ret) {
        printf("bmcv_faiss_indexflatIP api error\n");
        goto free_mem;
    }
    ret = bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_similarity)),
                  output_sorted_similarity_global_addr_device);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s output_similarity failed!\n");
        goto free_mem;
    }
    ret = bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_index)),
                  output_sorted_index_global_addr_device);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s output_index failed!\n");
        goto free_mem;
    }
    gettimeofday(&t1, NULL);
    if (is_transpose) {
        matrix_mul_ref_int(input_content_vec, db_content_vec_trans, ref_result, query_vecs_num, database_vecs_num, vec_dims);
    } else {
        matrix_mul_ref_int(input_content_vec, db_content_vec, ref_result, query_vecs_num, database_vecs_num, vec_dims);
    }
    gettimeofday(&t2, NULL);
    printf("CPU using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    ret = result_compare_int(output_similarity,
        output_index,
        ref_result,
        query_vecs_num,
        database_vecs_num,
        sort_cnt);
    if (ret != BM_SUCCESS) {
        printf("------------faiss_indexflatIP FIX8B COMPARE failed-----\n");
    } else {
        printf("-------------faiss_indexflatIP FIX8B COMPARE succeed-----------\n");
    }
free_mem:
    bm_free_device(handle, input_data_global_addr_device);
    bm_free_device(handle, db_data_global_addr_device);
    bm_free_device(handle, buffer_global_addr_device);
    bm_free_device(handle, output_sorted_similarity_global_addr_device);
    bm_free_device(handle, output_sorted_index_global_addr_device);
free_mem1:
    free(input_data);
    free(db_data);
    free(output_similarity);
    for (i = 0; i < query_vecs_num; ++i) {
        free(input_content_vec[i]);
        free(ref_result[i]);
    }
    if (is_transpose) {
        for (i = 0; i < database_vecs_num; ++i) {
            free(db_content_vec[i]);
        }
        for (i = 0; i < vec_dims; ++i) {
            free(db_content_vec_trans[i]);
        }
    } else {
        for (i = 0; i < vec_dims; ++i) {
            free(db_content_vec[i]);
        }
        for (i = 0; i < database_vecs_num; ++i) {
            free(db_content_vec_trans[i]);
        }
    }
    free(input_content_vec);
    free(db_content_vec);
    free(db_content_vec_trans);
    free(output_index);
    free(ref_result);
    return ret;
}

bm_status_t faiss_indexflatIP_fp16_single_test_u64 (bm_handle_t handle,
                                                    int vec_dims,
                                                    int query_vecs_num,
                                                    int database_vecs_num,
                                                    int sort_cnt,
                                                    int is_transpose,
                                                    int input_dtype,
                                                    int output_dtype) {
    printf("database_vecs_num: %d\n", database_vecs_num);
    printf("query_num:         %d\n", query_vecs_num);
    printf("sort_count:        %d\n", sort_cnt);
    printf("data_dims:         %d\n", vec_dims);
    printf("transpose:         %d\n", is_transpose);
    printf("input_dtype:       %d\n", input_dtype);
    printf("output_dtype:      %d\n", output_dtype);
    int i, j;
    bm_status_t ret = BM_SUCCESS;
    fp16* input_data = (fp16*)malloc(query_vecs_num * vec_dims * sizeof(fp16));
    fp16* db_data = (fp16*)malloc(database_vecs_num * vec_dims * sizeof(fp16));
    float* output_similarity_fp32 = (float*)malloc(query_vecs_num * sort_cnt * sizeof(float));
    fp16* output_similarity_fp16 = (fp16*)malloc(query_vecs_num * sort_cnt * sizeof(fp16));
    fp16** db_content_vec = (fp16**)malloc((is_transpose ? database_vecs_num : vec_dims) * sizeof(fp16*));
    fp16** db_content_vec_trans = (fp16**)malloc((is_transpose ? vec_dims : database_vecs_num) * sizeof(fp16*));
    if (is_transpose) {
        for(i = 0; i < vec_dims; i++) {
            db_content_vec_trans[i] = (fp16*)malloc(database_vecs_num * sizeof(fp16));
        }
        for (i = 0; i < database_vecs_num; i++) {
            db_content_vec[i] = (fp16*)malloc(vec_dims * sizeof(fp16));
            for (j = 0; j < vec_dims; j++) {
                #ifdef __linux__
                fp16 temp_val = fp32tofp16(random() % 120, 1);
                //fp16 temp_val = fp32tofp16(j * 0.1 + i * 0.53, 1);
                #else
                fp16 temp_val = fp32tofp16(rand() % 120, 0);
                //fp16 temp_val = fp32tofp16(rand() % 120, 1);
                #endif
                temp_val = fp32tofp16(fp16tofp32(temp_val) / 100, 0);
                db_content_vec[i][j] = temp_val;
                db_content_vec_trans[j][i] = temp_val;
            }
        }
    } else {
        for(i = 0; i < database_vecs_num; i++) {
            db_content_vec_trans[i] = (fp16*)malloc(vec_dims * sizeof(fp16));
        }
        for (i = 0; i < vec_dims; i++) {
            db_content_vec[i] = (fp16*)malloc(database_vecs_num * sizeof(fp16));
            for (j = 0; j < database_vecs_num; j++) {
                #ifdef __linux__
                fp16 temp_val = fp32tofp16(random() % 120, 1);
                //fp16 temp_val = fp32tofp16(i * 0.1 + j * 0.53, 1);
                #else
                fp16 temp_val = fp32tofp16(rand() % 120, 0);
                //fp16 temp_val = fp32tofp16(i * 0.1 + j * 0.53, 1);
                #endif
                temp_val = fp32tofp16(fp16tofp32(temp_val) / 100, 0);
                db_content_vec[i][j] = temp_val;
                db_content_vec_trans[j][i] = temp_val;
            }
        }
    }

    fp16** input_content_vec = (fp16**)malloc(query_vecs_num * sizeof(fp16*));
    for (i = 0; i < query_vecs_num; i++) {
        input_content_vec[i] = (fp16*)malloc(vec_dims * sizeof(fp16));
        for (j = 0; j < vec_dims; j++) {
            #ifdef __linux__
            fp16 temp_val = fp32tofp16(random() % 120, 1);
            #else
            fp16 temp_val = fp32tofp16(rand() % 120, 0);
            #endif
            temp_val = fp32tofp16(fp16tofp32(temp_val) / 100, 0);
            input_content_vec[i][j] = temp_val;
        }
    }

    for (i = 0; i < query_vecs_num; ++i) {
        for (j = 0; j < vec_dims; ++j) {
            input_data[i * vec_dims + j] = input_content_vec[i][j];
        }
    }
    if (is_transpose) {
        for (i = 0; i < database_vecs_num; ++i) {
            for (j = 0; j < vec_dims; ++j) {
                db_data[i * vec_dims + j] = db_content_vec[i][j];
            }
        }
    } else {
        for (i = 0; i < vec_dims; ++i) {
            for (j = 0; j < database_vecs_num; ++j) {
                db_data[i * database_vecs_num + j] = db_content_vec[i][j];
            }
        }
    }
    float** ref_result = (float**)malloc(query_vecs_num * sizeof(float*));
    for (i = 0; i < query_vecs_num; i++) {
        ref_result[i] = (float*)malloc(database_vecs_num * sizeof(float));
    }
    int* output_index = (int*)malloc(query_vecs_num * sort_cnt * sizeof(int));
    bm_device_mem_u64_t input_data_global_addr_device,
                    db_data_global_addr_device,
                    buffer_global_addr_device,
                    output_sorted_similarity_global_addr_device,
                    output_sorted_index_global_addr_device;
    ret = bm_malloc_device_byte_u64(handle,
                          &input_data_global_addr_device,
                          dtype_size((bm_data_type_t)input_dtype) * query_vecs_num * vec_dims);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte_u64 input_data_global_addr_device failed!\n");
        goto free_mem1;
    }
    ret = bm_malloc_device_byte_u64(handle,
                          &db_data_global_addr_device,
                          (u64)dtype_size((bm_data_type_t)input_dtype) * (u64)database_vecs_num * (u64)vec_dims);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte_u64 db_data_global_addr_device failed!\n");
        bm_free_device_u64(handle, input_data_global_addr_device);
        goto free_mem1;
    }
    ret = bm_malloc_device_byte_u64(handle,
                          &buffer_global_addr_device,
                          (u64)dtype_size((bm_data_type_t)DT_FP32) * (u64)query_vecs_num * (u64)database_vecs_num);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte_u64 buffer_global_addr_device failed!\n");
        bm_free_device_u64(handle, input_data_global_addr_device);
        bm_free_device_u64(handle, db_data_global_addr_device);
        goto free_mem1;
    }
    ret = bm_malloc_device_byte_u64(handle,
                          &output_sorted_similarity_global_addr_device,
                          dtype_size((bm_data_type_t)output_dtype) * query_vecs_num * sort_cnt);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte_u64 output_sorted_similarity_global_addr_device failed!\n");
        bm_free_device_u64(handle, input_data_global_addr_device);
        bm_free_device_u64(handle, db_data_global_addr_device);
        bm_free_device_u64(handle, buffer_global_addr_device);
        goto free_mem1;
    }
    ret = bm_malloc_device_byte_u64(handle,
                          &output_sorted_index_global_addr_device,
                          dtype_size(DT_INT32) * query_vecs_num * sort_cnt);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte_u64 output_sorted_index_global_addr_device failed!\n");
        bm_free_device_u64(handle, input_data_global_addr_device);
        bm_free_device_u64(handle, db_data_global_addr_device);
        bm_free_device_u64(handle, buffer_global_addr_device);
        bm_free_device_u64(handle, output_sorted_similarity_global_addr_device);
        goto free_mem1;
    }
    ret = bm_memcpy_s2d_u64(handle,
                  input_data_global_addr_device,
                  bm_mem_get_system_addr(bm_mem_from_system(input_data)));
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d_u64 input_data_global_addr_device failed!\n");
        goto free_mem;
    }
    ret = bm_memcpy_s2d_u64(handle,
                  db_data_global_addr_device,
                  bm_mem_get_system_addr(bm_mem_from_system(db_data)));
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d_u64 db_data_global_addr_device failed!\n");
        goto free_mem;
    }
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    ret = bmcv_faiss_indexflatIP_u64(handle,
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
    if (BM_SUCCESS != ret) {
        printf("bmcv_faiss_indexflatIP_u64 api error\n");
        goto free_mem;
    }
    printf("TPU using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    if (output_dtype == DT_FP32) {
        ret = bm_memcpy_d2s_u64(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_similarity_fp32)),
                  output_sorted_similarity_global_addr_device);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_d2s_u64 output_similarity_fp32 failed!\n");
            goto free_mem;
        }
    } else {
        ret = bm_memcpy_d2s_u64(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_similarity_fp16)),
                  output_sorted_similarity_global_addr_device);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_d2s_u64 output_similarity_fp16 failed!\n");
            goto free_mem;
        }
    }
    ret = bm_memcpy_d2s_u64(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_index)),
                  output_sorted_index_global_addr_device);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s_u64 output_index failed!\n");
        goto free_mem;
    }
    gettimeofday(&t1, NULL);
    if (is_transpose) {
        matrix_mul_ref_fp16(input_content_vec, db_content_vec_trans, ref_result, query_vecs_num, database_vecs_num, vec_dims);
    } else {
        matrix_mul_ref_fp16(input_content_vec, db_content_vec, ref_result, query_vecs_num, database_vecs_num, vec_dims);
    }
    gettimeofday(&t2, NULL);
    printf("CPU using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    if (output_dtype == DT_FP32) {
        ret = result_compare_fp32(output_similarity_fp32,
            output_index,
            ref_result,
            query_vecs_num,
            database_vecs_num,
            sort_cnt);
        if (ret != BM_SUCCESS) {
            printf("-------------faiss_indexflatIP_u64 fp32 compare failed-------\n");
        } else {
            printf("-------------faiss_indexflatIP_u64 fp32 compare succeed-----------\n");
        }
    } else {
        ret = result_compare_fp16(output_similarity_fp16,
            output_index,
            ref_result,
            query_vecs_num,
            database_vecs_num,
            sort_cnt);
        if (ret != BM_SUCCESS) {
            printf("-------------faiss_indexflatIP_u64 fp16 compare failed-------\n");
        } else {
            printf("-------------faiss_indexflatIP_u64 fp16 compare succeed-----------\n");
        }
    }
free_mem:
    bm_free_device_u64(handle, input_data_global_addr_device);
    bm_free_device_u64(handle, db_data_global_addr_device);
    bm_free_device_u64(handle, buffer_global_addr_device);
    bm_free_device_u64(handle, output_sorted_similarity_global_addr_device);
    bm_free_device_u64(handle, output_sorted_index_global_addr_device);
free_mem1:
    free(input_data);
    free(db_data);
    free(output_similarity_fp32);
    free(output_similarity_fp16);
    for (i = 0; i < query_vecs_num; ++i) {
        free(input_content_vec[i]);
        free(ref_result[i]);
    }
    if (is_transpose) {
        for (i = 0; i < database_vecs_num; ++i) {
            free(db_content_vec[i]);
        }
        for (i = 0; i < vec_dims; ++i) {
            free(db_content_vec_trans[i]);
        }
    } else {
        for (i = 0; i < vec_dims; ++i) {
            free(db_content_vec[i]);
        }
        for (i = 0; i < database_vecs_num; ++i) {
            free(db_content_vec_trans[i]);
        }
    }
    free(input_content_vec);
    free(db_content_vec);
    free(db_content_vec_trans);
    free(output_index);
    free(ref_result);
    return ret;
}

bm_status_t faiss_indexflatIP_fp16_single_test(bm_handle_t handle,
                                               int vec_dims,
                                               int query_vecs_num,
                                               int database_vecs_num,
                                               int sort_cnt,
                                               int is_transpose,
                                               int input_dtype,
                                               int output_dtype) {
    printf("database_vecs_num: %d\n", database_vecs_num);
    printf("query_num:         %d\n", query_vecs_num);
    printf("sort_count:        %d\n", sort_cnt);
    printf("data_dims:         %d\n", vec_dims);
    printf("transpose:         %d\n", is_transpose);
    printf("input_dtype:       %d\n", input_dtype);
    printf("output_dtype:      %d\n", output_dtype);
    int i, j;
    bm_status_t ret = BM_SUCCESS;
    fp16* input_data = (fp16*)malloc(query_vecs_num * vec_dims * sizeof(fp16));
    fp16* db_data = (fp16*)malloc(database_vecs_num * vec_dims * sizeof(fp16));
    float* output_similarity_fp32 = (float*)malloc(query_vecs_num * sort_cnt * sizeof(float));
    fp16* output_similarity_fp16 = (fp16*)malloc(query_vecs_num * sort_cnt * sizeof(fp16));
    fp16** db_content_vec = (fp16**)malloc((is_transpose ? database_vecs_num : vec_dims) * sizeof(fp16*));
    fp16** db_content_vec_trans = (fp16**)malloc((is_transpose ? vec_dims : database_vecs_num) * sizeof(fp16*));
    if (is_transpose) {
        for(i = 0; i < vec_dims; i++) {
            db_content_vec_trans[i] = (fp16*)malloc(database_vecs_num * sizeof(fp16));
        }
        for (i = 0; i < database_vecs_num; i++) {
            db_content_vec[i] = (fp16*)malloc(vec_dims * sizeof(fp16));
            for (j = 0; j < vec_dims; j++) {
                #ifdef __linux__
                fp16 temp_val = fp32tofp16(random() % 120, 1);
                //fp16 temp_val = fp32tofp16(j * 0.1 + i * 0.53, 1);
                #else
                fp16 temp_val = fp32tofp16(rand() % 120, 0);
                //fp16 temp_val = fp32tofp16(rand() % 120, 1);
                #endif
                temp_val = fp32tofp16(fp16tofp32(temp_val) / 100, 0);
                db_content_vec[i][j] = temp_val;
                db_content_vec_trans[j][i] = temp_val;
            }
        }
    } else {
        for(i = 0; i < database_vecs_num; i++) {
            db_content_vec_trans[i] = (fp16*)malloc(vec_dims * sizeof(fp16));
        }
        for (i = 0; i < vec_dims; i++) {
            db_content_vec[i] = (fp16*)malloc(database_vecs_num * sizeof(fp16));
            for (j = 0; j < database_vecs_num; j++) {
                #ifdef __linux__
                fp16 temp_val = fp32tofp16(random() % 120, 1);
                //fp16 temp_val = fp32tofp16(i * 0.1 + j * 0.53, 1);
                #else
                fp16 temp_val = fp32tofp16(rand() % 120, 0);
                //fp16 temp_val = fp32tofp16(i * 0.1 + j * 0.53, 1);
                #endif
                temp_val = fp32tofp16(fp16tofp32(temp_val) / 100, 0);
                db_content_vec[i][j] = temp_val;
                db_content_vec_trans[j][i] = temp_val;
            }
        }
    }

    fp16** input_content_vec = (fp16**)malloc(query_vecs_num * sizeof(fp16*));
    for (i = 0; i < query_vecs_num; i++) {
        input_content_vec[i] = (fp16*)malloc(vec_dims * sizeof(fp16));
        for (j = 0; j < vec_dims; j++) {
            #ifdef __linux__
            fp16 temp_val = fp32tofp16(random() % 120, 1);
            #else
            fp16 temp_val = fp32tofp16(rand() % 120, 0);
            #endif
            temp_val = fp32tofp16(fp16tofp32(temp_val) / 100, 0);
            input_content_vec[i][j] = temp_val;
        }
    }

    for (i = 0; i < query_vecs_num; ++i) {
        for (j = 0; j < vec_dims; ++j) {
            input_data[i * vec_dims + j] = input_content_vec[i][j];
        }
    }
    if (is_transpose) {
        for (i = 0; i < database_vecs_num; ++i) {
            for (j = 0; j < vec_dims; ++j) {
                db_data[i * vec_dims + j] = db_content_vec[i][j];
            }
        }
    } else {
        for (i = 0; i < vec_dims; ++i) {
            for (j = 0; j < database_vecs_num; ++j) {
                db_data[i * database_vecs_num + j] = db_content_vec[i][j];
            }
        }
    }
    float** ref_result = (float**)malloc(query_vecs_num * sizeof(float*));
    for (i = 0; i < query_vecs_num; i++) {
        ref_result[i] = (float*)malloc(database_vecs_num * sizeof(float));
    }
    int* output_index = (int*)malloc(query_vecs_num * sort_cnt * sizeof(int));
    bm_device_mem_t input_data_global_addr_device,
                    db_data_global_addr_device,
                    buffer_global_addr_device,
                    output_sorted_similarity_global_addr_device,
                    output_sorted_index_global_addr_device;
    ret = bm_malloc_device_byte(handle,
                          &input_data_global_addr_device,
                          dtype_size((bm_data_type_t)input_dtype) * query_vecs_num * vec_dims);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte input_data_global_addr_device failed!\n");
        input_data_global_addr_device = BM_MEM_NULL;
        goto free_mem1;
    }
    ret = bm_malloc_device_byte(handle,
                          &db_data_global_addr_device,
                          dtype_size((bm_data_type_t)input_dtype) * database_vecs_num * vec_dims);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte db_data_global_addr_device failed!\n");
        bm_free_device(handle, input_data_global_addr_device);
        db_data_global_addr_device = BM_MEM_NULL;
        goto free_mem1;
    }
    ret = bm_malloc_device_byte(handle,
                          &buffer_global_addr_device,
                          dtype_size((bm_data_type_t)DT_FP32) * query_vecs_num * database_vecs_num);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte buffer_global_addr_device failed!\n");
        bm_free_device(handle, input_data_global_addr_device);
        bm_free_device(handle, db_data_global_addr_device);
        buffer_global_addr_device = BM_MEM_NULL;
        goto free_mem1;
    }
    ret = bm_malloc_device_byte(handle,
                          &output_sorted_similarity_global_addr_device,
                          dtype_size((bm_data_type_t)output_dtype) * query_vecs_num * sort_cnt);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte output_sorted_similarity_global_addr_device failed!\n");
        bm_free_device(handle, input_data_global_addr_device);
        bm_free_device(handle, db_data_global_addr_device);
        bm_free_device(handle, buffer_global_addr_device);
        output_sorted_similarity_global_addr_device = BM_MEM_NULL;
        goto free_mem1;
    }
    ret = bm_malloc_device_byte(handle,
                          &output_sorted_index_global_addr_device,
                          dtype_size(DT_INT32) * query_vecs_num * sort_cnt);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte output_sorted_index_global_addr_device failed!\n");
        bm_free_device(handle, input_data_global_addr_device);
        bm_free_device(handle, db_data_global_addr_device);
        bm_free_device(handle, buffer_global_addr_device);
        bm_free_device(handle, output_sorted_similarity_global_addr_device);
        output_sorted_index_global_addr_device = BM_MEM_NULL;
        goto free_mem1;
    }
    ret = bm_memcpy_s2d(handle,
                  input_data_global_addr_device,
                  bm_mem_get_system_addr(bm_mem_from_system(input_data)));
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d input_data_global_addr_device failed!\n");
        goto free_mem;
    }
    ret = bm_memcpy_s2d(handle,
                  db_data_global_addr_device,
                  bm_mem_get_system_addr(bm_mem_from_system(db_data)));
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d db_data_global_addr_device failed!\n");
        goto free_mem;
    }
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    ret = bmcv_faiss_indexflatIP(handle,
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
    if (BM_SUCCESS != ret) {
        printf("bmcv_faiss_indexflatIP api error\n");
        goto free_mem;
    }
    printf("TPU using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    if (output_dtype == DT_FP32) {
        ret = bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_similarity_fp32)),
                  output_sorted_similarity_global_addr_device);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_d2s output_similarity_fp32 failed!\n");
            goto free_mem;
        }
    } else {
        ret = bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_similarity_fp16)),
                  output_sorted_similarity_global_addr_device);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_d2s output_similarity_fp16 failed!\n");
            goto free_mem;
        }
    }
    ret = bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_index)),
                  output_sorted_index_global_addr_device);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s output_index failed!\n");
        goto free_mem;
    }
    gettimeofday(&t1, NULL);
    if (is_transpose) {
        matrix_mul_ref_fp16(input_content_vec, db_content_vec_trans, ref_result, query_vecs_num, database_vecs_num, vec_dims);
    } else {
        matrix_mul_ref_fp16(input_content_vec, db_content_vec, ref_result, query_vecs_num, database_vecs_num, vec_dims);
    }
    gettimeofday(&t2, NULL);
    printf("CPU using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    if (output_dtype == DT_FP32) {
        ret = result_compare_fp32(output_similarity_fp32,
            output_index,
            ref_result,
            query_vecs_num,
            database_vecs_num,
            sort_cnt);
        if (ret != BM_SUCCESS) {
            printf("-------------faiss_indexflatIP fp32 compare failed-------\n");
        } else {
            printf("-------------faiss_indexflatIP fp32 compare succeed-----------\n");
        }
    } else {
        ret = result_compare_fp16(output_similarity_fp16,
            output_index,
            ref_result,
            query_vecs_num,
            database_vecs_num,
            sort_cnt);
        if (ret != BM_SUCCESS) {
            printf("-------------faiss_indexflatIP fp16 compare failed-------\n");
        } else {
            printf("-------------faiss_indexflatIP fp16 compare succeed-----------\n");
        }
    }
free_mem:
    bm_free_device(handle, input_data_global_addr_device);
    bm_free_device(handle, db_data_global_addr_device);
    bm_free_device(handle, buffer_global_addr_device);
    bm_free_device(handle, output_sorted_similarity_global_addr_device);
    bm_free_device(handle, output_sorted_index_global_addr_device);
free_mem1:
    free(input_data);
    free(db_data);
    free(output_similarity_fp32);
    free(output_similarity_fp16);
    for (i = 0; i < query_vecs_num; ++i) {
        free(input_content_vec[i]);
        free(ref_result[i]);
    }
    if (is_transpose) {
        for (i = 0; i < database_vecs_num; ++i) {
            free(db_content_vec[i]);
        }
        for (i = 0; i < vec_dims; ++i) {
            free(db_content_vec_trans[i]);
        }
    } else {
        for (i = 0; i < vec_dims; ++i) {
            free(db_content_vec[i]);
        }
        for (i = 0; i < database_vecs_num; ++i) {
            free(db_content_vec_trans[i]);
        }
    }
    free(input_content_vec);
    free(db_content_vec);
    free(db_content_vec_trans);
    free(output_index);
    free(ref_result);
    return ret;
}

bm_status_t faiss_indexflatIP_fp32_single_test(bm_handle_t handle,
                                               int vec_dims,
                                               int query_vecs_num,
                                               int database_vecs_num,
                                               int sort_cnt,
                                               int is_transpose,
                                               int input_dtype,
                                               int output_dtype) {
    printf("database_vecs_num: %d\n", database_vecs_num);
    printf("query_num:         %d\n", query_vecs_num);
    printf("sort_count:        %d\n", sort_cnt);
    printf("data_dims:         %d\n", vec_dims);
    printf("transpose:         %d\n", is_transpose);
    printf("input_dtype:       %d\n", input_dtype);
    printf("output_dtype:      %d\n", output_dtype);
    int i, j;
    bm_status_t ret = BM_SUCCESS;
    float* input_data = (float*)malloc(query_vecs_num * vec_dims * sizeof(float));
    float* db_data = (float*)malloc(database_vecs_num * vec_dims * sizeof(float));
    float* output_similarity_fp32 = (float*)malloc(query_vecs_num * sort_cnt * sizeof(float));
    fp16* output_similarity_fp16 = (fp16*)malloc(query_vecs_num * sort_cnt * sizeof(fp16));
    float** input_content_vec = (float**)malloc(query_vecs_num * sizeof(float*));
    for (i = 0; i < query_vecs_num; i++) {
        input_content_vec[i] = (float*)malloc(vec_dims * sizeof(float));
        for (j = 0; j < vec_dims; j++) {
            #ifdef __linux__
            float temp_val = random() % 120;
            #else
            float temp_val = rand() % 120;
            #endif
            temp_val = temp_val / 100;
            input_content_vec[i][j] = temp_val;
        }
    }

    float** db_content_vec = (float**)malloc((is_transpose ? database_vecs_num : vec_dims) * sizeof(float*));
    float** db_content_vec_trans = (float**)malloc((is_transpose ? vec_dims : database_vecs_num) * sizeof(float*));
    if (is_transpose) {
        for(i = 0; i < vec_dims; i++) {
            db_content_vec_trans[i] = (float*)malloc(database_vecs_num * sizeof(float));
        }
        for (i = 0; i < database_vecs_num; i++) {
            db_content_vec[i] = (float*)malloc(vec_dims * sizeof(float));
            for (j = 0; j < vec_dims; j++) {
                #ifdef __linux__
                //float temp_val = j * 0.1 + i * 1;
                float temp_val = random() % 120;
                #else
                float temp_val = rand() % 120;
                #endif
                temp_val = temp_val / 100;
                db_content_vec[i][j] = temp_val;
                db_content_vec_trans[j][i] = temp_val;
            }
        }
    } else {
        for(i = 0; i < database_vecs_num; i++) {
            db_content_vec_trans[i] = (float*)malloc(vec_dims * sizeof(float));
        }
        for (i = 0; i < vec_dims; i++) {
            db_content_vec[i] = (float*)malloc(database_vecs_num * sizeof(float));
            for (j = 0; j < database_vecs_num; j++) {
                #ifdef __linux__
                float temp_val = random() % 120;
                #else
                float temp_val = rand() % 120;
                #endif
                temp_val = temp_val / 100;
                db_content_vec[i][j] = temp_val;
                db_content_vec_trans[j][i] = temp_val;
            }
        }
    }
    for (i = 0; i < query_vecs_num; ++i) {
        for (j = 0; j < vec_dims; ++j) {
            input_data[i * vec_dims + j] = input_content_vec[i][j];
        }
    }
    if (is_transpose) {
        for (i = 0; i < database_vecs_num; ++i) {
            for (j = 0; j < vec_dims; ++j) {
                db_data[i * vec_dims + j] = db_content_vec[i][j];
            }
        }
    } else {
        for (i = 0; i < vec_dims; ++i) {
            for (j = 0; j < database_vecs_num; ++j) {
                db_data[i * database_vecs_num + j] = db_content_vec[i][j];
            }
        }
    }
    int* output_index = (int*)malloc(query_vecs_num * sort_cnt * sizeof(int));
    float** ref_result = (float**)malloc(query_vecs_num * sizeof(float*));
    for (i = 0; i < query_vecs_num; i++) {
        ref_result[i] = (float*)malloc(database_vecs_num * sizeof(float));
    }
    bm_device_mem_t input_data_global_addr_device,
                    db_data_global_addr_device,
                    buffer_global_addr_device,
                    output_sorted_similarity_global_addr_device,
                    output_sorted_index_global_addr_device;
    ret = bm_malloc_device_byte(handle,
                          &input_data_global_addr_device,
                          dtype_size((bm_data_type_t)input_dtype) * query_vecs_num * vec_dims);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte input_data_global_addr_device failed!\n");
        input_data_global_addr_device = BM_MEM_NULL;
        goto free_mem1;
    }
    ret = bm_malloc_device_byte(handle,
                          &db_data_global_addr_device,
                          dtype_size((bm_data_type_t)input_dtype) * database_vecs_num * vec_dims);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte output_sorted_index_global_addr_device failed!\n");
        bm_free_device(handle, input_data_global_addr_device);
        db_data_global_addr_device = BM_MEM_NULL;
        goto free_mem1;
    }
    ret = bm_malloc_device_byte(handle,
                          &buffer_global_addr_device,
                          dtype_size((bm_data_type_t)DT_FP32) * query_vecs_num * database_vecs_num);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte output_sorted_index_global_addr_device failed!\n");
        bm_free_device(handle, input_data_global_addr_device);
        bm_free_device(handle, db_data_global_addr_device);
        buffer_global_addr_device = BM_MEM_NULL;
        goto free_mem1;
    }
    ret = bm_malloc_device_byte(handle,
                          &output_sorted_similarity_global_addr_device,
                          dtype_size((bm_data_type_t)output_dtype) * query_vecs_num * sort_cnt);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte output_sorted_index_global_addr_device failed!\n");
        bm_free_device(handle, input_data_global_addr_device);
        bm_free_device(handle, db_data_global_addr_device);
        bm_free_device(handle, buffer_global_addr_device);
        output_sorted_similarity_global_addr_device = BM_MEM_NULL;
        goto free_mem1;
    }
    ret = bm_malloc_device_byte(handle,
                          &output_sorted_index_global_addr_device,
                          dtype_size(DT_INT32) * query_vecs_num * sort_cnt);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte output_sorted_index_global_addr_device failed!\n");
        bm_free_device(handle, input_data_global_addr_device);
        bm_free_device(handle, db_data_global_addr_device);
        bm_free_device(handle, buffer_global_addr_device);
        bm_free_device(handle, output_sorted_similarity_global_addr_device);
        output_sorted_index_global_addr_device = BM_MEM_NULL;
        goto free_mem1;
    }
    ret = bm_memcpy_s2d(handle,
                  input_data_global_addr_device,
                  bm_mem_get_system_addr(bm_mem_from_system(input_data)));
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d input_data_global_addr_device failed!\n");
        goto free_mem;
    }
    ret = bm_memcpy_s2d(handle,
                  db_data_global_addr_device,
                  bm_mem_get_system_addr(bm_mem_from_system(db_data)));
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d db_data_global_addr_device failed!\n");
        goto free_mem;
    }
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
    ret = bmcv_faiss_indexflatIP(handle,
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
    printf("TPU using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    if (BM_SUCCESS != ret) {
        printf("bmcv_faiss_indexflatIP api error\n");
        goto free_mem;
    }
    if (output_dtype == DT_FP32) {
        ret = bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_similarity_fp32)),
                  output_sorted_similarity_global_addr_device);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_d2s output_similarity_fp32 failed!\n");
            goto free_mem;
        }
    } else {
        ret = bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_similarity_fp16)),
                  output_sorted_similarity_global_addr_device);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_d2s output_similarity_fp16 failed!\n");
            goto free_mem;
        }
    }
    ret = bm_memcpy_d2s(handle,
                  bm_mem_get_system_addr(bm_mem_from_system(output_index)),
                  output_sorted_index_global_addr_device);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s output_index failed!\n");
        goto free_mem;
    }
    gettimeofday(&t1, NULL);
    if (is_transpose) {
        matrix_mul_ref_fp32(input_content_vec, db_content_vec_trans, ref_result, query_vecs_num, database_vecs_num, vec_dims);
    } else {
        matrix_mul_ref_fp32(input_content_vec, db_content_vec, ref_result, query_vecs_num, database_vecs_num, vec_dims);
    }
    gettimeofday(&t2, NULL);
    printf("CPU using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    if(output_dtype == DT_FP32) {
        ret = result_compare_fp32(output_similarity_fp32,
            output_index,
            ref_result,
            query_vecs_num,
            database_vecs_num,
            sort_cnt);
        if (ret != BM_SUCCESS) {
            printf("-------------faiss_indexflatIP fp32 compare failed-------\n");
        } else {
            printf("-------------faiss_indexflatIP fp32 compare succeed-----------\n");
        }
    } else {
        ret = result_compare_fp16(output_similarity_fp16,
            output_index,
            ref_result,
            query_vecs_num,
            database_vecs_num,
            sort_cnt);
        if (ret != BM_SUCCESS) {
            printf("-------------faiss_indexflatIP fp16 compare failed-------\n");
        } else {
            printf("-------------faiss_indexflatIP fp16 compare succeed-----------\n");
        }
    }
free_mem:
    bm_free_device(handle, input_data_global_addr_device);
    bm_free_device(handle, db_data_global_addr_device);
    bm_free_device(handle, buffer_global_addr_device);
    bm_free_device(handle, output_sorted_similarity_global_addr_device);
    bm_free_device(handle, output_sorted_index_global_addr_device);
free_mem1:
    free(input_data);
    free(db_data);
    free(output_similarity_fp32);
    for (i = 0; i < query_vecs_num; ++i) {
        free(input_content_vec[i]);
        free(ref_result[i]);
    }
    if (is_transpose) {
        for (i = 0; i < database_vecs_num; ++i) {
            free(db_content_vec[i]);
        }
        for (i = 0; i < vec_dims; ++i) {
            free(db_content_vec_trans[i]);
        }
    } else {
        for (i = 0; i < vec_dims; ++i) {
            free(db_content_vec[i]);
        }
        for (i = 0; i < database_vecs_num; ++i) {
            free(db_content_vec_trans[i]);
        }
    }
    free(input_content_vec);
    free(db_content_vec);
    free(db_content_vec_trans);
    free(output_index);
    free(ref_result);
    return ret;
}

void* test_faiss_indexflatIP(void* args) {
    faiss_indexflatIP_thread_arg_t* faiss_indexflatIP_thread_arg = (faiss_indexflatIP_thread_arg_t*)args;
    int loop = faiss_indexflatIP_thread_arg->loop;
    int database_vecs_num = faiss_indexflatIP_thread_arg->database_vecs_num;
    int query_vecs_num = faiss_indexflatIP_thread_arg->query_vecs_num;
    int sort_cnt = faiss_indexflatIP_thread_arg->sort_cnt;
    int vec_dims = faiss_indexflatIP_thread_arg->vec_dims;
    int is_transpose = faiss_indexflatIP_thread_arg->is_transpose;
    int input_dtype = faiss_indexflatIP_thread_arg->input_dtype;
    int output_dtype = faiss_indexflatIP_thread_arg->output_dtype;
    bm_handle_t handle = faiss_indexflatIP_thread_arg->handle;
    for(int i = 0; i < loop; i++){
        if(loop > 1) {
            sort_cnt = 10;
            query_vecs_num = rand() % 100 + 1;
            database_vecs_num = rand() % 100000 + query_vecs_num + sort_cnt;
            is_transpose = rand() % 2;
            int flag = rand() % 3;
            if (flag == 0) {
                input_dtype = 5;
                output_dtype = (rand() % 2 == 0) ? 3 : 5;
            } else if (flag == 1) {
                input_dtype = 3;
                output_dtype = (rand() % 2 == 0) ? 3 : 5;
            } else {
                input_dtype = 1;
                output_dtype = 9;
            }
        }
        switch (input_dtype) {
            case DT_FP32:
                if (BM_SUCCESS != faiss_indexflatIP_fp32_single_test(handle, vec_dims, query_vecs_num, database_vecs_num, sort_cnt, is_transpose, input_dtype, output_dtype)) {
                    printf("------faiss_indexflatIP_fp32_single_test failed------\n");
                    exit(-1);
                }
                break;
            case DT_FP16:
                if (BM_SUCCESS != faiss_indexflatIP_fp16_single_test(handle, vec_dims, query_vecs_num, database_vecs_num, sort_cnt, is_transpose, input_dtype, output_dtype)) {
                    printf("------faiss_indexflatIP_fp16_single_test failed------\n");
                    exit(-1);
                }
                if (BM_SUCCESS != faiss_indexflatIP_fp16_single_test_u64(handle, vec_dims, query_vecs_num, database_vecs_num, sort_cnt, is_transpose, input_dtype, output_dtype)) {
                    printf("------faiss_indexflatIP_fp16_single_test failed------\n");
                    exit(-1);
                }
                break;
            case DT_INT8:
                if (BM_SUCCESS != faiss_indexflatIP_fix8b_single_test(handle, vec_dims, query_vecs_num, database_vecs_num, sort_cnt, is_transpose, input_dtype, output_dtype)) {
                    printf("------faiss_indexflatIP_int8_single_test failed------\n");
                    exit(-1);
                }
                break;
            default:
                printf("input data_type not support!\n");
                exit(-1);
                break;
        }
    }
    return NULL;
}

int main(int argc, char* args[]) {
    struct timespec tp;
    clock_gettime(0, &tp);
    unsigned int seed = tp.tv_nsec;
    srand(seed);
    printf("random seed = %u\n", seed);
    int thread_num = 1;
    int loop = 1;
    int sort_cnt = rand() % 30 + 1;
    int query_vecs_num = rand() % 50 + 1;
    int database_vecs_num = rand() % 10000 + query_vecs_num + sort_cnt;
    int vec_dims = 512;
    int is_transpose = 1;
    int input_dtype = 3; //1-char 3-fp16 5-fp32 9-int
    int output_dtype = 5;

    if (argc == 2 && atoi(args[1]) == -1) {
        printf("usage: %d\n", argc);
        printf("%s thread_num loop_num database_num query_num sort_count data_dims is_transpose input_dtype output_dtype\n", args[0]);
        return 0;
    }
    if (argc > 1) thread_num = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) database_vecs_num = atoi(args[3]);
    if (argc > 4) query_vecs_num = atoi(args[4]);
    if (argc > 5) sort_cnt = atoi(args[5]);
    if (argc > 6) vec_dims = atoi(args[6]);
    if (argc > 7) is_transpose = atoi(args[7]);
    if (argc > 8) input_dtype = atoi(args[8]);
    if (argc > 9) output_dtype = atoi(args[9]);

    printf("thread_num:        %d\n", thread_num);
    printf("loop:              %d\n", loop);
    bm_status_t ret = BM_SUCCESS;
    bm_handle_t handle;
    ret = bm_dev_request(&handle, 0);
    if (BM_SUCCESS != ret) {
        printf("request dev failed\n");
        return BM_ERR_FAILURE;
    }
    // test for multi-thread
    pthread_t pid[thread_num];
    faiss_indexflatIP_thread_arg_t faiss_indexflatIP_thread_arg[thread_num];
    for (int i = 0; i < thread_num; i++) {
        faiss_indexflatIP_thread_arg[i].loop = loop;
        faiss_indexflatIP_thread_arg[i].database_vecs_num = database_vecs_num;
        faiss_indexflatIP_thread_arg[i].query_vecs_num = query_vecs_num;
        faiss_indexflatIP_thread_arg[i].sort_cnt = sort_cnt;
        faiss_indexflatIP_thread_arg[i].vec_dims = vec_dims;
        faiss_indexflatIP_thread_arg[i].is_transpose = is_transpose;
        faiss_indexflatIP_thread_arg[i].input_dtype = input_dtype;
        faiss_indexflatIP_thread_arg[i].output_dtype = output_dtype;
        faiss_indexflatIP_thread_arg[i].handle = handle;
        if (pthread_create(pid + i, NULL, test_faiss_indexflatIP, faiss_indexflatIP_thread_arg + i) != 0) {
            printf("create thread failed\n");
            return -1;
        }
    }
    for (int i = 0; i < thread_num; i++) {
        int ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            printf("Thread join failed\n");
            exit(-1);
        }
    }
    bm_dev_free(handle);
    return 0;
}
