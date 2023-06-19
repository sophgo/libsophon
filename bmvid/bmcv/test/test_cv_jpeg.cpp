// Created by nan.wu on 2019-08-22.
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include <assert.h>
#include <time.h>
#include <cmath>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

#ifndef USING_CMODEL
static int IMAGE_H = 768;
static int IMAGE_W = 1024;

// #define EXAMPLE_JPEG
// #define DEBUG_JPEG

typedef struct {
    int loop_num;
    bm_handle_t handle;
} jpeg_thread_arg_t;

static void write_file(char *filename, void* jpeg_data, size_t size)
{
    FILE *fp = fopen(filename, "wb+");
    assert(fp != NULL);
    fwrite(jpeg_data, size, 1, fp);
    printf("save to %s %ld bytes\n", filename, size);
    fclose(fp);
}

static void read_file(char *filename, void* jpeg_data, size_t* size)
{
    FILE *fp = fopen(filename, "rb+");
    assert(fp != NULL);
    fseek(fp, 0, SEEK_END);
    *size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    size_t cnt = fread(jpeg_data, *size, 1, fp);
    printf("read from %s %ld bytes\n", filename, cnt);
    fclose(fp);
}

static int image_cmp_u8_with_stride(
    u8 *p_exp,
    u8 *p_got,
    int src_format,
    int dst_format,
    int image_n,
    int image_h,
    int image_w,
    int* stride,
    const char *info_label,
    u8 delta)
{
    float uv_format_factor = 0.5;
    if (dst_format == FORMAT_NV16 || dst_format ==  FORMAT_NV61 || dst_format == FORMAT_YUV422P) {
        uv_format_factor = 1;
    } else if (dst_format == FORMAT_YUV444P) {
        uv_format_factor = 2;
    } else if (dst_format == FORMAT_GRAY) {
        uv_format_factor = 0;
    }
    int n_size = image_h * image_w + image_h * image_w * uv_format_factor;
    int n_stride = image_h * stride[0] + ((dst_format == FORMAT_GRAY) ? 0 :
                                          ((dst_format == FORMAT_NV12) ? image_h * stride[1] / 2 :
                                           ((dst_format == FORMAT_YUV444P || dst_format == FORMAT_YUV422P) ?
                                            image_h * stride[1] * 2 : image_h * stride[1])));
    //printf("n_stride %d  n_size %d  stride0 %d  stride1 %d\n", n_stride, n_size, stride[0], stride[1]);
    for(int n = 0; n < image_n; n++) {
        // compare Y
        for (int i = 0; i < image_h; i++) {
            for (int j = 0; j < image_w; j++)
                if ((int)fabs(p_got[j + i * stride[0] + n * n_stride] - (int)p_exp[j + i * image_w + n * n_size]) > delta) {
                    printf("%s abs error at Y: n=%d h=%d w=%d, exp %d got %d\n", info_label, n, i, j,
                            p_exp[j + i * image_w + n * n_size], p_got[j + i * stride[0] + n * n_stride]);
                    return -1;
                }
        }
        // compare UV
        if (dst_format == FORMAT_GRAY) {
            return 0;
        } else if ((src_format == FORMAT_YUV420P) && (dst_format == FORMAT_YUV420P)) {
            for (int i = 0; i < image_h; i++) {
                for (int j = 0; j < image_w / 2; j++) {
                    if ((int)fabs(p_got[j + i * stride[1] + image_h * stride[0] + n * n_stride] -
                        (int)p_exp[j + i * image_w / 2 + image_w * image_h + n * n_size]) > delta) {
                        printf("%s abs error at src=yuv420p dst=yuv420p UV: n=%d h=%d w=%d, exp %d got %d\n", info_label, n, i, j,
                                p_exp[j + i * image_w / 2 + image_w * image_h + n * n_size],
                                p_got[j + i * stride[1] + image_h * stride[0] + n * n_stride]);
                        return -1;
                    }
                }
            }
        } else if ((src_format == FORMAT_YUV420P) && (dst_format == FORMAT_NV12)) {
            // need remove got_image stride, then compare nv12(got) with yuv420p(exp)
            for (int i = 0; i < image_h / 2; i++) {
                for (int j = 0; j < image_w; j++) {
                    if ((int)fabs(p_exp[j / 2 + image_h / 2 * image_w / 2 * (j % 2) + i * image_w / 2 + image_h * image_w + n * n_size]
                        - (int)p_got[j + i * stride[1] + image_h * stride[0] + n * n_stride]) > delta) {
                        printf("%s abs error at src=yuv420p dst=nv12 UV: n=%d h=%d w=%d, exp %d got %d\n", info_label, n, i, j,
                                p_exp[j / 2 + image_h / 2 * image_w / 2 * (j % 2) + i * image_w / 2 + image_h * image_w + n * n_size],
                                p_got[j + i * stride[1] + image_h * stride[0] + n * n_stride]);
                        return -1;
                    }
                }
            }
        } else if ((src_format == FORMAT_NV12) && (dst_format == FORMAT_YUV420P)) {
            // need remove got_image stride, then compare yuv420p(got) with nv12(exp)
            for (int i = 0; i < image_h / 2; i++) {
                for (int j = 0; j < image_w; j++) {
                    if ((int)fabs(p_got[j / 2 + image_h / 2 * stride[1] * (j % 2) + i * stride[1] + image_h * stride[0] + n * n_stride]
                        - (int)p_exp[j + i * image_w + image_h * image_w + n * n_size]) > delta) {
                        printf("%s abs error at src=nv12 dst=yuv420p UV: n=%d h=%d w=%d, exp %d got %d\n", info_label, n, i, j,
                                p_exp[j + i * image_w + image_h * image_w + n * n_size],
                                p_got[j / 2 + image_h / 2 * stride[1] * (j % 2) + i * stride[1] + image_h * stride[0] + n * n_stride]);
                        return -1;
                    }
                }
            }
        } else if ((src_format == FORMAT_NV12) && (dst_format == FORMAT_NV12)) {
            // need remove got_image stride, then compare nv12(got) with nv12(exp)
            for (int i = 0; i < image_h / 2; i++) {
                for (int j = 0; j < image_w; j++) {
                    if ((int)fabs(p_got[j + i * stride[0] + image_h * stride[0] + n * n_stride]
                        - (int)p_exp[j + i * image_w + image_h * image_w + n * n_size]) > delta) {
                        printf("%s abs error at src=nv12 dst=nv12 UV: n=%d h=%d w=%d, exp %d got %d\n", info_label, n, i, j,
                                p_exp[j + i * image_w + image_h * image_w + n * n_size],
                                p_got[j + i * stride[0] + image_h * stride[0] + n * n_stride]);
                        return -1;
                    }
                }
            }
        } else if ((src_format == FORMAT_NV21) && (dst_format == FORMAT_NV12)) {
            // need remove got_image stride, then compare nv12(got) with nv21(exp)
            for (int i = 0; i < image_h / 2; i++) {
                for (int j = 0; j < image_w; j++) {
                    if ((int)fabs(p_got[((j % 2) ? -1 : 1) + j + i * stride[1] + image_h * stride[0] + n * n_stride]
                        - (int)p_exp[j + i * image_w + image_h * image_w + n * n_size]) > delta) {
                        printf("%s abs error at src=nv21 dst=nv12 UV: n=%d h=%d w=%d, exp %d got %d\n", info_label, n, i, j,
                                p_exp[j + i * image_w + image_h * image_w + n * n_size],
                                p_got[j + i * stride[0] + image_h * stride[1] + n * n_stride]);
                        return -1;
                    }
                }
            }
        } else if ((src_format == FORMAT_NV21) && (dst_format == FORMAT_YUV420P)) {
            // need remove got_image stride, then compare yuv420p(got) with nv21(exp)
            for (int i = 0; i < image_h / 2; i++) {
                for (int j = 0; j < image_w; j++) {
                    if ((int)fabs(p_got[j / 2 + image_h / 2 * stride[1] * (j % 2 ? 0 : 1) + i * stride[1] + image_h * stride[0] + n * n_stride]
                        - (int)p_exp[j + i * image_w + image_h * image_w + n * n_size]) > delta) {
                        printf("%s abs error at src=nv21 dst=yuv420p UV: n=%d h=%d w=%d, exp %d got %d\n", info_label, n, i, j,
                                p_exp[j + i * image_w + image_h * image_w + n * n_size],
                                p_got[j / 2 + image_h / 2 * stride[1] * (j % 2 ? 0 : 1) + i * stride[1] + image_h * stride[0] + n * n_stride]);
                        return -1;
                    }
                }
            }
        } else if ((src_format == FORMAT_YUV422P) && (dst_format == FORMAT_YUV422P)) {
            // need remove got_image stride, then compare yuv422p(got) with yuv422p(exp)
            for (int i = 0; i < image_h * 2; i++) {
                for (int j = 0; j < image_w / 2; j++) {
                    if ((int)fabs(p_got[j + i * stride[1] + image_h * stride[0] + n * n_stride]
                        - (int)p_exp[j + i * image_w / 2 + image_h * image_w + n * n_size]) > delta) {
                        printf("%s abs error at src=yuv422p dst=yuv422p  UV: n=%d h=%d w=%d, exp %d got %d\n", info_label, n, i, j,
                                p_exp[j + i * image_w / 2 + image_h * image_w + n * n_size],
                                p_got[j + i * stride[1] + image_h * stride[0] + n * n_stride]);
                        return -1;
                    }
                }
            }
        } else if ((src_format == FORMAT_NV16) && (dst_format == FORMAT_YUV422P)) {
            // need remove got_image stride, then compare yuv422p(got) with nv16(exp)
            for (int i = 0; i < image_h; i++) {
                for (int j = 0; j < image_w; j++) {
                    if ((int)fabs(p_got[j / 2 + image_h * stride[1] * (j % 2) + i * stride[1] + image_h * stride[0] + n * n_stride]
                        - (int)p_exp[j + i * image_w + image_h * image_w + n * n_size]) > delta) {
                        printf("%s abs error at src=nv16 dst=yuv422p UV: n=%d h=%d w=%d, exp %d got %d\n", info_label, n, i, j,
                                p_exp[j + i * image_w + image_h * image_w + n * n_size],
                                p_got[j / 2 + image_h * stride[1] * (j % 2) + i * stride[1] + image_h * stride[0] + n * n_stride]);
                        return -1;
                    }
                }
            }
        } else if ((src_format == FORMAT_NV61) && (dst_format == FORMAT_YUV422P)) {
            // need remove got_image stride, then compare yuv422p(got) with nv61(exp)
            for (int i = 0; i < image_h; i++) {
                for (int j = 0; j < image_w; j++) {
                    if ((int)fabs(p_got[j / 2 + image_h * stride[1] * (j % 2 ? 0 : 1) + i * stride[1] + image_h * stride[0] + n * n_stride]
                        - (int)p_exp[j + i * image_w + image_h * image_w + n * n_size]) > delta) {
                        printf("%s abs error at src=nv61 dst=yuv422p UV: n=%d h=%d w=%d, exp %d got %d\n", info_label, n, i, j,
                                p_exp[j + i * image_w + image_h * image_w + n * n_size],
                                p_got[j / 2 + image_h * stride[1] * (j % 2 ? 0 : 1) + i * stride[1] + image_h * stride[0] + n * n_stride]);
                        return -1;
                    }
                }
            }
        } else if ((src_format == FORMAT_YUV422P) && (dst_format == FORMAT_NV16)) {
            // need remove got_image stride, then compare nv16(got) with yuv422p(exp)
            for (int i = 0; i < image_h; i++) {
                for (int j = 0; j < image_w; j++) {
                    if ((int)fabs(p_exp[j / 2 + image_h * image_w / 2 * (j % 2) + i * image_w / 2 + image_h * image_w + n * n_size]
                        - (int)p_got[j + i * stride[1] + image_h * stride[0] + n * n_stride]) > delta) {
                        printf("%s abs error at src=yuv422p dst=nv16 UV: n=%d h=%d w=%d, exp %d got %d\n", info_label, n, i, j,
                                p_exp[j / 2 + image_h * image_w / 2 * (j % 2) + i * image_w / 2 + image_h * image_w + n * n_size],
                                p_got[j + i * stride[1] + image_h * stride[0] + n * n_stride]);
                        return -1;
                    }
                }
            }
        } else if ((src_format == FORMAT_NV16) && (dst_format == FORMAT_NV16)) {
            // need remove got_image stride, then compare nv16(got) with nv16(exp)
            for (int i = 0; i < image_h; i++) {
                for (int j = 0; j < image_w; j++) {
                    if ((int)fabs(p_got[j + i * stride[1] + image_h * stride[0] + n * n_stride]
                        - (int)p_exp[j + i * image_w + image_h * image_w + n * n_size]) > delta) {
                        printf("%s abs error at src=nv16 dst=nv16  UV: n=%d h=%d w=%d, exp %d got %d\n", info_label, n, i, j,
                                p_exp[j + i * image_w + image_h * image_w + n * n_size],
                                p_got[j + i * stride[1] + image_h * stride[0] + n * n_stride]);
                        return -1;
                    }
                }
            }
        } else if ((src_format == FORMAT_NV61) && (dst_format == FORMAT_NV16)) {
            // need remove got_image stride, then compare nv16(got) with nv61(exp)
            for (int i = 0; i < image_h; i++) {
                for (int j = 0; j < image_w; j++) {
                    if ((int)fabs(p_got[((j % 2) ? -1 : 1) + j + i * stride[0] + image_h * stride[0] + n * n_stride]
                        - (int)p_exp[j + i * image_w + image_h * image_w + n * n_size]) > delta) {
                        printf("%s abs error at src=nv61 dst=nv16 UV: n=%d h=%d w=%d, exp %d got %d\n", info_label, n, i, j,
                                p_exp[j + i * image_w + image_h * image_w + n * n_size],
                                p_got[((j % 2) ? -1 : 1) + j + i * stride[0] + image_h * stride[0] + n * n_stride]);
                        return -1;
                    }
                }
            }
        } else if ((src_format == FORMAT_YUV444P) && (dst_format == FORMAT_YUV444P)) {
            // need remove got_image stride, then compare yuv444p(got) with yuv444p(exp)
            for (int i = 0; i < image_h * 2; i++) {
                for (int j = 0; j < image_w; j++) {
                    if ((int)fabs(p_got[j + i * stride[1] + image_h * stride[0] + n * n_stride]
                        - (int)p_exp[j + i * image_w + image_h * image_w + n * n_size]) > delta) {
                        printf("%s abs error at src=yuv444p dst=yuv444p  UV: n=%d h=%d w=%d, exp %d got %d\n", info_label, n, i, j,
                                p_exp[j + i * image_w + image_h * image_w + n * n_size],
                                p_got[j + i * stride[1] + image_h * stride[0] + n * n_stride]);
                        return -1;
                    }
                }
            }
        } else {
            printf("NOT support this format!");
            return -1;
        }
    }
    return 0;
}

static void fill_image(bm_image* src, u8** raw_image, int n, int h, int w, int format) {
    float uv_format_factor = 0.5;
    if (format == FORMAT_NV16 || format == FORMAT_NV61 || format == FORMAT_YUV422P) {
        uv_format_factor = 1;
    } else if (format == FORMAT_YUV444P) {
        uv_format_factor = 2;
    } else if (format == FORMAT_GRAY) {
        uv_format_factor = 0;
    }
    int n_size = h * w + h * w * uv_format_factor;
    if (*raw_image == NULL) {
        *raw_image = (u8*)malloc(n * h * w * 3);
        for(int j = 0; j < n; j++){
            // Init Y
            for(int i = 0; i < h; i++) {
                unsigned char val = rand() % 256;
                for(int k = 0; k < w; k++) {
                    raw_image[0][k + i * w + j * n_size] = val;
                }
            }
            // Init UV
            for(int i = 0; i < h * uv_format_factor; i++) {
                unsigned char val = rand() % 256;
                for(int k = 0; k < w; k++) {
                    raw_image[0][k + i * w + h * w + j * n_size] = val;
                }
            }
        }
    }
    int stride[4] = {0, 0, 0, 0};
    bm_image_get_stride(*src, stride);
    int n_stride = h * stride[0] + h * stride[1] + h * stride[2];
    // convert orig_input to input with stride_h
    u8* input_ptr = new u8[n * n_stride];
    for(int nn = 0; nn < n; nn++) {
        // for Y
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++)
                input_ptr[j + i * stride[0] + nn * n_stride] = raw_image[0][j + i * w + nn * n_size];
        }
        // for UV
        if (format == FORMAT_YUV420P) {
            for (int i = 0; i < h; i++) {
                for (int j = 0; j < w / 2; j++) {
                    input_ptr[j + i * stride[1] + h * stride[0] + nn * n_stride] = raw_image[0][j + i * w / 2 + h * w + nn * n_size];
                }
            }
        } else if (format == FORMAT_YUV422P) {
            for (int i = 0; i < h * 2; i++) {
                for (int j = 0; j < w / 2; j++) {
                    input_ptr[j + i * stride[1] + h * stride[0] + nn * n_stride] = raw_image[0][j + i * w / 2 + h * w + nn * n_size];
                }
            }
        } else {
            for (int i = 0; i < h * uv_format_factor; i++) {
                for (int j = 0; j < w; j++)
                    input_ptr[j + i * stride[0] + h * stride[0] + nn * n_stride] = raw_image[0][j + i * w + h * w + nn * n_size];
            }
        }
    }
    for (int i = 0; i < n; i++) {
        u8* image_buf[3] = {(u8*)malloc(h * stride[0]),
                            (u8*)malloc(h * stride[1]),
                            (u8*)malloc(h * stride[2])};
        switch(format)
        {
            case FORMAT_YUV420P:
                memcpy(image_buf[0], input_ptr + i * n_stride, h * stride[0]);
                memcpy(image_buf[1], input_ptr + i * n_stride + h * stride[0], h * stride[0] / 4);
                memcpy(image_buf[2], input_ptr + i * n_stride + h * stride[0] + h * stride[0] / 4, h * stride[0] / 4);
                break;
            case FORMAT_NV12:
            case FORMAT_NV21:
                memcpy(image_buf[0], input_ptr + i * n_stride, h * stride[0]);
                memcpy(image_buf[1], input_ptr + i * n_stride + h * stride[0], h * stride[1] / 2);
                break;
            case FORMAT_YUV422P:
                memcpy(image_buf[0], input_ptr + i * n_stride, h * stride[0]);
                memcpy(image_buf[1], input_ptr + i * n_stride + h * stride[0], h * stride[1]);
                memcpy(image_buf[2], input_ptr + i * n_stride + h * stride[0] + h * stride[1], h * stride[2]);
                break;
            case FORMAT_NV16:
            case FORMAT_NV61:
                memcpy(image_buf[0], input_ptr + i * n_stride, h * stride[0]);
                memcpy(image_buf[1], input_ptr + i * n_stride + h * stride[0], h * stride[1]);
                break;
            case FORMAT_YUV444P:
                memcpy(image_buf[0], input_ptr + i * n_stride, h * stride[0]);
                memcpy(image_buf[1], input_ptr + i * n_stride + h * stride[0], h * stride[1]);
                memcpy(image_buf[2], input_ptr + i * n_stride + h * stride[0] + h * stride[1], h * stride[2]);
                break;
            case FORMAT_GRAY:
                memcpy(image_buf[0], input_ptr + i * n_stride, h * stride[0]);
                break;
            default:
                assert(0);
                break;
        }
        bm_image_copy_host_to_device(src[i], (void**)image_buf);
        free(image_buf[0]);
        free(image_buf[1]);
        free(image_buf[2]);
    }
    delete [] input_ptr;
}

// yuv -> Jpeg -> yuv
#ifdef __linux__
void* test_jpeg_enc_and_dec(void* argv)
#else
DWORD WINAPI test_jpeg_enc_and_dec(LPVOID argv)
#endif
{
    jpeg_thread_arg_t* jpeg_thread_arg = (jpeg_thread_arg_t *)argv;
    bm_handle_t handle = jpeg_thread_arg->handle;
    int loop_num = jpeg_thread_arg->loop_num;
    int dev_id = bm_get_devid(handle);
    for (int loop = 0; loop < loop_num; loop++) {
        int ret = 0;
        u8* raw_image = NULL;
        const char *img_format[16] = {"FORMAT_YUV420P",
                                      "FORMAT_YUV422P",
                                      "FORMAT_YUV444P",
                                      "FORMAT_NV12",
                                      "FORMAT_NV21",
                                      "FORMAT_NV16",
                                      "FORMAT_NV61",
                                      "FORMAT_NV24",
                                      "FORMAT_RGB_PLANAR",
                                      "FORMAT_BGR_PLANAR",
                                      "FORMAT_RGB_PACKED",
                                      "FORMAT_BGR_PACKED",
                                      "FORMAT_RGBP_SEPARATE",
                                      "FORMAT_BGRP_SEPARATE",
                                      "FORMAT_GRAY",
                                      "FORMAT_COMPRESSED"};
        int input_fmt[10] = {FORMAT_YUV420P, FORMAT_YUV422P, FORMAT_YUV444P, FORMAT_NV12,
                             FORMAT_NV21, FORMAT_NV16, FORMAT_NV61, FORMAT_GRAY};
        int format = input_fmt[rand() % 8];
        bool create_dst_image = rand() % 2 ? true : false;
        printf("image src format = %s\n", img_format[format]);
        int image_n = rand() % 4 + 1;
        int image_h = rand() % 1024 * 2 + 16;
        int image_w = rand() % 1024 * 2 + 16;
        int* stride = (int*)malloc(3 * sizeof(int));
        stride[0] = image_w + rand() % 10 * 4;
        printf("input image w stride is: %d\n", stride[0]);
        stride[1] = (format == FORMAT_YUV420P || format == FORMAT_YUV422P) ? stride[0] / 2 : stride[0];
        stride[2] = stride[1];
        // int* stride = NULL;

        bm_image src[4];
        for (int i = 0; i < image_n; i++) {
            bm_image_create(handle, image_h, image_w, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, src + i, stride);
        }
        fill_image(src, &raw_image, image_n, image_h, image_w, format);
        void* jpeg_data[4] = {NULL, NULL, NULL, NULL};
        size_t* size = (size_t*)malloc(image_n * sizeof(size_t));
        ret = bmcv_image_jpeg_enc(handle, image_n, src, jpeg_data, size, 95);
        assert(ret == BM_SUCCESS);
#ifdef DEBUG_JPEG
        char* file = (char*)"test1.jpg";
        write_file(file, jpeg_data[0], size[0]);
        file = (char*)"test2.jpg";
        write_file(file, jpeg_data[1], size[1]);

        // cv::Mat jpeg = cv::imread("test.jpg");
        // cv::cvtColor(jpeg, jpeg, cv::COLOR_BGR2RGB);
        // //uint8_t  *prgb = jpeg.data;
        // printf("jpeg col  = %d, raw image width  = %d\n", jpeg.cols, image_w);
        // printf("jpeg rows = %d, raw image height =  %d\n", jpeg.rows, image_h);
        // assert(jpeg.cols == image_w);
        // assert(jpeg.rows == image_h);
#endif
        // test decode
        bm_image dst[4];
        // get dst format
        if (create_dst_image) {
            #ifdef __linux__
            int dst_format = (format == FORMAT_NV12 || format == FORMAT_NV21 || format == FORMAT_YUV420P) ?
                              (random() % 2 ? FORMAT_YUV420P : FORMAT_NV12) :
                              ((format == FORMAT_NV61 || format == FORMAT_NV61 || format == FORMAT_YUV422P) ?
                               (random() % 2 ? FORMAT_YUV422P : FORMAT_NV16) : format);
            #else
            int dst_format = (format == FORMAT_NV12 || format == FORMAT_NV21 || format == FORMAT_YUV420P) ?
                              (rand() % 2 ? FORMAT_YUV420P : FORMAT_NV12) :
                              ((format == FORMAT_NV61 || format == FORMAT_NV61 || format == FORMAT_YUV422P) ?
                               (rand() % 2 ? FORMAT_YUV422P : FORMAT_NV16) : format);
            #endif
            printf("image dst format = %s\n", img_format[dst_format]);
            stride[0] = image_w + rand() % 10 * 4;
            printf("output image w stride is: %d\n", stride[0]);
            stride[1] = (dst_format == FORMAT_YUV420P || dst_format == FORMAT_YUV422P) ? stride[0] / 2 : stride[0];
            stride[2] = stride[1];
            for (int i = 0; i < image_n; i++) {
                bm_image_create(handle, image_h, image_w, (bm_image_format_ext)dst_format, DATA_TYPE_EXT_1N_BYTE, dst + i, stride);
            }
        }
        ret = bmcv_image_jpeg_dec(handle, (void**)jpeg_data, size, image_n, dst);
        assert(ret == BM_SUCCESS);

        int image_byte_size[3] = {0};
        bm_image_get_byte_size(dst[0], image_byte_size);
#ifdef DEBUG_JPEG
        printf("plane0 size: %d\n", image_byte_size[0]);
        printf("plane1 size: %d\n", image_byte_size[1]);
        printf("plane2 size: %d\n", image_byte_size[2]);
#endif
        int byte_size = image_byte_size[0] + image_byte_size[1] + image_byte_size[2];
        u8* output_ptr = (u8*)malloc(byte_size * image_n);
        for (int i = 0;i < image_n; i++) {
            void* out_ptr[3] = {(void*)((u8*)output_ptr + i * byte_size),
                                (void*)((u8*)output_ptr + i * byte_size + image_byte_size[0]),
                                (void*)((u8*)output_ptr + i * byte_size + image_byte_size[0] + image_byte_size[1])};
            bm_image_copy_device_to_host(dst[i], out_ptr);
        }

#ifdef DEBUG_JPEG
        printf("\ngot output:\n");
        for(int i=0; i<50; i++) {
            printf("%d   ", output_ptr[i]);
        }
        printf("\nexp output:\n");
        for(int i=0; i<50; i++) {
            printf("%d   ", raw_image[i]);
        }
        printf("\n");
#endif
        stride[0] = 0;
        stride[1] = 0;
        stride[2] = 0;
        bm_image_get_stride(dst[0], stride);
        int cmp_res = image_cmp_u8_with_stride((u8*)raw_image,
                                               (u8*)output_ptr,
                                               format,
                                               dst->image_format,
                                               image_n, image_h, image_w,
                                               stride,
                                               "jpeg enc and dec test ...\n",
                                               20);
        if(cmp_res != 0) {
            printf("test failed on dev%d!\n", dev_id);
            exit(-1);
        } else {
            printf("test encode and decode successful on dev%d!\n", dev_id);
        }
        for (int i = 0; i < image_n; i++) {
            bm_image_destroy(dst[i]);
        }
        free(output_ptr);

        for (int i = 0; i < image_n; i++) {
            free(jpeg_data[i]);
            bm_image_destroy(src[i]);
        }
        free(raw_image);
        free(size);
        free(stride);
    }
    return NULL;
}

void test_jpeg_dec(char* file) {
    int dev_id = 0;
    bm_handle_t handle;
    bm_status_t req = bm_dev_request(&handle, dev_id);
    if (req != BM_SUCCESS) {
        printf("create bm handle failed!");
        exit(-1);
    }
    // read input from picture
    size_t size = 0;
    u8* jpeg_data = (u8*)malloc(IMAGE_H * IMAGE_W * 3);
    u8* output_ptr = (u8*)malloc(IMAGE_H * IMAGE_W * 3);
    read_file(file, jpeg_data, &size);

    // create bm_image used to save output
    bm_image dst[1];
    // you can also create image, if not it will create in inner
#if 0
    int format = 0;  // picture format is yuv420p
    bm_image_create(handle, IMAGE_H, IMAGE_W, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, dst);
#endif
    // decode input
    int ret = bmcv_image_jpeg_dec(handle, (void**)&jpeg_data, &size, 1, dst);
    assert(ret == BM_SUCCESS);

    // get out from bm_image
    int image_byte_size[3] = {0};
    bm_image_get_byte_size(*dst, image_byte_size);
#ifdef DEBUG_JPEG
    printf("plane0 size: %d\n", image_byte_size[0]);
    printf("plane1 size: %d\n", image_byte_size[1]);
    printf("plane2 size: %d\n", image_byte_size[2]);
#endif
    int byte_size = image_byte_size[0] + image_byte_size[1] + image_byte_size[2];
    void* out_ptr[3] = {(void*)((u8*)output_ptr),
                        (void*)((u8*)output_ptr + image_byte_size[0]),
                        (void*)((u8*)output_ptr + image_byte_size[0] + image_byte_size[1])};
    bm_image_copy_device_to_host(*dst, out_ptr);

    // save output to file
    char* out_file = (char*)"test.dec";
    write_file(out_file, output_ptr, byte_size);

    bm_image_destroy(*dst);
    free(jpeg_data);
    free(output_ptr);
    bm_dev_free(handle);
}

void test_jpeg_enc(char* in_file)
{
    int dev_id = 0;
    bm_handle_t handle;
    bm_status_t req = bm_dev_request(&handle, dev_id);
    if (req != BM_SUCCESS) {
        printf("create bm handle failed!");
        exit(-1);
    }
    int ret = 0;
    u8* raw_image = (u8*)malloc(IMAGE_H * IMAGE_W * 3);
    size_t in_size = 0;
    read_file(in_file, (void*)raw_image, (size_t*)&in_size);

    int format = 0;
    bm_image src;
    bm_image_create(handle, IMAGE_H, IMAGE_W, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &src);
    fill_image(&src, &raw_image, 1, IMAGE_H, IMAGE_W, format);

    void* jpeg_data = NULL;
    size_t out_size = 0;
    ret = bmcv_image_jpeg_enc(handle, 1, &src, &jpeg_data, &out_size);
    assert(ret == BM_SUCCESS);

    // char* out_file = (char*)"test.jpg";
    // write_file(out_file, jpeg_data, out_size);

    // cv::Mat jpeg = cv::imread("test.jpg");
    // cv::cvtColor(jpeg, jpeg, cv::COLOR_BGR2RGB);
    // //uint8_t  *prgb = jpeg.data;
    // assert(jpeg.cols == IMAGE_W);
    // assert(jpeg.rows == IMAGE_H);

    free(jpeg_data);
    bm_image_destroy(src);
    free(raw_image);
    bm_dev_free(handle);
}

int main(int argc, char *argv[]) {
    int test_loop_times  = 1;
    int test_threads_num = 1;
    int dev_id = -1;
    struct timespec tp;
    clock_gettime_(0, &tp);

    unsigned int seed = tp.tv_nsec;
    if (argc > 1) {
        test_loop_times  = atoi(argv[1]);
    }
    if (argc > 2) {
        test_threads_num = atoi(argv[2]);
    }
    if (argc > 3) {
        dev_id = atoi(argv[3]);
    }
    if (argc > 4) {
        seed = atoi(argv[4]);
    }
    if (argc > 5) {
        std::cout << "command input error, please follow this "
                     "order:test_cv_jpeg loop_num multi_thread_num dev_id random_seed"
                  << std::endl;
        exit(-1);
    }
    if (test_loop_times < 1) {
        std::cout << "[TEST JPEG] loop times should be greater than 0"
                  << std::endl;
        exit(-1);
    }
    if (test_threads_num > 4 || test_threads_num < 1) {
        std::cout << "[TEST JPEG] thread nums should be 1~4 "
                  << std::endl;
        exit(-1);
    }
    printf("random seed %d\n", seed);
    srand(seed);
    bmlib_log_set_level(BMLIB_LOG_INFO);
#ifdef EXAMPLE_JPEG
    char* file = (char*)"dehua1.jpg";
    test_jpeg_dec(file);
    char* file_enc_in = (char*)"test.dec";
    test_jpeg_enc(file_enc_in);
#endif

    int dev_cnt;
    bm_dev_getcount(&dev_cnt);
    if (dev_id >= dev_cnt) {
        std::cout << "[TEST JPEG] dev_id should less than device count, only detect "<< dev_cnt << " devices "
                  << std::endl;
        exit(-1);
    }
    printf("device count = %d\n", dev_cnt);
    #ifdef __linux__
    bm_handle_t handle[dev_cnt];
    #else
    std::shared_ptr<bm_handle_t> handle_(new bm_handle_t[dev_cnt], std::default_delete<bm_handle_t[]>());
    bm_handle_t*                 handle = handle_.get();
    #endif
    for (int i = 0; i < dev_cnt; i++) {
        int id;
        if (dev_id != -1) {
            dev_cnt = 1;
            id = dev_id;
        } else {
            id = i;
        }
        bm_status_t req = bm_dev_request(handle + i, id);
        if (req != BM_SUCCESS) {
            printf("create bm handle for dev%d failed!\n", id);
            exit(-1);
        }
    }
    // test for multi-thread
    #ifdef __linux__
    pthread_t pid[dev_cnt * test_threads_num];
    jpeg_thread_arg_t jpeg_thread_arg[dev_cnt * test_threads_num];
    for (int d = 0; d < dev_cnt; d++) {
        for (int i = 0; i < test_threads_num; i++) {
            int idx = d * test_threads_num + i;
            jpeg_thread_arg[idx].loop_num = test_loop_times;
            jpeg_thread_arg[idx].handle = handle[d];
            if (pthread_create(pid + idx, NULL, test_jpeg_enc_and_dec, jpeg_thread_arg + idx)) {
                printf("create thread failed\n");
                return -1;
            }
        }
    }
    for (int d = 0; d < dev_cnt; d++) {
        for (int i = 0; i < test_threads_num; i++) {
            int ret = pthread_join(pid[d * test_threads_num + i], NULL);
            if (ret != 0) {
                printf("Thread join failed\n");
                exit(-1);
            }
        }
    }
    for (int d = 0; d < dev_cnt; d++) {
        bm_dev_free(handle[d]);
    }
    #else
    #define THREAD_NUM 64
    DWORD              dwThreadIdArray[THREAD_NUM];
    HANDLE             hThreadArray[THREAD_NUM];
    jpeg_thread_arg_t* jpeg_thread_arg =
            new jpeg_thread_arg_t[dev_cnt * test_threads_num];
    for (int d = 0; d < dev_cnt; d++) {
        for (int i = 0; i < test_threads_num; i++) {
            int idx = d * test_threads_num + i;
            jpeg_thread_arg[idx].loop_num = test_loop_times;
            jpeg_thread_arg[idx].handle   = handle[d];
            hThreadArray[idx] = CreateThread(
                NULL,                   // default security attributes
                0,                      // use default stack size
                test_jpeg_enc_and_dec,  // thread function name
                jpeg_thread_arg + idx,  // argument to thread function
                0,                      // use default creation flags
                &dwThreadIdArray[idx]);   // returns the thread identifier
            if (hThreadArray[idx] == NULL) {
                delete[] jpeg_thread_arg;
                perror("create thread failed\n");
                exit(-1);
            }
        }
    }
    int   ret = 0;
    DWORD dwWaitResult = WaitForMultipleObjects(
            dev_cnt * test_threads_num, hThreadArray, TRUE, INFINITE);
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
        delete[] jpeg_thread_arg;
        printf("Thread join failed\n");
        return -1;
    }
    for (int d = 0; d < dev_cnt; d++) {
        for (int i = 0; i < test_threads_num; i++) {
            int idx = d * test_threads_num + i;
            CloseHandle(hThreadArray[idx]);
        }
    }
    delete[] jpeg_thread_arg;
    #endif

    std::cout << "------[TEST JPEG] ALL TEST PASSED!" << std::endl;
    return 0;
}
#else

int main() {
    printf("JPEG not support cmodel mode!\n");
    return 0;
}
#endif
