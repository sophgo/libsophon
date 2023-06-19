#include "bmcv_api_ext.h"
#include "bmlib_runtime.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <new>
#include <string.h>
#include "test_misc.h"

#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

#define BMDNN_FIX_POINT_ERR 0
#define BMDNN_FLOAT32_ERR 0.01

using namespace std;

static bm_handle_t handle;

static void assign_fix8b_matrix(void* mat, int size, int is_16bit, int seed = 0) {
    srand(seed);
    for (int i = 0; i < size; i++) {
        if (is_16bit) {
            *((signed short*)mat + i) = rand() % 65536 - 32768;
        } else {
            *((signed char*)mat + i) = rand() % 256 - 128;
        }
    }
}

// transpose
template <typename T>
static int local_transpose(T* src, int row_num, int col_num){
    T* dst = new T[row_num * col_num];
    for(int i = 0; i < row_num; i++){
        for(int j = 0; j < col_num ; j++){
            dst[j * row_num + i] = src[i * col_num + j];
        }
    }
    memcpy(src, dst, sizeof(T) * col_num * row_num);
    return 0;
}

// native reference
template <typename T, typename D, typename L>
static int matrix_multiply( bool trans_A,
                            bool trans_B,
                            int M,
                            int N,
                            int K,
                            T* src_A,
                            int lda,
                            D* src_B,
                            int ldb,
                            L* src_C){
    T * pA = new T[M * K];
    D * pB = new D[K * N];
    memcpy(pA, src_A, sizeof(T) * M * K);
    memcpy(pB, src_B, sizeof(D) * K * N);
    L * pC = new L[M * N];
    memset(pC, 0, sizeof(L) * M * N);
    if(trans_A)
        local_transpose<T>(pA, K, lda);
    if(trans_B)
        local_transpose<D>(pB, N, ldb);
    // matrix_multiply
    for(int i = 0; i < M; i++){
        for(int j = 0; j < N; j++){
            for(int l = 0; l < K; l++){
                pC[i * N + j] += pA[i * K + l] * pB[l * N + j];
            }
        }
    }
    memcpy(src_C, pC, sizeof(L) * M * N);
    return 0;
}

static void rightShiftAndSaturateToInt(const int *srcData, void *dstData, int shiftNum, int length, int is_16bit) {
    int tmpData = 0;
    int half_data = 1 << (shiftNum - 1);
    half_data = shiftNum == 0 ? 0 : half_data;
    const int maxDataforInt = is_16bit ? 32767 : 127;
    const int minDataforInt = is_16bit ? -32768 : -128;
    for (int i = 0; i < length; i++) {
        // right shift
        tmpData = (srcData[i] + half_data) >> shiftNum;

        // saturate to int 8
        int dst_data = 0;
        if (tmpData >= maxDataforInt) {
            dst_data = maxDataforInt;
        }
        else if (tmpData <= minDataforInt) {
            dst_data = minDataforInt;
        } else {
            dst_data = tmpData;
        }
        if (is_16bit) {
            *((signed short*)dstData + i) = (signed short)dst_data;
        } else {
            *((signed char*)dstData + i) = (signed char)dst_data;
        }
    }
}

static void rightShiftAndSaturateToUnInt(const int *srcData, void *dstData, int shiftNum, int length, int is_16bit) {
    int tmpData = 0;
    // int half_data = 1 << (shiftNum - 1);
    long half_data = 1 << (shiftNum - 1);
    half_data = shiftNum == 0 ? 0 : half_data;
    const int maxDataforUnInt = is_16bit ? 65535 : 255;
    for (int i = 0; i < length; i++) {
        // right shift
        tmpData = (srcData[i] + half_data) >> shiftNum;

        // saturate to unint 8
        int dst_data = 0;
        if (tmpData >= maxDataforUnInt) {
            dst_data = maxDataforUnInt;
        }
        else if (tmpData <= 0) {
            dst_data = 0;
        } else {
            dst_data = tmpData;
        }
        if (is_16bit) {
            *((unsigned short*)dstData + i) = (unsigned short)dst_data;
        } else {
            *((unsigned char*)dstData + i) = (unsigned char)dst_data;
        }
    }
}

// native
static void native_matmul_cmp(signed char* src_A,
                              signed char* src_B,
                              void * output,
                              int M,
                              int N,
                              int K,
                              int trans_A,
                              int trans_B,
                              int signA,
                              int signB,
                              int right_shift_bit,
                              int result_type,
                              float alpha,
                              float beta){
    double* pA;
    double* pB;
    try {
        pA = new double[M * K];
        pB = new double[K * N];
    }
    catch(std::bad_alloc &memExp) {
        std::cerr<<memExp.what()<<std::endl;
        exit(-1);
    }
    const unsigned char* uA = (unsigned char*)src_A;
    const unsigned char* uB = (unsigned char*)src_B;
    for (int i = 0; i < M * K; i++) {
        pA[i] = (signA == 1) ? src_A[i] : uA[i];
    }
    for (int i = 0; i < K * N; i++) {
        pB[i] = (signB == 1) ? src_B[i] : uB[i];
    }
    int* output_dst = new int[M * N];
    if(beta != 0)
        output_dst = (int*)output;
    else
        memset(output_dst, 0, sizeof(int) * M * N);
    // src_A * src_B
    matrix_multiply(
        trans_A,
        trans_B,
        M,
        N,
        K,
        pA,
        trans_A ? M:K,
        pB,
        trans_B ? K:N,
        output_dst);
    // result_type --> void* output datatype
    if(result_type != 2){
        int len = M * N;
        if(signA ==1 || signB == 1)
            rightShiftAndSaturateToInt(output_dst, output, right_shift_bit, len, result_type);
        else
            rightShiftAndSaturateToUnInt(output_dst, output, right_shift_bit, len, result_type);
    }else{
        float *outputC = (float*)output;
        for(int i = 0; i < M * N; i++)
            outputC[i] = alpha * output_dst[i] + beta;
    }
    delete [] pA;
    delete [] pB;
    delete [] output_dst;
}

static void matmul_speific_case(
        int batch_size,
        int input_neuron_num,
        int output_neuron_num,
        int transpose,
        int input_sign,
        int weight_sign,
        int right_shift_bit,
        int result_type,
        float alpha,
        float beta
    ) {
    printf("\nbatch_size %d, input_neuron_num %d, output_neuron_num %d, transpose %d,  \
            \ninput_sign %d, weight_sign %d, right_shift_bit %d, result_type %d,   \
            \nalpha %f, beta %f\n",
           batch_size, input_neuron_num, output_neuron_num, transpose,
           input_sign, weight_sign, right_shift_bit, result_type, alpha, beta);

    signed char* input;
    signed char* weight;
    signed char *output;
    signed char *output_ref;
    try {
        input = new signed char[batch_size * input_neuron_num];
        weight = new signed char[input_neuron_num * output_neuron_num];
        if (result_type) {
            output = new signed char[batch_size * output_neuron_num * 2 * result_type];
            output_ref = new signed char[batch_size * output_neuron_num * 2 * result_type];
        } else {
            output = new signed char[batch_size * output_neuron_num];
            memset(output, 0, batch_size * output_neuron_num);
            output_ref = new signed char[batch_size * output_neuron_num];
            memset(output_ref, 0, batch_size * output_neuron_num);
        }
    }
    catch(std::bad_alloc &memExp) {
        std::cerr<<memExp.what()<<std::endl;
        exit(-1);
    }
    assign_fix8b_matrix(input, batch_size * input_neuron_num, 0, 1);
    assign_fix8b_matrix(weight, input_neuron_num * output_neuron_num, 0, 2);
    int trans_A = 0;
    native_matmul_cmp(input,
                      weight,
                      output_ref,
                      batch_size,
                      output_neuron_num,
                      input_neuron_num,
                      trans_A,
                      transpose,
                      input_sign,
                      weight_sign,
                      right_shift_bit,
                      result_type,
                      alpha,
                      beta);
    // call bmcv
    bm_status_t ret = BM_SUCCESS;
    struct timeval t1, t2;
    gettimeofday_(&t1);
    ret = bmcv_matmul(
        handle,
        batch_size,
        output_neuron_num,
        input_neuron_num,
        bm_mem_from_system((void*)input),
        bm_mem_from_system((void*)weight),
        bm_mem_from_system((void*)output),
        input_sign,
        weight_sign,
        right_shift_bit,
        result_type,
        transpose,
        alpha,
        beta);
    gettimeofday_(&t2);
    cout << "matmul TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

    if (ret == BM_SUCCESS) {
        // compare bmcv and reference
        char info[] = "test_cv_matmul_fix8b";
        int res = 0;
        if (result_type == 1) {
            res = array_cmp_fix16b_(
                    output_ref,
                    output,
                    (input_sign || weight_sign) ? 1 : 0,
                    batch_size * output_neuron_num,
                    info,
                    BMDNN_FIX_POINT_ERR);
        } else if (result_type == 2) {
            res = array_cmp_(
                    (float*)output_ref,
                    (float*)output,
                    batch_size * output_neuron_num,
                    info,
                    BMDNN_FLOAT32_ERR);
        } else {
            res = array_cmp_fix8b_(
                    output_ref,
                    output,
                    (input_sign || weight_sign) ? 1 : 0,
                    batch_size * output_neuron_num,
                    info,
                    BMDNN_FIX_POINT_ERR);
        }
        if (res != 0) {
            delete [] input;
            delete [] weight;
            delete [] output;
            delete [] output_ref;
            exit(-1);
        }
    } else {
        printf("Call bmcv_matmul failed. return code:%d\n", ret);
        delete [] input;
        delete [] weight;
        delete [] output;
        delete [] output_ref;
        exit(-1);
    }

    delete [] input;
    delete [] weight;
    delete [] output;
    delete [] output_ref;
}

static void matmul_random_test(int loop) {
        int random_test_num = loop;
        int* batch_size;
        int* input_neuron_num;
        int* output_neuron_num;
        int* transpose;
        int* input_sign;
        int* weight_sign;
        int* right_shift_bit;
        int* result_type;
        float* alpha;
        float* beta;

    try {
        batch_size = new int[random_test_num];
        input_neuron_num = new int[random_test_num];
        output_neuron_num = new int[random_test_num];
        transpose = new int[random_test_num];
        input_sign = new int[random_test_num];
        weight_sign = new int[random_test_num];
        right_shift_bit = new int[random_test_num];
        result_type = new int[random_test_num];
        alpha = new float[random_test_num];
        beta = new float[random_test_num];
    }
    catch(std::bad_alloc &memExp) {
        std::cerr<<memExp.what()<<std::endl;
        exit(-1);
    }

    for (int i = 0; i < random_test_num; i++) {
        batch_size[i] = rand() % 6 + 1;
        input_neuron_num[i] = rand() % 512 + 1;
        output_neuron_num[i] = rand() % 9216 + 1;
        transpose[i] = rand() % 2;
        input_sign[i] = rand() % 2;
        weight_sign[i] = rand() % 2;
        result_type[i] = rand() % 2;
        if (result_type[i] == 2) {
            alpha[i] = rand() % 100 / 10.0;
            beta[i] = rand() % 100;
            right_shift_bit[i] = 0;
        } else {
            alpha[i] = 1;
            beta[i] = 0;
            right_shift_bit[i] = rand() % 12 + 1;
        }
    }
    for (int i = 0; i < random_test_num; i++) {
        matmul_speific_case(
            batch_size[i],
            input_neuron_num[i],
            output_neuron_num[i],
            transpose[i],
            input_sign[i],
            weight_sign[i],
            right_shift_bit[i],
            result_type[i],
            alpha[i],
            beta[i]);
    }
    delete [] batch_size;
    delete [] input_neuron_num;
    delete [] output_neuron_num;
    delete [] transpose;
    delete [] input_sign;
    delete [] weight_sign;
    delete [] right_shift_bit;
    delete [] result_type;
    delete [] alpha;
    delete [] beta;
}

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);
    printf("*** Test bmcv matmul start!\n");
    int seed = (int)time(NULL);
    printf("--- random seed = %d ---\n", seed);
    srand(seed);
    bm_dev_request(&handle, 0);
    matmul_random_test(5);
    bm_dev_free(handle);

    printf("***test bmcv matmul success!\n");
    return 0;
}