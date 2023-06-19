#include <iostream>
#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "test_cv_naive_threadpool.hpp"
#include <assert.h>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

#define BM1684  0x1684
#define BM1684X 0x1686
#define DEBUG_WARP (0)
#define MAX_INT (float)(pow(2, 31) - 2)
#define MIN_INT (float)(1 - pow(2, 31))
#define MODE (STORAGE_MODE_4N_INT8)
#define IMG_C 3

static int image_sh = 3747;
static int image_sw = 3836;
static int image_dh = 1756;
static int image_dw = 1444;
static int flag = 0;
static bm_handle_t handle;
using namespace std;

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

void inverse_matrix(int n, float arcs[3][3], float astar[3][3]);
#define MAKE_BLOB(T, size) std::make_shared<Blob<T>>(size)
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

static int get_source_idx(int idx, int *matrix, int image_n) {
    for (int i = 0; i < image_n; i++) {
        if (idx < matrix[i]) {
            return i;
        }
    }
    printf("%s:%d[%s] Error:get source idx error\n",__FILE__, __LINE__, __FUNCTION__);
    exit(-1);
}

static void my_get_perspective_transform(int* sx, int* sy, int dw, int dh, float* matrix) {
    int A = sx[3] + sx[0] - sx[1] - sx[2];
    int B = sy[3] + sy[0] - sy[1] - sy[2];
    int C = sx[2] - sx[3];
    int D = sy[2] - sy[3];
    int E = sx[1] - sx[3];
    int F = sy[1] - sy[3];
    matrix[8] = 1;
    matrix[7] = (float)(A * F - B * E) / (float)(dh * (C * F - D * E));
    matrix[6] = (float)(A * D - B * C) / (float)(dw * (D * E - C * F));
    matrix[0] = (matrix[6] * dw * sx[1] + sx[1] - sx[0]) / dw;
    matrix[1] = (matrix[7] * dh * sx[2] + sx[2] - sx[0]) / dh;
    matrix[2] = sx[0];
    matrix[3] = (matrix[6] * dw * sy[1] + sy[1] - sy[0]) / dw;
    matrix[4] = (matrix[7] * dh * sy[2] + sy[2] - sy[0]) / dh;
    matrix[5] = sy[0];
}

static void write_file(char *filename, void* data, size_t size)
{
    FILE *fp = fopen(filename, "wb+");
    assert(fp != NULL);
    fwrite(data, size, 1, fp);
    printf("save to %s %ld bytes\n", filename, size);
    fclose(fp);
}

static u8_data image_read(
                       int            image_n,
                       int            image_c,
                       int            image_h,
                       int            image_w) {
    auto res = MAKE_BLOB(u8, image_n * image_c * image_h * image_w);
#ifdef OPENCV_SRC
    cv::Mat matSrc = cv::imread("test.png", 2 | 4);
    cv::resize(matSrc, matSrc, cv::Size(image_w,image_h), 0, 0, cv::INTER_LINEAR);
    cv::imwrite("output.png", matSrc);
    uchar *data = matSrc.data;

    unsigned char *sb = res->data;
    unsigned char *sg = sb + image_w * image_h;
    unsigned char *sr = sg + image_w * image_h;
    for (int i = 0; i < image_h * image_w; i++)
    {
        sb[i] = data[i * image_c];
        sg[i] = data[i * image_c + 1];
        sr[i] = data[i * image_c + 2];
    }
#else
    for (int i = 0; i < image_n * image_c * image_h * image_w; i++)
    {
        res->data[i] = i % 255;//((*engine)() & 0xff);
    }
#endif
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
#if 0 // Check All
    static int cc = 0;
    if (c_idx == 0) {
        if (cc >= 0 * image_dw && cc < 0 * image_dw + 10){
            printf("(%f, %f) coef_xy: %f, %f, %f, %f, index: %d, %d, %d, %d, pixel: %x, %x, %x, %x, mix: %x\n",
            x_idx, y_idx,
            coef_x_ceil, coef_y_ceil, coef_x_floor, coef_y_floor,
            floor_y * width + floor_x, ceil_y * width + floor_x, floor_y * width + ceil_x, ceil_y * width + ceil_x,
            pixel_lu, pixel_ld, pixel_ru, pixel_rd, mix);
        }
        cc++;
    }
#endif

    return mix;
};

static int bmcv_perspective_nearest_cmp_1n(
                            unsigned char * p_exp,
                            unsigned char * p_got,
                            unsigned char * src,
                            int*            map,
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
                unsigned char got = p_got[c * image_dw_stride * image_dh + y * image_dw_stride + x * w_stride];
                unsigned char exp = p_exp[c * image_dw_stride * image_dh + y * image_dw_stride + x * w_stride];

                if (got != exp) {
                    // loopup up/down left/right by 1 pixel in source image.
                    int sx = map_x[y * image_dw + x];
                    int sy = map_y[y * image_dw + x];
                    //if(sx >= image_sw || sx < 0 || sy >= image_sh || sy < 0)
                    //    continue;
                    unsigned char left = fetch_pixel(sx + 1, sy, c, image_sw, image_sh, w_stride, src);
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

static int bmcv_perspective_bilinear_cmp_1n(
                            unsigned char * p_exp,
                            unsigned char * p_got,
                            unsigned char * src,
                            float*          map,
                            int             image_c,
                            int             image_sh,
                            int             image_sw,
                            int             image_dh,
                            int             image_dw) {

    // generate map.
    UNUSED(src);
    UNUSED(map);
    UNUSED(image_sh);
    UNUSED(image_sw);
    //float* map_x    = map;
    //float* map_y    = map + image_dh * image_dw;
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
                    //float sx = map_x[y * image_dw + x];
                    //float sy = map_y[y * image_dw + x];

                    //u8 mix_pixel = fetch_pixel(sx, sy, c, image_sw, image_sh, src);

                    if (abs(exp - got) <= 2) {
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

static int bmcv_warp_perspective_cmp(
                         u8_data p_exp,
                         u8_data p_got,
                         u8_data src,
                         int_data map,
                         int matrix_num[4],
                         bool use_bilinear,
                         int image_n,
                         int image_c,
                         int image_sh,
                         int image_sw,
                         int image_dh,
                         int image_dw) {
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
#if DEBUG_WARP
        write_file((char*)"warp_test.got.o", p_got->data + dst_offset, image_c * image_dh * image_dw);
        write_file((char*)"warp_test.exp.o", p_exp->data + dst_offset, image_c * image_dh * image_dw);
#endif
        if (use_bilinear)
            ret = bmcv_perspective_bilinear_cmp_1n(
                                   p_exp->data + dst_offset,
                                   p_got->data + dst_offset,
                                   src->data + src_offset,
                                   (float*)map->data + i * image_dh * image_dw * 2,
                                   image_c,
                                   image_sh,
                                   image_sw,
                                   image_dh,
                                   image_dw);
        else
            ret = bmcv_perspective_nearest_cmp_1n(
                                   p_exp->data + dst_offset,
                                   p_got->data + dst_offset,
                                   src->data + src_offset,
                                   map->data + i * image_dh * image_dw * 2,
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

static bm_status_t bmcv_perspective_bilinear_1n_ref(
                                    // input
                                    unsigned char *src_image,
                                    float *        trans_mat,
                                    int            image_c,
                                    int            image_sh,
                                    int            image_sw,
                                    int            image_dh,
                                    int            image_dw,
                                    // output
                                    float          *map,
                                    unsigned char  *dst_image,
                                    float_data     tensor_S) {
    UNUSED(image_c);
    // float_data tensor_S = MAKE_BLOB(float, image_dh *image_dw * 2);
    float* tensor_SX = tensor_S->data;
    float* tensor_SY = tensor_SX + image_dh * image_dw;
    float *tensor_DX = map;
    float *tensor_DY = tensor_DX + image_dh * image_dw;
    int dst_w = image_dw;
    int dst_h = image_dh;
    int src_w = image_sw;
    int src_h = image_sh;
    float m0 = trans_mat[0];
    float m1 = trans_mat[1];
    float m2 = trans_mat[2];
    float m3 = trans_mat[3];
    float m4 = trans_mat[4];
    float m5 = trans_mat[5];
    float m6 = trans_mat[6];
    float m7 = trans_mat[7];
    float m8 = trans_mat[8];

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
            float dz = tensor_SX[y * dst_w + x] * m6 +
                       tensor_SY[y * dst_w + x] * m7 + m8;
            dx = dx / dz;
            dy = dy / dz;

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
    tensor_DX         = map;
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

static bm_status_t bmcv_perspective_nearest_1n_ref(
                                    // input
                                    unsigned char *src_image,
                                    float *        trans_mat,
                                    int            image_c,
                                    int            image_sh,
                                    int            image_sw,
                                    int            image_dh,
                                    int            image_dw,
                                    // output
                                    int *map,
                                    unsigned char * dst_image) {
    UNUSED(image_c);
    float_data tensor_S   = MAKE_BLOB(float, image_dh *image_dw * 2);
    float*     tensor_SX  = tensor_S->data;
    float*     tensor_SY  = tensor_SX + image_dh * image_dw;
    int        *tensor_DX = map;
    int        *tensor_DY = tensor_DX + image_dh * image_dw;
    int        dst_w      = image_dw;
    int        dst_h      = image_dh;
    int        src_w      = image_sw;
    int        src_h      = image_sh;
    float      m0         = trans_mat[0];
    float      m1         = trans_mat[1];
    float      m2         = trans_mat[2];
    float      m3         = trans_mat[3];
    float      m4         = trans_mat[4];
    float      m5         = trans_mat[5];
    float      m6         = trans_mat[6];
    float      m7         = trans_mat[7];
    float      m8         = trans_mat[8];

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
            float dz = tensor_SX[y * dst_w + x] * m6 +
                       tensor_SY[y * dst_w + x] * m7 + m8;

            dx = dx / dz;
            dy = dy / dz;

            tensor_DX[y * dst_w + x] = (int)(dx + 0.5f);
            tensor_DY[y * dst_w + x] = (int)(dy + 0.5f);
            tensor_DX[y * dst_w + x] =
                bm_min(tensor_DX[y * dst_w + x], image_sw - 1);
            tensor_DX[y * dst_w + x] =
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
            int sx = tensor_DX[y * dst_w + x];
            int sy = tensor_DY[y * dst_w + x];
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

static bm_status_t bmcv_warp_perspective_ref(
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
    bool use_bilinear,
    // output
    int_data map,
    u8_data dst_image,
    float_data tensor_S)
{
    int output_num = 0;
    int matrix_sigma[4] = {0};
    bm_status_t ret = BM_SUCCESS;
    for (int i = 0; i < image_n; i++) {
        output_num += matrix_num[i];
        matrix_sigma[i] = output_num;
    }
    for (int i = 0; i < output_num; i++) {
        int s_idx = get_source_idx(i, matrix_sigma, image_n);
        int src_offset = s_idx * image_c * image_sh * image_sw;
        int dst_offset = i * image_c * image_dh * image_dw;
        if (use_bilinear)
            ret = bmcv_perspective_bilinear_1n_ref(
                    src_image->data + src_offset,
                    trans_mat->data + i * 9,
                    image_c,
                    image_sh,
                    image_sw,
                    image_dh,
                    image_dw,
                    (float*)map->data + i * image_dh * image_dw * 2,
                    dst_image->data + dst_offset,
                    tensor_S);
        else
            ret = bmcv_perspective_nearest_1n_ref(
                    src_image->data + src_offset,
                    trans_mat->data + i * 9,
                    image_c,
                    image_sh,
                    image_sw,
                    image_dh,
                    image_dw,
                    map->data + i * image_dh * image_dw * 2,
                    dst_image->data + dst_offset);

        if (ret != BM_SUCCESS)
            return ret;
    }
    return ret;
}

static bm_status_t bmcv_warp_perspective_tpu(
                                 bm_handle_t handle,
                                 // input
                                 u8_data src_data,
                                 float_data trans_mat,
                                 int_data coordinate,
                                 int matrix_num[4],
                                 int src_mode,
                                 bool use_bilinear,
                                 int loop_times,
                                 int image_n,
                                 int image_c,
                                 int image_sh,
                                 int image_sw,
                                 int image_dh,
                                 int image_dw,
                                 // output
                                 u8_data dst_data) {
    int output_num = 0;
    for (int i = 0; i < image_n; i++) {
        output_num += matrix_num[i];
    }
    UNUSED(loop_times);
    int b4N = (src_mode == STORAGE_MODE_4N_INT8);
    bmcv_perspective_image_matrix matrix_image[4];
    bmcv_perspective_matrix* matrix = (bmcv_perspective_matrix *)(trans_mat->data);
    for (int i = 0; i < image_n; i++) {
        matrix_image[i].matrix_num = matrix_num[i];
        matrix_image[i].matrix = matrix;
        matrix += matrix_num[i];
    }

    bmcv_perspective_image_coordinate coordinate_image[4];
    bmcv_perspective_coordinate* coord = (bmcv_perspective_coordinate *)(coordinate->data);
    for (int i = 0; i < image_n; i++) {
        coordinate_image[i].coordinate_num = matrix_num[i];
        coordinate_image[i].coordinate = coord;
        coord += matrix_num[i];
    }
    UNUSED(coordinate_image);
    bm_image src_img[4];
    bm_image_format_ext image_format = FORMAT_BGR_PLANAR;
    bm_image_data_format_ext data_type =
        b4N ? DATA_TYPE_EXT_4N_BYTE : DATA_TYPE_EXT_1N_BYTE;
    int in_image_num = b4N ? 1 : image_n;
    int out_image_num = b4N ? (ALIGN(output_num, 4) / 4) : output_num;
    // cte source image.
    if (b4N) {
        src_data = convert_1N_INT8_to_4N_INT8(src_data, image_n, image_c, image_sh, image_sw);
    }

    for (int i = 0; i < in_image_num; i++) {
        BM_CHECK_RET(bm_image_create(
            handle, image_sh, image_sw, image_format, data_type, src_img + i));
        int stride = 0;
        // debug
        bm_image_get_stride(src_img[i], &stride);

        void *ptr = (void *)(src_data->data + 3 * stride * image_sh * i);
        BM_CHECK_RET(bm_image_copy_host_to_device(src_img[i], (void **)(&ptr)));
    }

    // create dst image.
    image_data dst_img = MAKE_BLOB(bm_image, out_image_num);

    for (int i = 0; i < out_image_num; i++) {
        BM_CHECK_RET(bm_image_create(handle, image_dh, image_dw, image_format, data_type, dst_img->data + i));
    }
    struct timeval t1, t2;
	if (loop_times % 2 == 0) {
        printf("No coordinate\n");
	    gettimeofday_(&t1);
	    BM_CHECK_RET(
	        bmcv_image_warp_perspective(handle, image_n, matrix_image, src_img, dst_img->data, use_bilinear));
	    gettimeofday_(&t2);
	}
    if (loop_times % 2 == 1) {
        printf("No coordinate and similar to opencv\n");
        float matrix_tem[3][3];
        float matrix_tem_inv[3][3];
        for (int i = 0; i < image_n; i++) {
            for(int matrix_no = 0; matrix_no < matrix_image[i].matrix_num; matrix_no++){
                memset(matrix_tem, 0, sizeof(matrix_tem));
                memset(matrix_tem_inv, 0, sizeof(matrix_tem_inv));
                for(int a=0;a<9;a++){
                        matrix_tem[(a/3)][(a%3)]=matrix_image[i].matrix->m[a];
                }
                inverse_matrix(3, matrix_tem, matrix_tem_inv);
                for(int a=0;a<9;a++){
                        matrix_image[i].matrix->m[a]=matrix_tem_inv[(a/3)][(a%3)];
                }
            }
        }
        gettimeofday_(&t1);
        BM_CHECK_RET(
            bmcv_image_warp_perspective_similar_to_opencv(handle, image_n, matrix_image, src_img, dst_img->data, use_bilinear));
        gettimeofday_(&t2);
    }
    std::cout << "---warp_perspective TPU using time= " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "(us)" << std::endl;
    int size = 0;
    bm_image_get_byte_size(dst_img->data[0], &size);
    auto temp_out = MAKE_BLOB(u8, out_image_num * size);

    for (int i = 0; i < out_image_num; i++) {
        void *ptr = (void *)(temp_out->data + size * i);
        BM_CHECK_RET(bm_image_copy_device_to_host(dst_img->data[i], (void **)&ptr));
    }
    memcpy(dst_data->data, temp_out->data, output_num * image_c * image_dh * image_dw);

    return BM_SUCCESS;
}

static void test_cv_warp_perspective_single_case(
                                     u8_data        src_image,
                                     float_data     trans_mat,
                                     int_data       coordinate,
                                     int            matrix_num[4],
                                     int            src_mode,
                                     bool           use_bilinear,
                                     int            loop_times,
                                     int            image_n,
                                     int            image_c,
                                     int            image_sh,
                                     int            image_sw,
                                     int            image_dh,
                                     int            image_dw,
                                     float_data     tensor_S) {
    int output_num = 0;
    for (int i = 0; i < image_n; i++) {
        output_num += matrix_num[i];
    }
    auto map           = MAKE_BLOB(int, output_num *image_dh *image_dw * 2);
    auto dst_image_ref = MAKE_BLOB(u8, output_num * image_c * image_dh * image_dw);
    auto dst_image_tpu = MAKE_BLOB(u8, output_num * image_c * image_dh * image_dw);

    // calculate use reference for compare.
    bm_status_t ret_ref = bmcv_warp_perspective_ref(
                                        src_image,
                                        trans_mat,
                                        matrix_num,
                                        image_n,
                                        image_c,
                                        image_sh,
                                        image_sw,
                                        image_dh,
                                        image_dw,
                                        use_bilinear,
                                        map,
                                        dst_image_ref,
                                        tensor_S);
    if (ret_ref != BM_SUCCESS) {
        printf("run bm_warp_perspective_ref failed ret = %d\n", ret_ref);
        exit(-1);
    }
#ifdef OPENCV_SRC
    write_file((char*)"dst_ref.txt", dst_image_ref->data, image_c * image_dh * image_dw);
	cv::Mat my_mat(image_dh, image_dw, CV_8UC3, dst_image_ref->data);
    cv::imwrite("dst_ref.bmp", my_mat);
#endif
    // calculate use NPU.
    bm_status_t ret = bmcv_warp_perspective_tpu(
                                    handle,
                                    src_image,
                                    trans_mat,
                                    coordinate,
                                    matrix_num,
                                    src_mode,
                                    use_bilinear,
                                    loop_times,
                                    image_n,
                                    image_c,
                                    image_sh,
                                    image_sw,
                                    image_dh,
                                    image_dw,
                                    dst_image_tpu);
    if (ret != BM_SUCCESS) {
        printf("run bm_warp_perspective on tpu failed ret = %d\n", ret);
        exit(-1);
    }
#ifdef OPENCV_SRC
    write_file((char*)"dst_tpu.txt", dst_image_tpu->data, 3 * (image_dh * image_dw + 2));
	cv::Mat my_mat1(image_dh, image_dw, CV_8UC3, dst_image_tpu->data);
    cv::imwrite("dst_tpu.bmp", my_mat1);
#endif
    // compare.
    int cmp_res = bmcv_warp_perspective_cmp(
                                dst_image_ref,
                                dst_image_tpu,
                                src_image,
                                map,
                                matrix_num,
                                use_bilinear,
                                image_n,
                                image_c,
                                image_sh,
                                image_sw,
                                image_dh,
                                image_dw);
    if (cmp_res != 0) {
        printf("cv_warp_perspective comparing failed\n");
        exit(-1);
    }

}

static bm_status_t src_data_generation(int i, int_data coordinate, float_data trans_mat, float_data tensor_S) {
    int   border_x1                          = rand() % image_sw;
    int   border_x2                          = rand() % image_sw;
    while (border_x1 == border_x2) border_x2 = rand() % image_sw;
    int   border_y1                          = rand() % image_sh;
    int   border_y2                          = rand() % image_sh;
    while (border_y1 == border_y2) border_y2 = rand() % image_sh;
    int   x_min                              = bm_min(border_x1, border_x2);
    int   x_max                              = bm_max(border_x1, border_x2);
    int   y_min                              = bm_min(border_y1, border_y2);
    int   y_max                              = bm_max(border_y1, border_y2);

    int x[4], y[4];
    int sx[4], sy[4];
    int idx = rand() % 4;
    x   [0] = x_min; // x_min + rand() % (x_max - x_min);
    y   [0] = y_min;
    x   [1] = x_max;
    y   [1] = y_min; // y_min + rand() % (y_max - y_min);
    x   [2] = x_max; // x_max - rand() % (x_max - x_min);
    y   [2] = y_max;
    x   [3] = x_min;
    y   [3] = y_max; // y_max - rand() % (y_max - y_min);
    sx  [0] = x[(0 + idx) % 4];
    sy  [0] = y[(0 + idx) % 4];
    sx  [1] = x[(1 + idx) % 4];
    sy  [1] = y[(1 + idx) % 4];
    sx  [2] = x[(3 + idx) % 4];
    sy  [2] = y[(3 + idx) % 4];
    sx  [3] = x[(2 + idx) % 4];
    sy  [3] = y[(2 + idx) % 4];
    printf("src coordinate: (%d %d) (%d %d) (%d %d) (%d %d)\n", sx[0], sy[0], sx[1], sy[1], sx[2], sy[2], sx[3], sy[3]);

    coordinate->data[0 + i * 8] = sx[0];
    coordinate->data[1 + i * 8] = sx[1];
    coordinate->data[2 + i * 8] = sx[2];
    coordinate->data[3 + i * 8] = sx[3];
    coordinate->data[4 + i * 8] = sy[0];
    coordinate->data[5 + i * 8] = sy[1];
    coordinate->data[6 + i * 8] = sy[2];
    coordinate->data[7 + i * 8] = sy[3];

    float matrix_cv[9];
    my_get_perspective_transform(sx, sy, image_dw-1, image_dh-1, matrix_cv);
    trans_mat->data[0 + i * 9] = matrix_cv[0];
    trans_mat->data[1 + i * 9] = matrix_cv[1];
    trans_mat->data[2 + i * 9] = matrix_cv[2];
    trans_mat->data[3 + i * 9] = matrix_cv[3];
    trans_mat->data[4 + i * 9] = matrix_cv[4];
    trans_mat->data[5 + i * 9] = matrix_cv[5];
    trans_mat->data[6 + i * 9] = matrix_cv[6];
    trans_mat->data[7 + i * 9] = matrix_cv[7];
    trans_mat->data[8 + i * 9] = matrix_cv[8];

    printf("trans_mat->data[0 + i * 9] = %f\n", trans_mat->data[0 + i * 9]);
    printf("trans_mat->data[1 + i * 9] = %f\n", trans_mat->data[1 + i * 9]);
    printf("trans_mat->data[2 + i * 9] = %f\n", trans_mat->data[2 + i * 9]);
    printf("trans_mat->data[3 + i * 9] = %f\n", trans_mat->data[3 + i * 9]);
    printf("trans_mat->data[4 + i * 9] = %f\n", trans_mat->data[4 + i * 9]);
    printf("trans_mat->data[5 + i * 9] = %f\n", trans_mat->data[5 + i * 9]);
    printf("trans_mat->data[6 + i * 9] = %f\n", trans_mat->data[6 + i * 9]);
    printf("trans_mat->data[7 + i * 9] = %f\n", trans_mat->data[7 + i * 9]);
    printf("trans_mat->data[8 + i * 9] = %f\n", trans_mat->data[8 + i * 9]);

    float*     tensor_SX  = tensor_S->data;
    float*     tensor_SY  = tensor_SX + image_dh * image_dw;
    for (int y = 0; y < image_dh; y++) {
        for (int x = 0; x < image_dw; x++) {
            float dx = tensor_SX[y * image_dw + x] * trans_mat->data[0 + i * 9] +
                tensor_SY[y * image_dw + x] * trans_mat->data[1 + i * 9] + trans_mat->data[2 + i * 9];
            float dy = tensor_SX[y * image_dw + x] * trans_mat->data[3 + i * 9] +
                    tensor_SY[y * image_dw + x] * trans_mat->data[4 + i * 9] + trans_mat->data[5 + i * 9];
            float dz = tensor_SX[y * image_dw + x] * trans_mat->data[6 + i * 9] +
                    tensor_SY[y * image_dw + x] * trans_mat->data[7 + i * 9] + trans_mat->data[8 + i * 9];

            dx = dx / dz;
            dy = dy / dz;

            if (dx < MIN_INT || dx > MAX_INT || dy < MIN_INT || dy > MAX_INT || fabs(dz) == 0) {
                printf("--------- the input data is not leagel! --------- \n");
                return BM_ERR_DATA;
            }
        }
    }
    return BM_SUCCESS;
}

static void test_cv_warp_perspective_random(int trials) {
    int dev_id = 0;
    int loop_times = 0;
    bm_status_t ret = bm_dev_request(&handle, dev_id);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        exit(-1);
    }
    for (int idx_trial = 0; idx_trial < trials; idx_trial++) {
        int image_c = IMG_C;
        int matrix_num[4] = {0};
        bool use_bilinear = rand() % 2 ? true : false;
        use_bilinear = true;
        printf("use_bilinear: %d\n", use_bilinear);
        int src_mode = (rand() & 0x01) ? STORAGE_MODE_1N_INT8 : STORAGE_MODE_4N_INT8;
        src_mode = STORAGE_MODE_1N_INT8;
        int image_n = 1 + (rand() & 0x3);
        if (flag == 1){
            image_sh = (rand() & 0x7fff);
            image_sw = (rand() & 0x7fff);
            while (image_sh > 2160 || image_sh < 1080) {
                image_sh = (rand() & 0x7fff);
            }
            while (image_sw > 3840 || image_sw < 1920) {
                image_sw = (rand() & 0x7fff);
            }

            image_dh = (rand() & 0x3ff) + 1;
            image_dw = (rand() & 0x3ff) + 1;
        }

        if (flag == 2){
            image_sh = (rand() & 0x7fff);
            image_sw = (rand() & 0x7fff);
            while (image_sh > 3840 || image_sh < 2048) {
                image_sh = (rand() & 0x7fff);
            }
            while (image_sw > 3840 || image_sw < 2048) {
                image_sw = (rand() & 0x7fff);
            }

            image_dh = (rand() & 0x7fff) + 1;
            image_dw = (rand() & 0x7fff) + 1;

            while (image_dh > 2048 || image_dh < 1024) {
                image_dh = (rand() & 0x7fff);
            }
            while (image_dw > 2048 || image_dw < 1024) {
                image_dw = (rand() & 0x7fff);
            }
        }

        if (flag == 3){
            image_sh = (rand() & 0x7fff);
            image_sw = (rand() & 0x7fff);
            while (image_sh > 5000 || image_sh < 4096) {
                image_sh = (rand() & 0x7fff);
            }
            while (image_sw > 5000 || image_sw < 4096) {
                image_sw = (rand() & 0x7fff);
            }

            image_dh = (rand() & 0x7fff) + 1;
            image_dw = (rand() & 0x7fff) + 1;

            while (image_dh > 4096 || image_dh < 2048) {
                image_dh = (rand() & 0x7fff);
            }
            while (image_dw > 4096 || image_dw < 2048) {
                image_dw = (rand() & 0x7fff);
            }
        }

        matrix_num[0] = 1;
        matrix_num[1] = 1;
        matrix_num[2] = 1;
        matrix_num[3] = 1;
#if DEBUG_WARP
        image_n = 1;
        matrix_num[0] = 1;
        image_sh = SRC_H;
        image_sw = SRC_W;
        image_dh = SRC_H;
        image_dw = SRC_W;
        src_mode = STORAGE_MODE_4N_INT8;
#endif
        printf("image_n %d mode :%d input[%dx%d] output[%dx%d]\n",
                image_n,
                src_mode,
                image_sw,
                image_sh,
                image_dw,
                image_dh);
        int output_num = 0;
        printf("matrix number: ");
        for (int i = 0; i < image_n; i++) {
            output_num += matrix_num[i];
            printf("%d ", matrix_num[i]);
        }
        printf("total %d\n", output_num);
        if (!output_num) {
            return;
        }

        bm_status_t ret        = BM_ERR_FAILURE;
        auto        src_data   = image_read(image_n, image_c, image_sh, image_sw);
#ifdef OPENCV_SRC
        cv::Mat my_mat(image_sh, image_sw, CV_8UC3, src_data->data);
        cv::imwrite("input.png", my_mat);
#endif
        float_data  trans_mat  = MAKE_BLOB(float, output_num * 9);
        int_data    coordinate = MAKE_BLOB(int, output_num * 8);
        float_data  tensor_S   = MAKE_BLOB(float, image_dh *image_dw * 2);
        float*      tensor_SX  = tensor_S->data;
        float*      tensor_SY  = tensor_SX + image_dh * image_dw;
        for (int y = 0; y < image_dh; y++) {
            for (int x = 0; x < image_dw; x++) {
                tensor_SX[y * image_dw + x] = (float)x;
                tensor_SY[y * image_dw + x] = (float)y;
            }
        }

        for (int i = 0; i < output_num; i++) {
            ret = src_data_generation(i, coordinate, trans_mat, tensor_S);
            while (BM_ERR_DATA == ret)
                ret = src_data_generation(i, coordinate, trans_mat, tensor_S);
        }
#if DEBUG_WARP
        int dx[4] = {0, 1920, 0, 1920};
        int dy[4] = {0, 0, 1080, 1080};
        int sx[4] = {1000, 1500, 500, 1900};
        int sy[4] = {300, 500, 1080, 1080};
        float* matrix_cv = new float [9];
        opencv_get_perspective_transform(dx, dy, sx, sy, matrix_cv);
        for (int i = 0; i < 9; i++) {
            printf("m[%d] = %f\n", i, matrix_cv[i]);
        }
        for (int i = 0; i < output_num; i++) {
            trans_mat->data[0 + i * 9] = matrix_cv[0];
            trans_mat->data[1 + i * 9] = matrix_cv[1];
            trans_mat->data[2 + i * 9] = matrix_cv[2];
            trans_mat->data[3 + i * 9] = matrix_cv[3];
            trans_mat->data[4 + i * 9] = matrix_cv[4];
            trans_mat->data[5 + i * 9] = matrix_cv[5];
            trans_mat->data[6 + i * 9] = matrix_cv[6];
            trans_mat->data[7 + i * 9] = matrix_cv[7];
            trans_mat->data[8 + i * 9] = matrix_cv[8];
        }
        delete [] matrix_cv;
#endif
        test_cv_warp_perspective_single_case(
                                    src_data,
                                    trans_mat,
                                    coordinate,
                                    matrix_num,
                                    src_mode,
                                    use_bilinear,
                                    loop_times,
                                    image_n,
                                    image_c,
                                    image_sh,
                                    image_sw,
                                    image_dh,
                                    image_dw,
                                    tensor_S);
        loop_times++;
    }
    bm_dev_free(handle);
    printf("cv_warp_perspective random tests done!\n");
}

int main(int argc, char *argv[]) {
    int test_loop_times = 1;
    int thread_num = 1;
    int seed = -1;
    bool fixed = false;

    if (argc > 1){
        flag = atoi(argv[1]);
    }

    if (argc > 2){
        flag = atoi(argv[1]);
        image_sh = atoi(argv[2]);
        image_sw = atoi(argv[3]);
        image_dh = atoi(argv[4]);
        image_dw = atoi(argv[5]);
    }

    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST WARP] loop times should be 1~1500" << std::endl;
        exit(-1);
    }
    if (thread_num < 1 || thread_num > 4) {
        std::cout << "[TEST WARP PERSPECTIVE] thread_num should be 1~4" << std::endl;
        exit(-1);
    }

    std::cout << "[TEST WARP] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST WARP] LOOP " << loop_idx << "------"
                  << std::endl;

        struct timespec tp;
        clock_gettime_(0, &tp);

        seed = (fixed) ? seed : tp.tv_nsec;
        srand(seed);
        printf("random seed %d\n", seed);
        test_cv_warp_perspective_random(3);
    }
    std::cout << "------[TEST WARP PERSPECTIVE] ALL TEST PASSED!" << std::endl;

    return 0;
}
