#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <assert.h>
#include <iostream>
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "bmlib_runtime.h"
#ifdef __linux__
  #include <sys/time.h>
#else
  #include <windows.h>
  #include "time.h"
#endif
#include <string.h>

using namespace std;

#define IS_YUV(a) (a == FORMAT_NV12 || a == FORMAT_NV21 || a == FORMAT_NV16 \
                || a == FORMAT_NV61 || a == FORMAT_NV24 || a == FORMAT_YUV420P \
                || a == FORMAT_YUV422P || a == FORMAT_YUV444P)
#define IS_RGB(a) (a == FORMAT_RGB_PACKED || a == FORMAT_RGB_PLANAR \
                || a == FORMAT_BGR_PACKED || a == FORMAT_BGR_PLANAR \
                || a == FORMAT_RGBP_SEPARATE || a == FORMAT_BGRP_SEPARATE)

int csc_crop_resize(int argc, char** argv) {
#ifndef USING_CMODEL
    int loop = 10000;
    int iw = 1280;
    int ih = 720;
    int rw = 32;
    int rh = 32;
    int crop_w = 32;
    int crop_h = 32;
    int crop_num = 1;
    int pad_h = 0;
    int pad_w = 0;
    bm_image_format_ext fmt_i = FORMAT_YUV420P;
    bm_image_format_ext fmt_o = FORMAT_RGB_PLANAR;

    if (argc < 10) {
        cout << "csc_crop_resize parameter error!" << endl;
        cout << "parameter list: in_fmt out_fmt in_w in_h crop_w crop_h resize_w resize_h crop_num pad_w pad_h" << endl;
        return -1;
    }
    fmt_i = argc > 2 ? (bm_image_format_ext)atoi(argv[2]) : fmt_i;
    fmt_o = argc > 3 ? (bm_image_format_ext)atoi(argv[3]) : fmt_o;
    iw = argc > 4 ? atoi(argv[4]) : iw;
    ih = argc > 5 ? atoi(argv[5]) : ih;
    crop_w = argc > 6 ? atoi(argv[6]) : crop_w;
    crop_h = argc > 7 ? atoi(argv[7]) : crop_h;
    rw = argc > 8 ? atoi(argv[8]) : rw;
    rh = argc > 9 ? atoi(argv[9]) : rh;
    crop_num = argc > 10 ? atoi(argv[10]) : crop_num;
    pad_w = argc > 11 ? atoi(argv[11]) : pad_w;
    pad_h = argc > 12 ? atoi(argv[12]) : pad_h;
    int ow = rw + 2 * pad_w;
    int oh = rh + 2 * pad_h;

    cout << "---------------parameter-------------" << endl;
    cout << "fmt convert: " << fmt_i << " -> " << fmt_o << endl;
    cout << "crop_num: " << crop_num << endl;
    cout << "input size: " << iw << " * " << ih << endl;
    cout << "crop size: " << crop_w << " * " << crop_h << endl;
    cout << "resize size: " << rw << " * " << rh << endl;
    cout << "pad_w: " << pad_w << "   pad_h: " << pad_h << endl;
    cout << "-------------------------------------" << endl;

    bm_handle_t handle;
    int ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        cout << "device request failed" << endl;
        return -1;
    }
    bmcv_rect_t rect[10];
    bmcv_padding_atrr_t pad[10];
    bm_image src;
    bm_image dst[10];
    int dst_yuv_stride[3] = {(ow + 63) / 64 * 64, (ow + 31) / 32 * 32, (ow + 31) / 32 * 32};
    int dst_rgb_stride[3] = {(ow + 63) / 64 * 64, (ow + 63) / 64 * 64, (ow + 63) / 64 * 64};
    int src_yuv_stride[3] = {(iw + 63) / 64 * 64, (iw + 31) / 32 * 32, (iw + 31) / 32 * 32};
    int src_rgb_stride[3] = {(iw + 63) / 64 * 64, (iw + 63) / 64 * 64, (iw + 63) / 64 * 64};
    // creat input bm_image and alloc device memory for it
    bm_image_create(handle, ih, iw, fmt_i, DATA_TYPE_EXT_1N_BYTE, &src,
                    IS_YUV(fmt_i) ? src_yuv_stride : src_rgb_stride);
    bm_image_alloc_dev_mem_heap_mask(src, 6);
    // prepare input data and copy it from host memory to device memory
    int ch = IS_RGB(fmt_i) ? 3 : 1;
    std::shared_ptr<unsigned char*> ch0_ptr = std::make_shared<unsigned char*>(new unsigned char[ih * iw * ch]);
    std::shared_ptr<unsigned char*> ch1_ptr = std::make_shared<unsigned char*>(new unsigned char[ih * iw]);
    std::shared_ptr<unsigned char*> ch2_ptr = std::make_shared<unsigned char*>(new unsigned char[ih * iw]);
    memset((void *)(*ch0_ptr.get()), 15, ih * iw);
    memset((void *)(*ch1_ptr.get()), 115, ih * iw);
    memset((void *)(*ch2_ptr.get()), 215, ih * iw);
    unsigned char *host_ptr[] = {*ch0_ptr.get(), *ch1_ptr.get(), *ch2_ptr.get()};
    bm_image_copy_host_to_device(src, (void **)host_ptr);

    // config pad and crop info, create dst bm_image and alloc device memory for it.
    for (int i = 0; i < crop_num; i++) {
        pad[i].dst_crop_stx = pad_w;
        pad[i].dst_crop_sty = pad_h;
        pad[i].dst_crop_w = rw;
        pad[i].dst_crop_h = rh;
        pad[i].padding_r = 0;
        pad[i].padding_g = 0;
        pad[i].padding_b = 0;
        pad[i].if_memset = 0;
        rect[i].start_x = 50 * i;
        rect[i].start_y = 50 * i;
        rect[i].crop_h = crop_h;
        rect[i].crop_w = crop_w;
        rect[i].start_x = rect[i].start_x + crop_w > iw ? iw - crop_w : rect[i].start_x;
        rect[i].start_y = rect[i].start_y + crop_h > ih ? ih - crop_h : rect[i].start_y;
        bm_image_create(handle, oh, ow, fmt_o, DATA_TYPE_EXT_1N_BYTE, dst + i,
                        IS_YUV(fmt_o) ? dst_yuv_stride : dst_rgb_stride);
        bm_image_alloc_dev_mem(dst[i]);
    }

    struct timeval t1, t2;
    gettimeofday_(&t1);

    for (int i = 0; i < loop; i++) {
        if (pad_h || pad_w) {
            for (int j = 0; j < crop_num; j++) {
                bm_device_mem_t dev_mem[3];
                bm_image_get_device_mem(dst[j], dev_mem);
                for (int k = 0; k < bm_image_get_plane_num(dst[j]); k++)
                    bm_memset_device(handle, 0, dev_mem[k]);
            }
        }
        bmcv_image_vpp_basic(handle, 1, &src, dst, &crop_num, rect, pad, BMCV_INTER_NEAREST);
    }

    gettimeofday_(&t2);
    cout << "vpp basic using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) / loop << "us" << endl;
    // copy result from device memory to host
    bm_image_copy_device_to_host(dst[0], (void **)host_ptr);

    bm_image_destroy(src);
    for (int i = 0; i < crop_num; i++) {
        bm_image_destroy(dst[i]);
    }
    bm_dev_free(handle);
#else
    UNUSED(argc);
    UNUSED(argv);
    cout << "NOT support cmodel" << endl;
#endif
    return 0;
}

int warp_affine(int argc, char** argv) {
    int loop = 10000;
    int iw = 1280;
    int ih = 720;
    int ow = 32;
    int oh = 32;
    bm_image_format_ext fmt = FORMAT_RGB_PLANAR;
    int use_bilinear = 0;

    if (argc < 7) {
        cout << "warp_affine parameter error!" << endl;
        cout << "parameter list: iw ih ow oh use_bilinear" << endl;
        return -1;
    }
    iw = atoi(argv[2]);
    ih = atoi(argv[3]);
    ow = atoi(argv[4]);
    oh = atoi(argv[5]);
    use_bilinear = atoi(argv[6]);

    cout << "---------------parameter-------------" << endl;
    cout << "input size: " << iw << " * " << ih << endl;
    cout << "output size: " << ow << " * " << oh << endl;
    cout << "use_bilinear: " << use_bilinear << endl;
    cout << "-------------------------------------" << endl;

    bm_handle_t handle;
    int ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        cout << "device request failed" << endl;
        return -1;
    }
    bm_image src, dst;
    bm_image_create(handle, ih, iw, fmt, DATA_TYPE_EXT_1N_BYTE, &src);
    bm_image_alloc_dev_mem(src);
    bm_image_create(handle, oh, ow, fmt, DATA_TYPE_EXT_1N_BYTE, &dst);
    bm_image_alloc_dev_mem(dst);
    float trans[6] = {3.848, -0.024, 916.203,
                       0.024, 3.898, 55.978};
    bmcv_affine_image_matrix matrix_image;
    bmcv_affine_matrix* matrix = (bmcv_affine_matrix *)(trans);
    matrix_image.matrix_num = 1;
    matrix_image.matrix = matrix;

    struct timeval t1, t2;
    gettimeofday_(&t1);
    for (int i = 0; i < loop; i++)
        bmcv_image_warp_affine(handle, 1, &matrix_image, &src, &dst, use_bilinear);
    gettimeofday_(&t2);
    cout << "using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) / loop << "us" << endl;

    bm_image_destroy(src);
    bm_image_destroy(dst);
    bm_dev_free(handle);
    return 0;
}

int main(int argc, char** argv) {
    int func = argc > 1 ? atoi(argv[1]) : 0;
    if (func < 0 || func > 1) {
        cout << "function choose error!" << endl;
        cout << "0: csc-crop-resize" << endl;
        cout << "1: warp-affine" <<endl;
        return -1;
    }
    switch(func) {
        case 0:
            csc_crop_resize(argc, argv);
            break;
        case 1:
            warp_affine(argc, argv);
            break;
        default:
            assert(0);
    }
    return 0;
}
