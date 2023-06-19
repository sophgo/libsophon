#include <memory>
#include <assert.h>
#include <iostream>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <cmath>
#include <cstring>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "test_misc.h"

#ifdef __linux__
  #include <unistd.h>
  #include <sys/time.h>
  #include <pthread.h>
#else
  #include <windows.h>
  #include "time.h"
#endif
#ifndef USING_CMODEL
#include "vpplib.h"
#endif

using namespace std;
using std::multiset;
using std::vector;

typedef int bm_resize_data_type_t;
typedef struct {
    u64 img_addr;
    int width;
    int height;
    int width_stride;
} bm_img_info_t;

typedef struct {
    int trials;
} resize_thread_arg_t;

#define BM_W_SHIFT_VAL (7)
#define MAX_ROI_PER_IMAGE (32)
#define W_STRIDE_MAX_BITS (12)
#define W_SHIFT_MAX_BITS (4)
#define W_STRIDE_MAX_VAL (1 << W_STRIDE_MAX_BITS)
#define W_SHIFT_MAX_VAL (1 << W_SHIFT_MAX_BITS)

typedef enum { IMG_TEST_MODE = 0, RAND_TEST_MODE, MAX_TEST_MODE } test_mode_e;
typedef enum {
    TPU_RESIZE = 0,
    VPP_RESIZE,
    VPP_CROP_ONE,
    VPP_CROP_MUL
} vpp_mode_e;
typedef enum {
    RAND_TO_MAX = 0,
    MAX_TO_RAND,
    RAND_TO_MIN,
    MIN_TO_RAND,
    MAX_RAND_MODE
} rand_mode_e;

#define RESIZE_ALIGN(addr, alignment)                                          \
    (((addr + alignment - 1) / alignment) * alignment)
#define VPP_ALIGNMENT (64)
#define MAX_W (2048)
#define MAX_H (2048)
#define MIN_W (MAX_W / 32)
#define MIN_H (MAX_H / 32)
const int channel_num       = 3;
int       constant_resize_w = 400;
int       constant_resize_h = 200;
int       constant_crop_w   = 711;
int       constant_crop_h   = 400;
int       dev_id = 0;
int       is_random = 1;
// int resize_w = 200;
// int resize_h = 200;
// int crop_w = 16;
// int crop_h = 16;
//// const int crop_w = 720;
//// const int crop_h = 400;
//// const int resize_w = 720;
//// const int resize_h = 400;
// const int resize_w = 7680;
// const int resize_h = 2160;
// const int crop_w = 7680;
// const int crop_h = 2160;
// const int crop_w = 720;
// const int crop_h = 400;
// const int resize_w = 720;
// const int resize_h = 400;
const int crop_start_x             = 0;
const int crop_start_y             = 0;
int       constant_test_img_width  = 1920;
int       constant_test_img_height = 1080;

static u64
allocate_dev_mem(bm_handle_t handle, bm_device_mem_t *dst_dev_mem, int size) {
    bm_malloc_device_byte(handle, dst_dev_mem, size);
    return bm_mem_get_device_addr(*dst_dev_mem);
}

static int dynamic_get_w_info(float scale_w,
                              int   if_4n_to_1n,
                              int & w_stride,
                              int & w_shift) {
    int times = (int)(W_STRIDE_MAX_VAL / scale_w);
    w_shift   = 0;
    int temp  = times;
    while ((temp != 0) && (temp != 1)) {
        temp = temp >> 1;
        w_shift++;
    }
    if (if_4n_to_1n) {
        w_shift = (((1 << w_shift) * 4) >= W_STRIDE_MAX_VAL)
                      ? (W_STRIDE_MAX_BITS - 3)
                      : (w_shift);
    }
    w_shift  = (w_shift >= W_SHIFT_MAX_VAL) ? (W_SHIFT_MAX_VAL - 1) : (w_shift);
    w_stride = (int)(scale_w * (1 << w_shift));
    if (w_stride >= W_STRIDE_MAX_VAL) {
        w_shift  = w_shift - 1;
        w_stride = (int)(scale_w * (1 << w_shift));
    }

    return 0;
}

void interleave(unsigned char *inout, int N, int H, int W) {
    unsigned char *temp = new unsigned char[H * W * 3];
    for (int n = 0; n < N; n++) {
        unsigned char *start = inout + 3 * H * W * n;
        for (int h = 0; h < H; h++) {
            for (int w = 0; w < W; w++) {
                temp[3 * (h * W + w)]     = start[(h * W + w)];
                temp[3 * (h * W + w) + 1] = start[(h * W + w) + H * W];
                temp[3 * (h * W + w) + 2] = start[(h * W + w) + 2 * H * W];
            }
        }
        memcpy(start, temp, H * W * 3);
    }
    delete[] temp;
}

// void storage_sys_memory(void *buf, char *fn, int n, int h, int w) {
//     cv::Mat        img;
//     int            c           = 3;
//     int            tensor_size = n * c * h * w;
//     int            c_size      = c * h * w;
//     unsigned char *s           = new unsigned char[tensor_size];
//     memcpy(s, buf, tensor_size);
//     interleave(s, n, h, w);
//     for (int i = 0; i < n; i++) {
//         char fname[256];
//         sprintf(fname, "%s_%d.png", fn, i);
//         img.create(h, w, CV_8UC3);
//         memcpy(img.data, s + c_size * i, c_size);
//         cv::imwrite(fname, img);
//     }
//     delete[] s;
// }

// void read_sys_memory(u8 *buf, char *fn, int c, int h, int w) {
//     cv::Mat img_packed;
//     cv::Mat img_planar;
//     img_packed      = cv::imread(fn);
//     u8 *data_packed = img_packed.data;
//     int image_size  = w * h;
//     for (int i = 0; i < (int)img_packed.total() * c; i += 3) {
//         buf[i / 3]                  = data_packed[i + 0];  // B
//         buf[i / 3 + image_size]     = data_packed[i + 1];  // G
//         buf[i / 3 + image_size * 2] = data_packed[i + 2];  // R
//     }
// }


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

template <typename T, typename T1>
static int resize_ref(T *            old_img_buf,
                      T1 *           new_img_buf,
                      bmcv_resize_t *resize_img_attr,
                      int            image_num,
                      int            channel_num,
                      int            width_stride,
                      int            height_stride,
                      int            new_width_stride,
                      int            new_height_stride,
                      int            if_4N_to_1N,
                      bool           in_is_packed,
                      int            total_roi_nums = 0,
                      vector<int>    roi_vec        = vector<int>()) {
    T *temp_old_buf =
        new T[image_num * channel_num * width_stride * height_stride];
    T *flatten_old_buf =
        new T[total_roi_nums * channel_num * width_stride * height_stride];
    T *planar_buf;

    if (in_is_packed) {
        int hw = height_stride * width_stride;
        planar_buf = new T[image_num * channel_num * hw];
        if (if_4N_to_1N) {
            T *in_1N_buf = new T[image_num * channel_num * hw];
            for (int n = 0; n < image_num; n++) {
                for (int loop = 0; loop < channel_num * hw; loop++) {
                    in_1N_buf[n * channel_num * hw + loop] =
                        old_img_buf[(n / 4) * 4 * channel_num * hw + 4 * loop + n % 4];
                }
            }
            for (int i = 0; i < image_num; i++) {
                for (int loop = 0; loop < hw; loop++) {
                    planar_buf[3 * hw * i + loop] = in_1N_buf[3 * hw * i + 3 * loop];
                    planar_buf[3 * hw * i + loop + hw] = in_1N_buf[3 * hw * i + 3 * loop + 1];
                    planar_buf[3 * hw * i + loop + 2 * hw] = in_1N_buf[3 * hw * i + 3 * loop + 2];
                }
            }
            delete[] in_1N_buf;
        } else {
            for (int i = 0; i < image_num; i++) {
                for (int loop = 0; loop < hw; loop++) {
                    planar_buf[3 * hw * i + loop] = old_img_buf[3 * hw * i + 3 * loop];
                    planar_buf[3 * hw * i + loop + hw] = old_img_buf[3 * hw * i + 3 * loop + 1];
                    planar_buf[3 * hw * i + loop + 2 * hw] = old_img_buf[3 * hw * i + 3 * loop + 2];
                }
            }
        }
    } else {
        planar_buf = old_img_buf;
    }

    memcpy(temp_old_buf,
           planar_buf,
           image_num * channel_num * width_stride * height_stride * sizeof(T));
    if (!in_is_packed && if_4N_to_1N) {
        convert_4N_2_1N((unsigned char *)temp_old_buf,
                        image_num,
                        channel_num,
                        height_stride,
                        width_stride);
    }
    if (total_roi_nums > 0) {
        int image_offset      = width_stride * height_stride * channel_num;
        int roi_offset        = image_offset;
        int image_addr_offset = 0;
        for (int image_cnt = 0; image_cnt < image_num; image_cnt++) {
            for (int roi_cnt = 0; roi_cnt < roi_vec[image_cnt]; roi_cnt++) {
                memcpy(flatten_old_buf + roi_offset * roi_cnt +
                           image_addr_offset,
                       temp_old_buf + image_offset * image_cnt,
                       image_offset * sizeof(T));
            }
            image_addr_offset =
                image_addr_offset + roi_vec[image_cnt] * image_offset;
        }
        delete[] temp_old_buf;
        temp_old_buf = flatten_old_buf;
    }
    image_num = (total_roi_nums > 0) ? (total_roi_nums) : (image_num);
    for (int image_idx = 0; image_idx < image_num; image_idx++) {
        float scale_w = (float)(resize_img_attr[image_idx].in_width) /
                        (float)resize_img_attr[image_idx].out_width;
        float scale_h = (float)(resize_img_attr[image_idx].in_height) /
                        (float)resize_img_attr[image_idx].out_height;
        int scale_para = 0;
        // int new_width = resize_img_attr[image_idx].out_width;
        // int new_height = resize_img_attr[image_idx].out_height;
        // int scale_para_fix = (int)(scale_w * scale_para);
        int scale_para_fix = 0;
        int w_shift        = 0;
        dynamic_get_w_info(scale_w, if_4N_to_1N, scale_para_fix, w_shift);
        scale_para  = 1 << w_shift;
        int start_x = resize_img_attr[image_idx].start_x;
        int start_y = resize_img_attr[image_idx].start_y;
        for (int channel_idx = 0; channel_idx < channel_num; channel_idx++) {
            for (int j = 0; j < resize_img_attr[image_idx].out_height; j++) {
                int h_offset = (int)(j * scale_h);
                h_offset     = (h_offset < resize_img_attr[image_idx].in_height)
                               ? (h_offset)
                               : (resize_img_attr[image_idx].in_height - 1);
                for (int i = 0; i < resize_img_attr[image_idx].out_width; i++) {
                    int index = (start_x + start_y * width_stride) +
                                (int)((scale_para_fix * i) / scale_para) +
                                (h_offset * width_stride) +
                                ((channel_idx + image_idx * channel_num) *
                                 width_stride * height_stride);
                    int out_index = i + j * new_width_stride +
                                    (channel_idx + image_idx * channel_num) *
                                        new_width_stride * new_height_stride;
                    new_img_buf[out_index] = (T1) * (temp_old_buf + index);
                }
            }
        }
    }
    if (in_is_packed) {
        delete[] planar_buf;
    }
    if (total_roi_nums > 0) {
        delete[] temp_old_buf;
    } else {
        delete[] temp_old_buf;
        delete[] flatten_old_buf;
    }

    return 0;
}

template <typename T, typename T1>
static int bmcv_resize_cmp(T1 *p_exp,
                           T1 *p_got,
                           T * src,
                           int image_num,
                           int image_channel,
                           int image_sh,
                           int image_sw,
                           int image_dh,
                           int image_dw,
                           int resize_in_h,
                           int resize_in_w) {
    const int         err_threshold = 1;
    int               find_x_index  = 0;
    int               find_y_index  = 0;
    int               find_index    = 0;
    multiset<uint8_t> find_table;
    // float             scale_w = (float)(image_sw) / (float)image_dw;
    // float             scale_h = (float)(image_sh) / (float)image_dh;
    float scale_w = (float)(resize_in_w) / (float)image_dw;
    float scale_h = (float)(resize_in_h) / (float)image_dh;

    for (int n_idx = 0; n_idx < image_num; n_idx++) {
        for (int c_idx = 0; c_idx < image_channel; c_idx++) {
            for (int y = 0; y < image_dh; y++) {
                for (int x = 0; x < image_dw; x++) {
                    int check_idx =
                        (n_idx * image_channel + c_idx) * image_dw * image_dh +
                        y * image_dw + x;
                    T1 got = p_got[check_idx];
                    T1 exp = p_exp[check_idx];
                    if (got != exp) {
                        for (int tmp_y = (0 - err_threshold);
                             tmp_y <= err_threshold;
                             tmp_y++) {
                            for (int tmp_x = (0 - err_threshold);
                                 tmp_x <= err_threshold;
                                 tmp_x++) {
                                find_x_index = round(x * scale_w + tmp_x);
                                find_x_index =
                                    (find_x_index < 0) ? (0) : (find_x_index);
                                find_y_index = round(y * scale_h + tmp_y);
                                find_y_index =
                                    (find_y_index < 0) ? (0) : (find_y_index);
                                find_index = (n_idx * image_channel + c_idx) *
                                                 image_sw * image_sh +
                                             find_y_index * image_sw +
                                             find_x_index;
                                T find_elem = (T)src[find_index];
                                find_table.insert(find_elem);
                            }
                        }
                        if (find_table.end() == find_table.find(got)) {
                            // printf("h %d w %d,c:%d,n:%d, got:%d exp %d\n",
                            //        y,
                            //        x,
                            //        c_idx,
                            //        n_idx,
                            //        (int)got,
                            //        (int)exp);
                            std::cout << "h: " << y << " w: " << x
                                      << " c: " << c_idx << " n: " << n_idx
                                      << " got: " << (int)got
                                      << " exp: " << (int)exp << std::endl;

                            return -1;
                        }
                        find_table.clear();
                    }
                }
            }
        }
    }
    return 0;
}

int vpp_resize_cmp(
    u8 *got, u8 *exp, int w, int h, int c, int n, float percision) {
    for (int idx = 0; idx < n; idx++) {
        for (int k = 0; k < c; k++) {
            for (int j = 0; j < h; j++) {
                for (int i = 0; i < w; i++) {
                    int len = idx * c * w * h + k * w * h + w * j + i;
                    if (fabs(got[len] - exp[len]) > percision) {
                        cout << "cmp error, got: " << (u8)got[len]
                             << " exp: " << (u8)exp[len] << " ,n: " << idx
                             << " ,c: " << k << " ,w: " << i << " ,h: " << j
                             << endl;
                        return -1;
                    }
                }
            }
        }
    }

    return 0;
}

template <typename T, typename T1>
static int resize_test_rand(bm_handle_t handle,
                            int         data_type,
                            int         out_data_type,
                            int         test_img_width,
                            int         test_img_height,
                            int         crop_w,
                            int         crop_h,
                            int         resize_w,
                            int         resize_h,
                            int         image_num     = 1,
                            int         image_channel = 1,
                            int         if_4N_to_1N   = 0,
                            int         if_multi_roi  = 0,
                            int         if_vpp        = TPU_RESIZE) {

    #ifdef __linux__
    bm_image            input[image_num], output[32];
    #else
    std::shared_ptr<bm_image> input_(new bm_image[image_num], std::default_delete<bm_image[]>());
    bm_image*                 input = input_.get();
    bm_image                  output[32];
    #endif
    int                 width_stride = 0, height_stride = 0;
    int                 tmp_test_img_width  = test_img_width;
    int                 tmp_test_img_height = test_img_height;
    int                 tmp_crop_w          = crop_w;
    int                 tmp_crop_h          = crop_h;
    int                 tmp_resize_w        = resize_w;
    int                 tmp_resize_h        = resize_h;
    bm_image_format_ext in_image_format = (1 == image_channel) ?
        (FORMAT_GRAY) : (rand() % 2 ? FORMAT_BGR_PACKED : FORMAT_BGR_PLANAR);
    bm_image_format_ext out_image_format =
        (1 == image_channel) ? (FORMAT_GRAY) : (FORMAT_BGR_PLANAR);
    int input_num  = 0;
    int output_num = 0;
    if (VPP_CROP_ONE == if_vpp) {
        test_img_width  = RESIZE_ALIGN(test_img_width, VPP_ALIGNMENT);
        test_img_height = test_img_height;
        crop_w          = RESIZE_ALIGN(crop_w, VPP_ALIGNMENT);
        crop_h          = crop_h;
        // resize_w        = RESIZE_ALIGN(crop_w, VPP_ALIGNMENT);
        // resize_h        = crop_h;
        resize_w = RESIZE_ALIGN(resize_w, VPP_ALIGNMENT);
        resize_h = resize_h;
        // in_image_format  = FORMAT_BGR_PACKED;
        // out_image_format = FORMAT_BGR_PACKED;
        in_image_format  = rand() % 2 ? FORMAT_BGR_PLANAR : FORMAT_BGR_PACKED;
        out_image_format = FORMAT_BGR_PLANAR;
        if_multi_roi     = 1;
    }
    if (VPP_CROP_MUL == if_vpp) {
        test_img_width  = RESIZE_ALIGN(test_img_width, VPP_ALIGNMENT);
        test_img_height = test_img_height;
        crop_w          = RESIZE_ALIGN(crop_w, VPP_ALIGNMENT);
        crop_h          = crop_h;
        // resize_w         = RESIZE_ALIGN(crop_w, VPP_ALIGNMENT);
        // resize_h         = crop_h;
        resize_w = RESIZE_ALIGN(resize_w, VPP_ALIGNMENT);
        resize_h = resize_h;
        // in_image_format  = FORMAT_BGR_PACKED;
        // out_image_format = FORMAT_BGR_PACKED;
        in_image_format  = rand() % 2 ? FORMAT_BGR_PLANAR : FORMAT_BGR_PACKED;
        out_image_format = FORMAT_BGR_PLANAR;
        if_multi_roi     = 0;
    }
    bool in_is_packed = ((in_image_format == FORMAT_BGR_PACKED) ||
                      (in_image_format == FORMAT_RGB_PACKED));
    if ((DATA_TYPE_EXT_4N_BYTE == data_type) && (!if_4N_to_1N)) {
        image_num = ALIGN(image_num, 4) / 4;
    }
    int image_len =
        image_num * image_channel * test_img_width * test_img_height;
    int resize_image_len = image_num * image_channel * resize_w * resize_h;

    T * img_data = new T[image_len];
    T1 *res_data = new T1[resize_image_len];
    T1 *ref_data = new T1[resize_image_len];
    memset(img_data, 0x0, image_len);
    memset(res_data, 0x0, resize_image_len);
    memset(ref_data, 0x0, resize_image_len);
#ifndef USING_CMODEL
    vpp_rect *rect = nullptr;
#endif
    for (int i = 0; i < image_len; i++) {
        img_data[i] = rand() % 255;
    }
    // int single_len = test_img_width * test_img_height;
    // for (int image_idx = 0; image_idx < image_num; image_idx++) {
    //    for (int i = 0; i < single_len; i++) {
    //        img_data[image_idx * single_len * 3 + i]                  = 255;
    //        img_data[image_idx * single_len * 3 + i + single_len]     = 0;
    //        img_data[image_idx * single_len * 3 + i + single_len * 2] = 0;
    //    }
    //}
    // int single_len = test_img_width * test_img_height * 3;
    // for (int image_idx = 0; image_idx < image_num; image_idx++) {
    //    read_sys_memory(img_data + image_idx * single_len,
    //                    (char *)"./test_2.jpg",
    //                    image_channel,
    //                    test_img_height,
    //                    test_img_width);
    //}
    // int single_len = test_img_width * test_img_height;
    // for (int image_idx = 0; image_idx < image_num; image_idx++) {
    //    for (int i = 0; i < single_len; i++) {
    //        img_data[image_idx * single_len * 3 + i * 3]     = 255;
    //        img_data[image_idx * single_len * 3 + i * 3 + 1] = 0;
    //        img_data[image_idx * single_len * 3 + i * 3 + 2] = 0;
    //    }
    //}
    // storage_sys_memory(
    //     img_data, (char *)"input", image_num, test_img_height,
    //     test_img_width);
    width_stride  = test_img_width;
    height_stride = test_img_height;
    bmcv_resize_image resize_attr[4];
#ifndef USING_CMODEL
    // int vpp_total_rois = 0;
#endif
    int local_roi_num = 0;
    if (!if_multi_roi) {
        #ifdef __linux__
        bmcv_resize_t resize_img_attr[image_num];
        #else
        std::shared_ptr<bmcv_resize_t> resize_img_attr_(new bmcv_resize_t[image_num], std::default_delete<bmcv_resize_t[]>());
        bmcv_resize_t* resize_img_attr = resize_img_attr_.get();
        #endif
        if (!if_4N_to_1N) {
            for (int img_idx = 0; img_idx < image_num; img_idx++) {
                resize_img_attr[img_idx].start_x    = 0;
                resize_img_attr[img_idx].start_y    = 0;
                resize_img_attr[img_idx].in_width   = crop_w;
                resize_img_attr[img_idx].in_height  = crop_h;
                resize_img_attr[img_idx].out_width  = resize_w;
                resize_img_attr[img_idx].out_height = resize_h;
            }
        } else {
            resize_img_attr[0].start_x    = 0;
            resize_img_attr[0].start_y    = 0;
            resize_img_attr[0].in_width   = crop_w;
            resize_img_attr[0].in_height  = crop_h;
            resize_img_attr[0].out_width  = resize_w;
            resize_img_attr[0].out_height = resize_h;

            resize_img_attr[1].start_x    = 0;
            resize_img_attr[1].start_y    = 0;
            resize_img_attr[1].in_width   = crop_w;
            resize_img_attr[1].in_height  = crop_h;
            resize_img_attr[1].out_width  = resize_w;
            resize_img_attr[1].out_height = resize_h;

            resize_img_attr[2].start_x    = 0;
            resize_img_attr[2].start_y    = 0;
            resize_img_attr[2].in_width   = crop_w;
            resize_img_attr[2].in_height  = crop_h;
            resize_img_attr[2].out_width  = resize_w;
            resize_img_attr[2].out_height = resize_h;

            resize_img_attr[3].start_x    = 0;
            resize_img_attr[3].start_y    = 0;
            resize_img_attr[3].in_width   = crop_w;
            resize_img_attr[3].in_height  = crop_h;
            resize_img_attr[3].out_width  = resize_w;
            resize_img_attr[3].out_height = resize_h;
        }
        for (int img_idx = 0; img_idx < image_num; img_idx++) {
            resize_attr[img_idx].resize_img_attr = &resize_img_attr[img_idx];
            resize_attr[img_idx].roi_num         = 1;
            resize_attr[img_idx].stretch_fit     = 1;
            resize_attr[img_idx].interpolation   = BMCV_INTER_NEAREST;
        }
#ifndef USING_CMODEL
        if (VPP_CROP_MUL == if_vpp) {
            // rect = (vpp_rect *)malloc(total_roi_num);
            rect = new vpp_rect[image_num];
            for (int idx = 0; idx < image_num; idx++) {
                rect[idx].x     = resize_attr[idx].resize_img_attr[0].start_x;
                rect[idx].y     = resize_attr[idx].resize_img_attr[0].start_y;
                rect[idx].width = resize_attr[idx].resize_img_attr[0].in_width;
                rect[idx].height =
                    resize_attr[idx].resize_img_attr[0].in_height;
            }
        }
#endif
        input_num  = (!if_4N_to_1N) ? (image_num) : (image_num / 4);
        output_num = image_num;
        for (int img_idx = 0; img_idx < input_num; img_idx++) {
            bm_image_create(handle,
                            test_img_height,
                            test_img_width,
                            in_image_format,
                            (bm_image_data_format_ext)data_type,
                            &input[img_idx]);
        }
        if (TPU_RESIZE == if_vpp) {
            bm_image_alloc_contiguous_mem(input_num, input, BMCV_IMAGE_FOR_OUT);
        } else {
            bm_image_alloc_contiguous_mem(input_num, input, BMCV_IMAGE_FOR_IN);
        }
        for (int img_idx = 0; img_idx < input_num; img_idx++) {
            int img_offset = test_img_width * test_img_height * image_channel;
            T * input_img_data = img_data + img_offset * img_idx;
            bm_image_copy_host_to_device(input[img_idx],
                                         (void **)&input_img_data);
        }
        for (int img_idx = 0; img_idx < output_num; img_idx++) {
            int output_data_type =
                (if_4N_to_1N) ? (DATA_TYPE_EXT_1N_BYTE) : (out_data_type);
            bm_image_create(handle,
                            resize_h,
                            resize_w,
                            out_image_format,
                            (bm_image_data_format_ext)output_data_type,
                            &output[img_idx]);
        }
        bm_image_alloc_contiguous_mem(output_num, output, BMCV_IMAGE_FOR_OUT);
        bmcv_image_resize(handle, input_num, resize_attr, input, output);
        // bmcv_image_resize(handle, image_num, resize_attr, input, output);
        for (int img_idx = 0; img_idx < output_num; img_idx++) {
            int img_offset   = resize_w * resize_h * image_channel;
            T1 *res_img_data = res_data + img_offset * img_idx;
            bm_image_copy_device_to_host(output[img_idx],
                                         (void **)&res_img_data);
        }
        // storage_sys_memory(res_data,
        //                   (char *)"output",
        //                   image_num,
        //                   output[0].height,
        //                   output[0].width);
        resize_ref<T, T1>((T *)img_data,
                          (T1 *)ref_data,
                          resize_img_attr,
                          image_num,
                          image_channel,
                          width_stride,
                          height_stride,
                          output[0].width,
                          output[0].height,
                          if_4N_to_1N,
                          in_is_packed);
    } else {
        // int if_same_size = DIFF_SIZE;
        int           roi_per_image = 4;
        std::shared_ptr<bmcv_resize_t> resize_img_attr_(new bmcv_resize_t[image_num*roi_per_image], std::default_delete<bmcv_resize_t[]>());
        bmcv_resize_t *resize_img_attr = resize_img_attr_.get();
        int roi_start_x_offset = (crop_w == test_img_width) ? (0) : (1);
        int roi_start_y_offset = (crop_h == test_img_height) ? (0) : (1);
        if (if_4N_to_1N) {
            roi_start_x_offset = 4 * roi_start_x_offset;
            roi_start_y_offset = 4 * roi_start_y_offset;
        }
        roi_start_x_offset =
            (roi_start_x_offset * roi_per_image + crop_w > test_img_width)
                ? (0)
                : (roi_start_x_offset);
        roi_start_y_offset =
            (roi_start_y_offset * roi_per_image + crop_h > test_img_height)
                ? (0)
                : (roi_start_y_offset);
        #ifdef __linux__
        int         roi_num_array[image_num];
        memset(roi_num_array, 0, image_num*sizeof(int));
        #else
        std::shared_ptr<int> roi_num_array_(new int[image_num],
                                            std::default_delete<int[]>());
        int *                roi_num_array = roi_num_array_.get();
        for (int i = 0; i < image_num; i++) {
            roi_num_array[i] = 0;
        }
        #endif

        vector<int> roi_num_vec(image_num);
        for (int img_cnt = 0; img_cnt < image_num; img_cnt++) {
            for (int roi_cnt = 0; roi_cnt < roi_per_image; roi_cnt++) {
                resize_img_attr[img_cnt * roi_per_image + roi_cnt].start_x = roi_cnt * roi_start_x_offset;
                resize_img_attr[img_cnt * roi_per_image + roi_cnt].start_y = roi_cnt * roi_start_y_offset;
                resize_img_attr[img_cnt * roi_per_image + roi_cnt].in_width   = crop_w;
                resize_img_attr[img_cnt * roi_per_image + roi_cnt].in_height  = crop_h;
                resize_img_attr[img_cnt * roi_per_image + roi_cnt].out_width  = resize_w;
                resize_img_attr[img_cnt * roi_per_image + roi_cnt].out_height = resize_h;
            }
            roi_num_array[img_cnt]               = roi_per_image;
            roi_num_vec[img_cnt]                 = roi_per_image;
            resize_attr[img_cnt].resize_img_attr = &resize_img_attr[img_cnt * roi_per_image];
            resize_attr[img_cnt].roi_num         = roi_per_image;
            resize_attr[img_cnt].stretch_fit     = 1;
            resize_attr[img_cnt].interpolation   = BMCV_INTER_NEAREST;
        }
        int total_roi_num = image_num * roi_per_image;
#ifndef USING_CMODEL
        if (VPP_CROP_ONE == if_vpp) {
            // rect = (vpp_rect *)malloc(total_roi_num);
            rect = new vpp_rect[total_roi_num];
            for (int idx = 0; idx < total_roi_num; idx++) {
                rect[idx].x     = resize_attr[0].resize_img_attr[idx].start_x;
                rect[idx].y     = resize_attr[0].resize_img_attr[idx].start_y;
                rect[idx].width = resize_attr[0].resize_img_attr[idx].in_width;
                rect[idx].height =
                    resize_attr[0].resize_img_attr[idx].in_height;
            }
        }
        // vpp_total_rois = total_roi_num;
#endif
        local_roi_num = total_roi_num;
        // image_len =
        //    total_roi_num * image_channel * test_img_width *
        //    test_img_height;
        image_len = total_roi_num * image_channel * resize_w * resize_h;
        delete[] res_data;
        delete[] ref_data;
        res_data = new T1[image_len];
        ref_data = new T1[image_len];
        memset(res_data, 0x0, image_len);
        memset(ref_data, 0x0, image_len);

        input_num  = (!if_4N_to_1N) ? (image_num) : (image_num / 4);
        output_num = total_roi_num;
        for (int img_idx = 0; img_idx < input_num; img_idx++) {
            bm_image_create(handle,
                            test_img_height,
                            test_img_width,
                            in_image_format,
                            (bm_image_data_format_ext)data_type,
                            &input[img_idx]);
        }
        if (TPU_RESIZE == if_vpp) {
            bm_image_alloc_contiguous_mem(input_num, input, BMCV_IMAGE_FOR_OUT);
        } else {
            bm_image_alloc_contiguous_mem(input_num, input, BMCV_IMAGE_FOR_IN);
        }
        for (int img_idx = 0; img_idx < input_num; img_idx++) {
            int img_offset = test_img_width * test_img_height * image_channel;
            T * input_img_data = img_data + img_offset * img_idx;
            bm_image_copy_host_to_device(input[img_idx],
                                         (void **)&input_img_data);
        }
        for (int img_idx = 0; img_idx < output_num; img_idx++) {
            int output_data_type =
                (if_4N_to_1N) ? (DATA_TYPE_EXT_1N_BYTE) : (out_data_type);
            bm_image_create(handle,
                            resize_h,
                            resize_w,
                            out_image_format,
                            (bm_image_data_format_ext)output_data_type,
                            &output[img_idx]);
        }
        bm_image_alloc_contiguous_mem(output_num, output, BMCV_IMAGE_FOR_OUT);

        struct timeval t1, t2;
        gettimeofday_(&t1);
        bmcv_image_resize(handle, input_num, resize_attr, input, output);
        gettimeofday_(&t2);
        cout << "resize using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

        // bmcv_image_resize(handle, image_num, resize_attr, input, output);
        for (int img_idx = 0; img_idx < output_num; img_idx++) {
            int img_offset   = resize_w * resize_h * image_channel;
            T1 *res_img_data = res_data + img_offset * img_idx;
            bm_image_copy_device_to_host(output[img_idx],
                                         (void **)&res_img_data);
        }
        // storage_sys_memory(res_data,
        //                   (char *)"output",
        //                   output_num,
        //                   output[0].height,
        //                   output[0].width);
        resize_ref<T, T1>((T *)img_data,
                          (T1 *)ref_data,
                          (bmcv_resize_t *)resize_img_attr,
                          image_num,
                          image_channel,
                          width_stride,
                          height_stride,
                          output[0].width,
                          output[0].height,
                          if_4N_to_1N,
                          in_is_packed,
                          total_roi_num,
                          roi_num_vec);
    }
    int ret = 0;
    if (TPU_RESIZE == if_vpp) {
        ret = bmcv_resize_cmp<T, T1>(ref_data,
                                     res_data,
                                     img_data,
                                     (if_multi_roi) ? (local_roi_num)
                                                    : (image_num),
                                     image_channel,
                                     test_img_height,
                                     test_img_width,
                                     resize_h,
                                     resize_w,
                                     crop_h,
                                     crop_w);
#ifndef USING_CMODEL
    } else if (VPP_CROP_ONE == if_vpp) {
        delete[] rect;
    } else if (VPP_CROP_MUL == if_vpp) {
        delete[] rect;
#endif
    }

    if (ret < 0) {
        // printf("[ERROR] COMPARE FAILED !\r\n");
        cout << "[ERROR] COMPARE FAILED !\r\n" << endl;
        delete[] img_data;
        delete[] res_data;
        delete[] ref_data;
        bm_image_free_contiguous_mem(input_num, input);
        bm_image_free_contiguous_mem(output_num, output);
        for (int i = 0; i < input_num; i++) {
            bm_image_destroy(input[i]);
        }
        for (int i = 0; i < output_num; i++) {
            bm_image_destroy(output[i]);
        }
        bm_dev_free(handle);
        exit(-1);
    } else {
        // printf("COMPARE PASSED\r\n");
        cout << "COMPARE PASSED!\r\n" << endl;
    }
    if (TPU_RESIZE != if_vpp) {
        test_img_width  = tmp_test_img_width;
        test_img_height = tmp_test_img_height;
        crop_w          = tmp_crop_w;
        crop_h          = tmp_crop_h;
        resize_w        = tmp_resize_w;
        resize_h        = tmp_resize_h;
    }
    delete[] img_data;
    delete[] res_data;
    delete[] ref_data;
    bm_image_free_contiguous_mem(input_num, input);
    bm_image_free_contiguous_mem(output_num, output);
    for (int i = 0; i < input_num; i++) {
        bm_image_destroy(input[i]);
    }
    for (int i = 0; i < output_num; i++) {
        bm_image_destroy(output[i]);
    }

    return 0;
}

int gen_test_size(int *image_w,
                  int *image_h,
                  int *crop_w,
                  int *crop_h,
                  int *resize_w,
                  int *resize_h,
                  int *image_n,
                  int *image_c,
                  int  gen_mode) {
    switch (gen_mode) {
        case (RAND_TO_MAX): {
            *image_w  = rand() % MAX_W;
            *image_h  = rand() % MAX_H;
            *image_w  = (*image_w > MIN_W) ? (*image_w) : (MIN_W);
            *image_h  = (*image_h > MIN_H) ? (*image_h) : (MIN_H);
            *crop_w   = rand() % (*image_w);
            *crop_h   = rand() % (*image_h);
            *crop_w   = (*crop_w > MIN_W) ? (*crop_w) : (MIN_W);
            *crop_h   = (*crop_h > MIN_H) ? (*crop_h) : (MIN_H);
            *resize_w = MAX_W;
            *resize_h = MAX_H;
            *image_n  = rand() % 4 + 1;
            *image_c  = 3;
            break;
        }
        case (MAX_TO_RAND): {
            *image_w  = MAX_W;
            *image_h  = MAX_H;
            *crop_w   = rand() % (*image_w);
            *crop_h   = rand() % (*image_h);
            *crop_w   = (*crop_w > MIN_W) ? (*crop_w) : (MIN_W);
            *crop_h   = (*crop_h > MIN_H) ? (*crop_h) : (MIN_H);
            *resize_w = rand() % *crop_w;
            *resize_h = rand() % *crop_h;
            *resize_w = (*resize_w > MIN_W) ? (*resize_w) : (MIN_W);
            *resize_h = (*resize_h > MIN_H) ? (*resize_h) : (MIN_H);
            *image_n  = rand() % 4 + 1;
            *image_c  = 3;
            break;
        }
        case (RAND_TO_MIN): {
            *image_w  = rand() % MAX_W;
            *image_h  = rand() % MAX_H;
            *image_w  = (*image_w > MIN_W) ? (*image_w) : (MIN_W);
            *image_h  = (*image_h > MIN_H) ? (*image_h) : (MIN_H);
            *crop_w   = rand() % (*image_w);
            *crop_h   = rand() % (*image_h);
            *crop_w   = (*crop_w > MIN_W) ? (*crop_w) : (MIN_W);
            *crop_h   = (*crop_h > MIN_H) ? (*crop_h) : (MIN_H);
            *resize_w = MIN_W;
            *resize_h = MIN_H;
            *image_n  = rand() % 4 + 1;
            *image_c  = 3;
            break;
        }
        case (MIN_TO_RAND): {
            *image_w  = MIN_W;
            *image_h  = MIN_H;
            *crop_w   = *image_w;
            *crop_h   = *image_h;
            *resize_w = rand() % MAX_W;
            *resize_h = rand() % MAX_H;
            *resize_w = (*resize_w > MIN_W) ? (*resize_w) : (MIN_W);
            *resize_h = (*resize_h > MIN_H) ? (*resize_h) : (MIN_H);
            *image_n  = rand() % 4 + 1;
            *image_c  = 3;
            break;
        }
        default: {
            cout << "gen mode error" << endl;
            exit(-1);
        }
    }

    return 0;
}

#ifdef __linux__
void *test_resize_thread(void *arg) {
#else
DWORD WINAPI test_resize_thread(LPVOID arg) {
#endif
    resize_thread_arg_t *resize_thread_arg = (resize_thread_arg_t *)arg;
    int                  test_loop_times   = resize_thread_arg->trials;
    #ifdef __linux__
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << pthread_self() << std::endl;
    #else
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << GetCurrentThreadId() << std::endl;
    #endif
    std::cout << "[TEST RESIZE] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    bm_handle_t handle;
    bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
    if (dev_ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", dev_ret);
        exit(-1);
    }
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST RESIZE] LOOP " << loop_idx << "------"
                  << std::endl;
        unsigned int seed = (unsigned)time(NULL);
        // seed              = 1569487766;
        cout << "seed: " << seed << endl;
        srand(seed);
        int image_num       = 4;
        int image_channel   = 3;
        int test_img_width  = constant_test_img_width;
        int test_img_height = constant_test_img_height;
        int crop_w          = constant_crop_w;
        int crop_h          = constant_crop_h;
        int resize_w        = constant_resize_w;
        int resize_h        = constant_resize_h;
        cout << "---------RESIZE CLASSICAL SIZE TEST----------" << endl;
        cout << "[RESIZE TEST] single roi: 1n_int8->1n_int8 starts" << endl;
        resize_test_rand<uint8_t, uint8_t>(handle,
                                           DATA_TYPE_EXT_1N_BYTE,
                                           DATA_TYPE_EXT_1N_BYTE,

                                           test_img_width,
                                           test_img_height,
                                           crop_w,
                                           crop_h,
                                           resize_w,
                                           resize_h,
                                           image_num,
                                           image_channel);
        cout << "[RESIZE TEST] single roi: 1n_fp32->1n_fp32 starts" << endl;
        resize_test_rand<float, float>(handle,
                                       DATA_TYPE_EXT_FLOAT32,
                                       DATA_TYPE_EXT_FLOAT32,

                                       test_img_width,
                                       test_img_height,
                                       crop_w,
                                       crop_h,
                                       resize_w,
                                       resize_h,
                                       image_num,
                                       image_channel);
        cout << "[RESIZE TEST] single roi: 4n_int8->4n_int8 starts" << endl;
        resize_test_rand<float, float>(handle,
                                       DATA_TYPE_EXT_4N_BYTE,
                                       DATA_TYPE_EXT_4N_BYTE,

                                       test_img_width,
                                       test_img_height,
                                       crop_w,
                                       crop_h,
                                       resize_w,
                                       resize_h,
                                       image_num,
                                       image_channel);
        cout << "[RESIZE TEST] single roi: 4n_int8->1n_int8 starts" << endl;
        resize_test_rand<uint8_t, uint8_t>(handle,
                                           DATA_TYPE_EXT_4N_BYTE,
                                           DATA_TYPE_EXT_1N_BYTE,
                                           test_img_width,
                                           test_img_height,
                                           crop_w,
                                           crop_h,
                                           resize_w,
                                           resize_h,
                                           image_num,
                                           image_channel,
                                           1);
        cout << "[RESIZE TEST] multi roi: 4n_int8->1n_int8 starts" << endl;
        resize_test_rand<uint8_t, uint8_t>(handle,
                                           DATA_TYPE_EXT_4N_BYTE,
                                           DATA_TYPE_EXT_1N_BYTE,
                                           test_img_width,
                                           test_img_height,
                                           crop_w,
                                           crop_h,
                                           resize_w,
                                           resize_h,
                                           image_num,
                                           image_channel,
                                           1,
                                           1);
        cout << "[RESIZE TEST] multi roi: 1n_int8->1n_int8 starts" << endl;
        resize_test_rand<uint8_t, uint8_t>(handle,
                                           DATA_TYPE_EXT_1N_BYTE,
                                           DATA_TYPE_EXT_1N_BYTE,

                                           test_img_width,
                                           test_img_height,
                                           crop_w,
                                           crop_h,
                                           resize_w,
                                           resize_h,
                                           image_num,
                                           image_channel,
                                           0,
                                           1);
        cout << "[RESIZE TEST] multi roi: 1n_fp32->1n_fp32 starts" << endl;
        resize_test_rand<float, float>(handle,
                                       DATA_TYPE_EXT_FLOAT32,
                                       DATA_TYPE_EXT_FLOAT32,
                                       test_img_width,
                                       test_img_height,

                                       crop_w,
                                       crop_h,
                                       resize_w,
                                       resize_h,
                                       image_num,
                                       image_channel,
                                       0,
                                       1);
        cout
            << "[RESIZE TEST] multi roi(image 1, c 3): 1n_int8->1n_int8 starts "
            << endl;
        resize_test_rand<uint8_t, uint8_t>(handle,
                                           DATA_TYPE_EXT_1N_BYTE,
                                           DATA_TYPE_EXT_1N_BYTE,
                                           test_img_width,
                                           test_img_height,
                                           crop_w,
                                           crop_h,
                                           resize_w,
                                           resize_h,
                                           1,
                                           image_channel,
                                           0,
                                           1);
        cout
            << "[RESIZE TEST] multi roi(image 1, c 1): 1n_int8->1n_int8 starts "
            << endl;
        resize_test_rand<uint8_t, uint8_t>(handle,
                                           DATA_TYPE_EXT_1N_BYTE,
                                           DATA_TYPE_EXT_1N_BYTE,
                                           test_img_width,
                                           test_img_height,
                                           crop_w,
                                           crop_h,
                                           resize_w,
                                           resize_h,
                                           1,
                                           1,
                                           0,
                                           1);
        cout
            << "[RESIZE TEST] multi roi(image 1, c 3): 1n_fp32->1n_fp32 starts "
            << endl;
        resize_test_rand<float, float>(handle,
                                       DATA_TYPE_EXT_FLOAT32,
                                       DATA_TYPE_EXT_FLOAT32,

                                       test_img_width,
                                       test_img_height,
                                       crop_w,
                                       crop_h,
                                       resize_w,
                                       resize_h,
                                       1,
                                       image_channel,
                                       0,
                                       1);
#ifndef USING_CMODEL
        cout << "[RESIZE TEST] vpp crop one: 1n_int8->1n_int8 starts" << endl;
        resize_test_rand<uint8_t, uint8_t>(handle,
                                           DATA_TYPE_EXT_1N_BYTE,
                                           DATA_TYPE_EXT_1N_BYTE,

                                           test_img_width,
                                           test_img_height,
                                           crop_w,
                                           crop_h,
                                           resize_w,
                                           resize_h,
                                           image_num,
                                           image_channel,
                                           0,
                                           1,
                                           VPP_CROP_ONE);
        cout << "[RESIZE TEST] vpp crop mul: 1n_int8->1n_int8 starts" << endl;
        resize_test_rand<uint8_t, uint8_t>(handle,
                                           DATA_TYPE_EXT_1N_BYTE,
                                           DATA_TYPE_EXT_1N_BYTE,

                                           test_img_width,
                                           test_img_height,
                                           crop_w,
                                           crop_h,
                                           resize_w,
                                           resize_h,
                                           image_num,
                                           image_channel,
                                           0,
                                           0,
                                           VPP_CROP_MUL);
#endif
        cout << "----------RESIZE CORNER TEST---------" << endl;
        if(is_random){
            int loop_num = 2;
            for (int loop_idx = 0; loop_idx < loop_num; loop_idx++) {
                for (int rand_mode = 0; rand_mode < MAX_RAND_MODE; rand_mode++) {
                    gen_test_size(&test_img_width,
                                &test_img_height,
                                &crop_w,
                                &crop_h,
                                &resize_w,
                                &resize_h,
                                &image_num,
                                &image_channel,
                                rand_mode);
                    // test_img_width = 64;
                    // test_img_height = 1221;
                    // crop_w = 64;
                    // crop_h = 694;
                    // resize_w = 64;
                    // resize_h = 64;
                    // image_num = 4;
                    // image_channel = 3;

                    cout << "rand mode : " << rand_mode
                        << " ,img_w: " << test_img_width
                        << " ,img_h: " << test_img_height << " ,crop_w: " << crop_w
                        << ", crop_h: " << crop_h << ", resize_w: " << resize_w
                        << ", resize_h: " << resize_h << ", image_n: " << image_num
                        << ", image_c: " << image_channel << endl;

                    cout << "[RESIZE TEST] single roi: 1n_int8->1n_int8 starts"
                        << endl;
                    resize_test_rand<uint8_t, uint8_t>(handle,
                                                    DATA_TYPE_EXT_1N_BYTE,
                                                    DATA_TYPE_EXT_1N_BYTE,
                                                    test_img_width,
                                                    test_img_height,
                                                    crop_w,
                                                    crop_h,
                                                    resize_w,
                                                    resize_h,
                                                    image_num,
                                                    image_channel);
                    cout << "[RESIZE TEST] single roi: 1n_fp32->1n_fp32 starts"
                        << endl;
                    resize_test_rand<float, float>(handle,
                                                DATA_TYPE_EXT_FLOAT32,
                                                DATA_TYPE_EXT_FLOAT32,
                                                test_img_width,
                                                test_img_height,
                                                crop_w,
                                                crop_h,
                                                resize_w,
                                                resize_h,
                                                image_num,
                                                image_channel);
                    cout << "[RESIZE TEST] single roi: 4n_int8->4n_int8 starts"
                        << endl;
                    resize_test_rand<float, float>(handle,
                                                DATA_TYPE_EXT_4N_BYTE,
                                                DATA_TYPE_EXT_4N_BYTE,
                                                test_img_width,
                                                test_img_height,
                                                crop_w,
                                                crop_h,
                                                resize_w,
                                                resize_h,
                                                4,
                                                image_channel);
                    cout << "[RESIZE TEST] single roi: 4n_int8->1n_int8 starts"
                        << endl;
                    resize_test_rand<uint8_t, uint8_t>(handle,
                                                    DATA_TYPE_EXT_4N_BYTE,
                                                    DATA_TYPE_EXT_1N_BYTE,

                                                    test_img_width,
                                                    test_img_height,
                                                    crop_w,
                                                    crop_h,
                                                    resize_w,
                                                    resize_h,
                                                    4,
                                                    image_channel,
                                                    1);
                    cout << "[RESIZE TEST] multi roi: 4n_int8->1n_int8 starts"
                        << endl;
                    resize_test_rand<uint8_t, uint8_t>(handle,
                                                    DATA_TYPE_EXT_4N_BYTE,
                                                    DATA_TYPE_EXT_1N_BYTE,

                                                    test_img_width,
                                                    test_img_height,
                                                    crop_w,
                                                    crop_h,
                                                    resize_w,
                                                    resize_h,
                                                    4,
                                                    image_channel,
                                                    1,
                                                    1);
                    cout << "[RESIZE TEST] multi roi: 1n_int8->1n_int8 starts"
                        << endl;
                    resize_test_rand<uint8_t, uint8_t>(handle,
                                                    DATA_TYPE_EXT_1N_BYTE,
                                                    DATA_TYPE_EXT_1N_BYTE,

                                                    test_img_width,
                                                    test_img_height,
                                                    crop_w,
                                                    crop_h,
                                                    resize_w,
                                                    resize_h,
                                                    image_num,
                                                    image_channel,
                                                    0,
                                                    1);
                    cout << "[RESIZE TEST] multi roi: 1n_fp32->1n_fp32 starts"
                        << endl;
                    resize_test_rand<float, float>(handle,
                                                DATA_TYPE_EXT_FLOAT32,
                                                DATA_TYPE_EXT_FLOAT32,

                                                test_img_width,
                                                test_img_height,
                                                crop_w,
                                                crop_h,
                                                resize_w,
                                                resize_h,
                                                image_num,
                                                image_channel,
                                                0,
                                                1);
                }
            }
        }
        cout << "----------RESIZE TEST OVER---------" << endl;
    }
    bm_dev_free(handle);
    std::cout << "------[TEST RESIZE] ALL TEST PASSED!" << std::endl;
    return NULL;
}

int main(int argc, char **argv) {
    int test_loop_times  = 1;
    int test_threads_num = 1;
    if(argc > 11)
        std::cout << "command input error, please follow this\n"
                    "order:test_resize loop_num multi_thread_num in_w in_h crop_w crop_h out_w out_h is_random devid\n"
                << std::endl;
    if(argc > 10)
        dev_id = atoi(argv[10]);
    if(argc > 9)
        is_random = atoi(argv[9]);
    if(argc > 8){
        constant_test_img_width = atoi(argv[3]);
        constant_test_img_height = atoi(argv[4]);
        constant_crop_w = atoi(argv[5]);
        constant_crop_h = atoi(argv[6]);
        constant_resize_w = atoi(argv[7]);
        constant_resize_h = atoi(argv[8]);
    }
    if(argc > 2)
        test_threads_num = atoi(argv[2]);
    if(argc > 1)
        test_loop_times  = atoi(argv[1]);

    printf("input parameter:\nloop_num(%d) multi_thread_num(%d) in_w(%d) in_h(%d) crop_w(%d) crop_h(%d) out_w(%d) out_h(%d) is_random(%d) devid(%d)\n\n", 
                test_loop_times, test_threads_num, constant_test_img_width, constant_test_img_height, constant_crop_w, constant_crop_h, constant_resize_w,
                constant_resize_h, is_random, dev_id);

    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST RESIZE] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    if (test_threads_num > 4 || test_threads_num < 1) {
        std::cout << "[TEST RESIZE] thread nums should be 1~4 " << std::endl;
        exit(-1);
    }
    #ifdef __linux__
    pthread_t *          pid = new pthread_t[test_threads_num];
    resize_thread_arg_t *resize_thread_arg =
        new resize_thread_arg_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        resize_thread_arg[i].trials = test_loop_times;  // dev_id 0
        if (pthread_create(
                &pid[i], NULL, test_resize_thread, &resize_thread_arg[i])) {
            delete[] pid;
            delete[] resize_thread_arg;
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int ret = 0;
    for (int i = 0; i < test_threads_num; i++) {
        ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            delete[] pid;
            delete[] resize_thread_arg;
            perror("Thread join failed");
            exit(-1);
        }
    }
    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    delete[] pid;
    delete[] resize_thread_arg;
    #else
    #define THREAD_NUM 64
    DWORD  dwThreadIdArray[THREAD_NUM];
    HANDLE  hThreadArray[THREAD_NUM];
    resize_thread_arg_t *resize_thread_arg =
        new resize_thread_arg_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        resize_thread_arg[i].trials = test_loop_times;
        hThreadArray[i] = CreateThread(
            NULL,                   // default security attributes
            0,                      // use default stack size
            test_resize_thread,     // thread function name
            &resize_thread_arg[i],  // argument to thread function
            0,                      // use default creation flags
            &dwThreadIdArray[i]);   // returns the thread identifier
        if (hThreadArray[i] == NULL) {
            delete[] resize_thread_arg;
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
        delete[] resize_thread_arg;
        printf("Thread join failed\n");
        return -1;
    }
    for (int i = 0; i < test_threads_num; i++)
        CloseHandle(hThreadArray[i]);
    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    delete[] resize_thread_arg;
    #endif

    return 0;
}
