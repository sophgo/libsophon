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
} crop_thread_arg_t;

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

void calculate_ref_float(int            c,
                         int            width,
                         int            height,
                         int            left,
                         int            top,
                         int            cropw,
                         int            croph,
                         int            stride_w,
                         void *         src_bgr,
                         unsigned char *ref_bgr) {
    float *b = (float *)src_bgr + top * width + left;
    float *g = b + width * height;
    float *r = g + width * height;

    unsigned char *cropb = ref_bgr;
    unsigned char *cropg = cropb + stride_w * croph;
    unsigned char *cropr = cropg + stride_w * croph;

    for (int i = 0; i < croph; i++) {
        for (int j = 0; j < cropw; j++) {
            cropb[j] = (unsigned char)lrint(b[j]);
        }
        if (c == 3) {
            for (int j = 0; j < cropw; j++) {
                cropg[j] = (unsigned char)lrint(g[j]);
            }
            for (int j = 0; j < cropw; j++) {
                cropr[j] = (unsigned char)lrint(r[j]);
            }
        }
        b += width;
        g += width;
        r += width;
        cropb += stride_w;
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

int compare_bgr_float(
    int c, int h, int w, int stride_w, unsigned char *ref_bgr, void *dst_bgr) {
    unsigned char *ref = (unsigned char *)ref_bgr;
    float *        dst = (float *)dst_bgr;

    for (int cloop = 0; cloop < c; cloop++) {
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                if (*ref != lrint(*dst)) {
                    printf("point in c: %d h: %d w: %d mismatch, expected: %x "
                           "calculated: %x\n",
                           cloop,
                           i,
                           j,
                           *ref,
                           (unsigned int)lrint(*dst));
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

void init_src(void *src, int h, int w, int type) {
    if (type == 1) {
        unsigned char *src_bgr = (unsigned char *)src;
        for (int i = 0; i < h * 3; i++) {
            for (int j = 0; j < w; j++) {
                src_bgr[i * w + j] = rand() % 255;
            }
        }
    } else {
        float *src_bgr = (float *)src;
        for (int i = 0; i < h * 3; i++) {
            for (int j = 0; j < w; j++) {
                src_bgr[i * w + j] = (float)(rand() % 255);
            }
        }
    }
}

#ifdef __linux__
void *test_crop_v2_thread(void *arg) {
#else
DWORD WINAPI test_crop_v2_thread(LPVOID arg) {
#endif
    crop_thread_arg_t *crop_thread_arg = (crop_thread_arg_t *)arg;
    int test_loop_times = crop_thread_arg->trials;
    #ifdef __linux__
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << pthread_self() << std::endl;
    #else
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << GetCurrentThreadId() << std::endl;
    #endif
    std::cout << "[TEST CROP v2] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    bm_handle_t    handle;
    bm_dev_request(&handle, 0);
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST CROP v2] LOOP " << loop_idx << "------"
                  << std::endl;

        // set parameter random
        bm_image_format_ext format_choice[5] = {FORMAT_BGR_PLANAR,
                                                FORMAT_RGB_PLANAR,
                                                FORMAT_BGR_PACKED,
                                                FORMAT_RGB_PACKED,
                                                FORMAT_GRAY};
        bm_image_format_ext image_format = format_choice[3];
        bm_image_data_format_ext data_type = DATA_TYPE_EXT_1N_BYTE;
        int h = 8 * 1024;
        int w = 8 * 1024;
        int c = image_format == FORMAT_GRAY ? 1 : 3;
        int sec_w = 16;
        int sec_h = 16;
        bm_status_t ret = BM_SUCCESS;
        int crop_num = sec_w * sec_h;
        bmcv_rect_t* rects = new bmcv_rect_t [crop_num];
        unsigned char **res_data = new unsigned char* [crop_num];
        unsigned char **ref_data = new unsigned char* [crop_num];
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
/*
        for (int i = 0; i < src_len; i++) {
            src_data[i] = rand() % 256;
        }
*/

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
        bm_image* output = new bm_image [crop_num];
        for (int i = 0; i < crop_num; i++) {
            bm_image_create(handle,
                            rects[i].crop_h,
                            rects[i].crop_w,
                            (bm_image_format_ext)image_format,
                            (bm_image_data_format_ext)data_type,
                            output + i);
            bm_image_alloc_dev_mem(output[i]);
        }

        // lunch tpu and get result
        struct timeval t1, t2;
        gettimeofday_(&t1);
        ret = bmcv_image_crop(handle, crop_num, rects, input, output);
        gettimeofday_(&t2);
        long used = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;
        printf("crop used time: %ld us\n", used);

        if (BM_SUCCESS != ret) {
            std::cout << "bmcv_crop error !!!" << std::endl;
            bm_image_destroy(input);
            for (int i = 0; i < crop_num; i++) {
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
        for (int i = 0; i < crop_num; i++) {
            bm_image_copy_device_to_host(output[i], (void **)&res_data[i]);
        }

        // calculate reference and compare with tpu result
        for (int i = 0; i < crop_num; i++) {
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
                printf("Error in result when crop %d image!\n", i);
                bm_image_destroy(input);
                for (int i = 0; i < crop_num; i++) {
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
        for (int i = 0; i < crop_num; i++) {
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
    std::cout << "------[TEST CROP v2] ALL TEST PASSED!" << std::endl;
    return NULL;
}

#ifdef __linux__
void *test_crop_thread(void *arg) {
#else
DWORD WINAPI test_crop_thread(LPVOID arg) {
#endif
    crop_thread_arg_t *crop_thread_arg = (crop_thread_arg_t *)arg;
    int                test_loop_times = crop_thread_arg->trials;
    #ifdef __linux__
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << pthread_self() << std::endl;
    #else
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << GetCurrentThreadId() << std::endl;
    #endif
    std::cout << "[TEST CROP] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    bm_handle_t    handle;
    bm_dev_request(&handle, 0);
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST CROP] LOOP " << loop_idx << "------"
                  << std::endl;
        int            w = 1920, h = 1080;
        void *         src_bgr  = 0;
        void *         dst_bgr  = 0;
        unsigned char *ref_bgr  = 0;
        int            loop_num = 40;

        printf("************      Test img crop     ****************\n");
        for (int loop = 0; loop < loop_num; loop++) {
            int top   = rand() % h;
            int left  = rand() % w;
            int cropw = w - left - rand() % (w - left);
            int croph = h - top - rand() % (h - top);
            int c     = (rand() % 100) > 50 ? 3 : 1;
            // int c        = 3;
            int stride_w = (cropw % 16) == 0 ? cropw : (cropw + 15) / 16 * 16;
            int input_format  = rand() % 2;
            int output_format = rand() % 2;

            input_format = input_format == 0 ? 1 : 4;
            // output_format = output_format==0?1:4;
            output_format = input_format;

            input_format = input_format == 0 ? 1 : 4;
            // output_format = output_format==0?1:4;
            output_format = input_format;
            left          = (left + stride_w <= w) ? (left) : (w - stride_w);
            printf("\ntesting loop %d: input %04dx%04d type %s channels %d "
                   "crop from left.top %04d.%04d %04dx%04d stride %04d output "
                   "format %s\n",
                   loop,
                   w,
                   h,
                   input_format == 1 ? "byte" : "float",
                   c,
                   left,
                   top,
                   cropw,
                   croph,
                   stride_w,
                   output_format == 1 ? "byte" : "float");
            if (input_format == 1) {
                src_bgr = malloc(w * h * 3);
            } else {
                src_bgr = malloc(w * h * 3 * 4);
            }
            if (src_bgr == 0) {
                printf("alloc test vector error, exit\n");
                exit(-1);
            }

            init_src(src_bgr, h, w, input_format);

            ref_bgr = new unsigned char[(stride_w * croph * c + 3) / 4 * 4];

            if (output_format == 1) {
                dst_bgr = malloc((stride_w * croph * c + 3) / 4 * 4);
            } else {
                dst_bgr = malloc(stride_w * croph * c * 4);
            }
            if (dst_bgr == 0 || ref_bgr == 0) {
                printf("alloc test vector error, exit!\n");
                free(src_bgr);
                exit(-1);
            }

            if (input_format == 1) {
                calculate_ref_i8(c,
                                 w,
                                 h,
                                 left,
                                 top,
                                 cropw,
                                 croph,
                                 stride_w,
                                 false,
                                 src_bgr,
                                 ref_bgr);
            } else {
                calculate_ref_float(c,
                                    w,
                                    h,
                                    left,
                                    top,
                                    cropw,
                                    croph,
                                    stride_w,
                                    src_bgr,
                                    ref_bgr);
            }

            bmcv_image input;
            input.color_space = COLOR_RGB;
            input.data_format =
                input_format == 1 ? DATA_TYPE_BYTE : DATA_TYPE_FLOAT;
            input.image_format = BGR;
            input.data[0]      = bm_mem_from_system(src_bgr);
            input.image_width  = w;
            input.image_height = h;
            input.stride[0]    = w;

            bmcv_image output;
            output.color_space = COLOR_RGB;
            output.data_format =
                output_format == 1 ? DATA_TYPE_BYTE : DATA_TYPE_FLOAT;
            output.image_format = BGR;
            output.data[0]      = bm_mem_from_system(dst_bgr);
            output.image_width  = cropw;
            output.image_height = croph;
            output.stride[0]    = stride_w;

            bmcv_img_crop(handle,
                          // input
                          input,
                          c,
                          top,
                          left,
                          // output
                          output);

            if (output_format == 1) {
                if (0 != compare_bgr_i8(
                             c, croph, cropw, stride_w, ref_bgr, dst_bgr)) {
                    printf("Error in result!\n");
                    exit(-1);
                } else {
                    printf("No error in result!\n");
                }
            } else {
                if (0 != compare_bgr_float(
                             c, croph, cropw, stride_w, ref_bgr, dst_bgr)) {
                    printf("Error in result!\n");
                    exit(-1);
                } else {
                    printf("No error in result!\n");
                }
            }

            free(dst_bgr);
            delete[] ref_bgr;
            free(src_bgr);
        }
    }
    bm_dev_free(handle);
    std::cout << "------[TEST CROP] ALL TEST PASSED!" << std::endl;
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
                     "order:test_crop loop_num multi_thread_num"
                  << std::endl;
        exit(-1);
    }
    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST CROP] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    if (test_threads_num > 4 || test_threads_num < 1) {
        std::cout << "[TEST CROP] thread nums should be 1~4 " << std::endl;
        exit(-1);
    }
    #ifdef __linux__
    pthread_t *        pid = new pthread_t[test_threads_num];
    crop_thread_arg_t *crop_thread_arg =
        new crop_thread_arg_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        crop_thread_arg[i].trials = test_loop_times;  // dev_id 0
        void* (*fp) (void*) = test_crop_v2_thread;
        if (pthread_create(
                &pid[i], NULL, fp, &crop_thread_arg[i])) {
            delete[] pid;
            delete[] crop_thread_arg;
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int ret = 0;
    for (int i = 0; i < test_threads_num; i++) {
        ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            delete[] pid;
            delete[] crop_thread_arg;
            perror("Thread join failed");
            exit(-1);
        }
    }
    #else
    #define THREAD_NUM 64
    DWORD  dwThreadIdArray[THREAD_NUM];
    HANDLE hThreadArray[THREAD_NUM];
    crop_thread_arg_t *crop_thread_arg = new crop_thread_arg_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        crop_thread_arg[i].trials = test_loop_times;
        //void *(*fp)(void *) = rand() % 2 ? test_crop_thread : test_crop_v2_thread;
        hThreadArray[i] =
            CreateThread(NULL,                  // default security attributes
                         0,                     // use default stack size
                         rand() % 2 ? test_crop_thread : test_crop_v2_thread,                    // thread function name
                         &crop_thread_arg[i],   // argument to thread function
                         0,                     // use default creation flags
                         &dwThreadIdArray[i]);  // returns the thread identifier
        if (hThreadArray[i] == NULL) {
            delete[] crop_thread_arg;
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int ret = 0;
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
        delete[] crop_thread_arg;
        printf("Thread join failed\n");
        return -1;
    }
    for (int i = 0; i < test_threads_num; i++)
        CloseHandle(hThreadArray[i]);
    #endif

    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    #ifdef __linux__
    delete[] pid;
    #endif
    delete[] crop_thread_arg;
    return 0;
}
