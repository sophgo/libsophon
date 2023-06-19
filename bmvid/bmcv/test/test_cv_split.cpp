#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <math.h>
#include <string.h>
#include <vector>
#include "bmcv_api.h"
#include "test_misc.h"
#ifdef __linux__
  #include <sys/time.h>
#else
  #include <windows.h>
#endif

typedef struct {
    int trials;
} split_thread_arg_t;

using namespace std;

void calculate_ref_i8(int            c,
                      int            width,
                      int            height,
                      int            left,
                      int            top,
                      int            cropw,
                      int            croph,
                      int            stride_w,
                      bool           is_packed,
                      void *         src_bgr,
                      unsigned char *ref_bgr) {
    int bytes = is_packed ? 3 : 1;
    c = is_packed ? 1 : c;
    unsigned char *b = (unsigned char *)src_bgr + top * width * bytes + left * bytes;
    unsigned char *g = b + width * height;
    unsigned char *r = g + width * height;

    unsigned char *cropb = ref_bgr;
    unsigned char *cropg = cropb + stride_w * croph;
    unsigned char *cropr = cropg + stride_w * croph;

    for (int i = 0; i < croph; i++) {
        memcpy(cropb, b, cropw * bytes);
        if (c == 3) {
            memcpy(cropg, g, cropw);
            memcpy(cropr, r, cropw);
        }
        b += width * bytes;
        g += width;
        r += width;
        cropb += stride_w * bytes;
        cropg += stride_w;
        cropr += stride_w;
    }
}

int compare_bgr_i8(
    int c, int h, int w, int stride_w, unsigned char *ref_bgr, void *dst_bgr) {
    unsigned char *ref = (unsigned char *)ref_bgr;
    unsigned char *dst = (unsigned char *)dst_bgr;

    for (int cloop = 0; cloop < c; cloop++) {
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                if (*ref != *dst) {
                    printf("point in c: %d h: %d w: %d mismatch, expected: %x "
                           "calculated: %x\n",
                           cloop,
                           i,
                           j,
                           *ref,
                           *dst);
                    return -1;
                }
                ref++;
                dst++;
            }
            ref += (stride_w - w);
            dst += (stride_w - w);
        }
    }
    return 0;
}

#ifdef __linux__
void *test_split_thread(void *arg) {
#else
DWORD WINAPI test_split_thread(LPVOID arg) {
#endif
    split_thread_arg_t *split_thread_arg = (split_thread_arg_t *)arg;
    int test_loop_times = split_thread_arg->trials;
    #ifdef __linux__
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << pthread_self() << std::endl;
    #else
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << GetCurrentThreadId() << std::endl;
    #endif
    std::cout << "[TEST SPLIT] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    bm_handle_t    handle;
    bm_dev_request(&handle, 0);
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST SPLIT] LOOP " << loop_idx << "------"
                  << std::endl;

        // set parameter random
        bm_image_format_ext image_format = FORMAT_BGR_PACKED;
        bm_image_data_format_ext data_type = DATA_TYPE_EXT_1N_BYTE_SIGNED;
        int h = 12 * 1024;
        int w = 16 * 1024;
        int c = 3;
        int sec_w = w / 256;
        int sec_h = h / 256;
        int split_num = sec_w * sec_h;
        bmcv_rect_t* rects = new bmcv_rect_t [split_num];
        unsigned char **res_data = new unsigned char* [split_num];
        unsigned char **ref_data = new unsigned char* [split_num];
        for (int i = 0; i < sec_h; i++) {
            for (int j = 0; j < sec_w; j++) {
                rects[i * sec_w + j].start_x = 256 * j;
                rects[i * sec_w + j].start_y = 256 * i;
                rects[i * sec_w + j].crop_h = 256;
                rects[i * sec_w + j].crop_w = 256;
                res_data[i * sec_w + j] = new unsigned char [rects[i * sec_w + j].crop_h * rects[i * sec_w + j].crop_w * c];
                ref_data[i * sec_w + j] = new unsigned char [rects[i * sec_w + j].crop_h * rects[i * sec_w + j].crop_w * c];
            }
        }
        int src_len = h * w * c;
        unsigned char *src_data = new unsigned char [src_len];

        for (int i = 0; i < src_len; i++) {
            src_data[i] = rand() % 256;
        }

        // create input image
        bm_image input;
        bm_image_create(handle,
                        h,
                        w,
                        (bm_image_format_ext)image_format,
                        (bm_image_data_format_ext)data_type,
                        &input);
        bm_image_alloc_dev_mem(input);
        bm_image_copy_host_to_device(input, (void **)&src_data);

        // create output image
        bm_image* output = new bm_image [split_num];
        for (int i = 0; i < split_num; i++) {
            bm_image_create(handle,
                            rects[i].crop_h,
                            rects[i].crop_w,
                            (bm_image_format_ext)image_format,
                            (bm_image_data_format_ext)data_type,
                            output + i);
        }
        bm_image_alloc_contiguous_mem(split_num, output);

        // lunch tpu and get result
        struct timeval t1, t2;
        gettimeofday_(&t1);
        bm_status_t ret = bmcv_image_split(handle, input, output);
        gettimeofday_(&t2);
        long used = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;
        printf("split used time: %ld us\n", used);

        if (BM_SUCCESS != ret) {
            std::cout << "bmcv_split error !!!" << std::endl;
            bm_image_destroy(input);
            for (int i = 0; i < split_num; i++) {
                bm_image_destroy(output[i]);
                delete [] res_data[i];
                delete [] ref_data[i];
            }
            delete [] rects;
            delete [] src_data;
            delete [] res_data;
            delete [] ref_data;
            delete [] output;
            bm_dev_free(handle);
            exit(-1);
        }
        for (int i = 0; i < split_num; i++) {
            bm_image_copy_device_to_host(output[i], (void **)&res_data[i]);
        }

        // calculate reference and compare with tpu result
        for (int i = 0; i < split_num; i++) {
            calculate_ref_i8(c,
                             w,
                             h,
                             rects[i].start_x,
                             rects[i].start_y,
                             rects[i].crop_w,
                             rects[i].crop_h,
                             rects[i].crop_w,
                             (image_format == FORMAT_BGR_PACKED ||
                              image_format == FORMAT_RGB_PACKED),
                             src_data,
                             ref_data[i]);
            if (0 != compare_bgr_i8(c,
                                    rects[i].crop_h,
                                    rects[i].crop_w,
                                    rects[i].crop_w,
                                    ref_data[i],
                                    res_data[i])) {
                printf("Error in result when split %d image!\n", i);
                bm_image_destroy(input);
                for (int i = 0; i < split_num; i++) {
                    bm_image_destroy(output[i]);
                    delete [] res_data[i];
                    delete [] ref_data[i];
                }
                delete [] rects;
                delete [] src_data;
                delete [] res_data;
                delete [] ref_data;
                delete [] output;
                bm_dev_free(handle);
                exit(-1);
            }
        }

        bm_image_destroy(input);
        bm_image_free_contiguous_mem(split_num, output);
        for (int i = 0; i < split_num; i++) {
            bm_image_destroy(output[i]);
            delete [] res_data[i];
            delete [] ref_data[i];
        }
        delete [] rects;
        delete [] src_data;
        delete [] res_data;
        delete [] ref_data;
        delete [] output;
    }
    bm_dev_free(handle);
    std::cout << "------[TEST SPLIT] ALL TEST PASSED!" << std::endl;
    return NULL;
}

int main(int argc, char *argv[]) {
    unsigned int seed = (unsigned)time(NULL);
    std::cout << "seed: " << seed << std::endl;
    srand(seed);
    int test_loop_times  = 0;
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
                     "order:test_split loop_num multi_thread_num"
                  << std::endl;
        exit(-1);
    }
    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST SPLIT] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    if (test_threads_num > 4 || test_threads_num < 1) {
        std::cout << "[TEST SPLIT] thread nums should be 1~4 " << std::endl;
        exit(-1);
    }
    #ifdef __linux__
    pthread_t *        pid = new pthread_t[test_threads_num];
    split_thread_arg_t *split_thread_arg =
        new split_thread_arg_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        split_thread_arg[i].trials = test_loop_times;  // dev_id 0
        if (pthread_create(
                &pid[i], NULL, test_split_thread, &split_thread_arg[i])) {
            delete[] pid;
            delete[] split_thread_arg;
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int ret = 0;
    for (int i = 0; i < test_threads_num; i++) {
        ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            delete[] pid;
            delete[] split_thread_arg;
            perror("Thread join failed");
            exit(-1);
        }
    }
    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    delete[] pid;
    delete[] split_thread_arg;
    #else
    #define THREAD_NUM 64
    DWORD              dwThreadIdArray[THREAD_NUM];
    HANDLE             hThreadArray[THREAD_NUM];
    split_thread_arg_t *split_thread_arg =
        new split_thread_arg_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        split_thread_arg[i].trials = test_loop_times;
        hThreadArray[i] = CreateThread(
            NULL,                  // default security attributes
            0,                     // use default stack size
            test_split_thread,     // thread function name
            &split_thread_arg[i],  // argument to thread function
            0,                     // use default creation flags
            &dwThreadIdArray[i]);  // returns the thread identifier
        if (hThreadArray[i] == NULL) {
            delete[] split_thread_arg;
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
        delete[] split_thread_arg;
        printf("Thread join failed\n");
        return -1;
    }
    for (int i = 0; i < test_threads_num; i++)
        CloseHandle(hThreadArray[i]);
    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    delete[] split_thread_arg;
    #endif
    return 0;
}
