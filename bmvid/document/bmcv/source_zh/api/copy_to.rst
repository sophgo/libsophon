bmcv_image_copy_to
==================

该接口实现将一幅图像拷贝到目的图像的对应内存区域。


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_image_copy_to(
                    bm_handle_t handle,
                    bmcv_copy_to_atrr_t copy_to_attr,
                    bm_image input,
                    bm_image output);


**参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄。

* bmcv_copy_to_atrr_t copy_to_attr

  输入参数。api 所对应的属性配置。

* bm_image input

  输入参数。输入 bm_image，bm_image 需要外部调用 bmcv_image _create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image output

  输出参数。输出 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。如果不主动分配将在 api 内部进行自行分配。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**数据类型说明：**


    .. code-block:: c

        typedef struct bmcv_copy_to_atrr_s {
            int           start_x;
            int           start_y;
            unsigned char padding_r;
            unsigned char padding_g;
            unsigned char padding_b;
            int if_padding;
        } bmcv_copy_to_atrr_t;

* padding_b 表示当 input 的图像要小于输出图像的情况下，多出来的图像 b 通道上被填充的值。

* padding_r 表示当 input 的图像要小于输出图像的情况下，多出来的图像 r 通道上被填充的值。

* padding_g 表示当 input 的图像要小于输出图像的情况下，多出来的图像 g 通道上被填充的值。

* start_x 描述了 copy_to 拷贝到输出图像所在的起始横坐标。

* start_y 描述了 copy_to 拷贝到输出图像所在的起始纵坐标。

* if_padding 表示当 input 的图像要小于输出图像的情况下，是否需要对多余的图像区域填充特定颜色，0表示不需要，1表示需要。当该值填0时，padding_r，padding_g，padding_b 的设置将无效


**格式支持：**

bm1684支持以下 image_format 和 data_type 的组合：

+-----+------------------------+-------------------------+
| num | image_format           | data_type               |
+=====+========================+=========================+
|  1  | FORMAT_BGR_PACKED      | DATA_TYPE_EXT_FLOAT32   |
+-----+------------------------+-------------------------+
|  2  | FORMAT_BGR_PLANAR      | DATA_TYPE_EXT_FLOAT32   |
+-----+------------------------+-------------------------+
|  3  | FORMAT_BGR_PACKED      | DATA_TYPE_EXT_1N_BYTE   |
+-----+------------------------+-------------------------+
|  4  | FORMAT_BGR_PLANAR      | DATA_TYPE_EXT_1N_BYTE   |
+-----+------------------------+-------------------------+
|  5  | FORMAT_BGR_PLANAR      | DATA_TYPE_EXT_4N_BYTE   |
+-----+------------------------+-------------------------+
|  7  | FORMAT_RGB_PACKED      | DATA_TYPE_EXT_FLOAT32   |
+-----+------------------------+-------------------------+
|  8  | FORMAT_RGB_PLANAR      | DATA_TYPE_EXT_FLOAT32   |
+-----+------------------------+-------------------------+
|  9  | FORMAT_RGB_PACKED      | DATA_TYPE_EXT_1N_BYTE   |
+-----+------------------------+-------------------------+
|  10 | FORMAT_RGB_PLANAR      | DATA_TYPE_EXT_1N_BYTE   |
+-----+------------------------+-------------------------+
|  11 | FORMAT_RGB_PLANAR      | DATA_TYPE_EXT_4N_BYTE   |
+-----+------------------------+-------------------------+
|  12 | FORMAT_GRAY            | DATA_TYPE_EXT_1N_BYTE   |
+-----+------------------------+-------------------------+

bm1684x支持以下数据输入、输出类型：

+-----+------------------------+-------------------------------+
| num | input data type        | output data type              |
+=====+========================+===============================+
|  1  |                        | DATA_TYPE_EXT_FLOAT32         |
+-----+                        +-------------------------------+
|  2  |                        | DATA_TYPE_EXT_1N_BYTE         |
+-----+                        +-------------------------------+
|  3  | DATA_TYPE_EXT_1N_BYTE  | DATA_TYPE_EXT_1N_BYTE_SIGNED  |
+-----+                        +-------------------------------+
|  4  |                        | DATA_TYPE_EXT_FP16            |
+-----+                        +-------------------------------+
|  5  |                        | DATA_TYPE_EXT_BF16            |
+-----+------------------------+-------------------------------+
|  6  | DATA_TYPE_EXT_FLOAT32  | DATA_TYPE_EXT_FLOAT32         |
+-----+------------------------+-------------------------------+

输入和输出支持的色彩格式为：

+-----+-------------------------------+
| num | image_format                  |
+=====+===============================+
|  1  | FORMAT_YUV420P                |
+-----+-------------------------------+
|  2  | FORMAT_YUV444P                |
+-----+-------------------------------+
|  3  | FORMAT_NV12                   |
+-----+-------------------------------+
|  4  | FORMAT_NV21                   |
+-----+-------------------------------+
|  5  | FORMAT_RGB_PLANAR             |
+-----+-------------------------------+
|  6  | FORMAT_BGR_PLANAR             |
+-----+-------------------------------+
|  7  | FORMAT_RGB_PACKED             |
+-----+-------------------------------+
|  8  | FORMAT_BGR_PACKED             |
+-----+-------------------------------+
|  9  | FORMAT_RGBP_SEPARATE          |
+-----+-------------------------------+
|  10 | FORMAT_BGRP_SEPARATE          |
+-----+-------------------------------+
|  11 | FORMAT_GRAY                   |
+-----+-------------------------------+


**注意事项：**

1、在调用 bmcv_image_copy_to()之前必须确保输入的 image 内存已经申请。

2、bm1684中的input output 的 data_type，image_format 必须相同。

3、为了避免内存越界，输入图像width + start_x 必须小于等于输出图像width stride。


**代码示例：**

    .. code-block:: c

        #include <assert.h>
        #include <stdint.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include "bmcv_api_ext.h"

        static void readBin(const char* path, unsigned char* input_data, int size)
        {
            FILE *fp_src = fopen(path, "rb");

            if (fread((void *)input_data, 4, size, fp_src) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_src);
        }

        static void writeBin(const char * path, unsigned char* input_data, int size)
        {
            FILE *fp_dst = fopen(path, "wb");
            if (fwrite((void *)input_data, 4, size, fp_dst) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_dst);
        }

        int main()
        {
            int channel = 3;
            int in_w = 400;
            int in_h = 400;
            int out_w = 800;
            int out_h = 800;
            int dev_id = 0;
            int image_n = 1;
            bm_handle_t handle;
            bmcv_copy_to_atrr_t copy_to_attr;
            bm_image input, output;
            float* src_data = (float *)malloc(image_n * channel * in_w * in_h * sizeof(float));
            float* res_data = (float *)malloc(image_n * channel * out_w * out_h * sizeof(float));
            const char* src_name = "path/to/src";
            const char* dst_name = "path/to/dst";

            bm_dev_request(&handle, dev_id);
            readBin(src_name, (unsigned char*)src_data, channel * in_w * in_h);

            copy_to_attr.start_x = 200;
            copy_to_attr.start_y = 200;
            copy_to_attr.padding_r = 0;
            copy_to_attr.padding_g = 0;
            copy_to_attr.padding_b = 0;
            copy_to_attr.if_padding = 1;

            bm_image_create(handle, in_h, in_w, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32, &input);
            bm_image_alloc_dev_mem(input);
            bm_image_copy_host_to_device(input, (void **)&src_data);
            bm_image_create(handle, out_h, out_w, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32, &output);
            bm_image_alloc_dev_mem(output);
            bmcv_image_copy_to(handle, copy_to_attr, input, output);
            bm_image_copy_device_to_host(output, (void **)&res_data);
            writeBin(dst_name, (unsigned char*)res_data, channel * out_w * out_h);

            bm_image_destroy(input);
            bm_image_destroy(output);
            free(src_data);
            free(res_data);
            bm_dev_free(handle);
            return 0;
        }