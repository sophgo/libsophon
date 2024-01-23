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

static int cpu_hist_balance(uint8_t* input_host, uint8_t* output_cpu, int height, int width, int batch)
{
    int len = height * width;
    int i, j;
    uint8_t binX;
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

    for (i = 0; i < batch; ++i) {
        memset(cpu_cdf, 0.f, GRAY_SERIES * sizeof(float));

        for (j = 0; j < len; ++j) {
            binX = input_host[i * len + j];
            cpu_cdf[binX]++;
        }

        for (j = 1; j < GRAY_SERIES ; ++j) {
            cpu_cdf[j] += cpu_cdf[j - 1];
        }

        cdf_min = get_cdf_min(cpu_cdf, GRAY_SERIES);
        cdf_max = cpu_cdf[GRAY_SERIES - 1];

        for(j = 0; j < len; ++j) {
            gray_index_tmp = input_host[i * len + j];
            gray_tmp = round((cpu_cdf[gray_index_tmp] - cdf_min) * (GRAY_SERIES - 1) / (cdf_max - cdf_min));
            output_cpu[i * len + j] = (uint8_t)gray_tmp;
        }
    }

    free(cpu_cdf);
    return 0;
}

static int tpu_hist_balance(uint8_t* input_host, uint8_t* output_tpu, int height, int width, int batch)
{
    bm_handle_t handle = NULL;
    bm_device_mem_t input, output;
    int len = height * width;
    int dev_id = 0;
    bm_status_t ret = BM_SUCCESS;
    struct timeval t1, t2;

    ret = bm_dev_request(&handle, dev_id);
    if (ret != BM_SUCCESS) {
        printf("bm_dev_request failed. ret = %d\n", ret);
        return -1;
    }

    ret = bm_malloc_device_byte(handle, &output, batch * len * sizeof(uint8_t));
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte output float failed. ret = %d\n", ret);
        bm_dev_free(handle);
        return -1;
    }

    ret = bm_malloc_device_byte(handle, &input, batch * len * sizeof(uint8_t));
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
    ret = bmcv_hist_balance(handle, input, output, height, width, batch);
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
        if (abs(out_tpu[i] - out_cpu[i] > ERR_MAX)) {
            printf("out_tpu[%d] = %u, out_cpu[%d] = %u\n", i, out_tpu[i], i, out_cpu[i]);
            return -1;
        }
    }

    return 0;
}

static int test_hist_balance(int height, int width, int batch)
{

    int len = height * width;
    int ret = 0;
    int i;
    uint8_t* inputHost = (uint8_t*)malloc(batch * len * sizeof(uint8_t));
    uint8_t* output_cpu = (uint8_t*)malloc(batch * len * sizeof(uint8_t));
    uint8_t* output_tpu = (uint8_t*)malloc(batch * len * sizeof(uint8_t));
    struct timeval t1, t2;

    for (i = 0; i < batch * len; ++i) {
        inputHost[i] = (uint8_t)(rand() % 256);
    }

    gettimeofday_(&t1);
    ret = cpu_hist_balance(inputHost, output_cpu, height, width, batch);
    if (ret) {
        printf("cpu_hist_balance failed!\n");
        goto exit;
    }
    gettimeofday_(&t2);
    printf("Calc_hist CPU using time = %ld(us)\n", TIME_COST_US(t1, t2));

    ret = tpu_hist_balance(inputHost, output_tpu, height, width, batch);
    if (ret) {
        printf("tpu_hist_balance failed!\n");
        goto exit;
    }
    ret = cmp_result(output_cpu, output_tpu, len * batch);
    if (ret) {
        printf("cmp_result failed!\n");
        goto exit;
    }

exit:
    free(inputHost);
    free(output_cpu);
    free(output_tpu);
    return ret;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    int ret = 0;
    int height;
    int width;
    int batch;
    int i;
    int loop;

    printf("please input the loop: \n");
    ret = scanf("%d", &loop);
    printf("please input the img height: \n");
    ret = scanf("%d", &height);
    printf("please input the img width: \n");
    ret = scanf("%d", &width);
    printf("please input the batch: \n");
    ret = scanf("%d", &batch);

    for (i = 0; i < loop; ++i) {
        ret = test_hist_balance(height, width, batch);
        if (ret) {
            printf("test hist balance failed\n");
            return ret;
        }
    }
    printf("Compare TPU result with OpenCV successfully!\n");

    return ret;
}
