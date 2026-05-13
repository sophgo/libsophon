#include "bmcv_api_ext.h"
#include "bmlib_runtime.h"
#include <iostream>
#include <fstream>
#include <cmath>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif
#include "test_misc.h"

using namespace std;

char* opencvFile_path = NULL;

static int test() {
    int flag = 0;
    int H = 1024;
    int W = 1024;
    int C = rand() % 3 + 1;
    C = 1;
    int dim = rand() % C + 1;
    int channels[3] = {0, 1, 2};
    int histSizes[] = {15000, 32, 32};
    float ranges[] = {0, 1000000, 0, 256, 0, 256};
    int totalHists = 1;
    for (int i = 0; i < dim; ++i)
        totalHists *= histSizes[i];
    printf("C = %d ", C);
    printf("H = %d ", H);
    printf("W = %d ", W);
    printf("dims = %d \n", dim);
    bm_handle_t handle = nullptr;
    bm_status_t ret = bm_dev_request(&handle, 0);
    float *inputHost = new float[C * H * W];
    float *inputHostInterleaved = new float[C * H * W];
    float *outputHost = new float[totalHists];
    int count = 1;
    for (int i = 0; i < C; ++i)
        for (int j = 0; j < H * W; ++j){
            //inputHost[i * H * W + j] = static_cast<float>(rand() % 1000000);
            inputHost[i * H * W + j] = count;
            count++;
            if(count == 1000000)
                count = 1;
        }
    if (ret != BM_SUCCESS) {
        printf("bm_dev_request failed. ret = %d\n", ret);
        exit(-1);
    }
    bm_device_mem_t input, output;
    ret = bm_malloc_device_byte(handle, &input, C * H * W * 4);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte failed. ret = %d\n", ret);
        exit(-1);
    }
    ret = bm_memcpy_s2d(handle, input, inputHost);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_s2d failed. ret = %d\n", ret);
        exit(-1);
    }
    ret = bm_malloc_device_byte(handle, &output, totalHists * 4);
    if (ret != BM_SUCCESS) {
        printf("bm_malloc_device_byte failed. ret = %d\n", ret);
        exit(-1);
    }

    struct timeval t1, t2;
    gettimeofday_(&t1);
    ret = bmcv_calc_hist(handle,
                        input,
                        output,
                        C,
                        H,
                        W,
                        channels,
                        dim,
                        histSizes,
                        ranges,
                        0);
    gettimeofday_(&t2);
    printf("calcHist TPU using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));

    if (ret != BM_SUCCESS) {
        printf("bmcv_calc_hist failed. ret = %d\n", ret);
        exit(-1);
    }
    ret = bm_memcpy_d2s(handle, outputHost, output);
    if (ret != BM_SUCCESS) {
        printf("bm_memcpy_d2s failed. ret = %d\n", ret);
        exit(-1);
    }
    for (int i = 0; i < C; ++i)
        for (int j = 0; j < H * W; ++j)
            inputHostInterleaved[j * C + i] = inputHost[i * H * W + j];
    #ifdef __linux__
        float *outputOpencv = new float[totalHists];
        std::ifstream opencv_readfile((std::string(opencvFile_path) + std::string("/opencv_calcHist.bin")), std::ios::binary | std::ios::in);
        if(!opencv_readfile){
            printf("Error opening file\n");
            return -1;
        }
        opencv_readfile.read((char*)outputOpencv, sizeof(float) * totalHists);
        opencv_readfile.close();
        printf("calcHist CV average using time: 2357um");
    #else
        float *outputOpencv = new float[totalHists];
        std::ifstream opencv_readfile((std::string(opencvFile_path) + std::string("/opencv_calcHist.bin")), std::ios::binary | std::ios::in);
        if(!opencv_readfile){
            printf("Error opening file");
            return -1;
        }
        opencv_readfile.read((char*)outputOpencv, sizeof(float) * totalHists);
        opencv_readfile.close();
        printf("calcHist CV average using time: 2357um");
    #endif
#if 0
    float sum0 = 0.f, sum1 = 0.f;
#if 0
    for (int i = 0; i < C * H * W; ++i)
        printf("%f ", (float)inputHost[i]);
    printf("\n");
#endif
    for (int i = 0; i < totalHists; ++i) {
        printf("%f ", outputHost[i]);
        sum0 += outputHost[i];
    }
    printf("\n");
    for (int i = 0; i < totalHists; ++i) {
        print("%f ", outputOpencv[i]);
        sum1 +=outputOpencv[i];
    }
    printf("\n");
    for (int i = 0; i < totalHists; ++i)
        printf("%f, ", outputHost[i] - outputOpencv[i]);
    printf("\n");
    printf("sum0: %f, sum1:%f\n",sum0, sum1);
#endif
    for (int i = 0; i < totalHists; ++i) {
        if (std::abs(outputHost[i] - outputOpencv[i]) > 5e-4) {
            printf("%f vs %f at %d\n", outputHost[i], outputOpencv[i], i);
            flag = -1;
            exit(flag);
        }
    }
    bm_free_device(handle, input);
    bm_free_device(handle, output);
    bm_dev_free(handle);
    delete [] inputHost;
    delete [] inputHostInterleaved;
    delete [] outputHost;
    delete [] outputOpencv;
    return flag;
}
int main(int argc, char *argv[]) {
    (void)(argc);
    (void)(argv);
    opencvFile_path = getenv("BMCV_TEST_FILE_PATH");
    if (opencvFile_path == NULL) {
        printf("please set environment vairable: BMCV_TEST_FILE_PATH !\n");
        return -1;
    }
    int ret = 0;
    for (int i = 0; i < 1; ++i) {
        struct timespec tp;
        clock_gettime_(0, &tp);

        srand(tp.tv_nsec);
        printf("test %d random seed: %ld\n", i, tp.tv_nsec);
        //test();
        ret = test();
        if (ret) {
            printf("test absdiff failed");
            return ret;
        }
    }
    printf("Compare TPU result with OpenCV successfully!\n");
    return 0;
}
