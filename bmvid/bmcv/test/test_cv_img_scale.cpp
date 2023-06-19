#include <iostream>
#include <vector>
#include <cmath>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include "bmcv_api.h"
#include "bmcv_api_ext.h"
#include "test_misc.h"

#ifdef __linux__
  #include <unistd.h>
  #include <sys/time.h>
#else
  #include <windows.h>
#endif

using namespace std;

typedef int bm_resize_data_type_t;
typedef struct {
    u64 img_addr;
    int width;
    int height;
    int width_stride;
} bm_img_info_t;
typedef struct {
    float alpha;
    float beta;
} convert_to_arg_t;

typedef struct {
    int trials;
} img_scale_thread_arg_t;

#define BM_W_SHIFT_VAL (7)
#define MAX_ROI_PER_IMAGE (32)
#define W_STRIDE_MAX_BITS (12)
#define W_SHIFT_MAX_BITS (4)
#define W_STRIDE_MAX_VAL (1 << W_STRIDE_MAX_BITS)
#define W_SHIFT_MAX_VAL (1 << W_SHIFT_MAX_BITS)

typedef enum { IMG_TEST_MODE = 0, RAND_TEST_MODE, MAX_TEST_MODE } test_mode_e;

typedef enum { NOPADDING = 0, PADDINGH = 1, PADDINGW = 2 } paddingmode_e;

const int channel_num = 3;
const int crop_w      = 711;
const int crop_h      = 400;
//// const int crop_w = 720;
//// const int crop_h = 400;
//// const int img_scale_w = 720;
//// const int img_scale_h = 400;
// const int img_scale_w = 7680;
// const int img_scale_h = 2160;
// const int crop_w = 7680;
// const int crop_h = 2160;
// const int crop_w = 720;
// const int crop_h = 400;
// const int img_scale_w = 720;
// const int img_scale_h = 400;
const int crop_start_x = 0;
const int crop_start_y = 0;

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

static u64
allocate_dev_mem(bm_handle_t handle, bm_device_mem_t *dst_dev_mem, int size) {
    bm_malloc_device_byte(handle, dst_dev_mem, size);
    return bm_mem_get_device_addr(*dst_dev_mem);
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
static int image_padding_ref_kernel(
    T *output_buf, int src_w, int src_h, int *dst_w, int *dst_h) {

    return 0;
}

template <typename T>
static int image_padding_ref(T ** new_output_buf,
                             int  image_num,
                             int  src_w,
                             int  src_h,
                             int *dst_w,
                             int *dst_h,
                             int  out_data_type,
                             int  image_format,
                             T    padding_b,
                             T    padding_g,
                             T    padding_r) {
    float hratio   = (float)*dst_h / (float)src_h;
    float wratio   = (float)*dst_w / (float)src_w;
    int   margin_h = 0;
    int   margin_w = 0;
    if (out_data_type == DATA_TYPE_EXT_4N_BYTE) {
        u32 temp_padding_r = (u32)padding_r;
        u32 temp_padding_g = (u32)padding_g;
        u32 temp_padding_b = (u32)padding_b;
        temp_padding_r =
            (((temp_padding_r << 24) & 0xFF000000) +
             ((temp_padding_r << 16) & 0x00FF0000) +
             ((temp_padding_r << 8) & 0x0000FF00) + temp_padding_r);
        temp_padding_g =
            (((temp_padding_g << 24) & 0xFF000000) +
             ((temp_padding_g << 16) & 0x00FF0000) +
             ((temp_padding_g << 8) & 0x0000FF00) + temp_padding_g);
        temp_padding_b =
            (((temp_padding_b << 24) & 0xFF000000) +
             ((temp_padding_b << 16) & 0x00FF0000) +
             ((temp_padding_b << 8) & 0x0000FF00) + temp_padding_b);
        memcpy(&padding_r, &temp_padding_r, sizeof(T));
        memcpy(&padding_g, &temp_padding_g, sizeof(T));
        memcpy(&padding_b, &temp_padding_b, sizeof(T));
    }
    T * output_buf   = *new_output_buf;
    int dst_w_stride = *dst_w;
    int dst_len      = (*dst_w) * (*dst_h);
    if (hratio > wratio) {
        int output_h = lrint((float)src_h * wratio);
        margin_h     = (*dst_h - output_h + 1) / 2;
        *dst_h       = output_h;
    } else {
        int output_w = lrint((float)src_w * hratio);
        margin_w     = (*dst_w - output_w + 1) / 2;
        *dst_w       = output_w;
    }
    for (int n_idx = 0; n_idx < image_num; n_idx++) {
        T *tmp_output_buf = output_buf + n_idx * 3 * dst_len;
        if (FORMAT_BGR_PLANAR == image_format) {
            for (int i = 0; i < dst_len; i++) {
                tmp_output_buf[i] = padding_b;
            }
            tmp_output_buf = tmp_output_buf + dst_len;
            for (int i = 0; i < dst_len; i++) {
                tmp_output_buf[i] = padding_g;
            }
            tmp_output_buf = tmp_output_buf + dst_len;
            for (int i = 0; i < dst_len; i++) {
                tmp_output_buf[i] = padding_r;
            }
        } else if (FORMAT_RGB_PLANAR == image_format) {
            for (int i = 0; i < dst_len; i++) {
                tmp_output_buf[i] = padding_r;
            }
            tmp_output_buf = tmp_output_buf + dst_len;
            for (int i = 0; i < dst_len; i++) {
                tmp_output_buf[i] = padding_g;
            }
            tmp_output_buf = tmp_output_buf + dst_len;
            for (int i = 0; i < dst_len; i++) {
                tmp_output_buf[i] = padding_b;
            }
        } else {
            printf("image format not support!\n");
            exit(-1);
        }
    }
    *new_output_buf = *new_output_buf + margin_w + margin_h * dst_w_stride;

    return 0;
}

template <typename T, typename T1>
static int img_scale_ref(T *            old_img_buf,
                         T1 *           new_img_buf,
                         bmcv_resize_t *img_scale_img_attr,
                         int            image_num,
                         int            channel_num,
                         int            width_stride,
                         int            height_stride,
                         int            new_width_stride,
                         int            new_height_stride,
                         int            if_4N_to_1N,
                         int            total_roi_nums = 0,
                         vector<int>    roi_vec        = vector<int>()) {
    T *temp_old_buf =
        new T[image_num * channel_num * width_stride * height_stride];
    T *flatten_old_buf =
        new T[total_roi_nums * channel_num * width_stride * height_stride];
    memcpy(temp_old_buf,
           old_img_buf,
           image_num * channel_num * width_stride * height_stride * sizeof(T));
    if (if_4N_to_1N) {
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
    } else {
        delete[] flatten_old_buf;
    }

    image_num = (total_roi_nums > 0) ? (total_roi_nums) : (image_num);
    for (int image_idx = 0; image_idx < image_num; image_idx++) {
        float scale_w = (float)(img_scale_img_attr[image_idx].in_width) /
                        (float)img_scale_img_attr[image_idx].out_width;
        float scale_h = (float)(img_scale_img_attr[image_idx].in_height) /
                        (float)img_scale_img_attr[image_idx].out_height;
        int scale_para = 0;
        // int new_width = resize_img_attr[image_idx].out_width;
        // int new_height = resize_img_attr[image_idx].out_height;
        // int scale_para_fix = (int)(scale_w * scale_para);
        int scale_para_fix = 0;
        int w_shift        = 0;
        dynamic_get_w_info(scale_w, if_4N_to_1N, scale_para_fix, w_shift);
        scale_para  = 1 << w_shift;
        int start_x = img_scale_img_attr[image_idx].start_x;
        int start_y = img_scale_img_attr[image_idx].start_y;
        for (int channel_idx = 0; channel_idx < channel_num; channel_idx++) {
            for (int j = 0; j < img_scale_img_attr[image_idx].out_height; j++) {
                int h_offset = (int)(j * scale_h);
                h_offset = (h_offset < img_scale_img_attr[image_idx].in_height)
                               ? (h_offset)
                               : (img_scale_img_attr[image_idx].in_height - 1);
                for (int i = 0; i < img_scale_img_attr[image_idx].out_width;
                     i++) {
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
    delete[] temp_old_buf;

    return 0;
}

template <typename T0, typename T1>
int32_t convert_to_ref(T0 *              src,
                       T1 *              dst,
                       convert_to_arg_t *convert_to_arg,
                       int               image_num,
                       int               image_channel,
                       int               image_h,
                       int               image_w,
                       int               convert_format) {
    if (CONVERT_4N_TO_4N == convert_format) {
        image_num = (image_num + 3) / 4;
        image_w   = image_w * 4;
    }
    T0 *temp_old_buf = new T0[image_num * image_channel * image_w * image_h];
    memcpy(temp_old_buf,
           src,
           sizeof(T0) * image_num * image_channel * image_w * image_h);
    for (int32_t n_idx = 0; n_idx < image_num; n_idx++) {
        for (int32_t c_idx = 0; c_idx < image_channel; c_idx++) {
            for (int32_t y = 0; y < image_h; y++) {
                for (int32_t x = 0; x < image_w; x++) {
                    int check_idx =
                        (n_idx * image_channel + c_idx) * image_w * image_h +
                        y * image_w + x;
                    float temp = 0.0;
                    temp       = ((float)temp_old_buf[check_idx]) *
                               convert_to_arg[c_idx].alpha +
                           convert_to_arg[c_idx].beta;
                    temp = ((1 == sizeof(T1)) && (temp > 255)) ? (255) : (temp);
                    if (is_same<T1, float>::value) {
                        dst[check_idx] = (T1)(temp);
                    } else {
                        dst[check_idx] = (T1)(lrint(temp));
                    }
                }
            }
        }
    }
    delete[] temp_old_buf;

    return 0;
}

template <typename T, typename T1>
static int bmcv_img_scale_cmp(T1 *p_exp,
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
    float             scale_w        = (float)(resize_in_w) / (float)image_dw;
    float             scale_h        = (float)(resize_in_h) / (float)image_dh;
    float             data_threshold = 0.001;

    printf("cmp start!\n");
    for (int n_idx = 0; n_idx < image_num; n_idx++) {
        for (int c_idx = 0; c_idx < image_channel; c_idx++) {
            for (int y = 0; y < image_dh; y++) {
                for (int x = 0; x < image_dw; x++) {
                    int check_idx =
                        (n_idx * image_channel + c_idx) * image_dw * image_dh +
                        y * image_dw + x;
                    T1 got = p_got[check_idx];
                    T1 exp = p_exp[check_idx];
                    if ((got - exp > 0.01) || (exp - got > 0.01)) {
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
                                if ((got - src[find_index] > data_threshold) ||
                                    (src[find_index] - got > data_threshold)) {
                                    std::cout
                                        << "h: " << y << " w: " << x
                                        << " c: " << c_idx << " n: " << n_idx
                                        << " got: " << (int)(got)
                                        << " exp: " << (int)(exp) << std::endl;

                                    return -1;
                                }
                            }
                        }
                        find_table.clear();
                    }
                }
            }
        }
    }
    return 0;
}

template <typename T, typename T1>
static int img_scale_test_rand(bm_handle_t      handle,
                               bm_image_format  image_format,
                               bmcv_data_format data_format,
                               int              image_num,
                               int              image_channel,
                               int              img_scale_h,
                               int              img_scale_w) {
    bmcv_image input, output;
    // const int test_img_width = 7680;
    // const int test_img_height = 2160;
    const int test_img_width  = 1920;
    const int test_img_height = 1080;
    // const int test_img_width = 720;
    // const int test_img_height = 400;

    int image_len =
        image_num * image_channel * test_img_width * test_img_height;
    int img_scale_image_len =
        image_num * image_channel * img_scale_w * img_scale_h;
    T * img_data     = new T[image_len];
    T * img_res_data = new T[image_len];
    T * img_ref_data = new T[image_len];
    T1 *res_data     = new T1[img_scale_image_len];
    T1 *ref_data     = new T1[img_scale_image_len];
    memset(img_data, 0x0, image_len);
    memset(res_data, 0x0, img_scale_image_len);
    memset(ref_data, 0x0, img_scale_image_len);
    for (int i = 0; i < image_len; i++) {
        img_data[i] = rand() % 255;
    }
    memcpy(img_res_data, img_data, image_len * sizeof(T));
    memcpy(img_ref_data, img_data, image_len * sizeof(T));
    input.image_width   = test_img_width;
    input.image_height  = test_img_height;
    input.image_format  = image_format;
    input.data_format   = data_format;
    input.color_space   = COLOR_RGB;
    input.data[0]       = bm_mem_from_system(img_res_data);
    output.image_width  = img_scale_w;
    output.image_height = img_scale_h;
    output.image_format = image_format;
    output.data_format  = data_format;
    output.color_space  = COLOR_RGB;
    output.data[0]      = bm_mem_from_system(res_data);
    int do_crop         = rand() % 2;
    int top             = rand() % abs(crop_h - img_scale_h);
    int left            = rand() % abs(crop_w - img_scale_w);
    // int top = 0;
    // int left = 0;
    int              stretch_fit = rand() % 2;
    int              padding_r   = rand() % 255;
    int              padding_g   = rand() % 255;
    int              padding_b   = rand() % 255;
    convert_to_arg_t convert_to_arg[3];
    for (int i = 0; i < 3; i++) {
        convert_to_arg[i].alpha = ((float)(rand() % 20)) / (float)10;
        convert_to_arg[i].beta  = ((float)(rand() % 20)) / (float)10;
    }
    if ((BGR4N == input.image_format) || (RGB4N == input.image_format)) {
        image_num = ALIGN(image_num, 4) / 4;
    }
    int   pixel_weight_bias = rand() % 2;
    float weight_b          = 1;
    float bias_b            = 0;
    float weight_g          = 1;
    float bias_g            = 0;
    float weight_r          = 1;
    float bias_r            = 0;
    if ((BGR == input.image_format) || (BGR4N == input.image_format)) {
        weight_b = convert_to_arg[0].alpha;
        bias_b   = convert_to_arg[0].beta;
        weight_g = convert_to_arg[1].alpha;
        bias_g   = convert_to_arg[1].beta;
        weight_r = convert_to_arg[2].alpha;
        bias_r   = convert_to_arg[2].beta;
    } else {
        weight_r = convert_to_arg[0].alpha;
        bias_r   = convert_to_arg[0].beta;
        weight_g = convert_to_arg[1].alpha;
        bias_g   = convert_to_arg[1].beta;
        weight_b = convert_to_arg[2].alpha;
        bias_b   = convert_to_arg[2].beta;
    }
    bmcv_img_scale(handle,
                   input,
                   image_num,
                   do_crop,
                   top,
                   left,
                   crop_h,
                   crop_w,
                   stretch_fit,
                   padding_b,
                   padding_g,
                   padding_r,
                   pixel_weight_bias,
                   weight_b,
                   bias_b,
                   weight_g,
                   bias_g,
                   weight_r,
                   bias_r,
                   output);
    if (pixel_weight_bias) {
        cout << "weight_r: " << weight_r << " beta_r: " << bias_r
             << "weight_g: " << weight_g << " beta_g: " << bias_g
             << "weight_b: " << weight_b << " beta_b: " << bias_b << endl;
        if ((BGR4N == input.image_format) || (RGB4N == input.image_format)) {
            u8 *temp_src       = (u8 *)img_ref_data;
            u8 *temp_dst       = (u8 *)img_ref_data;
            int convert_format = CONVERT_4N_TO_4N;
            convert_to_ref<u8, u8>(temp_src,
                                   temp_dst,
                                   convert_to_arg,
                                   image_num,
                                   image_channel,
                                   input.image_height,
                                   input.image_width,
                                   convert_format);
        } else {
            int convert_format = CONVERT_1N_TO_1N;
            convert_to_ref<T1, T1>(img_ref_data,
                                   img_ref_data,
                                   convert_to_arg,
                                   image_num,
                                   image_channel,
                                   input.image_height,
                                   input.image_width,
                                   convert_format);
        }
    }

    T1 *          new_output_buf = ref_data;
    int           dst_w          = output.image_width;
    int           dst_h          = output.image_height;
    #ifdef __linux__
    bmcv_resize_t img_scale_img_attr[image_num];
    #else
    std::shared_ptr<bmcv_resize_t> img_scale_img_attr_(new bmcv_resize_t[image_num], std::default_delete<bmcv_resize_t[]>());
    bmcv_resize_t*                 img_scale_img_attr = img_scale_img_attr_.get();
    #endif
    if (do_crop) {
        cout << "top: " << top << " left: " << left << endl;
    }
    for (int img_idx = 0; img_idx < image_num; img_idx++) {
        img_scale_img_attr[img_idx].start_x = 0;
        img_scale_img_attr[img_idx].start_y = 0;
        if (do_crop) {
            img_scale_img_attr[img_idx].start_x   = left;
            img_scale_img_attr[img_idx].start_y   = top;
            img_scale_img_attr[img_idx].in_width  = crop_w;
            img_scale_img_attr[img_idx].in_height = crop_h;
        } else {
            img_scale_img_attr[img_idx].in_width  = input.image_width;
            img_scale_img_attr[img_idx].in_height = input.image_height;
        }
        for (int img_idx = 0; img_idx < image_num; img_idx++) {
            img_scale_img_attr[img_idx].out_width  = dst_w;
            img_scale_img_attr[img_idx].out_height = dst_h;
        }
    }
    if (!stretch_fit) {
        cout << "padding_r: " << padding_r << " padding_b: " << padding_b
             << " padding_g: " << padding_g << endl;
        int out_data_format = DATA_TYPE_EXT_1N_BYTE;
        if (((data_format == DATA_TYPE_BYTE) && (image_format == BGR4N)) ||
            ((data_format == DATA_TYPE_BYTE) && (image_format == RGB4N))) {
            out_data_format = DATA_TYPE_EXT_4N_BYTE;
        } else if (data_format == DATA_TYPE_BYTE) {
            out_data_format = DATA_TYPE_EXT_1N_BYTE;
        } else if (data_format == DATA_TYPE_FLOAT) {
            out_data_format = DATA_TYPE_EXT_FLOAT32;
        }
        int out_image_format = FORMAT_BGR_PLANAR;
        if ((RGB == image_format) || (RGB4N == image_format)) {
            out_image_format = FORMAT_RGB_PLANAR;
        } else if ((BGR == image_format) || (BGR4N == image_format)) {
            out_image_format = FORMAT_BGR_PLANAR;
        }
        image_padding_ref<T1>(&new_output_buf,
                              image_num,
                              img_scale_img_attr[0].in_width,
                              img_scale_img_attr[0].in_height,
                              &dst_w,
                              &dst_h,
                              out_data_format,
                              out_image_format,
                              (T1)padding_b,
                              (T1)padding_g,
                              (T1)padding_r);
        for (int img_idx = 0; img_idx < image_num; img_idx++) {
            img_scale_img_attr[img_idx].out_width  = dst_w;
            img_scale_img_attr[img_idx].out_height = dst_h;
        }
    }
    img_scale_ref<T, T1>((T *)img_ref_data,
                         (T1 *)new_output_buf,
                         img_scale_img_attr,
                         image_num,
                         image_channel,
                         input.image_width,
                         input.image_height,
                         output.image_width,
                         output.image_height,
                         0);

    int ret = bmcv_img_scale_cmp<T, T1>(ref_data,
                                        res_data,
                                        img_ref_data,
                                        image_num,
                                        image_channel,
                                        test_img_height,
                                        test_img_width,
                                        // img_scale_img_attr[0].in_height,
                                        // img_scale_img_attr[0].in_width,
                                        output.image_height,
                                        output.image_width,
                                        img_scale_img_attr[0].in_height,
                                        img_scale_img_attr[0].in_width);

    if (ret < 0) {
        printf("[IMG SCALE]compare failed !\r\n");
        delete[] img_data;
        delete[] img_res_data;
        delete[] img_ref_data;
        delete[] res_data;
        delete[] ref_data;
        exit(-1);
    } else {
        printf("compare passed\r\n");
    }
    delete[] img_data;
    delete[] img_res_data;
    delete[] img_ref_data;
    delete[] res_data;
    delete[] ref_data;

    return 0;
}

#ifdef __linux__
void *test_img_scale_thread(void *arg) {
#else
DWORD WINAPI test_img_scale_thread(LPVOID arg) {
#endif
    img_scale_thread_arg_t *img_scale_thread_arg =
        (img_scale_thread_arg_t *)arg;
    int test_loop_times = img_scale_thread_arg->trials;
    #ifdef __linux__
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << pthread_self() << std::endl;
    #else
    std::cout << "------MULTI THREAD TEST STARTING----------thread id is "
              << GetCurrentThreadId() << std::endl;
    #endif
    std::cout << "[TEST IMG SCALE] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST IMG SCALE] LOOP " << loop_idx << "------"
                  << std::endl;
        bm_handle_t  handle;
        unsigned int seed = (unsigned)time(NULL);
        // seed = 1567772785;
        cout << "seed: " << seed << endl;
        srand(seed);
        int         dev_id = 0;
        bm_status_t ret    = bm_dev_request(&handle, dev_id);
        if (ret != BM_SUCCESS) {
            printf("Create bm handle failed. ret = %d\n", ret);
            exit(-1);
        }
        for (int loop_idx = 0; loop_idx < 5; loop_idx++) {
            int   image_num      = 1 + (rand() % 4);
            int   image_channel  = 3;
            int   img_scale_w    = 200;
            int   img_scale_h    = 300;
            float scale_factor_w = (float)((rand() % 9) + 1) / 10;
            float scale_factor_h = (float)((rand() % 9) + 1) / 10;
            img_scale_h          = (int)(crop_h * scale_factor_h);
            img_scale_w          = (int)(crop_w * scale_factor_w);
            if (img_scale_h == 0) {
                img_scale_h = crop_h / 2;
            }
            if (img_scale_w == 0) {
                img_scale_w = crop_w / 2;
            }
            cout << "[IMG_SCALE TEST]: 1n_int8->1n_int8 starts: n: "
                 << image_num << ", img_scale_w:" << img_scale_w
                 << ", img_scale_h:" << img_scale_h << endl;
            img_scale_test_rand<uint8_t, uint8_t>(handle,
                                                  BGR,
                                                  DATA_TYPE_BYTE,
                                                  image_num,
                                                  image_channel,
                                                  img_scale_h,
                                                  img_scale_w);
            cout << "[IMG_SCALE TEST]: 1n_fp32->1n_fp32 starts: n: "
                 << image_num << ", img_scale_w:" << img_scale_w
                 << ", img_scale_h:" << img_scale_h << endl;
            img_scale_test_rand<float, float>(handle,
                                              BGR,
                                              DATA_TYPE_FLOAT,
                                              image_num,
                                              image_channel,
                                              img_scale_h,
                                              img_scale_w);
            cout << "[IMG_SCALE TEST]: 4n_int8->4n_int8 starts: n: "
                 << image_num << ", img_scale_w:" << img_scale_w
                 << ", img_scale_h:" << img_scale_h << endl;
            img_scale_test_rand<int, int>(handle,
                                              BGR4N,
                                              DATA_TYPE_BYTE,
                                              image_num,
                                              image_channel,
                                              img_scale_h,
                                              img_scale_w);
        }
        bm_dev_free(handle);
    }
    std::cout << "------[TEST IMG SCALE] ALL TEST PASSED!" << std::endl;

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
                     "order:test_cv_img_scale loop_num multi_thread_num"
                  << std::endl;
        exit(-1);
    }
    if (test_threads_num > 4 || test_threads_num < 1) {
        std::cout << "[TEST IMG SCALE] thread nums should be 1~4 " << std::endl;
        exit(-1);
    }
    #ifdef __linux__
    pthread_t *             pid = new pthread_t[test_threads_num];
    img_scale_thread_arg_t *img_scale_thread_arg =
        new img_scale_thread_arg_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        img_scale_thread_arg[i].trials = test_loop_times;  // dev_id 0
        if (pthread_create(&pid[i],
                           NULL,
                           test_img_scale_thread,
                           &img_scale_thread_arg[i])) {
            delete[] pid;
            delete[] img_scale_thread_arg;
            perror("create thread failed\n");
            exit(-1);
        }
    }
    int ret = 0;
    for (int i = 0; i < test_threads_num; i++) {
        ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            delete[] pid;
            delete[] img_scale_thread_arg;
            perror("Thread join failed");
            exit(-1);
        }
    }
    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    delete[] pid;
    delete[] img_scale_thread_arg;
    #else
    #define THREAD_NUM 64
    DWORD              dwThreadIdArray[THREAD_NUM];
    HANDLE             hThreadArray[THREAD_NUM];
    img_scale_thread_arg_t *img_scale_thread_arg =
        new img_scale_thread_arg_t[test_threads_num];
    for (int i = 0; i < test_threads_num; i++) {
        img_scale_thread_arg[i].trials = test_loop_times;
        hThreadArray[i] = CreateThread(
            NULL,                     // default security attributes
            0,                        // use default stack size
            test_img_scale_thread,    // thread function name
            &img_scale_thread_arg[i], // argument to thread function
            0,                        // use default creation flags
            &dwThreadIdArray[i]);     // returns the thread identifier
        if (hThreadArray[i] == NULL) {
            delete[] img_scale_thread_arg;
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
        delete[] img_scale_thread_arg;
        printf("Thread join failed\n");
        return -1;
    }
    for (int i = 0; i < test_threads_num; i++)
        CloseHandle(hThreadArray[i]);

    std::cout << "--------ALL THREADS TEST OVER---------" << std::endl;
    delete[] img_scale_thread_arg;
    #endif

    return 0;
}
