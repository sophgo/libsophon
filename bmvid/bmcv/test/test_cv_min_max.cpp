#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "bmlib_runtime.h"
#include <iostream>
#include <cmath>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#endif
#define ONLY_MIN (0)
#define ONLY_MAX (1)
#define BOTH     (2)
bm_status_t test() {
    int L = 1024 * 1024;
    float *XHost = new float[L];
    for (int i = 0; i < L; ++i)
        XHost[i] = (rand() % 2 ? 1.f : -1.f) * (rand() % 1000 + (rand() % 100000) * 0.01);
    bm_handle_t handle = nullptr;
    BM_CHECK_RET(bm_dev_request(&handle, 0));
    bm_device_mem_t XDev;
    BM_CHECK_RET(bm_malloc_device_byte(handle, &XDev, L * 4));
    BM_CHECK_RET(bm_memcpy_s2d(handle, XDev, XHost));
    float minVal, maxVal;

    struct timeval t1, t2;
    gettimeofday_(&t1);
    BM_CHECK_RET(bmcv_min_max(handle,
                            XDev,
                            &minVal,
                            &maxVal,
                            L));
    gettimeofday_(&t2);
    std::cout << "min max TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << std::endl;

    float minRef, maxRef;
    maxRef = XHost[0];
    for (int i = 1; i < L; ++i)
        maxRef = maxRef > XHost[i] ? maxRef : XHost[i];
    std::cout << "MAX: " << maxRef << " vs " << maxVal << std::endl;
    minRef = XHost[0];
    for (int i = 1; i < L; ++i)
        minRef = minRef < XHost[i] ? minRef : XHost[i];
    std::cout << "MIN: " << minRef << " vs " << minVal << std::endl;
    delete [] XHost;
    bm_free_device(handle, XDev);
    bm_dev_free(handle);
    return BM_SUCCESS;
}
int main(int argc, char *argv[]) {
    (void)(argc);
    (void)(argv);
    for (int i = 0; i < 1; ++i) {
        struct timespec tp;
        clock_gettime_(0, &tp);
        srand(tp.tv_nsec);
        std::cout << "test " << i << ": random seed: " << tp.tv_nsec << std::endl;
        test();
    }
    std::cout << "test done." << std::endl;
    return 0;
}
