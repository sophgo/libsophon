#include "bmlib_runtime.h"
#include "bmcv_api_ext.h"

#include <memory>
#include <iostream>
#include <random>
#include <vector>
#include <map>
#include <unordered_map>

#include "stdio.h"
#include "math.h"

#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

#include "stdlib.h"
#include "string.h"
#include "test_misc.h"

using std::make_shared;
using std::shared_ptr;
using namespace std;

#define u8                      unsigned char
#define u16                     unsigned short
#define MAX_MULTI_CROP_NUM      10
static bm_handle_t handle;

/*
 *   params:
 *      src image sizes:
 *      1280*720
 *
 *      image format:
 *      NV12, YUV420P
 *
 *      roi num:
 *      1, 4, 5
 *
 *      roi sizes:
 *      64*64, 128*128, 256*256, 512*512, 1280*720
 *
 *      dst image sizes:
 *      32*32, 64*64, 128*128, 224*224, 352*352, 960*540
 *
 */
static bm_image_format_ext format_list[4][2] = {
    {FORMAT_NV12, FORMAT_NV12}, {FORMAT_NV12, FORMAT_YUV420P},{FORMAT_YUV420P, FORMAT_YUV420P},{FORMAT_YUV420P, FORMAT_NV12}};
static int src_image_sizes[1][2] = {{1280, 720}};
static int shape_lists[30][4] = {
  {32, 32, 64, 64}, {32, 32, 128, 128}, {32, 32, 256, 256}, {32, 32, 512, 512}, {32, 32, 1280, 720},
  {64, 64, 64, 64}, {64, 64, 128, 128}, {64, 64, 256, 256}, {64, 64, 512, 512}, {64, 64, 1280, 720},
  {128, 128, 64, 64}, {128, 128, 128, 128}, {128, 128, 256, 256}, {128, 128, 512, 512}, {128, 128, 1280, 720},
  {224, 224, 64, 64}, {224, 224, 128, 128}, {224, 224, 256, 256}, {224, 224, 512, 512}, {224, 224, 1280, 720},
  {352, 352, 64, 64}, {352, 352, 128, 128}, {352, 352, 256, 256}, {352, 352, 512, 512}, {352, 352, 1280, 720},
  {960, 540, 64, 64}, {960, 540, 128, 128}, {960, 540, 256, 256}, {960, 540, 512, 512}, {960, 540, 1280, 720}
};


/*   CROP_RESIZE
 *   params:
 *      src image sizes:
 *      960*540
 *
 *      image format:
 *      GRAY, RGB_PLANAR
 *
 *      roi num:
 *      1
 *
 *      roi sizes:
 *      32*32, 64*64, 96*96, 128*128, 256*256, 384*384, 512*512, 960*540, 64*512, 512*64
 *
 *      dst image sizes:
 *      64*64, 128*128, 256*256, 512*512, 950*540,
 *      32*64, 96*64, 128*64, 160*64, 200*64, 256*64, 512*64
 *      64*32, 64*96, 64*128, 64*160, 64*200, 64*256, 64*512
 *      32*128, 96*128, 128*128, 160*128, 200*128, 256*128, 512*128
 *
 */
// static bm_image_format_ext format_list[2][2] = {
//     {FORMAT_GRAY, FORMAT_GRAY}, {FORMAT_RGB_PLANAR, FORMAT_RGB_PLANAR}};
// static int src_image_sizes[1][2] = {{960, 540}};
// static int shape_lists[88][4] = {
//     {64, 64, 32, 32}, {64, 64, 64, 64}, {64, 64, 96, 96}, {64, 64, 128, 128},
//     {64, 64, 256, 256}, {64, 64, 384, 384}, {64, 64, 512, 512}, {64, 64, 960, 540},
//     {128, 128, 32, 32}, {128, 128, 64, 64}, {128, 128, 96, 96}, {128, 128, 128, 128},
//     {128, 128, 256, 256}, {128, 128, 384, 384}, {128, 128, 512, 512}, {128, 128, 960, 540},
//     {256, 256, 32, 32}, {256, 256, 64, 64}, {256, 256, 96, 96}, {256, 256, 128, 128},
//     {256, 256, 256, 256}, {256, 256, 384, 384}, {256, 256, 512, 512}, {256, 256, 960, 540},
//     {512, 512, 32, 32}, {512, 512, 64, 64}, {512, 512, 96, 96}, {512, 512, 128, 128},
//     {512, 512, 256, 256}, {512, 512, 384, 384}, {512, 512, 512, 512}, {512, 512, 960, 540},
//     {950, 540, 32, 32}, {950, 540, 64, 64}, {950, 540, 96, 96}, {950, 540, 128, 128},
//     {950, 540, 256, 256}, {950, 540, 384, 384}, {950, 540, 512, 512}, {950, 540, 960, 540},

//     {32, 128, 64, 64}, {64, 128, 64, 64}, {96, 128, 64, 64}, {128, 128, 64, 64},
//     {160, 128, 64, 64}, {200, 128, 64, 64}, {256, 128, 64, 64}, {512, 128, 64, 64},
//     {32, 128, 256, 256}, {64, 128, 256, 256}, {96, 128, 256, 256}, {128, 128, 256, 256},
//     {160, 128, 256, 256}, {200, 128, 256, 256}, {256, 128, 256, 256}, {512, 128, 256, 256},

//     {32, 64, 64, 64}, {64, 64, 64, 64}, {96, 64, 64, 64}, {128, 64, 64, 64},
//     {160, 64, 64, 64}, {200, 64, 64, 64}, {256, 64, 64, 64}, {512, 64, 64, 64},
//     {64, 32, 64, 64}, {64, 64, 64, 64}, {64, 96, 64, 64}, {64, 128, 64, 64},
//     {64, 160, 64, 64}, {64, 200, 64, 64}, {64, 256, 64, 64}, {64, 512, 64, 64},

//     {64, 32, 64, 512}, {64, 64, 64, 512}, {64, 96, 64, 512}, {64, 128, 64, 512},
//     {64, 160, 64, 512}, {64, 200, 64, 512}, {64, 256, 64, 512}, {64, 512, 64, 512},
//     {32, 64, 512, 64}, {64, 64, 512, 64}, {96, 64, 512, 64}, {128, 64, 512, 64},
//     {160, 64, 512, 64}, {200, 64, 512, 64}, {256, 64, 512, 64}, {512, 64, 512, 64}
// };

/*   CROP
 *   params:
 *      src image sizes:
 *      1280*720, 960*540
 *
 *      image format:
 *      NV12, YUV420P
 *
 *      roi num:
 *      1
 *
 *      roi sizes:
 *      1280*720: 32*32, 64*64, 96*96, 128*128, 256*256, 384*384, 512*512, 1280*720
 *      960*540: 32*32, 64*64, 96*96, 128*128, 256*256, 384*384, 512*512, 960*540
 *
 *
 */
// static bm_image_format_ext format_list[2][2] = {
//     {FORMAT_NV12, FORMAT_NV12},{FORMAT_YUV420P, FORMAT_YUV420P}};
// static int src_image_sizes[1][2] = {{1280, 720}};
// static int shape_lists[8][4] = {
//     {32, 32, 32, 32}, {64, 64, 64, 64}, {96, 96, 96, 96}, {128, 128, 128, 128},
//     {256, 256, 256, 256}, {384, 384, 384, 384}, {512, 512, 512, 512}, {1280, 720, 1280, 720}
// };

// static bm_image_format_ext format_list[2][2] = {
//     {FORMAT_NV12, FORMAT_NV12},{FORMAT_YUV420P, FORMAT_YUV420P}};
// static int src_image_sizes[1][2] = {{960,540}};
// static int shape_lists[8][4] = {
//     {32, 32, 32, 32}, {64, 64, 64, 64}, {96, 96, 96, 96}, {128, 128, 128, 128},
//     {256, 256, 256, 256}, {384, 384, 384, 384}, {512, 512, 512, 512}, {960, 540, 960, 540}
// };

#define CLIP(val, max, min)             \
    val = (val <= min) ? min : val;     \
    val = (val >= max) ? max : val

typedef struct sg_api_cv_multi_crop_resize {
    unsigned long long  input_global_offset;
    unsigned long long  aux_input0_global_offset;
    unsigned long long  aux_input1_global_offset;
    unsigned long long  yuv444_global_offset;
    unsigned long long  cropped_global_offset[MAX_MULTI_CROP_NUM];
    unsigned long long  resized_global_offset[MAX_MULTI_CROP_NUM];
    unsigned long long  output_global_offset[MAX_MULTI_CROP_NUM];
    unsigned long long  aux_output0_global_offset[MAX_MULTI_CROP_NUM];
    unsigned long long  aux_output1_global_offset[MAX_MULTI_CROP_NUM];
    int  src_height;
    int  src_width;
    int  dst_height;
    int  dst_width;
    int  roi_num;
    int  start_x[MAX_MULTI_CROP_NUM];
    int  start_y[MAX_MULTI_CROP_NUM];
    int  roi_height;
    int  roi_width;
    unsigned int  src_format;
    unsigned int  dst_format;
} sg_api_cv_multi_crop_resize_t;

class Blob {
  public:
    void *data      = NULL;
    int   byte_size = 0;

    explicit Blob(int size) {
        if (size == 0) {
            data      = NULL;
            byte_size = 0;
            return;
        }
        data      = malloc(size);
        byte_size = size;
    }
    ~Blob() {
        if (data) {
            free(data);
        }
    }
};

static shared_ptr<Blob> convert_unpack_1N_RGB_to_YUV444(shared_ptr<Blob> input_,
                                                        int              image_n,
                                                        int              image_h,
                                                        int              image_w) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(image_n * image_h * image_w * 3 * sizeof(u8));

    u8 *raw_ptr       = (u8 *)input_->data;
    u8 *reference_ptr = (u8 *)output_->data;

    for (int n = 0; n < image_n; n++) {
        for (int h = 0; h < image_h; h++) {
            for (int w = 0; w < image_w; w++) {
                float r =
                    (float)raw_ptr[n * 3 * image_h * image_w + image_w * h + w];
                float g = (float)raw_ptr[n * 3 * image_h * image_w +
                                         image_h * image_w + image_w * h + w];
                float b =
                    (float)raw_ptr[n * 3 * image_h * image_w +
                                   2 * image_h * image_w + image_w * h + w];
                float y, u, v;
                y = (0.257 * r + 0.504 * g + 0.098 * b + 16);
                u = (-0.148 * r - 0.291 * g + 0.439 * b + 128);
                v = (0.439 * r - 0.368 * g - 0.071 * b + 128);
                CLIP(y, 255, 0);
                CLIP(u, 255, 0);
                CLIP(v, 255, 0);
                // temp_y[n * image_h * image_w + image_w * h + w] = y;
                reference_ptr[n * image_h * image_w * 3 + image_w * h + w] =
                    (u8)y;
                reference_ptr[n * image_h * image_w * 3 + image_h * image_w +
                              image_w * h + w]                         = (u8)u;
                reference_ptr[n * image_h * image_w * 3 +
                              image_h * image_w * 2 + image_w * h + w] = (u8)v;
            }
        }
    }
    return output_;
}

static shared_ptr<Blob> convert_YUV444_to_NV12(shared_ptr<Blob> input_,
                                               int              image_n,
                                               int              image_h,
                                               int              image_w) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(image_n * image_h * image_w * 3 / 2 * sizeof(u8));

    u8 *raw_ptr = (u8 *)input_->data;
    u8 *out     = (u8 *)output_->data;

    for (int n = 0; n < image_n; n++) {
        memcpy(out + n * image_h * image_w * 3 / 2,
               raw_ptr + n * image_h * image_w * 3,
               image_h * image_w);
        for (int h = 0; h < image_h / 2; h++) {
            for (int w = 0; w < image_w / 2; w++) {
                u16 u = raw_ptr[n * image_h * image_w * 3 + image_h * image_w +
                                image_w * h * 2 + w * 2] +
                        raw_ptr[n * image_h * image_w * 3 + image_h * image_w +
                                image_w * (h * 2 + 1) + w * 2 + 1] +
                        raw_ptr[n * image_h * image_w * 3 + image_h * image_w +
                                image_w * h * 2 + w * 2 + 1] +
                        raw_ptr[n * image_h * image_w * 3 + image_h * image_w +
                                image_w * (h * 2 + 1) + w * 2];

                u16 v =
                    raw_ptr[n * image_h * image_w * 3 + 2 * image_h * image_w +
                            image_w * h * 2 + w * 2] +
                    raw_ptr[n * image_h * image_w * 3 + 2 * image_h * image_w +
                            image_w * (h * 2 + 1) + w * 2 + 1] +
                    raw_ptr[n * image_h * image_w * 3 + 2 * image_h * image_w +
                            image_w * h * 2 + w * 2 + 1] +
                    raw_ptr[n * image_h * image_w * 3 + 2 * image_h * image_w +
                            image_w * (h * 2 + 1) + w * 2];
                u                            = u >> 2;
                v                            = v >> 2;
                out[n * image_h * image_w * 3 / 2 + image_h * image_w +
                    image_w * h + w * 2]     = (u8)u;
                out[n * image_h * image_w * 3 / 2 + image_h * image_w +
                    image_w * h + w * 2 + 1] = (u8)v;
            }
        }
    }
    return output_;
}

static shared_ptr<Blob> convert_YUV444_to_NV12_float(shared_ptr<Blob> input_,
                                                     int              image_n,
                                                     int              image_h,
                                                     int              image_w) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(image_n * image_h * image_w * 3 / 2 * sizeof(float));

    float *raw_ptr = (float *)input_->data;
    float *out     = (float *)output_->data;

    for (int n = 0; n < image_n; n++) {
        memcpy(out + n * image_h * image_w * 3 / 2,
               raw_ptr + n * image_h * image_w * 3,
               image_h * image_w * sizeof(float));
        for (int h = 0; h < image_h / 2; h++) {
            for (int w = 0; w < image_w / 2; w++) {
                float u = raw_ptr[n * image_h * image_w * 3 + image_h * image_w +
                                image_w * h * 2 + w * 2] +
                        raw_ptr[n * image_h * image_w * 3 + image_h * image_w +
                                image_w * (h * 2 + 1) + w * 2 + 1] +
                        raw_ptr[n * image_h * image_w * 3 + image_h * image_w +
                                image_w * h * 2 + w * 2 + 1] +
                        raw_ptr[n * image_h * image_w * 3 + image_h * image_w +
                                image_w * (h * 2 + 1) + w * 2];

                float v =
                    raw_ptr[n * image_h * image_w * 3 + 2 * image_h * image_w +
                            image_w * h * 2 + w * 2] +
                    raw_ptr[n * image_h * image_w * 3 + 2 * image_h * image_w +
                            image_w * (h * 2 + 1) + w * 2 + 1] +
                    raw_ptr[n * image_h * image_w * 3 + 2 * image_h * image_w +
                            image_w * h * 2 + w * 2 + 1] +
                    raw_ptr[n * image_h * image_w * 3 + 2 * image_h * image_w +
                            image_w * (h * 2 + 1) + w * 2];
                u                            = u / 4;
                v                            = v / 4;
                out[n * image_h * image_w * 3 / 2 + image_h * image_w +
                    image_w * h + w * 2]     = (float)u;
                out[n * image_h * image_w * 3 / 2 + image_h * image_w +
                    image_w * h + w * 2 + 1] = (float)v;
            }
        }
    }
    return output_;
}

static shared_ptr<Blob> convert_NV12_to_YUV420P(shared_ptr<Blob> input_,
                                                int              IMAGE_N,
                                                int              IMAGE_H,
                                                int              IMAGE_W) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(IMAGE_N * IMAGE_H * IMAGE_W * 3 / 2 * sizeof(u8));

    u8 *raw_ptr = (u8 *)input_->data;
    u8 *out     = (u8 *)output_->data;

    for (int n = 0; n < IMAGE_N; n++) {
        memcpy(out + n * IMAGE_H * IMAGE_W * 3 / 2,
               raw_ptr + n * IMAGE_H * IMAGE_W * 3 / 2,
               IMAGE_H * IMAGE_W);
        for (int i = 0; i < IMAGE_H * IMAGE_W / 4; i++) {
            u8 u = raw_ptr[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W +
                           2 * i];
            u8 v = raw_ptr[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W +
                           2 * i + 1];

            out[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W + i] = (u8)u;
            out[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W +
                IMAGE_H * IMAGE_W / 4 + i]                             = (u8)v;
        }
    }
    return output_;
}

static shared_ptr<Blob> convert_NV12_to_YUV420P_float(shared_ptr<Blob> input_,
                                                      int              IMAGE_N,
                                                      int              IMAGE_H,
                                                      int              IMAGE_W) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(IMAGE_N * IMAGE_H * IMAGE_W * 3 / 2 * sizeof(float));

    float *raw_ptr = (float *)input_->data;
    float *out     = (float *)output_->data;

    for (int n = 0; n < IMAGE_N; n++) {
        memcpy(out + n * IMAGE_H * IMAGE_W * 3 / 2,
               raw_ptr + n * IMAGE_H * IMAGE_W * 3 / 2,
               IMAGE_H * IMAGE_W * sizeof(float));
        for (int i = 0; i < IMAGE_H * IMAGE_W / 4; i++) {
            float u = raw_ptr[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W +
                           2 * i];
            float v = raw_ptr[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W +
                           2 * i + 1];

            out[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W + i] = (float)u;
            out[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W +
                IMAGE_H * IMAGE_W / 4 + i]                             = (float)v;
        }
    }
    return output_;
}

static shared_ptr<Blob> convert_YUV420P_to_NV12(shared_ptr<Blob> input_,
                                                int              IMAGE_N,
                                                int              IMAGE_H,
                                                int              IMAGE_W) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(IMAGE_N * IMAGE_H * IMAGE_W * 3 * sizeof(u8));

    u8 *raw_ptr = (u8 *)input_->data;
    u8 *out     = (u8 *)output_->data;

    for (int n = 0; n < IMAGE_N; n++) {
        memcpy(out + n * IMAGE_H * IMAGE_W * 3 / 2,
               raw_ptr + n * IMAGE_H * IMAGE_W * 3 / 2,
               IMAGE_H * IMAGE_W);
        for (int i = 0; i < IMAGE_H * IMAGE_W / 4; i++) {
            u8 u =
                raw_ptr[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W + i];
            u8 v = raw_ptr[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W +
                           IMAGE_H * IMAGE_W / 4 + i];

            out[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W + i * 2] =
                (u8)u;
            out[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W + i * 2 + 1] =
                (u8)v;
        }
    }
    return output_;
}

static shared_ptr<Blob> convert_NV12_to_YUV444(shared_ptr<Blob> input_,
                                               int              IMAGE_N,
                                               int              IMAGE_H,
                                               int              IMAGE_W) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(IMAGE_N * IMAGE_H * IMAGE_W * 3 * sizeof(u8));

    u8 *raw_ptr = (u8 *)input_->data;
    u8 *out     = (u8 *)output_->data;

    for (int n = 0; n < IMAGE_N; n++) {
        memcpy(out + n * IMAGE_H * IMAGE_W * 3,
               raw_ptr + n * IMAGE_H * IMAGE_W * 3 / 2,
               IMAGE_H * IMAGE_W);
        for (int h = 0; h < IMAGE_H; h++) {
            for (int w = 0; w < IMAGE_W; w++) {
                u8 u =
                    raw_ptr[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W +
                            IMAGE_W * (h / 2) + (w / 2) * 2];
                u8 v =
                    raw_ptr[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W +
                            IMAGE_W * (h / 2) + (w / 2) * 2 + 1];

                out[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                    IMAGE_W * h + w] = (u8)u;
                out[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W * 2 +
                    IMAGE_W * h + w] = (u8)v;
            }
        }
    }
    return output_;
}

static int array_cmp_u8(
    u8 *p_exp, u8 *p_got, int len, const char *info_label, u8 delta) {
    int idx = 0;
    for (idx = 0; idx < len; idx++) {
        if ((int)fabs(p_exp[idx] - (int)p_got[idx]) > delta) {
            printf("%s abs error at index %d exp %d got %d\n",
                   info_label,
                   idx,
                   p_exp[idx],
                   p_got[idx]);
            return -1;
        }
    }
    return 0;
}

shared_ptr<Blob> gen_random_rgb_data(int image_n, int image_h, int image_w) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(image_n * image_h * image_w * 3 * sizeof(u8));
    u8 *input_ptr = (u8 *)output_->data;
    for (int n = 0; n < image_n; n++) {
       for (int i = 0; i < image_h * image_w; i++) {
            input_ptr[n * 3 * image_h * image_w + i] = rand() % 256;
            input_ptr[n * 3 * image_h * image_w + image_h * image_w + i] =
                rand() % 256;
            input_ptr[n * 3 * image_h * image_w + 2 * image_h * image_w + i] =
                rand() % 256;
        }
    }
    return output_;
}

shared_ptr<Blob> gen_random_gray_data(int image_n, int image_h, int image_w) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(image_n * image_h * image_w * sizeof(u8));
    u8 *input_ptr = (u8 *)output_->data;
    for (int n = 0; n < image_n; n++) {
       for (int i = 0; i < image_h * image_w; i++) {
            input_ptr[n * image_h * image_w + i] = rand() % 256;
        }
    }
    return output_;
}

shared_ptr<Blob> convert_to_float_data(shared_ptr<Blob> u8_input,
        int image_n, int image_h, int image_w, int is_gray) {

    shared_ptr<Blob> output_ =
        make_shared<Blob>(image_n * image_h * image_w * (is_gray ? 1 : 3) * sizeof(float));
    float *input_ptr = (float *)output_->data;
    u8 *u8_input_ptr = (u8 *)u8_input->data;
    for (int n = 0; n < image_n; n++) {
        for (int i = 0; i < image_h * image_w; i++) {
            input_ptr[n * 3 * image_h * image_w + i] =
                    (float)(u8_input_ptr[n * 3 * image_h * image_w + i]);
            if(!is_gray) {
                input_ptr[n * 3 * image_h * image_w + image_h * image_w + i] =
                        (float)(u8_input_ptr[n * 3 * image_h * image_w + image_h * image_w + i]);
                input_ptr[n * 3 * image_h * image_w + 2 * image_h * image_w + i] =
                        (float)(u8_input_ptr[n * 3 * image_h * image_w + 2 * image_h * image_w + i]);
            }
        }
    }
    return output_;
}

shared_ptr<Blob> convert_from_float_data(shared_ptr<Blob> float_input, int len) {
    shared_ptr<Blob> u8_output_ =
        make_shared<Blob>(len * sizeof(u8));
    float *input_ptr = (float *)float_input->data;
    u8 *u8_output_ptr = (u8 *)u8_output_->data;
    for (int i = 0; i < len; i++) {
        u8_output_ptr[i] = (u8)round(input_ptr[i] );
    }
    return u8_output_;
}

vector<shared_ptr<Blob> > image_crop_float(
        shared_ptr<Blob> input,
        int  roi_num,
        int* start_x,
        int* start_y,
        int  src_h,
        int  src_w,
        int  roi_h,
        int  roi_w,
        int  is_gray) {
    int stride_w = roi_w;
    vector<shared_ptr<Blob> > cropped_outputs;
    for(int i = 0; i < roi_num; i++) {
        shared_ptr<Blob> output =
            make_shared<Blob>(roi_h * roi_w * (is_gray ? 1 : 3) * sizeof(float));
        float *input_ptr = (float *)input->data;
        float *output_ptr = (float *)output->data;

        float *y = (float *)input_ptr + start_y[i] * src_w + start_x[i];
        float *u;
        float *v;

        float *cropy = output_ptr;
        float *cropu;
        float *cropv;

        if(!is_gray){
            u = y + src_w * src_h;
            v = u + src_w * src_h;
            cropu = cropy + stride_w * roi_h;
            cropv = cropu + stride_w * roi_h;
        }


        for (int i = 0; i < roi_h; i++) {
            memcpy(cropy, y, roi_w * sizeof(float));
            if(!is_gray){
                memcpy(cropu, u, roi_w * sizeof(float));
                memcpy(cropv, v, roi_w * sizeof(float));
            }

            y += src_w;
            if(!is_gray){
                u += src_w;
                v += src_w;
            }
            cropy += stride_w;
            if(!is_gray){
                cropu += stride_w;
                cropv += stride_w;
            }
        }
        cropped_outputs.push_back(output);
    }

    return cropped_outputs;
}

shared_ptr<Blob> image_resize_bilinear_float(shared_ptr<Blob> input,
        int roi_h, int roi_w, int dst_h, int dst_w, int is_gray) {

    shared_ptr<Blob> output = make_shared<Blob>(dst_h * dst_w * (is_gray ? 1 : 3) * sizeof(float));
    float* dataDst = (float *)output->data;
    float* dataSrc = (float *)input->data;
    int stepDst = dst_h * dst_w;
    int stepSrc = roi_h * roi_w;
    float scale_x = (float)roi_w / dst_w;
    float scale_y = (float)roi_h / dst_h;

    for (int k = 0; k < (is_gray ? 1 : 3); ++k) {
        int sx_plus_1;
        int sy_plus_1;

        for (int j = 0; j < dst_h; ++j) {
            float fy = (float)((j + 0.5) * scale_y - 0.5);
            int sy = floor(fy);
            fy -= sy;

            sy_plus_1 = sy + 1;
            if (sy >= roi_h - 1) {
                sy = roi_h - 1;
                sy_plus_1 = sy;
            }
            if (sy < 0) {
                sy = 0;
                sy_plus_1 = 0;
                fy = 0;
            }

            float cbufy[2];
            cbufy[0] = 1.f - fy;
            cbufy[1] = fy;

            for (int i = 0; i < dst_w; ++i) {
                float fx = (float)((i + 0.5) * scale_x - 0.5);
                int sx = floor(fx);
                fx -= sx;

                sx_plus_1 = sx + 1;
                if (sx >= roi_w - 1) {
                    sx = roi_w -1;
                    sx_plus_1 = sx;
                }
                if (sx < 0) {
                    fx = 0, sx = 0;
                    sx_plus_1 = 1;
                }

                float cbufx[2];
                cbufx[0] = 1.f - fx;
                cbufx[1] = fx;

                *(dataDst + k * stepDst + j * dst_w + i) =
                        (*(dataSrc + k * stepSrc + sy * roi_w + sx) * cbufx[0] * cbufy[0] +
                        *(dataSrc + k * stepSrc + sy_plus_1 * roi_w + sx) * cbufx[0] * cbufy[1] +
                        *(dataSrc + k * stepSrc + sy * roi_w + sx_plus_1) * cbufx[1] * cbufy[0] +
                        *(dataSrc + k * stepSrc + sy_plus_1 * roi_w + sx_plus_1) * cbufx[1] * cbufy[1]);
            }
        }
    }

    return output;
}

vector<shared_ptr<Blob> > convert_crop_and_resize_native(
        shared_ptr<Blob> src_format_input,
        bm_image_format_ext src_format,
        bm_image_format_ext dst_format,
        int  roi_num,
        int* start_x,
        int* start_y,
        int  src_h,
        int  src_w,
        int  roi_h,
        int  roi_w,
        int  dst_h,
        int  dst_w) {

    shared_ptr<Blob> converted_output = src_format_input;
    shared_ptr<Blob> resized_output;
    vector<shared_ptr<Blob> > cropped_outputs;
    vector<shared_ptr<Blob> > resized_outputs;
    int is_gray = (src_format == FORMAT_GRAY) ? 1 : 0;
    if(src_format == FORMAT_YUV420P) {
        converted_output = convert_YUV420P_to_NV12(src_format_input, 1, src_h, src_w);
        converted_output = convert_NV12_to_YUV444(converted_output, 1, src_h, src_w);
    }
    else if(src_format == FORMAT_NV12) {
        converted_output = convert_NV12_to_YUV444(src_format_input, 1, src_h, src_w);
    }
    converted_output = convert_to_float_data(converted_output, 1, src_h, src_w, is_gray);
    cropped_outputs = image_crop_float(converted_output,
            roi_num, start_x, start_y, src_h, src_w, roi_h, roi_w, is_gray);
    for(auto cropped_output: cropped_outputs) {
        resized_output = image_resize_bilinear_float(cropped_output,
                roi_h, roi_w, dst_h, dst_w, is_gray);

        int dst_data_num = 0;
        if(dst_format == FORMAT_YUV420P) {
            dst_data_num = dst_h * dst_w * 3 / 2;
            resized_output = convert_YUV444_to_NV12_float(resized_output, 1, dst_h, dst_w);
            resized_output = convert_NV12_to_YUV420P_float(resized_output, 1, dst_h, dst_w);
        }
        else if(dst_format == FORMAT_NV12) {
            dst_data_num = dst_h * dst_w * 3 / 2;
            resized_output = convert_YUV444_to_NV12_float(resized_output, 1, dst_h, dst_w);
        }
        else if(dst_format == FORMAT_RGB_PLANAR) {
            dst_data_num = dst_h * dst_w * 3;
        }
        else if(dst_format == FORMAT_GRAY) {
            dst_data_num = dst_h * dst_w;
        }
        resized_output = convert_from_float_data(resized_output, dst_data_num);
        resized_outputs.push_back(resized_output);
    }
    if((int)resized_outputs.size() != roi_num) {
        std::cout << "Number of resized outputs doesn't match number of roi's." << std::endl;
        exit(-1);
    }

    return resized_outputs;
}

bm_status_t bmcv_image_multi_crop_resize(
        bm_handle_t  handle,
        int          crop_num,
        bmcv_rect_t* rect,
        bm_image     input,
        bm_image*    output) {

    if (handle == NULL) {
        bmlib_log("bmkernel_image_multi_crop_resize", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }

    int output_num = crop_num;
    if (!bm_image_is_attached(input)) {
        bmlib_log("bmkernel_image_multi_crop_resize", BMLIB_LOG_ERROR,
                    "input image not attach device memory %s: %s: %d\n",
                    __FILE__, __func__, __LINE__);
        return BM_ERR_FAILURE;
    }

    int  src_height = input.height;
    int  src_width = input.width;
    bm_device_mem_t yuv444_mem;
    int total_size = 3 * src_height * src_width * sizeof(float);
    if (BM_SUCCESS != bm_malloc_device_byte(handle, &yuv444_mem, total_size)) {
        bmlib_log("bmkernel_image_multi_crop_resize", BMLIB_LOG_ERROR,
                "bm_image_alloc_dev_mem error %s: %s: %d\n",
                __FILE__, __func__, __LINE__);
        return BM_ERR_FAILURE;
    }

    int  dst_height = output[0].height;
    int  dst_width = output[0].width;

    bm_device_mem_t cropped_mem[output_num];
    bm_device_mem_t resized_mem[output_num];
    for (int i = 0; i < output_num; i++) {
        // alloc mem for buffer to hold cropped images
        int total_size = 3 * rect[i].crop_w * rect[i].crop_h * sizeof(float);
        if (BM_SUCCESS != bm_malloc_device_byte(handle, cropped_mem + i, total_size)) {
            bmlib_log("bmkernel_image_multi_crop_resize", BMLIB_LOG_ERROR,
                    "bm_image_alloc_dev_mem error %s: %s: %d\n",
                    __FILE__, __func__, __LINE__);
            return BM_ERR_FAILURE;
        }

        // alloc mem for buffer to hold resized images
        total_size = 3 * dst_width * dst_height * sizeof(float);
        if (BM_SUCCESS != bm_malloc_device_byte(handle, resized_mem + i, total_size)) {
            bmlib_log("bmkernel_image_multi_crop_resize", BMLIB_LOG_ERROR,
                    "bm_image_alloc_dev_mem error %s: %s: %d\n",
                    __FILE__, __func__, __LINE__);
            return BM_ERR_FAILURE;
        }

        // check device for outputs
        if (!bm_image_is_attached(output[i])) {
            if (BM_SUCCESS != bm_image_alloc_dev_mem(output[i], BMCV_HEAP_ANY)) {
                bmlib_log("bmkernel_image_multi_crop_resize", BMLIB_LOG_ERROR,
                      "bm_image_alloc_dev_mem error %s: %s: %d\n",
                      __FILE__, __func__, __LINE__);
                return BM_ERR_FAILURE;
            }
        }
    }

    unsigned long long input_global_offset = 0;
    unsigned long long aux_input0_global_offset = 0;
    unsigned long long aux_input1_global_offset = 0;
    bm_device_mem_t input_mem[3];
    bm_image_get_device_mem(input, input_mem);
    for (int p = 0; p < bm_image_get_plane_num(input); p++) {
        if (p == 0)
            input_global_offset = bm_mem_get_device_addr(input_mem[p]);
        else if (p == 1)
            aux_input0_global_offset = bm_mem_get_device_addr(input_mem[p]);
        else if (p == 2)
            aux_input1_global_offset = bm_mem_get_device_addr(input_mem[p]);
    }

    unsigned long long yuv444_global_offset = bm_mem_get_device_addr(yuv444_mem);

    unsigned long long cropped_global_offset[output_num];
    unsigned long long resized_global_offset[output_num];
    for (int k = 0; k < output_num; k++) {
        cropped_global_offset[k] = bm_mem_get_device_addr(cropped_mem[k]);
        resized_global_offset[k] = bm_mem_get_device_addr(resized_mem[k]);
    }

    unsigned long long output_global_offset[output_num];
    unsigned long long aux_output0_global_offset[output_num];
    unsigned long long aux_output1_global_offset[output_num];
    bm_device_mem_t output_mem[output_num][3];
    for (int k = 0; k < output_num; k++) {
        bm_image_get_device_mem(output[k], output_mem[k]);
        for (int p = 0; p < bm_image_get_plane_num(output[k]); p++) {
            if (p == 0)
                output_global_offset[k] = bm_mem_get_device_addr(output_mem[k][p]);
            else if (p == 1)
                aux_output0_global_offset[k] = bm_mem_get_device_addr(output_mem[k][p]);
            else if (p == 2)
                aux_output1_global_offset[k] = bm_mem_get_device_addr(output_mem[k][p]);
        }
    }

    int  roi_num = crop_num;
    int  start_x[roi_num];
    int  start_y[roi_num];
    for(int i = 0; i < roi_num; i++) {
        start_x[i] = rect[i].start_x;
        start_y[i] = rect[i].start_y;
    }
    int  roi_height = rect[0].crop_h;
    int  roi_width = rect[0].crop_w;
    unsigned int src_format = input.image_format;
    unsigned int dst_format = output[0].image_format;

    sg_api_cv_multi_crop_resize_t param;
    memset(&param, 0, sizeof(sg_api_cv_multi_crop_resize_t));

    param.input_global_offset = input_global_offset;
    param.aux_input0_global_offset = aux_input0_global_offset;
    param.aux_input1_global_offset = aux_input1_global_offset;
    param.yuv444_global_offset = yuv444_global_offset;

    for(int i = 0; i < roi_num; i++) {
        param.cropped_global_offset[i] = cropped_global_offset[i];
        param.resized_global_offset[i] = resized_global_offset[i];
        param.output_global_offset[i] = output_global_offset[i];
        param.aux_output0_global_offset[i] = aux_output0_global_offset[i];
        param.aux_output1_global_offset[i] = aux_output1_global_offset[i];
        param.start_x[i] = start_x[i];
        param.start_y[i] = start_y[i];
    }

    param.src_height = src_height;
    param.src_width = src_width;
    param.dst_height = dst_height;
    param.dst_width = dst_width;
    param.roi_num = roi_num;
    param.roi_height = roi_height;
    param.roi_width = roi_width;
    param.src_format = src_format;
    param.dst_format = dst_format;

    tpu_kernel_launch_sync(handle, "cv_multi_crop_resize", &param, sizeof(param));
    bm_thread_sync(handle);

    // free device mem for inter-buffer
    for (int i = 0; i < output_num; i++) {
        bm_free_device(handle, cropped_mem[i]);
        bm_free_device(handle, resized_mem[i]);
    }
    bm_free_device(handle, yuv444_mem);

    return BM_SUCCESS;
}

void single_test(bm_image_format_ext src_format,
                 bm_image_format_ext dst_format,
                 int  roi_num,
                 int* start_x,
                 int* start_y,
                 int  src_h,
                 int  src_w,
                 int  roi_h,
                 int  roi_w,
                 int  dst_h,
                 int  dst_w) {
    std::cout << "[BMKERNEL-MULTI_CROP_RESIZE] src_format: " << src_format << " dst_format: " <<
             dst_format << " roi_num: " << roi_num << " src_h: " << src_h <<
             " src_w: " << src_w << " roi_h: " << roi_h << " roi_w: " << roi_w <<
             " dst_h: " << dst_h << " dst_w: " << dst_w << std::endl;

    // rect check
    for(int i = 0; i < roi_num; i++) {
        if(start_x[i] >= src_w || start_y[i] >= src_h){
            std::cout << "roi not valid!" << std::endl;
            exit(-1);
        }
    }

    // prepare input data
    shared_ptr<Blob> src_format_input;
    if(src_format == FORMAT_NV12) {
        shared_ptr<Blob> rgb_input = gen_random_rgb_data(1, src_h, src_w);
        shared_ptr<Blob> yuv444_input = convert_unpack_1N_RGB_to_YUV444(rgb_input, 1, src_h, src_w);
        src_format_input = convert_YUV444_to_NV12(yuv444_input, 1, src_h, src_w);
    }
    else if(src_format == FORMAT_YUV420P) {
        shared_ptr<Blob> rgb_input = gen_random_rgb_data(1, src_h, src_w);
        shared_ptr<Blob> yuv444_input = convert_unpack_1N_RGB_to_YUV444(rgb_input, 1, src_h, src_w);
        src_format_input = convert_YUV444_to_NV12(yuv444_input, 1, src_h, src_w);
        src_format_input = convert_NV12_to_YUV420P(src_format_input, 1, src_h, src_w);
    }
    else if(src_format == FORMAT_GRAY) {
        src_format_input = gen_random_gray_data(1, src_h, src_w);
    }
    else if(src_format == FORMAT_RGB_PLANAR) {
        src_format_input = gen_random_rgb_data(1, src_h, src_w);
    } else {
        std::cout << "Src format not supported." << std::endl;
        exit(-1);
    }

    // calculate reference
    vector<shared_ptr<Blob> > out_ref = convert_crop_and_resize_native(
            src_format_input, src_format, dst_format, roi_num, start_x, start_y,
            src_h, src_w, roi_h, roi_w, dst_h, dst_w);

    // calculate with bmkernel code
    bm_image_data_format_ext src_data_format = DATA_TYPE_EXT_1N_BYTE;
    bm_image_data_format_ext dst_data_format = DATA_TYPE_EXT_1N_BYTE;
    bm_image_format_ext src_format_ = src_format;
    bm_image_format_ext dst_format_ = dst_format;
    int dst_num = roi_num;

    bm_image input;
    bm_image_create(handle,
                    src_h,
                    src_w,
                    src_format_,
                    src_data_format,
                    &input);

    int src_size[3] = {0};
    bm_image_get_byte_size(input, src_size);
    u8 *input_host_ptr = (u8 *)src_format_input->data;
    u8 *input_image_ptr[4] = {input_host_ptr,
                             input_host_ptr + src_size[0],
                             input_host_ptr + src_size[0] + src_size[1],
                             input_host_ptr + src_size[0] + src_size[1] + src_size[2]};
    bm_image_copy_host_to_device(input, (void **)(input_image_ptr));

    bmcv_rect_t rect[roi_num];
    bm_image output[roi_num];
    for (int i = 0; i < dst_num; i++) {
        rect[i].start_x = start_x[i];
        rect[i].start_y = start_y[i];
        rect[i].crop_w = roi_w;
        rect[i].crop_h = roi_h;

        bm_image_create(handle,
                        dst_h,
                        dst_w,
                        dst_format_,
                        dst_data_format,
                        output + i);
    }

    int loop = 1;
    bm_status_t ret;
    struct timeval t1, t2;
    gettimeofday_(&t1);
    for (int i = 0; i < loop; i++) {
      ret = bmcv_image_multi_crop_resize(handle, roi_num, rect, input, output);
    }
    gettimeofday_(&t2);


    cout << "multi crop resize using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) / loop << "us" << endl;

    if (ret != BM_SUCCESS) {
        printf("run bmkernel_multi_crop_resize failed ret = %d\n", ret);
        bm_image_destroy(input);
        for (int i = 0; i < dst_num; i++) {
            bm_image_destroy(output[i]);
        }
        exit(-1);
    }

    int dst_size[3] = {0};
    bm_image_get_byte_size(output[0], dst_size);
    int total_size_dst = dst_size[0] + dst_size[1] + dst_size[2];
    vector<shared_ptr<Blob> > output_blobs;
    for(int i = 0; i < dst_num; i++) {
        shared_ptr<Blob> output_blob_ =
                make_shared<Blob>(total_size_dst * sizeof(u8));
        u8 *output_host_ptr = (u8 *)output_blob_->data;
        u8 *image_ptr[4] = {output_host_ptr,
                            output_host_ptr + dst_size[0],
                            output_host_ptr + dst_size[0] + dst_size[1],
                            output_host_ptr + dst_size[0] + dst_size[1] + dst_size[2]};
        bm_image_copy_device_to_host(output[i], (void **)image_ptr);
        output_blobs.push_back(output_blob_);
    }

    bm_image_destroy(input);
    for (int i = 0; i < dst_num; i++) {
        bm_image_destroy(output[i]);
    }

    // compare results with reference
    int cmp_res_x = 0;
    for (int i = 0; i < dst_num; i++) {
        printf("bmkernel multi_crop_resize comparing for output idx %d\n", i);
        const char *info_label = "bmkernel multi_crop_resize test ...\n";
        cmp_res_x = array_cmp_u8((u8 *)out_ref[i]->data,
                                (u8 *)output_blobs[i]->data,
                                total_size_dst,
                                info_label,
                                1);
        if(cmp_res_x != 0) {
            break;
        }
    }

    if (cmp_res_x != 0) {
        printf("bmkernel multi_crop_resize comparing failed\n");
        exit(-1);
    }
    printf("bmkernel multi_crop_resize single_test succeeded\n");
}


void test_bmkernel_crop_and_resize() {
    for (auto src_shape: src_image_sizes) {
        int roi_num = 4; // 1, 4, 5
        for (auto image_format: format_list) {
            bm_image_format_ext dst_format = image_format[1];
            bm_image_format_ext src_format = image_format[0];
            for (auto shapes: shape_lists) {
                int src_h = src_shape[0];
                int src_w = src_shape[1];
                int dst_h = shapes[0];
                int dst_w = shapes[1];
                int roi_h = shapes[2];
                int roi_w = shapes[3];
                int start_x[roi_num];
                int start_y[roi_num];
                for (int i = 0; i < roi_num; i++) {
                    start_x[i] = rand() % (src_w - roi_w - 1);
                    start_y[i] = rand() % (src_h - roi_h - 1);
                }
                single_test(src_format, dst_format, roi_num,
                        start_x, start_y, src_h, src_w,
                        roi_h, roi_w, dst_h, dst_w);
            }
        }
    }
}

int main() {
    int dev_id = 0;
    bm_status_t ret = bm_dev_request(&handle, dev_id);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        exit(-1);
    }

    test_bmkernel_crop_and_resize();

    bm_dev_free(handle);

    std::cout << "------[TEST MULTI_CROP_AND_RESIZE WITH BMKERNEL] ALL TEST PASSED!" << std::endl;

    return 0;
}
