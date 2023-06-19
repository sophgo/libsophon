#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "bmcv_api.h"
#include "bmlib_runtime.h"

#ifdef __linux__
  #include <unistd.h>
  #include <pthread.h>
  #include <sys/ioctl.h>
  #include <sys/types.h>
  #include <sys/time.h>
#else
  #include <windows.h>
#endif

#include "test_misc.h"
#include "bmcv_api_ext.h"
#include <time.h>

using namespace std;

double tt0;
double tt1;
#ifndef SOC_MODE
struct ce_base {
    uint64_t     src;
    uint64_t     dst;
    uint64_t     len;
    bool direction;
};
#endif

char const *base64std =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "+/=";

char const *base64url =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "-_=";



static int test_check_result_base(unsigned char *spacc, unsigned char *software, int len) {
// char may lead to problems here, have a look later
    int res = 0;
        int i;

    for (i = 0; i < len; ++i) {
        if (spacc[i] != software[i]) {
            printf(
        "%dth byte not match, golden is [%02x] but test is [%02x]\n"
                   , i, software[i], spacc[i]);
            res |= 1;
            break;
        }
    }
    printf("the test %s\n", res?"FAIL":"PASS");
    return res;
}

static int base64_enc_core(void *dst, void *src, char *box) {
    // convert 3x8bits to 4x6bits
    unsigned char *d, *s;
        d = (unsigned char*)dst;
        s = (unsigned char*)src;
    d[0] = box[(s[0] >> 2) & 0x3f];
    d[1] = box[((s[0] & 0x3) << 4) | (s[1] >> 4)];
    d[2] = box[((s[1] & 0xf) << 2) | (s[2] >> 6)];
    d[3] = box[(s[2] & 0x3f)];
    return 0;
}

static int base64_enc_left(void *dst, void *src, int size, char *box) {
    // convert 3x8bits to 4x6bits
    unsigned char *d, s[3];
    memset(s, 0x00, sizeof(s));
    memcpy(s, src, size);
        d = (unsigned char *)dst;
    d[0] = box[(s[0] >> 2) & 0x3f];
    d[1] = box[((s[0] & 0x3) << 4) | (s[1] >> 4)];
    d[2] = box[((s[1] & 0xf) << 2) | (s[2] >> 6)];
    d[3] = box[64];
    if ( size == 1 )
        d[2] = d[3];
    return 0;
}

static int base64_encode(void *dst, void *src, int size, char *box) {
    unsigned long block = size / 3;
    unsigned long left = size % 3;
        unsigned char *d = (unsigned char *)dst;
        unsigned char *s = (unsigned char *)src;
        unsigned i;
    for (i = 0; i < block; ++i) {
        base64_enc_core(d, s, box);
        d += 4;
        s += 3;
    }
    if (left)
        base64_enc_left(d, s, left, box);
    return 0;
}

static int base64_dec_core(void *dst, void *src, unsigned char *box) {
    unsigned char *d, *s;
    unsigned char t[4];
        d = (unsigned char *)dst;
        s = (unsigned char *)src;
    t[0] = box[s[0]];
    t[1] = box[s[1]];
    t[2] = box[s[2]];
    t[3] = box[s[3]];

    d[0] = (t[0] << 2) | (t[1] >> 4);
    d[1] = (t[1] << 4) | (t[2] >> 2);
    d[2] = (t[2] << 6) | (t[3]);

    return 0;
}

static int base64_box_t(unsigned char box_t[256], char *box) {
        int i;

        memset(box_t, 0x00, 256);
        for (i = 0; i < 64; ++i) {
            box_t[(unsigned int)box[i]] = i;
        }
        box_t[64] = 0;
        return 0;
}

int base64_decode(void *dst, void *src, unsigned long size, char *box) {
        unsigned char *d = (unsigned char *)dst;
        char *s = (char *)src;
    assert(size % 4 == 0);
    unsigned char box_t[256];
    base64_box_t(box_t, box);
    unsigned long block = size / 4;
    unsigned long i;
    for (i = 0; i < block; ++i) {
        base64_dec_core(d, s, box_t);
        d += 3;
        s += 4;
    }
    return 0;
}

#ifndef USING_CMODEL
/********************************************************/
/*len here is the number of unsigned long                                   */
/*b is the pointer of the result of the encoding, put it external just*/
/*for easier being made use of the decoder test                          */
/********************************************************/
static int test_base64_enc(int len, int dst_len, char *src, char *dst) {
    bm_status_t ret;
    unsigned char* dst_soft;
    bm_handle_t handle;
    unsigned long lenth[2];

    lenth[0] = (unsigned long)len;
    dst_soft = (unsigned char*)malloc(dst_len + 3);
    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        exit(-1);
    }
    struct timeval t1, t2;
    gettimeofday_(&t1);
    bmcv_base64_enc(handle, bm_mem_from_system(src), bm_mem_from_system(dst), lenth);
    gettimeofday_(&t2);
    cout << "bmcv excution time is: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec)<< "us" << endl;
    gettimeofday_(&t1);
    base64_encode(dst_soft, src, len, (char*)base64std);
    gettimeofday_(&t2);
    cout << "soft excution time is: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;
    //tt0 = (double)cv::getTickCount();
    //bmcv_base64_enc(handle, bm_mem_from_system(src), bm_mem_from_system(dst), lenth);
    //tt1 = (double)cv::getTickCount() - tt0;
    //printf("bmcv excution time is %gms\n", tt1 * 1000. / cv::getTickFrequency());

    //tt0 = (double)cv::getTickCount();
    //base64_encode(dst_soft, src, len, (char *)base64std);
    //tt1 = (double)cv::getTickCount() - tt0;
    //printf("soft excution time is %gms\n", tt1 * 1000. / cv::getTickFrequency());
    test_check_result_base((unsigned char *)dst,
        (unsigned char *)dst_soft, dst_len);
    free(dst_soft);
    bm_dev_free(handle);

    return 0;
}

static int test_base64_dec(int len, int dst_len, char *src, char *dst) {
    bm_status_t ret;
    unsigned char* dst_soft;
    bm_handle_t handle;
    unsigned long lenth[2];

    lenth[0] = (unsigned long)len;
    dst_soft = (unsigned char*)malloc(dst_len + 3);
    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        exit(-1);
    }
    struct timeval t1, t2;
    gettimeofday_(&t1);
    bmcv_base64_dec(handle, bm_mem_from_system(src), bm_mem_from_system(dst), lenth);
    gettimeofday_(&t2);
    cout << "bmcv excution time is: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;
    gettimeofday_(&t1);
    base64_decode(dst_soft, src, len, (char*)base64std);
    gettimeofday_(&t2);
    cout << "soft excution time is: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

    //tt0 = (double)cv::getTickCount();
    //bmcv_base64_dec(handle, bm_mem_from_system(src), bm_mem_from_system(dst), lenth);
    //printf("decode len is %d\n", (int)lenth[1]);
    //tt1 = (double)cv::getTickCount() - tt0;
    //printf("bmcv excution time is %gms\n", tt1 * 1000. / cv::getTickFrequency());

    //tt0 = (double)cv::getTickCount();
    //base64_decode(dst_soft, src, len, (char *)base64std);
    //tt1 = (double)cv::getTickCount() - tt0;
    //printf("soft excution time is %gms\n", tt1 * 1000. / cv::getTickFrequency());
    test_check_result_base((unsigned char *)dst,
        (unsigned char *)dst_soft, dst_len);
    free(dst_soft);
    bm_dev_free(handle);

    return 0;
}

#ifdef __linux__
static void *test_base64_thread(void *arg) {
#else
DWORD WINAPI test_base64_thread(LPVOID arg) {
#endif
    char *a;
    char *b;
    int i;
    int j;
    int loop_times;
    int original_len;
    int encoded_len;
    int ret = 0;
    unsigned int seed;

    loop_times = *(int *)arg;

    seed = (unsigned)time(NULL);
    std::cout << "seed: " << seed << std::endl;
    srand(seed);

    for (i = 0; i <loop_times; i++) {
        original_len = (rand() % 134217728) + 1;
        encoded_len = (original_len + 2) / 3 * 4;
        a = (char *)malloc((original_len + 3) * sizeof(char));
        b = (char *)malloc((encoded_len + 3) * sizeof(char));
        printf("=======>original=%d,encoded=%d\n",original_len,encoded_len);
        for (j = 0; j < original_len; j++)
            a[j] = (char)((rand() % 100) + 1);

        ret |= test_base64_enc(original_len, encoded_len,
            a, b);
        ret |= test_base64_dec(encoded_len, original_len,
            b, a);

        free(a);
        free(b);
    }

    if (ret == 0) {
        printf("all test pass of one thread\n");
    } else {
        printf("something not correct!\n");
    }

    return 0;
}
#endif
int main(int32_t argc, char **argv) {
#ifndef USING_CMODEL
    int test_loop_times  = 1;
    int test_threads_num = 1;
    if (argc == 1) {
        test_loop_times  = 1;
        test_threads_num = 1;
    } else if (argc == 2) {
        test_loop_times  = atoi(argv[1]);
        test_threads_num = 1;
    } else if (argc == 3) {
        test_loop_times  = atoi(argv[1]);
        test_threads_num = atoi(argv[2]);
    } else {
        std::cout << "command input error, please follow this "
                     "order:test_resize loop_num multi_thread_num"
                  << std::endl;
        exit(-1);
    }
    if (test_loop_times > 1000000 || test_loop_times < 1) {
        std::cout << "[TEST NMS] loop times should be 1~1000000" << std::endl;
        exit(-1);
    }
    if (test_threads_num > 4 || test_threads_num < 1) {
        std::cout << "[TEST NMS] thread nums should be 1~4 " << std::endl;
        exit(-1);
    }
    #ifdef __linux__
    pthread_t * pid = new pthread_t[test_threads_num];

    for (int i = 0; i < test_threads_num; i++) {
        if (pthread_create(
            &pid[i], NULL, test_base64_thread, (void *)(&test_loop_times))) {
            delete[] pid;
            printf("create thread failed\n");
            exit(-1);
        }
    }
    int ret = 0;
    for (int i = 0; i < test_threads_num; i++) {
        ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            delete[] pid;
            printf("Thread join failed");
            exit(-1);
        }
    }
    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    delete[] pid;
    #else
    #define THREAD_NUM 64
    DWORD              dwThreadIdArray[THREAD_NUM];
    HANDLE             hThreadArray[THREAD_NUM];
    for (int i = 0; i < test_threads_num; i++) {
        hThreadArray[i] = CreateThread(
            NULL,                        // default security attributes
            0,                           // use default stack size
            test_base64_thread,          // thread function name
            (void *)(&test_loop_times),  // argument to thread function
            0,                           // use default creation flags
            &dwThreadIdArray[i]);        // returns the thread identifier
        if (hThreadArray[i] == NULL) {
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int   ret = 0;
    DWORD dwWaitResult =
        WaitForMultipleObjects(test_threads_num, hThreadArray, TRUE, INFINITE);
    // DWORD dwWaitResult = WaitForSingleObject(hThreadArray[i], INFINITE);
    switch (dwWaitResult) {
        // case WAIT_OBJECT_0:
        //    ret = 0;
        //    break;
        case WAIT_FAILED:
            ret = -1;
            break;
        case WAIT_ABANDONED:
            ret = -2;
            break;
        case WAIT_TIMEOUT:
            ret = -3;
            break;
        default:
            ret = 0;
            break;
    }
    if (ret < 0) {
        printf("Thread join failed\n");
        return -1;
    }
    for (int i = 0; i < test_threads_num; i++)
        CloseHandle(hThreadArray[i]);
    #endif

    return 0;

#else
    UNUSED(argc);
    UNUSED(argv);
    printf("CMODEL not supported");
#endif
}

