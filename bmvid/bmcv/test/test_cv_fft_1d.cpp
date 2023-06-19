#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "md5.h"
#include "string.h"
#include <iostream>
#include <cmath>
#ifdef __linux__
    #include <sys/time.h>
#else
    #include <windows.h>
#endif

using namespace std;

static std::string  unsignedCharToHex(unsigned char ch[16]){
    const char hex_chars[] = "0123456789abcdef";
    std::string result = "";
    for(int i = 0; i < 16; i++){
        unsigned int highHalfByte = (ch[i] >> 4) & 0x0f;
        unsigned int lowHalfByte = (ch[i] & 0x0f);
        result += hex_chars[highHalfByte];
        result += hex_chars[lowHalfByte];
    }
    return result;
}

static int cmpv2(unsigned char* got, unsigned char* exp,int L, int batch){
    unsigned char* md5_tpuOut = new unsigned char[16];
    md5_get(got, sizeof(float) * L * batch, md5_tpuOut);
    if(0 != strcmp((unsignedCharToHex(md5_tpuOut).c_str()), (const char*)exp)){
        std::cout << "cmp error!" << std::endl;
        return -1;
    }
    return 0;
}

static int test(int L, int batch, bool forward, bool realInput) {
    std::cout << "L = " << L << std::endl;
    float *XRHost = new float[L * batch];//4
    float *XIHost = new float[L * batch];
    float *YRHost = new float[L * batch];
    float *YIHost = new float[L * batch];
    int count = 0;
    for (int i = 0; i < L * batch; ++i) {
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
    BM_CHECK_RET(bm_malloc_device_byte(handle, &XRDev, L * batch * 4));
    BM_CHECK_RET(bm_malloc_device_byte(handle, &XIDev, L * batch * 4));
    BM_CHECK_RET(bm_malloc_device_byte(handle, &YRDev, L * batch * 4));
    BM_CHECK_RET(bm_malloc_device_byte(handle, &YIDev, L * batch * 4));
    BM_CHECK_RET(bm_memcpy_s2d(handle, XRDev, XRHost));
    BM_CHECK_RET(bm_memcpy_s2d(handle, XIDev, XIHost));
    void *plan = nullptr;
    BM_CHECK_RET(bmcv_fft_1d_create_plan(handle, batch, L, forward, plan));

    struct timeval t1, t2;
    gettimeofday_(&t1);
    if (realInput)
        BM_CHECK_RET(bmcv_fft_execute_real_input(handle, XRDev, YRDev, YIDev, plan));
    else
        BM_CHECK_RET(bmcv_fft_execute(handle, XRDev, XIDev, YRDev, YIDev, plan));
    gettimeofday_(&t2);
    std::cout << "fft 1d execute using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << std::endl;

    bmcv_fft_destroy_plan(handle, plan);
    BM_CHECK_RET(bm_memcpy_d2s(handle, YRHost, YRDev));
    BM_CHECK_RET(bm_memcpy_d2s(handle, YIHost, YIDev));
    bm_free_device(handle, XRDev);
    bm_free_device(handle, XIDev);
    bm_free_device(handle, YRDev);
    bm_free_device(handle, YIDev);
    bm_dev_free(handle);
    unsigned char YRRef_md5[] = "b6213405b118d574b271602c32f591cd";
    unsigned char YIRef_md5[] = "90d11e727f5b0c5afd85eeed43d2f479";
    if((0 != (cmpv2((unsigned char*)YRHost, YRRef_md5, L, batch)) || (0 != (cmpv2((unsigned char*)YIHost, YIRef_md5, L, batch))))){
        return -1;
    }
    delete [] XRHost;
    delete [] XIHost;
    delete [] YRHost;
    delete [] YIHost;
    return 0;
}
int main(int argc, char *argv[]) {
    (void)(argc);
    (void)(argv);
    if (argc > 1)
        test(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
    else {
        for (int i = 0; i < 1; ++i) {
            struct timespec tp;

            clock_gettime_(0, &tp);

            srand(tp.tv_nsec);
            std::cout << "test " << i << ": random seed: " << tp.tv_nsec << std::endl;
            int ret = test(4, 1, true, false);
            if(ret){
                std::cout << "test fft_1d failed" << std::endl;
                return ret;
            }
        std::cout << "Compare TPU result with OpenCV successfully" << std::endl;
        }
    }
    return 0;
}