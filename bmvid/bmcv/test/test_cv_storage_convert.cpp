#include <memory>
#include <iostream>
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "test_cv_naive_threadpool.hpp"
#include <random>
#include <mutex>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

static thread_local int IMAGE_N = 2;
static thread_local  int IMAGE_H = 2;
static thread_local int IMAGE_W = 2;

#define BMCV_COMPARE_EPSILON (1e-4)
#define CLIP(val, max, min)                                                    \
    val = (val <= min) ? min : val;                                            \
    val = (val >= max) ? max : val

using std::make_shared;
using std::shared_ptr;

static bm_handle_t handle;
static std::shared_ptr<ThreadPool> pool = std::make_shared<ThreadPool>(1);

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

shared_ptr<Blob> convert_4N_INT8_to_1N_INT8(
    shared_ptr<Blob> input_, int N, int C, int H, int W) {
    shared_ptr<Blob> output_ = make_shared<Blob>(N * C * H * W * sizeof(u8));
    u8 *             input   = (u8 *)input_->data;
    u8 *             output  = (u8 *)output_->data;
    for (int n = 0; n < N; n++) {
        for (int loop = 0; loop < C * H * W; loop++) {
            output[n * C * H * W + loop] =
                input[(n / 4) * 4 * C * H * W + 4 * loop + n % 4];
        }
    }
    return output_;
}

shared_ptr<Blob>
convert_Float_to_1N_INT8(shared_ptr<Blob> input_, int N, int C, int H, int W) {
    shared_ptr<Blob> output_ = make_shared<Blob>(N * C * H * W * sizeof(u8));
    float *          input   = (float *)input_->data;
    u8 *             output  = (u8 *)output_->data;
    for (int n = 0; n < N; n++) {
        for (int loop = 0; loop < C * H * W; loop++) {
            output[n * C * H * W + loop] =
                (unsigned char)((int)input[n * C * H * W + loop]);
        }
    }
    return output_;
}

shared_ptr<Blob> convert_1N_INT8_to_4N_INT8(
    shared_ptr<Blob> input_, int n, int c, int h, int w) {
    shared_ptr<Blob> output_ = make_shared<Blob>(4 * c * h * w * sizeof(u8));
    u8 *             input   = (u8 *)input_->data;
    u8 *             output  = (u8 *)output_->data;

    for (int i = 0; i < ALIGN(n, 4) / 4; i++) {
        for (int loop = 0; loop < c * h * w; loop++) {
            for (int k = 0; k < bm_min(n, 4); k++) {
                output[i * 4 * c * h * w + 4 * loop + k] =
                    input[i * 4 * c * h * w + k * c * h * w + loop];
            }
        }
    }
    return output_;
}

shared_ptr<Blob>
convert_1N_INT8_to_Float(shared_ptr<Blob> input_, int n, int c, int h, int w) {
    shared_ptr<Blob> output_ = make_shared<Blob>(4 * c * h * w * sizeof(float));
    u8 *             input   = (u8 *)input_->data;
    float *          output  = (float *)output_->data;
    for (int i = 0; i < n; i++) {
        for (int loop = 0; loop < c * h * w; loop++) {
            output[i * c * h * w + loop] =
                (float)((unsigned int)input[i * c * h * w + loop]);
        }
    }
    return output_;
}

shared_ptr<Blob> convert_pack_1N_to_unpack_1N(
    shared_ptr<Blob> input_, int n, int h, int w, bool exchange) {
    shared_ptr<Blob> output_ = make_shared<Blob>(n * 3 * h * w * sizeof(u8));
    u8 *             input   = (u8 *)input_->data;
    u8 *             output  = (u8 *)output_->data;
    for (int i = 0; i < n; i++) {
        for (int loop = 0; loop < h * w; loop++) {
            if (!exchange) {
                output[3 * h * w * i + loop] = input[3 * h * w * i + 3 * loop];
                output[3 * h * w * i + loop + w * h] =
                    input[3 * h * w * i + 3 * loop + 1];
                output[3 * h * w * i + loop + 2 * w * h] =
                    input[3 * h * w * i + 3 * loop + 2];
            } else {
                output[3 * h * w * i + loop] =
                    input[3 * h * w * i + 3 * loop + 2];
                output[3 * h * w * i + loop + w * h] =
                    input[3 * h * w * i + 3 * loop + 1];
                output[3 * h * w * i + loop + 2 * w * h] =
                    input[3 * h * w * i + 3 * loop];
            }
        }
    }
    return output_;
}

shared_ptr<Blob> convert_unpack_1N_to_pack_1N(
    shared_ptr<Blob> input_, int n, int h, int w, bool exchange) {
    shared_ptr<Blob> output_ = make_shared<Blob>(n * 3 * h * w * sizeof(u8));
    u8 *             input   = (u8 *)input_->data;
    u8 *             output  = (u8 *)output_->data;
    for (int i = 0; i < n; i++) {
        for (int loop = 0; loop < h * w; loop++) {
            if (!exchange) {
                output[3 * h * w * i + 3 * loop] = input[3 * h * w * i + loop];
                output[3 * h * w * i + 3 * loop + 1] =
                    input[3 * h * w * i + loop + w * h];
                output[3 * h * w * i + 3 * loop + 2] =
                    input[3 * h * w * i + loop + 2 * w * h];
            } else {
                output[3 * h * w * i + 3 * loop] =
                    input[3 * h * w * i + loop + 2 * w * h];
                output[3 * h * w * i + 3 * loop + 1] =
                    input[3 * h * w * i + loop + w * h];
                output[3 * h * w * i + 3 * loop + 2] =
                    input[3 * h * w * i + loop];
            }
        }
    }
    return output_;
}

shared_ptr<Blob>
convert_pack_1N_exchange(shared_ptr<Blob> input_, int n, int h, int w) {
    shared_ptr<Blob> output_ = make_shared<Blob>(n * 3 * h * w * sizeof(u8));
    u8 *             input   = (u8 *)input_->data;
    u8 *             output  = (u8 *)output_->data;
    for (int i = 0; i < n; i++) {
        for (int loop = 0; loop < h * w; loop++) {
            output[3 * h * w * i + 3 * loop] =
                input[3 * h * w * i + 3 * loop + 2];
            output[3 * h * w * i + 3 * loop + 1] =
                input[3 * h * w * i + 3 * loop + 1];
            output[3 * h * w * i + 3 * loop + 2] =
                input[3 * h * w * i + 3 * loop];
        }
    }
    return output_;
}

shared_ptr<Blob>
convert_unpack_1N_exchange(shared_ptr<Blob> input_, int n, int h, int w) {
    shared_ptr<Blob> output_ = make_shared<Blob>(n * 3 * h * w * sizeof(u8));
    u8 *             input   = (u8 *)input_->data;
    u8 *             output  = (u8 *)output_->data;
    for (int i = 0; i < n; i++) {
        memcpy(
            output + 3 * h * w * i, input + 3 * h * w * i + h * w * 2, h * w);
        memcpy(output + 3 * h * w * i + h * w,
               input + 3 * h * w * i + h * w,
               h * w);
        memcpy(
            output + 3 * h * w * i + 2 * h * w, input + 3 * h * w * i, h * w);
    }
    return output_;
}

void calculate_yuv2rgb(
    float y, float u, float v, float *r, float *g, float *b) {
    y    = 1.16438 * (y - 16);
    u    = u - 128;
    v    = v - 128;
    r[0] = y + 1.59603 * v;
    g[0] = y - 0.39176 * u - 0.81297 * v;
    b[0] = y + 2.01723 * u;
    CLIP(r[0], 255, 0);
    CLIP(g[0], 255, 0);
    CLIP(b[0], 255, 0);
}

shared_ptr<Blob> convert_YUV444_to_RGB_1N_unpack(shared_ptr<Blob> input_,
                                                 int              IMAGE_N,
                                                 int              IMAGE_H,
                                                 int              IMAGE_W) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(IMAGE_N * IMAGE_H * IMAGE_W * 3 * sizeof(u8));

    u8 *raw_ptr       = (u8 *)input_->data;
    u8 *reference_ptr = (u8 *)output_->data;

    for (int n = 0; n < IMAGE_N; n++) {
        for (int h = 0; h < IMAGE_H; h++) {
            for (int w = 0; w < IMAGE_W; w++) {
                float y =
                    (float)raw_ptr[n * 3 * IMAGE_H * IMAGE_W + IMAGE_W * h + w];
                float u = (float)raw_ptr[n * 3 * IMAGE_H * IMAGE_W +
                                         IMAGE_H * IMAGE_W + IMAGE_W * h + w];
                float v =
                    (float)raw_ptr[n * 3 * IMAGE_H * IMAGE_W +
                                   2 * IMAGE_H * IMAGE_W + IMAGE_W * h + w];
                float r, g, b;
                calculate_yuv2rgb(y, u, v, &r, &g, &b);

                // temp_y[n * IMAGE_H * IMAGE_W + IMAGE_W * h + w] = y;
                reference_ptr[n * IMAGE_H * IMAGE_W * 3 + IMAGE_W * h + w] =
                    (u8)r;
                reference_ptr[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                              IMAGE_W * h + w]                         = (u8)g;
                reference_ptr[n * IMAGE_H * IMAGE_W * 3 +
                              IMAGE_H * IMAGE_W * 2 + IMAGE_W * h + w] = (u8)b;
            }
        }
    }
    return output_;
}

shared_ptr<Blob> convert_unpack_1N_RGB_to_YUV444(shared_ptr<Blob> input_,
                                                 int              IMAGE_N,
                                                 int              IMAGE_H,
                                                 int              IMAGE_W) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(IMAGE_N * IMAGE_H * IMAGE_W * 3 * sizeof(u8));

    u8 *raw_ptr       = (u8 *)input_->data;
    u8 *reference_ptr = (u8 *)output_->data;

    for (int n = 0; n < IMAGE_N; n++) {
        for (int h = 0; h < IMAGE_H; h++) {
            for (int w = 0; w < IMAGE_W; w++) {
                float r =
                    (float)raw_ptr[n * 3 * IMAGE_H * IMAGE_W + IMAGE_W * h + w];
                float g = (float)raw_ptr[n * 3 * IMAGE_H * IMAGE_W +
                                         IMAGE_H * IMAGE_W + IMAGE_W * h + w];
                float b =
                    (float)raw_ptr[n * 3 * IMAGE_H * IMAGE_W +
                                   2 * IMAGE_H * IMAGE_W + IMAGE_W * h + w];
                float y, u, v;
                y = (0.257 * r + 0.504 * g + 0.098 * b + 16);
                u = (-0.148 * r - 0.291 * g + 0.439 * b + 128);
                v = (0.439 * r - 0.368 * g - 0.071 * b + 128);
                CLIP(y, 255, 0);
                CLIP(u, 255, 0);
                CLIP(v, 255, 0);
                // temp_y[n * IMAGE_H * IMAGE_W + IMAGE_W * h + w] = y;
                reference_ptr[n * IMAGE_H * IMAGE_W * 3 + IMAGE_W * h + w] =
                    (u8)y;
                reference_ptr[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                              IMAGE_W * h + w]                         = (u8)u;
                reference_ptr[n * IMAGE_H * IMAGE_W * 3 +
                              IMAGE_H * IMAGE_W * 2 + IMAGE_W * h + w] = (u8)v;
            }
        }
    }
    return output_;
}

shared_ptr<Blob> convert_YUV444_to_NV12(shared_ptr<Blob> input_,
                                        int              IMAGE_N,
                                        int              IMAGE_H,
                                        int              IMAGE_W) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(IMAGE_N * IMAGE_H * IMAGE_W * 3 / 2 * sizeof(u8));

    u8 *raw_ptr = (u8 *)input_->data;
    u8 *out     = (u8 *)output_->data;

    for (int n = 0; n < IMAGE_N; n++) {
        memcpy(out + n * IMAGE_H * IMAGE_W * 3 / 2,
               raw_ptr + n * IMAGE_H * IMAGE_W * 3,
               IMAGE_H * IMAGE_W);
        for (int h = 0; h < IMAGE_H / 2; h++) {
            for (int w = 0; w < IMAGE_W / 2; w++) {
                u16 u = raw_ptr[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                                IMAGE_W * h * 2 + w * 2] +
                        raw_ptr[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                                IMAGE_W * (h * 2 + 1) + w * 2 + 1] +
                        raw_ptr[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                                IMAGE_W * h * 2 + w * 2 + 1] +
                        raw_ptr[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                                IMAGE_W * (h * 2 + 1) + w * 2];

                u16 v =
                    raw_ptr[n * IMAGE_H * IMAGE_W * 3 + 2 * IMAGE_H * IMAGE_W +
                            IMAGE_W * h * 2 + w * 2] +
                    raw_ptr[n * IMAGE_H * IMAGE_W * 3 + 2 * IMAGE_H * IMAGE_W +
                            IMAGE_W * (h * 2 + 1) + w * 2 + 1] +
                    raw_ptr[n * IMAGE_H * IMAGE_W * 3 + 2 * IMAGE_H * IMAGE_W +
                            IMAGE_W * h * 2 + w * 2 + 1] +
                    raw_ptr[n * IMAGE_H * IMAGE_W * 3 + 2 * IMAGE_H * IMAGE_W +
                            IMAGE_W * (h * 2 + 1) + w * 2];
                u                            = u >> 2;
                v                            = v >> 2;
                out[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W +
                    IMAGE_W * h + w * 2]     = (u8)u;
                out[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W +
                    IMAGE_W * h + w * 2 + 1] = (u8)v;
            }
        }
    }
    return output_;
}

shared_ptr<Blob> convert_NV12_to_YUV444(shared_ptr<Blob> input_,
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

shared_ptr<Blob> convert_NV12_to_YUV420P(shared_ptr<Blob> input_,
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

shared_ptr<Blob> convert_YUV420P_to_NV12(shared_ptr<Blob> input_,
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
            // u8 u = raw_ptr[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H * IMAGE_W
            // + 2 * i]; u8 v = raw_ptr[n * IMAGE_H * IMAGE_W * 3 / 2 + IMAGE_H
            // * IMAGE_W + 2 * i + 1];

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

shared_ptr<Blob> convert_NV16_to_YUV444(shared_ptr<Blob> input_,
                                        int              IMAGE_N,
                                        int              IMAGE_H,
                                        int              IMAGE_W) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(IMAGE_N * IMAGE_H * IMAGE_W * 3 * sizeof(u8));

    u8 *raw_ptr = (u8 *)input_->data;
    u8 *out     = (u8 *)output_->data;

    for (int n = 0; n < IMAGE_N; n++) {
        memcpy(out + n * IMAGE_H * IMAGE_W * 3,
               raw_ptr + n * IMAGE_H * IMAGE_W * 2,
               IMAGE_H * IMAGE_W);
        for (int h = 0; h < IMAGE_H; h++) {
            for (int w = 0; w < IMAGE_W; w++) {
                u8 u = raw_ptr[n * IMAGE_H * IMAGE_W * 2 + IMAGE_H * IMAGE_W +
                               IMAGE_W * h + (w / 2) * 2];
                u8 v = raw_ptr[n * IMAGE_H * IMAGE_W * 2 + IMAGE_H * IMAGE_W +
                               IMAGE_W * h + (w / 2) * 2 + 1];

                out[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                    IMAGE_W * h + w] = (u8)u;
                out[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W * 2 +
                    IMAGE_W * h + w] = (u8)v;
            }
        }
    }
    return output_;
}

shared_ptr<Blob> convert_YUV444_to_NV16(shared_ptr<Blob> input_,
                                        int              IMAGE_N,
                                        int              IMAGE_H,
                                        int              IMAGE_W) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(IMAGE_N * IMAGE_H * IMAGE_W * 2 * sizeof(u8));

    u8 *raw_ptr = (u8 *)input_->data;
    u8 *out     = (u8 *)output_->data;

    for (int n = 0; n < IMAGE_N; n++) {
        memcpy(out + n * IMAGE_H * IMAGE_W * 2,
               raw_ptr + n * IMAGE_H * IMAGE_W * 3,
               IMAGE_H * IMAGE_W);
        for (int h = 0; h < IMAGE_H; h++) {
            for (int w = 0; w < IMAGE_W / 2; w++) {
                u16 u = raw_ptr[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                                IMAGE_W * h + w * 2] +
                        raw_ptr[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                                IMAGE_W * h + w * 2 + 1];

                u16 v =
                    raw_ptr[n * IMAGE_H * IMAGE_W * 3 + 2 * IMAGE_H * IMAGE_W +
                            IMAGE_W * h + w * 2] +
                    raw_ptr[n * IMAGE_H * IMAGE_W * 3 + 2 * IMAGE_H * IMAGE_W +
                            IMAGE_W * h + w * 2 + 1];
                u                            = u >> 1;
                v                            = v >> 1;
                out[n * IMAGE_H * IMAGE_W * 2 + IMAGE_H * IMAGE_W +
                    IMAGE_W * h + w * 2]     = (u8)u;
                out[n * IMAGE_H * IMAGE_W * 2 + IMAGE_H * IMAGE_W +
                    IMAGE_W * h + w * 2 + 1] = (u8)v;
            }
        }
    }
    return output_;
}


shared_ptr<Blob> convert_NV16_to_YUV422P(shared_ptr<Blob> input_,
                                        int              IMAGE_N,
                                        int              IMAGE_H,
                                        int              IMAGE_W) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(IMAGE_N * IMAGE_H * IMAGE_W * 3 * sizeof(u8));

    u8 *raw_ptr = (u8 *)input_->data;
    u8 *out     = (u8 *)output_->data;

    for (int n = 0; n < IMAGE_N; n++) {
        memcpy(out + n * IMAGE_H * IMAGE_W * 3,
               raw_ptr + n * IMAGE_H * IMAGE_W * 2,
               IMAGE_H * IMAGE_W);
        for (int h = 0; h < IMAGE_H; h++) {
            for (int w = 0; w < IMAGE_W / 2; w++) {
                u8 u = raw_ptr[n * IMAGE_H * IMAGE_W * 2 + IMAGE_H * IMAGE_W +
                               IMAGE_W * h + w];
                u8 v = raw_ptr[n * IMAGE_H * IMAGE_W * 2 + IMAGE_H * IMAGE_W +
                               IMAGE_W * h + w + 1];

                out[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                    IMAGE_W / 2 * h + w] = (u8)u;
                out[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W * 2 / 3 +
                    IMAGE_W / 2 * h + w] = (u8)v;
            }
        }
    }
    return output_;
}

shared_ptr<Blob> convert_YUV422P_to_NV16(shared_ptr<Blob> input_,
                                        int              IMAGE_N,
                                        int              IMAGE_H,
                                        int              IMAGE_W) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(IMAGE_N * IMAGE_H * IMAGE_W * 2 * sizeof(u8));

    u8 *raw_ptr = (u8 *)input_->data;
    u8 *out     = (u8 *)output_->data;

    for (int n = 0; n < IMAGE_N; n++) {
        memcpy(out + n * IMAGE_H * IMAGE_W * 2,
               raw_ptr + n * IMAGE_H * IMAGE_W * 3,
               IMAGE_H * IMAGE_W);
        for (int h = 0; h < IMAGE_H; h++) {
            for (int w = 0; w < IMAGE_W / 2; w++) {
                u16 u = raw_ptr[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                                IMAGE_W / 2 * h + w];
                u16 v = raw_ptr[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W * 3 / 2 +
                                IMAGE_W / 2 * h + w];
                out[n * IMAGE_H * IMAGE_W * 2 + IMAGE_H * IMAGE_W +
                    IMAGE_W * h + w]     = (u8)u;
                out[n * IMAGE_H * IMAGE_W * 2 + IMAGE_H * IMAGE_W +
                    IMAGE_W * h + w + 1] = (u8)v;
            }
        }
    }
    return output_;
}

shared_ptr<Blob> convert_NV24_to_YUV444(shared_ptr<Blob> input_,
                                        int              IMAGE_N,
                                        int              IMAGE_H,
                                        int              IMAGE_W) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(IMAGE_N * IMAGE_H * IMAGE_W * 3 * sizeof(u8));

    u8 *raw_ptr = (u8 *)input_->data;
    u8 *out     = (u8 *)output_->data;

    for (int n = 0; n < IMAGE_N; n++) {
        memcpy(out + n * IMAGE_H * IMAGE_W * 3,
               raw_ptr + n * IMAGE_H * IMAGE_W * 2,
               IMAGE_H * IMAGE_W);
        for (int h = 0; h < IMAGE_H; h++) {
            for (int w = 0; w < IMAGE_W; w++) {
                u8 u = raw_ptr[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                               IMAGE_W * 2 * h + w * 2];
                u8 v = raw_ptr[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                               IMAGE_W * 2 * h + w * 2 + 1];

                out[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                    IMAGE_W * h + w] = (u8)u;
                out[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W * 2 +
                    IMAGE_W * h + w] = (u8)v;
            }
        }
    }
    return output_;
}

shared_ptr<Blob> convert_YUV444_to_NV24(shared_ptr<Blob> input_,
                                        int              IMAGE_N,
                                        int              IMAGE_H,
                                        int              IMAGE_W) {
    shared_ptr<Blob> output_ =
        make_shared<Blob>(IMAGE_N * IMAGE_H * IMAGE_W * 2 * sizeof(u8));

    u8 *raw_ptr = (u8 *)input_->data;
    u8 *out     = (u8 *)output_->data;

    for (int n = 0; n < IMAGE_N; n++) {
        memcpy(out + n * IMAGE_H * IMAGE_W * 2,
               raw_ptr + n * IMAGE_H * IMAGE_W * 3,
               IMAGE_H * IMAGE_W);
        for (int h = 0; h < IMAGE_H; h++) {
            for (int w = 0; w < IMAGE_W; w++) {
                u16 u = raw_ptr[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                                IMAGE_W * h + w];
                u16 v = raw_ptr[n * IMAGE_H * IMAGE_W * 3 + 2 * IMAGE_H * IMAGE_W +
                                IMAGE_W * h + w];
                out[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                    IMAGE_W * 2 * h + w * 2]     = (u8)u;
                out[n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                    IMAGE_W * 2 * h + w * 2 + 1] = (u8)v;
            }
        }
    }
    return output_;
}

shared_ptr<Blob> uv_exchange(shared_ptr<Blob> input_,
                             int              IMAGE_N,
                             int              IMAGE_H,
                             int              IMAGE_W,
                             int              uv_factor) {
    // uv_factor 1 : nv12, 2 : nv16
    shared_ptr<Blob> output_ =
        make_shared<Blob>(IMAGE_N * 3 * IMAGE_H * IMAGE_W * sizeof(u8));
    u8 *input    = (u8 *)input_->data;
    u8 *output   = (u8 *)output_->data;
    int n_stride = IMAGE_H * IMAGE_W * (2 + uv_factor) / 2;
    for (int n = 0; n < IMAGE_N; n++) {
        memcpy(output + n_stride * n, input + n_stride * n, IMAGE_H * IMAGE_W);
        for (int i = 0; i < IMAGE_H * IMAGE_W / (2 / uv_factor); i = i + 2) {
            output[n_stride * n + IMAGE_H * IMAGE_W + i] =
                input[n_stride * n + IMAGE_H * IMAGE_W + i + 1];
            output[n_stride * n + IMAGE_H * IMAGE_W + i + 1] =
                input[n_stride * n + IMAGE_H * IMAGE_W + i];
        }
    }
    return output_;
}

int array_cmp_u8(
    u8 *p_exp, u8 *p_got, int len, const char *info_label, u8 delta) {
    int idx = 0;
    for (idx = 0; idx < len; idx++) {
        // compare abs
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

static void get_format(int                       token,
                       bm_image_format_ext *     image_format_,
                       bm_image_data_format_ext *data_format_,
                       char *                    format_name) {
    bm_image_format_ext      image_format = FORMAT_BGR_PLANAR;
    bm_image_data_format_ext data_format  = DATA_TYPE_EXT_1N_BYTE;
    switch (token) {
        case 0:
            image_format = FORMAT_BGR_PLANAR;
            data_format  = DATA_TYPE_EXT_1N_BYTE;
            memcpy((void *)format_name,
                   "1N BGR unpack int8",
                   strlen("1N BGR unpack int8") + 1);
            break;
        case 1:
            image_format = FORMAT_RGB_PLANAR;
            data_format  = DATA_TYPE_EXT_1N_BYTE;
            memcpy((void *)format_name,
                   "1N RGB unpack int8",
                   strlen("1N RGB unpack int8") + 1);
            break;
        case 2:
            image_format = FORMAT_BGR_PACKED;
            data_format  = DATA_TYPE_EXT_1N_BYTE;
            memcpy((void *)format_name,
                   "1N BGR pack int8",
                   strlen("1N BGR pack int8") + 1);
            break;
        case 3:
            image_format = FORMAT_RGB_PACKED;
            data_format  = DATA_TYPE_EXT_1N_BYTE;
            memcpy((void *)format_name,
                   "1N RGB pack int8",
                   strlen("1N RGB pack int8") + 1);
            break;
        case 4:
            image_format = FORMAT_BGR_PLANAR;
            data_format  = DATA_TYPE_EXT_4N_BYTE;
            memcpy((void *)format_name,
                   "4N BGR unpack int8",
                   strlen("4N BGR unpack int8") + 1);
            break;
        case 5:
            image_format = FORMAT_RGB_PLANAR;
            data_format  = DATA_TYPE_EXT_4N_BYTE;
            memcpy((void *)format_name,
                   "4N RGB unpack int8",
                   strlen("4N RGB unpack int8") + 1);
            break;
        case 6:
            image_format = FORMAT_BGR_PACKED;
            data_format  = DATA_TYPE_EXT_4N_BYTE;
            memcpy((void *)format_name,
                   "4N BGR pack int8",
                   strlen("4N BGR pack int8") + 1);
            break;
        case 7:
            image_format = FORMAT_RGB_PACKED;
            data_format  = DATA_TYPE_EXT_4N_BYTE;
            memcpy((void *)format_name,
                   "4N RGB pack int8",
                   strlen("4N RGB pack int8") + 1);
            break;
        case 8:
            image_format = FORMAT_BGR_PLANAR;
            data_format  = DATA_TYPE_EXT_FLOAT32;
            memcpy((void *)format_name,
                   "1N BGR unpack float",
                   strlen("1N BGR unpack float") + 1);
            break;
        case 9:
            image_format = FORMAT_RGB_PLANAR;
            data_format  = DATA_TYPE_EXT_FLOAT32;
            memcpy((void *)format_name,
                   "1N RGB unpack float",
                   strlen("1N RGB unpack float") + 1);
            break;
        case 10:
            image_format = FORMAT_BGR_PACKED;
            data_format  = DATA_TYPE_EXT_FLOAT32;
            memcpy((void *)format_name,
                   "1N BGR pack float",
                   strlen("1N BGR pack float") + 1);
            break;
        case 11:
            image_format = FORMAT_RGB_PACKED;
            data_format  = DATA_TYPE_EXT_FLOAT32;
            memcpy((void *)format_name,
                   "1N RGB pack float",
                   strlen("1N RGB pack float") + 1);
            break;
        case 12:
            image_format = FORMAT_BGRP_SEPARATE;
            data_format  = DATA_TYPE_EXT_1N_BYTE;
            memcpy((void *)format_name,
                   "1N BGR separate int8",
                   strlen("1N BGR separate int8") + 1);
            break;
        case 13:
            image_format = FORMAT_RGBP_SEPARATE;
            data_format  = DATA_TYPE_EXT_1N_BYTE;
            memcpy((void *)format_name,
                   "1N RGB separate int8",
                   strlen("1N RGB separate int8") + 1);
            break;
        case 14:
            image_format = FORMAT_BGRP_SEPARATE;
            data_format  = DATA_TYPE_EXT_4N_BYTE;
            memcpy((void *)format_name,
                   "4N BGR separate int8",
                   strlen("4N BGR separate int8") + 1);
            break;
        case 15:
            image_format = FORMAT_RGBP_SEPARATE;
            data_format  = DATA_TYPE_EXT_4N_BYTE;
            memcpy((void *)format_name,
                   "4N RGB separate int8",
                   strlen("4N RGB separate int8") + 1);
            break;
        case 16:
            image_format = FORMAT_BGRP_SEPARATE;
            data_format  = DATA_TYPE_EXT_FLOAT32;
            memcpy((void *)format_name,
                   "1N BGR separate fp32",
                   strlen("1N BGR separate fp32") + 1);
            break;
        case 17:
            image_format = FORMAT_RGBP_SEPARATE;
            data_format  = DATA_TYPE_EXT_FLOAT32;
            memcpy((void *)format_name,
                   "1N RGB separate fp32",
                   strlen("1N RGB separate fp32") + 1);
            break;
        /////////////////////////
        case 18:
            image_format = FORMAT_NV12;
            data_format  = DATA_TYPE_EXT_1N_BYTE;
            memcpy((void *)format_name, "NV12", strlen("NV12") + 1);
            break;
        case 19:
            image_format = FORMAT_NV21;
            data_format  = DATA_TYPE_EXT_1N_BYTE;
            memcpy((void *)format_name, "NV21", strlen("NV21") + 1);
            break;
        case 20:
            image_format = FORMAT_NV16;
            data_format  = DATA_TYPE_EXT_1N_BYTE;
            memcpy((void *)format_name, "NV16", strlen("NV16") + 1);
            break;
        case 21:
            image_format = FORMAT_NV61;
            data_format  = DATA_TYPE_EXT_1N_BYTE;
            memcpy((void *)format_name, "NV61", strlen("NV61") + 1);
            break;
        case 22:
            image_format = FORMAT_YUV420P;
            data_format  = DATA_TYPE_EXT_1N_BYTE;
            memcpy((void *)format_name, "YUV420P", strlen("YUV420P") + 1);
            break;
        case 23:
            image_format = FORMAT_YUV422P;
            data_format  = DATA_TYPE_EXT_1N_BYTE;
            memcpy((void *)format_name, "YUV422P", strlen("YUV422P") + 1);
            break;
        case 24:
            image_format = FORMAT_YUV444P;
            data_format  = DATA_TYPE_EXT_1N_BYTE;
            memcpy((void *)format_name, "YUV444P", strlen("YUV444P") + 1);
            break;
        case 25:
            image_format = FORMAT_NV24;
            data_format  = DATA_TYPE_EXT_1N_BYTE;
            memcpy((void *)format_name, "NV24", strlen("NV24") + 1);
            break;
    }
    image_format_[0] = image_format;
    data_format_[0]  = data_format;
}

shared_ptr<Blob> convert_from_1N_unpack_BGR(
    shared_ptr<Blob> src, int src_format, int n, int h, int w) {
    shared_ptr<Blob> dst;
    switch (src_format) {
        case 0:
        case 12:
            // image_format = FORMAT_BGR_PLANAR;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = src;
            return dst;
        case 1:
        case 13:
            // image_format = FORMAT_RGB_PLANAR;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_unpack_1N_exchange(src, n, h, w);
            return dst;
        case 2:
            // image_format = FORMAT_BGR_PACKED;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_unpack_1N_to_pack_1N(src, n, h, w, false);
            return dst;
        case 3:
            // image_format = FORMAT_RGB_PACKED;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_unpack_1N_to_pack_1N(src, n, h, w, true);
            return dst;
        case 4:
        case 14:
            // image_format = FORMAT_BGR_PLANAR;
            // data_format = DATA_TYPE_EXT_4N_BYTE;
            dst = convert_1N_INT8_to_4N_INT8(src, n, 3, h, w);
            return dst;
        case 5:
        case 15:
            // image_format = FORMAT_RGB_PLANAR;
            // data_format = DATA_TYPE_EXT_4N_BYTE;
            dst = convert_unpack_1N_exchange(src, n, h, w);
            dst = convert_1N_INT8_to_4N_INT8(dst, n, 3, h, w);
            return dst;
        case 6:
            // image_format = FORMAT_BGR_PACKED;
            // data_format = DATA_TYPE_EXT_4N_BYTE;
            dst = convert_unpack_1N_to_pack_1N(src, n, h, w, false);
            dst = convert_1N_INT8_to_4N_INT8(dst, n, 3, h, w);
            return dst;
        case 7:
            // image_format = FORMAT_RGB_PACKED;
            // data_format = DATA_TYPE_EXT_4N_BYTE;
            dst = convert_unpack_1N_to_pack_1N(src, n, h, w, true);
            dst = convert_1N_INT8_to_4N_INT8(dst, n, 3, h, w);
            return dst;
        case 8:
        case 16:
            // image_format = FORMAT_BGR_PLANAR;
            // data_format = DATA_TYPE_EXT_FLOAT32;
            dst = convert_1N_INT8_to_Float(src, n, 3, h, w);
            return dst;
        case 9:
        case 17:
            // image_format = FORMAT_RGB_PLANAR;
            // data_format = DATA_TYPE_EXT_FLOAT32;
            dst = convert_unpack_1N_exchange(src, n, h, w);
            dst = convert_1N_INT8_to_Float(dst, n, 3, h, w);
            return dst;
        case 10:
            // image_format = FORMAT_BGR_PACKED;
            // data_format = DATA_TYPE_EXT_FLOAT32;
            dst = convert_unpack_1N_to_pack_1N(src, n, h, w, false);
            dst = convert_1N_INT8_to_Float(dst, n, 3, h, w);
            return dst;
        case 11:
            // image_format = FORMAT_RGB_PACKED;
            // data_format = DATA_TYPE_EXT_FLOAT32;
            dst = convert_unpack_1N_to_pack_1N(src, n, h, w, true);
            dst = convert_1N_INT8_to_Float(dst, n, 3, h, w);
            return dst;
        /////////////////////////
        case 18:
            // image_format = FORMAT_NV12;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_unpack_1N_exchange(src, n, h, w);
            dst = convert_unpack_1N_RGB_to_YUV444(dst, n, h, w);
            dst = convert_YUV444_to_NV12(dst, n, h, w);
            return dst;
        case 19:
            // image_format = FORMAT_NV21;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_unpack_1N_exchange(src, n, h, w);
            dst = convert_unpack_1N_RGB_to_YUV444(dst, n, h, w);
            dst = convert_YUV444_to_NV12(dst, n, h, w);
            dst = uv_exchange(dst, n, h, w, 1);
            return dst;
        case 20:
            // image_format = FORMAT_NV16;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_unpack_1N_exchange(src, n, h, w);
            dst = convert_unpack_1N_RGB_to_YUV444(dst, n, h, w);
            dst = convert_YUV444_to_NV16(dst, n, h, w);
            return dst;
        case 21:
            // image_format = FORMAT_NV61;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_unpack_1N_exchange(src, n, h, w);
            dst = convert_unpack_1N_RGB_to_YUV444(dst, n, h, w);
            dst = convert_YUV444_to_NV16(dst, n, h, w);
            dst = uv_exchange(dst, n, h, w, 2);
            return dst;
        case 22:
            // image_format = FORMAT_YUV420P;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_unpack_1N_exchange(src, n, h, w);
            dst = convert_unpack_1N_RGB_to_YUV444(dst, n, h, w);
            dst = convert_YUV444_to_NV12(dst, n, h, w);
            dst = convert_NV12_to_YUV420P(dst, IMAGE_N, IMAGE_H, IMAGE_W);
            return dst;
        case 23:
            // image_format = FORMAT_YUV422P;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_unpack_1N_exchange(src, n, h, w);
            dst = convert_unpack_1N_RGB_to_YUV444(dst, n, h, w);
            dst = convert_YUV444_to_NV16(dst, n, h, w);
            dst = convert_NV16_to_YUV422P(dst, n, h, w);
            return dst;
        case 24:
            // image_format = FORMAT_YUV444P;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_unpack_1N_exchange(src, n, h, w);
            dst = convert_unpack_1N_RGB_to_YUV444(dst, n, h, w);
            return dst;
        case 25:
            // image_format = FORMAT_NV24;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_unpack_1N_exchange(src, n, h, w);
            dst = convert_unpack_1N_RGB_to_YUV444(dst, n, h, w);
            dst = convert_YUV444_to_NV24(dst, n, h, w);
            return dst;
    }
    return src;
};

shared_ptr<Blob> convert_to_1N_unpack_BGR(
    shared_ptr<Blob> src, int src_format, int n, int h, int w) {
    shared_ptr<Blob> dst;
    switch (src_format) {
        case 0:
        case 12:
            // image_format = FORMAT_BGR_PLANAR;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = src;
            return dst;
        case 1:
        case 13:
            // image_format = FORMAT_RGB_PLANAR;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_unpack_1N_exchange(src, n, h, w);
            return dst;
        case 2:
            // image_format = FORMAT_BGR_PACKED;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_pack_1N_to_unpack_1N(src, n, h, w, false);
            return dst;
        case 3:
            // image_format = FORMAT_RGB_PACKED;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_pack_1N_to_unpack_1N(src, n, h, w, true);
            return dst;
        case 4:
        case 14:
            // image_format = FORMAT_BGR_PLANAR;
            // data_format = DATA_TYPE_EXT_4N_BYTE;
            dst = convert_4N_INT8_to_1N_INT8(src, n, 3, h, w);
            return dst;
        case 5:
        case 15:
            // image_format = FORMAT_RGB_PLANAR;
            // data_format = DATA_TYPE_EXT_4N_BYTE;
            dst = convert_4N_INT8_to_1N_INT8(src, n, 3, h, w);
            dst = convert_unpack_1N_exchange(dst, n, h, w);
            return dst;
        case 6:
            // image_format = FORMAT_BGR_PACKED;
            // data_format = DATA_TYPE_EXT_4N_BYTE;
            dst = convert_4N_INT8_to_1N_INT8(src, n, 3, h, w);
            dst = convert_pack_1N_to_unpack_1N(dst, n, h, w, false);
            return dst;
        case 7:
            // image_format = FORMAT_RGB_PACKED;
            // data_format = DATA_TYPE_EXT_4N_BYTE;
            dst = convert_4N_INT8_to_1N_INT8(src, n, 3, h, w);
            dst = convert_pack_1N_to_unpack_1N(dst, n, h, w, true);
            return dst;
        case 8:
        case 16:
            // image_format = FORMAT_BGR_PLANAR;
            // data_format = DATA_TYPE_EXT_FLOAT32;
            dst = convert_Float_to_1N_INT8(src, n, 3, h, w);
            return dst;
        case 9:
        case 17:
            // image_format = FORMAT_RGB_PLANAR;
            // data_format = DATA_TYPE_EXT_FLOAT32;
            dst = convert_Float_to_1N_INT8(src, n, 3, h, w);
            dst = convert_unpack_1N_exchange(dst, n, h, w);
            return dst;
        case 10:
            // image_format = FORMAT_BGR_PACKED;
            // data_format = DATA_TYPE_EXT_FLOAT32;
            dst = convert_Float_to_1N_INT8(src, n, 3, h, w);
            dst = convert_pack_1N_to_unpack_1N(dst, n, h, w, false);
            return dst;
        case 11:
            // image_format = FORMAT_RGB_PACKED;
            // data_format = DATA_TYPE_EXT_FLOAT32;
            dst = convert_Float_to_1N_INT8(src, n, 3, h, w);
            dst = convert_pack_1N_to_unpack_1N(dst, n, h, w, true);
            return dst;
        /////////////////////////
        case 18:
            // image_format = FORMAT_NV12;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_NV12_to_YUV444(src, n, h, w);
            dst = convert_YUV444_to_RGB_1N_unpack(dst, n, h, w);
            dst = convert_unpack_1N_exchange(dst, n, h, w);
            return dst;
        case 19:
            // image_format = FORMAT_NV21;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = uv_exchange(src, n, h, w, 1);
            dst = convert_NV12_to_YUV444(dst, n, h, w);
            dst = convert_YUV444_to_RGB_1N_unpack(dst, n, h, w);
            dst = convert_unpack_1N_exchange(dst, n, h, w);
            return dst;
        case 20:
            // image_format = FORMAT_NV16;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_NV16_to_YUV444(src, n, h, w);
            dst = convert_YUV444_to_RGB_1N_unpack(dst, n, h, w);
            dst = convert_unpack_1N_exchange(dst, n, h, w);
            return dst;
        case 21:
            // image_format = FORMAT_NV61;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = uv_exchange(src, n, h, w, 2);
            dst = convert_NV16_to_YUV444(dst, n, h, w);
            dst = convert_YUV444_to_RGB_1N_unpack(dst, n, h, w);
            dst = convert_unpack_1N_exchange(dst, n, h, w);
            return dst;
        case 22:
            // image_format = FORMAT_YUV420P;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_YUV420P_to_NV12(src, IMAGE_N, IMAGE_H, IMAGE_W);
            dst = convert_NV12_to_YUV444(dst, n, h, w);
            dst = convert_YUV444_to_RGB_1N_unpack(dst, n, h, w);
            dst = convert_unpack_1N_exchange(dst, n, h, w);
            return dst;
        case 23:
            // image_format = FORMAT_YUV422P;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_YUV422P_to_NV16(src, n, h, w);
            dst = convert_NV16_to_YUV444(dst, n, h, w);
            dst = convert_YUV444_to_RGB_1N_unpack(dst, n, h, w);
            dst = convert_unpack_1N_exchange(dst, n, h, w);
            return dst;
        case 24:
            // image_format = FORMAT_YUV444P;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_YUV444_to_RGB_1N_unpack(src, n, h, w);
            dst = convert_unpack_1N_exchange(dst, n, h, w);
            return dst;
        case 25:
            // image_format = FORMAT_NV24;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_NV24_to_YUV444(src, n, h, w);
            dst = convert_YUV444_to_RGB_1N_unpack(dst, n, h, w);
            dst = convert_unpack_1N_exchange(dst, n, h, w);
            return dst;
    }
    return src;
};

shared_ptr<Blob>
yuv_convert_to_YUV444(shared_ptr<Blob> src, int format, int n, int h, int w) {
    shared_ptr<Blob> dst;
    switch (format) {
        /////////////////////////
        case 18:
            // image_format = FORMAT_NV12;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_NV12_to_YUV444(src, n, h, w);
            return dst;
        case 19:
            // image_format = FORMAT_NV21;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = uv_exchange(src, n, h, w, 1);
            dst = convert_NV12_to_YUV444(dst, n, h, w);
            return dst;
        case 20:
            // image_format = FORMAT_NV16;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_NV16_to_YUV444(src, n, h, w);
            return dst;
        case 21:
            // image_format = FORMAT_NV61;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = uv_exchange(src, n, h, w, 2);
            dst = convert_NV16_to_YUV444(dst, n, h, w);
            return dst;
        case 22:
            // image_format = FORMAT_YUV420P;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_YUV420P_to_NV12(src, IMAGE_N, IMAGE_H, IMAGE_W);
            dst = convert_NV12_to_YUV444(dst, n, h, w);
            return dst;
        case 23:
            // image_format = FORMAT_YUV422P;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_YUV422P_to_NV16(src, IMAGE_N, IMAGE_H, IMAGE_W);
            dst = convert_NV16_to_YUV444(dst, n, h, w);
            return dst;
        case 24:
            // image_format = FORMAT_YUV444P;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = src;
            return dst;
        case 25:
            // image_format = FORMAT_NV24;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_NV24_to_YUV444(src, n, h, w);
            return dst;
        default:
            printf("------- unexpected here");
            exit(-1);
    }
    return src;
}

shared_ptr<Blob>
yuv_convert_from_YUV444(shared_ptr<Blob> src, int format, int n, int h, int w) {
    shared_ptr<Blob> dst;
    switch (format) {
        /////////////////////////
        case 18:
            // image_format = FORMAT_NV12;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_YUV444_to_NV12(src, n, h, w);
            return dst;
        case 19:
            // image_format = FORMAT_NV21;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_YUV444_to_NV12(src, n, h, w);
            dst = uv_exchange(dst, n, h, w, 1);
            return dst;
        case 20:
            // image_format = FORMAT_NV16;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_YUV444_to_NV16(src, n, h, w);
            return dst;
        case 21:
            // image_format = FORMAT_NV61;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_YUV444_to_NV16(src, n, h, w);
            dst = uv_exchange(dst, n, h, w, 2);
            return dst;
        case 22:
            // image_format = FORMAT_YUV420P;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_YUV444_to_NV12(src, n, h, w);
            dst = convert_NV12_to_YUV420P(dst, n, h, w);
            return dst;
        case 23:
            // image_format = FORMAT_YUV422P;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_YUV444_to_NV16(src, n, h, w);
            dst = convert_NV16_to_YUV422P(dst, n, h, w);
            return dst;
        case 24:
            // image_format = FORMAT_YUV444P;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = src;
            return dst;
        case 25:
            // image_format = FORMAT_NV24;
            // data_format = DATA_TYPE_EXT_1N_BYTE;
            dst = convert_YUV444_to_NV24(src, n, h, w);
            return dst;
        default:
            printf("------- unexpected here");
            exit(-1);
    }
    return src;
}

shared_ptr<Blob> gen_random_data(int format) {
    auto engine = pool->get_random_engine();
    IMAGE_N = 1 + (*engine)() % 4;
    IMAGE_H = ALIGN(1 + (*engine)() % 1024, 4);
    IMAGE_W = ALIGN(1 + (*engine)() % 1024, 4);
    while (IMAGE_W % 64 == 0) {
      IMAGE_W = ALIGN(1 + (*engine)() % 1024, 4);
  }

  shared_ptr<Blob> output_ =
      make_shared<Blob>(IMAGE_N * IMAGE_H * IMAGE_W * 3 * sizeof(u8));
  u8 *input_ptr = (u8 *)output_->data;
  for (int n = 0; n < IMAGE_N; n++) {
      for (int i = 0; i < IMAGE_H * IMAGE_W; i++) {
                input_ptr[n * 3 * IMAGE_H * IMAGE_W + i] = (*engine)() % 256;
                input_ptr[n * 3 * IMAGE_H * IMAGE_W + IMAGE_H * IMAGE_W + i] =
                    (*engine)() % 256;
                input_ptr[n * 3 * IMAGE_H * IMAGE_W + 2 * IMAGE_H * IMAGE_W + i] =
                    (*engine)() % 256;
        }
  }

  return convert_from_1N_unpack_BGR(
      output_, format, IMAGE_N, IMAGE_H, IMAGE_W);
}

void single_test(int src_format, int dst_format) {
    if (src_format == dst_format)
        return;
    bm_image_format_ext      src_image_format, dst_image_format;
    bm_image_data_format_ext src_data_format, dst_data_format;
    char                     src_format_name[30];
    char                     dst_format_name[30];
    get_format(
        src_format, &src_image_format, &src_data_format, src_format_name);
    get_format(
        dst_format, &dst_image_format, &dst_data_format, dst_format_name);

    shared_ptr<Blob> raw_input = gen_random_data(src_format);
    shared_ptr<Blob> reference = convert_to_1N_unpack_BGR(
        raw_input, src_format, IMAGE_N, IMAGE_H, IMAGE_W);
    shared_ptr<Blob> out_ref = convert_from_1N_unpack_BGR(
        reference, dst_format, IMAGE_N, IMAGE_H, IMAGE_W);

    shared_ptr<Blob> out_ref_ext;
    if (src_format >= 18 && dst_format >= 18) {
        // currently not test yuv2yuv
        return;
        out_ref_ext = yuv_convert_to_YUV444(
            raw_input, src_format, IMAGE_N, IMAGE_H, IMAGE_W);
        out_ref_ext = yuv_convert_from_YUV444(
            out_ref_ext, dst_format, IMAGE_N, IMAGE_H, IMAGE_W);
    }

    printf("start storage convert from %s %d to %s %d with N %d H %d W %d\n",
           src_format_name,
           src_format,
           dst_format_name,
           dst_format,
           IMAGE_N,
           IMAGE_H,
           IMAGE_W);

    int src_is_4N = (src_data_format == DATA_TYPE_EXT_4N_BYTE ||
                     src_data_format == DATA_TYPE_EXT_4N_BYTE_SIGNED);
    int dst_is_4N = (dst_data_format == DATA_TYPE_EXT_4N_BYTE ||
                     dst_data_format == DATA_TYPE_EXT_4N_BYTE_SIGNED);
    int src_num   = (src_is_4N ? 1 : IMAGE_N);
    int dst_num   = (dst_is_4N ? 1 : IMAGE_N);
    // int src_data_size = src_is_4N || (src_data_format ==
    // DATA_TYPE_EXT_FLOAT32); int dst_data_size = dst_is_4N || (dst_data_format
    // == DATA_TYPE_EXT_FLOAT32);
    bm_image input[4];
    bm_image output[4];

    u8 *raw_ptr = (u8 *)raw_input->data;
    for (int i = 0; i < src_num; i++) {
        bm_image_create(handle,
                        IMAGE_H,
                        IMAGE_W,
                        src_image_format,
                        src_data_format,
                        input + i);
    }

    int src_size[3] = {0};
    bm_image_get_byte_size(input[0], src_size);
    int total_size_src = src_size[0] + src_size[1] + src_size[2];
    for (int i = 0; i < src_num; i++) {
        u8 *host_ptr     = raw_ptr + i * total_size_src;
        u8 *image_ptr[4] = {host_ptr,
                            host_ptr + src_size[0],
                            host_ptr + src_size[0] + src_size[1],
                            host_ptr + src_size[0] + src_size[1] + src_size[2]};
        bm_image_copy_host_to_device(input[i], (void **)(image_ptr));
    }

    for (int i = 0; i < dst_num; i++) {
        bm_image_create(handle,
                        IMAGE_H,
                        IMAGE_W,
                        dst_image_format,
                        dst_data_format,
                        output + i);
    }

    bm_status_t ret =
        bmcv_image_storage_convert(handle, IMAGE_N, input, output);

    if (ret != BM_SUCCESS) {
        printf("run bm_storage_convert from %s to %s failed ret = %d\n",
               src_format_name,
               dst_format_name,
               ret);
        for (int i = 0; i < src_num; i++) {
            bm_image_destroy(input[i]);
        }
        for (int i = 0; i < dst_num; i++) {
            bm_image_destroy(output[i]);
        }
        exit(-1);
    }

    shared_ptr<Blob> output_blob =
        make_shared<Blob>(4 * IMAGE_H * IMAGE_W * 3 * sizeof(float));
    u8 *output_host_ptr = (u8 *)output_blob->data;
    int dst_size[3]     = {0};
    bm_image_get_byte_size(output[0], dst_size);
    int total_size_dst = dst_size[0] + dst_size[1] + dst_size[2];

    for (int i = 0; i < dst_num; i++) {
        u8 *host_ptr     = output_host_ptr + i * total_size_dst;
        u8 *image_ptr[4] = {host_ptr,
                            host_ptr + dst_size[0],
                            host_ptr + dst_size[0] + dst_size[1],
                            host_ptr + dst_size[0] + dst_size[1] + dst_size[2]};
        bm_image_copy_device_to_host(output[i], (void **)image_ptr);
    }

    shared_ptr<Blob> output_bgr_unpack_1n = convert_to_1N_unpack_BGR(
        output_blob, dst_format, IMAGE_N, IMAGE_H, IMAGE_W);

    int         compare_size = IMAGE_N * IMAGE_W * IMAGE_H * 3;
    int         cmp_res_x    = 0;
    const char *info_label   = "cv_storage_convert test ...\n";

    if (dst_format < 18) {
        // dst is RGB
        cmp_res_x = array_cmp_u8((u8 *)reference->data,
                                 (u8 *)output_bgr_unpack_1n->data,
                                 compare_size,
                                 info_label,
                                 1);
    } else if (dst_format >= 18 && src_format >= 18) {
        cmp_res_x = array_cmp_u8((u8 *)out_ref_ext->data,
                                 (u8 *)output_blob->data,
                                 total_size_dst * IMAGE_N,
                                 info_label,
                                 1);
    } else {
        // dst_format >= 18 && src_format < 18
        cmp_res_x = array_cmp_u8((u8 *)out_ref->data,
                                 (u8 *)output_blob->data,
                                 total_size_dst * IMAGE_N,
                                 info_label,
                                 1);
    }

    if (cmp_res_x != 0) {
        printf("cv_storage_convert from %s to %s comparing failed with N %d H "
               "%d W %d failed\n",
               src_format_name,
               dst_format_name,
               IMAGE_N,
               IMAGE_H,
               IMAGE_W);
        exit(-1);
    }

    for (int i = 0; i < src_num; i++) {
        bm_image_destroy(input[i]);
    }
    for (int i = 0; i < dst_num; i++) {
        bm_image_destroy(output[i]);
    }
    printf("convert from %s to %s succeed\n", src_format_name, dst_format_name);
}

static void test_cv_storage_convert_all() {
    for (int src_token = 0; src_token < 25; src_token++) {
        for (int dst_token = 0; dst_token < 25; dst_token++) {
            if(src_token == 23 || dst_token == 23) continue; // YUV422 or NV24 Not Supported Yet
                  pool->enqueue(single_test, src_token, dst_token);
             }
      }
}

int main(int argc, char *argv[]) {
  int test_loop_times = 1;
    switch(argc)
    {
        case 3:
        {
            int thread_num = atoi(argv[2]);
            if(thread_num < 1 || thread_num > 4)
            {
                std::cout << "[TEST WARP] thread_num should be 1~4" << std::endl;
                exit(-1);
            }
            if(thread_num != 1)
                pool = std::make_shared<ThreadPool>(thread_num);
        }
        break;
        case 2:
            test_loop_times = atoi(argv[1]);
        break;
    }

    if (test_loop_times > 1500 || test_loop_times < 1)
    {
        std::cout << "[TEST STORAGE CONVERT] loop times should be 1~1500" << std::endl;
    exit(-1);
  }
  std::cout << "[TEST STORAGE CONVERT] test starts... LOOP times will be "
            << test_loop_times << std::endl;

    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++)
    {
        std::cout << "------[TEST STORAGE CONVERT] LOOP " << loop_idx <<
                 "------" << std::endl;

    int             dev_id = 0;
    bm_status_t     ret    = bm_dev_request(&handle, dev_id);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        exit(-1);
    }

        struct timespec tp;
        clock_gettime_(0, &tp);

        tp.tv_nsec = 100;
        printf("random seed %lu\n", tp.tv_nsec);
        pool->srand(tp.tv_nsec);
        for (int i = 0; i < 5; i++)
        {
            test_cv_storage_convert_all();
        }
        pool->wait_all_done();
        bm_dev_free(handle);
    }
  std::cout << "------[TEST STORAGE CONVERT] ALL TEST PASSED!" << std::endl;

  return 0;
}
