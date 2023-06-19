#include <iostream>
#include <memory>
#ifdef __linux__
#include <sys/time.h>
#else
#include <windows.h>
#include "time.h"
#endif

#include "bmcv_api_ext.h"
#include "test_misc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define DEBUG_FILE (0)

#define MODE (STORAGE_MODE_1N_INT8)

using namespace std;

static bm_handle_t handle;

class Blob {
  public:
    unsigned char *data      = NULL;
    int            byte_size = 0;

    explicit Blob(int size) {
        if (size == 0) {
            data      = NULL;
            byte_size = 0;
            return;
        }
        data      = (unsigned char *)malloc(size);
        byte_size = size;
    }
    ~Blob() {
        if (data) {
            free(data);
        }
    }
};

static void image_fill(unsigned char *image, int data_count) {
    for (int i = 0; i < data_count; i++) {
        image[i] = 10;  //(rand() & 0xff);
    }
}

static int
bmcv_width_align_cmp(unsigned char *p_exp, unsigned char *p_got, int count) {
    int ret = 0;
    for (int j = 0; j < count; j++) {
        if (p_exp[j] != p_got[j]) {
            printf("error: when idx=%d,  exp=%d but got=%d\n",
                   j,
                   (int)p_exp[j],
                   (int)p_got[j]);
            return -1;
        }
    }

    return ret;
}

static bm_status_t bmcv_width_align_ref(
    // input
    unsigned char *     src_image,
    bm_image_format_ext format,
    int                 image_h,
    int                 image_w,
    int *               stride_src,
    int *               stride_dst,
    // output
    unsigned char *dst_image) {
    unsigned char *src_offset;
    unsigned char *dst_offset;
    if (format == FORMAT_BGR_PLANAR) {
        for (int i = 0; i < 3 * image_h; i++) {
            src_offset = src_image + i * stride_src[0];
            dst_offset = dst_image + i * stride_dst[0];
            memcpy(dst_offset, src_offset, image_w);
        }
    } else if (format == FORMAT_BGR_PACKED) {
        for (int i = 0; i < image_h; i++) {
            src_offset = src_image + i * stride_src[0];
            dst_offset = dst_image + i * stride_dst[0];
            memcpy(dst_offset, src_offset, image_w * 3);
        }
    } else if (format == FORMAT_NV12) {
        for (int i = 0; i < image_h; i++) {
            src_offset = src_image + i * stride_src[0];
            dst_offset = dst_image + i * stride_dst[0];
            memcpy(dst_offset, src_offset, image_w);
        }
        for (int i = 0; i < image_h / 2; i++) {
            src_offset =
                src_image + image_h * stride_src[0] + i * stride_src[1];
            dst_offset =
                dst_image + image_h * stride_dst[0] + i * stride_dst[1];
            memcpy(dst_offset, src_offset, image_w);
        }
    } else if (format == FORMAT_NV16) {
        for (int i = 0; i < image_h; i++) {
            src_offset = src_image + i * stride_src[0];
            dst_offset = dst_image + i * stride_dst[0];
            memcpy(dst_offset, src_offset, image_w);
        }
        for (int i = 0; i < image_h; i++) {
            src_offset =
                src_image + image_h * stride_src[0] + i * stride_src[1];
            dst_offset =
                dst_image + image_h * stride_dst[0] + i * stride_dst[1];
            memcpy(dst_offset, src_offset, image_w);
        }
    } else if (format == FORMAT_YUV420P) {
        for (int i = 0; i < image_h; i++) {
            src_offset = src_image + i * stride_src[0];
            dst_offset = dst_image + i * stride_dst[0];
            memcpy(dst_offset, src_offset, image_w);
        }
        for (int i = 0; i < image_h / 2; i++) {
            src_offset =
                src_image + image_h * stride_src[0] + i * stride_src[1];
            dst_offset =
                dst_image + image_h * stride_dst[0] + i * stride_dst[1];
            memcpy(dst_offset, src_offset, image_w / 2);
        }
        for (int i = 0; i < image_h / 2; i++) {
            src_offset = src_image + image_h * stride_src[0] +
                         image_h / 2 * stride_src[1] + i * stride_src[2];
            dst_offset = dst_image + image_h * stride_dst[0] +
                         image_h / 2 * stride_dst[1] + i * stride_dst[2];
            memcpy(dst_offset, src_offset, image_w / 2);
        }
    } else if (format == FORMAT_GRAY) {
        for (int i = 0; i < image_h; i++) {
            src_offset = src_image + i * stride_src[0];
            dst_offset = dst_image + i * stride_dst[0];
            memcpy(dst_offset, src_offset, image_w);
        }
    }
    return BM_SUCCESS;
}

static bm_status_t test_cv_width_align(int trials) {
    int         dev_id = 0;
    bm_status_t ret    = bm_dev_request(&handle, dev_id);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        exit(-1);
    }
    bm_image src_img;
    bm_image dst_img;

    for (int idx_trial = 0; idx_trial < trials; idx_trial++) {
        int image_h = (rand() & 0x7fff);
        int image_w = (rand() & 0x7fff);
        while (image_h > 2160 || image_h < 1080) {
            image_h = (rand() & 0x7fff);
        }
        while (image_w > 3840 || image_w < 1920) {
            image_w = (rand() & 0x7fff);
        }

        int                      default_stride[3] = {0};
        int                      src_stride[3]     = {0};
        int                      dst_stride[3]     = {0};
        bm_image_format_ext      image_format      = FORMAT_BGR_PLANAR;
        bm_image_data_format_ext data_type         = DATA_TYPE_EXT_1N_BYTE;
        int                      raw_size          = 0;
        int                      src_size          = 0;
        int                      dst_size          = 0;

        int test = rand() % 6;
        switch (test) {
            case 0:
                image_format      = FORMAT_BGR_PLANAR;
                default_stride[0] = image_w;
                src_stride[0]     = image_w + rand() % 16;
                dst_stride[0]     = image_w + rand() % 16;

                raw_size = image_h * image_w * 3;
                src_size = 3 * image_h * src_stride[0];
                dst_size = 3 * image_h * dst_stride[0];
                break;
            case 1:
                image_format      = FORMAT_BGR_PACKED;
                default_stride[0] = 3 * image_w;
                src_stride[0]     = 3 * image_w + rand() % 16;
                dst_stride[0]     = 3 * image_w + rand() % 16;

                raw_size = image_h * image_w * 3;
                src_size = image_h * src_stride[0];
                dst_size = image_h * dst_stride[0];
                break;
            case 2:
                image_format = FORMAT_NV12;
                image_w      = ALIGN(image_w, 2);
                image_h      = ALIGN(image_h, 2);

                default_stride[0] = image_w;
                src_stride[0]     = image_w + rand() % 16;
                dst_stride[0]     = image_w + rand() % 16;

                default_stride[1] = image_w;
                src_stride[1]     = image_w + rand() % 16;
                dst_stride[1]     = image_w + rand() % 16;

                raw_size = image_h * image_w * 3 / 2;
                src_size =
                    image_h * src_stride[0] + image_h / 2 * src_stride[1];
                dst_size =
                    image_h * dst_stride[0] + image_h / 2 * dst_stride[1];
                break;
            case 3:
                image_format      = FORMAT_GRAY;
                default_stride[0] = image_w;
                src_stride[0]     = image_w + rand() % 16;
                dst_stride[0]     = image_w + rand() % 16;

                raw_size = image_h * image_w;
                src_size = image_h * src_stride[0];
                dst_size = image_h * dst_stride[0];
                break;
            case 4:
                image_format = FORMAT_NV16;
                image_w      = ALIGN(image_w, 2);
                image_h      = ALIGN(image_h, 2);

                default_stride[0] = image_w;
                src_stride[0]     = image_w + rand() % 16;
                dst_stride[0]     = image_w + rand() % 16;

                default_stride[1] = image_w;
                src_stride[1]     = image_w + rand() % 16;
                dst_stride[1]     = image_w + rand() % 16;

                raw_size = image_h * image_w * 2;
                src_size = image_h * src_stride[0] + image_h * src_stride[1];
                dst_size = image_h * dst_stride[0] + image_h * dst_stride[1];
                break;
            case 5:
                image_format = FORMAT_YUV420P;
                image_w      = ALIGN(image_w, 2);
                image_h      = ALIGN(image_h, 2);

                default_stride[0] = image_w;
                src_stride[0]     = image_w + rand() % 16;
                dst_stride[0]     = image_w + rand() % 16;

                default_stride[1] = image_w / 2;
                src_stride[1]     = image_w / 2 + rand() % 16;
                dst_stride[1]     = image_w / 2 + rand() % 16;

                default_stride[2] = image_w / 2;
                src_stride[2]     = image_w / 2 + rand() % 16;
                dst_stride[2]     = image_w / 2 + rand() % 16;

                raw_size = image_h * image_w * 3 / 2;
                src_size = image_h * src_stride[0] +
                           image_h / 2 * src_stride[1] +
                           image_h / 2 * src_stride[2];
                dst_size = image_h * dst_stride[0] +
                           image_h / 2 * dst_stride[1] +
                           image_h / 2 * dst_stride[2];
                break;
        }

        printf("image_h=%d  image_w=%d test type %d\n",
               image_h,
               image_w,
               (int)test);

        std::shared_ptr<Blob> raw_image = std::make_shared<Blob>(raw_size);
        std::shared_ptr<Blob> src_image = std::make_shared<Blob>(src_size);
        std::shared_ptr<Blob> dst_image = std::make_shared<Blob>(dst_size);
        std::shared_ptr<Blob> inner_ptr = std::make_shared<Blob>(raw_size);

        // read source data form input file.
        image_fill(raw_image->data, raw_size);

        // calculate use reference for compare.
        bmcv_width_align_ref(raw_image->data,
                             image_format,
                             image_h,
                             image_w,
                             default_stride,
                             src_stride,
                             src_image->data);

        bmcv_width_align_ref(raw_image->data,
                             image_format,
                             image_h,
                             image_w,
                             default_stride,
                             dst_stride,
                             dst_image->data);

        // create source image.
        bm_image_create(handle,
                        image_h,
                        image_w,
                        image_format,
                        data_type,
                        &src_img,
                        src_stride);
        bm_image_create(handle,
                        image_h,
                        image_w,
                        image_format,
                        data_type,
                        &dst_img,
                        dst_stride);

        int size[3] = {0};
        bm_image_get_byte_size(src_img, size);
        u8 *host_ptr_src[] = {src_image->data,
                              src_image->data + size[0],
                              src_image->data + size[0] + size[1]};
        bm_image_get_byte_size(dst_img, size);
        u8 *host_ptr_dst[] = {dst_image->data,
                              dst_image->data + size[0],
                              dst_image->data + size[0] + size[1]};

        ret = bm_image_copy_host_to_device(src_img, (void **)(host_ptr_src));
        if (ret != BM_SUCCESS) {
            printf("test data prepare failed");
            break;
        }

        struct timeval t1, t2;
        gettimeofday_(&t1);
        ret = bmcv_width_align(handle, src_img, dst_img);
        if (ret != BM_SUCCESS) {
            printf("bmcv width align failed");
            break;
        }
        gettimeofday_(&t2);
        cout << "width align using time: " << ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec) << "us" << endl;

        ret = bm_image_copy_device_to_host(dst_img, (void **)(host_ptr_dst));
        if (ret != BM_SUCCESS) {
            printf("test data copy_back failed");
            break;
        }
        bm_image_destroy(src_img);
        bm_image_destroy(dst_img);

        std::shared_ptr<Blob> src_data =
            std::make_shared<Blob>(bm_max(src_size, dst_size));
        std::shared_ptr<Blob> dst_data =
            std::make_shared<Blob>(bm_max(src_size, dst_size));

        // transform the img_data to default stride
        bmcv_width_align_ref(src_image->data,
                             image_format,
                             image_h,
                             image_w,
                             src_stride,
                             default_stride,
                             src_data->data);
        bmcv_width_align_ref(dst_image->data,
                             image_format,
                             image_h,
                             image_w,
                             dst_stride,
                             default_stride,
                             dst_data->data);

        // compare.

        int cmp_res =
            bmcv_width_align_cmp(src_data->data, dst_data->data, raw_size);
        if (cmp_res != 0) {
            printf("cv_width_align comparing failed\n");
            ret = BM_ERR_FAILURE;
            break;
        }
    }

    if(bm_image_is_attached(src_img)){
        bm_image_destroy(src_img);
        bm_image_destroy(dst_img);
    }
    bm_dev_free(handle);
    printf("cv_warp random tests done!\n");
    return ret;
}

int main(int argc, char *argv[]) {
    int test_loop_times = 0;
    if (argc == 1) {
        test_loop_times = 1;
    } else {
        test_loop_times = atoi(argv[1]);
    }
    if (test_loop_times > 1500 || test_loop_times < 1) {
        std::cout << "[TEST WIDTH ALIGN] loop times should be 1~1500"
                  << std::endl;
        exit(-1);
    }
    std::cout << "[TEST WIDTH ALIGN] test starts... LOOP times will be "
              << test_loop_times << std::endl;
    for (int loop_idx = 0; loop_idx < test_loop_times; loop_idx++) {
        std::cout << "------[TEST WIDTH ALIGN] LOOP " << loop_idx << "------"
                  << std::endl;
        struct timespec tp;
        clock_gettime_(0, &tp);

        srand(tp.tv_nsec);
        printf("random seed %lu\n", tp.tv_nsec);
        if(BM_SUCCESS != test_cv_width_align(100))
            return -1;
    }
    std::cout << "------[TEST WIDTH ALIGN] ALL TEST PASSED!" << std::endl;

    return 0;
}
