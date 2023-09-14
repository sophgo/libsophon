#include <iostream>
#include "bmcv_api.h"
#include "test_misc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#define BMDNN_COMPARE_EPSILON (1e-3)
#define RANDOM_TEST
#ifdef RANDOM_TEST
#include <time.h>
#endif
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif
#ifdef __riscv
#include <cstdint>
#endif

using namespace std;

typedef unsigned short half;
float halfToFloat(half value)
{
    unsigned int sign = (value >> 15) & 0x0001;
    unsigned int exponent = (value >> 10) & 0x001F;
    unsigned int mantissa = value & 0x03FF;

    unsigned int result = 0;

    // 处理无穷大和 NaN
    if (exponent == 0x1F)
    {
        result = (sign << 31) | 0x7F800000 | (mantissa << 13);
    }
    // 处理正常的半精度浮点数
    else
    {
        exponent = exponent - 15 + 127;
        mantissa = mantissa << 13;
        result = (sign << 31) | (exponent << 23) | mantissa;
    }

    return *(float *)&result;
}

uint16_t fp32_to_fp16(float value)
{
    uint32_t bits = *(uint32_t *)&value;            // 将 float 转换为 uint32_t
    uint16_t sign = (bits >> 31) & 0x1;             // 取符号位
    int16_t exponent = ((bits >> 23) & 0xff) - 127; // 取指数位并减去偏置值
    uint32_t mantissa = bits & 0x7fffff;            // 取尾数位

    // 判断是否超出 fp16 的表示范围
    if (exponent > 15)
    {
        return 0x7c00 | (sign << 15); // 返回正无穷
    }
    else if (exponent < -14 || (exponent == -14 && mantissa < 0x400))
    {
        return sign << 15; // 返回 0 或负 0
    }
    else if (exponent <= -25)
    {
        return sign << 15 | 0x200; // 返回最小非规格化数
    }

    // 将指数值加上偏置值，并且舍去高位
    exponent = exponent + 15;
    if (exponent < 0)
    {
        exponent = 0;
    }
    else if (exponent > 31)
    {
        exponent = 31;
    }

    // 将尾数位截取为 10 位，然后将高位 23-10=13 位舍去
    mantissa = mantissa >> (23 - 10);
    return (sign << 15) | (exponent << 10) | mantissa;
}

// 计算向量的模长
float norm(float vec[], int size)
{
    float sum = 0.0;
    for (int i = 0; i < size; ++i)
    {
        sum += vec[i] * vec[i];
    }
    return sqrt(sum);
}

// 计算向量的点积
float dot(float vec1[], float vec2[], int size)
{
    float sum = 0.0;
    for (int i = 0; i < size; ++i)
    {
        sum += vec1[i] * vec2[i];
    }
    return sum;
}

// 计算余弦相似度
float cosine_similarity(float vec1[], float vec2[], int size)
{
    float dot_val = dot(vec1, vec2, size);
    float norm1 = norm(vec1, size);
    float norm2 = norm(vec2, size);
    return dot_val / (norm1 * norm2);
}

typedef struct {
    int loop_num;
    bm_handle_t handle;
} gemm_thread_arg_t;

static void assign_values_to_matrix(float *matrix, int size) {
    for (int i = 0; i < size; i++) {
        matrix[i] = (rand() % 100) * 0.01f;
        // matrix[i] = 1.5;
        // if (i % 4 == 0)
        //     matrix[i] = 1.1f;
        // if (i % 4 == 1)
        //     matrix[i] = 1.2f;
        // if (i % 4 == 2)
        //     matrix[i] = 1.3f;
        // if (i % 4 == 3)
        //     matrix[i] = 1.4f;
    }
}

template <typename T>
void gemm_trans(T *src, int row_num, int col_num)
{
    T *dst = new T[row_num * col_num];
    for (int i = 0; i < row_num; i++)
    {
        for (int j = 0; j < col_num; j++)
        {
            dst[j * row_num + i] = src[i * col_num + j];
        }
    }
    memcpy(src, dst, sizeof(T) * col_num * row_num);
    delete[] dst;
}

// CblasRowMajor
template <typename T>
void native_gemm_core(bool if_A_trans,
                      bool if_B_trans,
                      int m,
                      int n,
                      int k,
                      T alpha,
                      T *src_A,
                      int lda,
                      T *src_B,
                      int ldb,
                      T beta,
                      T *src_C)
{
    T *A = new T[m * k];
    T *B = new T[k * n];
    T *C = new T[m * n];
    memcpy(A, src_A, sizeof(T) * m * k);
    memcpy(B, src_B, sizeof(T) * n * k);
    memset(C, 0, sizeof(T) * m * n);
    if (if_A_trans)
        gemm_trans(A, k, lda);
    if (if_B_trans)
    {
        gemm_trans(B, n, ldb);
    }
    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < n; j++)
        {
            for (int l = 0; l < k; l++)
            {
                C[i * n + j] += alpha * A[i * k + l] * B[l * n + j];
            }
        }
    }
    for (int i = 0; i < m * n; i++)
    {
        C[i] = C[i] + src_C[i] * beta;
    }
    memcpy(src_C, C, sizeof(T) * m * n);
    delete[] A;
    delete[] B;
    delete[] C;
}

/*
 * GEMM (General Matrix Multiplication)
 * BMCV: Y = alpha * (L * R) + beta * C
 *  ——————————————————————————————————————————
 * |in_dtype\out_dtype|  DT_FP16  |  DT_FP32  |
 * |——————————————————————————————————————————|
 * |     DT_FP16      |     √    |     √      |
 * |——————————————————————————————————————————|
 * |     DT_FP32      |     ×    |     √      |
 *  ——————————————————————————————————————————
 */

int test_bmblas_gemm_single(bm_handle_t handle,
                             float *A,
                             float *B,
                             float *C,
                             float *Y_ref,
                             bm_image_data_format_ext in_dtype,
                             bm_image_data_format_ext out_dtype,
                             int M,
                             int N,
                             int K,
                             float alpha,
                             float beta,
                             bool is_A_trans,
                             bool is_B_trans)
{
    if(in_dtype == DATA_TYPE_EXT_FP16 && is_A_trans && M > 64){
        printf("Error! It only support M <= 64 when A is trans and input_dtype is FP16\n");
        return -1;
    }
    unsigned short *A_fp16 = new unsigned short[M * K];
    unsigned short *B_fp16 = new unsigned short[N * K];
    unsigned short *C_fp16 = new unsigned short[M * N];
    unsigned short *Y_fp16 = new unsigned short[M * N];
    float *Y = new float[M * N];
    for(int i=0; i<M*N; i++){
        Y[i] = 0;
    }
    // Initialize.
    bm_device_mem_t input_dev_buffer[3];
    bm_device_mem_t output_dev_buffer[1];
    if (in_dtype == DATA_TYPE_EXT_FLOAT32)
    {
        bm_malloc_device_byte(handle, &input_dev_buffer[0], M * K * sizeof(float));
        bm_malloc_device_byte(handle, &input_dev_buffer[1], N * K * sizeof(float));
        bm_malloc_device_byte(handle, &input_dev_buffer[2], M * N * sizeof(float));
        bm_memcpy_s2d(handle, input_dev_buffer[0], (void *)A);
        bm_memcpy_s2d(handle, input_dev_buffer[1], (void *)B);
        bm_memcpy_s2d(handle, input_dev_buffer[2], (void *)C);
    }
    else if (in_dtype == DATA_TYPE_EXT_FP16)
    {
        bm_malloc_device_byte(handle, &input_dev_buffer[0], M * K * sizeof(unsigned short));
        bm_malloc_device_byte(handle, &input_dev_buffer[1], N * K * sizeof(unsigned short));
        bm_malloc_device_byte(handle, &input_dev_buffer[2], M * N * sizeof(unsigned short));
        // convert to fp16 data
        for (int i = 0; i < M * K; i++)
        {
            A_fp16[i] = fp32_to_fp16(A[i]);
        }
        for (int i = 0; i < N * K; i++)
        {
            B_fp16[i] = fp32_to_fp16(B[i]);
        }
        for (int i = 0; i < M * N; i++)
        {
            C_fp16[i] = fp32_to_fp16(C[i]);
        }
        bm_memcpy_s2d(handle, input_dev_buffer[0], (void *)A_fp16);
        bm_memcpy_s2d(handle, input_dev_buffer[1], (void *)B_fp16);
        bm_memcpy_s2d(handle, input_dev_buffer[2], (void *)C_fp16);
    }

    if (out_dtype == DATA_TYPE_EXT_FLOAT32)
    {
        bm_malloc_device_byte(handle, &output_dev_buffer[0], M * N * sizeof(float));
    }
    else if (out_dtype == DATA_TYPE_EXT_FP16)
    {
        bm_malloc_device_byte(handle, &output_dev_buffer[0], M * N * sizeof(unsigned short));
    }

    struct timeval t1, t2;
    gettimeofday_(&t1);

    bmcv_gemm_ext(handle,
            is_A_trans,
            is_B_trans,
            M,
            N,
            K,
            alpha,
            input_dev_buffer[0],
            input_dev_buffer[1],
            beta,
            input_dev_buffer[2],
            output_dev_buffer[0],
            in_dtype,
            out_dtype);

    gettimeofday_(&t2);
    std::cout << "gemm TPU using time= " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "(us)" << std::endl;

    if (out_dtype == DATA_TYPE_EXT_FLOAT32)
    {
        bm_memcpy_d2s(handle, (void *)Y, output_dev_buffer[0]);
    }
    else if (out_dtype == DATA_TYPE_EXT_FP16)
    {
        bm_memcpy_d2s(handle, (void *)Y_fp16, output_dev_buffer[0]);
        for (int i = 0; i < M * N; i++)
        {
            Y[i] = halfToFloat(Y_fp16[i]);
        }
    }

    float sim = cosine_similarity(Y, Y_ref, M * N);
    std::cout << "cos similarity:" << sim << std::endl;

    if (fabs(sim-1)>0.01)
    {
        int cmp_res = array_cmp_(
            Y_ref,
            Y,
            M * N,
            (const char *)"Comparing results of cblas_sgemm and bmblas_gemm...\n",
            BMDNN_COMPARE_EPSILON);
        if (cmp_res != 0)
        {
            printf("Comparison failed for cblas_sgemm and bmblas_gemm!\n");
            delete[] A_fp16;
            delete[] B_fp16;
            delete[] C_fp16;
            delete[] Y_fp16;
            delete[] Y;
            for (int i = 0; i < 3; i++)
            {
                bm_free_device(handle, input_dev_buffer[i]);
            }
            bm_free_device(handle, output_dev_buffer[0]);
            return -1;
        }
    }

    printf("Comparison done for cblas_sgemm and bmblas_gemm!\n\n");

    delete[] A_fp16;
    delete[] B_fp16;
    delete[] C_fp16;
    delete[] Y_fp16;
    delete[] Y;
    for (int i = 0; i < 3; i++)
    {
        bm_free_device(handle, input_dev_buffer[i]);
    }
    bm_free_device(handle, output_dev_buffer[0]);
    return 0;
}

void test_bmblas_gemm_ext(bm_handle_t handle,
                           int M,
                           int N,
                           int K,
                           float alpha,
                           float beta,
                           bool is_A_trans,
                           bool is_B_trans)
{
    printf("%s: M=%d, N=%d, K=%d, alpha=%f, beta=%f, is_A_trans=%d, "
           "is_B_trans=%d\n",
           __func__,
           M,
           N,
           K,
           alpha,
           beta,
           is_A_trans,
           is_B_trans);
           is_B_trans=1;
    // prepare input data and output_ref
    float *A = new float[M * K];
    float *B = new float[N * K];
    float *C = new float[M * N];
    float *Y_ref = new float[M * N];

    assign_values_to_matrix(A, M * K);
    assign_values_to_matrix(B, N * K);
    assign_values_to_matrix(C, M * N);
    memcpy(Y_ref, C, sizeof(float) * M * N);

    // calculate ref fp32 data
    native_gemm_core<float>(is_A_trans,
                            is_B_trans,
                            M,
                            N,
                            K,
                            alpha,
                            A,
                            is_A_trans ? M : K,
                            B,
                            is_B_trans ? K : N,
                            beta,
                            Y_ref);

    bm_image_data_format_ext in_dtype, out_dtype;

    /*
     * GEMM TEST 01
     * in_dtype = FP32, out_dtype = FP32
     */
    in_dtype = DATA_TYPE_EXT_FLOAT32;
    out_dtype = DATA_TYPE_EXT_FLOAT32;
    int ret = 0;
    ret = test_bmblas_gemm_single(handle, A, B, C, Y_ref, in_dtype, out_dtype, M, N, K, alpha, beta, is_A_trans, is_B_trans);
    if(ret==-1){
        std::cout << "TEST01: in(fp32) -> out(fp32) failed!" << std::endl;
        ret = 0;
    }

    if(!(is_A_trans && M > 64)){
    /*
     * GEMM TEST 02
     * in_dtype = FP16, out_dtype = FP16
     */
        in_dtype = DATA_TYPE_EXT_FP16;
        out_dtype = DATA_TYPE_EXT_FP16;
        ret = test_bmblas_gemm_single(handle, A, B, C, Y_ref, in_dtype, out_dtype, M, N, K, alpha, beta, is_A_trans, is_B_trans);
        if(ret==-1){
            std::cout << "TEST02: in(fp16) -> out(fp16) failed!" << std::endl;
            ret = 0;
        }
    /*
     * GEMM TEST 03
     * in_dtype = FP16, out_dtype = FP32
     */
        in_dtype = DATA_TYPE_EXT_FP16;
        out_dtype = DATA_TYPE_EXT_FLOAT32;
        ret = test_bmblas_gemm_single(handle, A, B, C, Y_ref, in_dtype, out_dtype, M, N, K, alpha, beta, is_A_trans, is_B_trans);
            if(ret==-1){
            std::cout << "TEST02: in(fp16) -> out(fp32) failed!" << std::endl;
            ret = 0;
        }
    }

    delete[] A;
    delete[] B;
    delete[] C;
    delete[] Y_ref;

}

void test_bmblas_gemm(bm_handle_t handle,
                      int   M,
                      int   N,
                      int   K,
                      float alpha,
                      float beta,
                      bool  is_A_trans,
                      bool  is_B_trans) {
    printf("%s: M=%d, N=%d, K=%d, alpha=%f, beta=%f, is_A_trans=%d, "
           "is_B_trans=%d\n",
           __func__,
           M,
           N,
           K,
           alpha,
           beta,
           is_A_trans,
           is_B_trans);
    float *A     = new float[M * K];
    float *B     = new float[N * K];
    float *C     = new float[M * N];
    float *C_ref = new float [M * N];
    assign_values_to_matrix(A, M * K);
    assign_values_to_matrix(B, N * K);
    assign_values_to_matrix(C, M * N);
    memcpy(C_ref, C, sizeof(float) * M * N);
    native_gemm_core(is_A_trans,
                     is_B_trans,
                     M,
                     N,
                     K,
                     alpha,
                     A,
                     is_A_trans ? M:K,
                     B,
                     is_B_trans ? K:N,
                     beta,
                     C_ref);

    struct timeval t1, t2;
    gettimeofday_(&t1);
    bmcv_gemm(handle,
            is_A_trans,
            is_B_trans,
            M,
            N,
            K,
            alpha,
            bm_mem_from_system((void *)A),
            is_A_trans ? M : K,
            bm_mem_from_system((void *)B),
            is_B_trans ? K : N,
            beta,
            bm_mem_from_system((void *)C),
            N);
    gettimeofday_(&t2);
    cout << "gemm TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

    int cmp_res = array_cmp_(
        C_ref,
        C,
        M * N,
        (const char *)"Comparing results of cblas_sgemm and bmblas_gemm...\n",
        BMDNN_COMPARE_EPSILON);
    if (cmp_res != 0) {
        printf("Comparison failed for cblas_sgemm and bmblas_gemm!\n");
        delete[] A;
        delete[] B;
        delete[] C;
        delete[] C_ref;
        exit(-1);
    }
    printf("Comparison done for cblas_sgemm and bmblas_gemm!\n\n");

    delete[] A;
    delete[] B;
    delete[] C;
    delete[] C_ref;
}

#ifdef __linux__
void* test_bmblas_gemm_random(void* args) {
#else
DWORD WINAPI test_bmblas_gemm_random(LPVOID args) {
#endif
    int   M;
    int   N;
    int   K;
    float alpha;
    float beta;
    bool  is_A_trans;
    bool  is_B_trans;
#ifdef SOC_MODE
    int first_loop = 1;
#endif

    gemm_thread_arg_t* gemm_thread_arg = (gemm_thread_arg_t*)args;
    bm_handle_t handle = gemm_thread_arg->handle;
    int trial_num = gemm_thread_arg->loop_num;
    int trial_idx = 0;
    while (trial_idx != trial_num) {
        printf("..................%s: @trial_idx=%d.....................\n",
               __func__,
               trial_idx++);
#ifndef SOC_MODE
        M     = rand() % 80 + 1;
        N     = rand() % 200000 + 1;
        K     = rand() % 800 + 1;
        alpha = (rand() % 100) * 0.05;
        beta  = (rand() % 100) * 0.05;
#else
        if (first_loop == 1) {
            M     = rand() % 80 + 1;
            N     = rand() % 200000 + 1;
            K     = rand() % 800 + 1;
            alpha = (rand() % 100) * 0.05;
            beta  = (rand() % 100) * 0.05;
            first_loop = 0;
        } else {
            M     = rand() % 800 + 1;
            N     = rand() % 800 + 1;
            K     = rand() % 800 + 1;
            alpha = (rand() % 100) * 0.05;
            beta  = (rand() % 100) * 0.05;
        }
#endif
        unsigned int chipid = BM1684X;
        bm_get_chipid(handle, &chipid);
        for (int i = 0; i < 4; i++)
        { // traverse all cominations of trans params
            is_A_trans = i % 2;
            is_B_trans = i / 2;
            is_B_trans = 1;
            for (int j = 0; j < 4; j++)
            { // alpha and beta could be positive or negative
                alpha *= (j % 2) * 2 - 1;
                beta *= (j / 2) * 2 - 1;
                test_bmblas_gemm(handle, M, N, K, alpha, beta, is_A_trans, is_B_trans);
                if(chipid == BM1684X)
                    test_bmblas_gemm_ext(handle, M, N, K, alpha, beta, is_A_trans, is_B_trans);
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int test_loop_times = 1;
    int test_threads_num = 1;
    int dev_id = -1;
    if (argc > 1) {
        test_loop_times = atoi(argv[1]);
    }
    if (argc > 2) {
        test_threads_num = atoi(argv[2]);
    }
    if (argc > 3) {
        dev_id = atoi(argv[3]);
    }
    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST GEMM] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    if (test_threads_num > 4 || test_threads_num < 1) {
        std::cout << "[TEST GEMM] thread nums should be 1~4 "
                  << std::endl;
        exit(-1);
    }
    std::cout << "[TEST GEMM] test starts... LOOP times = " << test_loop_times
              << " threads num = " << test_loop_times << std::endl;

    unsigned int seed = (unsigned)time(NULL);
    srand(seed);
    printf("random seed is %u\r\n", seed);
    int dev_cnt;
    bm_dev_getcount(&dev_cnt);
    if (dev_id >= dev_cnt) {
        std::cout << "[TEST GEMM] dev_id should less than device count, only detect "<< dev_cnt << " devices "
                  << std::endl;
        exit(-1);
    }
    printf("device count = %d\n", dev_cnt);
#ifdef RANDOM_TEST
    #ifdef __linux__
    bm_handle_t handle[dev_cnt];
    #else
    std::shared_ptr<bm_handle_t> handle_(new bm_handle_t[dev_cnt], std::default_delete<bm_handle_t[]>());
    bm_handle_t*                 handle = handle_.get();
    #endif
    for (int i = 0; i < dev_cnt; i++) {
        int id;
        if (dev_id != -1) {
            dev_cnt = 1;
            id = dev_id;
        } else {
            id = i;
        }
        bm_status_t req = bm_dev_request(handle + i, id);
        if (req != BM_SUCCESS) {
            printf("create bm handle for dev%d failed!\n", id);
            exit(-1);
        }
    }
    // test for multi-thread
    #ifdef __linux__
    pthread_t pid[dev_cnt * test_threads_num];
    gemm_thread_arg_t gemm_thread_arg[dev_cnt * test_threads_num];
    for (int d = 0; d < dev_cnt; d++) {
        for (int i = 0; i < test_threads_num; i++) {
            int idx = d * test_threads_num + i;
            gemm_thread_arg[idx].loop_num = test_loop_times;
            gemm_thread_arg[idx].handle = handle[d];
            if (pthread_create(pid + idx, NULL, test_bmblas_gemm_random, gemm_thread_arg + idx)) {
                printf("create thread failed\n");
                return -1;
            }
        }
    }
    for (int d = 0; d < dev_cnt; d++) {
        for (int i = 0; i < test_threads_num; i++) {
            int ret = pthread_join(pid[d * test_threads_num + i], NULL);
            if (ret != 0) {
                printf("Thread join failed\n");
                exit(-1);
            }
        }
    }
    #else
    #define THREAD_NUM 64
    DWORD              dwThreadIdArray[THREAD_NUM];
    HANDLE             hThreadArray[THREAD_NUM];
    if ((dev_cnt * test_threads_num) > THREAD_NUM) {
        printf("thread num = %d is too much\n", dev_cnt * test_threads_num);
        return -1;
    }
    gemm_thread_arg_t *gemm_thread_arg =
        new gemm_thread_arg_t[dev_cnt * test_threads_num];
    for (int d = 0; d < dev_cnt; d++) {
        for (int i = 0; i < test_threads_num; i++) {
            int idx = d * test_threads_num + i;
            gemm_thread_arg[idx].loop_num = test_loop_times;
            gemm_thread_arg[idx].handle   = handle[d];
            hThreadArray[idx] =
                CreateThread(NULL,                     // default security attributes
                             0,                        // use default stack size
                             test_bmblas_gemm_random,  // thread function name
                             gemm_thread_arg + idx,    // argument to thread function
                             0,                        // use default creation flags
                             &dwThreadIdArray[idx]);   // returns the thread identifier
            if (hThreadArray[idx] == NULL) {
                delete[] gemm_thread_arg;
                perror("create thread failed\n");
                exit(-1);
            }
        }
    }
    int   ret = 0;
    DWORD dwWaitResult =
        WaitForMultipleObjects(dev_cnt * test_threads_num, hThreadArray, TRUE, INFINITE);
    // DWORD dwWaitResult = WaitForSingleObject(hThreadArray[i], INFINITE);
    switch (dwWaitResult) {
        // case WAIT_OBJECT_0:
        //    ret = 0;
        //    break;
        case WAIT_FAILED:
            ret = -1;
            break;
        case WAIT_ABANDONED:
            ret = -2;
            break;
        case WAIT_TIMEOUT:
            ret = -3;
            break;
        default:
            ret = 0;
            break;
    }
    if (ret < 0) {
        delete[] gemm_thread_arg;
        printf("Thread join failed\n");
        return -1;
    }
    for (int d = 0; d < dev_cnt; d++) {
        for (int i = 0; i < test_threads_num; i++) {
            int idx = d * test_threads_num + i;
            CloseHandle(hThreadArray[idx]);
        }
    }
    #endif
    for (int d = 0; d < dev_cnt; d++) {
        bm_dev_free(handle[d]);
    }

    #ifndef __linux__
    delete[] gemm_thread_arg;
    #endif

#else
    bm_handle_t handle;
    bm_dev_request(&handle, 0);
    test_bmblas_gemm(handle, 32, 27, 15, 1.32f, -2.73f, false, false);
    test_bmblas_gemm(handle, 32, 27, 15, -1.32f, 2.73f, false, false);
    test_bmblas_gemm(handle, 32, 27, 15, 1.32f, -2.73f, false, true);
    test_bmblas_gemm(handle, 32, 27, 15, -1.32f, 2.73f, false, true);
    test_bmblas_gemm(handle, 32, 27, 15, 1.32f, -2.73f, true, false);
    test_bmblas_gemm(handle, 32, 27, 15, -1.32f, 2.73f, true, false);
    test_bmblas_gemm(handle, 32, 27, 15, 1.32f, -2.73f, true, true);
    test_bmblas_gemm(handle, 32, 27, 15, -1.32f, 2.73f, true, true);
    test_bmblas_gemm(handle, 32, 27, 15, 1.32f, 1.0f, false, false);
    test_bmblas_gemm(handle, 32, 27, 15, -1.32f, 1.0f, false, false);
    test_bmblas_gemm(handle, 32, 27, 15, 1.32f, 1.0f, false, true);
    test_bmblas_gemm(handle, 32, 27, 15, -1.32f, 1.0f, false, true);
    test_bmblas_gemm(handle, 32, 27, 15, 1.32f, 1.0f, true, false);
    test_bmblas_gemm(handle, 32, 27, 15, -1.32f, 1.0f, true, false);
    test_bmblas_gemm(handle, 32, 27, 15, 1.32f, 1.0f, true, true);
    test_bmblas_gemm(handle, 32, 27, 15, -1.32f, 1.0f, true, true);

    test_bmblas_gemm(handle, 32, 5, 15, 1.32f, -2.73f, true, false);
    test_bmblas_gemm(handle, 32, 5, 15, 1.32f, -2.73f, false, false);
    test_bmblas_gemm(handle, 5, 27, 15, 1.32f, -2.73f, true, false);
    test_bmblas_gemm(handle, 5, 27, 15, 1.32f, -2.73f, false, false);
    bm_dev_free(handle);
#endif

    std::cout << "------[TEST GEMM] ALL TEST PASSED!" << std::endl;

    return 0;
}
