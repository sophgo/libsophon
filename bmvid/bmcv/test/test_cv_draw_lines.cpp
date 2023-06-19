#include <iostream>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bmlib_runtime.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <assert.h>
#ifdef __linux__
#include <sys/time.h>
#endif
#include <vector>
#include <math.h>
#include <float.h>

using namespace std;

//#define ALIGN(a, b) (((a) + (b) - 1) / (b) * (b))
#define SATURATE(a, s, e) ((a) > (e) ? (e) : ((a) < (s) ? (s) : (a)))

static void fill(
        unsigned char* input,
        int width,
        int height) {
    for (int i = 0; i < height * 3; i++) {
        for (int j = 0; j < width; j++) {
            input[i * width + j] = rand() % 256;
            //input[i * width + j] = (j + i)%256;
        }
    }
}

static vector<int> get_image_size(int format, int width, int height) {
    vector<int> size;
    switch (format) {
        case FORMAT_YUV420P:
            size.push_back(width * height);
            size.push_back(ALIGN(width, 2) * ALIGN(height, 2) >> 2);
            size.push_back(ALIGN(width, 2) * ALIGN(height, 2) >> 2);
            break;
        case FORMAT_YUV422P:
            size.push_back(width * height);
            size.push_back(ALIGN(width, 2) * height >> 1);
            size.push_back(ALIGN(width, 2) * height >> 1);
            break;
        case FORMAT_YUV444P:
        case FORMAT_RGB_PLANAR:
        case FORMAT_BGR_PLANAR:
        case FORMAT_RGB_PACKED:
        case FORMAT_BGR_PACKED:
        case FORMAT_RGBP_SEPARATE:
        case FORMAT_BGRP_SEPARATE:
            size.push_back(width * height);
            size.push_back(width * height);
            size.push_back(width * height);
            break;
        case FORMAT_NV12:
        case FORMAT_NV21:
            size.push_back(width * height);
            size.push_back(ALIGN(width, 2) * ALIGN(height, 2) >> 1);
            break;
        case FORMAT_NV16:
        case FORMAT_NV61:
        case FORMAT_NV24:
            size.push_back(width * height);
            size.push_back(ALIGN(width, 2) * height);
            break;
        case FORMAT_GRAY:
            size.push_back(width * height);
            break;
        default:
            cout << "format error" << endl;
    }
    return size;
}

static void get_image_default_step(int format, int width, int* step) {
    switch (format) {
        case FORMAT_GRAY:
            step[0] = width;
            break;
        case FORMAT_YUV420P:
        case FORMAT_YUV422P:
            step[0] = width;
            step[1] = ALIGN(width, 2) >> 1;
            step[2] = ALIGN(width, 2) >> 1;
            break;
        case FORMAT_YUV444P:
            step[0] = width;
            step[1] = width;
            step[2] = width;
            break;
        case FORMAT_NV12:
        case FORMAT_NV21:
        case FORMAT_NV16:
        case FORMAT_NV61:
            step[0] = width;
            step[1] = ALIGN(width, 2);
            break;
        default:
            cout << "not support format" << endl;
            break;
    }
}

static int cmp(
        unsigned char* got,
        unsigned char* exp,
        int len) {
    for (int i = 0; i < len; i++) {
        if (got[i] != exp[i]) {
            printf("cmp error: idx=%d  exp=%d  got=%d\n", i, exp[i], got[i]);
            return -1;
        }
    }
    return 0;
}

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
    cnt = cnt;
    printf("read from %s %ld bytes\n", filename, *size);
    fclose(fp);
}

static int draw_line_cpu(
        unsigned char* input,
        int height,
        int width,
        int line_num,
        int format,
        const bmcv_point_t* start,
        const bmcv_point_t* end,
        unsigned char color[3],
        int thickness) {
    // clip point
    bmcv_point_t* sp = new bmcv_point_t [line_num];
    bmcv_point_t* ep = new bmcv_point_t [line_num];
    for (int i = 0; i < line_num; i++) {
        sp[i].x = SATURATE(start[i].x, 0, width - 1);
        sp[i].y = SATURATE(start[i].y, 0, height - 1);
        ep[i].x = SATURATE(end[i].x, 0, width - 1);
        ep[i].y = SATURATE(end[i].y, 0, height - 1);
        // because only support start_y < end_y
        if (sp[i].y > ep[i].y) {
            swap(sp[i].x, ep[i].x);
            swap(sp[i].y, ep[i].y);
        }
    }
    vector<int> offsize = get_image_size(format, width, height);
    unsigned char* in_ptr[3] = {input, input + offsize[0], input + offsize[0] + offsize[1]};
    #ifdef __linux__
        struct timeval t1, t2;
        gettimeofday(&t1, NULL);
    #else
        struct timespec tp1, tp2;
        clock_gettime(0, &tp1);
    #endif
    bmcv_color_t rgb = {color[0], color[1], color[2]};
    int step[3];
    get_image_default_step(format, width, step);
    bmMat mat;
    mat.width = width;
    mat.height = height;
    mat.format = (bm_image_format_ext)format;
    mat.step = step;
    mat.data = (void**)in_ptr;
    for (int i = 0; i < line_num; i++) {
        draw_line(mat, sp[i], ep[i], rgb, thickness);
    }
    #ifdef __linux__
        gettimeofday(&t2, NULL);
        cout << "Draw-Line cpu using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;
    #else
        clock_gettime(0, &tp2);
        cout << "Draw-Line cpu using time: " << ((tp2.tv_sec - tp1.tv_sec) * 1000000 + (tp2.tv_nsec - tp1.tv_nsec)/1000) << "us" << endl;
    #endif
    delete [] sp;
    delete [] ep;
    return 0;
}

static int draw_line_bmcv(
        unsigned char* input,
        int height,
        int width,
        int line_num,
        int format,
        const bmcv_point_t* p1,
        const bmcv_point_t* p2,
        unsigned char color[3],
        int thickness,
        bool enable_cpu) {
    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        return -1;
    }
    if (enable_cpu) {
        ret = bmcv_open_cpu_process(handle);
        if (ret != BM_SUCCESS) {
            printf("BMCV enable CPU failed. ret = %d\n", ret);
            bm_dev_free(handle);
            return -1;
        }
    }
    bm_image input_img;
    bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &input_img);
    bm_image_alloc_dev_mem(input_img);
    vector<int> offsize = get_image_size(format, width, height);
    unsigned char* in_ptr[3] = {input, input + offsize[0], input + offsize[0] + offsize[1]};
    bm_image_copy_host_to_device(input_img, (void **)in_ptr);
    bmcv_color_t rgb = {color[0], color[1], color[2]};
    #ifdef __linux__
        struct timeval t1, t2;
        gettimeofday(&t1, NULL);
        bmcv_image_draw_lines(handle, input_img, p1, p2, line_num, rgb, thickness);
        gettimeofday(&t2, NULL);
        cout << "Draw-Line bmcv using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "um" << endl;
    #else
        struct timespec tp1, tp2;
        clock_gettime(0, &tp1);
        bmcv_image_draw_lines(handle, input_img, p1, p2, line_num, rgb, thickness);
        clock_gettime(0, &tp2);
        cout << "Draw-Line bmcv using time: " << ((tp2.tv_sec - tp1.tv_sec) * 1000000 + (tp2.tv_nsec - tp1.tv_nsec)/1000) << "um" << endl;
    #endif
    bm_image_copy_device_to_host(input_img, (void **)in_ptr);
    bm_image_destroy(input_img);
    if (enable_cpu) {
        ret = bmcv_close_cpu_process(handle);
        if (ret != BM_SUCCESS) {
            printf("BMCV disable CPU failed. ret = %d\n", ret);
            bm_dev_free(handle);
            return -1;
        }
    }
    bm_dev_free(handle);
    return 0;
}

static int test_draw_line_random(
        int random,
        int height,
        int width,
        int format,
        bool enable_cpu,
        char* image) {
    if (random) {
        struct timespec tp;
        #ifdef __linux__
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
        #else
        clock_gettime(0, &tp);
        #endif
        unsigned int seed = tp.tv_nsec;
        srand(seed);
        cout << "seed = " << seed << endl;
        width = 100 + rand() % 1900;
        height = 100 + rand() % 2048;
        format = rand() % 7;
    }
    int line_num = 3;
    bmcv_point_t start[4] = {{500, 500}, {500, 500}, {500, 500}, {100, 100}};
    bmcv_point_t end[4] = {{100, 100}, {1000, 100}, {2000, 2000}, {0, 2000}};
    unsigned char color[3] = {255, 0, 0};
    int thickness = 4;
    cout << "format: " << format << endl;
    cout << "width: " << width << "  height: " << height << endl;
    unsigned char* data_cpu = new unsigned char [width * height * 3];
    unsigned char* data_bmcv = new unsigned char [width * height * 3];
    if((image != NULL) && (random != 1)){
        size_t size = 0;
        read_file(image, data_cpu, &size);
    } else {
        fill(data_cpu, width, height);
    }
    memcpy(data_bmcv, data_cpu, width * height * 3);
    draw_line_cpu(
        data_cpu,
        height,
        width,
        line_num,
        format,
        start,
        end,
        color,
        thickness);
    draw_line_bmcv(
        data_bmcv,
        height,
        width,
        line_num,
        format,
        start,
        end,
        color,
        thickness,
        enable_cpu);
    vector<int> img_size = get_image_size(format, width, height);
    int total_sz = 0;
    for (auto sz : img_size) {
        total_sz += sz;
    }
    write_file((char*)"draw_line_cpu.yuv", data_cpu, total_sz);
    write_file((char*)"draw_line_bmcv.yuv", data_bmcv, total_sz);
    int ret = cmp(data_bmcv, data_cpu, total_sz);
    delete [] data_cpu;
    delete [] data_bmcv;
    return ret;
}

int main(int argc, char* args[]) {
    int random = 1;
    int loop = 1;
    int height = 1080;
    int width = 1920;
    int format = 3;
    bool enable_cpu = true;
    char* image = NULL;
    if (argc > 1) random = atoi(args[1]);
    if (argc > 2) loop = atoi(args[2]);
    if (argc > 3) height = atoi(args[3]);
    if (argc > 4) width = atoi(args[4]);
    if (argc > 5) format = atoi(args[5]);
    if (argc > 6) enable_cpu = atoi(args[6]);
    if (argc > 7) image = args[7];
    int ret = 0;
    for (int i = 0; i < loop; i++) {
        ret = test_draw_line_random(random, height, width, format, enable_cpu, image);
        if (ret) {
            cout << "test draw_line failed" << endl;
            return ret;
        }
    }
    cout << "Compare TPU result with OpenCV successfully!" << endl;
    return 0;
}