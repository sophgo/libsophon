#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "bmlib_runtime.h"
#include <iostream>
#include <fstream>
#include <cmath>
#ifdef __linux__
    #include <sys/time.h>
#endif
using namespace std;

char* opencvFile_path = NULL;

static int test(int M, int N, bool forward, bool realInput) {
    printf("M = %d, N = %d\n", M, N);
    float *XRHost = new float[M * N];
    float *XIHost = new float[M * N];
    float *YRHost = new float[M * N];
    float *YIHost = new float[M * N];
    float *YRRef = new float[M * N];
    float *YIRef = new float[M * N];
    int count = 0;
    for (int i = 0; i < M * N; ++i) {
        // XRHost[i] = rand() % 5 - 2;
        // XIHost[i] = realInput ? 0 : rand() % 5 - 2;
        XRHost[i] = count - 2;
        XIHost[i] = realInput ? 0 : count - 2;
        count++;
        if(count == 5)
            count = 0;
    }
    bm_handle_t handle = nullptr;
    BM_CHECK_RET(bm_dev_request(&handle, 0));
    bm_device_mem_t XRDev, XIDev, YRDev, YIDev;
    BM_CHECK_RET(bm_malloc_device_byte(handle, &XRDev, M * N * 4));
    BM_CHECK_RET(bm_malloc_device_byte(handle, &XIDev, M * N * 4));
    BM_CHECK_RET(bm_malloc_device_byte(handle, &YRDev, M * N * 4));
    BM_CHECK_RET(bm_malloc_device_byte(handle, &YIDev, M * N * 4));
    BM_CHECK_RET(bm_memcpy_s2d(handle, XRDev, XRHost));
    BM_CHECK_RET(bm_memcpy_s2d(handle, XIDev, XIHost));
    void *plan = nullptr;
    BM_CHECK_RET(bmcv_fft_2d_create_plan(handle, M, N, forward, plan));
    #ifdef __linux__
        struct timeval t1, t2;
        gettimeofday(&t1, NULL);
        if (realInput)
            BM_CHECK_RET(bmcv_fft_execute_real_input(handle, XRDev, YRDev, YIDev, plan));
        else
            BM_CHECK_RET(bmcv_fft_execute(handle, XRDev, XIDev, YRDev, YIDev, plan));
        gettimeofday(&t2, NULL);
        printf("fft 2d execute using time: %ld(us)\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
    #else
        struct timespec tp1, tp2;
        clock_gettime_win(0, &tp1);
        if (realInput)
            BM_CHECK_RET(bmcv_fft_execute_real_input(handle, XRDev, YRDev, YIDev, plan));
        else
            BM_CHECK_RET(bmcv_fft_execute(handle, XRDev, XIDev, YRDev, YIDev, plan));
        clock_gettime_win(0, &tp2);
        std::cout << "fft 2d execute using time: " << ((tp2.tv_sec - tp1.tv_sec) * 1000000 + (tp2.tv_nsec - tp1.tv_nsec)/1000) << "us" << std::endl;
    #endif
    bmcv_fft_destroy_plan(handle, plan);
    BM_CHECK_RET(bm_memcpy_d2s(handle, YRHost, YRDev));
    BM_CHECK_RET(bm_memcpy_d2s(handle, YIHost, YIDev));
    bm_free_device(handle, XRDev);
    bm_free_device(handle, XIDev);
    bm_free_device(handle, YRDev);
    bm_free_device(handle, YIDev);
    bm_dev_free(handle);
    std::ifstream opencv_readfile_YRRef((std::string(opencvFile_path) + std::string("/opencv_fft_2d_YRRef.bin")), std::ios::binary | std::ios::in);
    std::ifstream opencv_readfile_YIRef((std::string(opencvFile_path) + std::string("/opencv_fft_2d_YIRef.bin")), std::ios::binary | std::ios::in);
    if((!opencv_readfile_YRRef) || (!opencv_readfile_YIRef)){
        printf("Error opening file\n");
        // exit(-1);
        return -1;
    }
    opencv_readfile_YRRef.read((char*)YRRef, sizeof(float) * M * N);
    opencv_readfile_YIRef.read((char*)YIRef, sizeof(float) * M * N);
    opencv_readfile_YRRef.close();
    opencv_readfile_YIRef.close();
    const float tol = 1e-3;
    for (int b = 0; b < M; ++b) {
        for (int i = 0; i < N; ++i) {
            int idx_ref = b * N + i;
            int idx_cal = idx_ref;
            float errR = std::abs(YRRef[idx_ref] - YRHost[idx_cal]) / std::max(std::max(std::abs(YRRef[idx_ref]), std::abs(YRHost[idx_cal])), 1.f);
            float errI = std::abs(YIRef[idx_ref] - YIHost[idx_cal]) / std::max(std::max(std::abs(YIRef[idx_ref]), std::abs(YIHost[idx_cal])), 1.f);
            if (errR > tol || errI > tol) {
                printf("<%d, %d>: (%f, %f) vs (%f, %f)\n",
                       b, i, YRRef[idx_ref], YIRef[idx_ref], YRHost[idx_cal], YIHost[idx_cal]);
                //exit(-1);
                return -1;
            }
        }
    }
    delete [] XRHost;
    delete [] XIHost;
    delete [] YRHost;
    delete [] YIHost;
    delete [] YRRef;
    delete [] YIRef;
    return 0;
}
int main(int argc, char *argv[]) {
    (void)(argc);
    (void)(argv);
    opencvFile_path = getenv("BMCV_TEST_FILE_PATH");
    if (opencvFile_path == NULL) {
        printf("please set environment vairable: BMCV_TEST_FILE_PATH !\n");
        return -1;
    }
    if (argc > 1)
        test(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
    else {
        for (int i = 0; i < 1; ++i) {
            struct timespec tp;
            #ifdef __linux__
            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
            #else
            clock_gettime_win(0, &tp);
            #endif
            srand(tp.tv_nsec);
            printf("test %d: random seed %ld\n", i, tp.tv_nsec);
            int ret = test(4, 10, true, false);
            if (ret) {
                printf("test fft_2d failed\n");
                return ret;
            }
        printf("Compare TPU result with OpenCV successfully!\n");
        }
    }
    return 0;
}