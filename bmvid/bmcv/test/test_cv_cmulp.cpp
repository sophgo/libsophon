#include "bmcv_api_ext.h"
#include "bmlib_runtime.h"
#include <iostream>
#include <cmath>
#ifdef __linux__
#include <sys/time.h>
#else
#include "time.h"
#endif
#include "test_misc.h"

using namespace std;

static inline void cmul(float a, float b, float c, float d, float &x, float &y) {
    x = a * c - b * d;
    y = a * d + b * c;
}
bm_status_t test() {
    int L = 5;
    int batch = 2;
    float *XRHost = new float[L * batch];
    float *XIHost = new float[L * batch];
    float *PRHost = new float[L];
    float *PIHost = new float[L];
    for (int i = 0; i < L * batch; ++i) {
        XRHost[i] = rand() % 5 - 2;
        XIHost[i] = rand() % 5 - 2;
    }
    for (int i = 0; i < L; ++i) {
        PRHost[i] = rand() % 5 - 2;
        PIHost[i] = rand() % 5 - 2;
    }
    float *YRHost = new float[L * batch];
    float *YIHost = new float[L * batch];
    float *YRRef = new float[L * batch];
    float *YIRef = new float[L * batch];
    bm_handle_t handle = nullptr;
    BM_CHECK_RET(bm_dev_request(&handle, 0));
    bm_device_mem_t XRDev, XIDev, PRDev, PIDev, YRDev, YIDev;
    BM_CHECK_RET(bm_malloc_device_byte(handle, &XRDev, L * batch * 4));
    BM_CHECK_RET(bm_malloc_device_byte(handle, &XIDev, L * batch * 4));
    BM_CHECK_RET(bm_malloc_device_byte(handle, &PRDev, L * 4));
    BM_CHECK_RET(bm_malloc_device_byte(handle, &PIDev, L * 4));
    BM_CHECK_RET(bm_malloc_device_byte(handle, &YRDev, L * batch * 4));
    BM_CHECK_RET(bm_malloc_device_byte(handle, &YIDev, L * batch * 4));
    BM_CHECK_RET(bm_memcpy_s2d(handle, XRDev, XRHost));
    BM_CHECK_RET(bm_memcpy_s2d(handle, XIDev, XIHost));
    BM_CHECK_RET(bm_memcpy_s2d(handle, PRDev, PRHost));
    BM_CHECK_RET(bm_memcpy_s2d(handle, PIDev, PIHost));
    #ifdef __linux__
        struct timeval t1, t2;
        gettimeofday(&t1, NULL);
        BM_CHECK_RET(bmcv_cmulp(handle,
                            XRDev,
                            XIDev,
                            PRDev,
                            PIDev,
                            YRDev,
                            YIDev,
                            batch,
                            L));
        gettimeofday(&t2, NULL);
        std::cout << "cmulp TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << std::endl;
    #else
        struct timespec tp1, tp2;
        clock_gettime_win(0, &tp1);
        BM_CHECK_RET(bmcv_cmulp(handle,
                            XRDev,
                            XIDev,
                            PRDev,
                            PIDev,
                            YRDev,
                            YIDev,
                            batch,
                            L));
        clock_gettime_win(0, &tp2);
        std::cout << "cmulp TPU using time: " << ((tp2.tv_sec - tp1.tv_sec) * 1000000 + (tp2.tv_nsec - tp1.tv_nsec)/1000) << "us" << std::endl;
    #endif
    BM_CHECK_RET(bm_memcpy_d2s(handle, YRHost, YRDev));
    BM_CHECK_RET(bm_memcpy_d2s(handle, YIHost, YIDev));
    for (int b = 0; b < batch; ++b)
        for (int i = 0; i < L; ++i)
            cmul(XRHost[b * L + i], XIHost[b * L + i], PRHost[i], PIHost[i], YRRef[b * L + i], YIRef[b * L + i]);
    const float tol = 1e-5;
    for (int b = 0; b < batch; ++b) {
        for (int i = 0; i < L; ++i) {
            int idx_ref = b * L + i;
            int idx_cal = idx_ref;
            float errR = std::abs(YRRef[idx_ref] - YRHost[idx_cal]) / std::max(std::max(std::abs(YRRef[idx_ref]), std::abs(YRHost[idx_cal])), 1.f);
            float errI = std::abs(YIRef[idx_ref] - YIHost[idx_cal]) / std::max(std::max(std::abs(YIRef[idx_ref]), std::abs(YIHost[idx_cal])), 1.f);
            if (errR > tol || errI > tol) {
                std::cout << "<" << b << ", " << i << ">: ";
                std::cout << "(" << YRRef[idx_ref] << ", " << YIRef[idx_ref] << ") vs ";
                std::cout << "(" << YRHost[idx_cal] << ", " << YIHost[idx_cal] << ")";
                std::cout << std::endl;
                //exit(-1);
            }
        }
    }
    delete [] XRHost;
    delete [] XIHost;
    delete [] PRHost;
    delete [] PIHost;
    delete [] YRHost;
    delete [] YIHost;
    delete [] YRRef;
    delete [] YIRef;
    bm_free_device(handle, XRDev);
    bm_free_device(handle, XIDev);
    bm_free_device(handle, YRDev);
    bm_free_device(handle, YIDev);
    bm_free_device(handle, PRDev);
    bm_free_device(handle, PIDev);
    bm_dev_free(handle);
    return BM_SUCCESS;
}
int main(int argc, char *argv[]) {
    (void)(argc);
    (void)(argv);
    for (int i = 0; i < 1; ++i) {
        struct timespec tp;
        #ifdef __linux__
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
        #else
        clock_gettime_win(0, &tp);
        #endif
        srand(tp.tv_nsec);
        std::cout << "test " << i << ": random seed: " << tp.tv_nsec << std::endl;
        test();
    }
    return 0;
}
