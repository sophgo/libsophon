#include "bmcv_api_ext.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include "test_misc.h"
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif
#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))
#define COMPARE_EPSILON 1e-2f
#define GEMM_INPUT_NUM 3
#define GEMM_OUTPUT_NUM 1
// /*
//  * GEMM (General Matrix Multiplication)
//  * BMCV: Y = alpha * (L * R) + beta * C
//  *  ——————————————————————————————————————————
//  * |in_dtype\out_dtype|  DT_FP16  |  DT_FP32  |
//  * |——————————————————————————————————————————|
//  * |     DT_FP16      |     √    |     √      |
//  * |——————————————————————————————————————————|
//  * |     DT_FP32      |     ×    |     √      |
//  *  ——————————————————————————————————————————
//  */
typedef unsigned short half;

typedef struct {
    int loop;
    int M;
    int N;
    int K;
    float alpha;
    float beta;
    bool if_A_trans;
    bool if_B_trans;
    bm_handle_t handle;
} cv_gemm_thread_arg_t;

static void gemm_trans(float* src, int row_num, int col_num) {
    int i, j;

    float* dst = (float*)malloc(row_num * col_num * sizeof(float));

    for (i = 0; i < row_num; ++i) {
        for (j = 0; j < col_num; j++) {
            dst[j * row_num + i] = src[i * col_num + j];
        }
    }
    memcpy(src, dst, col_num * row_num * sizeof(float));
    free(dst);
}

static int cpu_gemm(bool if_A_trans, bool if_B_trans, int M, int N, int K, float alpha, float* src_A,
                    int lda, float* src_B, int ldb, float beta, float* src_C) {
    float* A = (float*)malloc(M * K * sizeof(float));
    float* B = (float*)malloc(N * K * sizeof(float));
    float* C = (float*)malloc(M * N * sizeof(float));
    int i, j, k;

    memcpy(A, src_A, sizeof(float) * M * K);
    memcpy(B, src_B, sizeof(float) * N * K);
    memset(C, 0.f, sizeof(float) * M * N);

    if (if_A_trans) {
        gemm_trans(A, K, lda);
    }
    if (if_B_trans) {
        gemm_trans(B, N, ldb);
    }

    for (i = 0; i < M; ++i) {
        for (j = 0; j < N; ++j) {
            for (k = 0; k < K; ++k) {
                C[i * N + j] += alpha * A[i * K + k] * B[k * N + j];
            }
        }
    }

    for (i = 0; i < M * N; ++i) {
        C[i] = C[i] + src_C[i] * beta;
    }

    memcpy(src_C, C, sizeof(float) * M * N);

    free(A);
    free(B);
    free(C);

    return 0;
}

static int cmp_result(float* tpu_res, float* cpu_res, int len) {
    int i;
    const float ZERO_THRESHOLD = 5e-3f;

    for (i = 0; i < len; ++i) {
        float tpu_abs = fabs(tpu_res[i]);
        float cpu_abs = fabs(cpu_res[i]);

        if (tpu_abs < ZERO_THRESHOLD && cpu_abs < ZERO_THRESHOLD) {
            continue;
        }

        if (tpu_abs > 1.f && cpu_abs > 1.f) {
            if (fabs(tpu_abs - cpu_abs) > COMPARE_EPSILON) {
                printf("index = %d, tpu[i] = %.10f, cpu[i] = %.10f\n",
                       i, tpu_res[i], cpu_res[i]);
                return -1;
            }
        }
        else {
            float max_val = fmax(tpu_abs, cpu_abs);
            if (max_val < ZERO_THRESHOLD) {
                continue;
            }

            float relative_diff = fabs(tpu_abs - cpu_abs) / max_val;
            if (relative_diff > COMPARE_EPSILON) {
                printf("index = %d, tpu[i] = %.10f, cpu[i] = %.10f, relative_diff = %.10f\n",
                       i, tpu_res[i], cpu_res[i], relative_diff);
                return -1;
            }
        }
    }

    return 0;
}

static float halfToFloat(half value) {
    unsigned int sign = (value >> 15) & 0x0001;
    unsigned int exponent = (value >> 10) & 0x001F;
    unsigned int mantissa = value & 0x03FF;
    unsigned int result = 0;
    float res;

    if (exponent == 0x1F) {
        result = (sign << 31) | 0x7F800000 | (mantissa << 13);
    } else {
        exponent = exponent - 15 + 127;
        mantissa = mantissa << 13;
        result = (sign << 31) | (exponent << 23) | mantissa;
    }

    memcpy(&res, &result, sizeof(result));

    return res;
}

static uint16_t fp32_to_fp16(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(value));
    uint16_t sign = (bits >> 31) & 0x1;
    int16_t exponent = ((bits >> 23) & 0xff) - 127;
    uint32_t mantissa = bits & 0x7fffff;

    if (exponent > 15) {
        return 0x7c00 | (sign << 15);
    } else if (exponent < -14 || (exponent == -14 && mantissa < 0x400)) {
        return sign << 15;
    } else if (exponent <= -25) {
        return sign << 15 | 0x200;
    }

    exponent = exponent + 15;
    if (exponent < 0) {
        exponent = 0;
    } else if (exponent > 31) {
        exponent = 31;
    }

    mantissa = mantissa >> (23 - 10);
    return (sign << 15) | (exponent << 10) | mantissa;
}

// Calculate the module length of the vector
float norm(float* vec, int size) {
    float sum = 0.0;
    int i;

    for (i = 0; i < size; ++i) {
        sum += vec[i] * vec[i];
    }
    return sqrt(sum);
}

// Calculate the dot product of vectors
float dot(float* vec1, float* vec2, int size) {
    float sum = 0.0;
    int i;

    for (i = 0; i < size; ++i) {
        sum += vec1[i] * vec2[i];
    }
    return sum;
}

/* Calculate cosine similarity */
float cosine_similarity(float* vec1, float* vec2, int size) {
    float dot_val = dot(vec1, vec2, size);
    float norm1 = norm(vec1, size);
    float norm2 = norm(vec2, size);

    return dot_val / (norm1 * norm2);
}

static int assign_values_to_matrix(float* matrix, int size) {
    int i;

    if (matrix == NULL || size <= 0) {
        printf("the assign_values_to_matrix func error!\n");
        return -1;
    }

    for (i = 0; i < size; ++i) {
        matrix[i] = (rand() % 100) * 0.01f;
    }

    return 0;
}

static int tpu_gemm_ext(bm_handle_t handle, float* A, float* B, float* C, float* tpu_res,
                        bm_image_data_format_ext in_dtype, bm_image_data_format_ext out_dtype,
                        int M, int N, int K, float alpha, float beta, bool is_A_trans, bool is_B_trans) {
    if (in_dtype == DATA_TYPE_EXT_FP16 && is_A_trans && M > 64) {
        printf("Error! It only support M <= 64 when A is trans and input_dtype is FP16\n");
        return -1;
    }

    unsigned short* A_fp16 = (unsigned short*)malloc(M * K * sizeof(unsigned short));
    unsigned short* B_fp16 = (unsigned short*)malloc(N * K * sizeof(unsigned short));
    unsigned short* C_fp16 = (unsigned short*)malloc(M * N * sizeof(unsigned short));
    unsigned short* Y_fp16 = (unsigned short*)malloc(M * N * sizeof(unsigned short));
    bm_device_mem_t input_dev_buffer[GEMM_INPUT_NUM];
    bm_device_mem_t output_dev_buffer[GEMM_OUTPUT_NUM];
    bm_status_t ret = BM_SUCCESS;
    struct timeval t1, t2;
    int i;

    if (in_dtype == DATA_TYPE_EXT_FLOAT32) {
        ret = bm_malloc_device_byte(handle, &input_dev_buffer[0], M * K * sizeof(float));
        if (ret != BM_SUCCESS) {
            printf("bm_malloc_device_byte failed!\n");
            goto err0;
        }
        ret = bm_malloc_device_byte(handle, &input_dev_buffer[1], N * K * sizeof(float));
        if (ret != BM_SUCCESS) {
            printf("bm_malloc_device_byte failed!\n");
            goto err1;
        }
        ret = bm_malloc_device_byte(handle, &input_dev_buffer[2], M * N * sizeof(float));
        if (ret != BM_SUCCESS) {
            printf("bm_malloc_device_byte failed!\n");
            goto err2;
        }
        ret = bm_memcpy_s2d(handle, input_dev_buffer[0], (void*)A);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_s2d failed!\n");
            goto err3;
        }
        ret = bm_memcpy_s2d(handle, input_dev_buffer[1], (void*)B);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_s2d failed!\n");
            goto err3;
        }
        ret = bm_memcpy_s2d(handle, input_dev_buffer[2], (void*)C);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_s2d failed!\n");
            goto err3;
        }
    } else if (in_dtype == DATA_TYPE_EXT_FP16) {
        ret = bm_malloc_device_byte(handle, &input_dev_buffer[0], M * K * sizeof(unsigned short));
        if (ret != BM_SUCCESS) {
            printf("bm_malloc_device_byte failed!\n");
            goto err0;
        }
        ret = bm_malloc_device_byte(handle, &input_dev_buffer[1], N * K * sizeof(unsigned short));
        if (ret != BM_SUCCESS) {
            printf("bm_malloc_device_byte failed!\n");
            goto err1;
        }
        ret = bm_malloc_device_byte(handle, &input_dev_buffer[2], M * N * sizeof(unsigned short));
        if (ret != BM_SUCCESS) {
            printf("bm_malloc_device_byte failed!\n");
            goto err2;
        }
        // convert to fp16 data
        for (i = 0; i < M * K; ++i) {
            A_fp16[i] = fp32_to_fp16(A[i]);
        }
        for (i = 0; i < N * K; ++i) {
            B_fp16[i] = fp32_to_fp16(B[i]);
        }
        for (i = 0; i < M * N; ++i) {
            C_fp16[i] = fp32_to_fp16(C[i]);
        }

        ret = bm_memcpy_s2d(handle, input_dev_buffer[0], (void*)A_fp16);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_s2d failed!\n");
            goto err3;
        }
        ret = bm_memcpy_s2d(handle, input_dev_buffer[1], (void*)B_fp16);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_s2d failed!\n");
            goto err3;
        }
        ret = bm_memcpy_s2d(handle, input_dev_buffer[2], (void*)C_fp16);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_s2d failed!\n");
            goto err3;
        }
    }

    if (out_dtype == DATA_TYPE_EXT_FLOAT32) {
        ret = bm_malloc_device_byte(handle, &output_dev_buffer[0], M * N * sizeof(float));
        if (ret != BM_SUCCESS) {
            printf("bm_malloc_device_byte failed!\n");
            goto err3;
        }
    } else if (out_dtype == DATA_TYPE_EXT_FP16) {
        ret = bm_malloc_device_byte(handle, &output_dev_buffer[0], M * N * sizeof(unsigned short));
        if (ret != BM_SUCCESS) {
            printf("bm_malloc_device_byte failed!\n");
            goto err3;
        }
    }

    gettimeofday_(&t1);
    ret = bmcv_gemm_ext(handle, is_A_trans, is_B_trans, M, N, K, alpha, input_dev_buffer[0],
                        input_dev_buffer[1], beta, input_dev_buffer[2], output_dev_buffer[0],
                        in_dtype, out_dtype);
    if (ret != BM_SUCCESS) {
        printf("%s: bmcv_gemm_ext failed!\n", __func__);
        goto err4;
    }
    gettimeofday_(&t2);
    printf("Gemm_EXT TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));

    if (out_dtype == DATA_TYPE_EXT_FLOAT32) {
        ret = bm_memcpy_d2s(handle, (void*)tpu_res, output_dev_buffer[0]);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_d2s failed!\n");
            goto err4;
        }
    } else if (out_dtype == DATA_TYPE_EXT_FP16) {
        ret = bm_memcpy_d2s(handle, (void *)Y_fp16, output_dev_buffer[0]);
        if (ret != BM_SUCCESS) {
            printf("bm_memcpy_d2s failed!\n");
            goto err4;
        }
        for (i = 0; i < M * N; ++i) {
            tpu_res[i] = halfToFloat(Y_fp16[i]);
        }
    }

err4:
    bm_free_device(handle, output_dev_buffer[0]);
err3:
    bm_free_device(handle, input_dev_buffer[2]);
err2:
    bm_free_device(handle, input_dev_buffer[1]);
err1:
    bm_free_device(handle, input_dev_buffer[0]);
err0:
    free(A_fp16);
    free(B_fp16);
    free(C_fp16);
    free(Y_fp16);
    return ret;
}

static int cmp_gemm_ext(float* tpu_res, float* cpu_res, int len) {
    float sim;
    int ret = 0;

    sim = cosine_similarity(tpu_res, cpu_res, len);

    if (fabs(sim - 1) > COMPARE_EPSILON) {
        ret = cmp_result(tpu_res, cpu_res, len);
        if (ret) {
            printf("cmp_gemm_ext cmp failed!\n");
            return ret;
        }
    }

    return ret;
}

static int tpu_gemm(bm_handle_t handle, bool if_A_trans, bool if_B_trans, int M, int N, int K, float alpha,
                    float* src_A, int lda, float* src_B, int ldb, float beta, float* src_C) {
    struct timeval t1, t2;
    bm_status_t ret = BM_SUCCESS;

    gettimeofday_(&t1);
    ret= bmcv_gemm(handle, if_A_trans, if_B_trans, M, N, K, alpha, bm_mem_from_system((void *)src_A),
                    lda, bm_mem_from_system((void *)src_B), ldb, beta,
                    bm_mem_from_system((void *)src_C), N);
    if(ret != BM_SUCCESS) {
        printf("the bmcv_gemm failed!\n");
        return -1;
    }
    gettimeofday_(&t2);
    printf("Gemm TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    return 0;
}

static int test_gemm(bm_handle_t handle, int M, int N, int K, float alpha,
                    float beta, bool is_A_trans, bool is_B_trans) {
    printf("%s: M = %d, N = %d, K = %d, alpha = %f, beta = %f, is_A_trans = %d, is_B_trans = %d\n",
           __func__, M, N, K, alpha, beta, is_A_trans, is_B_trans);

    float* A = (float*)malloc(M * K * sizeof(float));
    float* B = (float*)malloc(N * K * sizeof(float));
    float* tpu_C = (float*)malloc(M * N * sizeof(float));
    float* cpu_C = (float*)malloc(M * N * sizeof(float));
    int ret = 0;
    int lda = is_A_trans ? M : K;
    int ldb = is_B_trans ? K : N;
    struct timeval t1, t2;

    ret = assign_values_to_matrix(A, M * K);
    if (ret) {
        printf("assign_values_to_matrix failed!\n");
        goto exit;
    }
    ret = assign_values_to_matrix(B, N * K);
    if (ret) {
        printf("assign_values_to_matrix failed!\n");
        goto exit;
    }
    ret = assign_values_to_matrix(tpu_C, M * N);
    if (ret) {
        printf("assign_values_to_matrix failed!\n");
        goto exit;
    }
    memcpy(cpu_C, tpu_C, sizeof(float) * M * N);

    gettimeofday_(&t1);
    ret = cpu_gemm(is_A_trans, is_B_trans, M, N, K, alpha, A, lda, B, ldb, beta, cpu_C);
    gettimeofday_(&t2);
    printf("Gemm CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    if (ret) {
        printf("%s: cpu_gemm failed!\n", __func__);
        goto exit;
    }

    ret = tpu_gemm(handle, is_A_trans, is_B_trans, M, N, K, alpha, A, lda, B, ldb, beta, tpu_C);
    if (ret) {
        printf("%s: tpu_gemm failed!\n", __func__);
        goto exit;
    }
    ret = cmp_result(tpu_C, cpu_C, M * N);
    if (ret) {
        printf("%s: cmp_result failed!\n", __func__);
        goto exit;
    }
exit:
    free(A);
    free(B);
    free(cpu_C);
    free(tpu_C);
    return ret;
}

static int test_gemm_ext(bm_handle_t handle, int M, int N, int K, float alpha,
                        float beta, bool is_A_trans, bool is_B_trans) {
    printf("%s: M = %d, N = %d, K = %d, alpha = %f, beta = %f, is_A_trans = %d, is_B_trans = %d\n",
           __func__, M, N, K, alpha, beta, is_A_trans, is_B_trans);

    float* A = (float*)malloc(M * K * sizeof(float));
    float* B = (float*)malloc(N * K * sizeof(float));
    float* C = (float*)malloc(M * N * sizeof(float));
    float* cpu_C = (float*)malloc(M * N * sizeof(float));
    float* tpu_C = (float*)malloc(M * N * sizeof(float));
    int ret = 0;
    int lda = is_A_trans ? M : K;
    int ldb = is_B_trans ? K : N;
    bm_image_data_format_ext in_dtype, out_dtype;
    struct timeval t1, t2;

    ret = assign_values_to_matrix(A, M * K);
    if (ret) {
        printf("assign_values_to_matrix failed!\n");
        goto exit;
    }
    ret = assign_values_to_matrix(B, N * K);
    if (ret) {
        printf("assign_values_to_matrix failed!\n");
        goto exit;
    }
    ret = assign_values_to_matrix(C, M * N);
    if (ret) {
        printf("assign_values_to_matrix failed!\n");
        goto exit;
    }
    memcpy(cpu_C, C, sizeof(float) * M * N);
    memset(tpu_C, 0.f, sizeof(float) * M * N);

    gettimeofday_(&t1);
    ret = cpu_gemm(is_A_trans, is_B_trans, M, N, K, alpha, A, lda, B, ldb, beta, cpu_C);
    gettimeofday_(&t2);
    printf("Gemm_EXT CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    if (ret) {
        printf("%s: cpu_gemm failed!\n", __func__);
        goto exit;
    }

    in_dtype = DATA_TYPE_EXT_FLOAT32;
    out_dtype = DATA_TYPE_EXT_FLOAT32;
    memset(tpu_C, 0.f, sizeof(float) * M * N);
    ret = tpu_gemm_ext(handle, A, B, C, tpu_C, in_dtype, out_dtype, M, N, K, alpha, beta, is_A_trans, is_B_trans);
    if (ret) {
        printf("%s: tpu_gemm_ext failed!\n", __func__);
        goto exit;
    }

    ret = cmp_gemm_ext(tpu_C, cpu_C, M * N);
    if (ret) {
        printf("%s: cmp_gemm_ext in(fp32) -> out(fp32) cmp failed!\n", __func__);
        goto exit;
    }

    if (!(is_A_trans && M > 64)) {
        in_dtype = DATA_TYPE_EXT_FP16;
        out_dtype = DATA_TYPE_EXT_FP16;
        memset(tpu_C, 0.f, sizeof(float) * M * N);
        ret = tpu_gemm_ext(handle, A, B, C, tpu_C, in_dtype, out_dtype, M, N, K, alpha, beta, is_A_trans, is_B_trans);
        if (ret) {
            printf("%s: tpu_gemm_ext failed!\n", __func__);
            goto exit;
        }

        ret = cmp_gemm_ext(tpu_C, cpu_C, M * N);
        if (ret) {
            printf("%s: cmp_gemm_ext in(fp16) -> out(fp16) cmp failed!\n", __func__);
            goto exit;
        }

        in_dtype = DATA_TYPE_EXT_FP16;
        out_dtype = DATA_TYPE_EXT_FLOAT32;
        memset(tpu_C, 0.f, sizeof(float) * M * N);
        ret = tpu_gemm_ext(handle, A, B, C, tpu_C, in_dtype, out_dtype, M, N, K, alpha, beta, is_A_trans, is_B_trans);
        if (ret) {
            printf("%s: tpu_gemm_ext failed!\n", __func__);
            goto exit;
        }

        ret = cmp_gemm_ext(tpu_C, cpu_C, M * N);
        if (ret) {
            printf("%s: cmp_gemm_ext in(fp16) -> out(fp32) cmp failed!\n", __func__);
            goto exit;
        }
    }

exit:
    free(A);
    free(B);
    free(C);
    free(tpu_C);
    free(cpu_C);
    return ret;
}

void* test_gemm_all(void* args) {
    cv_gemm_thread_arg_t* cv_gemm_thread_arg = (cv_gemm_thread_arg_t*)args;

    int loop = cv_gemm_thread_arg->loop;
    int M = cv_gemm_thread_arg->M;
    int N = cv_gemm_thread_arg->N;
    int K = cv_gemm_thread_arg->K;
    float alpha = cv_gemm_thread_arg->alpha;
    float beta = cv_gemm_thread_arg->beta;
    bool if_A_trans = cv_gemm_thread_arg->if_A_trans;
    bool if_B_trans = cv_gemm_thread_arg->if_B_trans;
    bm_handle_t handle = cv_gemm_thread_arg->handle;
    int i = 0;
    int ret = 0;

    for (i = 0; i < loop; ++i) {
        if(loop > 1) {
            M = 1 + rand() % 800;
            N = 1 + rand() % 800;
            K = 1 + rand() % 800;
            int rand_sign_a = (rand() % 2 == 0) ? 1 : -1;
            int rand_sign_b = (rand() % 2 == 0) ? 1 : -1;
            alpha = rand_sign_a * (rand() % 100) * 0.05;
            beta  = rand_sign_b * (rand() % 100) * 0.05;
            if_A_trans = rand() % 2;
            if_B_trans = rand () % 2;
            if (if_A_trans) {
                if_B_trans = true;
            }
        }
        ret = test_gemm(handle, M, N, K, alpha, beta, if_A_trans, if_B_trans);
        if (ret) {
            printf("------Test Gemm Failed!------\n");
            exit(-1);
        }
        ret = test_gemm_ext(handle, M, N, K, alpha, beta, if_A_trans, if_B_trans);
        if (ret) {
            printf("------Test Gemm_EXT Failed!------\n");
            exit(-1);
        }
    }
    printf("------Test Gemm Successed!------\n");
    return (void*)0;
}

static int parameters_check(bool is_A_trans, bool is_B_trans) {
    int error = 0;

    if (is_A_trans && !is_B_trans) {
        printf("GEMM Can not support L_trans and R_not_trans!\r\n");
        error = -1;
    }

    return error;
}

int main(int argc, char* args[]) {
    struct timespec tp;
    clock_gettime_(0, &tp);
    srand(tp.tv_nsec);
    const int MAX_THREADS = 32;
    int thread_num = 1;
    int loop = 1;
    int M = 1 + rand() % 800;
    int N = 1 + rand() % 800;
    int K = 1 + rand() % 800;
    int rand_sign_a = (rand() % 2 == 0) ? 1 : -1;
    int rand_sign_b = (rand() % 2 == 0) ? 1 : -1;
    float alpha = rand_sign_a * (rand() % 100) * 0.05;
    float beta  = rand_sign_b * (rand() % 100) * 0.05;
    bool if_A_trans = rand() % 2;
    bool if_B_trans = rand () % 2;
    int ret = 0;
    bm_handle_t handle;
    int i;

    if (if_A_trans) {
        if_B_trans = true;
    }

    ret = bm_dev_request(&handle, 0);
    if (ret) {
        printf("bm_dev_request failed. ret = %d\n", ret);
        return ret;
    }

    if (argc == 2 && atoi(args[1]) == -1) {
        printf("%s thread loop M N K alpha beta if_A_trans if_B_trans\n", args[0]);
        printf("example:\n");
        printf("%s \n", args[0]);
        printf("%s 2\n", args[0]);
        printf("%s 1 3\n", args[0]);
        printf("%s 1 1 800 800 800 -1.32f 2.73f 0 0\n", args[0]);
        return 0;
    }

    if (argc > 1) {
        thread_num = atoi(args[1]);
        if (thread_num > MAX_THREADS) {
            printf("Warning: thread_num %d exceeds maximum %d, using %d instead\n", thread_num, MAX_THREADS, MAX_THREADS);
            thread_num = MAX_THREADS;
        }
    }
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) M = atoi(args[3]);
    if (argc > 4) N = atoi(args[4]);
    if (argc > 5) K = atoi(args[5]);
    if (argc > 6) alpha = atof(args[6]);
    if (argc > 7) beta = atof(args[7]);
    if (argc > 8) if_A_trans = atoi(args[8]);
    if (argc > 9) if_B_trans = atoi(args[9]);

    ret = parameters_check(if_A_trans, if_B_trans);
    if (ret) {
        printf("Parameters Failed! \n");
        return ret;
    }

    pthread_t pid[MAX_THREADS];
    cv_gemm_thread_arg_t cv_gemm_thread_arg[MAX_THREADS];
    for (i = 0; i < thread_num; i++) {
        cv_gemm_thread_arg[i].loop = loop;
        cv_gemm_thread_arg[i].M = M;
        cv_gemm_thread_arg[i].N = N;
        cv_gemm_thread_arg[i].K = K;
        cv_gemm_thread_arg[i].alpha = alpha;
        cv_gemm_thread_arg[i].beta = beta;
        cv_gemm_thread_arg[i].if_A_trans = if_A_trans;
        cv_gemm_thread_arg[i].if_B_trans = if_B_trans;
        cv_gemm_thread_arg[i].handle = handle;
        if (pthread_create(&pid[i], NULL, test_gemm_all, &cv_gemm_thread_arg[i]) != 0) {
            printf("create thread failed\n");
            return -1;
        }
    }

    for (i = 0; i < thread_num; i++) {
        ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            printf("Thread join failed\n");
            exit(-1);
        }
    }

    bm_dev_free(handle);
    return ret;
}
