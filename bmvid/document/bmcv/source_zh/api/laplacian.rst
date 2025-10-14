bmcv_image_laplacian
====================

梯度计算laplacian算子。


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_image_laplacian(
                    bm_handle_t handle,
                    bm_image input,
                    bm_image output,
                    unsigned int ksize);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image input

  输入参数。输入图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image output

  输出参数。输出 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。如果不主动分配将在 api 内部进行自行分配。

* int ksize = 3

  Laplacian核的大小，必须是1或3。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**格式支持：**

该接口目前支持以下 image_format:

+-----+------------------------+------------------------+
| num | input image_format     | output image_format    |
+=====+========================+========================+
| 1   | FORMAT_GRAY            | FORMAT_GRAY            |
+-----+------------------------+------------------------+


目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**注意事项：**

1、在调用该接口之前必须确保输入的 image 内存已经申请。

2、input output 的 data_type必须相同。

3、目前支持图像的最大width为2048。


**代码示例：**

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <stdio.h>
        #include <stdlib.h>
        #include <math.h>
        #include <string.h>

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
            int height = 1080;
            int width = 1920;
            unsigned int ksize = 3;
            bm_image_format_ext fmt = FORMAT_GRAY;
            bm_handle_t handle;
            bm_image_data_format_ext data_type = DATA_TYPE_EXT_1N_BYTE;
            bm_image input;
            bm_image output;
            unsigned char* input_data = (unsigned char*)malloc(width * height * sizeof(unsigned char));
            unsigned char* tpu_out = (unsigned char*)malloc(width * height * sizeof(unsigned char));
            const char* src_name = "path/to/src";
            const char* dst_name = "path/to/dst";

            bm_dev_request(&handle, 0);
            bm_image_create(handle, height, width, fmt, data_type, &input);
            bm_image_alloc_dev_mem(input);
            bm_image_create(handle, height, width, fmt, data_type, &output);
            bm_image_alloc_dev_mem(output);

            readBin(src_name, input_data, width * height);
            bm_image_copy_host_to_device(input, (void**)&input_data);
            bmcv_image_laplacian(handle, input, output, ksize);
            bm_image_copy_device_to_host(output, (void **)&tpu_out);
            writeBin(dst_name, tpu_out, width * height);

            bm_image_destroy(input);
            bm_image_destroy(output);
            bm_dev_free(handle);
            free(input_data);
            free(tpu_out);
            return 0;
        }