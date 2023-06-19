#ifndef BMCV_UTIL_H
#define BMCV_UTIL_H

#define ALIGN(a, b) (((a) + (b) - 1) / (b) * (b))
#define UINT64_PTR(p) (reinterpret_cast<unsigned long long *>(p))
#define INT16_PTR(p) (reinterpret_cast<short *>(p))
#define UINT8_PTR(p) (reinterpret_cast<unsigned char *>(p))
#define VOID_PTR(p) (reinterpret_cast<void *>(p))

typedef long long int64;
typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned char u8;

typedef enum {
    FORMAT_YUV420P,
    FORMAT_YUV422P,
    FORMAT_YUV444P,
    FORMAT_NV12,
    FORMAT_NV21,
    FORMAT_NV16,
    FORMAT_NV61,
    FORMAT_NV24,
    FORMAT_RGB_PLANAR,
    FORMAT_BGR_PLANAR,
    FORMAT_RGB_PACKED,
    FORMAT_BGR_PACKED,
    FORMAT_RGBP_SEPARATE,
    FORMAT_BGRP_SEPARATE,
    FORMAT_GRAY,
    FORMAT_COMPRESSED,
    FORMAT_HSV_PLANAR,
} bm_image_format_ext;

struct bmMat {
    int height;
    int width;
    bm_image_format_ext format;
    int chan;
    void* data[3];
    int step[3];
    int size[3];
    bmMat(int h, int w, bm_image_format_ext fmt, void* d):
        height(h),
        width(w),
        format(fmt) {
        memcpy(data, d, 3 * sizeof(void*));
        switch(fmt) {
            case FORMAT_GRAY:
                chan = 1;
                step[0] = w;
                size[0] = h * w;
                break;
            case FORMAT_YUV420P:
                chan = 3;
                step[0] = w;
                step[1] = ALIGN(w, 2) >> 1;
                step[2] = ALIGN(w, 2) >> 1;
                size[0] = h * w;
                size[1] = ALIGN(w, 2) * ALIGN(h, 2) >> 2;
                size[2] = ALIGN(w, 2) * ALIGN(h, 2) >> 2;
                break;
            case FORMAT_YUV422P:
                chan = 3;
                step[0] = w;
                step[1] = ALIGN(w, 2) >> 1;
                step[2] = ALIGN(w, 2) >> 1;
                size[0] = h * w;
                size[1] = ALIGN(w, 2) * h >> 1;
                size[2] = ALIGN(w, 2) * h >> 1;
                break;
            case FORMAT_YUV444P:
                chan = 3;
                step[0] = w;
                step[1] = w;
                step[2] = w;
                size[0] = h * w;
                size[1] = h * w;
                size[2] = h * w;
                break;
            case FORMAT_NV12:
            case FORMAT_NV21:
                chan = 2;
                step[0] = w;
                step[1] = ALIGN(w, 2);
                size[0] = h * w;
                size[1] = ALIGN(h, 2) * ALIGN(w, 2) >> 1;
                break;
            case FORMAT_NV16:
            case FORMAT_NV61:
                chan = 2;
                step[0] = w;
                step[1] = ALIGN(w, 2);
                size[0] = h * w;
                size[1] = h * ALIGN(w, 2);
                break;
            default:
                break;
        }
    };
};

struct bmPoint {
    int x;
    int y;
};

typedef struct {
    int x;
    int y;
} bmcv_point_t;

typedef struct {
    float x;
    float y;
} bmcv_point2f_t;

#endif
