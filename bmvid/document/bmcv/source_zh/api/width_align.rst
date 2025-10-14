bmcv_width_align
=================
将内存区域的数据按照指定的宽的布局转换并存储到目标内存区域

**处理器型号支持：**

该接口支持BM1684X/BM1688/BM1690。

**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_width_align(
                    bm_handle_t handle,
                    bm_image    input,
                    bm_image    output)

**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image input

  输入参数。输入图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image output

  输出参数。输出 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他:失败

**格式支持：**

该接口目前支持以下 image_format:

+-----+------------------------+------------------------+
| num | input image_format     | output image_format    |
+=====+========================+========================+
| 1   | FORMAT_BGR_PACKED      | FORMAT_BGR_PACKED      |
+-----+------------------------+------------------------+
| 2   | FORMAT_BGR_PLANAR      | FORMAT_BGR_PLANAR      |
+-----+------------------------+------------------------+
| 3   | FORMAT_RGB_PACKED      | FORMAT_RGB_PACKED      |
+-----+------------------------+------------------------+
| 4   | FORMAT_RGB_PLANAR      | FORMAT_RGB_PLANAR      |
+-----+------------------------+------------------------+
| 5   | FORMAT_RGBP_SEPARATE   | FORMAT_RGBP_SEPARATE   |
+-----+------------------------+------------------------+
| 6   | FORMAT_BGRP_SEPARATE   | FORMAT_BGRP_SEPARATE   |
+-----+------------------------+------------------------+
| 7   | FORMAT_GRAY            | FORMAT_GRAY            |
+-----+------------------------+------------------------+
| 8   | FORMAT_YUV420P         | FORMAT_YUV420P         |
+-----+------------------------+------------------------+
| 9   | FORMAT_YUV422P         | FORMAT_YUV422P         |
+-----+------------------------+------------------------+
| 10  | FORMAT_YUV444P         | FORMAT_YUV444P         |
+-----+------------------------+------------------------+
| 11  | FORMAT_NV12            | FORMAT_NV12            |
+-----+------------------------+------------------------+
| 12  | FORMAT_NV21            | FORMAT_NV21            |
+-----+------------------------+------------------------+
| 13  | FORMAT_NV16            | FORMAT_NV16            |
+-----+------------------------+------------------------+
| 14  | FORMAT_NV61            | FORMAT_NV61            |
+-----+------------------------+------------------------+
| 15  | FORMAT_NV24            | FORMAT_NV24            |
+-----+------------------------+------------------------+


目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+
| 2   | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+
| 3   | DATA_TYPE_EXT_FP16             |
+-----+--------------------------------+

**注意事项：**

1、在调用该接口之前必须确保输入的 image 内存已经申请。

2、input output 的image_format以及data_type必须相同。

**代码示例：**

    .. code-block:: c
        #include <iostream>
        #include <memory>
        #ifdef __linux__
        #include <sys/time.h>
        #endif

        #include "bmcv_api_ext.h"
        #include "test_misc.h"
        #include "stdio.h"
        #include "stdlib.h"
        #include "string.h"

        #include <assert.h>
        #include <vector>

        #define DEBUG_FILE (0)

        #define MODE (STORAGE_MODE_1N_INT8)

        using namespace std;

        static bm_handle_t handle;

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
        static void readBin(const char* path, unsigned char* input_data, int size)
        {
            FILE *fp_src = fopen(path, "rb");
            if (fread((void *)input_data, 1, size, fp_src) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_src);
        }

        static void write_bin(const char *output_path, unsigned char *output_data, int size)
        {
            FILE *fp_dst = fopen(output_path, "wb");

            if (fp_dst == NULL) {
                printf("unable to open output file %s\n", output_path);
                return;
            }

            if(fwrite(output_data, sizeof(unsigned char), size, fp_dst) != 0) {
                printf("write image success\n");
            }
            fclose(fp_dst);
        }

        int main() {
            int         dev_id = 0;
            bm_status_t ret    = bm_dev_request(&handle, dev_id);
            bm_image src_img;
            bm_image dst_img;

            int image_h = 1080;
            int image_w = 1920;

            int                      default_stride[3] = {0};
            int                      src_stride[3]     = {0};
            int                      dst_stride[3]     = {0};
            bm_image_format_ext      image_format      = FORMAT_BGR_PLANAR;
            bm_image_data_format_ext data_type         = DATA_TYPE_EXT_1N_BYTE;
            int                      raw_size          = 0;

            image_format      = FORMAT_GRAY;
            default_stride[0] = image_w;
            src_stride[0]     = image_w + rand() % 16;
            dst_stride[0]     = image_w + rand() % 16;

            raw_size = image_h * image_w;
            unsigned char* raw_image = (unsigned char*)malloc(image_w * image_h * sizeof(unsigned char));
            unsigned char* dst_data = (unsigned char*)malloc(image_w * image_h * sizeof(unsigned char));
            unsigned char* src_image = (unsigned char*)malloc(src_stride[0] * image_h * sizeof(unsigned char));
            unsigned char* dst_image = (unsigned char*)malloc(dst_stride[0] * image_h * sizeof(unsigned char));

            const char* input_path = "path/to/input";
            const char* output_path = "path/to/output";
            readBin(input_path, raw_image, raw_size);

            // calculate use reference for compare.
            unsigned char *src_s_offset;
            unsigned char *src_d_offset;
            for (int i = 0; i < image_h; i++) {
                src_s_offset = raw_image + i * default_stride[0];
                src_d_offset = src_image + i * src_stride[0];
                memcpy(src_d_offset, src_s_offset, image_w);
            }

            // create source image.
            bm_image_create(handle, image_h, image_w,image_format, data_type, &src_img, src_stride);
            bm_image_create(handle, image_h, image_w, image_format, data_type, &dst_img, dst_stride);

            int size[3] = {0};
            bm_image_get_byte_size(src_img, size);
            u8 *host_ptr_src[] = {src_image,
                                    src_image + size[0],
                                    src_image + size[0] + size[1]};
            bm_image_get_byte_size(dst_img, size);
            u8 *host_ptr_dst[] = {dst_image,
                                    dst_image + size[0],
                                    dst_image + size[0] + size[1]};

            ret = bm_image_copy_host_to_device(src_img, (void **)(host_ptr_src));
            if (ret != BM_SUCCESS) {
                printf("test data prepare failed");
                return ret;
            }

            ret = bmcv_width_align(handle, src_img, dst_img);
            if (ret != BM_SUCCESS) {
                printf("bmcv width align failed");
                return ret;
            }

            ret = bm_image_copy_device_to_host(dst_img, (void **)(host_ptr_dst));
            if (ret != BM_SUCCESS) {
                printf("test data copy_back failed");
                return ret;
            }
            bm_image_destroy(src_img);
            bm_image_destroy(dst_img);

            unsigned char *dst_s_offset;
            unsigned char *dst_d_offset;
            for (int i = 0; i < image_h; i++) {
                dst_s_offset = dst_image + i * dst_stride[0];
                dst_d_offset = dst_data + i * default_stride[0];
                memcpy(dst_d_offset, dst_s_offset, image_w);
            }
            write_bin(output_path, dst_data, raw_size);
            // compare.

            int cmp_res =
                bmcv_width_align_cmp(raw_image, dst_data, raw_size);
            if (cmp_res != 0) {
                printf("cv_width_align comparing failed\n");
                ret = BM_ERR_FAILURE;
                return ret;
            }
            std::cout << "------[TEST WIDTH ALIGN] ALL TEST PASSED!" << std::endl;
            free(raw_image);
            free(dst_data);
            free(src_image);
            free(dst_image);
            return 0;
        }
