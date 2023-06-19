#include "bmlib_runtime.h"
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <memory>
#ifdef __linux__
  #include <sys/time.h>
#else
  #include <windows.h>
  #include "time.h"
#endif
#include <algorithm>

using namespace std;

static bm_status_t load_int2float_lookup_table_(bm_handle_t  handle,
                                                const void*  table,
                                                unsigned int size) {
    bm_device_mem_t dev_mem =
        bm_mem_from_device(0x10323c00, size);
    return bm_memcpy_s2d(handle, dev_mem, (void*)table);
}

static bm_status_t load_int2float_lookup_table(bm_handle_t handle) {
    float table[256];
    for (int i = 0; i < 256; i++) {
        table[i] = (float)i;
    }
    return load_int2float_lookup_table_(handle, (void*)table, 256 * sizeof(float));
}

static int yuv2hsv_ref(
        bm_image_format_ext fmt_i,
        int ih,
        int iw,
        unsigned char ** yuv,
        unsigned char ** hsv,
        bmcv_rect_t* rect,
        int pad_h,
        int pad_w) {
    UNUSED(ih);
    int oh = rect->crop_h + 2 * pad_h;
    int ow = rect->crop_w + 2 * pad_w;
    for (int i = 0; i < oh; i++) {
        for (int j = 0; j < ow; j++) {
            if (i < pad_h || i >= rect->crop_h + pad_h || j < pad_w || j >= rect->crop_w + pad_w) {
                hsv[0][i * ow + j] = 0;
                hsv[1][i * ow + j] = 0;
                hsv[2][i * ow + j] = 0;
                continue;
            }
            int y_idx = (rect->start_y + i - pad_h) * iw + rect->start_x + j - pad_w;
            int uv_idx = (rect->start_y + i - pad_h) / 2 * iw / 2 + (rect->start_x + j - pad_w) / 2;
            int u_idx = (rect->start_y + i - pad_h) / 2 * iw + (rect->start_x + j - pad_w) / 2 * 2;
            int v_idx = (rect->start_y + i - pad_h) / 2 * iw + (rect->start_x + j - pad_w) / 2 * 2 + 1;
            int y = (unsigned char)yuv[0][y_idx] - 16;
            int u = 0;
            int v = 0;
            if (fmt_i == FORMAT_YUV420P) {
                u = (unsigned char)yuv[1][uv_idx] - 128;
                v = (unsigned char)yuv[2][uv_idx] - 128;
            } else if (fmt_i == FORMAT_NV12) {
                u = (unsigned char)yuv[1][u_idx] - 128;
                v = (unsigned char)yuv[1][v_idx] - 128;
            } else if (fmt_i == FORMAT_NV21) {
                u = (unsigned char)yuv[1][v_idx] - 128;
                v = (unsigned char)yuv[1][u_idx] - 128;
            }
            float r_fp = 1.16438 * y + 1.59603 * v;
            float g_fp = 1.16438 * y - 0.39176 * u - 0.81297 * v;
            float b_fp = 1.16438 * y + 2.01723 * u;

            r_fp = r_fp > 255.0 ? 255.0 : (r_fp < 0.0 ? 0.0 : r_fp);
            g_fp = g_fp > 255.0 ? 255.0 : (g_fp < 0.0 ? 0.0 : g_fp);
            b_fp = b_fp > 255.0 ? 255.0 : (b_fp < 0.0 ? 0.0 : b_fp);

            #ifdef __linux__
            float max = std::max(std::max(r_fp, g_fp), b_fp);
            float min = std::min(std::min(r_fp, g_fp), b_fp);
            #else
            float max = (std::max)((std::max)(r_fp, g_fp), b_fp);
            float min = (std::min)((std::min)(r_fp, g_fp), b_fp);
            #endif
            float v_fp = max;
            float s_fp = 255.f * (max - min) / max;
            float h_fp = 0.0;
            if (max - min == 0) {
                h_fp = 0;
            } else {
              if (r_fp == max) h_fp = (g_fp - b_fp) / (max - min) * 60;
              if (g_fp == max) h_fp = 120 + (b_fp - r_fp) / (max - min) * 60;
              if (b_fp == max) h_fp = 240 + (r_fp - g_fp) / (max - min) * 60;
            }

            if (h_fp < 0) h_fp = h_fp + 360;
            h_fp = h_fp / 2; // opencv'h is range [0, 180]

            hsv[0][i * ow + j] =
                    (unsigned char)((h_fp > 255 ? 255 : (h_fp < 0 ? 0 : h_fp)) + 0.5);
            hsv[1][i * ow + j] =
                    (unsigned char)((s_fp > 255 ? 255 : (s_fp < 0 ? 0 : s_fp)) + 0.5);
            hsv[2][i * ow + j] =
                    (unsigned char)((v_fp > 255 ? 255 : (v_fp < 0 ? 0 : v_fp)) + 0.5);
        }
    }
    return 0;
}

static int cmp(
        unsigned char* exp,
        unsigned char* got,
        int len) {
    for (int i = 0; i < len; i++) {
        int diff = (exp[i] > got[i]) ? (exp[i] - got[i]) : (got[i] - exp[i]);
        if (diff > 1) {
            printf("ERROR! idx = %d  exp = %d  got = %d\n", i, exp[i], got[i]);
            return -1;
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    int loop = 1;
    int ih = 1920;
    int iw = 1080;
    int crop_h = 1920;
    int crop_w = 1080;
    int pad_h = 0;
    int pad_w = 0;
    bm_image_format_ext fmt_i = FORMAT_YUV420P;

    fmt_i = argc > 1 ? (bm_image_format_ext)atoi(argv[1]) : fmt_i;
    ih = argc > 2 ? atoi(argv[2]) : ih;
    iw = argc > 3 ? atoi(argv[3]) : iw;
    crop_h = argc > 4 ? atoi(argv[4]) : crop_h;
    crop_w = argc > 5 ? atoi(argv[5]) : crop_w;
    pad_h = argc > 6 ? atoi(argv[6]) : pad_h;
    pad_w = argc > 7 ? atoi(argv[7]) : pad_w;
    loop = argc > 8 ? atoi(argv[8]) : loop;
    int oh = crop_h + 2 * pad_h;
    int ow = crop_w + 2 * pad_w;
    cout << "---------------parameter-------------" << endl;
    cout << "fmt convert: " << fmt_i << " -> HSV" << endl;
    cout << "input size: " << iw << " * " << ih << endl;
    cout << "crop size: " << crop_w << " * " << crop_h << endl;
    cout << "pad_w: " << pad_w << "   pad_h: " << pad_h << endl;
    cout << "ow: " << ow << "   oh: " << oh << endl;
    cout << "-------------------------------------" << endl;
    bm_image_format_ext fmt_o = FORMAT_HSV_PLANAR;
    bm_image_data_format_ext data_type = DATA_TYPE_EXT_1N_BYTE;
    bm_status_t ret = BM_SUCCESS;
    bm_handle_t handle;
    ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS)
        throw("bm_dev_request failed");
    // load lookup table for uint to float
    ret = load_int2float_lookup_table(handle);
    if (ret != BM_SUCCESS)
        throw("load lookup table failed");
    bm_image input;
    bm_image output;
    bmcv_rect_t rect;
    rect.start_x = 10;
    rect.start_y = 10;
    rect.crop_w = crop_w;
    rect.crop_h = crop_h;
    rect.start_x = rect.start_x + crop_w > iw ? 0 : rect.start_x;
    rect.start_y = rect.start_y + crop_h > ih ? 0 : rect.start_y;
    bm_image_create(handle, ih, iw, fmt_i, data_type, &input);
    bm_image_create(handle, oh, ow, fmt_o, data_type, &output);
    bm_image_alloc_dev_mem(input);
    bm_image_alloc_dev_mem(output);
    // fill input data
    std::shared_ptr<unsigned char*> ch0_ptr = std::make_shared<unsigned char*>(new unsigned char[ih * iw]);
    std::shared_ptr<unsigned char*> ch1_ptr = std::make_shared<unsigned char*>(new unsigned char[ih * iw]);
    std::shared_ptr<unsigned char*> ch2_ptr = std::make_shared<unsigned char*>(new unsigned char[ih * iw]);
    std::shared_ptr<unsigned char*> res0_tpu = std::make_shared<unsigned char*>(new unsigned char[oh * ow]);
    std::shared_ptr<unsigned char*> res1_tpu = std::make_shared<unsigned char*>(new unsigned char[oh * ow]);
    std::shared_ptr<unsigned char*> res2_tpu = std::make_shared<unsigned char*>(new unsigned char[oh * ow]);
    std::shared_ptr<unsigned char*> res0_cpu = std::make_shared<unsigned char*>(new unsigned char[oh * ow]);
    std::shared_ptr<unsigned char*> res1_cpu = std::make_shared<unsigned char*>(new unsigned char[oh * ow]);
    std::shared_ptr<unsigned char*> res2_cpu = std::make_shared<unsigned char*>(new unsigned char[oh * ow]);
    for (int i = 0; i < ih * iw; i++) {
        (*ch0_ptr.get())[i] = rand() % 256;
        (*ch1_ptr.get())[i] = rand() % 256;
        (*ch2_ptr.get())[i] = rand() % 256;
    }
    // copy input data from host memory to device memory
    unsigned char *host_ptr[] = {*ch0_ptr.get(), *ch1_ptr.get(), *ch2_ptr.get()};
    bm_image_copy_host_to_device(input, (void **)host_ptr);
    // do yuv2hsv using TPU
    struct timeval t1, t2;
    gettimeofday_(&t1);
    for (int i = 0; i < loop; i++)
        bmcv_image_yuv2hsv(handle, rect, input, output);
    gettimeofday_(&t2);
    cout << "yuv2hsv using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) / loop << "us" << endl;

    // copy output data from device memory to host memory
    unsigned char *out_ptr[] = {*res0_tpu.get(), *res1_tpu.get(), *res2_tpu.get()};
    bm_image_copy_device_to_host(output, (void **)out_ptr);
    unsigned char *res_cpu[] = {*res0_cpu.get(), *res1_cpu.get(), *res2_cpu.get()};
    yuv2hsv_ref(fmt_i, ih, iw, host_ptr, res_cpu, &rect, pad_h, pad_w);
    if (cmp(*res0_cpu.get(), *res0_tpu.get(), oh * ow) ||
        cmp(*res1_cpu.get(), *res1_tpu.get(), oh * ow) ||
        cmp(*res2_cpu.get(), *res2_tpu.get(), oh * ow)) {
        cout << "YUV2HSV failed" << endl;
    } else {
        cout << "YUV2HSV succeed" << endl;
    }
    // free
    bm_image_destroy(input);
    bm_image_destroy(output);
    bm_dev_free(handle);
    return 0;
}
