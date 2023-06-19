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
    std::cout << "C = " << C << " ";
    std::cout << "H = " << H << " ";
    std::cout << "W = " << W << " ";
    std::cout << "dims = " << dim << " ";
    std::cout << std::endl;
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
    std::cout << "calcHist TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << std::endl;

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
            std::cout << "Error opening file" << std::endl;
            return -1;
        }
        opencv_readfile.read((char*)outputOpencv, sizeof(float) * totalHists);
        opencv_readfile.close();
        std::cout << "calcHist CV average using time: 2357um" << std::endl;
    #else
        float *outputOpencv = new float[totalHists];
        std::ifstream opencv_readfile((std::string(opencvFile_path) + std::string("/opencv_calcHist.bin")), std::ios::binary | std::ios::in);
        if(!opencv_readfile){
            std::cout << "Error opening file" << std::endl;
            return -1;
        }
        opencv_readfile.read((char*)outputOpencv, sizeof(float) * totalHists);
        opencv_readfile.close();
        std::cout << "calcHist CV average using time: 2357um" << std::endl;
    #endif
#if 0
    float sum0 = 0.f, sum1 = 0.f;
#if 0
    for (int i = 0; i < C * H * W; ++i)
        std::cout << (float)inputHost[i] << " ";
    std::cout << std::endl;
#endif
    for (int i = 0; i < totalHists; ++i) {
        std::cout << outputHost[i] << " ";
        sum0 += outputHost[i];
    }
    std::cout << std::endl;
    for (int i = 0; i < totalHists; ++i) {
        std::cout << outputOpencv[i] << " ";
        sum1 +=outputOpencv[i];
    }
    std::cout << std::endl;
    for (int i = 0; i < totalHists; ++i)
        std::cout << outputHost[i] - outputOpencv[i] << " ";
    std::cout << std::endl;
    std::cout << sum0 << " " << sum1 << std::endl;
#endif
    for (int i = 0; i < totalHists; ++i) {
        if (std::abs(outputHost[i] - outputOpencv[i]) > 5e-4) {
            std::cout << outputHost[i] << " vs " << outputOpencv[i] << " at " << i << std::endl;
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
        std::cout << "test " << i << ": random seed: " << tp.tv_nsec << std::endl;
        //test();
        ret = test();
        if (ret) {
            std::cout << "test absdiff failed" << std::endl;
            return ret;
        }
    }
    std::cout << "Compare TPU result with OpenCV successfully!" << std::endl;
    return 0;
}
