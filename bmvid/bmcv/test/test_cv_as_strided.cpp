#include <iostream>
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

#define DIV_UP(a, b) ( ((a) + (b) - 1) / (b) )

using namespace std;

void as_strided_gen_data(float* data, int len) {
    srand((unsigned int)time(NULL));
    for (int i = 0; i < len; i++) {
        data[i] = (float)rand() / (float)RAND_MAX * 100;
    }
}

void concatenation(float *input, int input_row, int input_col, int n) {
    int original_size = input_row * input_col;
    int new_size = original_size * n;
    float *temp = (float *)malloc(new_size * sizeof(float));

    for (int i = 0; i < new_size; i++) {
        int original_index = i % original_size;
        temp[i] = input[original_index];
    }

    for (int i = 0; i < new_size; i++) {
        input[i] = temp[i];
    }

    free(temp);
}

void printdata(float* data, int row, int col) {
    int num = row * col;
    for (int i = 0; i < num; i++) {
        printf("%f ", data[i]);
        if ((i + 1) % col == 0)
            printf("\n");
    }
    printf("\n");
}

int cmp(float* exp, float* got, int n_Rows, int n_Cols) {
    const float epsilon = 1e-6;

    for (int i = 0; i < n_Rows; ++i) {
        for (int j = 0; j < n_Cols; ++j) {
            int index = i * n_Cols + j;
            if (fabs(exp[index] - got[index]) > epsilon) {
                printf("i = %d, j = %d, exp = %f, got = %f\n", i-1, j-1, exp[index-1], got[index-1]);
                return 1;
            }
        }
    }
    return 0;
}

void native_as_strided_core(float* input, float* output, int input_row, int input_col, int output_row, int output_col, int row_stride, int col_stride) {
    for(int k = 0; k < output_row * output_col; k++){
        output[k] = 0;
    }

    int input_size = input_row * input_col;
    int start = 0;
    int cur = start;
    for(int i = 0; i < output_row; i++) {
        cur = start;
        for(int j = 0; j < output_col; j++) {
            output[i * output_col + j] = input[cur];
            cur += col_stride;
            cur %= input_size;
        }
        start += row_stride;
        start %= input_size;
    }

    float* temp_output = new float[output_row * output_col];
    for(int i = 0; i < output_col; i++) {
        for(int j = 0; j < output_row; j++) {
            temp_output[i * output_row + j] = output[j * output_col + i];
        }
    }

    for(int k = 0; k < output_row * output_col; k++){
        output[k] = temp_output[k];
    }

    delete[] temp_output;
}

bm_status_t test_as_strided_random(bm_handle_t handle, int input_row, int input_col, int output_row, int output_col, int row_stride, int col_stride) {
    int compare = 1;
    bm_status_t ret = BM_SUCCESS;
    printf("%s: input_row=%d, input_col=%d, output_row=%d, output_col=%d, row_stride=%d, col_stride=%d\n",
           __func__, input_row , input_col, output_row, output_col, row_stride, col_stride);

    int n = DIV_UP((1 + (output_col - 1) * col_stride + (output_row - 1) * row_stride), input_row * input_col);

    float* input_data = new float[input_row * input_col * n];
    float* output_data = new float[output_row * output_col];
    as_strided_gen_data(input_data, input_row * input_col);

    if (n > 1){
        concatenation(input_data, input_row, input_col, n);
    }

    bm_device_mem_t input_dev_mem, output_dev_mem;
    bm_malloc_device_byte(handle, &input_dev_mem, input_row * input_col * n * sizeof(float));
    bm_malloc_device_byte(handle, &output_dev_mem, output_row * output_col * sizeof(float));

    bm_memcpy_s2d(handle, input_dev_mem, input_data);

    struct timeval t1, t2;
    gettimeofday_(&t1);
    ret = bmcv_as_strided(handle, input_dev_mem, output_dev_mem, input_row * n, input_col, output_col, output_row, col_stride, row_stride);
    gettimeofday_(&t2);
    std::cout << "as_strided TPU using time= " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "(us)" << std::endl;
    if (ret != BM_SUCCESS) {
        printf("as_strided failed. ret = %d\n", ret);
        goto exit;
    }

    bm_memcpy_d2s(handle, output_data, output_dev_mem);

    if(compare) {
        float* output_data_ref = new float[output_row * output_col * sizeof(float)];
        native_as_strided_core(input_data, output_data_ref, input_row * n, input_col, output_row, output_col, row_stride, col_stride);

        // std::cout << "input=" << std::endl;
        // printdata((float*)input_data, input_row  * n, input_col);
        // std::cout << "output_host=" << std::endl;
        // printdata((float*)output_data_ref, output_row, output_col);
        // std::cout << "output_device=" << std::endl;
        // printdata((float*)output_data, output_row, output_col);

        int diff = cmp((float*)output_data_ref, (float*)output_data, output_row, output_col);
        std::cout << "diff=" << diff << std::endl;
        if (diff)
            ret = BM_ERR_DATA;
        delete[] output_data_ref;
    }

    if (ret != BM_SUCCESS) {
        std::cout << "failed" << std::endl;
        goto exit;
    }
    else {
        std::cout << "success" << std::endl;
    }
    exit:
        bm_free_device(handle, input_dev_mem);
        bm_free_device(handle, output_dev_mem);
        delete[] output_data;
        delete[] input_data;
        return ret;
}

int main(int argc, char* argv[]){
    int loop = 1;
    int input_row = 5;
    int input_col = 5;
    int output_row = 3;
    int output_col = 3;
    int row_stride = 1;
    int col_stride = 2;

    bm_handle_t handle;
    bm_status_t ret = BM_SUCCESS;
    ret = bm_dev_request(&handle, 0);
    if (BM_SUCCESS != ret){
        printf("request dev failed\n");
        return BM_ERR_FAILURE;
    }

    if(argc == 1){
        ret = test_as_strided_random(handle, input_row, input_col, output_row, output_col, row_stride, col_stride);
        if (ret != BM_SUCCESS) {
            cout << "test as_strided failed" << endl;
            goto exit;
        }
    }
    else if(argc == 2){
        loop = atoi(argv[1]);
        for (int i = 0; i < loop; i++) {
            ret = test_as_strided_random(handle, input_row, input_col, output_row, output_col, row_stride, col_stride);
            if (ret != BM_SUCCESS) {
                cout << "test as_strided failed" << endl;
                goto exit;
            }
        }
    }
    else if(argc == 8){
        loop = atoi(argv[1]);
        input_row = atoi(argv[2]);
        input_col = atoi(argv[3]);
        output_row = atoi(argv[4]);
        output_col = atoi(argv[5]);
        row_stride = atoi(argv[6]);
        col_stride = atoi(argv[7]);
        for (int i = 0; i < loop; i++) {
            ret = test_as_strided_random(handle, input_row, input_col, output_row, output_col, row_stride, col_stride);
            if (ret != BM_SUCCESS) {
                cout << "test as_strided failed" << endl;
                goto exit;
            }
        }
    }
    else{
        printf("usage1: use example\n");
        printf("%s\n", argv[0]);
        printf("example: ");
        printf("n_Rows=6 n_Cols = 6 row_stride = 3 col_stride = 1, loop once\n");
        printf("%s\n", argv[0]);
        printf("\n");

        printf("usage2: set loop\n");
        printf("%s loop\n", argv[0]);
        printf("example: ");
        printf("n_Rows=6 n_Cols = 6 row_stride = 3 col_stride = 1, loop twice\n");
        printf("%s 2\n", argv[0]);
        printf("\n");

        printf("usage3: set shape, stride and loop\n");
        printf("%s loop n_Rows n_Cols row_stride col_stride\n", argv[0]);
        printf("example: ");
        printf("n_Rows=5 n_Cols = 5 row_stride = 1 col_stride = 2, loop twice\n");
        printf("%s 2 5 5 1 2\n", argv[0]);
        goto exit;
    }

    cout << "Compare TPU result with CPU successfully!" << endl;
    exit:
        bm_dev_free(handle);
        return ret;
}
