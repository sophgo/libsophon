#include <iostream>
#include "bmcv_api_ext.h"
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

#define DEBUG_FILE (0)
#define USE_RANDOM (1)

#define MODE (STORAGE_MODE_4N_INT8)
#define IMG_N 4
#define IMG_C 3
#define SRC_H 1080
#define SRC_W 1920
#define DST_H 112
#define DST_W 112

static bm_handle_t handle;
static std::shared_ptr<ThreadPool> pool = std::make_shared<ThreadPool>(1);

template<typename T>
class Blob {
  public:
    T *data      = NULL;
    int   blob_size = 0;

    explicit Blob(int size) {
        if (size == 0) {
            data      = NULL;
            blob_size = 0;
            return;
        }
        data      = new T[size];
        blob_size = size;
    }
    ~Blob() {
        if (data) {
            delete[] data;
            data = NULL;
            blob_size = 0;
        }
    }
};

typedef std::shared_ptr<Blob<u8>> u8_data;
typedef std::shared_ptr<Blob<int>> int_data;
typedef std::shared_ptr<Blob<float>> float_data;
typedef std::shared_ptr<Blob<bm_image>> image_data;

#define MAKE_BLOB(T, size) std::make_shared<Blob<T>>(size)

u8_data convert_4N_INT8_to_1N_INT8(
    u8_data input_, int N, int C, int H, int W) {
    u8_data output_ = MAKE_BLOB(u8, N * C * H * W);
    u8 *             input   = (u8 *)input_->data;
    u8 *             output  = (u8 *)output_->data;
    for (int n = 0; n < N; n++) {
        for (int loop = 0; loop < C * H * W; loop++) {
            output[n * C * H * W + loop] =
                input[(n / 4) * 4 * C * H * W + 4 * loop + n % 4];
        }
    }
    return output_;
}

u8_data convert_1N_INT8_to_4N_INT8(
    u8_data input_, int n, int c, int h, int w) {
    u8_data output_ = MAKE_BLOB(u8, 4 * c * h * w);
    u8 *             input   = (u8 *)input_->data;
    u8 *             output  = (u8 *)output_->data;

    for (int i = 0; i < ALIGN(n, 4) / 4; i++) {
        for (int loop = 0; loop < c * h * w; loop++) {
            for (int k = 0; k < bm_min(n, 4); k++) {
                output[i * 4 * c * h * w + 4 * loop + k] =
                    input[i * 4 * c * h * w + k * c * h * w + loop];
            }
        }
    }
    return output_;
};

static int         get_source_idx(int idx, int *matrix, int image_n) {
    for (int i = 0; i < image_n; i++) {
        if (idx < matrix[i]) {
            return i;
        }
    }
    printf("%s:%d[%s] Error:get source idx error\n",__FILE__, __LINE__, __FUNCTION__);
    exit(-1);
}

static u8_data image_read(
                       int            image_n,
                       int            image_c,
                       int            image_h,
                       int            image_w) {

    auto engine = pool->get_random_engine();
    auto res = MAKE_BLOB(u8, image_n * image_c * image_h * image_w);
    for (int i = 0; i < image_n * image_c * image_h * image_w; i++)
    {
        res->data[i] = ((*engine)() & 0xff);
    }
    return res;
}

u8 inline fetch_pixel(int x_idx, int y_idx, int c_idx, int width, int height, int w_stride, u8* image)
{
    if(x_idx < 0 || x_idx >= width || y_idx < 0 || y_idx >= height)
        return 0;
    return image[width * height * w_stride * c_idx + y_idx * width * w_stride + x_idx * w_stride];
}

u8 fetch_pixel(float x_idx, float y_idx, int c_idx, int width, int height, u8* image)
{
    x_idx = bm_min(width - 1, x_idx);
    x_idx = bm_max(0, x_idx);

    y_idx = bm_min(height - 1, y_idx);
    y_idx = bm_max(0, y_idx);

    int ceil_x  = ceil(x_idx);
    int ceil_y  = ceil(y_idx);
    int floor_x = floor(x_idx);
    int floor_y = floor(y_idx);

    u8 pixel_lu = image[width * height * c_idx + floor_y * width + floor_x];
    u8 pixel_ld = image[width * height * c_idx + ceil_y * width + floor_x];
    u8 pixel_ru = image[width * height * c_idx + floor_y * width + ceil_x];
    u8 pixel_rd = image[width * height * c_idx + ceil_y * width + ceil_x];

    float coef_x_ceil = x_idx - floor_x;
    float coef_y_ceil = y_idx - floor_y;

    float coef_x_floor = 1.0 - coef_x_ceil;
    float coef_y_floor = 1.0 - coef_y_ceil;

    u8 mix = (u8)(coef_x_floor * coef_y_floor * pixel_lu +
                coef_x_floor * coef_y_ceil * pixel_ld +
                coef_x_ceil * coef_y_floor * pixel_ru +
                coef_x_ceil * coef_y_ceil * pixel_rd + 0.5f);
    return mix;
};

static int bmcv_affine_nearest_cmp_1n(
                            unsigned char * p_exp,
                            unsigned char * p_got,
                            unsigned char * src,
                            int *           map,
                            int             image_c,
                            int             image_sh,
                            int             image_sw,
                            int             image_dh,
                            int             image_dw) {
    // generate map.
    int* map_x    = map;
    int* map_y    = map + image_dh * image_dw;
    int w_stride = 1;
    int image_dw_stride = image_dw * w_stride;

    for (int c = 0; c < image_c; c++) {
        for (int y = 0; y < image_dh; y++) {
            for (int x = 0; x < image_dw; x++) {
                unsigned char got = p_got[c * image_dw_stride * image_dh +
                                          y * image_dw_stride + x * w_stride];
                unsigned char exp = p_exp[c * image_dw_stride * image_dh +
                                          y * image_dw_stride + x * w_stride];
                if (got != exp) {
                    // loopup up/down left/right by 1 pixel in source image.
                    int sx = map_x[y * image_dw + x];
                    int sy = map_y[y * image_dw + x];
                    //if(sx >= image_sw || sx < 0 || sy >= image_sh || sy < 0)
                    //    continue;
                    unsigned char left = fetch_pixel(
                        sx + 1, sy, c, image_sw, image_sh, w_stride, src);
                    unsigned char right = fetch_pixel(sx - 1, sy, c, image_sw, image_sh, w_stride, src);
                    unsigned char up = fetch_pixel(sx, sy - 1, c, image_sw, image_sh, w_stride, src);
                    unsigned char down = fetch_pixel(sx, sy + 1, c, image_sw, image_sh, w_stride, src);
                    unsigned char left_up = fetch_pixel(sx - 1, sy - 1, c, image_sw, image_sh, w_stride, src);
                    unsigned char right_up = fetch_pixel(sx + 1, sy - 1, c, image_sw, image_sh, w_stride, src);
                    unsigned char left_down = fetch_pixel(sx - 1, sy + 1, c, image_sw, image_sh, w_stride, src);
                    unsigned char right_down = fetch_pixel(sx + 1, sy + 1, c, image_sw, image_sh, w_stride, src);
                    if (got == left || got == right || got == up ||
                        got == down || got == left_up || got == left_down ||
                        got == right_up || got == right_down) {
                        // find the neighbour pixel, continue.
                        // printf("c:%d h %d w %d, got:%d exp %d\n", c, y, x,
                        // got, exp); printf("got 0x%x left 0x%x right 0x%x up
                        // 0x%x down 0x%x left_up 0x%x left_down 0x%x right_up
                        // 0x%x right_down 0x%x\n",
                        //        got, left, right, up, down, left_up,
                        //        left_down, right_up, right_down);
                    } else {
                        printf("c:%d h %d w %d, got:%d exp %d\n",
                               c,
                               y,
                               x,
                               got,
                               exp);
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

static int bmcv_affine_bilinear_cmp_1n(unsigned char * p_exp,
                                       unsigned char * p_got,
                                       unsigned char * src,
                                       float*          map,
                                       int             image_c,
                                       int             image_sh,
                                       int             image_sw,
                                       int             image_dh,
                                       int             image_dw) {
    // generate map.
    float* map_x = map;
    float* map_y = map + image_dh * image_dw;

    for (int c = 0; c < image_c; c++) {
        for (int y = 0; y < image_dh; y++) {
            for (int x = 0; x < image_dw; x++) {
                unsigned char got = p_got[c * image_dw * image_dh +
                                          y * image_dw + x];
                unsigned char exp = p_exp[c * image_dw * image_dh +
                                          y * image_dw + x];
                if (got != exp) {
                    // loopup up/down left/right by 1 pixel in source image.
                    float sx = map_x[y * image_dw + x];
                    float sy = map_y[y * image_dw + x];
                    //if(sx >= image_sw || sx < 0 || sy >= image_sh || sy < 0)
                    //    continue;
                    u8 mix_pixel =
                        fetch_pixel(sx, sy, c, image_sw, image_sh, src);

                    if (abs(mix_pixel - got) <= 1) {
                        continue;
                        // find the neighbour pixel, continue.
                        // printf("c:%d h %d w %d, got:%d exp %d\n", c, y, x,
                        // got, exp); printf("got 0x%x left 0x%x right 0x%x up
                        // 0x%x down 0x%x left_up 0x%x left_down 0x%x right_up
                        // 0x%x right_down 0x%x\n",
                        //        got, left, right, up, down, left_up,
                        //        left_down, right_up, right_down);
                    } else {
                        printf("c:%d h %d w %d, got:%d exp %d\n",
                               c,
                               y,
                               x,
                               got,
                               exp);
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

static int bmcv_warp_cmp(u8_data   p_exp,
                         u8_data   p_got,
                         u8_data   src,
                         int_data  map,
                         int       matrix_num[4],
                         int       image_n,
                         int       image_c,
                         int       image_sh,
                         int       image_sw,
                         int       image_dh,
                         int       image_dw,
                         bool      is_bilinear) {
    int ret             = 0;
    int output_num      = 0;
    int matrix_sigma[4] = {0};
    for (int i = 0; i < image_n; i++) {
        output_num += matrix_num[i];
        matrix_sigma[i] = output_num;
    }

    for (int i = 0; i < output_num; i++) {
        int s_idx = get_source_idx(i, matrix_sigma, image_n);
        int src_offset = s_idx * image_c * image_sh * image_sw;
        int dst_offset = i * image_c * image_dh * image_dw;
        if (is_bilinear)
            ret = bmcv_affine_bilinear_cmp_1n(p_exp->data + dst_offset,
                                              p_got->data + dst_offset,
                                              src->data + src_offset,
                                              (float*)map->data + i * image_dh * image_dw * 2,
                                              image_c,
                                              image_sh,
                                              image_sw,
                                              image_dh,
                                              image_dw);
        else
            ret = bmcv_affine_nearest_cmp_1n(p_exp->data + dst_offset,
                                             p_got->data + dst_offset,
                                             src->data + src_offset,
                                             (int*)map->data + i * image_dh * image_dw * 2,
                                             image_c,
                                             image_sh,
                                             image_sw,
                                             image_dh,
                                             image_dw);
        if (ret != 0) {
            return ret;
        }
    }
    return ret;
}

static bm_status_t bmcv_affine_nearest_1n_ref(
                                    // input
                                    unsigned char *src_image,
                                    float *        trans_mat,
                                    int            image_c,
                                    int            image_sh,
                                    int            image_sw,
                                    int            image_dh,
                                    int            image_dw,
                                    // output
                                    int *          map,
                                    unsigned char *dst_image) {
    UNUSED(image_c);
    float_data tensor_S = MAKE_BLOB(float, image_dh *image_dw * 2);
    float *tensor_SX = tensor_S->data;
    float *         tensor_SY = tensor_SX + image_dh * image_dw;
    int *tensor_DX = map;
    int *tensor_DY = tensor_DX + image_dh * image_dw;
    int             dst_w     = image_dw;
    int             dst_h     = image_dh;
    int             src_w     = image_sw;
    int             src_h     = image_sh;
    float           m0        = trans_mat[0];
    float           m1        = trans_mat[1];
    float           m2        = trans_mat[2];
    float           m3        = trans_mat[3];
    float           m4        = trans_mat[4];
    float           m5        = trans_mat[5];

    // generate the input for calculate coordinate map.
    for (int y = 0; y < dst_h; y++) {
        for (int x = 0; x < dst_w; x++) {
            tensor_SX[y * dst_w + x] = (float)x;
            tensor_SY[y * dst_w + x] = (float)y;
        }
    }
    // calculate coordinate map.
    for (int y = 0; y < dst_h; y++) {
        for (int x = 0; x < dst_w; x++) {
            float dx = tensor_SX[y * dst_w + x] * m0 +
                       tensor_SY[y * dst_w + x] * m1 + m2;
            float dy = tensor_SX[y * dst_w + x] * m3 +
                       tensor_SY[y * dst_w + x] * m4 + m5;
            tensor_DX[y * dst_w + x] = (unsigned short)(dx + 0.5f);
            tensor_DY[y * dst_w + x] = (unsigned short)(dy + 0.5f);
            tensor_DX[y * dst_w + x] =
                bm_min(tensor_DX[y * dst_w + x], image_sw - 1);       tensor_DX[y * dst_w + x] =
                bm_max(tensor_DX[y * dst_w + x], 0);
            tensor_DY[y * dst_w + x] =
                bm_min(tensor_DY[y * dst_w + x], image_sh - 1);
            tensor_DY[y * dst_w + x] =
                bm_max(tensor_DY[y * dst_w + x], 0);
        }
    }
    int w_stride = 1;
    int src_w_stride = src_w * w_stride;
    int dst_w_stride = dst_w * w_stride;

    // warp in source image directly.
    unsigned char *sb = src_image;
    unsigned char *sg = sb + src_w_stride * src_h;
    unsigned char *sr = sg + src_w_stride * src_h;
    unsigned char *db = dst_image;
    unsigned char *dg = db + dst_w_stride * dst_h;
    unsigned char *dr = dg + dst_w_stride * dst_h;
    tensor_DX         = map;
    tensor_DY         = tensor_DX + dst_h * dst_w;
    for (int y = 0; y < dst_h; y++) {
        for (int x = 0; x < dst_w; x++) {
            unsigned short sx = tensor_DX[y * dst_w + x];
            unsigned short sy = tensor_DY[y * dst_w + x];
            db[y * dst_w_stride + x * w_stride] =
                sb[sy * src_w_stride + sx * w_stride];
            dg[y * dst_w_stride + x * w_stride] =
                sg[sy * src_w_stride + sx * w_stride];
            dr[y * dst_w_stride + x * w_stride] =
                sr[sy * src_w_stride + sx * w_stride];
        }
    }

    return BM_SUCCESS;
}

static bm_status_t bmcv_affine_bilinear_1n_ref(
                                    // input
                                    unsigned char *src_image,
                                    float *        trans_mat,
                                    int            image_c,
                                    int            image_sh,
                                    int            image_sw,
                                    int            image_dh,
                                    int            image_dw,
                                    // output
                                    float *        map_index,
                                    unsigned char *dst_image) {
    UNUSED(image_c);
    float_data tensor_S = MAKE_BLOB(float, image_dh *image_dw * 2);
    float *tensor_SX = tensor_S->data;
    float *         tensor_SY = tensor_SX + image_dh * image_dw;
    float *tensor_DX = map_index;
    float *tensor_DY = tensor_DX + image_dh * image_dw;
    int             dst_w     = image_dw;
    int             dst_h     = image_dh;
    int             src_w     = image_sw;
    int             src_h     = image_sh;
    float           m0        = trans_mat[0];
    float           m1        = trans_mat[1];
    float           m2        = trans_mat[2];
    float           m3        = trans_mat[3];
    float           m4        = trans_mat[4];
    float           m5        = trans_mat[5];

    // generate the input for calculate coordinate map.
    for (int y = 0; y < dst_h; y++) {
        for (int x = 0; x < dst_w; x++) {
            tensor_SX[y * dst_w + x] = (float)x;
            tensor_SY[y * dst_w + x] = (float)y;
        }
    }
    // calculate coordinate map.
    for (int y = 0; y < dst_h; y++) {
        for (int x = 0; x < dst_w; x++) {
            float dx = tensor_SX[y * dst_w + x] * m0 +
                       tensor_SY[y * dst_w + x] * m1 + m2;
            float dy = tensor_SX[y * dst_w + x] * m3 +
                       tensor_SY[y * dst_w + x] * m4 + m5;
            tensor_DX[y * dst_w + x] = dx;
            tensor_DY[y * dst_w + x] = dy;
        }
    }
    int w_stride = 1;
    int src_w_stride = src_w * w_stride;
    int dst_w_stride = dst_w * w_stride;

    // warp in source image directly.
    unsigned char *sb = src_image;
    unsigned char *sg = sb + src_w_stride * src_h;
    unsigned char *sr = sg + src_w_stride * src_h;
    unsigned char *db = dst_image;
    unsigned char *dg = db + dst_w_stride * dst_h;
    unsigned char *dr = dg + dst_w_stride * dst_h;
    tensor_DX         = map_index;
    tensor_DY         = tensor_DX + dst_h * dst_w;
    for (int y = 0; y < dst_h; y++) {
        for (int x = 0; x < dst_w; x++) {
            float sx = tensor_DX[y * dst_w + x];
            float sy = tensor_DY[y * dst_w + x];
            db[y * dst_w_stride + x * w_stride] =
                fetch_pixel(sx, sy, 0, image_sw, image_sh, sb);
            dg[y * dst_w_stride + x * w_stride] =
                fetch_pixel(sx, sy, 0, image_sw, image_sh, sg);
            dr[y * dst_w_stride + x * w_stride] =
                fetch_pixel(sx, sy, 0, image_sw, image_sh, sr);
        }
    }
    return BM_SUCCESS;
}

static bm_status_t bmcv_warp_ref(
    // input
    u8_data src_image,
    float_data trans_mat,
    int matrix_num[4],
    int image_n,
    int image_c,
    int image_sh,
    int image_sw,
    int image_dh,
    int image_dw,
    bool is_bilinear,
    // output
    int_data map,
    u8_data dst_image)
{
    int output_num      = 0;
    int matrix_sigma[4] = {0};
    for (int i = 0; i < image_n; i++) {
        output_num += matrix_num[i];
        matrix_sigma[i] = output_num;
    }
    for (int i = 0; i < output_num; i++) {
        int s_idx = get_source_idx(i, matrix_sigma, image_n);
        int src_offset = s_idx * image_c * image_sh * image_sw;
        int dst_offset = i * image_c * image_dh * image_dw;
        if (is_bilinear)
            bmcv_affine_bilinear_1n_ref(
                src_image->data + src_offset,
                trans_mat->data + i * 6,
                image_c,
                image_sh,
                image_sw,
                image_dh,
                image_dw,
                (float *)map->data + i * image_dh * image_dw * 2,
                dst_image->data + dst_offset);
        else
            bmcv_affine_nearest_1n_ref(
                src_image->data + src_offset,
                trans_mat->data + i * 6,
                image_c,
                image_sh,
                image_sw,
                image_dh,
                image_dw,
                (int *)map->data + i * image_dh * image_dw * 2,
                dst_image->data + dst_offset);
    }
    return BM_SUCCESS;
}

static bm_status_t bmcv_warp_tpu(bm_handle_t handle,
                                 // input
                                 u8_data src_data,
                                 float_data trans_mat,
                                 int matrix_num[4],
                                 int src_mode,
                                 int image_n,
                                 int image_c,
                                 int image_sh,
                                 int image_sw,
                                 int image_dh,
                                 int image_dw,
                                 bool is_bilinear,
                                 // output
                                 u8_data dst_data) {
    int output_num = 0;
    for (int i = 0; i < image_n; i++) {
        output_num += matrix_num[i];
    }

    int                    b4N = (src_mode == STORAGE_MODE_4N_INT8);
    bmcv_affine_image_matrix matrix_image[4];
    bmcv_affine_matrix *     matrix = (bmcv_affine_matrix *)(trans_mat->data);
    for (int i = 0; i < image_n; i++) {
        matrix_image[i].matrix_num = matrix_num[i];
        matrix_image[i].matrix     = matrix;
        matrix += matrix_num[i];
    }

    bm_image                 src_img[4];
    bm_image_format_ext      image_format = FORMAT_BGR_PLANAR;
    bm_image_data_format_ext data_type =
        b4N ? DATA_TYPE_EXT_4N_BYTE : DATA_TYPE_EXT_1N_BYTE;
    int in_image_num = b4N ? 1 : image_n;
    int out_image_num = b4N ? (ALIGN(output_num, 4) / 4) : output_num;
    // create source image.
    if(b4N)
    {
        src_data = convert_1N_INT8_to_4N_INT8(src_data, image_n, image_c, image_sh, image_sw);
    }

    for (int i = 0; i < in_image_num; i++)
    {
        BM_CHECK_RET(bm_image_create(
            handle, image_sh, image_sw, image_format, data_type, src_img + i));
        int stride = 0;
        bm_image_get_stride(src_img[i], &stride);

        void *ptr = (void *)(src_data->data + 3 * stride * image_sh * i);
        BM_CHECK_RET(
            bm_image_copy_host_to_device(src_img[i], (void **)(&ptr)));
    }

    // create dst image.
    image_data dst_img = MAKE_BLOB(bm_image, out_image_num);

    for (int i = 0; i < out_image_num; i++) {
        BM_CHECK_RET(bm_image_create(
            handle, image_dh, image_dw, image_format, data_type, dst_img->data + i));
    }

    BM_CHECK_RET(
        bmcv_image_warp_affine(handle, image_n, matrix_image, src_img, dst_img->data, is_bilinear));

    int size = 0;
    bm_image_get_byte_size(dst_img->data[0], &size);
    auto temp_out = MAKE_BLOB(u8, out_image_num * size);

    for (int i = 0; i < out_image_num; i++) {
        void *ptr = (void *)(temp_out->data + size * i);
        BM_CHECK_RET(
            bm_image_copy_device_to_host(dst_img->data[i], (void **)&ptr));
    }

    for (int i = 0; i < in_image_num; i++)
    {
        bm_image_destroy(src_img[i]);
    }

    for (int i = 0; i < out_image_num; i++)
    {
        bm_image_destroy(dst_img->data[i]);
    }

    if(b4N)
    {
        temp_out = convert_4N_INT8_to_1N_INT8(temp_out, output_num, image_c, image_dh, image_dw);
    }

    memcpy(dst_data->data, temp_out->data, output_num * image_c * image_dh * image_dw);

    return BM_SUCCESS;
}

static void test_cv_warp_single_case(u8_data        src_image,
                                     float_data     trans_mat,
                                     int            matrix_num[4],
                                     int            src_mode,
                                     int            image_n,
                                     int            image_c,
                                     int            image_sh,
                                     int            image_sw,
                                     int            image_dh,
                                     int            image_dw,
                                     bool           is_bilinear) {
    int output_num = 0;
    for (int i = 0; i < image_n; i++) {
        output_num += matrix_num[i];
    }
    auto map = MAKE_BLOB(int, output_num *image_dh *image_dw * 2);
    auto dst_image_ref = MAKE_BLOB(u8, output_num * image_c * image_dh * image_dw);
    auto dst_image_tpu = MAKE_BLOB(u8, output_num * image_c * image_dh * image_dw);

    // calculate use reference for compare.
    bm_status_t ret_ref = bmcv_warp_ref(
                                        src_image,
                                        trans_mat,
                                        matrix_num,
                                        image_n,
                                        image_c,
                                        image_sh,
                                        image_sw,
                                        image_dh,
                                        image_dw,
                                        is_bilinear,
                                        map,
                                        dst_image_ref);
    if (ret_ref != BM_SUCCESS) {
        printf("run bm_warp_ref failed ret = %d\n", ret_ref);
        exit(-1);
    }

    // calculate use NPU.
    bm_status_t ret = bmcv_warp_tpu(handle,
                                    src_image,
                                    trans_mat,
                                    matrix_num,
                                    src_mode,
                                    image_n,
                                    image_c,
                                    image_sh,
                                    image_sw,
                                    image_dh,
                                    image_dw,
                                    is_bilinear,
                                    dst_image_tpu);
    if (ret != BM_SUCCESS) {
        printf("run bm_warp failed ret = %d\n", ret);
        exit(-1);
    }
    // compare.
    int cmp_res = bmcv_warp_cmp(dst_image_ref,
                                dst_image_tpu,
                                src_image,
                                map,
                                matrix_num,
                                image_n,
                                image_c,
                                image_sh,
                                image_sw,
                                image_dh,
                                image_dw,
                                is_bilinear);
    if (cmp_res != 0) {
        printf("cv_warp comparing failed\n");
        exit(-1);
    }
}

static void test_cv_warp_random(int trials) {
    int         dev_id = 0;
    bm_status_t ret    = bm_dev_request(&handle, dev_id);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        exit(-1);
    }
    for (int idx_trial = 0; idx_trial < trials; idx_trial++) {
        auto task = []() {
            int image_c = IMG_C;
            int matrix_num[4] = {0};

            auto engine = pool->get_random_engine();
            bool is_bilinear = ((*engine)() & 0x01) ? true : false;
            is_bilinear = false;
            printf("is_bilinear: %d \n", is_bilinear);
            int src_mode = ((*engine)() & 0x01) ? STORAGE_MODE_1N_INT8 : STORAGE_MODE_4N_INT8;
            int image_n = 1 + ((*engine)() & 0x3);
            int image_sh = ((*engine)() & 0x7fff);
            int image_sw = ((*engine)() & 0x7fff);
            while (image_sh > 2160 || image_sh < 1080)
            {
                image_sh = ((*engine)() & 0x7fff);
            }
            while (image_sw > 3840 || image_sw < 1920)
            {
                image_sw = ((*engine)() & 0x7fff);
            }
            int image_dh = ((*engine)() & 0x3ff);
            int image_dw = ((*engine)() & 0x3ff);
            image_dh = (image_dh == 0) ? 1 : image_dh;
            image_dw = (image_dw == 0) ? 1 : image_dw;

            matrix_num[0] = ((*engine)() & 0xf);
            matrix_num[1] = ((*engine)() & 0xf);
            matrix_num[2] = ((*engine)() & 0xf);
            matrix_num[3] = ((*engine)() & 0xf);

            printf("image_n %d mode :%d input[%dx%d] output[%dx%d]\n",
                   image_n,
                   src_mode,
                   image_sw,
                   image_sh,
                   image_dw,
                   image_dh);
            int output_num = 0;
            printf("matrix number: ");
            for (int i = 0; i < image_n; i++)
            {
                output_num += matrix_num[i];
                printf("%d ", matrix_num[i]);
            }
            printf("total %d\n", output_num);
            if (!output_num)
            {
                return;
            }

            auto src_data = image_read(image_n, image_c, image_sh, image_sw);
            float_data trans_mat = MAKE_BLOB(float, output_num * 6);

            for (int i = 0; i < output_num; i++)
            {
                trans_mat->data[0 + i * 6] = 3.84843f;
                trans_mat->data[1 + i * 6] = -0.0248411f;
                trans_mat->data[2 + i * 6] = 916.203f;
                trans_mat->data[3 + i * 6] = 0.0248411;
                trans_mat->data[4 + i * 6] = 3.84843f;
                trans_mat->data[5 + i * 6] = 55.9748f;
            }

            test_cv_warp_single_case(src_data,
                                     trans_mat,
                                     matrix_num,
                                     src_mode,
                                     image_n,
                                     image_c,
                                     image_sh,
                                     image_sw,
                                     image_dh,
                                     image_dw,
                                     is_bilinear);
        };
        pool->enqueue(task);
    }
    pool->wait_all_done();
    bm_dev_free(handle);
    printf("cv_warp random tests done!\n");
}

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
    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST WARP] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    std::cout << "[TEST WARP] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST WARP] LOOP " << loop_idx << "------"
                  << std::endl;

        struct timespec tp;
        clock_gettime_(0, &tp);

        pool->srand(tp.tv_nsec);
        printf("random seed %lu\n", tp.tv_nsec);
        test_cv_warp_random(10);
        pool->wait_all_done();
    }
    std::cout << "------[TEST WARP] ALL TEST PASSED!" << std::endl;

    return 0;
}
