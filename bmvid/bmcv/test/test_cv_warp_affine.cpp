#include <iostream>
#include <math.h>
#include <sys/time.h>
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

static  int image_sh = 3816;
static  int image_sw = 2729;
static  int image_dh = 1555;
static  int image_dw = 1909;
static int flag = 0;
static bm_handle_t handle;
static std::shared_ptr<ThreadPool> pool = std::make_shared<ThreadPool>(1);
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

float det(float arcs[3][3],int n){
    if(n == 1){
        return arcs[0][0];
    }
    float ans = 0;
    float temp[3][3];
    int i,j,k;
    for(i = 0;i < n;i++){
        for(j = 0;j < n - 1;j++){
            for(k = 0;k < n - 1;k++){
                temp[j][k] = arcs[j+1][(k >= i) ? k+1 : k];
            }
        }
        float t = det(temp, n-1);
        if(i%2 == 0){
            ans += arcs[0][i] * t;
        }
        else{
            ans -=  arcs[0][i] * t;
        }
    }
    return ans;
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

static u8_data image_read(
                       int            image_n,
                       int            image_c,
                       int            image_sh,
                       int            image_sw,
                       int            image_dh,
                       int            image_dw) {
    auto res = MAKE_BLOB(u8, image_n * image_c * image_sh * image_sw);
    auto res_temp = MAKE_BLOB(u8, image_n * image_c * image_dh * image_dw);
    auto res_temp_bak = MAKE_BLOB(u8, image_n * image_c * image_dh * image_dw);
    for (int i = 0; i < image_n * image_c * image_sh * image_sw; i++)
    {
        res->data[i] = i % 255;
    }

    if (image_dh <= image_sh && image_dw <= image_sw)
        return res;

    if (image_dh > image_sh){
        int pad_h_value = (image_dh - image_sh) / 2;
        for (int i = 0;i < pad_h_value * image_sw;i++)
            res_temp->data[i] = 0;

        for (int i = pad_h_value * image_sw; i < pad_h_value * image_sw + image_n * image_c * image_sh * image_sw;i++)
            res_temp->data[i] = res->data[i];

        for (int i = pad_h_value * image_sw + image_n * image_c * image_sh * image_sw;i <  pad_h_value * image_sw + image_n * image_c * image_sh * image_sw + pad_h_value * image_sw;i++)
            res_temp->data[i] = 0;
    }

    if (image_dw > image_sw){
        int pad_w_value = (image_dw - image_sw) / 2;
        for (int i = 0;i < image_dh;i++){
            for (int i = 0;i < pad_w_value;i++)
                res_temp_bak->data[i] = 0;
            for (int i = pad_w_value;i < pad_w_value + image_sw;i++)
                res_temp_bak->data[i] = res_temp->data[i-pad_w_value];
            for (int i = pad_w_value + image_sw;i < pad_w_value + image_sw + pad_w_value + image_sw;i++)
                res_temp_bak->data[i] = 0;
        }
    }

    return res_temp_bak;
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

void inverse_matrix(float matrix[3][3], float matrix_inv[2][3]){
    float det_value = det(matrix, 3);
    matrix_inv[0][0] = matrix[1][1] / det_value;
    matrix_inv[0][1] = -matrix[0][1] / det_value;
    matrix_inv[0][2] = (matrix[0][1] * matrix[1][2] - matrix[0][2] * matrix[1][1]) / det_value;
    matrix_inv[1][0] = - matrix[1][0] / det_value;
    matrix_inv[1][1] = matrix[0][0] / det_value;
    matrix_inv[1][2] = (matrix[0][2] * matrix[1][0] - matrix[0][0] * matrix[1][2]) / det_value;
}

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
                         int       image_dh,
                         int       image_dw
                         ) {
    int ret             = 0;
    int count = 0;
    int i = 0;
    for (i = 0; i < image_dh; i++) {
        for (int j = 0;j < image_dw;j++){
             if (abs(p_exp->data[count] - p_got->data[count]) > 1){
                std::cout << "image_dh = " << image_dh << "\n" << "image_dw = " << image_dw  << "\n" << std::endl;
                std::cout << "i = " << i << "\n" << "j = " << j  << "\n" << std::endl;
                std::cout << "p_exp->data = " << p_exp->data[count] << std::endl;
                std::cout << "p_got->data = " << p_got->data[count] << std::endl;
                return -1;
            }
            count++;
        }
    }

    return ret;
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
                                    unsigned char *dst_image,
                                    bool use_opencv) {

    UNUSED(image_c);
    float_data tensor_S = MAKE_BLOB(float, image_dh *image_dw * 2);
    float *tensor_SX    = tensor_S->data;
    float *   tensor_SY = tensor_SX + image_dh * image_dw;
    int *tensor_DX      = map;
    int *tensor_DY      = tensor_DX + image_dh * image_dw;
    int             dst_w     = image_dw;
    int             dst_h     = image_dh;
    int             src_w     = image_sw;
    int             src_h     = image_sh;

    if (use_opencv){
        float mat_array[6];
        float matrix_tem[3][3];
        float matrix_tem_inv[2][3];
        memset(matrix_tem, 0, sizeof(matrix_tem));
        memset(matrix_tem_inv, 0, sizeof(matrix_tem_inv));
        for(int a = 0;a < 6;a++){
            matrix_tem[(a/3)][(a%3)] = trans_mat[a];
        }
        matrix_tem[2][0] = 0;
        matrix_tem[2][1] = 0;
        matrix_tem[2][2] = 1;
        inverse_matrix(matrix_tem, matrix_tem_inv);
        for(int a = 0;a < 6;a++){
            mat_array[a] = matrix_tem_inv[(a/3)][(a%3)];
        }
        float           m0        = mat_array[0];
        float           m1        = mat_array[1];
        float           m2        = mat_array[2];
        float           m3        = mat_array[3];
        float           m4        = mat_array[4];
        float           m5        = mat_array[5];

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
                // tensor_DX[y * dst_w + x] = (unsigned short)(dx + 0.5f);
                // tensor_DY[y * dst_w + x] = (unsigned short)(dy + 0.5f);
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
    }else{
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
                // tensor_DX[y * dst_w + x] = (unsigned short)(dx + 0.5f);
                // tensor_DY[y * dst_w + x] = (unsigned short)(dy + 0.5f);
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
                                    unsigned char *dst_image,
                                    bool opencv_mode) {
    UNUSED(image_c);
    float_data tensor_S = MAKE_BLOB(float, image_dh *image_dw * 2);
    float *tensor_SX = tensor_S->data;
    float *tensor_SY = tensor_SX + image_dh * image_dw;
    float *tensor_DX = map_index;
    float *tensor_DY = tensor_DX + image_dh * image_dw;
    int             dst_w     = image_dw;
    int             dst_h     = image_dh;
    int             src_w     = image_sw;
    int             src_h     = image_sh;

    if (opencv_mode){
        float mat_array[6];
        float matrix_tem[3][3];
        float matrix_tem_inv[2][3];
        memset(matrix_tem, 0, sizeof(matrix_tem));
        memset(matrix_tem_inv, 0, sizeof(matrix_tem_inv));
        for(int a = 0;a < 6;a++){
            matrix_tem[(a/3)][(a%3)] = trans_mat[a];
        }
        matrix_tem[2][0] = 0;
        matrix_tem[2][1] = 0;
        matrix_tem[2][2] = 1;
        inverse_matrix(matrix_tem, matrix_tem_inv);
        for(int a = 0;a < 6;a++){
            mat_array[a] = matrix_tem_inv[(a/3)][(a%3)];
        }
        float           m0        = mat_array[0];
        float           m1        = mat_array[1];
        float           m2        = mat_array[2];
        float           m3        = mat_array[3];
        float           m4        = mat_array[4];
        float           m5        = mat_array[5];

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
    } else {
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
    }

    int w_stride = 1;
    int src_w_stride = src_w * w_stride;
    int dst_w_stride = dst_w * w_stride;

    if (image_c == 3) {
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
    }else {
        // warp in source image directly.
        unsigned char *sb = src_image;
        unsigned char *db = dst_image;
        tensor_DX         = map_index;
        tensor_DY         = tensor_DX + dst_h * dst_w;
        for (int y = 0; y < dst_h; y++) {
            for (int x = 0; x < dst_w; x++) {
                float sx = tensor_DX[y * dst_w + x];
                float sy = tensor_DY[y * dst_w + x];
                db[y * dst_w_stride + x * w_stride] =
                    fetch_pixel(sx, sy, 0, image_sw, image_sh, sb);
            }
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
    u8_data dst_image,
    bool use_opencv)
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
                dst_image->data + dst_offset,
                use_opencv);
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
                dst_image->data + dst_offset,
                use_opencv);
    }
    return BM_SUCCESS;
}

static bm_status_t bmcv_warp_tpu(bm_handle_t handle,
                                 // input
                                 u8_data src_data,
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
                                 u8_data dst_data,
                                 bool use_opencv) {
    int output_num = 0;
    for (int i = 0; i < image_n; i++) {
        output_num += matrix_num[i];
    }

    bmcv_affine_image_matrix matrix_image[4];
    bmcv_affine_matrix *     matrix = (bmcv_affine_matrix *)(trans_mat->data);
    for (int i = 0; i < image_n; i++) {
        matrix_image[i].matrix_num = matrix_num[i];
        matrix_image[i].matrix     = matrix;
        matrix += matrix_num[i];
    }

    bm_image                 src_img[4];
    bm_image_format_ext      image_format = FORMAT_BGR_PLANAR;
    bm_image_data_format_ext data_type    = DATA_TYPE_EXT_1N_BYTE;

    for (int i = 0; i < image_n; i++){
        BM_CHECK_RET(bm_image_create(
            handle, image_sh, image_sw, image_format, data_type, src_img + i));
        int stride = 0;
        bm_image_get_stride(src_img[i], &stride);

        void *ptr = (void *)(src_data->data + 3 * stride * image_sh * i);
        BM_CHECK_RET(
            bm_image_copy_host_to_device(src_img[i], (void **)(&ptr)));
    }

    // create dst image.
    image_data dst_img = MAKE_BLOB(bm_image, image_n);

    for (int i = 0; i < image_n; i++) {
        BM_CHECK_RET(bm_image_create(
            handle, image_dh, image_dw, image_format, data_type, dst_img->data + i));
    }
    struct timeval t1, t2;
    if (!use_opencv) {
        printf("normal mode\n");
        gettimeofday_(&t1);
        bmcv_image_warp_affine(handle, image_n, matrix_image, src_img, dst_img->data, is_bilinear);
        gettimeofday_(&t2);
    }else{
        printf("opencv mode\n");
        gettimeofday_(&t1);
        bmcv_image_warp_affine_similar_to_opencv(handle, image_n, matrix_image, src_img, dst_img->data, is_bilinear);
        gettimeofday_(&t2);
    }

    std::cout << "---warp_affine TPU using time= " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "(us)" << std::endl;

    int size = 0;
    bm_image_get_byte_size(dst_img->data[0], &size);
    auto temp_out = MAKE_BLOB(u8, output_num * size);

    for (int i = 0; i < image_n; i++) {
        void *ptr = (void *)(temp_out->data + size * i);
        BM_CHECK_RET(
            bm_image_copy_device_to_host(dst_img->data[i], (void **)&ptr));
    }
    memcpy(dst_data->data, temp_out->data, image_n * image_c * image_dh * image_dw);

    for (int i = 0; i < image_n; i++){
        bm_image_destroy(src_img[i]);
    }

    return BM_SUCCESS;
}

static void test_cv_warp_single_case(u8_data        src_image,
                                     float_data     trans_mat,
                                     int            matrix_num[4],
                                     int            image_n,
                                     int            image_c,
                                     int            image_sh,
                                     int            image_sw,
                                     int            image_dh,
                                     int            image_dw,
                                     bool           use_opencv,
                                     bool           is_bilinear) {
    int output_num = 0;
    int cmp_res = 0;
    for (int i = 0; i < image_n; i++) {
        output_num += matrix_num[i];
    }
    auto map           = MAKE_BLOB(int, output_num *image_dh *image_dw * 2);
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
                                        dst_image_ref,
                                        use_opencv);
    if (ret_ref != BM_SUCCESS) {
        printf("run bm_warp_ref failed ret = %d\n", ret_ref);
        exit(-1);
    }

    // calculate use NPU.
    bm_status_t ret = bmcv_warp_tpu(handle,
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
                                    dst_image_tpu,
                                    use_opencv);
    if (ret != BM_SUCCESS) {
        printf("run bm_warp failed ret = %d\n", ret);
        exit(-1);
    }
    // compare.
    cmp_res = bmcv_warp_cmp(dst_image_ref,
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
    for (int idx_trial = 0; idx_trial < trials; idx_trial++) {
        auto task = []() {
            int image_c = IMG_C;
            int matrix_num[4] = {0};

            auto engine = pool->get_random_engine();
            bool is_bilinear = ((*engine)() & 0x01) ? true : false;
            printf("is_bilinear: %d \n", is_bilinear);
            int src_mode = ((*engine)() & 0x01) ? STORAGE_MODE_1N_INT8 : STORAGE_MODE_4N_INT8;
            src_mode = STORAGE_MODE_1N_INT8;
            int image_n = rand() % 0x03 + 1;
            bool use_opencv = rand() % 0x02 ? true : false;
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

            printf("image_n %d mode :%d input[%dx%d] output[%dx%d]\n",
                   image_n,
                   src_mode,
                   image_sw,
                   image_sh,
                   image_dw,
                   image_dh);
            int output_num = 0;
            for (int i = 0; i < image_n; i++){
                output_num += matrix_num[i];
            }
            if (!output_num){
                return;
            }

            auto src_data = image_read(image_n, image_c, image_sh, image_sw, image_dh, image_dw);
            float_data trans_mat = MAKE_BLOB(float, output_num * 6);

            for (int i = 0; i < output_num; i++){
                trans_mat->data[0 + i * 6] = 3.84843f;
                trans_mat->data[1 + i * 6] = -0.0248411f;
                trans_mat->data[2 + i * 6] = 916.203f;
                trans_mat->data[3 + i * 6] = 0.0248411;
                trans_mat->data[4 + i * 6] = 3.84843f;
                trans_mat->data[5 + i * 6] = 55.9748f;
            }
            test_cv_warp_single_case(src_data, trans_mat, matrix_num, image_n, image_c,
                                            image_sh, image_sw, image_dh, image_dw, use_opencv, is_bilinear);
        };
        pool->enqueue(task);
    }
    pool->wait_all_done();
    bm_dev_free(handle);
    printf("cv_warp random tests done!\n");
}

int main(int argc, char *argv[]) {
    int         dev_id = 0;
    int seed           = -1;
    bool fixed         = false;
    bm_status_t ret    = bm_dev_request(&handle, dev_id);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        exit(-1);
    }
    int test_loop_times = 1;

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
        test_cv_warp_random(2);
    }
    std::cout << "------[TEST WARP] ALL TEST PASSED!" << std::endl;

    return 0;
}
