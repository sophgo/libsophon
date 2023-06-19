#include <iostream>
#include <vector>
#include "bmcv_api_ext.h"
#include "bmlib_runtime.h"
#include "test_misc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "test_cv_naive_threadpool.hpp"
#ifdef __linux__
    #include <sys/time.h>
#else
    #include <windows.h>
    #include "time.h"
#endif
static thread_local int IMAGE_N = 2;
static thread_local int IMAGE_H = 2;
static thread_local int IMAGE_W = 2;

#define BM_W_SHIFT_VAL (7)
#define BMCV_COMPARE_EPSILON (1e-4)
#define CLIP(val, max, min)                                                    \
    val = (val <= min) ? min : val;                                            \
    val = (val >= max) ? max : val

static bm_handle_t handle;
static std::shared_ptr<ThreadPool> pool = std::make_shared<ThreadPool>(1);
using namespace std;
void interleave(u8 *inout, int N, int H, int W) {
    u8 *temp = new u8[H * W * 3];
    for (int n = 0; n < N; n++) {
        u8 *start = inout + 3 * H * W * n;
        for (int h = 0; h < H; h++) {
            for (int w = 0; w < W; w++) {
                temp[3 * (h * W + w)]     = start[(h * W + w)];
                temp[3 * (h * W + w) + 1] = start[(h * W + w) + H * W];
                temp[3 * (h * W + w) + 2] = start[(h * W + w) + 2 * H * W];
            }
        }
        memcpy(start, temp, H * W * 3);
    }
}

void calculate_YUV2RGB_float(u8 y, u8 u, u8 v, float *r, float *g, float *b) {
    float r_fp, g_fp, b_fp;
    int   Y = (int)y;
    int   U = (int)u - 128;
    int   V = (int)v - 128;
    r_fp    = Y + 1.403 * V;
    g_fp    = Y - 0.344 * U - 0.714 * V;
    b_fp    = Y + 1.770 * U;
    CLIP(r_fp, 255.0, 0.0);
    CLIP(g_fp, 255.0, 0.0);
    CLIP(b_fp, 255.0, 0.0);
    *r = r_fp;
    *g = g_fp;
    *b = b_fp;
}

void calculate_YUV2RGB_u8(u8 y, u8 u, u8 v, u8 *r, u8 *g, u8 *b) {
    float fp_r, fp_g, fp_b;
    calculate_YUV2RGB_float(y, u, v, &fp_r, &fp_g, &fp_b);
    *r = (u8)fp_r;
    *g = (u8)fp_g;
    *b = (u8)fp_b;
}

void calculate_YCrCb2RGB_float(u8 y, u8 u, u8 v, float *r, float *g, float *b) {
    float r_fp, g_fp, b_fp;
    int   Y = (int)y - 16;
    int   U = (int)u - 128;
    int   V = (int)v - 128;
    r_fp    = 1.16438 * Y + 1.59603 * V;
    g_fp    = 1.16438 * Y - 0.39176 * U - 0.81297 * V;
    b_fp    = 1.16438 * Y + 2.01723 * U;
    CLIP(r_fp, 255.0, 0.0);
    CLIP(g_fp, 255.0, 0.0);
    CLIP(b_fp, 255.0, 0.0);
    *r = r_fp;
    *g = g_fp;
    *b = b_fp;
}

void calculate_YCrCb2RGB_u8(u8 y, u8 u, u8 v, u8 *r, u8 *g, u8 *b) {
    float fp_r, fp_g, fp_b;
    calculate_YCrCb2RGB_float(y, u, v, &fp_r, &fp_g, &fp_b);
    *r = (u8)fp_r;
    *g = (u8)fp_g;
    *b = (u8)fp_b;
}

void convert_4N_INT8_to_1N_INT8(u8 *inout_ptr, int n, int c, int h, int w) {
    u8 *temp_buf = new u8[4 * c * h * w];
    for (int i = 0; i < ALIGN(n, 4) / 4; i++) {
        memcpy(temp_buf,
               inout_ptr + 4 * c * h * w * i,
               4 * c * h * w * sizeof(u8));
        for (int loop = 0; loop < c * h * w; loop++) {
            inout_ptr[i * 4 * c * h * w + loop] = temp_buf[4 * loop];
            inout_ptr[i * 4 * c * h * w + 1 * c * h * w + loop] =
                temp_buf[4 * loop + 1];
            inout_ptr[i * 4 * c * h * w + 2 * c * h * w + loop] =
                temp_buf[4 * loop + 2];
            inout_ptr[i * 4 * c * h * w + 3 * c * h * w + loop] =
                temp_buf[4 * loop + 3];
        }
    }
    delete[] temp_buf;
}

int array_cmp_u8(
    u8 *p_exp, u8 *p_got, int len, const char *info_label, u8 delta) {
    int idx = 0;
    for (idx = 0; idx < len; idx++) {
        // compare abs
        if ((int)fabs(p_exp[idx] - (int)p_got[idx]) > delta) {
            printf("%s abs error at index %d exp %d got %d\n",
                   info_label,
                   idx,
                   p_exp[idx],
                   p_got[idx]);
            return -1;
        }
    }
    return 0;
}

static void gen_random_yuv_data(u8 *&  input_ptr,
                                void *&output_ptr,
                                void *&reference_ptr,
                                int *  dst_image_format,
                                int &  src_image_format,
                                int *  output_storage_mode) {
    auto engine = pool->get_random_engine();
    IMAGE_N = (1 + (*engine)() % 4);
    IMAGE_H = (1 + (*engine)() % 1024) * 2;
    IMAGE_W = (1 + (*engine)() % 512) * 2;
    IMAGE_W = ALIGN(IMAGE_W, 4);
    while (IMAGE_W % 64 == 0) {
        IMAGE_W = (1 + (*engine)() % 512) * 2;
        IMAGE_W = ALIGN(IMAGE_W, 4);
    }

    switch ((*engine)() % 3)
    // switch(0)
    {
        case 0:
            *output_storage_mode = DATA_TYPE_EXT_FLOAT32;
            break;
        case 1:
            *output_storage_mode = DATA_TYPE_EXT_4N_BYTE;
            break;
        case 2:
            *output_storage_mode = DATA_TYPE_EXT_1N_BYTE;
            break;
        default:
            *output_storage_mode = DATA_TYPE_EXT_1N_BYTE;
    }
    src_image_format = (*engine)() % 7;
    if(src_image_format == 2){
      src_image_format = 0;
    }
    int uv_format_factor = 2;
    if (src_image_format == FORMAT_NV16 ||
        src_image_format == FORMAT_NV61 ||
        src_image_format == FORMAT_YUV422P) {
        uv_format_factor = 1;
    }

    //*output_storage_mode = DATA_TYPE_EXT_4N_BYTE;
    int n_stride = IMAGE_H * IMAGE_W + IMAGE_H * IMAGE_W / uv_format_factor;
    input_ptr    = new u8[IMAGE_N * n_stride];
    *dst_image_format = ((*engine)() % 2) ? FORMAT_RGB_PLANAR : FORMAT_BGR_PLANAR;
    //*dst_image_format = FORMAT_RGB_PLANAR;
    for (int n = 0; n < IMAGE_N; n++) {
        // Init Y
        for (int i = 0; i < IMAGE_H * IMAGE_W; i++) {
            input_ptr[i + n * n_stride] = (u8)((*engine)() % 256);
        }
        // Init UV
        for (int i = 0; i < IMAGE_H * IMAGE_W / uv_format_factor; i++) {
            input_ptr[i + IMAGE_H * IMAGE_W + n * n_stride] =
                (u8)((*engine)() % 256);
        }
    }

    if (*output_storage_mode == DATA_TYPE_EXT_FLOAT32) {
        reference_ptr = malloc(IMAGE_N * IMAGE_H * IMAGE_W * 3 * sizeof(float));

        output_ptr = malloc(IMAGE_N * IMAGE_H * IMAGE_W * 3 * sizeof(float));

        for (int n = 0; n < IMAGE_N; n++) {
            for (int i = 0; i < IMAGE_H * IMAGE_W; i++) {
                u8  y, u, v;
                int h_index    = i / IMAGE_W;
                int w_index    = i % IMAGE_W;
                int uv_h_index = h_index / uv_format_factor;
                int u_w_index  = w_index / 2 * 2;
                int v_w_index  = w_index / 2 * 2 + 1;

                float *c0 = ((float *)reference_ptr) +
                            n * IMAGE_H * IMAGE_W * 3 + h_index * IMAGE_W +
                            w_index;
                float *c1 = ((float *)reference_ptr) +
                            n * IMAGE_H * IMAGE_W * 3 + IMAGE_H * IMAGE_W +
                            h_index * IMAGE_W + w_index;
                float *c2 = ((float *)reference_ptr) +
                            n * IMAGE_H * IMAGE_W * 3 + 2 * IMAGE_H * IMAGE_W +
                            h_index * IMAGE_W + w_index;
                y = input_ptr[i + n * n_stride];
                u = input_ptr[u_w_index + uv_h_index * IMAGE_W +
                              IMAGE_H * IMAGE_W + n * n_stride];
                v = input_ptr[v_w_index + uv_h_index * IMAGE_W +
                              IMAGE_H * IMAGE_W + n * n_stride];
                if (*dst_image_format == FORMAT_RGB_PLANAR)
                    calculate_YCrCb2RGB_float(y, u, v, c0, c1, c2);
                else
                    calculate_YCrCb2RGB_float(y, u, v, c2, c1, c0);
            }
        }
    } else {
        int output_n;
        if (*output_storage_mode == DATA_TYPE_EXT_4N_BYTE)
            output_n = ALIGN(IMAGE_N, 4);
        else  // STORAGE_MODE_1N_INT8
            output_n = IMAGE_N;

        reference_ptr = malloc(IMAGE_N * IMAGE_H * IMAGE_W * 3 * sizeof(u8));
        output_ptr    = malloc(output_n * IMAGE_H * IMAGE_W * 3 * sizeof(u8));

        for (int n = 0; n < IMAGE_N; n++) {
            for (int i = 0; i < IMAGE_H * IMAGE_W; i++) {
                u8  y, u, v;
                int h_index    = i / IMAGE_W;
                int w_index    = i % IMAGE_W;
                int uv_h_index = h_index / uv_format_factor;
                int u_w_index  = w_index / 2 * 2;
                int v_w_index  = w_index / 2 * 2 + 1;

                u8 *c0 = ((u8 *)reference_ptr) + n * IMAGE_H * IMAGE_W * 3 +
                         h_index * IMAGE_W + w_index;
                u8 *c1 = ((u8 *)reference_ptr) + n * IMAGE_H * IMAGE_W * 3 +
                         IMAGE_H * IMAGE_W + h_index * IMAGE_W + w_index;
                u8 *c2 = ((u8 *)reference_ptr) + n * IMAGE_H * IMAGE_W * 3 +
                         2 * IMAGE_H * IMAGE_W + h_index * IMAGE_W + w_index;
                y = input_ptr[i + n * n_stride];
                u = input_ptr[u_w_index + uv_h_index * IMAGE_W +
                              IMAGE_H * IMAGE_W + n * n_stride];
                v = input_ptr[v_w_index + uv_h_index * IMAGE_W +
                              IMAGE_H * IMAGE_W + n * n_stride];
                if (*dst_image_format == FORMAT_RGB_PLANAR)
                    calculate_YCrCb2RGB_u8(y, u, v, c0, c1, c2);
                else
                    calculate_YCrCb2RGB_u8(y, u, v, c2, c1, c0);
            }
        }
    }
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
static int resize_ref(T *              old_img_buf,
                      T *              new_img_buf,
                      bmcv_resize_t *  resize_img_attr,
                      int              image_num,
                      int              channel_num,
                      int              width_stride,
                      int              height_stride,
                      int              if_4N_to_1N,
                      int              total_roi_nums = 0,
                      std::vector<int> roi_vec        = std::vector<int>()) {
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
    }
    image_num = (total_roi_nums > 0) ? (total_roi_nums) : (image_num);
    for (int image_idx = 0; image_idx < image_num; image_idx++) {
        float scale_w = 0;
        float scale_h = 0;
        if ((resize_img_attr[image_idx].in_width ==
             (float)resize_img_attr[image_idx].out_width) &&
            (resize_img_attr[image_idx].in_height ==
             (float)resize_img_attr[image_idx].out_height)) {
            scale_w = (float)(resize_img_attr[image_idx].in_width) /
                      (float)resize_img_attr[image_idx].out_width;
            scale_h = (float)(resize_img_attr[image_idx].in_height) /
                      (float)resize_img_attr[image_idx].out_height;
        } else {
            scale_w = (float)(resize_img_attr[image_idx].in_width -
                              resize_img_attr[image_idx].start_x) /
                      (float)resize_img_attr[image_idx].out_width;
            scale_h = (float)(resize_img_attr[image_idx].in_height -
                              resize_img_attr[image_idx].start_y) /
                      (float)resize_img_attr[image_idx].out_height;
        }
        int scale_para     = 1 << BM_W_SHIFT_VAL;
        int new_width      = resize_img_attr[image_idx].out_width;
        int new_height     = resize_img_attr[image_idx].out_height;
        int scale_para_fix = 0;
        if (scale_w > 1) {
            scale_para_fix = round(scale_w * scale_para);
        } else {
            scale_para_fix = (int)(scale_w * scale_para);
        }
        scale_para_fix = (int)(scale_w * scale_para);
        int start_x    = resize_img_attr[image_idx].start_x;
        int start_y    = resize_img_attr[image_idx].start_y;
        for (int channel_idx = 0; channel_idx < channel_num; channel_idx++) {
            for (int j = 0; j < resize_img_attr[image_idx].out_height; j++) {
                for (int i = 0; i < resize_img_attr[image_idx].out_width; i++) {
                    int scale_h_fix_in_loop = (scale_h > 1)
                                                  ? ((int)round(j * scale_h))
                                                  : ((int)(j * scale_h));
                    scale_h_fix_in_loop = ((int)(j * scale_h));
                    int index           = (start_x + start_y * width_stride) +
                                (int)((scale_para_fix * i) / scale_para) +
                                //((int)round(j * scale_h)) * width_stride +
                                scale_h_fix_in_loop * width_stride +
                                ((channel_idx + image_idx * channel_num) *
                                 width_stride * height_stride);
                    int out_index = i + j * new_width +
                                    (channel_idx + image_idx * channel_num) *
                                        new_width * new_height;
                    new_img_buf[out_index] = *(temp_old_buf + index);
                }
            }
        }
    }
    delete[] temp_old_buf;

    return 0;
}

static void test_cv_yuv2rgb_single_case() {
    u8 *  input_ptr;
    void *output_ptr;
    void *reference_ptr;
    int   dst_image_format, src_image_format;
    int   output_data_format;

    int uv_format_factor = 2;

    gen_random_yuv_data(input_ptr,
                        output_ptr,
                        reference_ptr,
                        &dst_image_format,
                        src_image_format,
                        &output_data_format);
    if (src_image_format == FORMAT_NV16 ||
        src_image_format == FORMAT_NV61 ||
        src_image_format == FORMAT_YUV422P) {
        uv_format_factor = 1;
    }
    int n_size = IMAGE_H * IMAGE_W + IMAGE_H * IMAGE_W / uv_format_factor;

    printf("test with N %d H %d W %d dst_storage_mode %d\n",
           IMAGE_N,
           IMAGE_H,
           IMAGE_W,
           (int)output_data_format);

    bm_image src_image[4], dst_image[4];
    for (int i = 0; i < IMAGE_N; i++) {
        u8 *input_buf[3] = {(u8 *)malloc(IMAGE_H * IMAGE_W),
                            (u8 *)malloc(IMAGE_H * IMAGE_W / uv_format_factor),
                            (u8 *)malloc(IMAGE_H * IMAGE_W / 2)};
        switch (src_image_format) {
            case FORMAT_NV12:
                memcpy(input_buf[0], input_ptr + i * n_size, IMAGE_H * IMAGE_W);
                memcpy(input_buf[1],
                       input_ptr + i * n_size + IMAGE_H * IMAGE_W,
                       IMAGE_H * IMAGE_W / uv_format_factor);
                break;
            case FORMAT_NV21:
                memcpy(input_buf[0], input_ptr + i * n_size, IMAGE_H * IMAGE_W);
                for (int h = 0; h < IMAGE_H / uv_format_factor; h++) {
                    for (int w = 0; w < IMAGE_W / 2; w++) {
                        input_buf[1][IMAGE_W * h + 2 * w] =
                            input_ptr[i * IMAGE_H * IMAGE_W * 3 / 2 +
                                      IMAGE_H * IMAGE_W + IMAGE_W * h + 2 * w +
                                      1];
                        input_buf[1][IMAGE_W * h + 2 * w + 1] =
                            input_ptr[i * IMAGE_H * IMAGE_W * 3 / 2 +
                                      IMAGE_H * IMAGE_W + IMAGE_W * h + 2 * w];
                    }
                }
                break;
            case FORMAT_NV16:
                memcpy(input_buf[0], input_ptr + i * n_size, IMAGE_H * IMAGE_W);
                memcpy(input_buf[1],
                       input_ptr + i * n_size + IMAGE_H * IMAGE_W,
                       IMAGE_H * IMAGE_W / uv_format_factor);
                break;
            case FORMAT_NV61:
                memcpy(input_buf[0], input_ptr + i * n_size, IMAGE_H * IMAGE_W);
                for (int h = 0; h < IMAGE_H / uv_format_factor; h++) {
                    for (int w = 0; w < IMAGE_W / 2; w++) {
                        input_buf[1][IMAGE_W * h + 2 * w] =
                            input_ptr[i * IMAGE_H * IMAGE_W * 2 +
                                      IMAGE_H * IMAGE_W + IMAGE_W * h + 2 * w +
                                      1];
                        input_buf[1][IMAGE_W * h + 2 * w + 1] =
                            input_ptr[i * IMAGE_H * IMAGE_W * 2 +
                                      IMAGE_H * IMAGE_W + IMAGE_W * h + 2 * w];
                    }
                }
                break;
            case FORMAT_YUV420P:
                memcpy(input_buf[0],
                       input_ptr + i * IMAGE_H * IMAGE_W * 3 / 2,
                       IMAGE_H * IMAGE_W);
                for (int h = 0; h < IMAGE_H / 2; h++) {
                    for (int w = 0; w < IMAGE_W / 2; w++) {
                        input_buf[1][IMAGE_W / 2 * h + w] =
                            input_ptr[i * IMAGE_H * IMAGE_W * 3 / 2 +
                                      IMAGE_H * IMAGE_W + IMAGE_W * h + 2 * w];
                        input_buf[2][IMAGE_W / 2 * h + w] =
                            input_ptr[i * IMAGE_H * IMAGE_W * 3 / 2 +
                                      IMAGE_H * IMAGE_W + IMAGE_W * h + 2 * w +
                                      1];
                    }
                }
                break;
            default:  // FORMAT_YUV422P
                memcpy(input_buf[0], input_ptr + i * n_size, IMAGE_H * IMAGE_W);
                for (int h = 0; h < IMAGE_H / 2; h++) {
                    for (int w = 0; w < IMAGE_W; w++) {
                        input_buf[1][IMAGE_W * h + w] =
                            input_ptr[i * n_size + IMAGE_H * IMAGE_W +
                                      IMAGE_W * h * 2 + 2 * w];
                        input_buf[2][IMAGE_W * h + w] =
                            input_ptr[i * n_size + IMAGE_H * IMAGE_W +
                                      IMAGE_W * h * 2 + 2 * w + 1];
                    }
                }
                break;
        }

        bm_image_create(handle,
                        IMAGE_H,
                        IMAGE_W,
                        (bm_image_format_ext)src_image_format,
                        DATA_TYPE_EXT_1N_BYTE,
                        src_image + i);
        bm_image_copy_host_to_device(src_image[i], (void **)input_buf);
        free(input_buf[0]);
        free(input_buf[1]);
        free(input_buf[2]);
    }
    int output_image_num =
        output_data_format == DATA_TYPE_EXT_4N_BYTE ? 1 : IMAGE_N;
    for (int i = 0; i < output_image_num; i++) {
        bm_image_create(handle,
                        IMAGE_H,
                        IMAGE_W,
                        (bm_image_format_ext)dst_image_format,
                        (bm_image_data_format_ext)output_data_format,
                        dst_image + i);
    }

    struct timeval t1, t2;
    gettimeofday_(&t1);
    bm_status_t ret =
        bmcv_image_yuv2bgr_ext(handle, IMAGE_N, src_image, dst_image);
    gettimeofday_(&t2);
    cout << "yuv2rgb TPU using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;


    if (ret != BM_SUCCESS) {
        printf("run bm_yuv2rgb failed ret = %d\n", ret);
        free(input_ptr);
        free(output_ptr);
        free(reference_ptr);
        exit(-1);
    }

    int image_byte_size[3] = {0};
    bm_image_get_byte_size(dst_image[0], image_byte_size);
    int byte_size =
        image_byte_size[0] + image_byte_size[1] + image_byte_size[2];
    for (int i = 0; i < output_image_num; i++) {
        void *out_ptr[3] = {
            (void *)((u8 *)output_ptr + i * byte_size),
            (void *)((u8 *)output_ptr + i * byte_size + image_byte_size[0]),
            (void *)((u8 *)output_ptr + i * byte_size +
                     2 * image_byte_size[0])};
        bm_image_copy_device_to_host(dst_image[i], out_ptr);
    }

    const char *info_label = "cv_yuv2rgb test ...\n";

    if (output_data_format != DATA_TYPE_EXT_FLOAT32) {
        if (output_data_format == DATA_TYPE_EXT_4N_BYTE)
            convert_4N_INT8_to_1N_INT8(
                (u8 *)output_ptr, IMAGE_N, 3, IMAGE_H, IMAGE_W);
        int cmp_res_x = array_cmp_u8((u8 *)reference_ptr,
                                     (u8 *)output_ptr,
                                     IMAGE_N * IMAGE_W * IMAGE_H * 3,
                                     info_label,
                                     1);
        if (cmp_res_x != 0) {
            printf("cv_yuv2rgb comparing failed with N %d H %d W %d\n",
                   IMAGE_N,
                   IMAGE_H,
                   IMAGE_W);
            exit(-1);
        }
    } else {
        int cmp_res_x = array_cmp_((float *)reference_ptr,
                                  (float *)output_ptr,
                                  IMAGE_N * IMAGE_W * IMAGE_H * 3,
                                  info_label,
                                  1);
        if (cmp_res_x != 0) {
            printf("cv_yuv2rgb comparing failed with N %d H %d W %d\n",
                   IMAGE_N,
                   IMAGE_H,
                   IMAGE_W);
            exit(-1);
        }
    }

    for (int i = 0; i < IMAGE_N; i++) {
        bm_image_destroy(src_image[i]);
    }
    for (int i = 0; i < output_image_num; i++) {
        bm_image_destroy(dst_image[i]);
    }
    free(input_ptr);
    free(output_ptr);
    free(reference_ptr);
}

#ifdef OPENCV
using namespace cv;

int main(int argc, char *argv[]) {
    int         dev_id = 0;
    bm_status_t ret    = bm_dev_request(&handle, dev_id);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        exit(-1);
    }
    const char filename[] = "lena_small.yuv";
    FILE *     fp;
    fp = fopen(filename, "r");
    if (!fp) {
        printf("Can not open %s, quit\n", filename);
        exit(-1);
    }

    int H = 480;
    int W = 480;

    u8 *input_buffer = new u8[H * W * 3 / 2];
    u8 *yuv_buffer   = new u8[H * W * 3];
    u8 *ycrcb_buffer = new u8[H * W * 3];

    fread(input_buffer, 480 * 480 * 3 / 2, 1, fp);

    bmcv_yuv2rgb(handle,
                 bm_mem_from_system((void *)input_buffer),
                 (int)STORAGE_MODE_1N_INT8,
                 1,
                 H,
                 W,
                 1,
                 bm_mem_from_system((void *)yuv_buffer),
                 STORAGE_MODE_1N_INT8,
                 0);
    bmcv_yuv2rgb(handle,
                 bm_mem_from_system((void *)input_buffer),
                 (int)STORAGE_MODE_1N_INT8,
                 1,
                 H,
                 W,
                 0,
                 bm_mem_from_system((void *)ycrcb_buffer),
                 STORAGE_MODE_1N_INT8,
                 0);
    interleave(ycrcb_buffer, 1, H, W);
    interleave(yuv_buffer, 1, H, W);
    Mat yuv_img, ycrcb_img;
    yuv_img.create(H, W, CV_8UC3);
    ycrcb_img.create(H, W, CV_8UC3);
    memcpy(yuv_img.data, yuv_buffer, 3 * H * W * sizeof(u8));
    memcpy(ycrcb_img.data, ycrcb_buffer, 3 * H * W * sizeof(u8));
    imshow("yuv", yuv_img);
    imshow("ycrcb", ycrcb_img);
    waitKey();

    delete[] input_buffer;
    delete[] yuv_buffer;
    delete[] ycrcb_buffer;
    bm_dev_free(handle);
    return 0;
}
#else
int main(int argc, char *argv[]) {
    int test_loop_times = 1;
    switch(argc)
    {
        case 3:
        {
            int thread_num = atoi(argv[2]);
            if(thread_num < 1 || thread_num > 4)
            {
                std::cout << "[TEST WARP] thread_num should be 1~4" << std::endl;
                exit(-1);
            }
            if(thread_num != 1)
                pool = std::make_shared<ThreadPool>(thread_num);
        }
        break;
        case 2:
            test_loop_times = atoi(argv[1]);
        break;
    }
    if (test_loop_times > 1500 || test_loop_times < 1)
    {
        std::cout << "[TEST YUV2RGB] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    std::cout << "[TEST YUV2RGB] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST YUV2RGB] LOOP " << loop_idx << "------"
                  << std::endl;
        struct timespec tp;
        int             dev_id = 0;
        bm_status_t     ret    = bm_dev_request(&handle, dev_id);
        if (ret != BM_SUCCESS) {
            printf("Create bm handle failed. ret = %d\n", ret);
            exit(-1);
        }
        clock_gettime_(0, &tp);

        // tp.tv_nsec = 68798983;
        pool->srand(tp.tv_nsec);
        printf("random seed %lu\n", tp.tv_nsec);
        int test_loops = 10;
        for (int i = 0; i < test_loops; i++) {
            pool->enqueue(test_cv_yuv2rgb_single_case);
        }
        pool->wait_all_done();
        printf("yuv2rgb test pass\n");
        bm_dev_free(handle);
    }
    std::cout << "------[TEST YUV2RGB] ALL TEST PASSED!" << std::endl;

    return 0;
}
#endif
