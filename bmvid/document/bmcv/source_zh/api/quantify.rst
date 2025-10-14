bmcv_image_quantify
====================

将float类型数据转化成int类型(舍入模式为小数点后直接截断)，并将小于0的数变为0，大于255的数变为255。


**处理器型号支持：**

该接口仅支持BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_image_quantify(
                    bm_handle_t handle,
                    bm_image input,
                    bm_image output);


**参数说明：**

* bm_handle_t handle

  输入参数。 bm_handle 句柄。

* bm_image input

  输入参数。输入图像的 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以使用 bm_image_alloc_dev_mem 或者 bm_image_copy_host_to_device 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。

* bm_image output

  输出参数。输出 bm_image，bm_image 需要外部调用 bmcv_image_create 创建。image 内存可以通过 bm_image_alloc_dev_mem 来开辟新的内存，或者使用 bmcv_image_attach 来 attach 已有的内存。如果不主动分配将在 api 内部进行自行分配。


**返回值说明：**

* BM_SUCCESS: 成功

* 其他: 失败


**格式支持：**

该接口目前支持以下 image_format:

+-----+------------------------+------------------------+
| num | input image_format     | output image_format    |
+=====+========================+========================+
| 1   | FORMAT_RGB_PLANAR      | FORMAT_RGB_PLANAR      |
+-----+------------------------+------------------------+
| 2   | FORMAT_BGR_PLANAR      | FORMAT_BGR_PLANAR      |
+-----+------------------------+------------------------+


输入数据目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+

输出数据目前支持以下 data_type:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**注意事项：**

1. 在调用该接口之前必须确保输入的 image 内存已经申请。

2. 如调用该接口的程序为多线程程序，需要在创建bm_image前和销毁bm_image后加线程锁。

3. 该接口支持图像宽高范围为1x1～8192x8192。


**代码示例：**

    .. code-block:: c

        #include <stdio.h>
        #include "bmcv_api_ext.h"
        #include <stdlib.h>
        #include <string.h>
        #include <assert.h>
        #include <float.h>

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
            if (fwrite((void *)input_data, 1, size, fp_dst) < (unsigned int)size) {
                printf("file size is less than %d required bytes\n", size);
            };

            fclose(fp_dst);
        }

        int main()
        {
            int width = 1920;
            int height = 1080;
            bm_handle_t handle;
            bm_image input_img;
            bm_image output_img;
            float* input = (float*)malloc(width * height * 3 * sizeof(float));
            unsigned char* output = (unsigned char*)malloc(width * height * 3 * sizeof(unsigned char));
            const char *input_path = "path/to/input";
            const char *output_path = "path/to/output";

            readBin(input_path, (unsigned char*)input, width * height * 3);

            bm_dev_request(&handle, 0);
            bm_image_create(handle, height, width, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32, &input_img, NULL);
            bm_image_create(handle, height, width, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_1N_BYTE, &output_img, NULL);
            bm_image_alloc_dev_mem(input_img, 2);
            bm_image_alloc_dev_mem(output_img, 2);
            bm_image_copy_host_to_device(input_img, (void **)&input);
            bmcv_image_quantify(handle, input_img, output_img);
            bm_image_copy_device_to_host(output_img, (void **)&output);
            writeBin(output_path, output, (width * height * 3));

            bm_image_destroy(input_img);
            bm_image_destroy(output_img);
            free(input);
            free(output);
            bm_dev_free(handle);
            return 0;
        }