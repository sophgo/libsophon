#include "bmcv_api_ext_c.h"
#include "bmlib_runtime.h"
#include <iostream>
#include <cmath>
#include <string.h>
#include <cstdint>
#include "test_misc.h"
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

#define TIME_COST_US(start, end) ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec))
#define GRAY_SERIES 256
#define ERR_MAX 1

struct gray_para {
    uint8_t gray_val;
    float cdf_val;
};

static float get_cdf_min(float* cdf, int len)
{
    int i;

    for(i = 0; i < len; ++i) {
        if (cdf[i] != 0) {
            return cdf[i];
        }
    }
    return 0.f;
}

static int cpu_hist_balance(uint8_t* input_host, uint8_t* output_cpu, int height, int width)
{
    int H = height;
    int W = width;
    uint8_t binX;
    int j;
    float gray_tmp;
    uint8_t gray_index_tmp;
    float cdf_min;
    float cdf_max;
    float* cpu_cdf;

    if (input_host == NULL || output_cpu == NULL) {
        printf("cpu_calc_hist param error!\n");
        return -1;
    }

    cpu_cdf = (float*)malloc(GRAY_SERIES * sizeof(float));
    memset(cpu_cdf, 0.f, GRAY_SERIES *sizeof(float));

    for (j = 0; j < W * H; ++j) {
        binX = input_host[j];
        cpu_cdf[binX]++;
    }

    for (j = 1; j < GRAY_SERIES ; ++j) {
        cpu_cdf[j] += cpu_cdf[j - 1];
    }

    cdf_min = get_cdf_min(cpu_cdf, GRAY_SERIES);
    cdf_max = H * W;

    for(j = 0; j < H * W; ++j) {
        gray_index_tmp = input_host[j];
        gray_tmp = round((cpu_cdf[gray_index_tmp] - cdf_min) * (GRAY_SERIES - 1) / (cdf_max - cdf_min));
        output_cpu[j] = (uint8_t)gray_tmp;
    }

    free(cpu_cdf);
    return 0;
}

static int tpu_hist_balance(uint8_t* input_host, uint8_t* output_tpu, int height, int width)
{
    bm_handle_t handle = NULL;
    bm_device_mem_t input, output;
    int dev_id = 0;
    int H = height;
    int W = width;
    bm_status_t ret = BM_SUCCESS;
    struct timeval t1, t2;

    ret = bm_dev_request(&handle, dev_id);
    if (ret != BM_SUCCESS) {
        printf("bm_dev_request failed. ret = %d\n", ret);
        return -1;
    }

    ret = bm_malloc_device_byte(handle, &output, H * W * sizeof(uint8_t));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte output float failed. ret = %d\n", ret);
        bm_dev_free(handle);
        return -1;
    }

    ret = bm_malloc_device_byte(handle, &input, H * W * sizeof(uint8_t));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte intput uint8_t failed. ret = %d\n", ret);
        bm_free_device(handle, output);
        bm_dev_free(handle);
        return -1;
    }

    ret = bm_memcpy_s2d(handle, input, input_host);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d uint8_t failed. ret = %d\n", ret);
        bm_free_device(handle, input);
        bm_free_device(handle, output);
        bm_dev_free(handle);
        return -1;
    }

    gettimeofday_(&t1);
    ret = bmcv_hist_balance(handle, input, output, H, W);
    if (ret != BM_SUCCESS) {
        printf("bmcv_calc_hist_weighted uint8_t failed. ret = %d\n", ret);
        bm_free_device(handle, input);
        bm_free_device(handle, output);
        bm_dev_free(handle);
        return -1;
    }
    gettimeofday_(&t2);
    printf("bmcv_hist_balance TPU using time = %ld(us)\n", TIME_COST_US(t1, t2));

    ret = bm_memcpy_d2s(handle, output_tpu, output);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s failed. ret = %d\n", ret);
        bm_free_device(handle, input);
        bm_free_device(handle, output);
        bm_dev_free(handle);
        return -1;
    }

    bm_free_device(handle, input);
    bm_free_device(handle, output);
    bm_dev_free(handle);
    return 0;
}

static int cmp_result(uint8_t* out_cpu, uint8_t* out_tpu, int len)
{
    int i;

    for (i = 0; i < len; ++i) {
        if (abs(out_tpu[i] - out_cpu[i]) > ERR_MAX) {
            printf("out_tpu[%d] = %u, out_cpu[%d] = %u\n", i, out_tpu[i], i, out_cpu[i]);
            return -1;
        }
    }

    return 0;
}

static int test_hist_balance(int height, int width)
{
    int len = height * width;
    int j;
    int ret = 0;
    struct timeval t1, t2;

    uint8_t* inputHost = new uint8_t[len];
    uint8_t* output_cpu = new uint8_t[len];
    uint8_t* output_tpu = new uint8_t[len];

    for (j = 0; j < len; ++j) {
        inputHost[j] = (uint8_t)(rand() % 256);
    }

    gettimeofday_(&t1);
    ret = cpu_hist_balance(inputHost, output_cpu, height, width);
    gettimeofday_(&t2);
    printf("cpu_hist_balance CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));
    if (ret) {
        printf("cpu_hist_balance failed!\n");
        return ret;
    }
    ret = tpu_hist_balance(inputHost, output_tpu, height, width);
    if (ret) {
        printf("tpu_hist_balance failed!\n");
        return ret;
    }

    ret = cmp_result(output_cpu, output_tpu, len);
    if (ret) {
        printf("cmp_result failed!\n");
        return ret;
    }

    delete[]inputHost;
    delete[]output_cpu;
    delete[]output_tpu;
    return ret;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    int ret = 0;
    int height;
    int width;
    int loop = 1;
    int i;

    printf("please input the img height: \n");
    ret = scanf("%d", &(height));
    printf("please input the img width: \n");
    ret = scanf("%d", &(width));
    printf("please input the loop: \n");
    ret = scanf("%d", &(loop));

    for (i = 0; i < loop; ++i) {
        ret = test_hist_balance(height, width);
        if (ret) {
            std::cout << "test_hist_balance failed" << std::endl;
            return ret;
        }
    }
    std::cout << "Compare TPU result with CPU successfully!" << std::endl;
    return 0;
}