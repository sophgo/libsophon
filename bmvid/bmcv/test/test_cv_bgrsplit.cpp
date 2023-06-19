#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <math.h>
#include <string.h>
#include <time.h>
#include <vector>
#include "bmcv_api.h"

#ifdef __linux__
  #include <sys/time.h>
#endif

using namespace std;

static bm_handle_t handle;

#define AVE_TIMES (1)
#define DTYPE_F32 0
#define DTYPE_I8 4
#define CMP_EPS_INT 1
#define CMP_EPS_FP 1e-4f
#define PERF_MEASURE_EN                                                        \
    0  // if we want to measure performance of transpose and bgrsplit, we need
       // enable here and another definition in bmdnn/src/bmdnn_api_permute.cpp

int test_split_main(int n, int c, int h, int w, int in_type, int out_type) {
    void *src = 0;
    void *dst = 0;
    void *ref = 0;

    if(c != 3){
        return -1;
    };

    int total_data_num = w * h * c * n;
    if (in_type == DTYPE_F32)
        src = malloc(total_data_num * sizeof(float));
    else
        src = malloc((total_data_num + 3) / 4 * 4);

    if (out_type == DTYPE_F32)
        dst = malloc(total_data_num * sizeof(float));
    else
        dst = malloc((total_data_num + 3) / 4 * 4);

    if (out_type == DTYPE_F32)
        ref = malloc(total_data_num * sizeof(float));
    else
        ref = malloc((total_data_num + 3) / 4 * 4);

    // generate random input
    for (int i = 0; i < total_data_num; i++) {
        if (in_type == DTYPE_F32) {
            float *src_ = (float *)src;
            src_[i]     = rand() % 255 * 3.14159265f;
            while (src_[i] > 255) {
                src_[i] = src_[i] * 0.618f;
            };
        } else {
            unsigned char *src_ = (unsigned char *)src;
            src_[i]             = rand() % 255;
        }
    }

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < h; j++) {
            for (int k = 0; k < w; k++) {
                if (out_type == DTYPE_F32) {
                    float *o = (float *)ref;
                    if (in_type == DTYPE_F32) {
                        float *src_              = (float *)src;
                        o[i * h * w + j * w + k] = src_[j * 3 * w + k * 3 + i];
                    } else {
                        unsigned char *src_ = (unsigned char *)src;
                        o[i * h * w + j * w + k] =
                            (float)src_[j * 3 * w + k * 3 + i];
                    }
                } else {
                    unsigned char *o = (unsigned char *)ref;
                    if (in_type == DTYPE_F32) {
                        float *src_ = (float *)src;
                        o[i * h * w + j * w + k] =
                            lrint(src_[j * 3 * w + k * 3 + i]);
                    } else {
                        unsigned char *src_      = (unsigned char *)src;
                        o[i * h * w + j * w + k] = src_[j * 3 * w + k * 3 + i];
                    }
                }
            }
        }
    }

    bmcv_image input;
    input.color_space = COLOR_RGB;
    input.data_format = in_type == DTYPE_F32 ? DATA_TYPE_FLOAT : DATA_TYPE_BYTE;
    input.image_format = RGB_PACKED;
    input.image_width  = w;
    input.image_height = h;
    int in_data_size = (input.data_format == DATA_TYPE_FLOAT ? 4 : 1);

    bm_device_mem_t input_mem;
    bm_device_mem_t output_mem;
    if(BM_SUCCESS != bm_malloc_device_byte(handle, &input_mem, 3 * input.image_height* input.image_width * in_data_size)){
        printf("bm_malloc_device_byte error\r\n");
        return -1;
    }
    if(BM_SUCCESS != bm_memcpy_s2d(handle, input_mem, (void*)src)){
        printf("bm_memcpy_s2d error\r\n");
        bm_free_device(handle, input_mem);
        return -1;
    }
    input.data[0]      = input_mem;
    input.stride[0]    = w;

    bmcv_image output   = input;
    output.image_format = RGB;
    output.data_format =
        out_type == DTYPE_F32 ? DATA_TYPE_FLOAT : DATA_TYPE_BYTE;
    int output_data_size = (output.data_format == DATA_TYPE_FLOAT ? 4 : 1);
    if(BM_SUCCESS != bm_malloc_device_byte(handle, &output_mem, 3 * output.image_height* output.image_width * output_data_size)){
        printf("bm_malloc_device_byte error\r\n");
        bm_free_device(handle, input_mem);
        return -1;
    }
    output.data[0] = output_mem;
    output.stride[0] = w;

#if !PERF_MEASURE_EN
    bmcv_img_bgrsplit(handle, input, output);
#else
    for (int i = 0; i < AVE_TIMES; i++)
        bmcv_img_bgrsplit(handle, input, output);
#endif
    if(BM_SUCCESS != bm_memcpy_d2s(handle, (void*)dst, output_mem)) {
        printf("bm_memcpy_d2s error\r\n");
        bm_free_device(handle, input_mem);
        bm_free_device(handle, output_mem);
        return -1;
    }

    for (int i = 0; i < n * c * h * w; i++) {
        if (out_type == DTYPE_F32) {
            float *ref_ = (float *)ref;
            float *dst_ = (float *)dst;
            if (fabs(dst_[i] - ref_[i]) >= CMP_EPS_FP) {
                printf("compre of %d mismatch exp %f got %f\n",
                       i,
                       ref_[i],
                       dst_[i]);
                return -1;
            }
        } else {
            unsigned char *ref_ = (unsigned char *)ref;
            unsigned char *dst_ = (unsigned char *)dst;
            if (dst_[i] - ref_[i] != 0) {
                printf("compre of %d mismatch exp %d got %d\n",
                       i,
                       ref_[i],
                       dst_[i]);
                return -1;
            }
        }
    }

    free(src);
    free(ref);
    free(dst);
    bm_free_device(handle, input_mem);
    bm_free_device(handle, output_mem);
    return 0;
}

#define loopcnt 100

int main(int argc, char *argv[]) {
    int test_loop_times = 0;
    if (argc == 1) {
        test_loop_times = 1;
    } else {
        test_loop_times = atoi(argv[1]);
    }
    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST BGR SPLIT] loop times should be 1~1500"
                  << std::endl;
        exit(-1);
    }
    std::cout << "[TEST BGR SPLIT] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST BGR SPLIT] LOOP " << loop_idx << "------"
                  << std::endl;
        int n = 1, c = 3, h = 1080, w = 1920;
        int in_type = DTYPE_I8, out_type = DTYPE_I8;
        int ret = 0;

        bm_dev_request(&handle, 0);

#if !PERF_MEASURE_EN
        for (int i = 0; i < loopcnt; i++) {
            n       = 1;
            c       = 3;
            h       = rand() % 1080 + 2;
            w       = rand() % 1920 + 2;

            unsigned int chipid = 0x1686;
            bm_get_chipid(handle, &chipid);
            if((chipid == 0x1684)){
                in_type = rand() % 2 == 0 ? DTYPE_F32 : DTYPE_I8;
            }
            out_type = in_type;

            printf("Testing BGR Split %02d: h = %04d, w = %04d, input %s "
                   "output %s\n",
                   i,
                   h,
                   w,
                   in_type == DTYPE_F32 ? "Float" : "Byte",
                   out_type == DTYPE_F32 ? "Float" : "Byte");

            ret = test_split_main(n, c, h, w, in_type, out_type);
            if (ret != 0) {
                printf("Test BGR split Failed!\n");
                bm_dev_free(handle);
                exit(-1);
            } else
                printf("Test BGR Split Succeed!\n");
        }
#else

        // Measure performance of transpose and bgrsplit
        n = 1;
        c = 3;

        int img_profile[][2] = {{1920, 1080}, {800, 600}, {711, 400}};

        for (unsigned int i = 0;
             i < sizeof(img_profile) / sizeof(img_profile[0]);
             i++) {
            for (int j = 0; j < 2; j++) {
                for (int k = 0; k < 2; k++) {
                    h        = img_profile[i][1];
                    w        = img_profile[i][0];
                    in_type  = (j == 0) ? DTYPE_F32 : DTYPE_I8;
                    out_type = in_type;
                    ret      = test_split_main(n, c, h, w, in_type, out_type);
                    if (ret != 0) {
                        printf("Test IMG transpose Failed!\n");
                        bm_dev_free(handle);
                        exit(-1);
                    }
                }
            }
        }
#endif
        bm_dev_free(handle);
    }
    std::cout << "------[TEST BGR SPLIT] ALL TEST PASSED!" << std::endl;

    return 0;
}
