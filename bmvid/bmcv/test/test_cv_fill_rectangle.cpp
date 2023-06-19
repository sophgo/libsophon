#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <cstring>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "test_misc.h"

#ifdef __linux__
  #include <pthread.h>
  #include <sys/time.h>
#else
  #include <windows.h>
  #include "time.h"
#endif

#define MAX_RECT_NUM_TEST 20
#define FILL_RECT_DEBUG (1)

using namespace std;
typedef unsigned char u8;

extern void bm1684x_vpp_write_bin(bm_image dst, const char *output_name);
static void img_len(bm_image_format_ext format,
                    int h, int w,
                    int *height, int *width) {
    switch (format) {
        case FORMAT_YUV420P: {
            width[0]  = w;
            width[1]  = ALIGN(w, 2) / 2;
            width[2]  = width[1];
            height[0] = h;
            height[1] = ALIGN(h, 2) / 2;
            height[2] = height[1];
            break;
        }
        case FORMAT_YUV422P: {
            width[0]  = w;
            width[1]  = ALIGN(w, 2) / 2;
            width[2]  = width[1];
            height[0] = h;
            height[1] = height[0];
            height[2] = height[1];
            break;
        }
        case FORMAT_YUV444P: {
            width[0]  = w;
            width[1]  = width[0];
            width[2]  = width[1];
            height[0] = h;
            height[1] = height[0];
            height[2] = height[1];
            break;
        }
        case FORMAT_NV12:
        case FORMAT_NV21: {
            width[0]  = w;
            width[1]  = ALIGN(w, 2);
            height[0] = h;
            height[1] = ALIGN(h, 2) / 2;
            break;
        }
        case FORMAT_NV16:
        case FORMAT_NV61: {
            width[0]  = w;
            width[1]  = ALIGN(w, 2);
            height[0] = h;
            height[1] = h;
            break;
        }
        case FORMAT_GRAY: {
            width[0]  = w;
            height[0] = h;
            break;
        }
        case FORMAT_COMPRESSED: {
            width[0]  = w;
            height[0] = h;
            break;
        }
        case FORMAT_BGR_PACKED:
        case FORMAT_RGB_PACKED: {
            width[0]  = w * 3;
            height[0] = h;
            break;
        }
        case FORMAT_BGR_PLANAR:
        case FORMAT_RGB_PLANAR: {
            width[0]  = w;
            height[0] = h * 3;
            break;
        }
        case FORMAT_RGBP_SEPARATE:
        case FORMAT_BGRP_SEPARATE: {
            width[0]  = w;
            width[1]  = width[0];
            width[2]  = width[1];
            height[0] = h;
            height[1] = height[0];
            height[2] = height[1];
            break;
        default:
            break;
        }
    }
}

static void calculate_yuv(u8 r, u8 g, u8 b, u8* y_, u8* u_, u8* v_) {
    float y, u, v;
    y = (0.257 * r + 0.504 * g + 0.098 * b + 16);
    u = (-0.148 * r - 0.291 * g + 0.439 * b + 128);
    v = (0.439 * r - 0.368 * g - 0.071 * b + 128);
    y = (y <= 0) ? 0 : y;
    y = (y >= 255) ? 255 : y;
    u = (u <= 0) ? 0 : u;
    u = (u >= 255) ? 255 : u;
    v = (v <= 0) ? 0 : v;
    v = (v >= 255) ? 255 : v;

    *y_ = (u8)y;
    *u_ = (u8)u;
    *v_ = (u8)v;
}

static bmcv_rect_t refine_rect(bmcv_rect_t rect,
                               int height,
                               int width) {
    int up_left_x = bm_max(0, rect.start_x);
    int up_left_y = bm_max(0, rect.start_y);
    int right_down_x = bm_min(width - 1, rect.start_x + rect.crop_w - 1);
    int right_down_y = bm_min(height - 1, rect.start_y + rect.crop_h - 1);
    bmcv_rect_t res = {0, 0, 0, 0};
    if (right_down_y - up_left_y < 0 || right_down_x - up_left_x < 0) {
        return res;
    }
    res.start_x = up_left_x;
    res.start_y = up_left_y;
    res.crop_w  = right_down_x - up_left_x + 1;
    res.crop_h = right_down_y - up_left_y + 1;
    return res;
}

static int fill_solid_rectangle(u8 *base,
                                int pixel_stride,
                                int row_stride,
                                int h,
                                int w,
                                bmcv_rect_t rect,
                                u8 val) {
    int i;
    int j;
    int up_left_x = bm_max(0, rect.start_x);
    int up_left_y = bm_max(0, rect.start_y);
    int right_down_x = bm_min(w - 1, rect.start_x + rect.crop_w - 1);
    int right_down_y = bm_min(h - 1, rect.start_y + rect.crop_h - 1);

    for (i = up_left_y; i <= right_down_y; i++) {
        for (j = up_left_x; j <= right_down_x; j++) {
            base[i * row_stride + j * pixel_stride] = val;
        }
    }
    return 0;
}

static void get_stride(int *stride,
                       bm_image_format_ext image_format,
                       bm_image_data_format_ext data_type,
                       int h,
                       int w) {
    int data_size = 1;
    switch (data_type) {
        case DATA_TYPE_EXT_FLOAT32:
            data_size = 4;
            break;
        case DATA_TYPE_EXT_4N_BYTE:
        case DATA_TYPE_EXT_4N_BYTE_SIGNED:
            data_size = 4;
            break;
        default:
            data_size = 1;
            break;
    }

    stride[0] = 0;
    stride[1] = 0;
    stride[2] = 0;
    switch (image_format) {
        case FORMAT_YUV420P: {
            stride[0] = w * data_size;
            stride[1] = (ALIGN(w, 2) >> 1) * data_size;
            stride[2] = stride[1];
            break;
        }
        case FORMAT_YUV422P: {
            stride[0] = w * data_size;
            stride[1] = (ALIGN(w, 2) >> 1) * data_size;
            stride[2] = stride[1];
            break;
        }
        case FORMAT_YUV444P: {
            stride[0] = w * data_size;
            stride[1] = w * data_size;
            stride[2] = stride[1];
            break;
        }
        case FORMAT_NV12:
        case FORMAT_NV21: {
            stride[0] = w * data_size;
            stride[1] = ALIGN(w, 2) * data_size;
            break;
        }
        case FORMAT_NV16:
        case FORMAT_NV61: {
            stride[0] = w * data_size;
            stride[1] = ALIGN(w, 2) * data_size;
            break;
        }
        case FORMAT_GRAY: {
            stride[0] = w * data_size;
            break;
        }
        case FORMAT_COMPRESSED: {
            break;
        }
        case FORMAT_BGR_PACKED:
        case FORMAT_RGB_PACKED: {
            stride[0] = w * 3 * data_size;
            break;
        }
        case FORMAT_BGR_PLANAR:
        case FORMAT_RGB_PLANAR: {
            stride[0] = w * data_size;
            break;
        }
        case FORMAT_BGRP_SEPARATE:
        case FORMAT_RGBP_SEPARATE: {
            stride[0] = w * data_size;
            stride[1] = w * data_size;
            stride[2] = w * data_size;
            break;
        }
        default:
            break;
    }

    UNUSED(h);
}

static int fill_single_rectangle_refs(u8 **org_img,
                                      bm_image_format_ext image_format,
                                      int *img_len,
                                      bmcv_rect_t rect,
                                      int h,
                                      int w,
                                      unsigned char r,
                                      unsigned char g,
                                      unsigned char b) {
    u8  fill_val[3] = {r, g, b};
    int stride[3] = {0};

    get_stride(stride, image_format, DATA_TYPE_EXT_1N_BYTE, h, w);
    if (image_format == FORMAT_BGR_PLANAR || image_format == FORMAT_BGR_PACKED) {
        std::swap(fill_val[0], fill_val[2]);
    }

    if (image_format == FORMAT_BGR_PLANAR || image_format == FORMAT_RGB_PLANAR) {
        for (int i = 0; i < 3; i++) {
            u8 *plane_base = org_img[0] + i * h * stride[0];
            fill_solid_rectangle(
                plane_base, 1, stride[0], h, w, rect, fill_val[i]);
        }
    }

    else if (image_format == FORMAT_BGR_PACKED || image_format == FORMAT_RGB_PACKED) {
        for (int i = 0; i < 3; i++) {
            u8 *plane_base = org_img[0] + i;
            fill_solid_rectangle(
                plane_base, 3, stride[0], h, w, rect, fill_val[i]);
        };
    }

    else if (image_format == FORMAT_NV12 || image_format == FORMAT_NV21 ||
             image_format == FORMAT_NV16 || image_format == FORMAT_NV61 ||
             image_format == FORMAT_YUV420P) {
        calculate_yuv(r, g, b, fill_val, fill_val + 1, fill_val + 2);

        if (image_format == FORMAT_NV21 || image_format == FORMAT_NV61) {
            std::swap(fill_val[1], fill_val[2]);
        }

        // y plane
        u8 *plane_base = org_img[0];
        fill_solid_rectangle(plane_base,
                             1,
                             stride[0],
                             h,
                             w,
                             rect,
                             fill_val[0]);

        if (image_format == FORMAT_NV16 || image_format == FORMAT_NV61) {
            for (int i = 0; i < 2; i++) {
                plane_base = org_img[1] + i;
                bmcv_rect_t rect_;
                rect_.start_x = rect.start_x / 2;
                rect_.start_y = rect.start_y;
                rect_.crop_w  = ALIGN(rect.crop_w, 2) / 2;
                rect_.crop_h  = rect.crop_h;
                fill_solid_rectangle(plane_base,
                                     2,
                                     stride[1],
                                     h,
                                     w,
                                     rect_,
                                     fill_val[i + 1]);
            }
        }
        else if (image_format == FORMAT_NV12 || image_format == FORMAT_NV21) {
            for (int i = 0; i < 2; i++) {
                plane_base = org_img[1] + i;
                bmcv_rect_t rect_;
                rect_.start_x = rect.start_x / 2;
                rect_.start_y = rect.start_y / 2;
                rect_.crop_w  = ALIGN(rect.crop_w, 2) / 2;
                rect_.crop_h  = ALIGN(rect.crop_h, 2) / 2;
                fill_solid_rectangle(plane_base,
                                     2,
                                     stride[1],
                                     h / 2,
                                     w,
                                     rect_,
                                     fill_val[i + 1]);
            }
        }

        else if (image_format == FORMAT_YUV420P) {
            u8 *base1 = org_img[1];
            u8 *base2 = org_img[2];

            bmcv_rect_t rect_;
            rect_.start_x = rect.start_x / 2;
            rect_.start_y = rect.start_y / 2;
            rect_.crop_w  = ALIGN(rect.crop_w, 2) / 2;
            rect_.crop_h  = ALIGN(rect.crop_h, 2) / 2;
            fill_solid_rectangle(base1,
                                 1,
                                 stride[1],
                                 h / 2,
                                 w / 2,
                                 rect_,
                                 fill_val[1]);
            fill_solid_rectangle(base2,
                                 1,
                                 stride[2],
                                 h / 2,
                                 w / 2,
                                 rect_,
                                 fill_val[2]);
        }
    }
    UNUSED(img_len);

    return 0;
}

static int fill_rectangle_refs(u8 **org_img,
                               bm_image_format_ext image_format,
                               int *img_len,
                               int rect_num,
                               bmcv_rect_t *rects,
                               int h,
                               int w,
                               unsigned char r,
                               unsigned char g,
                               unsigned char b) {
    for (int i = 0; i < rect_num; i++) {
        bmcv_rect_t rect = refine_rect(rects[i], h, w);
        fill_single_rectangle_refs(org_img, image_format, img_len, rect,
                                   h, w, r, g, b);
    }
    return 0;
}

static bool fill_rectangle_cmp(u8 **hw_rslt,
                               u8 **refs_rslt,
                               int plane_num,
                               int *img_len) {
    int i, j;
    for (i = 0; i < plane_num; i++) {
        for (j = 0; j < img_len[i]; j++) {
            if (hw_rslt[i][j] != refs_rslt[i][j]) {
                cout << "testcase failed at i = "
                    << i
                    << " j = "
                    << j
                    << " expect "
                    << refs_rslt[i][j]
                    << " got "
                    << hw_rslt[i][j]
                    << endl;
                return false;
            }
        }
    }
    cout << "testcase success!" << endl;
    return true;
}

static int test_rectangle_random(bm_handle_t handle,
    bm_image_format_ext image_format) {

    int i, j;
    int h, w;
    bm_image image;
    u8 **org_img;
    u8 **hw_rslt;
    int image_len[4]={0};
    int rect_num;
    bmcv_rect_t *rect;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    int plane_height[3];
    int plane_width[3];
    int plane_num;

    h = rand() % 1072 + 12;
    w = rand() % 1912 + 12;
    cout << "fill_rect_test start! " <<endl;
    cout << "image_format is " << image_format <<endl;
    cout << "h = " << h << endl;
    cout << "w = " << w << endl;
    rect_num = rand() % MAX_RECT_NUM_TEST + 1;
    rect = new bmcv_rect_t[rect_num];
    memset(rect, 0x0, sizeof(bmcv_rect_t) * rect_num);
    for (i = 0; i < 3; i++) {
        plane_height[i] = 0;
        plane_width[i] = 0;
    }
    img_len(image_format, h, w,
        plane_height, plane_width);
    plane_num = 4;

    for (i = 0; i < rect_num; i++) {
        //rect[i].start_x = rand() % (w - 10) + 10;
        //rect[i].start_y = rand() % (h - 10) + 10;
        //rect[i].crop_w = rand() % (w - rect[i].start_x - 20) + 10;
        //rect[i].crop_h = rand() % (h - rect[i].start_y - 20) + 10;
        rect[i].start_x = rand() % ((w-8)/4);
        rect[i].start_y = rand() % ((h-8)/4);
        rect[i].crop_w = (rand() % (w-8)/2) + 8;
        rect[i].crop_h = (rand() % (h-8)/2) + 8;
        cout << "num:"<< i <<",strat_x:"<< rect[i].start_x << ",start_y:"<<rect[i].start_y <<",crop_w:"<<rect[i].crop_w<<",crop_h:"<<rect[i].crop_h<<endl;
    }
    r = (unsigned char)(rand() % 255);
    g = (unsigned char)(rand() % 255);
    b = (unsigned char)(rand() % 255);

    bm_image_create(handle,
                    h,
                    w,
                    (bm_image_format_ext)image_format,
                    (bm_image_data_format_ext)DATA_TYPE_EXT_1N_BYTE,
                    &image);

    bm_image_alloc_dev_mem(image);
    bm_image_get_byte_size(image, image_len);
    for (i = 0; i < plane_num; i++) {
        if (image_len[i] == 0) {
            plane_num = i;
            break;
        }
    }
    org_img = new u8 *[plane_num];
    hw_rslt = new u8 *[plane_num];
    for (i = 0; i < plane_num; i++) {
        if (image_len[i] != 0) {
            org_img[i] = new u8[image_len[i]];
            hw_rslt[i] = new u8[image_len[i]];
        }
    }
    for(i = 0; i < plane_num; i++) {
        if (image_len[i] != 0) {
            memset(org_img[i], 0x0, image_len[i]);
            memset(hw_rslt[i], 0x0, image_len[i]);
        }
    }
    for (i = 0; i < plane_num; i++) {
        if (image_len[i] != 0)
            for (j = 0; j < image_len[i]; j++)
                org_img[i][j] = (u8)(rand() % 255);
    }
    bm_image_copy_host_to_device(image, (void **)org_img);

   bm_status_t ret = BM_SUCCESS;

    struct timeval t1, t2;
    gettimeofday_(&t1);
    ret = bmcv_image_fill_rectangle(handle, image, rect_num, rect, r, g, b);
    gettimeofday_(&t2);
    long used = (t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec;
    printf("crop used time: %ld us\n", used);

    char fn[20] = "fill_rect.bin";
    if (BM_SUCCESS != ret) {
        std::cout << "bmcv_api running failed!" << std::endl;
        ret = BM_ERR_FAILURE;
        goto exit;
    }
#if FILL_RECT_DEBUG
    bm1684x_vpp_write_bin(image, fn);
#endif
    bm_image_copy_device_to_host(image, (void **)hw_rslt);

    fill_rectangle_refs(org_img, image_format, image_len,
                        rect_num, rect, h, w, r, g, b);
    if(false == fill_rectangle_cmp(hw_rslt, org_img, plane_num, image_len)){
        std::cout << "cmp error!" << std::endl;
        ret = BM_ERR_FAILURE;
        goto exit;
    }

exit:
    delete[] rect;
    for(i = 0; i < plane_num;i++) {
        if(image_len[i] != 0) {
            delete[] hw_rslt[i];
            delete[] org_img[i];
        }
    }
    delete[] hw_rslt;
    delete[] org_img;
    bm_image_destroy(image);
    return ret;
}

#ifdef __linux__
static void *test_fill_rectangle_thread(void *arg) {
#else
DWORD WINAPI test_fill_rectangle_thread(LPVOID arg) {
#endif

    int i;
    int loop_times;
    unsigned int seed;
    bm_handle_t handle;

    bm_dev_request(&handle, 0);
    loop_times = *(int *)arg;
    #ifdef __linux__
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << pthread_self() << std::endl;
    #else
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << GetCurrentThreadId() << std::endl;
    #endif
    std::cout << "[TEST CV DRAW RECTANGLE] test starts... LOOP times will be "
              << loop_times << std::endl;

    unsigned int chipid = BM1684X;
    bm_get_chipid(handle, &chipid);
    for(i = 0; i < loop_times; i++) {
        std::cout << "------[TEST CV DRAW RECTANGLE] LOOP " << i << "------"
                  << std::endl;
        seed = (unsigned)time(NULL);
        std::cout << "seed: " << seed << std::endl;
        srand(seed);

        cout << "start of loop test " << i << endl;
        if(chipid == 0x1684){
            test_rectangle_random(handle, FORMAT_BGR_PLANAR);
            test_rectangle_random(handle, FORMAT_BGR_PACKED);
            test_rectangle_random(handle, FORMAT_NV12);
            test_rectangle_random(handle, FORMAT_NV16);
            test_rectangle_random(handle, FORMAT_YUV420P);
            }
        else if(chipid == BM1684X){
            test_rectangle_random(handle, FORMAT_BGR_PLANAR);
            test_rectangle_random(handle, FORMAT_BGR_PACKED);
            }
    }
    bm_dev_free(handle);
    return 0;
}

int main(int argc, char **argv) {
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
                     "order:test_cv_fill_rectangle loop_num multi_thread_num"
                  << std::endl;
        exit(-1);
    }
    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST CV DRAW RECTANGLE] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    if (test_threads_num > 4 || test_threads_num < 1) {
        std::cout << "[TEST CV DRAW RECTANGLE] thread nums should be 1~4 " << std::endl;
        exit(-1);
    }
    #ifdef __linux__
    pthread_t *          pid = new pthread_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        if (pthread_create(
                &pid[i], NULL, test_fill_rectangle_thread, (void *)(&test_loop_times))) {
            delete[] pid;
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int ret = 0;
    for (int i = 0; i < test_threads_num; i++) {
        ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            delete[] pid;
            perror("Thread join failed");
            exit(-1);
        }
    }
    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    delete[] pid;
    #else
    #define THREAD_NUM 64
    DWORD              dwThreadIdArray[THREAD_NUM];
    HANDLE             hThreadArray[THREAD_NUM];
    for (int i = 0; i < test_threads_num; i++) {
        hThreadArray[i] = CreateThread(
            NULL,                        // default security attributes
            0,                           // use default stack size
            test_fill_rectangle_thread,  // thread function name
            (void *)(&test_loop_times),  // argument to thread function
            0,                           // use default creation flags
            &dwThreadIdArray[i]);        // returns the thread identifier
        if (hThreadArray[i] == NULL) {
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int   ret = 0;
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
        printf("Thread join failed\n");
        return -1;
    }
    for (int i = 0; i < test_threads_num; i++)
        CloseHandle(hThreadArray[i]);
    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    #endif

    return 0;
}
