bmcv_image_rotate
==================

实现图像顺时针旋转90度，180度，270度


**处理器型号支持：**

该接口仅支持BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_image_rotate(
                    bm_handle_t handle,
                    bm_image input,
                    bm_image output,
                    int rotation_angle);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image input

  输入参数。输入图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image output

  输出参数。输出图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。如果不主动分配将在 api 内部进行自行分配。

* rotation_angle

  顺时针旋转角度。可选角度90度，180度，270度。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**格式支持：**

该接口目前支持以下 image_format:

+-----+------------------------+
| num | image_format           |
+=====+========================+
| 1   | FORMAT_YUV444P         |
+-----+------------------------+
| 2   | FORMAT_NV12            |
+-----+------------------------+
| 3   | FORMAT_NV21            |
+-----+------------------------+
| 4   | FORMAT_RGB_PLANAR      |
+-----+------------------------+
| 5   | FORMAT_BGR_PLANAR      |
+-----+------------------------+
| 6   | FORMAT_RGBP_SEPARATE   |
+-----+------------------------+
| 7   | FORMAT_BGRP_SEPARATE   |
+-----+------------------------+
| 8   | FORMAT_GRAY            |
+-----+------------------------+

目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**注意事项：**

1、在调用 bmcv_image_rotate()之前必须确保输入的 image 内存已经申请。

2、输入输出图像的data_type，image_format必须相同。

3、输入输出图像的宽高尺寸支持8*8-8192*8192。

4、输入输出图像为nv12和nv21图像格式时，因处理过程中会经过多次色彩变换，输出图像像素值将存在误差，但肉眼观察差异不大。


**代码示例：**

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include <unistd.h>

        static void readBin(const char* path, unsigned char* input_data, int size)
        {
            FILE *fp_src = fopen(path, "rb");

            if (fread((void *)input_data, 1, size, fp_src) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_src);
        }

        static void writeBin(const char * path, unsigned char* input_data, int size)
        {
            FILE *fp_dst = fopen(path, "wb");
            if (fwrite((void *)input_data, 1, size, fp_dst) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_dst);
        }

        int main()
        {
            int width = 1920;
            int height = 1080;
            int format = FORMAT_RGB_PLANAR;
            int rotation_angle = 90;
            int dev_id = 0;
            unsigned char* input_data = (unsigned char*)malloc(width * height * 3 * sizeof(unsigned char));
            unsigned char* output_data = (unsigned char*)malloc(width * height * 3 * sizeof(unsigned char));
            bm_handle_t handle;
            bm_image input_img, output_img;
            const char *src_name = "/path/to/src";
            const char *dst_name = "path/to/dst";
            unsigned char* in_ptr[3] = {input_data, input_data + width * height, input_data + width * height * 2};
            unsigned char* out_ptr[3] = {output_data, output_data + width * height, output_data + width * height * 2};

            bm_dev_request(&handle, dev_id);
            readBin(src_name, input_data, width * height * 3);
            bm_image_create(handle, height, width, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &input_img, NULL);
            bm_image_create(handle, width, height, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &output_img, NULL);
            bm_image_alloc_dev_mem(input_img);
            bm_image_alloc_dev_mem(output_img);
            bm_image_copy_host_to_device(input_img, (void **)in_ptr);
            bmcv_image_rotate(handle, input_img, output_img, rotation_angle);
            bm_image_copy_device_to_host(output_img, (void **)out_ptr);
            writeBin(dst_name, output_data, width * height * 3);

            bm_image_destroy(input_img);
            bm_image_destroy(output_img);
            bm_dev_free(handle);
            free(input_data);
            free(output_data);
            return 0;
        }