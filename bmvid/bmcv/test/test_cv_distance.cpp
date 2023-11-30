#include "bmcv_api_ext.h"
#include "bmlib_runtime.h"
#include <iostream>
#include <cmath>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#endif
#include "test_misc.h"

using namespace std;

bm_status_t test(int dim) {
    int L = 1024 * 1024;
    float pnt32[8] = {0};
    for (int i = 0; i < dim; ++i)
        pnt32[i] = (rand() % 2 ? 1.f : -1.f) * (rand() % 100 + (rand() % 100) * 0.01);
    float *XHost = new float[L * dim];
    for (int i = 0; i < L * dim; ++i)
        XHost[i] = (rand() % 2 ? 1.f : -1.f) * (rand() % 100 + (rand() % 100) * 0.01);
    float *YHost = new float[L];
    float *YRef = new float[L];

    int dsize = 4;
    int round = 1;
    float max_error = 1e-5;
    fp16 *XHost16 = new fp16[L * dim];
    fp16 *YHost16 = new fp16[L];
    fp16 *pnt16 = new fp16[8];
    data_type_t dtype = rand() % 2 ? DT_FP16 : DT_FP32; // DT_FP32 DT_FP16;
    bm_handle_t handle = nullptr;
    BM_CHECK_RET(bm_dev_request(&handle, 0));
    unsigned int chipid = 0x1686;
    BM_CHECK_RET(bm_get_chipid(handle, &chipid));
    if(chipid == 0x1684)
        dtype = DT_FP32;
    if (dtype == DT_FP16) {
        printf("Data type: DT_FP16\n");
        dsize = 2;
        max_error = 1e-3;
    } else {
        printf("Data type: DT_FP32\n");
    }

    bm_device_mem_t XDev, YDev;
    void* pnt;
    BM_CHECK_RET(bm_malloc_device_byte(handle, &XDev, L * dim * dsize));
    BM_CHECK_RET(bm_malloc_device_byte(handle, &YDev, L * dsize));
    if (dtype == DT_FP16) {
        for (int i = 0; i < L * dim; ++i)
            XHost16[i] = fp32tofp16(XHost[i], round);
        BM_CHECK_RET(bm_memcpy_s2d(handle, XDev, XHost16));
        for (int i = 0; i < dim; ++i){
            pnt16[i] = fp32tofp16(pnt32[i], round);
        }
        pnt = (void*)pnt16;
    } else {
        BM_CHECK_RET(bm_memcpy_s2d(handle, XDev, XHost));
        pnt = (void*)pnt32;
    }
    #ifdef __linux__
        struct timeval t1, t2;
        gettimeofday(&t1, NULL);
        BM_CHECK_RET(bmcv_distance_ext(handle,
                               XDev,
                               YDev,
                               dim,
                               pnt,
                               L,
                               dtype));
        gettimeofday(&t2, NULL);
        std::cout << "dim is " << dim << std::endl;
        std::cout << "distance TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << std::endl;
    #else
        struct timespec tp1, tp2;
        clock_gettime_win(0, &tp1);
        BM_CHECK_RET(bmcv_distance_ext(handle,
                               XDev,
                               YDev,
                               dim,
                               pnt,
                               L,
                               dtype));
        clock_gettime_win(0, &tp2);
        std::cout << "distance TPU using time: " << ((tp2.tv_sec - tp1.tv_sec) * 1000000 + (tp2.tv_nsec - tp1.tv_nsec)/1000) << "us" << std::endl;
    #endif
    if (dtype == DT_FP16)
        BM_CHECK_RET(bm_memcpy_d2s(handle, YHost16, YDev));
    else
        BM_CHECK_RET(bm_memcpy_d2s(handle, YHost, YDev));

    for (int i = 0; i < L; ++i) {
        YRef[i] = 0.f;
        for (int j = 0; j < dim; ++j) {
            if (dtype == DT_FP16) {
                XHost[i * dim + j] = fp16tofp32(XHost16[i * dim + j]);
                pnt32[j] = fp16tofp32(pnt16[j]);
            }
            YRef[i] += (XHost[i * dim + j] - pnt32[j]) * (XHost[i * dim + j] - pnt32[j]);
        }
        YRef[i] = std::sqrt(YRef[i]);
        if (dtype == DT_FP16){
            fp16 ff = fp32tofp16(YRef[i], round);
            YRef[i] = fp16tofp32(ff);
        }
    }

    for (int i = 0; i < L; ++i) {
        float err;
        if (dtype == DT_FP16)
            YHost[i] = fp16tofp32(YHost16[i]);
          #ifdef __linux__
          err = std::abs(YRef[i] - YHost[i]) / std::max(YRef[i], YHost[i]);
          #else
          err = std::abs(YRef[i] - YHost[i]) / (std::max)(YRef[i], YHost[i]);
          #endif
        if (err > max_error){
            std::cout << "error :" << YRef[i] << " vs " << YHost[i] << " at " << i << std::endl;
            break;
        }
    }
    delete [] XHost16;
    delete [] YHost16;
    delete [] pnt16;

    delete [] XHost;
    delete [] YHost;
    delete [] YRef;
    bm_free_device(handle, XDev);
    bm_free_device(handle, YDev);
    bm_dev_free(handle);
    return BM_SUCCESS;
}

int main(int argc, char *argv[]) {
    (void)(argc);
    (void)(argv);
    for (int i = 0; i < 8; ++i) {
        struct timespec tp;
        #ifdef __linux__
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
        #else
        clock_gettime_win(0, &tp);
        #endif
        srand(tp.tv_nsec);
        std::cout << "test " << i << ": random seed: " << tp.tv_nsec << std::endl;
        test(i + 1);
    }
    return 0;
}
