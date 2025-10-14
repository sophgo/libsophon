#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <cmath>
#include "bmcv_api.h"
#include "test_misc.h"
#ifdef __linux__
  #include <pthread.h>
  #include <unistd.h>
  #include <sys/time.h>
#else
  #include <windows.h>
#endif

using namespace std;

#define ERR_MAX 1e-1
#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))
struct cv_lap_thread_arg_t {
    int loop;
    int row;
    int col;
    bm_handle_t handle;
};

int tpu_lap(float* input_host, int row, int col, float* output_host, bm_handle_t handle)
{
    bm_device_mem_t input, output;
    bm_status_t ret = BM_SUCCESS;
    struct timeval t1, t2;

    ret = bm_malloc_device_byte(handle, &output, row * col * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte output float failed. ret = %d\n", ret);
        return -1;
    }

    ret = bm_malloc_device_byte(handle, &input, row * col * sizeof(float));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte intput uint8_t failed. ret = %d\n", ret);
        bm_free_device(handle, output);
        return -1;
    }

    ret = bm_memcpy_s2d(handle, input, input_host);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d uint8_t failed. ret = %d\n", ret);
        bm_free_device(handle, input);
        bm_free_device(handle, output);
        return -1;
    }
    gettimeofday_(&t1);
    ret = bmcv_lap_matrix(handle, input, output, row, col);
    if (ret != BM_SUCCESS) {
        printf("bmcv_lap_matrix float failed. ret = %d\n", ret);
        bm_free_device(handle, input);
        bm_free_device(handle, output);
        return -1;
    }
    gettimeofday_(&t2);
    printf("bmcv_lap_matrix TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));

    ret = bm_memcpy_d2s(handle, output_host, output);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s failed. ret = %d\n", ret);
        bm_free_device(handle, input);
        bm_free_device(handle, output);
        return -1;
    }

    bm_free_device(handle, input);
    bm_free_device(handle, output);
    return 0;
}

int cpu_lap(float* input, int row, int col, float* output)
{
    float* degree_matrix;
    int i, j;
    float row_sum;

    degree_matrix = (float*)malloc(row * sizeof(float));
    memset((void*)degree_matrix, 0, row * sizeof(float));

    for (i = 0; i < row; ++i) {
        row_sum = 0.f;
        for (j = 0; j < col; ++j) {
            row_sum += input[i * col + j];
        }
        degree_matrix[i] = row_sum;
    }

    for (i = 0; i < row; ++i) {
        for (j = 0; j < col; ++j) {
                if (i == j) {
                    output[i * col + j] = degree_matrix[i] - input[i * col + j];
                } else {
                    output[i * col + j] = 0.f - input[i * col + j];
                }
        }
    }

    free(degree_matrix);
    return 0;
}

int cmp_res(float* tpu_out, float* cpu_out, int row, int col)
{
    int i, j;

    for (i = 0; i < row; ++i) {
        for (j = 0; j < col; ++j) {
            if (abs(cpu_out[i * col + j] - tpu_out[i * col + j]) > ERR_MAX) {
                printf("index[%d][%d]: cpu_output = %f, tpu_output = %f\n", \
                        i, j, cpu_out[i * col + j], tpu_out[i * col + j]);
                return -1;
            }
        }
    }
    return 0;
}


int test_lap(bm_handle_t handle, int row, int col)
{
    float* input = (float*)malloc(row * col * sizeof(float));
    float* output_cpu = (float*)malloc(row * col * sizeof(float));
    float* output_tpu = (float*)malloc(row * col * sizeof(float));
    int ret = 0;
    int i, j;
    struct timeval t1, t2;

    for (i = 0; i < row; ++i) {
        for (j = 0; j < col; ++j) {
            input[i * col + j] = (float)rand() / RAND_MAX;
        }
    }

    gettimeofday_(&t1);
    ret = cpu_lap(input, row, col, output_cpu);
    if (ret) {
        printf("cpu_lap error\n");
        goto exit;
    }
    gettimeofday_(&t2);
    printf("bmcv_lap_matrix CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));

    ret = tpu_lap(input, row, col, output_tpu, handle);
    if (ret) {
        printf("tpu_lap error\n");
        goto exit;
    }

    ret = cmp_res(output_tpu, output_cpu, row, col);
    if (ret) {
        printf("cmp_res error\n");
        goto exit;
    }

exit:
free(input);
free(output_cpu);
free(output_tpu);
return ret;
}

int main(int argc, char* args[])
{
    struct timespec tp;
    clock_gettime_(0, &tp);
    srand(tp.tv_nsec);

    int loop = 1;
    int row = 3;
    int col = 3;
    int ret = 0;
    int i = 0;
    bm_handle_t handle;

    ret = bm_dev_request(&handle, 0);
    if (ret) {
        printf("bm_dev_request failed. ret = %d\n", ret);
        return ret;
    }

    if (argc > 1) loop = atoi(args[1]);
    if (argc > 2) row = atoi(args[2]);
    if (argc > 3) col = atoi(args[3]);

    for (i = 0; i < loop; ++i) {
        ret = test_lap(handle, row, col);
        if (ret) {
            printf("------Test lap Matrix Failed!------\n");
            bm_dev_free(handle);
            return ret;
        }
    }

    printf("------Test Lap Matrix Success!------\n");
    bm_dev_free(handle);
    return ret;
}