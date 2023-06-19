#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include "test_misc.h"
#include "bmcv_api_ext.h"

#ifdef __linux__
  #include <unistd.h>
  #include <pthread.h>
  #include <sys/time.h>
#else
  #include <windows.h>
  #include "time.h"
#endif

#define MAX_SIZE (2048)
#define MIN_SIZE (32)

typedef struct {
    int trials;
} copy_to_thread_arg_t;

typedef enum {
    MAX_SIZE_MODE = 0,
    MIN_SIZE_MODE,
    RAND_SIZE_MODE,
    MAX_RAND_MODE
} rand_mode_e;

int gen_test_size(int &in_w,
                  int &in_h,
                  int &out_w,
                  int &out_h,
                  int &start_x,
                  int &start_y,
                  int  gen_mode) {
    switch (gen_mode) {
        case (MAX_SIZE_MODE): {
            out_w = MAX_SIZE;
            out_h = MAX_SIZE;
            break;
        }
        case (MIN_SIZE_MODE): {
            out_w = MIN_SIZE;
            out_h = MIN_SIZE;
            break;
        }
        case (RAND_SIZE_MODE): {
            out_w = MIN_SIZE + rand() % (MAX_SIZE - MIN_SIZE);
            out_h = MIN_SIZE + rand() % (MAX_SIZE - MIN_SIZE);
            break;
        }
        default: {
            out_w = MIN_SIZE + rand() % (MAX_SIZE - MIN_SIZE);
            out_h = MIN_SIZE + rand() % (MAX_SIZE - MIN_SIZE);
            break;
        }
    }
    in_w = (out_w != MIN_SIZE) ? (MIN_SIZE + rand() % (out_w - MIN_SIZE))
                               : (MIN_SIZE);
    in_h = (out_h != MIN_SIZE) ? (MIN_SIZE + rand() % (out_h - MIN_SIZE))
                               : (MIN_SIZE);
    start_x = (out_w != in_w) ? (rand() % (out_w - in_w)) : (0);
    start_y = (out_h != in_h) ? (rand() % (out_h - in_h)) : (0);
    return 0;
}

static void convert_4N_2_1N(unsigned char *inout, int N, int C, int H, int W) {
    unsigned char *temp_buf = new unsigned char[4 * C * H * W];
    for (int i = 0; i < ALIGN(N, 4) / 4; i++) {
        memcpy(temp_buf, inout + 4 * C * H * W * i, 4 * C * H * W);
        for (int loop = 0; loop < C * H * W; loop++) {
            inout[i * 4 * C * H * W + loop] = temp_buf[4 * loop];
            inout[i * 4 * C * H * W + 1 * C * H * W + loop] =
                temp_buf[4 * loop + 1];
            inout[i * 4 * C * H * W + 2 * C * H * W + loop] =
                temp_buf[4 * loop + 2];
            inout[i * 4 * C * H * W + 3 * C * H * W + loop] =
                temp_buf[4 * loop + 3];
        }
    }
    delete[] temp_buf;
}

template <typename T>
static int packedToplanar(T * packed, int channel, int width, int height){
    T * planar = new T[channel * width * height];
    T * p_img = packed;
    T * p_r = planar;
    T * p_g = planar + height * width;
    T * p_b = planar + height * width * 2;
    for(int i = 0; i < height * width; i++){
        * p_r++ = * p_img++;
        * p_g++ = * p_img++;
        * p_b++ = * p_img++;
    }
    memcpy(packed, planar, sizeof(T) * channel * width * height);
    return 0;
}

template <typename T>
static int copy_to_ref_v2(T * src,
                          T * dst,
                          int channel,
                          int in_h,
                          int in_w,
                          int out_h,
                          int out_w,
                          int start_x,
                          int start_y,
                          int ispacked){
    if(ispacked){
        packedToplanar<T>(src, channel, in_w, in_h);
    }
    for(int c = 0; c < channel; c++){
        for(int i = 0; i < in_h; i++){
            for(int j = 0; j < in_w; j++){
                int index = c * out_w * out_h + (start_y + i) * out_w + start_x + j;
                    dst[index] = src[(c * in_h * in_w) + i * in_w + j];
            }
        }
    }
    return 0;
}
template <typename T>
static bool res_ref_comp(T *res_1n_planner, T *ref_1n_planner, int size) {
    for (int idx = 0; idx < size; idx++) {
        if (res_1n_planner[idx] != ref_1n_planner[idx]) {
            std::cout << "[COMP ERROR]: idx: " << idx
                      << " res: " << (int)res_1n_planner[idx]
                      << " ref: " << (int)ref_1n_planner[idx] << std::endl;
            return false;
        }
    }
    return true;
}

template <typename T>
static bool res_ref_comp(T * res_1n_planner,
                         T * ref_1n_planner,
                         int n,
                         int c,
                         int in_h,
                         int in_w,
                         int out_h,
                         int out_w,
                         int start_x,
                         int start_y) {
    int h_len = out_w;
    int c_len = out_w * out_h;
    int n_len = c_len * c;
    for (int n_idx = 0; n_idx < n; n_idx++) {
        for (int c_idx = 0; c_idx < c; c_idx++) {
            for (int h_idx = 0; h_idx < in_h; h_idx++) {
                for (int w_idx = 0; w_idx < in_w; w_idx++) {
                    int idx = n_idx * n_len + c_idx * c_len +
                              (h_idx + start_y) * h_len + w_idx + start_x;
                    if (res_1n_planner[idx] != ref_1n_planner[idx]) {
                        std::cout << "[COMP ERROR]: idx: " << idx
                                  << " res: " << (int)res_1n_planner[idx]
                                  << " ref: " << (int)ref_1n_planner[idx]
                                  << " n_idx: " << n_idx << " c_idx: " << c_idx
                                  << " h_idx: " << h_idx << " w_idx: " << w_idx
                                  << std::endl;
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

template <typename T>
static int copy_to_test_rand(bm_handle_t handle,
                             int         image_format,
                             int         data_type,
                             int         in_h,
                             int         in_w,
                             int         out_h,
                             int         out_w,
                             int         start_x,
                             int         start_y) {
    int channel   = 3;
    int if_packed = false;
    switch (image_format) {
        case FORMAT_RGB_PLANAR: {
            channel = 3;
            break;
        }
        case FORMAT_RGB_PACKED: {
            channel   = 3;
            if_packed = true;
            break;
        }
        case FORMAT_BGR_PLANAR: {
            channel = 3;
            break;
        }
        case FORMAT_BGR_PACKED: {
            channel   = 3;
            if_packed = true;
            break;
        }
        case FORMAT_GRAY: {
            channel = 1;
            break;
        }
        default: {
            channel = 3;
            break;
        }
    }
    int image_n = 1;
    int if_4n   = 0;
    switch (data_type) {
        case DATA_TYPE_EXT_1N_BYTE: {
            image_n = 1;
            if_4n   = 0;
            break;
        }
        case DATA_TYPE_EXT_1N_BYTE_SIGNED: {
            image_n = 1;
            if_4n   = 0;
            break;
        }
        case DATA_TYPE_EXT_FLOAT32: {
            image_n = 1;
            if_4n   = 0;
            break;
        }
        case DATA_TYPE_EXT_4N_BYTE: {
            image_n = 4;
            if_4n   = 1;
            break;
        }
        case DATA_TYPE_EXT_4N_BYTE_SIGNED: {
            image_n = 4;
            if_4n   = 1;
            break;
        }
        default: {
            image_n = 1;
            if_4n   = 0;
            break;
        }
    }
    std::shared_ptr<T> src_ptr(new T[image_n * channel * in_w * in_h],
                               std::default_delete<T[]>());
    std::shared_ptr<T> ref_ptr(new T[image_n * channel * out_w * out_h],
                               std::default_delete<T[]>());
    std::shared_ptr<T> res_ptr(new T[image_n * channel * out_w * out_h],
                               std::default_delete<T[]>());
    T *                src_data = src_ptr.get();
    T *                res_data = res_ptr.get();
    for (int i = 0; i < image_n * channel * in_w * in_h; i++) {
        src_data[i] = rand() % 255;
    }
    // calculate res
    bmcv_copy_to_atrr_t copy_to_attr;
    copy_to_attr.start_x    = start_x;
    copy_to_attr.start_y    = start_y;
    copy_to_attr.padding_r  = 0;
    copy_to_attr.padding_g  = 0;
    copy_to_attr.padding_b  = 0;
    copy_to_attr.if_padding = 0;
    bm_image input, output;
    bm_image_create(handle,
                    in_h,
                    in_w,
                    (bm_image_format_ext)image_format,
                    (bm_image_data_format_ext)data_type,
                    &input);
    bm_image_alloc_dev_mem(input);
    bm_image_copy_host_to_device(input, (void **)&src_data);
    bm_image_create(handle,
                    out_h,
                    out_w,
                    (bm_image_format_ext)image_format,
                    (bm_image_data_format_ext)data_type,
                    &output);
    bm_image_alloc_dev_mem(output);
    bm_status_t ret = BM_SUCCESS;
    struct timeval t1, t2;
    gettimeofday_(&t1);
    ret = bmcv_image_copy_to(handle, copy_to_attr, input, output);
    gettimeofday_(&t2);
    std::cout << "copy to using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << std::endl;


    if (BM_SUCCESS != ret) {
        std::cout << "bmcv_copy_to error !!!" << std::endl;
        bm_image_destroy(input);
        bm_image_destroy(output);
        exit(-1);
    }
    bm_image_copy_device_to_host(output, (void **)&res_data);
    // calculate ref
    if (if_4n) {
        convert_4N_2_1N(
            (unsigned char *)src_ptr.get(), image_n, channel, in_h, in_w);
        convert_4N_2_1N(
            (unsigned char *)res_ptr.get(), image_n, channel, out_h, out_w);
    }
    for (int i = 0; i < image_n; i++) {
        copy_to_ref_v2<T>( src_ptr.get() + i * channel * in_w * in_h,
                           ref_ptr.get() + i * channel * out_w * out_h,
                           channel,
                           in_h,
                           in_w,
                           out_h,
                           out_w,
                           start_x,
                           start_y,
                           if_packed);
    }
    if (if_packed) {
        packedToplanar<T>(res_ptr.get(), channel, out_w, out_h);
    }
    std::shared_ptr<T> res_no_padding_ptr(
        new T[image_n * channel * in_w * in_h], std::default_delete<T[]>());
    if (!copy_to_attr.if_padding) {
        if (false == res_ref_comp<T>(res_ptr.get(),
                                     ref_ptr.get(),
                                     image_n,
                                     channel,
                                     in_h,
                                     in_w,
                                     out_h,
                                     out_w,
                                     start_x,
                                     start_y)) {
            std::cout << "copy_to test error!!!" << std::endl;
            bm_image_destroy(input);
            bm_image_destroy(output);
            exit(-1);
        }
    } else {
        if (false == res_ref_comp<T>(res_ptr.get(),
                                     ref_ptr.get(),
                                     image_n * channel * out_h * out_w)) {
            std::cout << "copy_to test error!!!" << std::endl;
            bm_image_destroy(input);
            bm_image_destroy(output);
            exit(-1);
        }
    }
    bm_image_destroy(input);
    bm_image_destroy(output);
    std::cout << "copy_to test compare passed" << std::endl;
    return 0;
}

#ifdef __linux__
void *test_copy_to_thread(void *arg) {
#else
DWORD WINAPI test_copy_to_thread(LPVOID arg) {
#endif
    copy_to_thread_arg_t *copy_to_thread_arg = (copy_to_thread_arg_t *)arg;
    int                   test_loop_times    = copy_to_thread_arg->trials;
    #ifdef __linux__
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << pthread_self() << std::endl;
    #else
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << GetCurrentThreadId() << std::endl;
    #endif
    std::cout << "[TEST COPY_TO] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    int         dev_id = 0;
    bm_handle_t handle;
    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;
    bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
    if (dev_ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", dev_ret);
        exit(-1);
    }
    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
        printf("bm_get_chipid failed. ret = %d\n", ret);
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST COPY_TO] LOOP " << loop_idx << "------"
                  << std::endl;
        unsigned int seed = (unsigned)time(NULL);
        // seed = 1571392508;
        std::cout << "seed: " << seed << std::endl;
        srand(seed);
        int in_w    = 400;
        int in_h    = 400;
        int out_w   = 800;
        int out_h   = 800;
        int start_x = 200;
        int start_y = 200;
        std::cout << "---------COPY_TO CLASSICAL SIZE TEST----------"
                  << std::endl;
        std::cout << "[COPY_TO TEST] fp32 bgr packed: "
                    "1n_fp32->1n_fp32 starts"
                    << std::endl;
        copy_to_test_rand<float>(handle,
                                FORMAT_BGR_PACKED,
                                DATA_TYPE_EXT_FLOAT32,
                                in_h,
                                in_w,
                                out_h,
                                out_w,
                                start_x,
                                start_y);
        std::cout << "[COPY_TO TEST] fp32 bgr planner: "
                    "1n_fp32->1n_fp32 starts"
                    << std::endl;
        copy_to_test_rand<float>(handle,
                                FORMAT_BGR_PLANAR,
                                DATA_TYPE_EXT_FLOAT32,
                                in_h,
                                in_w,
                                out_h,
                                out_w,
                                start_x,
                                start_y);
        std::cout << "[COPY_TO TEST] int8 bgr packed: "
                    "1n_int8->1n_int8 starts"
                    << std::endl;
        copy_to_test_rand<int8_t>(handle,
                                FORMAT_BGR_PACKED,
                                DATA_TYPE_EXT_1N_BYTE_SIGNED,
                                in_h,
                                in_w,
                                out_h,
                                out_w,
                                start_x,
                                start_y);
        std::cout << "[COPY_TO TEST] int8 bgr planner: "
                    "1n_int8->1n_int8 starts"
                    << std::endl;
        copy_to_test_rand<int8_t>(handle,
                                FORMAT_BGR_PLANAR,
                                DATA_TYPE_EXT_1N_BYTE_SIGNED,
                                in_h,
                                in_w,
                                out_h,
                                out_w,
                                start_x,
                                start_y);
        if(chipid == 0x1684){
            std::cout << "[COPY_TO TEST] int8 bgr planner: "
                        "4n_int8->4n_int8 starts"
                        << std::endl;
            copy_to_test_rand<uint8_t>(handle,
                                        FORMAT_BGR_PLANAR,
                                        DATA_TYPE_EXT_4N_BYTE,
                                        in_h,
                                        in_w,
                                        out_h,
                                        out_w,
                                        start_x,
                                        start_y);
        }
        std::cout << "[COPY_TO TEST] uint8 bgr packed: "
                    "1n_uint8->1n_uint8 starts"
                << std::endl;
        copy_to_test_rand<uint8_t>(handle,
                                FORMAT_BGR_PACKED,
                                DATA_TYPE_EXT_1N_BYTE,
                                in_h,
                                in_w,
                                out_h,
                                out_w,
                                start_x,
                                start_y);
        std::cout << "[COPY_TO TEST] uint8 bgr planner: "
                    "1n_uint8->1n_uint8 starts"
                << std::endl;
        copy_to_test_rand<uint8_t>(handle,
                                FORMAT_BGR_PLANAR,
                                DATA_TYPE_EXT_1N_BYTE,
                                in_h,
                                in_w,
                                out_h,
                                out_w,
                                start_x,
                                start_y);
        std::cout << "[COPY_TO TEST] int8 gray planner: "
                    "1n_int8->1n_int8 starts"
                << std::endl;
        copy_to_test_rand<int8_t>(handle,
                                FORMAT_GRAY,
                                DATA_TYPE_EXT_1N_BYTE_SIGNED,
                                in_h,
                                in_w,
                                out_h,
                                out_w,
                                start_x,
                                start_y);
        std::cout << "[COPY_TO TEST] uint8 gray planner: "
                    "1n_uint8->1n_uint8 starts"
                << std::endl;
        copy_to_test_rand<uint8_t>(handle,
                                FORMAT_GRAY,
                                DATA_TYPE_EXT_1N_BYTE,
                                in_h,
                                in_w,
                                out_h,
                                out_w,
                                start_x,
                                start_y);
        std::cout << "----------COPY_TO CORNER TEST---------" << std::endl;
        int loop_num = 2;
        for (int loop_idx = 0; loop_idx < loop_num; loop_idx++) {
            for (int rand_mode = 0; rand_mode < MAX_RAND_MODE; rand_mode++) {
                gen_test_size(
                    in_w, in_h, out_w, out_h, start_x, start_y, rand_mode);
                std::cout << "rand mode : " << rand_mode << " ,in_w: " << in_w
                          << " ,in_h: " << in_h << " ,out_w: " << out_w
                          << ", out_h: " << out_h << ", start_x: " << start_x
                          << ", start_y: " << start_y << std::endl;
                std::cout << "[COPY_TO TEST] fp32 bgr packed: "
                            "1n_fp32->1n_fp32 starts"
                        << std::endl;
                copy_to_test_rand<float>(handle,
                                        FORMAT_BGR_PACKED,
                                        DATA_TYPE_EXT_FLOAT32,
                                        in_h,
                                        in_w,
                                        out_h,
                                        out_w,
                                        start_x,
                                        start_y);
                std::cout << "[COPY_TO TEST] fp32 bgr planner: "
                            "1n_fp32->1n_fp32 starts"
                        << std::endl;
                copy_to_test_rand<float>(handle,
                                        FORMAT_BGR_PLANAR,
                                        DATA_TYPE_EXT_FLOAT32,
                                        in_h,
                                        in_w,
                                        out_h,
                                        out_w,
                                        start_x,
                                        start_y);
                std::cout << "[COPY_TO TEST] int8 bgr packed: "
                            "1n_int8->1n_int8 starts"
                        << std::endl;
                copy_to_test_rand<int8_t>(handle,
                                        FORMAT_BGR_PACKED,
                                        DATA_TYPE_EXT_1N_BYTE_SIGNED,
                                        in_h,
                                        in_w,
                                        out_h,
                                        out_w,
                                        start_x,
                                        start_y);
                std::cout << "[COPY_TO TEST] int8 bgr planner: "
                            "1n_int8->1n_int8 starts"
                        << std::endl;
                copy_to_test_rand<int8_t>(handle,
                                        FORMAT_BGR_PLANAR,
                                        DATA_TYPE_EXT_1N_BYTE_SIGNED,
                                        in_h,
                                        in_w,
                                        out_h,
                                        out_w,
                                        start_x,
                                        start_y);
            if(chipid == 0x1684){
                std::cout << "[COPY_TO TEST] uint8 bgr planner: "
                            "4n_uint8->4n_uint8 starts"
                        << std::endl;
                copy_to_test_rand<uint8_t>(handle,
                                            FORMAT_BGR_PLANAR,
                                            DATA_TYPE_EXT_4N_BYTE,
                                            in_h,
                                            in_w,
                                            out_h,
                                            out_w,
                                            start_x,
                                            start_y);
            }
                std::cout << "[COPY_TO TEST] uint8 bgr packed: "
                            "1n_uint8->1n_uint8 starts"
                        << std::endl;
                copy_to_test_rand<uint8_t>(handle,
                                        FORMAT_BGR_PACKED,
                                        DATA_TYPE_EXT_1N_BYTE,
                                        in_h,
                                        in_w,
                                        out_h,
                                        out_w,
                                        start_x,
                                        start_y);
                std::cout << "[COPY_TO TEST] uint8 bgr planner: "
                            "1n_uint8->1n_uint8 starts"
                        << std::endl;
                copy_to_test_rand<uint8_t>(handle,
                                        FORMAT_BGR_PLANAR,
                                        DATA_TYPE_EXT_1N_BYTE,
                                        in_h,
                                        in_w,
                                        out_h,
                                        out_w,
                                        start_x,
                                        start_y);
                std::cout << "[COPY_TO TEST] uint8 gray planner: "
                            "1n_uint8->1n_uint8 starts"
                        << std::endl;
                copy_to_test_rand<uint8_t>(handle,
                                        FORMAT_GRAY,
                                        DATA_TYPE_EXT_1N_BYTE,
                                        in_h,
                                        in_w,
                                        out_h,
                                        out_w,
                                        start_x,
                                        start_y);
                std::cout << "[COPY_TO TEST] int8 gray planner: "
                            "1n_int8->1n_int8 starts"
                        << std::endl;
                copy_to_test_rand<int8_t>(handle,
                                        FORMAT_GRAY,
                                        DATA_TYPE_EXT_1N_BYTE_SIGNED,
                                        in_h,
                                        in_w,
                                        out_h,
                                        out_w,
                                        start_x,
                                        start_y);
            }
        }
        std::cout << "----------COPY_TO TEST OVER---------" << std::endl;
    }
    bm_dev_free(handle);
    std::cout << "------[TEST COPY_TO] ALL TEST PASSED!" << std::endl;
    return NULL;
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
                     "order:test_copy_to loop_num multi_thread_num"
                  << std::endl;
        exit(-1);
    }
    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST COPY_TO] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    if (test_threads_num > 4 || test_threads_num < 1) {
        std::cout << "[TEST COPY_TO] thread nums should be 1~4 " << std::endl;
        exit(-1);
    }
    #ifdef __linux__
    pthread_t *           pid = new pthread_t[test_threads_num];
    copy_to_thread_arg_t *copy_to_thread_arg =
        new copy_to_thread_arg_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        copy_to_thread_arg[i].trials = test_loop_times;  // dev_id 0
        if (pthread_create(
                &pid[i], NULL, test_copy_to_thread, &copy_to_thread_arg[i])) {
            delete[] pid;
            delete[] copy_to_thread_arg;
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int ret = 0;
    for (int i = 0; i < test_threads_num; i++) {
        ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            delete[] pid;
            delete[] copy_to_thread_arg;
            perror("Thread join failed");
            exit(-1);
        }
    }
    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    delete[] pid;
    delete[] copy_to_thread_arg;
    #else
#define THREAD_NUM 64
    DWORD              dwThreadIdArray[THREAD_NUM];
    HANDLE             hThreadArray[THREAD_NUM];
    copy_to_thread_arg_t *copy_to_thread_arg =
        new copy_to_thread_arg_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        copy_to_thread_arg[i].trials = test_loop_times;
        hThreadArray[i] = CreateThread(
            NULL,                  // default security attributes
            0,                     // use default stack size
            test_copy_to_thread,   // thread function name
            &copy_to_thread_arg[i],   // argument to thread function
            0,                     // use default creation flags
            &dwThreadIdArray[i]);  // returns the thread identifier
        if (hThreadArray[i] == NULL) {
            delete[] copy_to_thread_arg;
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
        delete[] copy_to_thread_arg;
        printf("Thread join failed\n");
        return -1;
    }
    for (int i = 0; i < test_threads_num; i++)
        CloseHandle(hThreadArray[i]);
    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    delete[] copy_to_thread_arg;
    #endif
    return 0;
}