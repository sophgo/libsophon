bmcv_image_yuv2hsv
==================

对YUV图像的指定区域转为HSV格式。


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_image_yuv2hsv(
                    bm_handle_t handle,
                    bmcv_rect_t rect,
                    bm_image input,
                    bm_image output);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bmcv_rect_t rect

  描述了原图中待转换区域的起始坐标以及大小。具体参数可参见bmcv_image_crop接口中的描述。

* bm_image input

  输入参数。输入图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image output

  输出参数。输出 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。如果不主动分配将在 api 内部进行自行分配。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**格式支持：**

bm1684： 该接口目前支持以下 image_format:

+-----+------------------------+------------------------+
| num | input image_format     | output image_format    |
+=====+========================+========================+
|  1  | FORMAT_YUV420P         | FORMAT_HSV_PLANAR      |
+-----+------------------------+------------------------+
|  2  | FORMAT_NV12            | FORMAT_HSV_PLANAR      |
+-----+------------------------+------------------------+
|  3  | FORMAT_NV21            | FORMAT_HSV_PLANAR      |
+-----+------------------------+------------------------+

bm1684x： 该接口目前

- 支持以下输入色彩格式:

+-----+-------------------------------+
| num | input image_format            |
+=====+===============================+
|  1  | FORMAT_YUV420P                |
+-----+-------------------------------+
|  2  | FORMAT_NV12                   |
+-----+-------------------------------+
|  3  | FORMAT_NV21                   |
+-----+-------------------------------+

- 支持输出色彩格式:

+-----+-------------------------------+
| num | output image_format           |
+=====+===============================+
|  1  | FORMAT_HSV180_PACKED          |
+-----+-------------------------------+
|  2  | FORMAT_HSV256_PACKED          |
+-----+-------------------------------+

目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
|  1  | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**注意事项：**

1、在调用该接口之前必须确保输入的 image 内存已经申请。


**代码示例：**

    .. code-block:: c

        #include <iostream>
        #include <vector>
        #include "bmcv_api_ext.h"
        #include "stdio.h"
        #include "stdlib.h"
        #include "string.h"
        #include <memory>

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
            int channel = 3;
            int width = 1920;
            int height = 1080;
            int dev_id = 0;
            bm_handle_t handle;
            bm_image input, output;
            bmcv_rect_t rect;
            unsigned char* src_data = (unsigned char*)malloc(channel * width * height / 2);
            unsigned char* res_data = (unsigned char*)malloc(channel * width * height);
            unsigned char* in_ptr[3] = {src_data, src_data + height * width, src_data + 2 * height * width};
            unsigned char* out_ptr[3] = {res_data, res_data + height * width, res_data + 2 * height * width};
            const char *filename_src = "path/to/src";
            const char *filename_dst = "path/to/dst";

            rect.start_x = 0;
            rect.start_y = 0;
            rect.crop_w = width;
            rect.crop_h = height;

            bm_dev_request(&handle, dev_id);
            readBin(filename_src, src_data, channel * width * height / 2);
            bm_image_create(handle, height, width, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, &input);
            bm_image_alloc_dev_mem(input);
            bm_image_copy_host_to_device(input, (void**)in_ptr);
            bm_image_create(handle, height, width, FORMAT_HSV180_PACKED, DATA_TYPE_EXT_1N_BYTE, &output);
            bm_image_alloc_dev_mem(output);
            bmcv_image_yuv2hsv(handle, rect, input, output);
            bm_image_copy_device_to_host(output, (void**)out_ptr);
            writeBin(filename_dst, res_data, channel * width * height);

            bm_image_destroy(input);
            bm_image_destroy(output);
            free(src_data);
            free(res_data);
            bm_dev_free(handle);
            return 0;
        }