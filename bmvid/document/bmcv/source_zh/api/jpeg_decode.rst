bmcv_image_jpeg_dec
===================

该接口可以实现对多张图片的 JPEG 解码过程。


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式：**

    .. code-block:: c

        bm_status_t bmcv_image_jpeg_dec(
                    bm_handle_t handle,
                    void* p_jpeg_data[],
                    size_t* in_size,
                    int image_num,
                    bm_image* dst);


**输入参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄。

* void \*  p_jpeg_data[]

  输入参数。待解码的图片数据指针，由于该接口支持对多张图片的解码，因此为指针数组。

* size_t \*in_size

  输入参数。待解码各张图片的大小（以 byte 为单位）存放在该指针中，也就是上述 p_jpeg_data 每一维指针所指向空间的大小。

* int  image_num

  输入参数。输入图片数量，最多支持 4

* bm_image\* dst

  输出参数。输出 bm_image的指针。每个 dst bm_image 用户可以选择自行调用 bm_image_create 创建，也可以选择不创建。如果用户只声明而不创建则由接口内部根据待解码图片信息自动创建，默认的 format 如下表所示, 当不再需要时仍然需要用户调用 bm_image_destory 来销毁。

+------------+------------------+
|  码 流     | 默认输出 format  |
+============+==================+
|  YUV420    |  FORMAT_YUV420P  |
+------------+------------------+
|  YUV422    |  FORMAT_YUV422P  |
+------------+------------------+
|  YUV444    |  FORMAT_YUV444P  |
+------------+------------------+
|  YUV400    |  FORMAT_GRAY     |
+------------+------------------+


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项：**

1. 如果用户没有使用bmcv_image_create创建dst的bm_image，那么需要将参数传入指针所指向的空间置0。


2. 目前解码支持的图片格式及其输出格式对应如下，如果用户需要指定以下某一种输出格式，可通过使用 bmcv_image_create 自行创建 dst bm_image，从而实现将图片解码到以下对应的某一格式。

+------------------+------------------+
|     码 流        |   输 出 format   |
+==================+==================+
|                  |  FORMAT_YUV420P  |
+  YUV420          +------------------+
|                  |  FORMAT_NV12     |
+                  +------------------+
|                  |  FORMAT_NV21     |
+------------------+------------------+
|                  |  FORMAT_YUV422P  |
+  YUV422          +------------------+
|                  |  FORMAT_NV16     |
+                  +------------------+
|                  |  FORMAT_NV61     |
+------------------+------------------+
|  YUV444          |  FORMAT_YUV444P  |
+------------------+------------------+
|  YUV400          |  FORMAT_GRAY     |
+------------------+------------------+

   目前解码支持的数据格式如下，

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**示例代码**

    .. code-block:: c

        #include <stdio.h>
        #include <stdint.h>
        #include <stdlib.h>
        #include <memory.h>
        #include "bmcv_api_ext.h"
        #include <assert.h>
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
            int format = FORMAT_YUV420P;
            int image_h = 1080;
            int image_w = 1920;
            bm_image src;
            bm_image dst;
            bm_handle_t handle;
            size_t byte_size = image_w * image_h * 3 / 2;
            unsigned char* input_data = (unsigned char*)malloc(byte_size);
            unsigned char* output_data = (unsigned char*)malloc(byte_size);
            unsigned char* in_ptr[3] = {input_data, input_data + image_h * image_w, input_data + 2 * image_h * image_w};
            unsigned char* out_ptr[3] = {output_data, output_data + image_h * image_w, output_data + 2 * image_h * image_w};
            void* jpeg_data[4] = {NULL, NULL, NULL, NULL};
            const char *dst_name = "path/to/dst";
            const char *src_name = "path/to/src";

            readBin(src_name, input_data, byte_size);
            bm_dev_request(&handle, 0);
            bm_image_create(handle, image_h, image_w, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &src, NULL);
            bm_image_alloc_dev_mem(src, BMCV_HEAP1_ID);
            bm_image_create(handle, image_h, image_w, (bm_image_format_ext)format, DATA_TYPE_EXT_1N_BYTE, &dst, NULL);
            bm_image_alloc_dev_mem(dst, BMCV_HEAP1_ID);
            bm_image_copy_host_to_device(src, (void**)in_ptr);
            bmcv_image_jpeg_enc(handle, 1, &src, jpeg_data, &byte_size, 95);
            bmcv_image_jpeg_dec(handle, (void**)jpeg_data, &byte_size, 1, &dst);
            bm_image_copy_device_to_host(dst, (void**)out_ptr);
            writeBin(dst_name, output_data, byte_size);

            bm_image_destroy(src);
            bm_image_destroy(dst);
            free(input_data);
            free(output_data);
            bm_dev_free(handle);
            return 0;
        }