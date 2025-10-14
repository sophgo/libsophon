bmcv_image_gaussian_blur
========================

该接口用于对图像进行高斯滤波。


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_image_gaussian_blur(
                    bm_handle_t handle,
                    bm_image input,
                    bm_image output,
                    int kw,
                    int kh,
                    float sigmaX,
                    float sigmaY = 0);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image input

  输入参数。输入图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image output

  输出参数。输出 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。如果不主动分配将在 api 内部进行自行分配。

* int kw

  kernel 在width方向上的大小。

* int kh

  kernel 在height方向上的大小。

* float sigmaX

  X方向上的高斯核标准差。

* float sigmaY = 0

  Y方向上的高斯核标准差。如果为0则表示与X方向上的高斯核标准差相同。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**格式支持：**

该接口目前支持以下 image_format:

+-----+------------------------+------------------------+
| num | input image_format     | output image_format    |
+=====+========================+========================+
| 1   | FORMAT_BGR_PLANAR      | FORMAT_BGR_PLANAR      |
+-----+------------------------+------------------------+
| 2   | FORMAT_RGB_PLANAR      | FORMAT_RGB_PLANAR      |
+-----+------------------------+------------------------+
| 3   | FORMAT_RGBP_SEPARATE   | FORMAT_RGBP_SEPARATE   |
+-----+------------------------+------------------------+
| 4   | FORMAT_BGRP_SEPARATE   | FORMAT_BGRP_SEPARATE   |
+-----+------------------------+------------------------+
| 5   | FORMAT_GRAY            | FORMAT_GRAY            |
+-----+------------------------+------------------------+

目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**注意事项：**

1、在调用该接口之前必须确保输入的 image 内存已经申请。

2、input output 的 data_type，image_format必须相同。

3、BM1684支持的图像最大宽为(2048 - kw)，BM1684X芯片下该算子在卷积核大小为3时，支持的宽高范围为8*8～8192*8192，核大小为5时支持的宽高范围为8*8～4096*8192，核大小为7时支持的宽高范围为8*8～2048*8192。

4、BM1684X下卷积核支持的大小为3*3,5*5和7*7。

5、BM1684支持的最大卷积核宽高为31，BM1684X支持的最大卷积核宽高为7。


**代码示例：**

    .. code-block:: c

        #include <stdio.h>
        #include "bmcv_api_ext.h"
        #include "stdlib.h"
        #include "string.h"
        #include <assert.h>
        #include <float.h>
        #include <math.h>

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
            int channel = 1;
            int width = 1920;
            int height = 1080;
            int dev_id = 0;
            bm_handle_t handle;
            bm_image input, output;
            const char *input_path = "path/to/input";
            const char *output_path = "path/to/output";
            unsigned char* src_data = new unsigned char[channel * width * height];
            unsigned char* res_data = new unsigned char[channel * width * height];

            readBin(input_path, src_data, channel * width * height);
            bm_dev_request(&handle, dev_id);
            bm_image_create(handle, height, width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &input);
            bm_image_alloc_dev_mem(input);
            bm_image_copy_host_to_device(input, (void**)&src_data);
            bm_image_create(handle, height,width, FORMAT_GRAY, DATA_TYPE_EXT_1N_BYTE, &output);
            bm_image_alloc_dev_mem(output);
            bmcv_image_gaussian_blur(handle, input, output, 3, 3, 0.1);
            bm_image_copy_device_to_host(output, (void**)&res_data);
            writeBin(output_path, res_data, channel * width * height);

            bm_image_destroy(input);
            bm_image_destroy(output);
            free(src_data);
            free(res_data);
            bm_dev_free(handle);
            return 0;
        }