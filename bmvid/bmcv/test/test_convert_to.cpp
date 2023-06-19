#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <type_traits>
#include <cstring>
#include "test_misc.h"
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "bmlib_runtime.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

using namespace std;

typedef struct {
    int trials;
    bm_handle_t handle;
} convert_to_thread_arg_t;

typedef struct {
    float alpha;
    float beta;
} convert_to_arg_t;

int test_seed = -1;

typedef struct {
    int n;
    int c;
    int w;
    int h;
} image_shape_t;

typedef enum {
    UINT8_C1 = 0,
    UINT8_C3,
    INT8_C1,
    INT8_C3,
    FLOAT32_C1,
    FLOAT32_C3,
    MAX_COLOR_TYPE
} cv_color_e;

struct bm_image_tensor {
    bm_image image;
    int      image_c;
    int      image_n;
};

#define BM1684_MAX_W (2048)
#define BM1684_MAX_H (2048)
#define BM1684X_MAX_W (4096)
#define BM1684X_MAX_H (4096)
#define MIN_W (16)
#define MIN_H (16)

#define __ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))

#ifdef __linux__
#define ALIGN(x, a) __ALIGN_MASK(x, (__typeof__(x))(a)-1)
#else
#define ALIGN(x, a) __ALIGN_MASK(x, (int)(a)-1)
#endif


typedef enum { MIN_TEST = 0, MAX_TEST, RAND_TEST, MAX_RAND_MODE } rand_mode_e;

static int gen_test_size(
    int chipid, int *image_w, int *image_h, int *image_n, int *image_c, int gen_mode) {
    int max_w = (chipid == 0x1686) ? BM1684X_MAX_W : BM1684_MAX_W;
    int max_h = (chipid == 0x1686) ? BM1684X_MAX_H : BM1684_MAX_H;
    switch (gen_mode) {
        case (MIN_TEST): {
            *image_w = MIN_W;
            *image_h = MIN_H;
            *image_n = rand() % 4 + 1;
            *image_c = 3;
            break;
        }
        case (MAX_TEST): {
            *image_w = max_w;
            *image_h = max_h;
            *image_n = rand() % 4 + 1;
            *image_c = 3;
            break;
        }
        case (RAND_TEST): {
            *image_w = rand() % max_w;
            *image_h = rand() % max_h;
            *image_w = (*image_w < MIN_W) ? (MIN_W) : (*image_w);
            *image_h = (*image_h < MIN_H) ? (MIN_H) : (*image_h);
            *image_n = rand() % 4 + 1;
            *image_c = 3;
            break;
        }
        default: {
            cout << "gen mode error" << endl;
            exit(-1);
        }
    }

    return 0;
}

static bm_image_data_format_ext
convert_to_type_translation(int cv_color, int data_type, int if_input) {
    bm_image_data_format_ext image_format = DATA_TYPE_EXT_1N_BYTE;
    if (((UINT8_C3 == cv_color) || (UINT8_C1 == cv_color)) &&
        (CONVERT_4N_TO_1N == data_type)) {
        image_format =
            (if_input) ? (DATA_TYPE_EXT_4N_BYTE) : (DATA_TYPE_EXT_1N_BYTE);
    } else if (((UINT8_C3 == cv_color) || (UINT8_C1 == cv_color)) &&
               (CONVERT_1N_TO_1N == data_type)) {
        image_format = DATA_TYPE_EXT_1N_BYTE;
    } else if (((UINT8_C3 == cv_color) || (UINT8_C1 == cv_color)) &&
               (CONVERT_4N_TO_4N == data_type)) {
        image_format = DATA_TYPE_EXT_4N_BYTE;
    } else if ((FLOAT32_C1 == cv_color) || (FLOAT32_C3 == cv_color)) {
        image_format = DATA_TYPE_EXT_FLOAT32;
    } else if (((INT8_C3 == cv_color) || (INT8_C1 == cv_color)) &&
               (CONVERT_4N_TO_1N == data_type)) {
        image_format = (if_input) ? (DATA_TYPE_EXT_4N_BYTE_SIGNED)
                                  : (DATA_TYPE_EXT_1N_BYTE_SIGNED);
    } else if (((INT8_C3 == cv_color) || (INT8_C1 == cv_color)) &&
               (CONVERT_1N_TO_1N == data_type)) {
        image_format = DATA_TYPE_EXT_1N_BYTE_SIGNED;
    } else if (((INT8_C3 == cv_color) || (INT8_C1 == cv_color)) &&
               (CONVERT_4N_TO_4N == data_type)) {
        image_format = DATA_TYPE_EXT_4N_BYTE_SIGNED;
    }

    return image_format;
}

template <typename T>
int32_t bmcv_convert_to_cmp(T * p_exp,
                            T * p_got,
                            int image_num,
                            int image_channel,
                            int image_h,
                            int image_w) {
    for (int32_t n_idx = 0; n_idx < image_num; n_idx++) {
        for (int32_t c_idx = 0; c_idx < image_channel; c_idx++) {
            for (int32_t y = 0; y < image_h; y++) {
                for (int32_t x = 0; x < image_w; x++) {
                    int check_idx =
                        (n_idx * image_channel + c_idx) * image_w * image_h +
                        y * image_w + x;
                    T got = p_got[check_idx];
                    T exp = p_exp[check_idx];
                    if (abs(got - exp) > 1) {
                        std::cout << "n: " << n_idx << " ,c:" << c_idx << ",h "
                                  << y << " w " << x
                                  << ", got: " << (int32_t)got << "exp "
                                  << (int32_t)exp << std::endl;

                        return -1;
                    }
                }
            }
        }
    }
    return 0;
}

static void convert_4N_2_1N(unsigned char *inout, int N, int C, int H, int W) {
    unsigned char *temp_buf = new unsigned char[4 * C * H * W];
    for (int i = 0; i < (N + 3) / 4; i++) {
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

static void convert_4N_2_1N_with_stride(
    unsigned char *inout, int N, int C, int H, int W, int W_stride) {
    int            img_4n_num = (N + 3) / 4;
    unsigned char *temp_buf = new unsigned char[img_4n_num * C * H * W_stride];
    memcpy(temp_buf, inout, img_4n_num * C * H * W_stride);
    for (int32_t n_idx = 0; n_idx < img_4n_num; n_idx++) {
        for (int32_t c_idx = 0; c_idx < C; c_idx++) {
            for (int32_t y = 0; y < H; y++) {
                for (int32_t x = 0; x < W; x++) {
                    inout[n_idx * 4 * W * H * C + c_idx * H * W + y * W + x] =
                        temp_buf[4 * x + y * W_stride + c_idx * H * W_stride +
                                 n_idx * C * H * W_stride];
                    inout[n_idx * 4 * W * H * C + W * H * C + c_idx * H * W +
                          y * W + x] = temp_buf[1 + 4 * x + y * W_stride +
                                                c_idx * H * W_stride +
                                                n_idx * C * H * W_stride];
                    inout[n_idx * 4 * W * H * C + 2 * W * H * C +
                          c_idx * H * W + y * W + x] =
                        temp_buf[2 + 4 * x + y * W_stride +
                                 c_idx * H * W_stride +
                                 n_idx * C * H * W_stride];
                    inout[n_idx * 4 * W * H * C + 3 * W * H * C +
                          c_idx * H * W + y * W + x] =
                        temp_buf[3 + 4 * x + y * W_stride +
                                 c_idx * H * W_stride +
                                 n_idx * C * H * W_stride];
                }
            }
        }
    }
    delete[] temp_buf;
}

template <typename T0, typename T1>
int32_t convert_to_ref(T0 *              src,
                       T1 *              dst,
                       convert_to_arg_t *convert_to_arg,
                       int               image_num,
                       int               image_channel,
                       int               image_h,
                       int               image_w_stride,
                       int               image_w,
                       int               convert_format) {
    int image_len = 0;
    if ((CONVERT_4N_TO_4N == convert_format) ||
        (CONVERT_4N_TO_1N == convert_format)) {
        image_len =
            ((image_num + 3) / 4) * image_channel * image_w_stride * image_h;
    } else {
        image_len = image_num * image_channel * image_w_stride * image_h;
    }
    T0 *temp_old_buf = new T0[image_len];
    memcpy(temp_old_buf, src, sizeof(T0) * image_len);
    if (CONVERT_4N_TO_1N == convert_format) {
        if ((image_w * 4 != image_w_stride)) {
            convert_4N_2_1N_with_stride((uint8_t *)temp_old_buf,
                                        image_num,
                                        image_channel,
                                        image_h,
                                        image_w,
                                        image_w_stride);
        } else {
            convert_4N_2_1N((uint8_t *)temp_old_buf,
                            image_num,
                            image_channel,
                            image_h,
                            image_w);
        }
        image_w_stride = image_w;
    }
    if (CONVERT_4N_TO_4N == convert_format) {
        image_num = (image_num + 3) / 4;
        image_w   = image_w * 4;
        // image_w_stride = image_w_stride * 4;
    }
    for (int32_t n_idx = 0; n_idx < image_num; n_idx++) {
        for (int32_t c_idx = 0; c_idx < image_channel; c_idx++) {
            for (int32_t y = 0; y < image_h; y++) {
                for (int32_t x = 0; x < image_w; x++) {
                    int check_idx =
                        (n_idx * image_channel + c_idx) * image_w * image_h +
                        y * image_w + x;
                    int src_check_idx = (n_idx * image_channel + c_idx) *
                                            image_w_stride * image_h +
                                        y * image_w_stride + x;
                    float temp = 0.0;
                    temp       = ((float)temp_old_buf[src_check_idx]) *
                               convert_to_arg[c_idx].alpha +
                           convert_to_arg[c_idx].beta;
                    if (std::is_same<T1, int8_t>::value) {
                        temp = (temp > 127) ? (127)
                                            : ((temp < -128) ? (-128) : (temp));
                    } else if (std::is_same<T1, uint8_t>::value) {
                        temp =
                            (temp > 255) ? (255) : ((temp < 0) ? (0) : (temp));
                    }
                    dst[check_idx] = (T1)round(temp);
                }
            }
        }
    }
    delete[] temp_old_buf;

    return 0;
}

template <typename SRC_T, typename DST_T>
static int32_t convert_to_test_rand(bm_handle_t       handle,
                                    int               input_data_type,
                                    int               output_data_type,
                                    image_shape_t     image_shape,
                                    convert_to_arg_t *convert_to_arg,
                                    int               convert_format,
                                    int               if_set_stride = 0) {
    if ((CONVERT_4N_TO_4N == convert_format) ||
        (CONVERT_4N_TO_1N == convert_format)) {
        image_shape.w = ALIGN(image_shape.w, 4) / 4;
        image_shape.n = ALIGN(image_shape.n, 4);
    }
    int image_num      = image_shape.n;
    int image_channel  = image_shape.c;
    int image_w        = image_shape.w;
    int image_w_stride = 0;
    int image_h        = image_shape.h;
    int image_len      = 0;
    if (if_set_stride) {
        if ((CONVERT_4N_TO_4N == convert_format) ||
            (CONVERT_4N_TO_1N == convert_format)) {
            image_w_stride = ALIGN(image_w * 4, 64);
        } else {
            image_w_stride = ALIGN(image_w, 64);
        }
        image_len = image_num * image_channel * image_w_stride * image_h;
    } else {
        if ((CONVERT_4N_TO_4N == convert_format) ||
            (CONVERT_4N_TO_1N == convert_format)) {
            image_w_stride = image_w * 4;
        } else {
            image_w_stride = image_w;
        }
        image_len = image_num * image_channel * image_w * image_h;
    }
    SRC_T *input    = new SRC_T[image_len];
    DST_T *bmcv_res = new DST_T[image_len];
    DST_T *ref_res  = new DST_T[image_len];
    int    ret      = -1;

    memset(input, 0x0, image_len);
    memset(bmcv_res, 0x0, image_len);
    memset(ref_res, 0x0, image_len);
    if (if_set_stride) {
        if ((CONVERT_4N_TO_4N == convert_format) ||
            (CONVERT_4N_TO_1N == convert_format)) {
            for (int n_idx = 0; n_idx < (image_num / 4); n_idx++) {
                for (int c_idx = 0; c_idx < image_channel; c_idx++) {
                    for (int h_idx = 0; h_idx < image_h; h_idx++) {
                        for (int w_idx = 0; w_idx < image_w * 4; w_idx++) {
                            input[w_idx + h_idx * image_w_stride +
                                  c_idx * image_h * image_w_stride +
                                  n_idx * image_channel * image_h *
                                      image_w_stride] = rand() % 255;
                        }
                        SRC_T *tmp =
                            &input[image_w * 4 + h_idx * image_w_stride +
                                   c_idx * image_h * image_w_stride +
                                   n_idx * image_channel * image_h *
                                       image_w_stride];
                        memset(tmp,
                               0x0,
                               sizeof(SRC_T) *
                                   (image_w_stride - (image_w * 4)));
                    }
                }
            }
        } else {
            for (int n_idx = 0; n_idx < image_num; n_idx++) {
                for (int c_idx = 0; c_idx < image_channel; c_idx++) {
                    for (int h_idx = 0; h_idx < image_h; h_idx++) {
                        for (int w_idx = 0; w_idx < image_w; w_idx++) {

                            input[w_idx + h_idx * image_w_stride +
                                  c_idx * image_h * image_w_stride +
                                  n_idx * image_channel * image_h *
                                      image_w_stride] = rand() % 255;
                        }
                        SRC_T *tmp = &input[image_w + h_idx * image_w_stride +
                                            c_idx * image_h * image_w_stride +
                                            n_idx * image_channel * image_h *
                                                image_w_stride];
                        memset(tmp,
                               0x0,
                               sizeof(SRC_T) * (image_w_stride - image_w));
                    }
                }
            }
        }
    } else {
        for (int i = 0; i < image_len; i++) {
            input[i] = rand() % 255;
        }
    }

    bm_image_tensor          input_img_tensor, output_img_tensor;
    bm_image_data_format_ext input_data_format_ext, output_data_format_ext;
    bmcv_convert_to_attr     convert_to_attr;
    convert_to_attr.alpha_0 = convert_to_arg[0].alpha;
    convert_to_attr.beta_0  = convert_to_arg[0].beta;
    convert_to_attr.alpha_1 = convert_to_arg[1].alpha;
    convert_to_attr.beta_1  = convert_to_arg[1].beta;
    convert_to_attr.alpha_2 = convert_to_arg[2].alpha;
    convert_to_attr.beta_2  = convert_to_arg[2].beta;
    input_data_format_ext =
        convert_to_type_translation(input_data_type, convert_format, 1);
    output_data_format_ext =
        convert_to_type_translation(output_data_type, convert_format, 0);

    #ifdef __linux__
    bm_image input_images[image_num];
    #else
    std::shared_ptr<bm_image> input_images_(new bm_image[image_num], std::default_delete<bm_image[]>());
    bm_image*                 input_images = input_images_.get();
    #endif
    bm_image output_images[32];
    int      input_num = ((input_data_format_ext == DATA_TYPE_EXT_4N_BYTE) ||
                     (input_data_format_ext == DATA_TYPE_EXT_4N_BYTE_SIGNED))
                        ? (image_num / 4)
                        : (image_num);
    int output_num = ((output_data_format_ext == DATA_TYPE_EXT_4N_BYTE) ||
                      (output_data_format_ext == DATA_TYPE_EXT_4N_BYTE_SIGNED))
                         ? (image_num / 4)
                         : (image_num);
    if (if_set_stride) {
        for (int img_idx = 0; img_idx < input_num; img_idx++) {
            int set_w_stride = image_w_stride * sizeof(SRC_T);
            set_w_stride =
                ((input_data_format_ext == DATA_TYPE_EXT_4N_BYTE) ||
                 (input_data_format_ext == DATA_TYPE_EXT_4N_BYTE_SIGNED))
                    ? (set_w_stride * 4)
                    : (set_w_stride);
            bm_image_create(handle,
                            image_h,
                            image_w,
                            FORMAT_BGR_PLANAR,
                            input_data_format_ext,
                            &input_images[img_idx],
                            &set_w_stride);
        }
    } else {
        for (int img_idx = 0; img_idx < input_num; img_idx++) {
            bm_image_create(handle,
                            image_h,
                            image_w,
                            FORMAT_BGR_PLANAR,
                            input_data_format_ext,
                            &input_images[img_idx]);
        }
    }
    int is_contiguous = rand() % 2;
    if (is_contiguous) {
        bm_image_alloc_contiguous_mem(input_num, input_images, BMCV_IMAGE_FOR_IN);
    } else {
        for(int i = input_num - 1; i >= 0; i--) {
            bm_image_alloc_dev_mem(input_images[i], BMCV_IMAGE_FOR_IN);
        }
    }
    for (int img_idx = 0; img_idx < input_num; img_idx++) {
        int    img_offset     = image_w_stride * image_h * image_channel;
        SRC_T *input_img_data = input + img_offset * img_idx;
        bm_image_copy_host_to_device(input_images[img_idx],
                                     (void **)&input_img_data);
    }
    for (int img_idx = 0; img_idx < output_num; img_idx++) {
        bm_image_create(handle,
                        image_h,
                        image_w,
                        FORMAT_BGR_PLANAR,
                        output_data_format_ext,
                        &output_images[img_idx]);
    }
    bm_image_alloc_contiguous_mem(
        output_num, output_images, BMCV_IMAGE_FOR_OUT);
    #ifdef __linux__
        struct timeval t1, t2;
        gettimeofday(&t1, NULL);
        bmcv_image_convert_to(handle, input_num, convert_to_attr, input_images, output_images);
        gettimeofday(&t2, NULL);
        cout << "Convert to  using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;
    #else
        struct timespec tp1, tp2;
        clock_gettime_win(0, &tp1);
        bmcv_image_convert_to(handle, input_num, convert_to_attr, input_images, output_images);
        clock_gettime_win(0, &tp2);
        cout << "Convert to using time: " << ((tp2.tv_sec - tp1.tv_sec) * 1000000 + (tp2.tv_nsec - tp1.tv_nsec)/1000) << "us" << endl;
    #endif

    for (int img_idx = 0; img_idx < output_num; img_idx++) {
        int    img_offset   = image_w * image_h * image_channel;
        DST_T *res_img_data = bmcv_res + img_offset * img_idx;
        bm_image_copy_device_to_host(output_images[img_idx],
                                     (void **)&res_img_data);
    }
    convert_to_ref<SRC_T, DST_T>(input,
                                 ref_res,
                                 convert_to_arg,
                                 image_num,
                                 image_channel,
                                 image_h,
                                 image_w_stride,
                                 image_w,
                                 convert_format);
    ret = bmcv_convert_to_cmp<DST_T>(
        ref_res, bmcv_res, image_num, image_channel, image_h, image_w);
    if (ret < 0) {
        // printf("compare failed !\r\n");
        std::cout << "compare failed !" << std::endl;
        delete[] input;
        delete[] bmcv_res;
        delete[] ref_res;
        bm_image_free_contiguous_mem(input_num, input_images);
        bm_image_free_contiguous_mem(output_num, output_images);
        for (int i = 0; i < input_num; i++) {
            bm_image_destroy(input_images[i]);
        }
        for (int i = 0; i < output_num; i++) {
            bm_image_destroy(output_images[i]);
        }
        exit(-1);
    }
    delete[] input;
    delete[] bmcv_res;
    delete[] ref_res;
    if (is_contiguous)
        bm_image_free_contiguous_mem(input_num, input_images);
    bm_image_free_contiguous_mem(output_num, output_images);
    for (int i = 0; i < input_num; i++) {
        bm_image_destroy(input_images[i]);
    }
    for (int i = 0; i < output_num; i++) {
        bm_image_destroy(output_images[i]);
    }

    return 0;
}

#ifdef __linux__
void *test_convert_to_thread(void *arg) {
#else
DWORD WINAPI test_convert_to_thread(LPVOID arg) {
#endif
    convert_to_thread_arg_t *resize_thread_arg = (convert_to_thread_arg_t *)arg;
    bm_handle_t handle = resize_thread_arg->handle;
    int test_loop_times = resize_thread_arg->trials;
    #ifdef __linux__
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << pthread_self() << std::endl;
    #else
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << GetCurrentThreadId() << std::endl;
    #endif
    std::cout << "[TEST CONVERT TO] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST CONVERT TO] LOOP " << loop_idx << "------"
                  << std::endl;
        int              image_n = 4;
        int              image_c = 3;
        int              image_w = 1920;
        int              image_h = 1080;
        image_shape_t    image_shape;
        #ifdef __linux__
        convert_to_arg_t convert_to_arg[image_c], same_convert_to_arg[image_c];
        #else
        std::shared_ptr<convert_to_arg_t> convert_to_arg_(new convert_to_arg_t[image_c], std::default_delete<convert_to_arg_t[]>());
        convert_to_arg_t*                 convert_to_arg = convert_to_arg_.get();
        std::shared_ptr<convert_to_arg_t> same_convert_to_arg_(new convert_to_arg_t[image_c], std::default_delete<convert_to_arg_t[]>());
        convert_to_arg_t*                 same_convert_to_arg = same_convert_to_arg_.get();
        #endif
        int              convert_format = CONVERT_1N_TO_1N;
        int              test_cnt = 0;

        int             dev_id = 0;
        struct timespec tp;
        #ifdef __linux__
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
        #else
        clock_gettime_win(0, &tp);
        #endif
        unsigned int seed;
        if (test_seed == -1) {
           seed = tp.tv_nsec;
        } else {
           seed = test_seed;
        }
        cout << "random seed " << seed << endl;
        srand(seed);
        bm_status_t ret = bm_dev_request(&handle, dev_id);
        if (ret != BM_SUCCESS) {
            std::cout << "Create bm handle failed. ret = " << ret << std::endl;
            exit(-1);
        }
        image_shape.n = image_n;
        image_shape.c = image_c;
        image_shape.h = image_h;
        image_shape.w = image_w;
        memset(convert_to_arg, 0x0, sizeof(convert_to_arg));
        memset(same_convert_to_arg, 0x0, sizeof(same_convert_to_arg));
        float same_alpha =((float)(rand() % 20)) / (float)10;
        float same_beta =((float)(rand() % 20)) / (float)10 -1;
        for (int i = 0; i < image_c; i++) {
            convert_to_arg[i].alpha = ((float)(rand() % 20)) / (float)10;
            convert_to_arg[i].beta  = ((float)(rand() % 20)) / (float)10 - 1;
            same_convert_to_arg[i].alpha = same_alpha;
            same_convert_to_arg[i].beta  = same_beta;
        }
        unsigned int chipid = 0x1686;
        bm_get_chipid(handle, &chipid);
        switch (chipid)
        {
        case 0x1684:{
            printf("=========================BM1684===================\n");
            cout << "---------CONVERT TO CLASSICAL SIZE TEST----------" << endl;
            convert_format = CONVERT_1N_TO_1N;
            cout << "CONVERT TO 1Nint8 TO 1N fp32 " << endl;
            convert_to_test_rand<uint8_t, float_t>(handle,
                                                UINT8_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format);
            cout << "result of " << test_cnt++ << " compare passed" << endl;
            cout << "CONVERT TO 1N uint8 TO 1N uint8" << endl;
            convert_to_test_rand<uint8_t, uint8_t>(handle,
                                                UINT8_C3,
                                                UINT8_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format);
            std::cout << "result of " << test_cnt++ << " compare passed"
                    << std::endl;
            cout << "CONVERT TO 1N int8 TO 1N int8" << endl;
            convert_to_test_rand<int8_t, int8_t>(handle,
                                                INT8_C3,
                                                INT8_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format);
            std::cout << "result of " << test_cnt++ << " compare passed"
                    << std::endl;
            cout << "CONVERT TO 1N uint8 TO 1N int8" << endl;
            convert_to_test_rand<uint8_t, int8_t>(handle,
                                                UINT8_C3,
                                                INT8_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            cout << "CONVERT TO 1N fp32 TO 1N fp32" << endl;
            convert_to_test_rand<float_t, float_t>(handle,
                                                FLOAT32_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            convert_format = CONVERT_4N_TO_1N;
            cout << "CONVERT TO 4N uint8 TO 1N fp32" << endl;
            convert_to_test_rand<uint8_t, float_t>(handle,
                                                UINT8_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            convert_format = CONVERT_4N_TO_4N;
            cout << "CONVERT TO 4N uint8 TO 4N uint8" << endl;
            convert_to_test_rand<uint8_t, uint8_t>(handle,
                                                UINT8_C3,
                                                UINT8_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            convert_format = CONVERT_1N_TO_1N;
            cout << "[WITH STRIDE] CONVERT TO 1Nint8 TO 1N fp32 " << endl;
            convert_to_test_rand<uint8_t, float_t>(handle,
                                                UINT8_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format,
                                                1);
            cout << "result of " << test_cnt++ << " compare passed" << endl;
            cout << "[WITH STRIDE] CONVERT TO 1N uint8 TO 1N uint8" << endl;
            convert_to_test_rand<uint8_t, uint8_t>(handle,
                                                UINT8_C3,
                                                UINT8_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format,
                                                1);
            std::cout << "result of " << test_cnt++ << " compare passed"
                    << std::endl;
            cout << "[WITH STRIDE] CONVERT TO 1N int8 TO 1N int8" << endl;
            convert_to_test_rand<int8_t, int8_t>(handle,
                                                INT8_C3,
                                                INT8_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format,
                                                1);
            std::cout << "result of " << test_cnt++ << " compare passed"
                    << std::endl;
            cout << "[WITH STRIDE] CONVERT TO 1N uint8 TO 1N int8" << endl;
            convert_to_test_rand<uint8_t, int8_t>(handle,
                                                UINT8_C3,
                                                INT8_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format,
                                                1);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            cout << "[WITH STRIDE] CONVERT TO 1N fp32 TO 1N fp32" << endl;
            convert_to_test_rand<float_t, float_t>(handle,
                                                FLOAT32_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format,
                                                1);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            convert_format = CONVERT_4N_TO_1N;
            cout << "[WITH STRIDE] CONVERT TO 4N uint8 TO 1N fp32" << endl;
            convert_to_test_rand<uint8_t, float_t>(handle,
                                                UINT8_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format,
                                                1);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            convert_format = CONVERT_4N_TO_4N;
            cout << "[WITH STRIDE] CONVERT TO 4N uint8 TO 4N uint8" << endl;
            convert_to_test_rand<uint8_t, uint8_t>(handle,
                                                UINT8_C3,
                                                UINT8_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format,
                                                1);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            convert_format = CONVERT_1N_TO_1N;
            cout << "[SAME PARA] CONVERT TO 1Nint8 TO 1N fp32 " << endl;
            convert_to_test_rand<uint8_t, float_t>(handle,
                                                UINT8_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format);
            cout << "result of " << test_cnt++ << " compare passed" << endl;
            cout << "[SAME PARA] CONVERT TO 1N uint8 TO 1N uint8" << endl;
            convert_to_test_rand<uint8_t, uint8_t>(handle,
                                                UINT8_C3,
                                                UINT8_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format);
            std::cout << "result of " << test_cnt++ << " compare passed"
                    << std::endl;
            cout << "[SAME PARA] CONVERT TO 1N int8 TO 1N int8" << endl;
            convert_to_test_rand<int8_t, int8_t>(handle,
                                                INT8_C3,
                                                INT8_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format);
            std::cout << "result of " << test_cnt++ << " compare passed"
                    << std::endl;
            cout << "[SAME PARA] CONVERT TO 1N uint8 TO 1N int8" << endl;
            convert_to_test_rand<uint8_t, int8_t>(handle,
                                                UINT8_C3,
                                                INT8_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            cout << "[SAME PARA] CONVERT TO 1N fp32 TO 1N fp32" << endl;
            convert_to_test_rand<float_t, float_t>(handle,
                                                FLOAT32_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            convert_format = CONVERT_4N_TO_1N;
            cout << "[SAME PARA] CONVERT TO 4N uint8 TO 1N fp32" << endl;
            convert_to_test_rand<uint8_t, float_t>(handle,
                                                UINT8_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            convert_format = CONVERT_4N_TO_4N;
            cout << "[SAME PARA] CONVERT TO 4N uint8 TO 4N uint8" << endl;
            convert_to_test_rand<uint8_t, uint8_t>(handle,
                                                UINT8_C3,
                                                UINT8_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            convert_format = CONVERT_1N_TO_1N;
            cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 1Nint8 TO 1N fp32 "
                << endl;
            convert_to_test_rand<uint8_t, float_t>(handle,
                                                UINT8_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format,
                                                1);
            cout << "result of " << test_cnt++ << " compare passed" << endl;
            cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 1N uint8 TO 1N uint8"
                << endl;
            convert_to_test_rand<uint8_t, uint8_t>(handle,
                                                UINT8_C3,
                                                UINT8_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format,
                                                1);
            std::cout << "result of " << test_cnt++ << " compare passed"
                    << std::endl;
            cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 1N int8 TO 1N int8"
                << endl;
            convert_to_test_rand<int8_t, int8_t>(handle,
                                                INT8_C3,
                                                INT8_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format,
                                                1);
            std::cout << "result of " << test_cnt++ << " compare passed"
                    << std::endl;
            cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 1N uint8 TO 1N int8"
                << endl;
            convert_to_test_rand<uint8_t, int8_t>(handle,
                                                UINT8_C3,
                                                INT8_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format,
                                                1);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 1N fp32 TO 1N fp32"
                << endl;
            convert_to_test_rand<float_t, float_t>(handle,
                                                FLOAT32_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format,
                                                1);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            convert_format = CONVERT_4N_TO_1N;
            cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 4N uint8 TO 1N fp32"
                << endl;
            convert_to_test_rand<uint8_t, float_t>(handle,
                                                UINT8_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format,
                                                1);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            convert_format = CONVERT_4N_TO_4N;
            cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 4N uint8 TO 4N uint8"
                << endl;
            convert_to_test_rand<uint8_t, uint8_t>(handle,
                                                UINT8_C3,
                                                UINT8_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format,
                                                1);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            cout << "---------CONVERT TO CORNER TEST----------" << endl;
            int rand_loop_num = 2;
            for (int rand_loop_idx = 0; rand_loop_idx < rand_loop_num;
                rand_loop_idx++) {
                for (int rand_mode = 0; rand_mode < MAX_RAND_MODE; rand_mode++) {
                    gen_test_size(chipid,
                                &image_shape.w,
                                &image_shape.h,
                                &image_shape.n,
                                &image_shape.c,
                                rand_mode);
                    std::cout << "rand mode : " << rand_mode
                            << " ,img_w: " << image_shape.w
                            << " ,img_h: " << image_shape.h
                            << " ,img_c: " << image_shape.c
                            << " ,img_n: " << image_shape.n << std::endl;
                    test_cnt       = 0;
                    convert_format = CONVERT_1N_TO_1N;
                    cout << "CONVERT TO 1N int8 TO 1N fp32" << endl;
                    convert_to_test_rand<uint8_t, float_t>(handle,
                                                        UINT8_C3,
                                                        FLOAT32_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format);
                    cout << "result of " << test_cnt++ << "compare passed" << endl;
                    cout << " CONVERT TO 1N int8 TO 1N int8" << endl;
                    convert_to_test_rand<uint8_t, uint8_t>(handle,
                                                        UINT8_C3,
                                                        UINT8_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format);
                    cout << "result of " << test_cnt++ << "compare passed  "
                        << endl;
                    cout << " CONVERT TO 1N fp32 TO 1N fp32 " << endl;
                    convert_to_test_rand<float_t, float_t>(handle,
                                                        FLOAT32_C3,
                                                        FLOAT32_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format);
                    cout << "result of " << test_cnt++ << "compare passed " << endl;
                    convert_format = CONVERT_4N_TO_1N;
                    cout << "CONVERT TO 4N uint8 TO 1N fp32" << endl;
                    convert_to_test_rand<uint8_t, float_t>(handle,
                                                        UINT8_C3,
                                                        FLOAT32_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format);
                    cout << "result of " << test_cnt++ << "compare passed " << endl;
                    convert_format = CONVERT_4N_TO_4N;
                    cout << "CONVERT TO 4N int8 TO 4N int8" << endl;
                    convert_to_test_rand<uint8_t, uint8_t>(handle,
                                                        UINT8_C3,
                                                        UINT8_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format);
                    cout << "result of " << test_cnt++ << "compare passed " << endl;
                    convert_format = CONVERT_1N_TO_1N;
                    cout << "[WITH STRIDE] CONVERT TO 1Nint8 TO 1N fp32" << endl;
                    convert_to_test_rand<uint8_t, float_t>(handle,
                                                        UINT8_C3,
                                                        FLOAT32_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format,
                                                        1);
                    cout << "result of " << test_cnt++ << " compare passed "
                        << endl;
                    cout << "[WITH STRIDE] CONVERT TO 1N uint8 TO 1N uint8 "
                        << endl;
                    convert_to_test_rand<uint8_t, uint8_t>(handle,
                                                        UINT8_C3,
                                                        UINT8_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format,
                                                        1);
                    std::cout << "result of " << test_cnt++ << " compare passed "
                            << std::endl;
                    cout << "[WITH STRIDE] CONVERT TO 1N int8 TO 1N int8 " << endl;
                    convert_to_test_rand<int8_t, int8_t>(handle,
                                                        INT8_C3,
                                                        INT8_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format,
                                                        1);
                    std::cout << "result of " << test_cnt++ << " compare passed"
                            << std::endl;
                    cout << "[WITH STRIDE] CONVERT TO 1N uint8 TO 1N int8 " << endl;
                    convert_to_test_rand<uint8_t, int8_t>(handle,
                                                        UINT8_C3,
                                                        INT8_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format,
                                                        1);
                    cout << "result of " << test_cnt++ << "compare passed " << endl;
                    cout << "[WITH STRIDE] CONVERT TO 1N fp32 TO 1N fp32 " << endl;
                    convert_to_test_rand<float_t, float_t>(handle,
                                                        FLOAT32_C3,
                                                        FLOAT32_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format,
                                                        1);
                    cout << "result of " << test_cnt++ << "compare passed " << endl;
                    convert_format = CONVERT_1N_TO_1N;
                    cout << "[SAME PARA] CONVERT TO 1N int8 TO 1N fp32" << endl;
                    convert_to_test_rand<uint8_t, float_t>(handle,
                                                        UINT8_C3,
                                                        FLOAT32_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format);
                    cout << "result of " << test_cnt++ << "compare passed" << endl;
                    cout << "[SAME PARA]  CONVERT TO 1N int8 TO 1N int8" << endl;
                    convert_to_test_rand<uint8_t, uint8_t>(handle,
                                                        UINT8_C3,
                                                        UINT8_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format);
                    cout << "result of " << test_cnt++ << "compare passed  "
                        << endl;
                    cout << "[SAME PARA]  CONVERT TO 1N fp32 TO 1N fp32 " << endl;
                    convert_to_test_rand<float_t, float_t>(handle,
                                                        FLOAT32_C3,
                                                        FLOAT32_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format);
                    cout << "result of " << test_cnt++ << "compare passed " << endl;
                    convert_format = CONVERT_4N_TO_1N;
                    cout << "[SAME PARA] CONVERT TO 4N uint8 TO 1N fp32" << endl;
                    convert_to_test_rand<uint8_t, float_t>(handle,
                                                        UINT8_C3,
                                                        FLOAT32_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format);
                    cout << "result of " << test_cnt++ << "compare passed " << endl;
                    convert_format = CONVERT_4N_TO_4N;
                    cout << "[SAME PARA] CONVERT TO 4N int8 TO 4N int8" << endl;
                    convert_to_test_rand<uint8_t, uint8_t>(handle,
                                                        UINT8_C3,
                                                        UINT8_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format);
                    cout << "result of " << test_cnt++ << "compare passed " << endl;
                    convert_format = CONVERT_1N_TO_1N;
                    cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 1Nint8 TO 1N fp32" << endl;
                    convert_to_test_rand<uint8_t, float_t>(handle,
                                                        UINT8_C3,
                                                        FLOAT32_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format,
                                                        1);
                    cout << "result of " << test_cnt++ << " compare passed "
                        << endl;
                    cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 1N uint8 TO 1N uint8 "
                        << endl;
                    convert_to_test_rand<uint8_t, uint8_t>(handle,
                                                        UINT8_C3,
                                                        UINT8_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format,
                                                        1);
                    std::cout << "result of " << test_cnt++ << " compare passed "
                            << std::endl;
                    cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 1N int8 TO 1N int8 " << endl;
                    convert_to_test_rand<int8_t, int8_t>(handle,
                                                        INT8_C3,
                                                        INT8_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format,
                                                        1);
                    std::cout << "result of " << test_cnt++ << " compare passed"
                            << std::endl;
                    cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 1N uint8 TO 1N int8 " << endl;
                    convert_to_test_rand<uint8_t, int8_t>(handle,
                                                        UINT8_C3,
                                                        INT8_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format,
                                                        1);
                    cout << "result of " << test_cnt++ << "compare passed " << endl;
                    cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 1N fp32 TO 1N fp32 " << endl;
                    convert_to_test_rand<float_t, float_t>(handle,
                                                        FLOAT32_C3,
                                                        FLOAT32_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format,
                                                        1);
                    cout << "result of " << test_cnt++ << "compare passed " << endl;

                }
            }
            break;
        }
        case 0x1686:{
            cout << "---------CONVERT TO CLASSICAL SIZE TEST----------" << endl;
            convert_format = CONVERT_1N_TO_1N;
            cout << "CONVERT TO 1N int8 TO 1N int8" << endl;
            convert_to_test_rand<int8_t, int8_t>(handle,
                                                INT8_C3,
                                                INT8_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format);
            std::cout << "result of " << test_cnt++ << " compare passed"
                    << std::endl;
            cout << "CONVERT TO 1N fp32 TO 1N fp32" << endl;
            convert_to_test_rand<float_t, float_t>(handle,
                                                FLOAT32_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            cout << "[WITH STRIDE] CONVERT TO 1N int8 TO 1N int8" << endl;
            convert_to_test_rand<int8_t, int8_t>(handle,
                                                INT8_C3,
                                                INT8_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format,
                                                1);
            std::cout << "result of " << test_cnt++ << " compare passed"
                    << std::endl;
            cout << "[WITH STRIDE] CONVERT TO 1N fp32 TO 1N fp32" << endl;
            convert_to_test_rand<float_t, float_t>(handle,
                                                FLOAT32_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                convert_to_arg,
                                                convert_format,
                                                1);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            cout << "[SAME PARA] CONVERT TO 1N int8 TO 1N int8" << endl;
            convert_to_test_rand<int8_t, int8_t>(handle,
                                                INT8_C3,
                                                INT8_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format);
            std::cout << "result of " << test_cnt++ << " compare passed"
                    << std::endl;
            cout << "[SAME PARA] CONVERT TO 1N fp32 TO 1N fp32" << endl;
            convert_to_test_rand<float_t, float_t>(handle,
                                                FLOAT32_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 1N int8 TO 1N int8"
                << endl;
            convert_to_test_rand<int8_t, int8_t>(handle,
                                                INT8_C3,
                                                INT8_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format,
                                                1);
            std::cout << "result of " << test_cnt++ << " compare passed"
                    << std::endl;
            cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 1N fp32 TO 1N fp32"
                << endl;
            convert_to_test_rand<float_t, float_t>(handle,
                                                FLOAT32_C3,
                                                FLOAT32_C3,
                                                image_shape,
                                                same_convert_to_arg,
                                                convert_format,
                                                1);
            cout << "result of " << test_cnt++ << "compare passed" << endl;
            cout << "---------CONVERT TO CORNER TEST----------" << endl;
            int rand_loop_num = 2;
            for (int rand_loop_idx = 0; rand_loop_idx < rand_loop_num;
                rand_loop_idx++) {
                for (int rand_mode = 0; rand_mode < MAX_RAND_MODE; rand_mode++) {
                    gen_test_size(chipid,
                                &image_shape.w,
                                &image_shape.h,
                                &image_shape.n,
                                &image_shape.c,
                                rand_mode);
                    std::cout << "rand mode : " << rand_mode
                            << " ,img_w: " << image_shape.w
                            << " ,img_h: " << image_shape.h
                            << " ,img_c: " << image_shape.c
                            << " ,img_n: " << image_shape.n << std::endl;
                    test_cnt       = 0;
                    convert_format = CONVERT_1N_TO_1N;
                    cout << " CONVERT TO 1N fp32 TO 1N fp32 " << endl;
                    convert_to_test_rand<float_t, float_t>(handle,
                                                        FLOAT32_C3,
                                                        FLOAT32_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format);
                    cout << "result of " << test_cnt++ << "compare passed " << endl;
                    cout << "[WITH STRIDE] CONVERT TO 1N int8 TO 1N int8 " << endl;
                    convert_to_test_rand<int8_t, int8_t>(handle,
                                                        INT8_C3,
                                                        INT8_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format,
                                                        1);
                    std::cout << "result of " << test_cnt++ << " compare passed"
                            << std::endl;
                    cout << "[WITH STRIDE] CONVERT TO 1N fp32 TO 1N fp32 " << endl;
                    convert_to_test_rand<float_t, float_t>(handle,
                                                        FLOAT32_C3,
                                                        FLOAT32_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format,
                                                        1);
                    cout << "result of " << test_cnt++ << "compare passed " << endl;
                    convert_format = CONVERT_1N_TO_1N;
                    cout << "[SAME PARA]  CONVERT TO 1N fp32 TO 1N fp32 " << endl;
                    convert_to_test_rand<float_t, float_t>(handle,
                                                        FLOAT32_C3,
                                                        FLOAT32_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format);
                    cout << "result of " << test_cnt++ << "compare passed " << endl;
                    cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 1N int8 TO 1N int8 " << endl;
                    convert_to_test_rand<int8_t, int8_t>(handle,
                                                        INT8_C3,
                                                        INT8_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format,
                                                        1);
                    std::cout << "result of " << test_cnt++ << " compare passed" << endl;
                    cout << "[SAME PARA] [WITH STRIDE] CONVERT TO 1N fp32 TO 1N fp32 " << endl;
                    convert_to_test_rand<float_t, float_t>(handle,
                                                        FLOAT32_C3,
                                                        FLOAT32_C3,
                                                        image_shape,
                                                        convert_to_arg,
                                                        convert_format,
                                                        1);
                    cout << "result of " << test_cnt++ << "compare passed " << endl;
                }
            }
            break;
        }
        default:
            printf("ChipID is NOT supported\n");
            break;
        }
    }
    return NULL;
}

int main(int32_t argc, char **argv) {
    int test_loop_times  = 0;
    int test_threads_num = 1;
    int dev_id = -1;
    if (argc == 1) {
        test_loop_times  = 1;
        test_threads_num = 1;
    } else if (argc == 2) {
        test_loop_times  = atoi(argv[1]);
        test_threads_num = 1;
    } else if (argc == 3) {
        test_loop_times  = atoi(argv[1]);
        test_threads_num = atoi(argv[2]);
    } else if (argc == 4) {
        test_loop_times  = atoi(argv[1]);
        test_threads_num = atoi(argv[2]);
        dev_id = atoi(argv[3]);
    } else if (argc == 5) {
        test_loop_times  = atoi(argv[1]);
        test_threads_num = atoi(argv[2]);
        dev_id = atoi(argv[3]);
        test_seed = atoi(argv[4]);
    } else {
        std::cout << "command input error, please follow this "
                     "order:test_convert_to loop_num multi_thread_numi dev_id seed"
                  << std::endl;
        exit(-1);
    }
    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST CONVERT TO] loop times should be 1~1500"
                  << std::endl;
        exit(-1);
    }
    if (test_threads_num > 4 || test_threads_num < 1) {
        std::cout << "[TEST CONVERT TO] thread nums should be 1~4 "
                  << std::endl;
        exit(-1);
    }
    int dev_cnt;
    bm_dev_getcount(&dev_cnt);
    if (dev_id >= dev_cnt) {
        std::cout << "[TEST SOER] dev_id should less than device count,"
                     " only detect "<< dev_cnt << " devices "
                  << std::endl;
        exit(-1);
    }
    printf("device count = %d\n", dev_cnt);
    #ifdef __linux__
    bm_handle_t handle[dev_cnt];
    #else
    std::shared_ptr<bm_handle_t> handle_(new bm_handle_t[dev_cnt], std::default_delete<bm_handle_t[]>());
    bm_handle_t* handle = handle_.get();
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
    #ifdef __linux__
    pthread_t *pid = new pthread_t[dev_cnt * test_threads_num];
    convert_to_thread_arg_t *convert_to_thread_arg =
        new convert_to_thread_arg_t[dev_cnt * test_threads_num];
    for (int d = 0; d < dev_cnt; d++) {
        for (int i = 0; i < test_threads_num; i++) {
            int idx = d * test_threads_num + i;
            convert_to_thread_arg[idx].trials = test_loop_times;
            convert_to_thread_arg[idx].handle = handle[d];
            if (pthread_create(&pid[idx],
                               NULL,
                               test_convert_to_thread,
                               &convert_to_thread_arg[idx])) {
                delete[] pid;
                delete[] convert_to_thread_arg;
                perror("create thread failed\n");
                exit(-1);
            }
        }
    }
    int ret = 0;
    for (int i = 0; i < dev_cnt * test_threads_num; i++) {
        ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            delete[] pid;
            delete[] convert_to_thread_arg;
            perror("Thread join failed");
            exit(-1);
        }
    }
    for (int d = 0; d < dev_cnt; d++) {
        bm_dev_free(handle[d]);
    }
    std::cout << "------[TEST CONVERT TO] ALL TEST PASSED!" << std::endl;
    delete[] pid;
    delete[] convert_to_thread_arg;
    #else
    #define THREAD_NUM 64
    DWORD              dwThreadIdArray[THREAD_NUM];
    HANDLE             hThreadArray[THREAD_NUM];
    convert_to_thread_arg_t *convert_to_thread_arg =
        new convert_to_thread_arg_t[dev_cnt * test_threads_num];
    for (int d = 0; d < dev_cnt; d++) {
        for (int i = 0; i < test_threads_num; i++) {
            int idx = d * test_threads_num + i;
            convert_to_thread_arg[idx].trials = test_loop_times;
            convert_to_thread_arg[idx].handle = handle[d];
            hThreadArray[idx] = CreateThread(
                NULL,                         // default security attributes
                0,                            // use default stack size
                test_convert_to_thread,       // thread function name
                &convert_to_thread_arg[idx],  // argument to thread function
                0,                            // use default creation flags
                &dwThreadIdArray[idx]);       // returns the thread identifier
            if (hThreadArray[idx] == NULL) {
                delete[] convert_to_thread_arg;
                perror("create thread failed\n");
                exit(-1);
            }
        }
    }
    int   ret = 0;
    DWORD dwWaitResult =
        WaitForMultipleObjects(dev_cnt * test_threads_num, hThreadArray, TRUE, INFINITE);
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
        delete[] convert_to_thread_arg;
        printf("Thread join failed\n");
        return -1;
    }
    for (int d = 0; d < dev_cnt; d++) {
        for (int i = 0; i < test_threads_num; i++) {
            int idx = d * test_threads_num + i;
            CloseHandle(hThreadArray[idx]);
        }
    }
    std::cout << "------[TEST CONVERT TO] ALL TEST PASSED!" << std::endl;
    delete[] convert_to_thread_arg;
    #endif
    return 0;
}
