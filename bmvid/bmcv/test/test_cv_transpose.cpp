#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "test_misc.h"
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

#include "bmlib_runtime.h"

using namespace std;

#define DTYPE_F32 4
#define DTYPE_I8 1


template <typename T>
static int bmcv_transpose_tpu_old(int channel,
                                  int height,
                                  int width,
                                  T* src,
                                  T* dst) {
    assert(channel == 3);
    assert(sizeof(T) == 1 || sizeof(T) == 4);
    bm_handle_t handle;
    bm_status_t ret;
    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("ERROR: Cannot get handle\n");
        return -1;
    }
    bmcv_image input;
    input.color_space = COLOR_RGB;
    input.data_format = sizeof(T) == 4 ? DATA_TYPE_FLOAT : DATA_TYPE_BYTE;
    input.image_format = BGR;
    input.data[0] = bm_mem_from_system(src);
    input.stride[0] = width;
    input.image_width = width;
    input.image_height = height;

    bmcv_image output;
    output.color_space = COLOR_RGB;
    output.data_format = sizeof(T) == 4 ? DATA_TYPE_FLOAT : DATA_TYPE_BYTE;
    output.image_format = BGR;
    output.data[0] = bm_mem_from_system(dst);
    output.stride[0] = height;
    output.image_width = height;
    output.image_height = width;

    struct timeval t1, t2;
    gettimeofday_(&t1);
    ret = bmcv_img_transpose(handle, input, output);
    if (ret != BM_SUCCESS) {
        printf("bmcv_image_transpose error!");
        return -1;
    }
    gettimeofday_(&t2);
    cout << "excution time is : " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

    bm_dev_free(handle);
    return 0;
    // double t0 = (double)cv::getTickCount();
    // ret = bmcv_img_transpose(handle, input, output);
    // if (ret != BM_SUCCESS) {
    //     printf("bmcv_image_transpose error!");
    //     return -1;
    // }
    // double t1 = (double)cv::getTickCount() - t0;
    // printf("excution time is %gms\n", t1 * 1000. / cv::getTickFrequency());
}


template <typename T>
static void bmcv_fill_input(int channel,
                            int height,
                            int width,
                            T* input) {
    int num = channel * height * width * sizeof(T) / 4;
    float* input_ = (float*)input;
    for (int i = 0; i < num; i++) {
        input_[i] = (float)rand() / 1000;
    }
}

template <typename T>
static void bmcv_transpose_ref(int channel,
                               int height,
                               int width,
                               int stride,
                               T* src,
                               T* dst) {
    for (int i=0; i < channel; i++) {
        for (int j = 0; j < width; j++) {
           for (int k = 0; k < height; k++) {
               dst[i * height * width + j * height + k] = src[i * height * stride + k * stride + j];
           }
        }
    }
}

static int bmcv_transpose_cmp(int channel,
                              int height,
                              int width,
                              int type_len,
                              void* got,
                              void* exp) {
    if (type_len == 1) {
        unsigned char* got_ = (unsigned char*)got;
        unsigned char* exp_ = (unsigned char*)exp;
        for (int i = 0; i < channel * height * width; i++) {
            if (got_[i] != exp_[i]) {
                printf("ERROR: compare of %d mismatch exp %d got %d\n", i, exp_[i], got_[i]);
                return -1;
            }
        }
    } else {
        float* got_ = (float*)got;
        float* exp_ = (float*)exp;
        for (int i = 0; i < channel * height * width; i++) {
            if (got_[i] != exp_[i]) {
                printf("ERROR: compare of %d mismatch exp %f got %f\n", i, exp_[i], got_[i]);
                return -1;
            }
        }
    }
    return 0;
}

template <typename T>
static int bmcv_transpose_tpu(int channel,
                              int height,
                              int width,
                              int stride,
                              T* src,
                              T* dst) {
    assert(channel == 1 || channel == 3);
    assert(sizeof(T) == 1 || sizeof(T) == 4);
    bm_handle_t handle;
    bm_status_t ret = BM_SUCCESS;
    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("ERROR: Cannot get handle\n");
        return -1;
    }
    int format = (channel == 3) ? FORMAT_BGR_PLANAR : FORMAT_GRAY;
    int type = (sizeof(T) == 1) ? DATA_TYPE_EXT_1N_BYTE : DATA_TYPE_EXT_FLOAT32;
    int stride_byte = stride * sizeof(T);
    bm_image input, output;
    bm_image_create(handle,
                    height,
                    width,
                    (bm_image_format_ext)format,
                    (bm_image_data_format_ext)type,
                    &input,
                    &stride_byte);
    bm_image_copy_host_to_device(input, (void**)&src);

    bm_image_create(handle,
                    width,
                    height,
                    (bm_image_format_ext)format,
                    (bm_image_data_format_ext)type,
                    &output);

    ret = bmcv_image_transpose(handle, input, output);
    if (ret != BM_SUCCESS) {
        printf("bmcv_image_transpose error!");
        ret = BM_ERR_FAILURE;
    }

    bm_image_copy_device_to_host(output, (void**)&dst);
    bm_image_destroy(input);
    bm_image_destroy(output);
    bm_dev_free(handle);
    return ret;
}

template <typename T>
static int bmcv_transpose_test(int channel,
                               int height,
                               int width,
                               int stride,
                               T* src,
                               T* dst,
                               T* ref) {
    int ret = 0;
    bmcv_fill_input<T>(channel, height, stride, src);
    bmcv_transpose_ref<T>(channel, height, width, stride, src, ref);
    ret = bmcv_transpose_tpu<T>(channel, height, width, stride, src, dst);
    if (ret == -1) return -1;
    ret = bmcv_transpose_cmp(channel, height, width, sizeof(T), dst, ref);
    if (ret == -1) return -1;
    return 0;
}

int main(int argc, char* argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    struct timespec tp;
    clock_gettime_(0, &tp);

    unsigned int seed = tp.tv_nsec;
    printf("random seed is %d\n", seed);
    srand(seed);
    int c = (rand() % 10 < 7) ? 3 : 1;
    int h = rand() % 2000 + 2;
    int w = rand() % 2000 + 2;
    int stride = (w + 63) / 64 * 64;
    int type = rand() % 2 ? DTYPE_F32 : DTYPE_I8;
    printf("c = %d  h = %d  w = %d  type_len = %d\n", c, h, w, type);
    void* src = malloc(c * h * stride * type);
    void* dst = malloc(c * h * w * type);
    void* ref = malloc(c * h * w * type);
    int ret = 0;
    if (type == DTYPE_F32) {
        ret = bmcv_transpose_test<float>(c, h, w, stride, (float*)src, (float*)dst, (float*)ref);
    } else {
        ret = bmcv_transpose_test<unsigned char>(c, h, w, stride, (unsigned char*)src, (unsigned char*)dst, (unsigned char*)ref);
    }
    if (ret == 0) {
        printf("bmcv transpose test passed!\n");
    } else {
        printf("bmcv transpose test failed!\n");
        free(src);
        free(dst);
        free(ref);
        return -1;
    }
    free(src);
    free(dst);
    free(ref);
    return 0;
}
