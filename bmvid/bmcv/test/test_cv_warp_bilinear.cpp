#include <iostream>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "test_cv_naive_threadpool.hpp"

#define IMG_N 4
#define IMG_C 3
#define SRC_H 1080
#define SRC_W 1920
#define DST_H 112
#define DST_W 112

static bm_handle_t handle;
static std::shared_ptr<ThreadPool> pool = std::make_shared<ThreadPool>(1);

/*
typedef bm_status_t (*bmcv_warp_affine_bilinear_fp)(
    bm_handle_t, bm_image, int, bm_image *, bmcv_affine_matrix *);
static bmcv_warp_affine_bilinear_fp bmcv_warp_affine_bilinear = nullptr;
*/
template <typename T>
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

static int bmcv_warp_cmp_1n(unsigned char * p_exp,
                            unsigned char * p_got,
                            unsigned char * src,
                            float*            map,
                            int             image_c,
                            int             image_sh,
                            int             image_sw,
                            int             image_dh,
                            int             image_dw) {
    // generate map.
    float* map_x    = map;
    float* map_y    = map + image_dh * image_dw;

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

static int bmcv_warp_cmp(u8_data p_exp,
                         u8_data p_got,
                         u8_data src,
                         float_data map_index,
                         int             output_num,
                         int             image_c,
                         int             image_sh,
                         int             image_sw,
                         int             image_dh,
                         int             image_dw) {
    int ret             = 0;

    for (int i = 0; i < output_num; i++) {
        int dst_offset = i * image_c * image_dh * image_dw;
        ret = bmcv_warp_cmp_1n(p_exp->data + dst_offset,
                               p_got->data + dst_offset,
                               src->data,
                               map_index->data + i * image_dh * image_dw * 2,
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

static bm_status_t bmcv_warp_1n_ref(
                                    // input
                                    unsigned char *src_image,
                                    float *        trans_mat,
                                    int            image_c,
                                    int            image_sh,
                                    int            image_sw,
                                    int            image_dh,
                                    int            image_dw,
                                    // output
                                    float *map_index,
                                    unsigned char * dst_image) {
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
    int output_num,
    int image_c,
    int image_sh,
    int image_sw,
    int image_dh,
    int image_dw,
    // output
    float_data map_index,
    u8_data dst_image)
{
    for (int i = 0; i < output_num; i++) {
        int dst_offset = i * image_c * image_dh * image_dw;
        bmcv_warp_1n_ref(
            src_image->data,
            trans_mat->data + i * 6,
            image_c,
            image_sh,
            image_sw,
            image_dh,
            image_dw,
            map_index->data + i * image_dh * image_dw * 2,
            dst_image->data + dst_offset);
    }
    return BM_SUCCESS;
}

static bm_status_t bmcv_warp_bilinear_tpu(bm_handle_t handle,
                                 // input
                                 u8_data src_data,
                                float_data trans_mat,
                                 int output_num,
                                 int image_sh,
                                 int image_sw,
                                 int image_dh,
                                 int image_dw,
                                 // output
                                 u8_data dst_data) {

    auto              matrix_data = MAKE_BLOB(bmcv_affine_matrix, output_num);
    for (int i = 0; i < output_num; i++) {
        for (int k = 0; k < 6; k++)
        {
            matrix_data->data[i].m[k]  = trans_mat->data[6 * i + k];
        }
    }

    bm_image                 src_img;
    bm_image_format_ext      image_format = FORMAT_BGR_PLANAR;
    bm_image_data_format_ext data_type    = DATA_TYPE_EXT_1N_BYTE;

    BM_CHECK_RET(bm_image_create(
        handle, image_sh, image_sw, image_format, data_type, &src_img));
    int stride = 0;
    bm_image_get_stride(src_img, &stride);
    BM_CHECK_RET(bm_image_copy_host_to_device(src_img, (void **)(&(src_data->data))));

    // create dst image.
    image_data dst_img = MAKE_BLOB(bm_image, output_num);

    for (int i = 0; i < output_num; i++) {
        BM_CHECK_RET(bm_image_create(
            handle, image_dh, image_dw, image_format, data_type, dst_img->data + i));
    }

    BM_CHECK_RET(bmcv_warp_affine_bilinear(
        handle, src_img, output_num, dst_img->data, matrix_data->data));

    int size = 0;
    bm_image_get_byte_size(dst_img->data[0], &size);

    for (int i = 0; i < output_num; i++) {
        void *ptr = (void *)(dst_data->data + size * i);
        BM_CHECK_RET(
            bm_image_copy_device_to_host(dst_img->data[i], (void **)&ptr));
    }

    bm_image_destroy(src_img);

    for (int i = 0; i < output_num; i++)
    {
        bm_image_destroy(dst_img->data[i]);
    }
    return BM_SUCCESS;
}
static void test_cv_warp_single_case(u8_data src_image,
                                     float_data        trans_mat,
                                     int            output_num,
                                     int            image_c,
                                     int            image_sh,
                                     int            image_sw,
                                     int            image_dh,
                                     int            image_dw) {

    auto map_index = MAKE_BLOB(float, output_num * image_dh * image_dw * 2);

    auto dst_image_ref = MAKE_BLOB(u8, output_num * image_c * image_dh * image_dw);
    auto dst_image_tpu = MAKE_BLOB(u8, output_num * image_c * image_dh * image_dw);

    // calculate use reference for compare.
    bm_status_t ret_ref = bmcv_warp_ref(
                                        src_image,
                                        trans_mat,
                                        output_num,
                                        image_c,
                                        image_sh,
                                        image_sw,
                                        image_dh,
                                        image_dw,
                                        map_index,
                                        dst_image_ref);
    if (ret_ref != BM_SUCCESS) {
        printf("run bm_warp_ref failed ret = %d\n", ret_ref);
        exit(-1);
    }

    // calculate use NPU.
    bm_status_t ret = bmcv_warp_bilinear_tpu(handle,
                                    src_image,
                                    trans_mat,
                                    output_num,
                                    image_sh,
                                    image_sw,
                                    image_dh,
                                    image_dw,
                                    dst_image_tpu);
    if (ret != BM_SUCCESS) {
        printf("run bm_warp failed ret = %d\n", ret);
        exit(-1);
    }
    // compare.
    int cmp_res = bmcv_warp_cmp(dst_image_ref,
                                dst_image_tpu,
                                src_image,
                                map_index,
                                output_num,
                                image_c,
                                image_sh,
                                image_sw,
                                image_dh,
                                image_dw);
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

            auto engine = pool->get_random_engine();

            int output_num = 1 + ((*engine)() & 0x8);
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


            printf("output_num %d input [%dx%d] output [%dx%d]\n",
                   output_num,
                   image_sw,
                   image_sh,
                   image_dw,
                   image_dh);


            auto src_data = image_read(1, image_c, image_sh, image_sw);
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
                                     output_num,
                                     image_c,
                                     image_sh,
                                     image_sw,
                                     image_dh,
                                     image_dw);
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
        #ifdef __linux__
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
        #else
        clock_gettime(0, &tp);
        #endif
        pool->srand(tp.tv_nsec);
        printf("random seed %lu\n", tp.tv_nsec);
        test_cv_warp_random(10);
        pool->wait_all_done();
    }
    std::cout << "------[TEST WARP] ALL TEST PASSED!" << std::endl;

    return 0;
}